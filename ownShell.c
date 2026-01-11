#include <stdio.h>
#include <windows.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <direct.h>

#define IDC_SHELL 101

// --- Functions ---
void ListDirectory(HWND hwnd);
void UpdateDirectory();
void UpdateProtectionLimit(HWND hwnd);
void ExecuteCommand(HWND hwnd, const char* cmd);

// --- Structures ---
typedef struct CommandNode {
    char command[256];
    struct CommandNode *prev;
    struct CommandNode *next;
} CommandNode;

// --- Globals ---
HWND hShell;
WNDPROC OldEditProc;
int protectionLimit = 0;
HBRUSH hbrBackground = NULL;

char currentInputBuffer[256] = "";
char currentDir[MAX_PATH];

CommandNode *historyHead = NULL;
CommandNode *historyTail = NULL;
CommandNode *historyCurrent = NULL;

// --- Functions Logic ---

void UpdateDirectory() {
    _getcwd(currentDir, MAX_PATH);
}

void UpdateProtectionLimit(HWND hwnd) {
    protectionLimit = GetWindowTextLength(hwnd);
}

void AddToHistory(const char* cmd) {
    if (strlen(cmd) == 0) return;
    CommandNode* newNode = (CommandNode*)malloc(sizeof(CommandNode));
    strncpy(newNode->command, cmd, 255);
    newNode->command[255] = '\0';
    newNode->next = NULL;
    newNode->prev = historyTail;
    if (historyTail) historyTail->next = newNode;
    historyTail = newNode;
    if (!historyHead) historyHead = newNode;
    historyCurrent = NULL;
}

void ListDirectory(HWND hwnd) {
    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile("*", &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        SendMessage(hwnd, EM_REPLACESEL, 0, (LPARAM)"\r\nError accessing directory.");
        return;
    }
    SendMessage(hwnd, EM_REPLACESEL, 0, (LPARAM)"\r\n");
    do {
        char entry[MAX_PATH + 30];
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            sprintf(entry, "<DIR>    %s\r\n", findData.cFileName);
        } else {
            sprintf(entry, "         %s\r\n", findData.cFileName);
        }
        SendMessage(hwnd, EM_REPLACESEL, 0, (LPARAM)entry);
    } while (FindNextFile(hFind, &findData) != 0);
    FindClose(hFind);
}

void ExecuteCommand(HWND hwnd, const char* cmd) {
    char cleanCmd[256] = {0};
    if (sscanf(cmd, "%s", cleanCmd) <= 0) return;

    if (strcmp(cleanCmd, "dir") == 0 || strcmp(cleanCmd, "ls") == 0) {
        ListDirectory(hwnd);
    }
    else if (strcmp(cleanCmd, "cls") == 0) {
        SetWindowText(hwnd, ""); 
        SendMessage(hwnd, EM_REPLACESEL, 0, (LPARAM)"ownShell v1.0\r\n");
    }
    else if (strcmp(cleanCmd, "~") == 0) {
        _chdir(getenv("USERPROFILE"));
    } 
    else if (strncmp(cmd, "cd ", 3) == 0) {
        const char* path = cmd + 3;
        if (_chdir(path) != 0) {
            SendMessage(hwnd, EM_REPLACESEL, 0, (LPARAM)"\r\nDirectory not found.");
        }
    }
    UpdateDirectory();
}

// --- Windows Messaging ---

LRESULT CALLBACK ShellSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_LBUTTONDOWN || uMsg == WM_SETFOCUS) {
        LRESULT res = CallWindowProc(OldEditProc, hwnd, uMsg, wParam, lParam);
        int start, end;
        SendMessage(hwnd, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
        if (start < protectionLimit) {
            SendMessage(hwnd, EM_SETSEL, (WPARAM)protectionLimit, (LPARAM)protectionLimit);
        }
        return res;
    }

    if (uMsg == WM_KEYDOWN) {
        if (wParam == VK_UP || wParam == VK_DOWN) {
            int currentTextLen = GetWindowTextLength(hwnd);
            if (wParam == VK_UP) {
                if (historyCurrent == NULL) {
                    char* fullText = (char*)malloc(currentTextLen + 1);
                    GetWindowText(hwnd, fullText, currentTextLen + 1);
                    char* lastPrompt = strrchr(fullText, '>');
                    if (lastPrompt && *(lastPrompt + 1) == ' ') strcpy(currentInputBuffer, lastPrompt + 2);
                    else strcpy(currentInputBuffer, "");
                    free(fullText);
                    historyCurrent = historyTail;
                } else if (historyCurrent->prev) historyCurrent = historyCurrent->prev;
            } else {
                if (historyCurrent) historyCurrent = historyCurrent->next;
            }
            const char* textToSet = (historyCurrent != NULL) ? historyCurrent->command : currentInputBuffer;
            SendMessage(hwnd, EM_SETSEL, (WPARAM)protectionLimit, (LPARAM)currentTextLen);
            SendMessage(hwnd, EM_REPLACESEL, 0, (LPARAM)textToSet);
            return 0;
        }

        int start, end;
        SendMessage(hwnd, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
        if (start < protectionLimit || end < protectionLimit) {
            if (!(wParam == VK_LEFT || wParam == VK_RIGHT || wParam == VK_UP || wParam == VK_DOWN)) return 0;
        }
    }

    if (uMsg == WM_CHAR) {
        if (wParam == VK_RETURN) {
            int len = GetWindowTextLength(hwnd);
            char* buffer = (char*)malloc(len + 1);
            GetWindowText(hwnd, buffer, len + 1);
            char* lastPrompt = strrchr(buffer, '>');
            if (lastPrompt && *(lastPrompt + 1) == ' ') {
                char cmd[256];
                strcpy(cmd, lastPrompt + 2);
                AddToHistory(cmd);
                ExecuteCommand(hwnd, cmd);
            }
            free(buffer);
            strcpy(currentInputBuffer, "");
            char newPrompt[MAX_PATH + 10];
            sprintf(newPrompt, "\r\n%s> ", currentDir);
            int newLen = GetWindowTextLength(hwnd);
            SendMessage(hwnd, EM_SETSEL, (WPARAM)newLen, (LPARAM)newLen);
            SendMessage(hwnd, EM_REPLACESEL, 0, (LPARAM)newPrompt);
            UpdateProtectionLimit(hwnd);
            return 0;
        }
        int start, end;
        SendMessage(hwnd, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
        if (start < protectionLimit) return 0;
    }
    return CallWindowProc(OldEditProc, hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            UpdateDirectory();
            char initialMsg[MAX_PATH + 100];
            sprintf(initialMsg, "ownShell v1.0\r\n\r\n%s> ", currentDir);
            hShell = CreateWindowEx(0, "EDIT", initialMsg,
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                0, 0, 0, 0, hwnd, (HMENU)IDC_SHELL, GetModuleHandle(NULL), NULL);
            SendMessage(hShell, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT), TRUE);
            OldEditProc = (WNDPROC)SetWindowLongPtr(hShell, GWLP_WNDPROC, (LONG_PTR)ShellSubclassProc);
            UpdateProtectionLimit(hShell);
            SetFocus(hShell);
            BOOL USE_DARK_MODE = TRUE;
            DwmSetWindowAttribute(hwnd, 20, &USE_DARK_MODE, sizeof(USE_DARK_MODE));
            SetWindowTheme(hShell, L"DarkMode_Explorer", NULL);
            break;
        case WM_SIZE:
            MoveWindow(hShell, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
            break;
        case WM_CTLCOLOREDIT: {
            HDC hdcStatic = (HDC)wParam;
            if (hbrBackground == NULL) hbrBackground = CreateSolidBrush(RGB(45, 45, 45));
            SetTextColor(hdcStatic, RGB(200, 200, 200));
            SetBkColor(hdcStatic, RGB(45, 45, 45));
            return (LRESULT)hbrBackground;
        }
        case WM_DESTROY:
            DeleteObject(hbrBackground);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const char CLASS_NAME[] = "ownShellWindowClass";
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(45, 45, 45));
    RegisterClass(&wc);
    HWND hwnd = CreateWindowEx(0, CLASS_NAME, "ownShell", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 400, NULL, NULL, hInstance, NULL);
    if (hwnd == NULL) return 0;
    ShowWindow(hwnd, nCmdShow);
    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}