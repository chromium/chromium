// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/win_utils.h"

#include <windows.h>

#include <ntstatus.h>
#include <psapi.h>

#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/string_util_win.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/tests/common/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

class ScopedTerminateProcess {
 public:
  explicit ScopedTerminateProcess(HANDLE process) : process_(process) {}

  ~ScopedTerminateProcess() { ::TerminateProcess(process_, 0); }

 private:
  HANDLE process_;
};

bool GetModuleList(HANDLE process, std::vector<HMODULE>* result) {
  std::vector<HMODULE> modules(256);
  DWORD size_needed = 0;
  if (EnumProcessModules(
          process, &modules[0],
          base::checked_cast<DWORD>(modules.size() * sizeof(HMODULE)),
          &size_needed)) {
    result->assign(modules.begin(),
                   modules.begin() + (size_needed / sizeof(HMODULE)));
    return true;
  }
  modules.resize(size_needed / sizeof(HMODULE));
  // Avoid the undefined-behavior of calling modules[0] on an empty list. This
  // can happen if the process has not yet started or has already exited.
  if (modules.size() == 0) {
    return false;
  }
  if (EnumProcessModules(
          process, &modules[0],
          base::checked_cast<DWORD>(modules.size() * sizeof(HMODULE)),
          &size_needed)) {
    result->assign(modules.begin(),
                   modules.begin() + (size_needed / sizeof(HMODULE)));
    return true;
  }
  return false;
}

std::wstring GetRandomName() {
  return base::ASCIIToWide(
      base::StringPrintf("chrome_%016" PRIX64 "%016" PRIX64, base::RandUint64(),
                         base::RandUint64()));
}

void CompareHandlePath(const base::win::ScopedHandle& handle,
                       const std::wstring& expected_path) {
  auto path = GetPathFromHandle(handle.get());
  ASSERT_TRUE(path.has_value());
  EXPECT_TRUE(base::EqualsCaseInsensitiveASCII(path.value(), expected_path));
}

void CompareHandleType(const base::win::ScopedHandle& handle,
                       const std::wstring& expected_type) {
  auto type_name = GetTypeNameFromHandle(handle.get());
  ASSERT_TRUE(type_name);
  EXPECT_TRUE(
      base::EqualsCaseInsensitiveASCII(type_name.value(), expected_type));
}

}  // namespace

TEST(WinUtils, IsPipe) {
  using sandbox::IsPipe;

  std::wstring pipe_name = L"\\??\\pipe\\mypipe";
  EXPECT_TRUE(IsPipe(pipe_name));

  pipe_name = L"\\??\\PiPe\\mypipe";
  EXPECT_TRUE(IsPipe(pipe_name));

  pipe_name = L"\\??\\pipe";
  EXPECT_FALSE(IsPipe(pipe_name));

  pipe_name = L"\\??\\_pipe_\\mypipe";
  EXPECT_FALSE(IsPipe(pipe_name));

  pipe_name = L"\\??\\ABCD\\mypipe";
  EXPECT_FALSE(IsPipe(pipe_name));

  // Written as two strings to prevent trigraph '?' '?' '/'.
  pipe_name =
      L"/?"
      L"?/pipe/mypipe";
  EXPECT_FALSE(IsPipe(pipe_name));

  pipe_name = L"\\XX\\pipe\\mypipe";
  EXPECT_FALSE(IsPipe(pipe_name));

  pipe_name = L"\\Device\\NamedPipe\\mypipe";
  EXPECT_FALSE(IsPipe(pipe_name));
}

TEST(WinUtils, NtStatusToWin32Error) {
  using sandbox::GetLastErrorFromNtStatus;
  EXPECT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            GetLastErrorFromNtStatus(STATUS_SUCCESS));
  EXPECT_EQ(static_cast<DWORD>(ERROR_NOT_SUPPORTED),
            GetLastErrorFromNtStatus(STATUS_NOT_SUPPORTED));
  EXPECT_EQ(static_cast<DWORD>(ERROR_ALREADY_EXISTS),
            GetLastErrorFromNtStatus(STATUS_OBJECT_NAME_COLLISION));
  EXPECT_EQ(static_cast<DWORD>(ERROR_ACCESS_DENIED),
            GetLastErrorFromNtStatus(STATUS_ACCESS_DENIED));
}

TEST(WinUtils, GetProcessBaseAddress) {
  using sandbox::GetProcessBaseAddress;
  STARTUPINFO start_info = {};
  PROCESS_INFORMATION proc_info = {};
  // The child process for this test must be a GUI app so that WaitForInputIdle
  // can be used to guarantee that the child process has started but has not
  // exited. notepad was used but will fail on Windows 11 if the store version
  // of notepad is not installed.
  WCHAR command_line[] = L"calc";
  start_info.cb = sizeof(start_info);
  start_info.dwFlags = STARTF_USESHOWWINDOW;
  start_info.wShowWindow = SW_HIDE;
  ASSERT_TRUE(::CreateProcessW(nullptr, command_line, nullptr, nullptr, false,
                               CREATE_SUSPENDED, nullptr, nullptr, &start_info,
                               &proc_info));
  base::win::ScopedProcessInformation scoped_proc_info(proc_info);
  ScopedTerminateProcess process_terminate(scoped_proc_info.process_handle());
  void* base_address = GetProcessBaseAddress(scoped_proc_info.process_handle());
  ASSERT_NE(nullptr, base_address);
  ASSERT_NE(static_cast<DWORD>(-1),
            ::ResumeThread(scoped_proc_info.thread_handle()));
  ::WaitForInputIdle(scoped_proc_info.process_handle(), 1000);
  ASSERT_NE(static_cast<DWORD>(-1),
            ::SuspendThread(scoped_proc_info.thread_handle()));

  std::vector<HMODULE> modules;
  // Compare against the loader's module list (which should now be initialized).
  ASSERT_TRUE(GetModuleList(scoped_proc_info.process_handle(), &modules));
  ASSERT_GT(modules.size(), 0U);
  EXPECT_EQ(base_address, modules[0]);
}

TEST(WinUtils, GetPathAndTypeFromHandle) {
  EXPECT_FALSE(GetPathFromHandle(nullptr));
  EXPECT_FALSE(GetTypeNameFromHandle(nullptr));
  std::wstring random_name = GetRandomName();
  ASSERT_FALSE(random_name.empty());
  std::wstring event_name = L"Global\\" + random_name;
  base::win::ScopedHandle event_handle(
      ::CreateEvent(nullptr, FALSE, FALSE, event_name.c_str()));
  ASSERT_TRUE(event_handle.is_valid());
  CompareHandlePath(event_handle, L"\\BaseNamedObjects\\" + random_name);
  CompareHandleType(event_handle, L"Event");
  std::wstring pipe_name = L"\\\\.\\pipe\\" + random_name;
  base::win::ScopedHandle pipe_handle(::CreateNamedPipe(
      pipe_name.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE,
      PIPE_UNLIMITED_INSTANCES, 0, 0, NMPWAIT_USE_DEFAULT_WAIT, nullptr));
  ASSERT_TRUE(pipe_handle.is_valid());
  CompareHandlePath(pipe_handle, L"\\Device\\NamedPipe\\" + random_name);
  CompareHandleType(pipe_handle, L"File");
}

TEST(WinUtils, ContainsNulCharacter) {
  std::wstring str = L"ABC";
  EXPECT_FALSE(ContainsNulCharacter(str));
  str.push_back('\0');
  EXPECT_TRUE(ContainsNulCharacter(str));
  str += L"XYZ";
  EXPECT_TRUE(ContainsNulCharacter(str));
}

}  // namespace sandbox
