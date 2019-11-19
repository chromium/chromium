// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/sandbox_poc/main_ui_window.h"

#include <windows.h>
#include <CommCtrl.h>
#include <commdlg.h>
#include <stdarg.h>
#include <stddef.h>
#include <time.h>
#include <windowsx.h>

#include <algorithm>
#include <sstream>

#include "base/logging.h"
#include "base/win/atl.h"
#include "sandbox/win/sandbox_poc/resource.h"
#include "sandbox/win/src/acl.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/win_utils.h"

HWND MainUIWindow::list_view_ = NULL;

const wchar_t MainUIWindow::kDefaultDll_[]        = L"\\POCDLL.dll";
const wchar_t MainUIWindow::kDefaultEntryPoint_[] = L"Run";
const wchar_t MainUIWindow::kDefaultLogFile_[]    = L"";

MainUIWindow::MainUIWindow()
    : broker_(NULL),
      spawn_target_(L""),
      instance_handle_(NULL),
      dll_path_(L""),
      entry_point_(L"") {
}

MainUIWindow::~MainUIWindow() {
}

unsigned int MainUIWindow::CreateMainWindowAndLoop(
    HINSTANCE instance,
    wchar_t* command_line,
    int show_command,
    sandbox::BrokerServices* broker) {
  DCHECK(instance);
  DCHECK(command_line);
  DCHECK(broker);

  instance_handle_ = instance;
  spawn_target_ = command_line;
  broker_ = broker;

  // We'll use spawn_target_ later for creating a child process, but
  // CreateProcess doesn't like double quotes, so we remove them along with
  // tabs and spaces from the start and end of the string
  const wchar_t *trim_removal = L" \r\t\"";
  spawn_target_.erase(0, spawn_target_.find_first_not_of(trim_removal));
  spawn_target_.erase(spawn_target_.find_last_not_of(trim_removal) + 1);

  WNDCLASSEX window_class = {0};
  window_class.cbSize        = sizeof(WNDCLASSEX);
  window_class.style         = CS_HREDRAW | CS_VREDRAW;
  window_class.lpfnWndProc   = MainUIWindow::WndProc;
  window_class.cbClsExtra    = 0;
  window_class.cbWndExtra    = 0;
  window_class.hInstance     = instance;
  window_class.hIcon         =
      ::LoadIcon(instance, MAKEINTRESOURCE(IDI_SANDBOX));
  window_class.hCursor       = ::LoadCursor(NULL, IDC_ARROW);
  window_class.hbrBackground = GetStockBrush(WHITE_BRUSH);
  window_class.lpszMenuName  = MAKEINTRESOURCE(IDR_MENU_MAIN_UI);
  window_class.lpszClassName = L"sandbox_ui_1";
  window_class.hIconSm       = NULL;

  INITCOMMONCONTROLSEX controls = {
    sizeof(INITCOMMONCONTROLSEX),
    ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES
  };
  ::InitCommonControlsEx(&controls);

  if (!::RegisterClassEx(&window_class))
    return ::GetLastError();

  // Create a main window of size 600x400
  HWND window = ::CreateWindowW(window_class.lpszClassName,
                                L"",            // window name
                                WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT,  // x
                                CW_USEDEFAULT,  // y
                                600,            // width
                                400,            // height
                                NULL,           // parent
                                NULL,           // NULL = use class menu
                                instance,
                                0);             // lpParam

  if (NULL == window)
    return ::GetLastError();

  ::SetWindowLongPtr(window,
                     GWLP_USERDATA,
                     reinterpret_cast<LONG_PTR>(this));

  ::SetWindowText(window, L"Sandbox Proof of Concept");

  ::ShowWindow(window, show_command);

  MSG message;
  // Now lets start the message pump retrieving messages for any window that
  // belongs to the current thread
  while (::GetMessage(&message, NULL, 0, 0)) {
    ::TranslateMessage(&message);
    ::DispatchMessage(&message);
  }

  return 0;
}

LRESULT CALLBACK MainUIWindow::WndProc(HWND window,
                                       UINT message_id,
                                       WPARAM wparam,
                                       LPARAM lparam) {
  MainUIWindow* host = FromWindow(window);

  #define HANDLE_MSG(hwnd, message, fn)    \
    case (message): return HANDLE_##message((hwnd), (wParam), (lParam), (fn))

  switch (message_id) {
    case WM_CREATE:
      // 'host' is not yet available when we get the WM_CREATE message
      return HANDLE_WM_CREATE(window, wparam, lparam, OnCreate);
    case WM_DESTROY:
      return HANDLE_WM_DESTROY(window, wparam, lparam, host->OnDestroy);
    case WM_SIZE:
      return HANDLE_WM_SIZE(window, wparam, lparam, host->OnSize);
    case WM_COMMAND: {
      // Look at which menu item was clicked on (or which accelerator)
      int id = LOWORD(wparam);
      switch (id) {
        case ID_FILE_EXIT:
          host->OnFileExit();
          break;
        case ID_COMMANDS_SPAWNTARGET:
          host->OnCommandsLaunch(window);
          break;
        default:
          // Some other menu item or accelerator
          break;
      }

      return ERROR_SUCCESS;
    }

    default:
      // Some other WM_message, let it pass to DefWndProc
      break;
  }

  return DefWindowProc(window, message_id, wparam, lparam);
}

INT_PTR CALLBACK MainUIWindow::SpawnTargetWndProc(HWND dialog,
                                                  UINT message_id,
                                                  WPARAM wparam,
                                                  LPARAM lparam) {
  // Grab a reference to the main UI window (from the window handle)
  MainUIWindow* host = FromWindow(GetParent(dialog));
  DCHECK(host);

  switch (message_id) {
    case WM_INITDIALOG: {
      // Initialize the window text for DLL name edit box
      HWND edit_box_dll_name = ::GetDlgItem(dialog, IDC_DLL_NAME);
      wchar_t current_dir[MAX_PATH];
      if (GetCurrentDirectory(MAX_PATH, current_dir)) {
        std::wstring dll_path =
            std::wstring(current_dir) + std::wstring(kDefaultDll_);
        ::SetWindowText(edit_box_dll_name, dll_path.c_str());
      }

      // Initialize the window text for Entry Point edit box
      HWND edit_box_entry_point = ::GetDlgItem(dialog, IDC_ENTRY_POINT);
      ::SetWindowText(edit_box_entry_point, kDefaultEntryPoint_);

      // Initialize the window text for Log File edit box
      HWND edit_box_log_file = ::GetDlgItem(dialog, IDC_LOG_FILE);
      ::SetWindowText(edit_box_log_file, kDefaultLogFile_);

      return static_cast<INT_PTR>(TRUE);
    }
    case WM_COMMAND:
      // If the user presses the OK button (Launch)
      if (LOWORD(wparam) == IDOK) {
        if (host->OnLaunchDll(dialog)) {
          if (host->SpawnTarget()) {
            ::EndDialog(dialog, LOWORD(wparam));
          }
        }
        return static_cast<INT_PTR>(TRUE);
      } else if (LOWORD(wparam) == IDCANCEL) {
        // If the user presses the Cancel button
        ::EndDialog(dialog, LOWORD(wparam));
        return static_cast<INT_PTR>(TRUE);
      } else if (LOWORD(wparam) == IDC_BROWSE_DLL) {
        // If the user presses the Browse button to look for a DLL
        std::wstring dll_path = host->OnShowBrowseForDllDlg(dialog);
        if (dll_path.length() > 0) {
          // Initialize the window text for Log File edit box
          HWND edit_box_dll_path = ::GetDlgItem(dialog, IDC_DLL_NAME);
          ::SetWindowText(edit_box_dll_path, dll_path.c_str());
        }
        return static_cast<INT_PTR>(TRUE);
      } else if (LOWORD(wparam) == IDC_BROWSE_LOG) {
        // If the user presses the Browse button to look for a log file
        std::wstring log_path = host->OnShowBrowseForLogFileDlg(dialog);
        if (log_path.length() > 0) {
          // Initialize the window text for Log File edit box
          HWND edit_box_log_file = ::GetDlgItem(dialog, IDC_LOG_FILE);
          ::SetWindowText(edit_box_log_file, log_path.c_str());
        }
        return static_cast<INT_PTR>(TRUE);
      }

      break;
  }

  return static_cast<INT_PTR>(FALSE);
}

MainUIWindow* MainUIWindow::FromWindow(HWND main_window) {
  // We store a 'this' pointer using SetWindowLong in CreateMainWindowAndLoop
  // so that we can retrieve it with this function later. This prevents us
  // from having to define all the message handling functions (that we refer to
  // in the window proc) as static
  ::GetWindowLongPtr(main_window, GWLP_USERDATA);
  return reinterpret_cast<MainUIWindow*>(
      ::GetWindowLongPtr(main_window, GWLP_USERDATA));
}

BOOL MainUIWindow::OnCreate(HWND parent_window, LPCREATESTRUCT) {
  // Create the listview that will the main app UI
  list_view_ = ::CreateWindow(WC_LISTVIEW,    // Class name
                              L"",            // Window name
                              WS_CHILD | WS_VISIBLE | LVS_REPORT |
                              LVS_NOCOLUMNHEADER | WS_BORDER,
                              0,              // x
                              0,              // y
                              0,              // width
                              0,              // height
                              parent_window,  // parent
                              NULL,           // menu
                              ::GetModuleHandle(NULL),
                              0);             // lpParam

  DCHECK(list_view_);
  if (!list_view_)
    return FALSE;

  LVCOLUMN list_view_column = {0};
  list_view_column.mask = LVCF_FMT | LVCF_WIDTH;
  list_view_column.fmt = LVCFMT_LEFT;
  list_view_column.cx = 10000;  // Maximum size of an entry in the list view.
  ListView_InsertColumn(list_view_, 0, &list_view_column);

  // Set list view to show green font on black background
  ListView_SetBkColor(list_view_, CLR_NONE);
  ListView_SetTextColor(list_view_, RGB(0x0, 0x0, 0x0));
  ListView_SetTextBkColor(list_view_, CLR_NONE);

  return TRUE;
}

void MainUIWindow::OnDestroy(HWND window) {
  // Post a quit message because our application is over when the
  // user closes this window.
  ::PostQuitMessage(0);
}

void MainUIWindow::OnSize(HWND window, UINT state, int cx, int cy) {
  // If we have a valid inner child, resize it to cover the entire
  // client area of the main UI window.
  if (list_view_) {
    ::MoveWindow(list_view_,
                 0,      // x
                 0,      // y
                 cx,     // width
                 cy,     // height
                 TRUE);  // repaint
  }
}

void MainUIWindow::OnPaint(HWND window) {
  PAINTSTRUCT paintstruct;
  ::BeginPaint(window, &paintstruct);
  // add painting code here if required
  ::EndPaint(window, &paintstruct);
}

void MainUIWindow::OnFileExit() {
  ::PostQuitMessage(0);
}

void MainUIWindow::OnCommandsLaunch(HWND window) {
  // User wants to see the Select DLL dialog box
  ::DialogBox(instance_handle_,
              MAKEINTRESOURCE(IDD_LAUNCH_DLL),
              window,
              SpawnTargetWndProc);
}

bool MainUIWindow::OnLaunchDll(HWND dialog) {
  HWND edit_box_dll_name = ::GetDlgItem(dialog, IDC_DLL_NAME);
  HWND edit_box_entry_point = ::GetDlgItem(dialog, IDC_ENTRY_POINT);
  HWND edit_log_file = ::GetDlgItem(dialog, IDC_LOG_FILE);

  wchar_t dll_path[MAX_PATH];
  wchar_t entry_point[MAX_PATH];
  wchar_t log_file[MAX_PATH];

  int dll_name_len    = ::GetWindowText(edit_box_dll_name, dll_path, MAX_PATH);
  int entry_point_len = ::GetWindowText(edit_box_entry_point,
                                        entry_point, MAX_PATH);
  // Log file is optional (can be blank)
  ::GetWindowText(edit_log_file, log_file, MAX_PATH);

  if (0 >= dll_name_len) {
    ::MessageBox(dialog,
                 L"Please specify a DLL for the target to load",
                 L"No DLL specified",
                 MB_ICONERROR);
    return false;
  }

  if (GetFileAttributes(dll_path) == INVALID_FILE_ATTRIBUTES) {
    ::MessageBox(dialog,
                 L"DLL specified was not found",
                 L"DLL not found",
                 MB_ICONERROR);
    return false;
  }

  if (0 >= entry_point_len) {
    ::MessageBox(dialog,
                 L"Please specify an entry point for the DLL",
                 L"No entry point specified",
                 MB_ICONERROR);
    return false;
  }

  // store these values in the member variables for use in SpawnTarget
  log_file_ = std::wstring(L"\"") + log_file + std::wstring(L"\"");
  dll_path_ = dll_path;
  entry_point_ = entry_point;

  return true;
}

DWORD WINAPI MainUIWindow::ListenPipeThunk(void *param) {
  return reinterpret_cast<MainUIWindow*>(param)->ListenPipe();
}

DWORD WINAPI MainUIWindow::WaitForTargetThunk(void *param) {
  return reinterpret_cast<MainUIWindow*>(param)->WaitForTarget();
}

// Thread waiting for the target application to die. It displays
// a message in the list view when it happens.
DWORD MainUIWindow::WaitForTarget() {
  WaitForSingleObject(target_.hProcess, INFINITE);

  DWORD exit_code = 0;
  if (!GetExitCodeProcess(target_.hProcess, &exit_code)) {
    exit_code = 0xFFFF;  // Default exit code
  }

  ::CloseHandle(target_.hProcess);
  ::CloseHandle(target_.hThread);

  AddDebugMessage(L"Targed exited with return code %d", exit_code);
  return 0;
}

// Thread waiting for messages on the log pipe. It displays the messages
// in the listview.
DWORD MainUIWindow::ListenPipe() {
  HANDLE logfile_handle = NULL;
  ATL::CString file_to_open = log_file_.c_str();
  file_to_open.Remove(L'\"');
  if (file_to_open.GetLength()) {
    logfile_handle = ::CreateFile(file_to_open.GetBuffer(),
                                  GENERIC_WRITE,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  NULL,  // Default security attributes
                                  CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL,
                                  NULL);  // No template
    if (INVALID_HANDLE_VALUE == logfile_handle) {
      AddDebugMessage(L"Failed to open \"%ls\" for logging. Error %d",
                      file_to_open.GetBuffer(), ::GetLastError());
      logfile_handle = NULL;
    }
  }

  const int kSizeBuffer = 1024;
  BYTE read_buffer[kSizeBuffer] = {0};
  ATL::CStringA read_buffer_global;
  ATL::CStringA string_to_print;

  DWORD last_error = 0;
  while (last_error == ERROR_SUCCESS || last_error == ERROR_PIPE_LISTENING ||
         last_error == ERROR_NO_DATA) {
    DWORD read_data_length;
    if (::ReadFile(pipe_handle_,
                  read_buffer,
                  kSizeBuffer - 1,  // Max read size
                  &read_data_length,
                  NULL)) {  // Not overlapped
      if (logfile_handle) {
        DWORD write_data_length;
        ::WriteFile(logfile_handle,
                    read_buffer,
                    read_data_length,
                    &write_data_length,
                    FALSE);  // Not overlapped
      }

      // Append the new buffer to the current buffer
      read_buffer[read_data_length] = NULL;
      read_buffer_global += reinterpret_cast<char *>(read_buffer);
      read_buffer_global.Remove(10);  // Remove the CRs

      // If we completed a new line, output it
      int endline = read_buffer_global.Find(13);  // search for LF
      while (-1 != endline) {
        string_to_print = read_buffer_global;
        string_to_print.Delete(endline, string_to_print.GetLength());
        read_buffer_global.Delete(0, endline);

        //  print the line (with the ending LF)
        OutputDebugStringA(string_to_print.GetBuffer());

        // Remove the ending LF
        read_buffer_global.Delete(0, 1);

        // Add the line to the log
        AddDebugMessage(L"%S", string_to_print.GetBuffer());

        endline = read_buffer_global.Find(13);
      }
      last_error = ERROR_SUCCESS;
    } else {
      last_error = GetLastError();
      Sleep(100);
    }
  }

  if (read_buffer_global.GetLength()) {
    AddDebugMessage(L"%S", read_buffer_global.GetBuffer());
  }

  CloseHandle(pipe_handle_);

  if (logfile_handle) {
    CloseHandle(logfile_handle);
  }

  return 0;
}

bool MainUIWindow::SpawnTarget() {
  // Generate the pipe name
  GUID random_id;
  CoCreateGuid(&random_id);

  wchar_t log_pipe[MAX_PATH] = {0};
  wnsprintf(log_pipe, MAX_PATH - 1,
            L"\\\\.\\pipe\\sbox_pipe_log_%lu_%lu_%lu_%lu",
            random_id.Data1,
            random_id.Data2,
            random_id.Data3,
            random_id.Data4);

  // We concatenate the four strings, add three spaces and a zero termination
  // We use the resulting string as a param to CreateProcess (in SpawnTarget)
  // Documented maximum for command line in CreateProcess is 32K (msdn)
  size_t size_call = spawn_target_.length() + entry_point_.length() +
                  dll_path_.length() + wcslen(log_pipe) + 6;
  if (32 * 1024 < (size_call * sizeof(wchar_t))) {
    AddDebugMessage(L"The length of the arguments exceeded 32K. "
                    L"Aborting operation.");
    return false;
  }

  wchar_t * arguments = new wchar_t[size_call];
  wnsprintf(arguments, static_cast<int>(size_call), L"%ls %ls \"%ls\" %ls",
            spawn_target_.c_str(), entry_point_.c_str(),
            dll_path_.c_str(), log_pipe);

  arguments[size_call - 1] = L'\0';

  scoped_refptr<sandbox::TargetPolicy> policy = broker_->CreatePolicy();
  policy->SetJobLevel(sandbox::JOB_LOCKDOWN, 0);
  policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                        sandbox::USER_LOCKDOWN);
  policy->SetAlternateDesktop(true);
  policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);

  // Set the rule to allow the POC dll to be loaded by the target. Note that
  // the rule allows 'all access' to the DLL, which could mean that the target
  // could modify the DLL on disk.
  policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                  sandbox::TargetPolicy::FILES_ALLOW_ANY, dll_path_.c_str());

  sandbox::ResultCode warning_result = sandbox::SBOX_ALL_OK;
  DWORD last_error = ERROR_SUCCESS;
  sandbox::ResultCode result =
      broker_->SpawnTarget(spawn_target_.c_str(), arguments, policy,
                           &warning_result, &last_error, &target_);

  policy.reset();

  bool return_value = false;
  if (sandbox::SBOX_ALL_OK != result) {
    AddDebugMessage(
        L"Failed to spawn target %ls w/args (%ls), sandbox error code: %d",
        spawn_target_.c_str(), arguments, result);
    return_value = false;
  } else {
    DWORD thread_id;
    ::CreateThread(NULL,  // Default security attributes
                   NULL,  // Default stack size
                   &MainUIWindow::WaitForTargetThunk,
                   this,
                   0,  // No flags
                   &thread_id);

    pipe_handle_ = ::CreateNamedPipe(log_pipe,
                                     PIPE_ACCESS_INBOUND | WRITE_DAC,
                                     PIPE_TYPE_MESSAGE | PIPE_NOWAIT,
                                     1,  // Number of instances.
                                     512,  // Out buffer size.
                                     512,  // In buffer size.
                                     NMPWAIT_USE_DEFAULT_WAIT,
                                     NULL);  // Default security descriptor

    if (INVALID_HANDLE_VALUE == pipe_handle_)
      AddDebugMessage(L"Failed to create pipe. Error %d", ::GetLastError());

    if (!sandbox::AddKnownSidToObject(pipe_handle_, SE_KERNEL_OBJECT,
                                      WinWorldSid, GRANT_ACCESS,
                                      FILE_ALL_ACCESS))
      AddDebugMessage(L"Failed to set security on pipe. Error %d",
                      ::GetLastError());

    ::CreateThread(NULL,  // Default security attributes
                   NULL,  // Default stack size
                   &MainUIWindow::ListenPipeThunk,
                   this,
                   0,  // No flags
                   &thread_id);

    ::ResumeThread(target_.hThread);

    AddDebugMessage(L"Successfully spawned target w/args (%ls)", arguments);
    return_value = true;
  }

  delete[] arguments;
  return return_value;
}

std::wstring MainUIWindow::OnShowBrowseForDllDlg(HWND owner) {
  wchar_t filename[MAX_PATH];
  wcscpy_s(filename, MAX_PATH, L"");

  OPENFILENAMEW file_info = {0};
  file_info.lStructSize = sizeof(file_info);
  file_info.hwndOwner = owner;
  file_info.lpstrFile = filename;
  file_info.nMaxFile = MAX_PATH;
  file_info.lpstrFilter = L"DLL files (*.dll)\0*.dll\0All files\0*.*\0\0\0";

  file_info.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

  if (GetOpenFileName(&file_info)) {
    return file_info.lpstrFile;
  }

  return L"";
}

std::wstring MainUIWindow::OnShowBrowseForLogFileDlg(HWND owner) {
  wchar_t filename[MAX_PATH];
  wcscpy_s(filename, MAX_PATH, L"");

  OPENFILENAMEW file_info = {0};
  file_info.lStructSize = sizeof(file_info);
  file_info.hwndOwner = owner;
  file_info.lpstrFile = filename;
  file_info.nMaxFile = MAX_PATH;
  file_info.lpstrFilter = L"Log file (*.txt)\0*.txt\0All files\0*.*\0\0\0";

  file_info.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

  if (GetSaveFileName(&file_info)) {
    return file_info.lpstrFile;
  }

  return L"";
}

void MainUIWindow::AddDebugMessage(const wchar_t* format, ...) {
  DCHECK(format);
  if (!format)
    return;

  const int kMaxDebugBuffSize = 1024;

  va_list arg_list;
  va_start(arg_list, format);

  wchar_t text[kMaxDebugBuffSize + 1];
  vswprintf_s(text, kMaxDebugBuffSize, format, arg_list);
  text[kMaxDebugBuffSize] = L'\0';

  InsertLineInListView(text);
  va_end(arg_list);
}


void MainUIWindow::InsertLineInListView(wchar_t* debug_message) {
  DCHECK(debug_message);
  if (!debug_message)
    return;

  // Prepend the time to the message
  const int kSizeTime = 100;
  size_t size_message_with_time = wcslen(debug_message) + kSizeTime;
  wchar_t * message_time = new wchar_t[size_message_with_time];

  time_t time_temp;
  time_temp = time(NULL);

  struct tm time = {0};
  localtime_s(&time, &time_temp);

  size_t return_code;
  return_code = wcsftime(message_time, kSizeTime, L"[%H:%M:%S] ", &time);

  wcscat_s(message_time, size_message_with_time, debug_message);

  // We add the debug message to the top of the listview
  LVITEM item;
  item.iItem = ListView_GetItemCount(list_view_);
  item.iSubItem = 0;
  item.mask = LVIF_TEXT | LVIF_PARAM;
  item.pszText = message_time;
  item.lParam = 0;

  ListView_InsertItem(list_view_, &item);

  delete[] message_time;
}
