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
  auto path = GetPathFromHandle(handle.Get());
  ASSERT_TRUE(path.has_value());
  EXPECT_TRUE(base::EqualsCaseInsensitiveASCII(path.value(), expected_path));
}

void CompareHandleType(const base::win::ScopedHandle& handle,
                       const std::wstring& expected_type) {
  auto type_name = GetTypeNameFromHandle(handle.Get());
  ASSERT_TRUE(type_name);
  EXPECT_TRUE(
      base::EqualsCaseInsensitiveASCII(type_name.value(), expected_type));
}

void FindHandle(const ProcessHandleMap& handle_map,
                const wchar_t* type_name,
                const base::win::ScopedHandle& handle) {
  ProcessHandleMap::const_iterator entry = handle_map.find(type_name);
  ASSERT_NE(handle_map.end(), entry);
  EXPECT_TRUE(base::Contains(entry->second, handle.Get()));
}

void TestCurrentProcessHandles(absl::optional<ProcessHandleMap> (*func)()) {
  std::wstring random_name = GetRandomName();
  ASSERT_FALSE(random_name.empty());
  base::win::ScopedHandle event_handle(
      ::CreateEvent(nullptr, FALSE, FALSE, random_name.c_str()));
  ASSERT_TRUE(event_handle.IsValid());
  std::wstring pipe_name = L"\\\\.\\pipe\\" + random_name;
  base::win::ScopedHandle pipe_handle(::CreateNamedPipe(
      pipe_name.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE,
      PIPE_UNLIMITED_INSTANCES, 0, 0, NMPWAIT_USE_DEFAULT_WAIT, nullptr));
  ASSERT_TRUE(pipe_handle.IsValid());

  absl::optional<ProcessHandleMap> handle_map = func();
  ASSERT_TRUE(handle_map);
  EXPECT_LE(2U, handle_map->size());
  FindHandle(*handle_map, L"Event", event_handle);
  FindHandle(*handle_map, L"File", pipe_handle);
}

}  // namespace

TEST(WinUtils, IsReparsePoint) {
  using sandbox::IsReparsePoint;
  // Create a temp file because we need write access to it.
  wchar_t temp_directory[MAX_PATH];
  wchar_t my_folder[MAX_PATH];
  ASSERT_NE(::GetTempPath(MAX_PATH, temp_directory), 0u);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, my_folder), 0u);

  // Delete the file and create a directory instead.
  ASSERT_TRUE(::DeleteFile(my_folder));
  ASSERT_TRUE(::CreateDirectory(my_folder, nullptr));

  EXPECT_EQ(static_cast<DWORD>(ERROR_NOT_A_REPARSE_POINT),
            IsReparsePoint(my_folder));

  std::wstring not_found = std::wstring(my_folder) + L"\\foo\\bar";
  EXPECT_EQ(static_cast<DWORD>(ERROR_NOT_A_REPARSE_POINT),
            IsReparsePoint(not_found));

  std::wstring new_file = std::wstring(my_folder) + L"\\foo";
  EXPECT_EQ(static_cast<DWORD>(ERROR_NOT_A_REPARSE_POINT),
            IsReparsePoint(new_file));

  // Replace the directory with a reparse point to %temp%.
  HANDLE dir = ::CreateFile(my_folder, FILE_WRITE_DATA,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  EXPECT_NE(INVALID_HANDLE_VALUE, dir);

  std::wstring temp_dir_nt = std::wstring(L"\\??\\") + temp_directory;
  EXPECT_TRUE(SetReparsePoint(dir, temp_dir_nt.c_str()));

  EXPECT_EQ(static_cast<DWORD>(ERROR_SUCCESS), IsReparsePoint(new_file));

  EXPECT_TRUE(DeleteReparsePoint(dir));
  EXPECT_TRUE(::CloseHandle(dir));
  EXPECT_TRUE(::RemoveDirectory(my_folder));
}

TEST(WinUtils, SameObject) {
  using sandbox::SameObject;

  // Create a temp file because we need write access to it.
  wchar_t temp_directory[MAX_PATH];
  wchar_t my_folder[MAX_PATH];
  ASSERT_NE(::GetTempPath(MAX_PATH, temp_directory), 0u);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, my_folder), 0u);

  // Delete the file and create a directory instead.
  ASSERT_TRUE(::DeleteFile(my_folder));
  ASSERT_TRUE(::CreateDirectory(my_folder, nullptr));

  std::wstring folder(my_folder);
  std::wstring file_name = folder + L"\\foo.txt";
  const ULONG kSharing = FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE;
  base::win::ScopedHandle file(CreateFile(file_name.c_str(), GENERIC_WRITE,
                                          kSharing, nullptr, CREATE_ALWAYS,
                                          FILE_FLAG_DELETE_ON_CLOSE, nullptr));

  EXPECT_TRUE(file.IsValid());
  std::wstring file_name_nt1 = std::wstring(L"\\??\\") + file_name;
  std::wstring file_name_nt2 = std::wstring(L"\\??\\") + folder + L"\\FOO.txT";
  EXPECT_TRUE(SameObject(file.Get(), file_name_nt1.c_str()));
  EXPECT_TRUE(SameObject(file.Get(), file_name_nt2.c_str()));

  file.Close();
  EXPECT_TRUE(::RemoveDirectory(my_folder));
}

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

// This test requires an elevated prompt to setup.
TEST(WinUtils, ConvertToLongPath) {
  // Test setup.
  base::FilePath orig_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &orig_path));
  orig_path = orig_path.Append(L"calc.exe");

  base::ScopedTempDir temp_dir;
  base::FilePath base_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_COMMON_APP_DATA, &base_path));
  ASSERT_TRUE(temp_dir.CreateUniqueTempDirUnderPath(base_path));

  base::FilePath temp_path = temp_dir.GetPath().Append(L"test_calc.exe");
  ASSERT_TRUE(base::CopyFile(orig_path, temp_path));

  // WIN32 long path: "C:\ProgramData\%TEMP%\test_calc.exe"
  wchar_t short_path[MAX_PATH] = {};
  DWORD size =
      ::GetShortPathNameW(temp_path.value().c_str(), short_path, MAX_PATH);
  EXPECT_TRUE(size > 0 && size < MAX_PATH);
  // WIN32 short path: "C:\PROGRA~3\%TEMP%\TEST_C~1.exe"

  // Sanity check that we actually got a short path above!  Small chance
  // it was disabled in the filesystem setup.
  EXPECT_NE(temp_path.value().length(), ::wcslen(short_path));

  auto short_form_native_path =
      sandbox::GetNtPathFromWin32Path(std::wstring(short_path));
  EXPECT_TRUE(short_form_native_path);
  // NT short path: "\Device\HarddiskVolume4\PROGRA~3\%TEMP%\TEST_C~1.EXE"

  // Test 1: convert win32 short path to long:
  std::wstring test1(short_path);
  EXPECT_TRUE(sandbox::ConvertToLongPath(&test1));
  EXPECT_TRUE(::wcsicmp(temp_path.value().c_str(), test1.c_str()) == 0);
  // Expected result: "C:\ProgramData\%TEMP%\test_calc.exe"

  // Test 2: convert native short path to long:
  std::wstring drive_letter = temp_path.value().substr(0, 3);
  std::wstring test2(short_form_native_path.value());
  EXPECT_TRUE(sandbox::ConvertToLongPath(&test2, &drive_letter));

  size_t index = short_form_native_path->find_first_of(
      L'\\', ::wcslen(L"\\Device\\HarddiskVolume"));
  EXPECT_TRUE(index != std::wstring::npos);
  std::wstring expected_result = short_form_native_path->substr(0, index + 1);
  expected_result.append(temp_path.value().substr(3));
  EXPECT_TRUE(::wcsicmp(expected_result.c_str(), test2.c_str()) == 0);
  // Expected result: "\Device\HarddiskVolumeX\ProgramData\%TEMP%\test_calc.exe"
}

TEST(WinUtils, GetPathAndTypeFromHandle) {
  EXPECT_FALSE(GetPathFromHandle(nullptr));
  EXPECT_FALSE(GetTypeNameFromHandle(nullptr));
  std::wstring random_name = GetRandomName();
  ASSERT_FALSE(random_name.empty());
  std::wstring event_name = L"Global\\" + random_name;
  base::win::ScopedHandle event_handle(
      ::CreateEvent(nullptr, FALSE, FALSE, event_name.c_str()));
  ASSERT_TRUE(event_handle.IsValid());
  CompareHandlePath(event_handle, L"\\BaseNamedObjects\\" + random_name);
  CompareHandleType(event_handle, L"Event");
  std::wstring pipe_name = L"\\\\.\\pipe\\" + random_name;
  base::win::ScopedHandle pipe_handle(::CreateNamedPipe(
      pipe_name.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE,
      PIPE_UNLIMITED_INSTANCES, 0, 0, NMPWAIT_USE_DEFAULT_WAIT, nullptr));
  ASSERT_TRUE(pipe_handle.IsValid());
  CompareHandlePath(pipe_handle, L"\\Device\\NamedPipe\\" + random_name);
  CompareHandleType(pipe_handle, L"File");
}

TEST(WinUtils, GetCurrentProcessHandles) {
  TestCurrentProcessHandles(GetCurrentProcessHandles);
}

}  // namespace sandbox
