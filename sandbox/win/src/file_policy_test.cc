// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <windows.h>

#include <ntstatus.h>
#include <winioctl.h>
#include <winternl.h>

#include <algorithm>

#include "base/strings/string_number_conversions_win.h"
#include "base/strings/string_util_win.h"
#include "base/win/scoped_handle.h"
#include "sandbox/win/src/filesystem_policy.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/win_utils.h"
#include "sandbox/win/tests/common/controller.h"
#include "sandbox/win/tests/common/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#define BINDNTDLL(name)                                   \
  name##Function name = reinterpret_cast<name##Function>( \
      ::GetProcAddress(::GetModuleHandle(L"ntdll.dll"), #name))

namespace sandbox {

namespace {
constexpr wchar_t kNTDevicePrefix[] = L"\\Device\\";
constexpr size_t kNTDevicePrefixLen = std::size(kNTDevicePrefix) - 1;
}  // namespace

const ULONG kSharing = FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE;

// Creates a file using different desired access. Returns if the call succeeded
// or not.  The first argument in argv is the filename. The second argument
// determines the type of access and the dispositino of the file.
SBOX_TESTS_COMMAND int File_Create(int argc, wchar_t** argv) {
  if (argc != 2)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  std::wstring operation(argv[0]);

  if (operation == L"Read") {
    base::win::ScopedHandle file1(CreateFile(
        argv[1], GENERIC_READ, kSharing, nullptr, OPEN_EXISTING, 0, nullptr));
    base::win::ScopedHandle file2(CreateFile(
        argv[1], FILE_EXECUTE, kSharing, nullptr, OPEN_EXISTING, 0, nullptr));

    if (file1.is_valid() == file2.is_valid()) {
      return file1.is_valid() ? SBOX_TEST_SUCCEEDED : SBOX_TEST_DENIED;
    }
    return file1.is_valid() ? SBOX_TEST_FIRST_ERROR : SBOX_TEST_SECOND_ERROR;

  } else if (operation == L"Write") {
    base::win::ScopedHandle file1(
        CreateFile(argv[1], GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE,
                   kSharing, nullptr, OPEN_EXISTING, 0, nullptr));
    base::win::ScopedHandle file2(
        CreateFile(argv[1], GENERIC_READ | FILE_WRITE_DATA, kSharing, nullptr,
                   OPEN_EXISTING, 0, nullptr));

    if (file1.is_valid() == file2.is_valid()) {
      return file1.is_valid() ? SBOX_TEST_SUCCEEDED : SBOX_TEST_DENIED;
    }
    return file1.is_valid() ? SBOX_TEST_FIRST_ERROR : SBOX_TEST_SECOND_ERROR;

  } else if (operation == L"ReadCreate") {
    base::win::ScopedHandle file2(CreateFile(argv[1], GENERIC_READ, kSharing,
                                             nullptr, CREATE_NEW, 0, nullptr));
    base::win::ScopedHandle file1(CreateFile(
        argv[1], GENERIC_READ, kSharing, nullptr, CREATE_ALWAYS, 0, nullptr));

    if (file1.is_valid() == file2.is_valid()) {
      return file1.is_valid() ? SBOX_TEST_SUCCEEDED : SBOX_TEST_DENIED;
    }
    return file1.is_valid() ? SBOX_TEST_FIRST_ERROR : SBOX_TEST_SECOND_ERROR;
  }

  return SBOX_TEST_INVALID_PARAMETER;
}

SBOX_TESTS_COMMAND int File_Win32Create(int argc, wchar_t** argv) {
  if (argc != 1) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  std::wstring full_path = MakePathToSys(argv[0], false);
  if (full_path.empty()) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  HANDLE file =
      ::CreateFileW(full_path.c_str(), GENERIC_READ, kSharing, nullptr,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

  if (INVALID_HANDLE_VALUE != file) {
    ::CloseHandle(file);
    return SBOX_TEST_SUCCEEDED;
  }
  if (ERROR_ACCESS_DENIED == ::GetLastError()) {
    return SBOX_TEST_DENIED;
  }
  return SBOX_TEST_FAILED;
}

// Creates the file in parameter using the NtCreateFile api and returns if the
// call succeeded or not.
SBOX_TESTS_COMMAND int File_CreateSys32(int argc, wchar_t** argv) {
  BINDNTDLL(NtCreateFile);
  if (!NtCreateFile) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  if (argc < 1 || argc > 2) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  std::wstring file(argv[0]);
  if (0 != _wcsnicmp(file.c_str(), kNTDevicePrefix, kNTDevicePrefixLen))
    file = MakePathToSys(argv[0], true);

  UNICODE_STRING object_name;
  ::RtlInitUnicodeString(&object_name, file.c_str());

  OBJECT_ATTRIBUTES obj_attributes = {};
  InitializeObjectAttributes(&obj_attributes, &object_name,
                             OBJ_CASE_INSENSITIVE, nullptr, nullptr);

  unsigned options = 0;
  if (argc == 2 && !base::StringToUint(argv[1], &options)) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  HANDLE handle;
  IO_STATUS_BLOCK io_block = {};
  NTSTATUS status =
      NtCreateFile(&handle, FILE_READ_DATA, &obj_attributes, &io_block, nullptr,
                   0, kSharing, FILE_OPEN, options, nullptr, 0);
  if (NT_SUCCESS(status)) {
    ::CloseHandle(handle);
    return SBOX_TEST_SUCCEEDED;
  } else if (STATUS_ACCESS_DENIED == status) {
    return SBOX_TEST_DENIED;
  } else if (STATUS_OBJECT_NAME_NOT_FOUND == status) {
    return SBOX_TEST_NOT_FOUND;
  }
  return SBOX_TEST_FAILED;
}

// Opens the file in parameter using the NtOpenFile api and returns if the
// call succeeded or not.
SBOX_TESTS_COMMAND int File_OpenSys32(int argc, wchar_t** argv) {
  BINDNTDLL(NtOpenFile);
  if (!NtOpenFile) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  if (argc < 1 || argc > 2) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  std::wstring file = MakePathToSys(argv[0], true);
  UNICODE_STRING object_name;
  ::RtlInitUnicodeString(&object_name, file.c_str());

  OBJECT_ATTRIBUTES obj_attributes = {};
  InitializeObjectAttributes(&obj_attributes, &object_name,
                             OBJ_CASE_INSENSITIVE, nullptr, nullptr);

  unsigned options = 0;
  if (argc == 2 && !base::StringToUint(argv[1], &options)) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  HANDLE handle;
  IO_STATUS_BLOCK io_block = {};
  NTSTATUS status = NtOpenFile(&handle, FILE_READ_DATA, &obj_attributes,
                               &io_block, kSharing, options);
  if (NT_SUCCESS(status)) {
    ::CloseHandle(handle);
    return SBOX_TEST_SUCCEEDED;
  } else if (STATUS_ACCESS_DENIED == status) {
    return SBOX_TEST_DENIED;
  } else if (STATUS_OBJECT_NAME_NOT_FOUND == status) {
    return SBOX_TEST_NOT_FOUND;
  }
  return SBOX_TEST_FAILED;
}

SBOX_TESTS_COMMAND int File_GetDiskSpace(int argc, wchar_t** argv) {
  std::wstring sys_path = MakePathToSys(L"", false);
  if (sys_path.empty()) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }
  ULARGE_INTEGER free_user = {};
  ULARGE_INTEGER total = {};
  ULARGE_INTEGER free_total = {};
  if (::GetDiskFreeSpaceExW(sys_path.c_str(), &free_user, &total,
                            &free_total)) {
    if ((total.QuadPart != 0) && (free_total.QuadPart != 0)) {
      return SBOX_TEST_SUCCEEDED;
    }
  } else {
    if (ERROR_ACCESS_DENIED == ::GetLastError()) {
      return SBOX_TEST_DENIED;
    } else {
      return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
    }
  }
  return SBOX_TEST_SUCCEEDED;
}

// Move a file using the MoveFileEx api and returns if the call succeeded or
// not.
SBOX_TESTS_COMMAND int File_Rename(int argc, wchar_t** argv) {
  if (argc != 2)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  if (::MoveFileEx(argv[0], argv[1], 0))
    return SBOX_TEST_SUCCEEDED;

  if (::GetLastError() != ERROR_ACCESS_DENIED)
    return SBOX_TEST_FAILED;

  return SBOX_TEST_DENIED;
}

// Query the attributes of file in parameter using the NtQueryAttributesFile api
// and NtQueryFullAttributesFile and returns if the call succeeded or not. The
// second argument in argv is "d" or "f" telling if we expect the attributes to
// specify a file or a directory. The expected attribute has to match the real
// attributes for the call to be successful.
SBOX_TESTS_COMMAND int File_QueryAttributes(int argc, wchar_t** argv) {
  BINDNTDLL(NtQueryAttributesFile);
  BINDNTDLL(NtQueryFullAttributesFile);
  if (!NtQueryAttributesFile || !NtQueryFullAttributesFile) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  if (argc != 2)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  bool expect_directory = (L'd' == argv[1][0]);

  UNICODE_STRING object_name;
  std::wstring file = MakePathToSys(argv[0], true);
  ::RtlInitUnicodeString(&object_name, file.c_str());

  OBJECT_ATTRIBUTES obj_attributes = {};
  InitializeObjectAttributes(&obj_attributes, &object_name,
                             OBJ_CASE_INSENSITIVE, nullptr, nullptr);

  FILE_BASIC_INFORMATION info = {};
  FILE_NETWORK_OPEN_INFORMATION full_info = {};
  NTSTATUS status1 = NtQueryAttributesFile(&obj_attributes, &info);
  NTSTATUS status2 = NtQueryFullAttributesFile(&obj_attributes, &full_info);

  if (status1 != status2)
    return SBOX_TEST_FAILED;

  if (NT_SUCCESS(status1)) {
    if (info.FileAttributes != full_info.FileAttributes)
      return SBOX_TEST_FAILED;

    bool is_directory1 = (info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (expect_directory == is_directory1)
      return SBOX_TEST_SUCCEEDED;
  } else if (STATUS_ACCESS_DENIED == status1) {
    return SBOX_TEST_DENIED;
  } else if (STATUS_OBJECT_NAME_NOT_FOUND == status1) {
    return SBOX_TEST_NOT_FOUND;
  }

  return SBOX_TEST_FAILED;
}

// Tries to create a backup of calc.exe in system32 folder. This should fail
// with ERROR_ACCESS_DENIED if everything is working as expected.
SBOX_TESTS_COMMAND int File_CopyFile(int argc, wchar_t** argv) {
  std::wstring calc_path = MakePathToSys(L"calc.exe", false);
  std::wstring calc_backup_path = MakePathToSys(L"calc.exe.bak", false);

  if (::CopyFile(calc_path.c_str(), calc_backup_path.c_str(), FALSE))
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  if (::GetLastError() != ERROR_ACCESS_DENIED)
    return SBOX_TEST_FAILED;

  return SBOX_TEST_SUCCEEDED;
}

TEST(FilePolicyTest, DenyNtCreateCalc) {
  TestRunner runner;
  EXPECT_TRUE(runner.AddRuleSys32(FileSemantics::kAllowAny, L"calc.txt"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"File_CreateSys32 calc.exe"));

  TestRunner before_revert;
  EXPECT_TRUE(
      before_revert.AddRuleSys32(FileSemantics::kAllowAny, L"calc.txt"));
  before_revert.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            before_revert.RunTest(L"File_CreateSys32 calc.exe"));
}

TEST(FilePolicyTest, AllowNtCreateCalc) {
  TestRunner runner;
  EXPECT_TRUE(runner.AddRuleSys32(FileSemantics::kAllowAny, L"calc.exe"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"File_CreateSys32 calc.exe"));

  TestRunner before_revert;
  EXPECT_TRUE(
      before_revert.AddRuleSys32(FileSemantics::kAllowAny, L"calc.exe"));
  before_revert.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            before_revert.RunTest(L"File_CreateSys32 calc.exe"));
}

TEST(FilePolicyTest, AllowNtCreateWithNativePath) {
  std::wstring calc = MakePathToSys(L"calc.exe", false);
  auto opt_nt_path = GetNtPathFromWin32Path(calc);
  ASSERT_TRUE(opt_nt_path);
  std::wstring nt_path = opt_nt_path.value();

  TestRunner runner;
  runner.AllowFileAccess(FileSemantics::kAllowReadonly, nt_path.c_str());
  wchar_t buff[MAX_PATH];
  ::wsprintfW(buff, L"File_CreateSys32 %s", nt_path.c_str());
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(buff));

  TestRunner runner2;
  runner2.AllowFileAccess(FileSemantics::kAllowReadonly, nt_path.c_str());
  nt_path = base::ToLowerASCII(nt_path);
  ::wsprintfW(buff, L"File_CreateSys32 %s", nt_path.c_str());
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner2.RunTest(buff));
}

std::unique_ptr<TestRunner> AllowReadOnlyRunner(wchar_t* temp_file_name) {
  auto runner = std::make_unique<TestRunner>();
  EXPECT_TRUE(
      runner->AllowFileAccess(FileSemantics::kAllowReadonly, temp_file_name));
  return runner;
}

TEST(FilePolicyTest, AllowReadOnly) {
  // Create a temp file because we need write access to it.
  wchar_t temp_directory[MAX_PATH];
  wchar_t temp_file_name[MAX_PATH];
  ASSERT_NE(::GetTempPath(MAX_PATH, temp_directory), 0u);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name), 0u);

  wchar_t command_read[MAX_PATH + 20] = {};
  wsprintf(command_read, L"File_Create Read \"%ls\"", temp_file_name);
  wchar_t command_read_create[MAX_PATH + 20] = {};
  wsprintf(command_read_create, L"File_Create ReadCreate \"%ls\"",
           temp_file_name);
  wchar_t command_write[MAX_PATH + 20] = {};
  wsprintf(command_write, L"File_Create Write \"%ls\"", temp_file_name);

  // Verify that we cannot create the file after revert.
  auto runner = AllowReadOnlyRunner(temp_file_name);
  EXPECT_EQ(SBOX_TEST_DENIED, runner->RunTest(command_read_create));

  // Verify that we don't have write access after revert.
  runner = AllowReadOnlyRunner(temp_file_name);
  EXPECT_EQ(SBOX_TEST_DENIED, runner->RunTest(command_write));

  // Verify that we have read access after revert.
  runner = AllowReadOnlyRunner(temp_file_name);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest(command_read));

  // Verify that we really have write access to the file.
  runner = AllowReadOnlyRunner(temp_file_name);
  runner->SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest(command_write));

  DeleteFile(temp_file_name);
}

// Tests support of "\\\\.\\DeviceName" kind of paths.
TEST(FilePolicyTest, AllowImplicitDeviceName) {
  wchar_t temp_directory[MAX_PATH];
  wchar_t temp_file_name[MAX_PATH];
  ASSERT_NE(::GetTempPath(MAX_PATH, temp_directory), 0u);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name), 0u);

  std::wstring path(temp_file_name);
  auto opt_nt_path = GetNtPathFromWin32Path(path);
  EXPECT_TRUE(opt_nt_path);
  path = opt_nt_path->substr(sandbox::kNTDevicePrefixLen);

  wchar_t command[MAX_PATH + 20] = {};
  wsprintf(command, L"File_Create Read \"\\\\.\\%ls\"", path.c_str());
  path = std::wstring(kNTPrefix) + path;

  TestRunner runner;
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(command));

  TestRunner runner_with_rule;
  EXPECT_TRUE(
      runner_with_rule.AllowFileAccess(FileSemantics::kAllowAny, path.c_str()));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner_with_rule.RunTest(command));

  DeleteFile(temp_file_name);
}

TEST(FilePolicyTest, AllowWildcard) {
  TestRunner runner;

  // Create a temp file because we need write access to it.
  wchar_t temp_directory[MAX_PATH];
  wchar_t temp_file_name[MAX_PATH];
  ASSERT_NE(::GetTempPath(MAX_PATH, temp_directory), 0u);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name), 0u);

  wcscat_s(temp_directory, MAX_PATH, L"*");
  EXPECT_TRUE(runner.AllowFileAccess(FileSemantics::kAllowAny, temp_directory));

  wchar_t command_write[MAX_PATH + 20] = {};
  wsprintf(command_write, L"File_Create Write \"%ls\"", temp_file_name);

  // Verify that we have write access after revert.
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(command_write));

  DeleteFile(temp_file_name);
}

std::unique_ptr<TestRunner> AllowNtCreatePatternRunner() {
  auto runner = std::make_unique<TestRunner>();
  EXPECT_TRUE(runner->AddRuleSys32(FileSemantics::kAllowAny, L"App*.dll"));
  return runner;
}

TEST(FilePolicyTest, AllowNtCreatePatternRule) {
  auto runner = AllowNtCreatePatternRunner();
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner->RunTest(L"File_OpenSys32 apphelp.dll"));

  runner = AllowNtCreatePatternRunner();
  EXPECT_EQ(SBOX_TEST_DENIED, runner->RunTest(L"File_OpenSys32 appwiz.cpl"));

  runner = AllowNtCreatePatternRunner();
  runner->SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner->RunTest(L"File_OpenSys32 apphelp.dll"));

  runner = AllowNtCreatePatternRunner();
  runner->SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest(L"File_OpenSys32 appwiz.cpl"));
}

TEST(FilePolicyTest, CheckNotFound) {
  TestRunner runner;
  EXPECT_TRUE(runner.AddRuleSys32(FileSemantics::kAllowAny, L"n*.dll"));

  EXPECT_EQ(SBOX_TEST_NOT_FOUND,
            runner.RunTest(L"File_OpenSys32 notfound.dll"));
}

TEST(FilePolicyTest, CheckNoLeak) {
  TestRunner runner;
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"File_CreateSys32 notfound.exe"));
}

std::unique_ptr<TestRunner> QueryAttributesFileRunner() {
  auto runner = std::make_unique<TestRunner>();
  EXPECT_TRUE(runner->AddRuleSys32(FileSemantics::kAllowAny, L"apphelp.dll"));
  EXPECT_TRUE(runner->AddRuleSys32(FileSemantics::kAllowAny, L"notfound.exe"));
  EXPECT_TRUE(runner->AddRuleSys32(FileSemantics::kAllowAny, L"drivers"));
  EXPECT_TRUE(
      runner->AddRuleSys32(FileSemantics::kAllowReadonly, L"ipconfig.exe"));
  return runner;
}

TEST(FilePolicyTest, TestQueryAttributesFile) {
  auto runner = QueryAttributesFileRunner();
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner->RunTest(L"File_QueryAttributes drivers d"));

  runner = QueryAttributesFileRunner();
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner->RunTest(L"File_QueryAttributes apphelp.dll f"));

  runner = QueryAttributesFileRunner();
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner->RunTest(L"File_QueryAttributes ipconfig.exe f"));

  runner = QueryAttributesFileRunner();
  EXPECT_EQ(SBOX_TEST_DENIED,
            runner->RunTest(L"File_QueryAttributes ftp.exe f"));

  runner = QueryAttributesFileRunner();
  EXPECT_EQ(SBOX_TEST_NOT_FOUND,
            runner->RunTest(L"File_QueryAttributes notfound.exe f"));
}

// Makes sure that we don't leak information when there is not policy to allow
// a path.
TEST(FilePolicyTest, TestQueryAttributesFileNoPolicy) {
  TestRunner runner;
  EXPECT_EQ(SBOX_TEST_DENIED,
            runner.RunTest(L"File_QueryAttributes ftp.exe f"));

  TestRunner runner2;
  EXPECT_EQ(SBOX_TEST_DENIED,
            runner2.RunTest(L"File_QueryAttributes notfound.exe f"));
}

// Expects 8 file names. Attempts to copy even to odd files will happen.
std::unique_ptr<TestRunner> RenameRunner(
    std::vector<std::wstring>& temp_files) {
  auto runner = std::make_unique<TestRunner>();
  // Add rules to make file0->file1 succeed.
  runner->AllowFileAccess(FileSemantics::kAllowAny, temp_files[0].c_str());
  runner->AllowFileAccess(FileSemantics::kAllowAny, temp_files[1].c_str());

  // Add rules to make file2->file3 fail.
  runner->AllowFileAccess(FileSemantics::kAllowAny, temp_files[2].c_str());
  runner->AllowFileAccess(FileSemantics::kAllowReadonly, temp_files[3].c_str());

  // Add rules to make file4->file5 fail.
  runner->AllowFileAccess(FileSemantics::kAllowReadonly, temp_files[4].c_str());
  runner->AllowFileAccess(FileSemantics::kAllowAny, temp_files[5].c_str());

  // Add rules to make file6->no_pol_file fail.
  runner->AllowFileAccess(FileSemantics::kAllowAny, temp_files[6].c_str());
  return runner;
}

TEST(FilePolicyTest, TestRename) {
  const size_t nFiles = 8;

  // Give access to the temp directory.
  wchar_t temp_directory[MAX_PATH];
  std::vector<std::wstring> temp_files;
  ASSERT_NE(::GetTempPath(MAX_PATH, temp_directory), 0u);
  for (size_t i = 0; i < nFiles; i++) {
    wchar_t temp_file[MAX_PATH];
    ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file), 0u);
    temp_files.push_back(std::wstring(temp_file));
  }

  // Delete the files where the files are going to be renamed to.
  ::DeleteFile(temp_files[1].c_str());
  ::DeleteFile(temp_files[3].c_str());
  ::DeleteFile(temp_files[5].c_str());
  ::DeleteFile(temp_files[7].c_str());

  auto runner = RenameRunner(temp_files);
  wchar_t command[MAX_PATH * 2 + 20] = {};
  wsprintf(command, L"File_Rename \"%ls\" \"%ls\"", temp_files[0].c_str(),
           temp_files[1].c_str());
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest(command));

  runner = RenameRunner(temp_files);
  wsprintf(command, L"File_Rename \"%ls\" \"%ls\"", temp_files[2].c_str(),
           temp_files[3].c_str());
  EXPECT_EQ(SBOX_TEST_DENIED, runner->RunTest(command));

  runner = RenameRunner(temp_files);
  wsprintf(command, L"File_Rename \"%ls\" \"%ls\"", temp_files[4].c_str(),
           temp_files[5].c_str());
  EXPECT_EQ(SBOX_TEST_DENIED, runner->RunTest(command));

  runner = RenameRunner(temp_files);
  wsprintf(command, L"File_Rename \"%ls\" \"%ls\"", temp_files[6].c_str(),
           temp_files[7].c_str());
  EXPECT_EQ(SBOX_TEST_DENIED, runner->RunTest(command));

  // Delete all the files in case they are still there.
  for (auto& file : temp_files)
    ::DeleteFile(file.c_str());
}

std::unique_ptr<TestRunner> AllowNotepadRunner() {
  auto runner = std::make_unique<TestRunner>();
  runner->AddRuleSys32(FileSemantics::kAllowAny, L"notepad.exe");
  return runner;
}

TEST(FilePolicyTest, OpenSys32FilesAllowNotepad) {
  auto runner = AllowNotepadRunner();
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner->RunTest(L"File_Win32Create notepad.exe"));

  runner = AllowNotepadRunner();
  EXPECT_EQ(SBOX_TEST_DENIED, runner->RunTest(L"File_Win32Create calc.exe"));

  runner = AllowNotepadRunner();
  runner->SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner->RunTest(L"File_Win32Create notepad.exe"));

  runner = AllowNotepadRunner();
  runner->SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest(L"File_Win32Create calc.exe"));
}

std::unique_ptr<TestRunner> FileGetDiskSpaceRunner() {
  auto runner = std::make_unique<TestRunner>();
  runner->AddRuleSys32(FileSemantics::kAllowReadonly, L"");
  return runner;
}

TEST(FilePolicyTest, FileGetDiskSpace) {
  auto runner = std::make_unique<TestRunner>();
  EXPECT_EQ(SBOX_TEST_DENIED, runner->RunTest(L"File_GetDiskSpace"));

  runner = std::make_unique<TestRunner>();
  runner->SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest(L"File_GetDiskSpace"));

  // Add an 'allow' rule in the windows\system32 such that GetDiskFreeSpaceEx
  // succeeds (it does an NtOpenFile) but windows\system32\notepad.exe is
  // denied since there is no wild card in the rule.
  runner = FileGetDiskSpaceRunner();
  runner->SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest(L"File_GetDiskSpace"));

  runner = FileGetDiskSpaceRunner();
  runner->SetTestState(AFTER_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest(L"File_GetDiskSpace"));

  runner = FileGetDiskSpaceRunner();
  runner->SetTestState(AFTER_REVERT);
  EXPECT_EQ(SBOX_TEST_DENIED, runner->RunTest(L"File_Win32Create notepad.exe"));
}

std::unique_ptr<TestRunner> ReparsePointRunner(
    std::wstring& temp_dir_wildcard) {
  auto runner = std::make_unique<TestRunner>();
  runner->AllowFileAccess(FileSemantics::kAllowAny, temp_dir_wildcard.c_str());
  return runner;
}

TEST(FilePolicyTest, TestReparsePoint) {
  // Create a temp file because we need write access to it.
  wchar_t temp_directory[MAX_PATH];
  wchar_t temp_file_name[MAX_PATH];
  ASSERT_NE(::GetTempPath(MAX_PATH, temp_directory), 0u);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name), 0u);

  // Delete the file and create a directory instead.
  ASSERT_TRUE(::DeleteFile(temp_file_name));
  ASSERT_TRUE(::CreateDirectory(temp_file_name, nullptr));

  // Create a temporary file in the subfolder.
  std::wstring subfolder = temp_file_name;
  std::wstring temp_file_title = subfolder.substr(subfolder.rfind(L"\\") + 1);
  std::wstring temp_file = subfolder + L"\\file_" + temp_file_title;

  HANDLE file = ::CreateFile(temp_file.c_str(), FILE_WRITE_DATA,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                             CREATE_ALWAYS, 0, nullptr);
  ASSERT_TRUE(INVALID_HANDLE_VALUE != file);
  ASSERT_TRUE(::CloseHandle(file));

  // Create a temporary file in the temp directory.
  std::wstring temp_dir = temp_directory;
  std::wstring temp_file_in_temp = temp_dir + L"file_" + temp_file_title;
  file = ::CreateFile(temp_file_in_temp.c_str(), FILE_WRITE_DATA,
                      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                      CREATE_ALWAYS, 0, nullptr);
  ASSERT_TRUE(INVALID_HANDLE_VALUE != file);
  ASSERT_TRUE(::CloseHandle(file));

  // Give write access to the temp directory.
  std::wstring temp_dir_wildcard = temp_dir + L"*";
  auto runner = ReparsePointRunner(temp_dir_wildcard);

  // Prepare the command to execute.
  std::wstring command_write;
  command_write += L"File_Create Write \"";
  command_write += temp_file;
  command_write += L"\"";

  // Verify that we have write access to the original file
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest(command_write.c_str()));

  // Replace the subfolder by a reparse point to %temp%.
  ::DeleteFile(temp_file.c_str());
  HANDLE dir = ::CreateFile(subfolder.c_str(), FILE_WRITE_DATA,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  EXPECT_TRUE(INVALID_HANDLE_VALUE != dir);

  std::wstring temp_dir_nt;
  temp_dir_nt += L"\\??\\";
  temp_dir_nt += temp_dir;
  EXPECT_TRUE(SetReparsePoint(dir, temp_dir_nt.c_str()));
  EXPECT_TRUE(::CloseHandle(dir));

  // Try to open the file again. This should still work.
  runner = ReparsePointRunner(temp_dir_wildcard);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest(command_write.c_str()));

  // Remove the reparse point.
  dir = ::CreateFile(subfolder.c_str(), FILE_WRITE_DATA,
                     FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                     FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                     nullptr);
  EXPECT_TRUE(INVALID_HANDLE_VALUE != dir);
  EXPECT_TRUE(DeleteReparsePoint(dir));
  EXPECT_TRUE(::CloseHandle(dir));

  // Cleanup.
  EXPECT_TRUE(::DeleteFile(temp_file_in_temp.c_str()));
  EXPECT_TRUE(::RemoveDirectory(subfolder.c_str()));
}

TEST(FilePolicyTest, TestCopyFile) {
  TestRunner runner;
  runner.SetTimeout(2000);

  // Allow read access to calc.exe, this should be on all Windows versions.
  ASSERT_TRUE(runner.AddRuleSys32(FileSemantics::kAllowReadonly, L"calc.exe"));

  sandbox::TargetPolicy* policy = runner.GetPolicy();

  // Set proper mitigation.
  EXPECT_EQ(policy->GetConfig()->SetDelayedProcessMitigations(
                MITIGATION_STRICT_HANDLE_CHECKS),
            SBOX_ALL_OK);

  ASSERT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"File_CopyFile"));
}

TEST(FilePolicyTest, DenyOpenById) {
  std::wstring option = base::NumberToWString(FILE_OPEN_BY_FILE_ID);
  auto runner = AllowNotepadRunner();
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner->RunTest(L"File_CreateSys32 notepad.exe 0"));
  runner = AllowNotepadRunner();
  std::wstring test = L"File_CreateSys32 notepad.exe " + option;
  EXPECT_EQ(SBOX_TEST_DENIED, runner->RunTest(test.c_str()));
  runner = AllowNotepadRunner();
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner->RunTest(L"File_OpenSys32 notepad.exe 0"));
  runner = AllowNotepadRunner();
  test = L"File_OpenSys32 notepad.exe " + option;
  EXPECT_EQ(SBOX_TEST_DENIED, runner->RunTest(test.c_str()));
}

}  // namespace sandbox
