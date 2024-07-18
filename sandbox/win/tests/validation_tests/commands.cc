// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/win/tests/validation_tests/commands.h"

#include <windows.h>

#include <Aclapi.h>
#include <stddef.h>

#include <string>

#include "sandbox/win/tests/common/controller.h"

namespace {

// Returns the HKEY corresponding to name. If there is no HKEY corresponding
// to the name it returns NULL.
HKEY GetHKEYFromString(const std::wstring& name) {
  if (name == L"HKLM")
    return HKEY_LOCAL_MACHINE;
  if (name == L"HKCR")
    return HKEY_CLASSES_ROOT;
  if (name == L"HKCC")
    return HKEY_CURRENT_CONFIG;
  if (name == L"HKCU")
    return HKEY_CURRENT_USER;
  if (name == L"HKU")
    return HKEY_USERS;

  return NULL;
}

// Modifies string to remove the leading and trailing quotes.
void trim_quote(std::wstring* string) {
  std::wstring::size_type pos1 = string->find_first_not_of(L'"');
  std::wstring::size_type pos2 = string->find_last_not_of(L'"');

  if (pos1 == std::wstring::npos || pos2 == std::wstring::npos)
    string->clear();
  else
    (*string) = string->substr(pos1, pos2 + 1);
}

int TestOpenFile(std::wstring path, bool for_write) {
  wchar_t path_expanded[MAX_PATH + 1] = {0};
  DWORD size = ::ExpandEnvironmentStrings(path.c_str(), path_expanded,
                                          MAX_PATH);
  if (!size)
    return sandbox::SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  HANDLE file;
  file = ::CreateFile(path_expanded,
                      for_write ? GENERIC_READ | GENERIC_WRITE : GENERIC_READ,
                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                      NULL,  // No security attributes.
                      OPEN_EXISTING,
                      FILE_FLAG_BACKUP_SEMANTICS,
                      NULL);  // No template.

  if (file != INVALID_HANDLE_VALUE) {
    ::CloseHandle(file);
    return sandbox::SBOX_TEST_SUCCEEDED;
  }
  return (::GetLastError() == ERROR_ACCESS_DENIED) ?
      sandbox::SBOX_TEST_DENIED : sandbox::SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
}

}  // namespace

namespace sandbox {

SBOX_TESTS_COMMAND int ValidWindow(int argc, wchar_t **argv) {
  return (argc == 1) ?
      TestValidWindow(
          reinterpret_cast<HWND>(static_cast<ULONG_PTR>(_wtoi(argv[0])))) :
      SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
}

int TestValidWindow(HWND window) {
  return ::IsWindow(window) ? SBOX_TEST_SUCCEEDED : SBOX_TEST_DENIED;
}

SBOX_TESTS_COMMAND int OpenProcessCmd(int argc, wchar_t **argv) {
  return (argc == 2) ?
      TestOpenProcess(_wtol(argv[0]), _wtol(argv[1])) :
      SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
}

int TestOpenProcess(DWORD process_id, DWORD access_mask) {
  HANDLE process = ::OpenProcess(access_mask,
                                 FALSE,  // Do not inherit handle.
                                 process_id);
  if (process != NULL) {
    ::CloseHandle(process);
    return SBOX_TEST_SUCCEEDED;
  }
  return (::GetLastError() == ERROR_ACCESS_DENIED) ?
      sandbox::SBOX_TEST_DENIED : sandbox::SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
}

SBOX_TESTS_COMMAND int OpenThreadCmd(int argc, wchar_t **argv) {
  return (argc == 1) ?
      TestOpenThread(_wtoi(argv[0])) : SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
}

int TestOpenThread(DWORD thread_id) {
  HANDLE thread = ::OpenThread(THREAD_QUERY_INFORMATION,
                               FALSE,  // Do not inherit handles.
                               thread_id);
  if (thread != NULL) {
    ::CloseHandle(thread);
    return SBOX_TEST_SUCCEEDED;
  }
  return (::GetLastError() == ERROR_ACCESS_DENIED) ?
      sandbox::SBOX_TEST_DENIED : sandbox::SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
}

SBOX_TESTS_COMMAND int OpenFileCmd(int argc, wchar_t **argv) {
  if (1 != argc)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  std::wstring path = argv[0];
  trim_quote(&path);

  return TestOpenReadFile(path);
}

int TestOpenReadFile(const std::wstring& path) {
  return TestOpenFile(path, false);
}

int TestOpenWriteFile(int argc, wchar_t **argv) {
  if (argc != 1)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  std::wstring path = argv[0];
  trim_quote(&path);
  return TestOpenWriteFile(path);
}

int TestOpenWriteFile(const std::wstring& path) {
  return TestOpenFile(path, true);
}

SBOX_TESTS_COMMAND int OpenKey(int argc, wchar_t **argv) {
  if (argc != 1 && argc != 2)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  // Get the hive.
  HKEY base_key = GetHKEYFromString(argv[0]);

  // Get the subkey.
  std::wstring subkey;
  if (argc == 2) {
    subkey = argv[1];
    trim_quote(&subkey);
  }

  return TestOpenKey(base_key, subkey);
}

int TestOpenKey(HKEY base_key, std::wstring subkey) {
  HKEY key;
  LONG err_code = ::RegOpenKeyEx(base_key,
                                 subkey.c_str(),
                                 0,  // Reserved, must be 0.
                                 MAXIMUM_ALLOWED,
                                 &key);
  if (err_code == ERROR_SUCCESS) {
    ::RegCloseKey(key);
    return SBOX_TEST_SUCCEEDED;
  }
  return (err_code == ERROR_INVALID_HANDLE || err_code == ERROR_ACCESS_DENIED) ?
      SBOX_TEST_DENIED : SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
}

// Returns true if the current's thread desktop is the interactive desktop.
// In Vista there is a more direct test but for XP and w2k we need to check
// the object name.
bool IsInteractiveDesktop(bool* is_interactive) {
  HDESK current_desk = ::GetThreadDesktop(::GetCurrentThreadId());
  if (current_desk == NULL)
    return false;
  wchar_t current_desk_name[256] = {0};
  if (!::GetUserObjectInformationW(current_desk, UOI_NAME, current_desk_name,
                                   sizeof(current_desk_name), NULL))
    return false;
  *is_interactive = (0 == _wcsicmp(L"default", current_desk_name));
  return true;
}

SBOX_TESTS_COMMAND int OpenInteractiveDesktop(int, wchar_t **) {
  return TestOpenInputDesktop();
}

int TestOpenInputDesktop() {
  bool is_interactive = false;
  if (IsInteractiveDesktop(&is_interactive) && is_interactive)
    return SBOX_TEST_SUCCEEDED;
  HDESK desk = ::OpenInputDesktop(0, FALSE, DESKTOP_CREATEWINDOW);
  if (desk) {
    ::CloseDesktop(desk);
    return SBOX_TEST_SUCCEEDED;
  }
  return SBOX_TEST_DENIED;
}

SBOX_TESTS_COMMAND int SwitchToSboxDesktop(int, wchar_t **) {
  return TestSwitchDesktop();
}

int TestSwitchDesktop() {
  HDESK desktop = ::GetThreadDesktop(::GetCurrentThreadId());
  if (desktop == NULL)
    return SBOX_TEST_FAILED;
  return ::SwitchDesktop(desktop) ? SBOX_TEST_SUCCEEDED : SBOX_TEST_DENIED;
}

SBOX_TESTS_COMMAND int OpenAlternateDesktop(int, wchar_t **argv) {
  return TestOpenAlternateDesktop(argv[0]);
}

int TestOpenAlternateDesktop(wchar_t *desktop_name) {
  // Test for WRITE_DAC permission on the handle.
  HDESK desktop = ::GetThreadDesktop(::GetCurrentThreadId());
  if (desktop) {
    HANDLE test_handle;
    if (::DuplicateHandle(::GetCurrentProcess(), desktop,
                          ::GetCurrentProcess(), &test_handle,
                          WRITE_DAC, FALSE, 0)) {
      DWORD result = ::SetSecurityInfo(test_handle, SE_WINDOW_OBJECT,
                                       DACL_SECURITY_INFORMATION, NULL, NULL,
                                       NULL, NULL);
      ::CloseHandle(test_handle);
      if (result == ERROR_SUCCESS)
        return SBOX_TEST_SUCCEEDED;
    } else if (::GetLastError() != ERROR_ACCESS_DENIED) {
      return SBOX_TEST_FAILED;
    }
  }

  // Open by name with WRITE_DAC.
  desktop = ::OpenDesktop(desktop_name, 0, FALSE, WRITE_DAC);
  if (!desktop && ::GetLastError() == ERROR_ACCESS_DENIED)
    return SBOX_TEST_DENIED;
  ::CloseDesktop(desktop);
  return SBOX_TEST_SUCCEEDED;
}

BOOL CALLBACK DesktopTestEnumProc(LPTSTR desktop_name, LPARAM result) {
  return TRUE;
}

SBOX_TESTS_COMMAND int EnumAlternateWinsta(int, wchar_t **) {
  return TestEnumAlternateWinsta();
}

int TestEnumAlternateWinsta() {
  // Try to enumerate the destops on the alternate windowstation.
  return ::EnumDesktopsW(NULL, DesktopTestEnumProc, 0) ?
      SBOX_TEST_SUCCEEDED : SBOX_TEST_DENIED;
}

SBOX_TESTS_COMMAND int SleepCmd(int argc, wchar_t **argv) {
  if (argc != 1)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  ::Sleep(_wtoi(argv[0]));
  return SBOX_TEST_SUCCEEDED;
}

SBOX_TESTS_COMMAND int AllocateCmd(int argc, wchar_t **argv) {
  if (argc != 1)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  size_t mem_size = static_cast<size_t>(_wtoll(argv[0]));
  void* memory = ::VirtualAlloc(NULL, mem_size, MEM_COMMIT | MEM_RESERVE,
                                PAGE_READWRITE);
  if (!memory) {
    // We need to give the broker a chance to kill our process on failure.
    ::Sleep(5000);
    return SBOX_TEST_DENIED;
  }

  return ::VirtualFree(memory, 0, MEM_RELEASE) ?
      SBOX_TEST_SUCCEEDED : SBOX_TEST_FAILED;
}


}  // namespace sandbox
