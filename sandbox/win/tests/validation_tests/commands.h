// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_TESTS_VALIDATION_TESTS_COMMANDS_H_
#define SANDBOX_WIN_TESTS_VALIDATION_TESTS_COMMANDS_H_

#include <windows.h>

#include <string>

#include "sandbox/win/tests/common/controller.h"

namespace sandbox {

// Checks if window is a real window. Returns a SboxTestResult.
int TestValidWindow(HWND window);

// Tries to open the process_id. Returns a SboxTestResult.
int TestOpenProcess(DWORD process_id, DWORD access_mask);

// Tries to open thread_id. Returns a SboxTestResult.
int TestOpenThread(DWORD thread_id);

// Tries to open path for read access. Returns a SboxTestResult.
int TestOpenReadFile(const std::wstring& path);

// Tries to open path for write access. Returns a SboxTestResult.
int TestOpenWriteFile(const std::wstring& path);

// Tries to open a registry key.
int TestOpenKey(HKEY base_key, std::wstring subkey);

// Tries to open the workstation's input desktop as long as the
// current desktop is not the interactive one. Returns a SboxTestResult.
int TestOpenInputDesktop();

// Tries to switch the interactive desktop. Returns a SboxTestResult.
int TestSwitchDesktop();

// Tries to open the alternate desktop. Returns a SboxTestResult.
int TestOpenAlternateDesktop(wchar_t *desktop_name);

// Tries to enumerate desktops on the alternate windowstation.
// Returns a SboxTestResult.
int TestEnumAlternateWinsta();

// Declare validation test commands.
SBOX_TEST_DECLARE_COMMAND(ValidWindow);
SBOX_TEST_DECLARE_COMMAND(OpenProcessCmd);
SBOX_TEST_DECLARE_COMMAND(OpenThreadCmd);
SBOX_TEST_DECLARE_COMMAND(OpenFileCmd);
SBOX_TEST_DECLARE_COMMAND(OpenKey);
SBOX_TEST_DECLARE_COMMAND(OpenInteractiveDesktop);
SBOX_TEST_DECLARE_COMMAND(SwitchToSboxDesktop);
SBOX_TEST_DECLARE_COMMAND(OpenAlternateDesktop);
SBOX_TEST_DECLARE_COMMAND(EnumAlternateWinsta);
SBOX_TEST_DECLARE_COMMAND(SleepCmd);
SBOX_TEST_DECLARE_COMMAND(AllocateCmd);
SBOX_TEST_DECLARE_COMMAND(InitCompleted);

}  // namespace sandbox

#endif  // SANDBOX_WIN_TESTS_VALIDATION_TESTS_COMMANDS_H_
