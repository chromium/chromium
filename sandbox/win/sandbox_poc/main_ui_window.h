// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SANDBOX_POC_MAIN_UI_WINDOW_H__
#define SANDBOX_SANDBOX_POC_MAIN_UI_WINDOW_H__

#include <windows.h>

#include <string>

#include "base/macros.h"

namespace sandbox {
class BrokerServices;
}

// Header file for the MainUIWindow, a simple window with a menu bar that
// can pop up a dialog (accessible through the menu bar), to specify a path and
// filename of any DLL to load and choose an entry point of their choice
// (note: only entry points with no parameters are expected to work).
//
// The purpose of this is to be able to spawn an EXE inside a SandBox, have it
// load a DLL and call the entry point on it to test how it behaves inside the
// sandbox. This is useful for developer debugging and for security testing.
//
// The MainUIWindow also has a listview that displays debugging information to
// the user.
//
// Sample usage:
//
//    MainUIWindow window;
//    unsigned int ret = window.CreateMainWindowAndLoop(
//        handle_to_current_instance,
//        ::GetCommandLineW(),
//        show_command,
//        broker);
//
// The CreateMainWindowAndLoop() contains a message loop that ends when the
// user closes the MainUIWindow.

// This class encapsulates the Main UI window for the broker application.
// It simply shows a menu that gives the user the ability (through a dialog) to
// specify a DLL and what entry point to call in the DLL.
class MainUIWindow {
 public:
  MainUIWindow();
  ~MainUIWindow();

  // Creates the main window, displays it and starts the message pump. This
  // call will not return until user closes the main UI window that appears
  // as a result. Arguments 'instance', 'command_line' and 'show_cmd' can be
  // passed in directly from winmain. The 'broker' argument is a pointer to a
  // BrokerService that will launch a new EXE inside the sandbox and load the
  // DLL of the user's choice.
  unsigned int CreateMainWindowAndLoop(HINSTANCE instance,
                                       wchar_t* command_line,
                                       int show_command,
                                       sandbox::BrokerServices* broker);

 private:
  // The default value DLL name to add to the edit box.
  static const wchar_t kDefaultDll_[];

  // The default value to show in the entry point.
  static const wchar_t kDefaultEntryPoint_[];

  // The default value to show in the log file.
  static const wchar_t kDefaultLogFile_[];

  // Handles the messages sent to the main UI window. The return value is the
  // result of the message processing and depends on the message.
  static LRESULT CALLBACK WndProc(HWND window,
                                  UINT message_id,
                                  WPARAM wparam,
                                  LPARAM lparam);

  // Handles the messages sent to the SpawnTarget dialog. The return value is
  // the result of the message processing and depends on the message.
  static INT_PTR CALLBACK SpawnTargetWndProc(HWND dialog,
                                             UINT message_id,
                                             WPARAM wparam,
                                             LPARAM lparam);

  // Retrieves a pointer to the MainWindow from a value stored along with the
  // window handle (passed in as hwnd). Return value is a pointer to the
  // MainUIWindow previously stored with SetWindowLong() during WM_CREATE.
  static MainUIWindow* FromWindow(HWND main_window);

  // Handles the WM_CREATE message for the main UI window. Returns TRUE on
  // success.
  static BOOL OnCreate(HWND parent_window, LPCREATESTRUCT);

  // Handles the WM_DESTROY message for the main UI window.
  void OnDestroy(HWND window);

  // Handles the WM_SIZE message for the main UI window.
  void OnSize(HWND window, UINT state, int cx, int cy);

  // Handles the WM_PAINT message for the main UI window.
  void OnPaint(HWND window);

  // Handles the menu command File \ Exit for the main UI window.
  void OnFileExit();

  // Handles the menu command Commands \ Launch for the main UI window.
  void OnCommandsLaunch(HWND window);

  // Handles the Launch button in the SpawnTarget dialog (normally clicked
  // after selecting DLL and entry point). OnLaunchDll will retrieve the
  // values entered by the user and store it in the members of the class.
  // Returns true if user selected a non-zero values for DLL filename
  // (possibly including path also) and entry point.
  bool OnLaunchDll(HWND dialog);

  // Spawns a target EXE inside the sandbox (with the help of the
  // BrokerServices passed in to CreateMainWindowAndLoop), and passes to it
  // (as command line arguments) the DLL path and the entry point function
  // name. The EXE is expected to parse the command line and load the DLL.
  // NOTE: The broker does not know if the target EXE successfully loaded the
  // DLL, for that you have to rely on the EXE providing a log.
  // Returns true if the broker reports that it was successful in creating
  // the target and false if not.
  bool SpawnTarget();

  // Shows a standard File Open dialog and returns the DLL filename selected or
  // blank string if the user cancelled (or an error occurred).
  std::wstring OnShowBrowseForDllDlg(HWND owner);

  // Shows a standard Save As dialog and returns the log filename selected or
  // blank string if the user cancelled (or an error occurred).
  std::wstring OnShowBrowseForLogFileDlg(HWND owner);

  // Formats a message using the supplied format string and prints it in the
  // listview in the main UI window. Passing a NULL param in 'fmt' results in
  // no action being performed. Maximum message length is 1K.
  void AddDebugMessage(const wchar_t* format, ...);

  // Assists AddDebugMessage in displaying a message in the ListView. It
  // simply wraps ListView_InsertItem to insert a debugging message to the
  // top of the list view. Passing a NULL param in 'fmt' results in no action
  // being performed.
  void InsertLineInListView(wchar_t* debug_message);

  // Calls ListenPipe using the class instance received in parameter. This is
  // used to create new threads executing ListenPipe
  static DWORD WINAPI ListenPipeThunk(void *param);

  // Calls WaitForTargetThunk using the class instance received in parameter
  // This is used to create new threads executing WaitForTarget.
  static DWORD WINAPI WaitForTargetThunk(void *param);

  // Listens on a pipe and output the data received to a file and to the UI.
  DWORD ListenPipe();

  // Waits for the target to dies and display a message in the UI.
  DWORD WaitForTarget();

  // The BrokerServices will be used to spawn an EXE in a sandbox and ask
  // it to load a DLL.
  sandbox::BrokerServices* broker_;

  // Contains the information about the running target.
  PROCESS_INFORMATION target_;

  // This is essentially a command line to a target executable that the
  // broker will spawn and ask to load the DLL.
  std::wstring spawn_target_;

  // A handle to the current instance of the app. Passed in to this class
  // through CreateMainWindowAndLoop.
  HINSTANCE instance_handle_;

  // A path to the DLL that the target should load once it executes.
  std::wstring dll_path_;

  // The name of the entry point the target should call after it loads the DLL.
  std::wstring entry_point_;

  // The name of the log file to use.
  std::wstring log_file_;

  // This is a static handle to the list view that fills up the entire main
  // UI window. The list view is used to display debugging information to the
  // user.
  static HWND list_view_;

  // Pipe used to communicate the logs between the target and the broker.
  HANDLE pipe_handle_;

  DISALLOW_COPY_AND_ASSIGN(MainUIWindow);
};

#endif  // SANDBOX_SANDBOX_POC_MAIN_UI_WINDOW_H__
