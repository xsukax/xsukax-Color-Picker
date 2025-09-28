#include <windows.h>
#include <commctrl.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

// Control IDs
#define ID_TRACK_BUTTON     1001
#define ID_PICK_BUTTON      1002
#define ID_COPY_HEX         1003
#define ID_COPY_RGB         1004
#define ID_COPY_HSL         1005
#define ID_HEX_EDIT         1006
#define ID_RGB_EDIT         1007
#define ID_HSL_EDIT         1008
#define ID_COORD_LABEL      1009
#define ID_STATUS_LABEL     1010
#define ID_TIMER            1011

// Modern color scheme
#define COLOR_BG            RGB(248, 249, 250)
#define COLOR_CARD          RGB(255, 255, 255)
#define COLOR_BORDER        RGB(229, 234, 239)
#define COLOR_PRIMARY       RGB(13, 110, 253)
#define COLOR_PRIMARY_HOVER RGB(11, 94, 215)
#define COLOR_SUCCESS       RGB(25, 135, 84)
#define COLOR_TEXT          RGB(33, 37, 41)
#define COLOR_TEXT_MUTED    RGB(108, 117, 125)

// Global variables
HWND g_hMainWnd = NULL;
HWND g_hColorRect = NULL;
HWND g_hTrackBtn = NULL;
HWND g_hPickBtn = NULL;
HWND g_hHexEdit = NULL;
HWND g_hRgbEdit = NULL;
HWND g_hHslEdit = NULL;
HWND g_hCoordLabel = NULL;
HWND g_hStatusLabel = NULL;

HFONT g_hFontMain = NULL;
HFONT g_hFontSmall = NULL;
HFONT g_hFontMono = NULL;
HBRUSH g_hBrushBg = NULL;
HBRUSH g_hBrushCard = NULL;
HBRUSH g_hBrushPrimary = NULL;

bool g_isTracking = false;
COLORREF g_currentColor = RGB(99, 102, 241);
POINT g_mousePos = {0, 0};

// Color conversion functions
void RGBtoHSL(int r, int g, int b, int& h, int& s, int& l) {
    double dr = r / 255.0;
    double dg = g / 255.0;
    double db = b / 255.0;
    
    double maxVal = std::max(std::max(dr, dg), db);
    double minVal = std::min(std::min(dr, dg), db);
    double delta = maxVal - minVal;
    
    // Lightness
    l = (int)((maxVal + minVal) / 2.0 * 100);
    
    if (delta == 0) {
        h = s = 0; // achromatic
    } else {
        // Saturation
        if (l < 50) {
            s = (int)(delta / (maxVal + minVal) * 100);
        } else {
            s = (int)(delta / (2.0 - maxVal - minVal) * 100);
        }
        
        // Hue
        if (maxVal == dr) {
            h = (int)(((dg - db) / delta + (dg < db ? 6 : 0)) * 60);
        } else if (maxVal == dg) {
            h = (int)(((db - dr) / delta + 2) * 60);
        } else {
            h = (int)(((dr - dg) / delta + 4) * 60);
        }
    }
}

std::string ColorToHex(COLORREF color) {
    int r = GetRValue(color);
    int g = GetGValue(color);
    int b = GetBValue(color);
    
    std::ostringstream oss;
    oss << "#" << std::hex << std::uppercase << std::setfill('0') 
        << std::setw(2) << r << std::setw(2) << g << std::setw(2) << b;
    return oss.str();
}

std::string ColorToRGB(COLORREF color) {
    int r = GetRValue(color);
    int g = GetGValue(color);
    int b = GetBValue(color);
    
    std::ostringstream oss;
    oss << r << ", " << g << ", " << b;
    return oss.str();
}

std::string ColorToHSL(COLORREF color) {
    int r = GetRValue(color);
    int g = GetGValue(color);
    int b = GetBValue(color);
    int h, s, l;
    
    RGBtoHSL(r, g, b, h, s, l);
    
    std::ostringstream oss;
    oss << h << " deg, " << s << "%, " << l << "%";
    return oss.str();
}

void CopyToClipboard(const std::string& text) {
    if (OpenClipboard(g_hMainWnd)) {
        EmptyClipboard();
        
        HGLOBAL hClipboardData = GlobalAlloc(GMEM_DDESHARE, text.length() + 1);
        char* pchData = (char*)GlobalLock(hClipboardData);
        strcpy_s(pchData, text.length() + 1, text.c_str());
        GlobalUnlock(hClipboardData);
        
        SetClipboardData(CF_TEXT, hClipboardData);
        CloseClipboard();
        
        // Visual feedback
        SetWindowTextA(g_hMainWnd, "xsukax Color Picker - Copied!");
        SetTimer(g_hMainWnd, ID_TIMER + 1, 1500, NULL);
    }
}

COLORREF GetPixelAtCursor() {
    POINT pt;
    GetCursorPos(&pt);
    g_mousePos = pt;
    
    HDC hdc = GetDC(NULL);
    COLORREF color = GetPixel(hdc, pt.x, pt.y);
    ReleaseDC(NULL, hdc);
    
    return color;
}

void UpdateColorDisplay(COLORREF color) {
    g_currentColor = color;
    
    // Update color rectangle
    InvalidateRect(g_hColorRect, NULL, TRUE);
    
    // Update text fields
    std::string hexStr = ColorToHex(color);
    std::string rgbStr = ColorToRGB(color);
    std::string hslStr = ColorToHSL(color);
    
    SetWindowTextA(g_hHexEdit, hexStr.c_str());
    SetWindowTextA(g_hRgbEdit, rgbStr.c_str());
    SetWindowTextA(g_hHslEdit, hslStr.c_str());
    
    // Update coordinates
    std::ostringstream coords;
    coords << "(" << g_mousePos.x << ", " << g_mousePos.y << ")";
    SetWindowTextA(g_hCoordLabel, coords.str().c_str());
}

void StartTracking() {
    if (g_isTracking) return;
    
    g_isTracking = true;
    SetWindowTextA(g_hTrackBtn, "Stop Tracking");
    EnableWindow(g_hPickBtn, FALSE);
    SetWindowTextA(g_hStatusLabel, "Move cursor to sample colors - Click to pick - ESC to stop");
    
    // Set cursor to crosshair
    SetCursor(LoadCursor(NULL, IDC_CROSS));
    
    // Start timer for real-time updates
    SetTimer(g_hMainWnd, ID_TIMER, 33, NULL); // ~30 FPS
}

void StopTracking() {
    if (!g_isTracking) return;
    
    g_isTracking = false;
    SetWindowTextA(g_hTrackBtn, "Start Tracking");
    EnableWindow(g_hPickBtn, TRUE);
    SetWindowTextA(g_hStatusLabel, "Ready to sample colors from your screen");
    
    // Reset cursor
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    
    // Stop timer
    KillTimer(g_hMainWnd, ID_TIMER);
}

void PickCurrentColor() {
    if (g_isTracking) {
        StopTracking();
    }
    
    // Visual feedback
    SetWindowTextA(g_hMainWnd, "xsukax Color Picker - Color Picked!");
    SetWindowTextA(g_hStatusLabel, "Color successfully picked and saved!");
    SetTimer(g_hMainWnd, ID_TIMER + 2, 2500, NULL);
}

// Custom button drawing
void DrawModernButton(HDC hdc, RECT* rect, const char* text, bool isPressed, bool isEnabled) {
    // Button background
    HBRUSH hBrush;
    if (!isEnabled) {
        hBrush = CreateSolidBrush(RGB(233, 236, 239));
    } else if (isPressed) {
        hBrush = CreateSolidBrush(COLOR_PRIMARY_HOVER);
    } else {
        hBrush = CreateSolidBrush(COLOR_PRIMARY);
    }
    
    // Draw rounded rectangle background
    HPEN hPen = CreatePen(PS_SOLID, 1, COLOR_BORDER);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
    
    RoundRect(hdc, rect->left, rect->top, rect->right, rect->bottom, 8, 8);
    
    // Draw text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, isEnabled ? RGB(255, 255, 255) : RGB(134, 142, 150));
    SelectObject(hdc, g_hFontMain);
    DrawTextA(hdc, text, -1, rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);
    DeleteObject(hBrush);
}

// Window procedure for color rectangle
LRESULT CALLBACK ColorRectProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT rect;
            GetClientRect(hwnd, &rect);
            
            // Draw color with modern rounded corners and shadow effect
            HBRUSH hBrush = CreateSolidBrush(g_currentColor);
            HPEN hPen = CreatePen(PS_SOLID, 2, COLOR_BORDER);
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
            
            // Draw shadow
            RECT shadowRect = {rect.left + 2, rect.top + 2, rect.right + 2, rect.bottom + 2};
            HBRUSH hShadowBrush = CreateSolidBrush(RGB(200, 200, 200));
            SelectObject(hdc, hShadowBrush);
            RoundRect(hdc, shadowRect.left, shadowRect.top, shadowRect.right, shadowRect.bottom, 12, 12);
            DeleteObject(hShadowBrush);
            
            // Draw main color rectangle
            SelectObject(hdc, hBrush);
            RoundRect(hdc, rect.left, rect.top, rect.right - 2, rect.bottom - 2, 12, 12);
            
            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBrush);
            DeleteObject(hPen);
            DeleteObject(hBrush);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Main window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            // Create modern fonts
            g_hFontMain = CreateFontA(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            
            g_hFontSmall = CreateFontA(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            
            g_hFontMono = CreateFontA(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_MODERN, "Consolas");
            
            // Create brushes
            g_hBrushBg = CreateSolidBrush(COLOR_BG);
            g_hBrushCard = CreateSolidBrush(COLOR_CARD);
            g_hBrushPrimary = CreateSolidBrush(COLOR_PRIMARY);
            
            // Initialize common controls with modern theme
            INITCOMMONCONTROLSEX icc;
            icc.dwSize = sizeof(icc);
            icc.dwICC = ICC_WIN95_CLASSES;
            InitCommonControlsEx(&icc);
            
            // Create title
            HWND hTitle = CreateWindowA("STATIC", "Color Picker", 
                                       WS_CHILD | WS_VISIBLE | SS_CENTER,
                                       20, 20, 320, 30, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessage(hTitle, WM_SETFONT, (WPARAM)g_hFontMain, TRUE);
            
            // Create color display rectangle
            g_hColorRect = CreateWindowA("ColorRect", "", WS_CHILD | WS_VISIBLE,
                                        20, 60, 320, 80, hwnd, NULL, GetModuleHandle(NULL), NULL);
            
            // Position and coordinates section
            HWND hPosLabel = CreateWindowA("STATIC", "Cursor Position", WS_CHILD | WS_VISIBLE,
                                          20, 155, 120, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessage(hPosLabel, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
            
            g_hCoordLabel = CreateWindowA("STATIC", "(0, 0)", WS_CHILD | WS_VISIBLE,
                                         150, 155, 190, 20, hwnd, (HMENU)ID_COORD_LABEL, GetModuleHandle(NULL), NULL);
            SendMessage(g_hCoordLabel, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
            
            // Control buttons with modern styling
            g_hTrackBtn = CreateWindowA("BUTTON", "Start Tracking", 
                                       WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
                                       20, 185, 150, 40, hwnd, (HMENU)ID_TRACK_BUTTON, GetModuleHandle(NULL), NULL);
            
            g_hPickBtn = CreateWindowA("BUTTON", "Pick Color", 
                                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
                                      190, 185, 150, 40, hwnd, (HMENU)ID_PICK_BUTTON, GetModuleHandle(NULL), NULL);
            
            // Color values section with modern cards
            // HEX section
            CreateWindowA("STATIC", "HEX", WS_CHILD | WS_VISIBLE,
                         20, 245, 40, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
            
            g_hHexEdit = CreateWindowA("EDIT", "#6366F1", 
                                      WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY | ES_CENTER,
                                      70, 243, 140, 26, hwnd, (HMENU)ID_HEX_EDIT, GetModuleHandle(NULL), NULL);
            SendMessage(g_hHexEdit, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
            
            CreateWindowA("BUTTON", "Copy", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                         220, 243, 65, 26, hwnd, (HMENU)ID_COPY_HEX, GetModuleHandle(NULL), NULL);
            
            // RGB section
            CreateWindowA("STATIC", "RGB", WS_CHILD | WS_VISIBLE,
                         20, 280, 40, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
            
            g_hRgbEdit = CreateWindowA("EDIT", "99, 102, 241", 
                                      WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY | ES_CENTER,
                                      70, 278, 140, 26, hwnd, (HMENU)ID_RGB_EDIT, GetModuleHandle(NULL), NULL);
            SendMessage(g_hRgbEdit, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
            
            CreateWindowA("BUTTON", "Copy", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                         220, 278, 65, 26, hwnd, (HMENU)ID_COPY_RGB, GetModuleHandle(NULL), NULL);
            
            // HSL section
            CreateWindowA("STATIC", "HSL", WS_CHILD | WS_VISIBLE,
                         20, 315, 40, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
            
            g_hHslEdit = CreateWindowA("EDIT", "244 deg, 85%, 67%", 
                                      WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY | ES_CENTER,
                                      70, 313, 140, 26, hwnd, (HMENU)ID_HSL_EDIT, GetModuleHandle(NULL), NULL);
            SendMessage(g_hHslEdit, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
            
            CreateWindowA("BUTTON", "Copy", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                         220, 313, 65, 26, hwnd, (HMENU)ID_COPY_HSL, GetModuleHandle(NULL), NULL);
            
            // Status section
            g_hStatusLabel = CreateWindowA("STATIC", "Ready to sample colors from your screen",
                                          WS_CHILD | WS_VISIBLE | SS_CENTER,
                                          20, 355, 320, 20, hwnd, (HMENU)ID_STATUS_LABEL, GetModuleHandle(NULL), NULL);
            SendMessage(g_hStatusLabel, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
            
            // Brand label
            HWND hBrand = CreateWindowA("STATIC", "xsukax Color Picker v2.0", WS_CHILD | WS_VISIBLE | SS_CENTER,
                                       20, 385, 320, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessage(hBrand, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
            
            // Set fonts for all controls
            HWND hChild = GetWindow(hwnd, GW_CHILD);
            while (hChild) {
                char className[256];
                GetClassNameA(hChild, className, sizeof(className));
                if (strcmp(className, "STATIC") == 0) {
                    SendMessage(hChild, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
                } else if (strcmp(className, "BUTTON") == 0) {
                    SendMessage(hChild, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
                }
                hChild = GetWindow(hChild, GW_HWNDNEXT);
            }
            
            // Initialize with default color
            UpdateColorDisplay(g_currentColor);
            
            return 0;
        }
        
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, COLOR_TEXT);
            SetBkColor(hdcStatic, COLOR_BG);
            return (LRESULT)g_hBrushBg;
        }
        
        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, COLOR_TEXT);
            SetBkColor(hdcEdit, COLOR_CARD);
            return (LRESULT)g_hBrushCard;
        }
        
        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
            if (dis->CtlType == ODT_BUTTON) {
                char text[256];
                GetWindowTextA(dis->hwndItem, text, sizeof(text));
                bool isPressed = (dis->itemState & ODS_SELECTED) != 0;
                bool isEnabled = (dis->itemState & ODS_DISABLED) == 0;
                DrawModernButton(dis->hDC, &dis->rcItem, text, isPressed, isEnabled);
                return TRUE;
            }
            break;
        }
        
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case ID_TRACK_BUTTON:
                    if (g_isTracking) {
                        StopTracking();
                    } else {
                        StartTracking();
                    }
                    break;
                    
                case ID_PICK_BUTTON:
                    PickCurrentColor();
                    break;
                    
                case ID_COPY_HEX: {
                    char buffer[256];
                    GetWindowTextA(g_hHexEdit, buffer, sizeof(buffer));
                    CopyToClipboard(std::string(buffer));
                    break;
                }
                
                case ID_COPY_RGB: {
                    char buffer[256];
                    GetWindowTextA(g_hRgbEdit, buffer, sizeof(buffer));
                    CopyToClipboard(std::string(buffer));
                    break;
                }
                
                case ID_COPY_HSL: {
                    char buffer[256];
                    GetWindowTextA(g_hHslEdit, buffer, sizeof(buffer));
                    CopyToClipboard(std::string(buffer));
                    break;
                }
            }
            return 0;
        }
        
        case WM_TIMER: {
            switch (wParam) {
                case ID_TIMER:
                    if (g_isTracking) {
                        COLORREF color = GetPixelAtCursor();
                        UpdateColorDisplay(color);
                    }
                    break;
                    
                case ID_TIMER + 1:
                    SetWindowTextA(g_hMainWnd, "xsukax Color Picker");
                    KillTimer(hwnd, ID_TIMER + 1);
                    break;
                    
                case ID_TIMER + 2:
                    SetWindowTextA(g_hMainWnd, "xsukax Color Picker");
                    SetWindowTextA(g_hStatusLabel, "Ready to sample colors from your screen");
                    KillTimer(hwnd, ID_TIMER + 2);
                    break;
            }
            return 0;
        }
        
        case WM_KEYDOWN: {
            if (wParam == VK_ESCAPE && g_isTracking) {
                StopTracking();
            }
            return 0;
        }
        
        case WM_CLOSE:
            StopTracking();
            DestroyWindow(hwnd);
            return 0;
            
        case WM_DESTROY:
            // Cleanup resources
            if (g_hFontMain) DeleteObject(g_hFontMain);
            if (g_hFontSmall) DeleteObject(g_hFontSmall);
            if (g_hFontMono) DeleteObject(g_hFontMono);
            if (g_hBrushBg) DeleteObject(g_hBrushBg);
            if (g_hBrushCard) DeleteObject(g_hBrushCard);
            if (g_hBrushPrimary) DeleteObject(g_hBrushPrimary);
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Hide console window if it exists
    HWND consoleWnd = GetConsoleWindow();
    if (consoleWnd != NULL) {
        ShowWindow(consoleWnd, SW_HIDE);
    }
    
    // Register color rectangle window class
    WNDCLASSA colorWc = {};
    colorWc.lpfnWndProc = ColorRectProc;
    colorWc.hInstance = hInstance;
    colorWc.lpszClassName = "ColorRect";
    colorWc.hbrBackground = CreateSolidBrush(COLOR_BG);
    colorWc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&colorWc);
    
    // Register main window class
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "ModernColorPicker";
    wc.hbrBackground = CreateSolidBrush(COLOR_BG);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassA(&wc);
    
    // Create main window with modern styling
    g_hMainWnd = CreateWindowA("ModernColorPicker", "xsukax Color Picker",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, 380, 450,
                              NULL, NULL, hInstance, NULL);
    
    if (!g_hMainWnd) {
        MessageBoxA(NULL, "Failed to create window!", "Error", MB_ICONERROR);
        return 1;
    }
    
    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);
    
    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}