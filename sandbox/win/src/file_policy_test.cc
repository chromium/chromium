// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <winternl.h>

#include <ntstatus.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/scoped_temp_file.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/win/scoped_handle.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_policy.h"
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
// or not.  The first argument in args is the filename. The second argument
// determines the type of access and the dispositino of the file.
SBOX_TEST_COMMAND(File_Create) {
  if (args.size() != 2) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  std::wstring_view operation = args[0];

  if (operation == L"Read") {
    base::win::ScopedHandle file1(CreateFile(args[1].c_str(), GENERIC_READ,
                                             kSharing, nullptr, OPEN_EXISTING,
                                             0, nullptr));
    base::win::ScopedHandle file2(CreateFile(args[1].c_str(), FILE_EXECUTE,
                                             kSharing, nullptr, OPEN_EXISTING,
                                             0, nullptr));

    if (file1.is_valid() == file2.is_valid()) {
      return file1.is_valid() ? SBOX_TEST_SUCCEEDED : SBOX_TEST_DENIED;
    }
    return file1.is_valid() ? SBOX_TEST_FIRST_ERROR : SBOX_TEST_SECOND_ERROR;

  } else if (operation == L"Write") {
    base::win::ScopedHandle file1(CreateFile(
        args[1].c_str(), GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE,
        kSharing, nullptr, OPEN_EXISTING, 0, nullptr));
    base::win::ScopedHandle file2(
        CreateFile(args[1].c_str(), GENERIC_READ | FILE_WRITE_DATA, kSharing,
                   nullptr, OPEN_EXISTING, 0, nullptr));

    if (file1.is_valid() == file2.is_valid()) {
      return file1.is_valid() ? SBOX_TEST_SUCCEEDED : SBOX_TEST_DENIED;
    }
    return file1.is_valid() ? SBOX_TEST_FIRST_ERROR : SBOX_TEST_SECOND_ERROR;

  } else if (operation == L"ReadCreate") {
    base::win::ScopedHandle file2(CreateFile(args[1].c_str(), GENERIC_READ,
                                             kSharing, nullptr, CREATE_NEW, 0,
                                             nullptr));
    base::win::ScopedHandle file1(CreateFile(args[1].c_str(), GENERIC_READ,
                                             kSharing, nullptr, CREATE_ALWAYS,
                                             0, nullptr));

    if (file1.is_valid() == file2.is_valid()) {
      return file1.is_valid() ? SBOX_TEST_SUCCEEDED : SBOX_TEST_DENIED;
    }
    return file1.is_valid() ? SBOX_TEST_FIRST_ERROR : SBOX_TEST_SECOND_ERROR;
  }

  return SBOX_TEST_INVALID_PARAMETER;
}

SBOX_TEST_COMMAND(File_Win32Create) {
  if (args.size() != 1) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  std::wstring full_path = MakePathToSys(args[0], false);
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
SBOX_TEST_COMMAND(File_CreateSys32) {
  BINDNTDLL(NtCreateFile);
  if (!NtCreateFile) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  if (args.size() < 1 || args.size() > 2) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  std::wstring file(args[0]);
  if (0 != _wcsnicmp(file.c_str(), kNTDevicePrefix, kNTDevicePrefixLen))
    file = MakePathToSys(args[0], true);

  UNICODE_STRING object_name;
  ::RtlInitUnicodeString(&object_name, file.c_str());

  OBJECT_ATTRIBUTES obj_attributes = {};
  InitializeObjectAttributes(&obj_attributes, &object_name,
                             OBJ_CASE_INSENSITIVE, nullptr, nullptr);

  unsigned options = 0;
  if (args.size() == 2 && !base::StringToUint(args[1], &options)) {
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
SBOX_TEST_COMMAND(File_OpenSys32) {
  BINDNTDLL(NtOpenFile);
  if (!NtOpenFile) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  if (args.size() < 1 || args.size() > 2) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  std::wstring file = MakePathToSys(args[0], true);
  UNICODE_STRING object_name;
  ::RtlInitUnicodeString(&object_name, file.c_str());

  OBJECT_ATTRIBUTES obj_attributes = {};
  InitializeObjectAttributes(&obj_attributes, &object_name,
                             OBJ_CASE_INSENSITIVE, nullptr, nullptr);

  unsigned options = 0;
  if (args.size() == 2 && !base::StringToUint(args[1], &options)) {
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

SBOX_TEST_COMMAND(File_GetDiskSpace) {
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
SBOX_TEST_COMMAND(File_Rename) {
  if (args.size() != 2) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  if (::MoveFileEx(args[0].c_str(), args[1].c_str(), 0)) {
    return SBOX_TEST_SUCCEEDED;
  }

  if (::GetLastError() != ERROR_ACCESS_DENIED)
    return SBOX_TEST_FAILED;

  return SBOX_TEST_DENIED;
}

// Query the attributes of file in parameter using the NtQueryAttributesFile api
// and NtQueryFullAttributesFile and returns if the call succeeded or not. The
// second argument in args is "d" or "f" telling if we expect the attributes to
// specify a file or a directory. The expected attribute has to match the real
// attributes for the call to be successful.
SBOX_TEST_COMMAND(File_QueryAttributes) {
  BINDNTDLL(NtQueryAttributesFile);
  BINDNTDLL(NtQueryFullAttributesFile);
  if (!NtQueryAttributesFile || !NtQueryFullAttributesFile) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  if (args.size() != 2) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  bool expect_directory = (L'd' == args[1][0]);

  UNICODE_STRING object_name;
  std::wstring file = MakePathToSys(args[0], true);
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
SBOX_TEST_COMMAND(File_CopyFile) {
  std::wstring calc_path = MakePathToSys(L"calc.exe", false);
  std::wstring calc_backup_path = MakePathToSys(L"calc.exe.bak", false);

  if (::CopyFile(calc_path.c_str(), calc_backup_path.c_str(), FALSE))
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  if (::GetLastError() != ERROR_ACCESS_DENIED)
    return SBOX_TEST_FAILED;

  return SBOX_TEST_SUCCEEDED;
}

TEST(FilePolicyTest, DenyNtCreateCalc) {
  File_CreateSys32TestRunner runner;
  EXPECT_TRUE(runner.AddRuleSys32(FileSemantics::kAllowAny, L"calc.txt"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"calc.exe"));

  File_CreateSys32TestRunner before_revert;
  EXPECT_TRUE(
      before_revert.AddRuleSys32(FileSemantics::kAllowAny, L"calc.txt"));
  before_revert.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, before_revert.RunTest(L"calc.exe"));
}

TEST(FilePolicyTest, AllowNtCreateCalc) {
  File_CreateSys32TestRunner runner;
  EXPECT_TRUE(runner.AddRuleSys32(FileSemantics::kAllowAny, L"calc.exe"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"calc.exe"));

  File_CreateSys32TestRunner before_revert;
  EXPECT_TRUE(
      before_revert.AddRuleSys32(FileSemantics::kAllowAny, L"calc.exe"));
  before_revert.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, before_revert.RunTest(L"calc.exe"));
}

TEST(FilePolicyTest, AllowNtCreateWithNativePath) {
  std::wstring calc = MakePathToSys(L"calc.exe", false);
  auto opt_nt_path = GetNtPathFromWin32Path(calc);
  ASSERT_TRUE(opt_nt_path);
  std::wstring nt_path = opt_nt_path.value();

  File_CreateSys32TestRunner runner;
  runner.AllowFileAccess(FileSemantics::kAllowReadonly, nt_path.c_str());
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(nt_path));

  File_CreateSys32TestRunner runner2;
  runner2.AllowFileAccess(FileSemantics::kAllowReadonly, nt_path.c_str());
  nt_path = base::ToLowerASCII(nt_path);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner2.RunTest(nt_path));
}

std::unique_ptr<File_CreateTestRunner> AllowReadOnlyRunner(
    std::wstring_view temp_file_name) {
  auto runner = std::make_unique<File_CreateTestRunner>();
  EXPECT_TRUE(
      runner->AllowFileAccess(FileSemantics::kAllowReadonly, temp_file_name));
  return runner;
}

TEST(FilePolicyTest, AllowReadOnly) {
  base::ScopedTempFile temp_file;
  ASSERT_TRUE(temp_file.Create());
  std::wstring temp_file_name = temp_file.path().value();

  // Create a temp file because we need write access to it.
  // Verify that we cannot create the file after revert.
  auto runner = AllowReadOnlyRunner(temp_file_name);
  EXPECT_EQ(SBOX_TEST_DENIED, runner->RunTest(L"ReadCreate", temp_file_name));

  // Verify that we don't have write access after revert.
  runner = AllowReadOnlyRunner(temp_file_name);
  EXPECT_EQ(SBOX_TEST_DENIED, runner->RunTest(L"Write", temp_file_name));

  // Verify that we have read access after revert.
  runner = AllowReadOnlyRunner(temp_file_name);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest(L"Read", temp_file_name));

  // Verify that we really have write access to the file.
  runner = AllowReadOnlyRunner(temp_file_name);
  runner->SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest(L"Write", temp_file_name));
}

// Tests support of "\\\\.\\DeviceName" kind of paths.
TEST(FilePolicyTest, AllowImplicitDeviceName) {
  base::ScopedTempFile temp_file;
  ASSERT_TRUE(temp_file.Create());
  std::wstring temp_file_name = temp_file.path().value();

  auto opt_nt_path = GetNtPathFromWin32Path(temp_file_name);
  EXPECT_TRUE(opt_nt_path);
  std::wstring path = opt_nt_path->substr(sandbox::kNTDevicePrefixLen);

  std::wstring command_path = L"\\\\.\\" + path;
  path = std::wstring(kNTPrefix) + path;

  File_CreateTestRunner runner;
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Read", command_path));

  File_CreateTestRunner runner_with_rule;
  EXPECT_TRUE(runner_with_rule.AllowFileAccess(FileSemantics::kAllowAny, path));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner_with_rule.RunTest(L"Read", command_path));
}

TEST(FilePolicyTest, AllowWildcard) {
  base::ScopedTempFile temp_file;
  ASSERT_TRUE(temp_file.Create());

  File_CreateTestRunner runner;
  EXPECT_TRUE(runner.AllowFileAccess(
      FileSemantics::kAllowAny, temp_file.path().DirName().value() + L"*"));

  // Verify that we have write access after revert.
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"Write", temp_file.path().value()));
}

std::unique_ptr<File_OpenSys32TestRunner> AllowNtCreatePatternRunner() {
  auto runner = std::make_unique<File_OpenSys32TestRunner>();
  EXPECT_TRUE(runner->AddRuleSys32(FileSemantics::kAllowAny, L"App*.dll"));
  return runner;
}

TEST(FilePolicyTest, AllowNtCreatePatternRule) {
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            AllowNtCreatePatternRunner()->RunTest(L"apphelp.dll"));
  EXPECT_EQ(SBOX_TEST_DENIED,
            AllowNtCreatePatternRunner()->RunTest(L"appwiz.cpl"));

  auto runner = AllowNtCreatePatternRunner();
  runner->SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest(L"apphelp.dll"));

  runner = AllowNtCreatePatternRunner();
  runner->SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest(L"appwiz.cpl"));
}

TEST(FilePolicyTest, CheckNotFound) {
  File_OpenSys32TestRunner runner;
  EXPECT_TRUE(runner.AddRuleSys32(FileSemantics::kAllowAny, L"n*.dll"));

  EXPECT_EQ(SBOX_TEST_NOT_FOUND, runner.RunTest(L"notfound.dll"));
}

TEST(FilePolicyTest, CheckNoLeak) {
  File_CreateSys32TestRunner runner;
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"notfound.exe"));
}

std::unique_ptr<File_QueryAttributesTestRunner> QueryAttributesFileRunner() {
  auto runner = std::make_unique<File_QueryAttributesTestRunner>();
  EXPECT_TRUE(runner->AddRuleSys32(FileSemantics::kAllowAny, L"apphelp.dll"));
  EXPECT_TRUE(runner->AddRuleSys32(FileSemantics::kAllowAny, L"notfound.exe"));
  EXPECT_TRUE(runner->AddRuleSys32(FileSemantics::kAllowAny, L"drivers"));
  EXPECT_TRUE(
      runner->AddRuleSys32(FileSemantics::kAllowReadonly, L"ipconfig.exe"));
  return runner;
}

TEST(FilePolicyTest, TestQueryAttributesFile) {
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            QueryAttributesFileRunner()->RunTest(L"drivers", L"d"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            QueryAttributesFileRunner()->RunTest(L"apphelp.dll", L"f"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            QueryAttributesFileRunner()->RunTest(L"ipconfig.exe", L"f"));
  EXPECT_EQ(SBOX_TEST_DENIED,
            QueryAttributesFileRunner()->RunTest(L"ftp.exe", L"f"));
  EXPECT_EQ(SBOX_TEST_NOT_FOUND,
            QueryAttributesFileRunner()->RunTest(L"notfound.exe", L"f"));
}

// Makes sure that we don't leak information when there is not policy to allow
// a path.
TEST(FilePolicyTest, TestQueryAttributesFileNoPolicy) {
  File_QueryAttributesTestRunner runner;
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"ftp.exe", L"f"));

  File_QueryAttributesTestRunner runner2;
  EXPECT_EQ(SBOX_TEST_DENIED, runner2.RunTest(L"notfound.exe", L"f"));
}

// Expects 8 file names. Attempts to copy even to odd files will happen.
std::unique_ptr<File_RenameTestRunner> RenameRunner(
    const std::vector<std::wstring>& temp_files) {
  auto runner = std::make_unique<File_RenameTestRunner>();
  // Add rules to make file0->file1 succeed.
  runner->AllowFileAccess(FileSemantics::kAllowAny, temp_files[0]);
  runner->AllowFileAccess(FileSemantics::kAllowAny, temp_files[1]);

  // Add rules to make file2->file3 fail.
  runner->AllowFileAccess(FileSemantics::kAllowAny, temp_files[2]);
  runner->AllowFileAccess(FileSemantics::kAllowReadonly, temp_files[3]);

  // Add rules to make file4->file5 fail.
  runner->AllowFileAccess(FileSemantics::kAllowReadonly, temp_files[4]);
  runner->AllowFileAccess(FileSemantics::kAllowAny, temp_files[5]);

  // Add rules to make file6->no_pol_file fail.
  runner->AllowFileAccess(FileSemantics::kAllowAny, temp_files[6]);
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
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest(temp_files[0], temp_files[1]));

  runner = RenameRunner(temp_files);
  EXPECT_EQ(SBOX_TEST_DENIED, runner->RunTest(temp_files[2], temp_files[3]));

  runner = RenameRunner(temp_files);
  EXPECT_EQ(SBOX_TEST_DENIED, runner->RunTest(temp_files[4], temp_files[5]));

  runner = RenameRunner(temp_files);
  EXPECT_EQ(SBOX_TEST_DENIED, runner->RunTest(temp_files[6], temp_files[7]));

  // Delete all the files in case they are still there.
  for (auto& file : temp_files)
    ::DeleteFile(file.c_str());
}

template <typename T>
std::unique_ptr<T> AllowNotepadRunner() {
  auto runner = std::make_unique<T>();
  runner->AddRuleSys32(FileSemantics::kAllowAny, L"notepad.exe");
  return runner;
}

TEST(FilePolicyTest, OpenSys32FilesAllowNotepad) {
  auto runner = AllowNotepadRunner<File_Win32CreateTestRunner>();
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest(L"notepad.exe"));

  runner = AllowNotepadRunner<File_Win32CreateTestRunner>();
  EXPECT_EQ(SBOX_TEST_DENIED, runner->RunTest(L"calc.exe"));

  runner = AllowNotepadRunner<File_Win32CreateTestRunner>();
  runner->SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest(L"notepad.exe"));

  runner = AllowNotepadRunner<File_Win32CreateTestRunner>();
  runner->SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest(L"calc.exe"));
}

std::unique_ptr<File_GetDiskSpaceTestRunner> FileGetDiskSpaceRunner() {
  auto runner = std::make_unique<File_GetDiskSpaceTestRunner>();
  runner->AddRuleSys32(FileSemantics::kAllowReadonly, L"");
  return runner;
}

TEST(FilePolicyTest, FileGetDiskSpace) {
  File_GetDiskSpaceTestRunner runner;
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest());

  File_GetDiskSpaceTestRunner runner2;
  runner2.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner2.RunTest());

  // Add an 'allow' rule in the windows\system32 such that GetDiskFreeSpaceEx
  // succeeds (it does an NtOpenFile) but windows\system32\notepad.exe is
  // denied since there is no wild card in the rule.
  auto runner3 = FileGetDiskSpaceRunner();
  runner3->SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner3->RunTest());

  runner3 = FileGetDiskSpaceRunner();
  runner3->SetTestState(AFTER_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner3->RunTest());

  File_Win32CreateTestRunner runner4;
  runner4.AddRuleSys32(FileSemantics::kAllowReadonly, L"");
  runner4.SetTestState(AFTER_REVERT);
  EXPECT_EQ(SBOX_TEST_DENIED, runner4.RunTest(L"notepad.exe"));
}

std::unique_ptr<File_CreateTestRunner> ReparsePointRunner(
    const std::wstring_view temp_dir_wildcard) {
  auto runner = std::make_unique<File_CreateTestRunner>();
  runner->AllowFileAccess(FileSemantics::kAllowAny, temp_dir_wildcard);
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

  // Verify that we have write access to the original file
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest(L"Write", temp_file));

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
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest(L"Write", temp_file));

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
  File_CopyFileTestRunner runner;
  runner.SetTimeout(base::Seconds(2));

  // Allow read access to calc.exe, this should be on all Windows versions.
  ASSERT_TRUE(runner.AddRuleSys32(FileSemantics::kAllowReadonly, L"calc.exe"));

  // Set proper mitigation.
  EXPECT_EQ(runner.GetConfig()->SetDelayedProcessMitigations(
                MITIGATION_STRICT_HANDLE_CHECKS),
            SBOX_ALL_OK);

  ASSERT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest());
}

TEST(FilePolicyTest, DenyOpenById) {
  unsigned int option = FILE_OPEN_BY_FILE_ID;
  auto runner = AllowNotepadRunner<File_CreateSys32TestRunner>();
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest(L"notepad.exe", 0));
  runner = AllowNotepadRunner<File_CreateSys32TestRunner>();
  EXPECT_EQ(SBOX_TEST_DENIED, runner->RunTest(L"notepad.exe", option));

  auto runner2 = AllowNotepadRunner<File_OpenSys32TestRunner>();
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner2->RunTest(L"notepad.exe", 0));
  runner2 = AllowNotepadRunner<File_OpenSys32TestRunner>();
  EXPECT_EQ(SBOX_TEST_DENIED, runner2->RunTest(L"notepad.exe", option));
}

}  // namespace sandbox
