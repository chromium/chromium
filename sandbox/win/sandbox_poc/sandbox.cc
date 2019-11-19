// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/sandbox_poc/sandbox.h"

#include <windows.h>
#include <tchar.h>
#include <shellapi.h>

#include <string>

#include "base/logging.h"
#include "sandbox/win/sandbox_poc/main_ui_window.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"

// Prototype allowed for functions to be called in the POC
typedef void(__cdecl *lpfnInit)(HANDLE);

bool ParseCommandLine(wchar_t* command_line,
                      std::string* dll_name,
                      std::string* entry_point,
                      std::wstring* log_file) {
  DCHECK(dll_name);
  DCHECK(entry_point);
  DCHECK(log_file);
  if (!dll_name || !entry_point || !log_file)
    return false;

  LPWSTR* arg_list;
  int arg_count;

  // We expect the command line to contain: EntryPointName "DLLPath" "LogPath"
  // NOTE: Double quotes are required, even if long path name not used
  // NOTE: LogPath can be blank, but still requires the double quotes
  arg_list = CommandLineToArgvW(command_line, &arg_count);
  if (NULL == arg_list || arg_count < 4) {
     return false;
  }

  std::wstring entry_point_wide = arg_list[1];
  std::wstring dll_name_wide = arg_list[2];
  *entry_point = std::string(entry_point_wide.begin(), entry_point_wide.end());
  *dll_name    = std::string(dll_name_wide.begin(), dll_name_wide.end());
  *log_file    = arg_list[3];

  // Free memory allocated for CommandLineToArgvW arguments.
  LocalFree(arg_list);

  return true;
}

int APIENTRY _tWinMain(HINSTANCE instance, HINSTANCE, wchar_t* command_line,
                       int show_command) {
  sandbox::BrokerServices* broker_service =
      sandbox::SandboxFactory::GetBrokerServices();
  sandbox::ResultCode result;

  // This application starts as the broker; an application with a UI that
  // spawns an instance of itself (called a 'target') inside the sandbox.
  // Before spawning a hidden instance of itself, the application will have
  // asked the user which DLL the spawned instance should load and passes
  // that as command line argument to the spawned instance.
  //
  // We check here to see if we can retrieve a pointer to the BrokerServices,
  // which is not possible if we are running inside the sandbox under a
  // restricted token so it also tells us which mode we are in. If we can
  // retrieve the pointer, then we are the broker, otherwise we are the target
  // that the broker launched.
  if (NULL != broker_service) {
    // Yes, we are the broker so we need to initialize and show the UI
    if (0 != (result = broker_service->Init())) {
      ::MessageBox(NULL, L"Failed to initialize the BrokerServices object",
                   L"Error during initialization", MB_ICONERROR);
      return 1;
    }

    wchar_t exe_name[MAX_PATH];
    if (0 == GetModuleFileName(NULL, exe_name, MAX_PATH - 1)) {
      ::MessageBox(NULL, L"Failed to get name of current EXE",
                   L"Error during initialization", MB_ICONERROR);
      return 1;
    }

    // The CreateMainWindowAndLoop() call will not return until the user closes
    // the application window (or selects File\Exit).
    MainUIWindow window;
    window.CreateMainWindowAndLoop(instance,
                                   exe_name,
                                   show_command,
                                   broker_service);


    // Cannot exit until we have cleaned up after all the targets we have
    // created
    broker_service->WaitForAllTargets();
  } else {
    // This is an instance that has been spawned inside the sandbox by the
    // broker, so we need to parse the command line to figure out which DLL to
    // load and what entry point to call
    sandbox::TargetServices* target_service
        = sandbox::SandboxFactory::GetTargetServices();

    if (NULL == target_service) {
      // TODO(finnur): write the failure to the log file
      // We cannot display messageboxes inside the sandbox unless access to
      // the desktop handle has been granted to us, and we don't have a
      // console window to write to. Therefore we need to have the broker
      // grant us access to a handle to a logfile and write the error that
      // occurred into the log before continuing
      return -1;
    }

    // Debugging the spawned application can be tricky, because DebugBreak()
    // and _asm int 3 cause the app to terminate (due to a flag in the job
    // object), MessageBoxes() will not be displayed unless we have been granted
    // that privilege and the target finishes its business so quickly we cannot
    // attach to it quickly enough. Therefore, you can uncomment the
    // following line and attach (w. msdev or windbg) as the target is sleeping

    // Sleep(10000);

    if (sandbox::SBOX_ALL_OK != (result = target_service->Init())) {
      // TODO(finnur): write the initialization error to the log file
      return -2;
    }

    // Parse the command line to find out what we need to call
    std::string dll_name, entry_point;
    std::wstring log_file;
    if (!ParseCommandLine(GetCommandLineW(),
                          &dll_name,
                          &entry_point,
                          &log_file)) {
      // TODO(finnur): write the failure to the log file
      return -3;
    }

    // Open the pipe to transfert the log output
    HANDLE pipe = ::CreateFile(log_file.c_str(),
                               GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL,  // Default security attributes.
                               CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL,
                               NULL);  // No template

    if (INVALID_HANDLE_VALUE == pipe) {
      return -4;
    }

    // We now know what we should load, so load it
    HMODULE dll_module = ::LoadLibraryA(dll_name.c_str());
    if (dll_module == NULL) {
      // TODO(finnur): write the failure to the log file
      CloseHandle(pipe);
      return -5;
    }

    // Initialization is finished, so we can enter lock-down mode
    target_service->LowerToken();

    lpfnInit init_function =
        (lpfnInit) ::GetProcAddress(dll_module, entry_point.c_str());

    if (!init_function) {
      // TODO(finnur): write the failure to the log file
      ::FreeLibrary(dll_module);
      CloseHandle(pipe);
      return -6;
    }

    // Transfer control to the entry point in the DLL requested
    init_function(pipe);

    CloseHandle(pipe);
    Sleep(1000);  // Give a change to the debug output to arrive before the
                  // end of the process

    ::FreeLibrary(dll_module);
  }

  return 0;
}
