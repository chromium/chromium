// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the validation tests for the sandbox.
// It includes the tests that need to be performed inside the
// sandbox.

#include <stddef.h>

#include "base/win/shlwapi.h"
#include "base/win/windows_version.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

// Callback that generates fresh TestRunners for process access tests.
typedef std::unique_ptr<sandbox::TestRunner> (*RunnerGenerator)();

namespace {

void TestProcessAccess(RunnerGenerator runner_gen, DWORD target) {
  const wchar_t *kCommandTemplate = L"OpenProcessCmd %d %d";
  wchar_t command[1024] = {0};
  std::unique_ptr<sandbox::TestRunner> runner = nullptr;

  // Test all the scary process permissions.
  wsprintf(command, kCommandTemplate, target, PROCESS_CREATE_THREAD);
  runner = runner_gen();
  EXPECT_EQ(sandbox::SBOX_TEST_DENIED, runner->RunTest(command));

  wsprintf(command, kCommandTemplate, target, PROCESS_DUP_HANDLE);
  runner = runner_gen();
  EXPECT_EQ(sandbox::SBOX_TEST_DENIED, runner->RunTest(command));

  wsprintf(command, kCommandTemplate, target, PROCESS_SET_INFORMATION);
  runner = runner_gen();
  EXPECT_EQ(sandbox::SBOX_TEST_DENIED, runner->RunTest(command));

  wsprintf(command, kCommandTemplate, target, PROCESS_VM_OPERATION);
  runner = runner_gen();
  EXPECT_EQ(sandbox::SBOX_TEST_DENIED, runner->RunTest(command));

  wsprintf(command, kCommandTemplate, target, PROCESS_VM_READ);
  runner = runner_gen();
  EXPECT_EQ(sandbox::SBOX_TEST_DENIED, runner->RunTest(command));

  wsprintf(command, kCommandTemplate, target, PROCESS_VM_WRITE);
  runner = runner_gen();
  EXPECT_EQ(sandbox::SBOX_TEST_DENIED, runner->RunTest(command));

  wsprintf(command, kCommandTemplate, target, PROCESS_QUERY_INFORMATION);
  runner = runner_gen();
  EXPECT_EQ(sandbox::SBOX_TEST_DENIED, runner->RunTest(command));

  wsprintf(command, kCommandTemplate, target, WRITE_DAC);
  runner = runner_gen();
  EXPECT_EQ(sandbox::SBOX_TEST_DENIED, runner->RunTest(command));

  wsprintf(command, kCommandTemplate, target, WRITE_OWNER);
  runner = runner_gen();
  EXPECT_EQ(sandbox::SBOX_TEST_DENIED, runner->RunTest(command));

  wsprintf(command, kCommandTemplate, target, READ_CONTROL);
  runner = runner_gen();
  EXPECT_EQ(sandbox::SBOX_TEST_DENIED, runner->RunTest(command));
}

}  // namespace

namespace sandbox {

// Returns true if the volume that contains any_path supports ACL security. The
// input path can contain unexpanded environment strings. Returns false on any
// failure or if the file system does not support file security (such as FAT).
bool VolumeSupportsACLs(const wchar_t* any_path) {
  wchar_t expand[MAX_PATH +1];
  DWORD len =::ExpandEnvironmentStringsW(any_path, expand, _countof(expand));
  if (0 == len) return false;
  if (len >  _countof(expand)) return false;
  if (!::PathStripToRootW(expand)) return false;
  DWORD fs_flags = 0;
  if (!::GetVolumeInformationW(expand, NULL, 0, 0, NULL, &fs_flags, NULL, 0))
    return false;
  if (fs_flags & FILE_PERSISTENT_ACLS) return true;
  return false;
}

// Tests if the suite is working properly.
TEST(ValidationSuite, TestSuite) {
  TestRunner runner;
  ASSERT_EQ(SBOX_TEST_PING_OK, runner.RunTest(L"ping"));
}

// Tests if the file system is correctly protected by the sandbox.
TEST(ValidationSuite, TestFileSystem) {
  // Do not perform the test if the system is using FAT or any other
  // file system that does not have file security.
  ASSERT_TRUE(VolumeSupportsACLs(L"%SystemDrive%\\"));
  ASSERT_TRUE(VolumeSupportsACLs(L"%SystemRoot%\\"));
  ASSERT_TRUE(VolumeSupportsACLs(L"%ProgramFiles%\\"));
  ASSERT_TRUE(VolumeSupportsACLs(L"%Temp%\\"));
  ASSERT_TRUE(VolumeSupportsACLs(L"%AppData%\\"));

  TestRunner runner_sysdrive;
  EXPECT_EQ(SBOX_TEST_DENIED,
            runner_sysdrive.RunTest(L"OpenFileCmd %SystemDrive%"));

  TestRunner runner_sysroot;
  EXPECT_EQ(SBOX_TEST_DENIED,
            runner_sysroot.RunTest(L"OpenFileCmd %SystemRoot%"));

  TestRunner runner_programfiles;
  EXPECT_EQ(SBOX_TEST_DENIED,
            runner_programfiles.RunTest(L"OpenFileCmd %ProgramFiles%"));

  TestRunner runner_system32;
  EXPECT_EQ(SBOX_TEST_DENIED,
            runner_system32.RunTest(L"OpenFileCmd %SystemRoot%\\System32"));

  TestRunner runner_explorer;
  EXPECT_EQ(SBOX_TEST_DENIED,
            runner_explorer.RunTest(L"OpenFileCmd %SystemRoot%\\explorer.exe"));

  TestRunner runner_cursors;
  EXPECT_EQ(SBOX_TEST_DENIED,
            runner_cursors.RunTest(
                L"OpenFileCmd %SystemRoot%\\Cursors\\arrow_i.cur"));

  TestRunner runner_profiles;
  EXPECT_EQ(SBOX_TEST_DENIED,
            runner_profiles.RunTest(L"OpenFileCmd %AllUsersProfile%"));

  TestRunner runner_temp;
  EXPECT_EQ(SBOX_TEST_DENIED, runner_temp.RunTest(L"OpenFileCmd %Temp%"));

  TestRunner runner_appdata;
  EXPECT_EQ(SBOX_TEST_DENIED, runner_appdata.RunTest(L"OpenFileCmd %AppData%"));
}

// Tests if the registry is correctly protected by the sandbox.
TEST(ValidationSuite, TestRegistry) {
  TestRunner runner_hklm;
  EXPECT_EQ(SBOX_TEST_DENIED, runner_hklm.RunTest(L"OpenKey HKLM"));

  TestRunner runner_hkcu;
  EXPECT_EQ(SBOX_TEST_DENIED, runner_hkcu.RunTest(L"OpenKey HKCU"));

  TestRunner runner_hku;
  EXPECT_EQ(SBOX_TEST_DENIED, runner_hku.RunTest(L"OpenKey HKU"));

  TestRunner runner_hklm_key;
  EXPECT_EQ(
      SBOX_TEST_DENIED,
      runner_hklm_key.RunTest(
          L"OpenKey HKLM "
          L"\"Software\\Microsoft\\Windows NT\\CurrentVersion\\WinLogon\""));
}

std::unique_ptr<TestRunner> DesktopRunner() {
  auto runner = std::make_unique<TestRunner>();
  EXPECT_EQ(SBOX_ALL_OK, runner->broker()->CreateAlternateDesktop(
                             Desktop::kAlternateWinstation));
  runner->GetPolicy()->GetConfig()->SetDesktop(Desktop::kAlternateWinstation);
  EXPECT_EQ(SBOX_ALL_OK, runner->GetPolicy()->GetConfig()->SetIntegrityLevel(
                             INTEGRITY_LEVEL_LOW));
  return runner;
}

// Tests that the permissions on the Windowstation does not allow the sandbox
// to get to the interactive desktop or to make the sbox desktop interactive.
TEST(ValidationSuite, TestDesktop) {
  auto runner = DesktopRunner();
  EXPECT_EQ(SBOX_TEST_DENIED, runner->RunTest(L"OpenInteractiveDesktop NULL"));

  runner = DesktopRunner();
  EXPECT_EQ(SBOX_TEST_DENIED, runner->RunTest(L"SwitchToSboxDesktop NULL"));
}

// Tests that the permissions on the Windowstation does not allow the sandbox
// to get to the interactive desktop or to make the sbox desktop interactive.
TEST(ValidationSuite, TestAlternateDesktop) {
  TestRunner runner_no_policy;
  EXPECT_EQ(SBOX_TEST_DENIED,
            runner_no_policy.RunTest(L"EnumAlternateWinsta NULL"));

  TestRunner runner;
  wchar_t command[1024] = {0};
  runner.SetTimeout(3600000);
  EXPECT_EQ(SBOX_ALL_OK, runner.broker()->CreateAlternateDesktop(
                             Desktop::kAlternateWinstation));
  runner.GetPolicy()->GetConfig()->SetDesktop(Desktop::kAlternateWinstation);
  EXPECT_EQ(SBOX_ALL_OK, runner.GetPolicy()->GetConfig()->SetIntegrityLevel(
                             INTEGRITY_LEVEL_LOW));
  // Ensure the desktop is created.
  EXPECT_EQ(SBOX_ALL_OK, runner.broker()->CreateAlternateDesktop(
                             Desktop::kAlternateWinstation));
  std::wstring desktop_name =
      runner.broker()->GetDesktopName(Desktop::kAlternateWinstation);
  desktop_name = desktop_name.substr(desktop_name.find('\\') + 1);
  wsprintf(command, L"OpenAlternateDesktop %lS", desktop_name.c_str());
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(command));
}

std::unique_ptr<TestRunner> AlternateDesktopLocalWinstationRunner() {
  auto runner = std::make_unique<TestRunner>();
  EXPECT_EQ(SBOX_ALL_OK, runner->broker()->CreateAlternateDesktop(
                             Desktop::kAlternateDesktop));
  runner->GetPolicy()->GetConfig()->SetDesktop(Desktop::kAlternateDesktop);
  EXPECT_EQ(SBOX_ALL_OK, runner->GetPolicy()->GetConfig()->SetIntegrityLevel(
                             INTEGRITY_LEVEL_LOW));
  return runner;
}

// Same as TestDesktop, but uses the local winstation, instead of an alternate
// one.
TEST(ValidationSuite, TestAlternateDesktopLocalWinstation) {
  auto runner = AlternateDesktopLocalWinstationRunner();
  EXPECT_EQ(SBOX_TEST_DENIED, runner->RunTest(L"OpenInteractiveDesktop NULL"));

  runner = AlternateDesktopLocalWinstationRunner();
  EXPECT_EQ(SBOX_TEST_DENIED, runner->RunTest(L"SwitchToSboxDesktop NULL"));
}

// Tests if the windows are correctly protected by the sandbox.
TEST(ValidationSuite, TestWindows) {
  // Due to a bug in Windows on builds based on the 19041 branch (20H1, 20H2,
  // 21H1 and 22H2) this test will fail on these versions. See
  // crbug.com/1057656.
  base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  if (os_info->version_number().build >= 19041 &&
      os_info->version_number().build <= 19045) {
    GTEST_SKIP() << "Skipping test for Win10 19041 branch, crbug.com/1057656.";
  }

  wchar_t command[1024] = {0};

  TestRunner runner_getshellwindow;
  wsprintf(command, L"ValidWindow %Id",
           reinterpret_cast<size_t>(::GetShellWindow()));
  EXPECT_EQ(SBOX_TEST_DENIED, runner_getshellwindow.RunTest(command));

  TestRunner runner_findwindow;
  wsprintf(command, L"ValidWindow %Id",
           reinterpret_cast<size_t>(::FindWindow(NULL, NULL)));
  EXPECT_EQ(SBOX_TEST_DENIED, runner_findwindow.RunTest(command));
}

std::unique_ptr<TestRunner> ProcessDenyLockdownRunner() {
  return std::make_unique<TestRunner>();
}

// Tests that a locked-down process cannot open another locked-down process.
TEST(ValidationSuite, TestProcessDenyLockdown) {
  TestRunner target;
  target.SetAsynchronous(true);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, target.RunTest(L"SleepCmd 30000"));

  TestProcessAccess(ProcessDenyLockdownRunner, target.process_id());
}

std::unique_ptr<TestRunner> ProcessDenyLowIntegrityRunner() {
  auto runner = std::make_unique<TestRunner>();
  EXPECT_EQ(SBOX_ALL_OK, runner->GetPolicy()->GetConfig()->SetIntegrityLevel(
                             INTEGRITY_LEVEL_LOW));
  EXPECT_EQ(SBOX_ALL_OK, runner->GetPolicy()->GetConfig()->SetTokenLevel(
                             USER_RESTRICTED_SAME_ACCESS, USER_INTERACTIVE));
  return runner;
}

// Tests that a low-integrity process cannot open a locked-down process (due
// to the integrity label changing after startup via SetDelayedIntegrityLevel).
TEST(ValidationSuite, TestProcessDenyLowIntegrity) {
  TestRunner target;
  target.SetAsynchronous(true);
  target.GetPolicy()->GetConfig()->SetDelayedIntegrityLevel(
      INTEGRITY_LEVEL_LOW);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, target.RunTest(L"SleepCmd 30000"));

  TestProcessAccess(ProcessDenyLowIntegrityRunner, target.process_id());
}

std::unique_ptr<TestRunner> ProcessDenyBelowLowIntegrityRunner() {
  auto runner = std::make_unique<TestRunner>();
  runner->GetPolicy()->GetConfig()->SetDelayedIntegrityLevel(
      INTEGRITY_LEVEL_UNTRUSTED);
  EXPECT_EQ(SBOX_ALL_OK, runner->GetPolicy()->GetConfig()->SetTokenLevel(
                             USER_RESTRICTED_SAME_ACCESS, USER_INTERACTIVE));
  return runner;
}

// Tests that a locked-down process cannot open a low-integrity process.
TEST(ValidationSuite, TestProcessDenyBelowLowIntegrity) {
  TestRunner target;
  target.SetAsynchronous(true);
  EXPECT_EQ(SBOX_ALL_OK, target.GetPolicy()->GetConfig()->SetIntegrityLevel(
                             INTEGRITY_LEVEL_LOW));
  EXPECT_EQ(SBOX_ALL_OK, target.GetPolicy()->GetConfig()->SetTokenLevel(
                             USER_RESTRICTED_SAME_ACCESS, USER_INTERACTIVE));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, target.RunTest(L"SleepCmd 30000"));

  TestProcessAccess(ProcessDenyBelowLowIntegrityRunner, target.process_id());
}

// Tests if the threads are correctly protected by the sandbox.
TEST(ValidationSuite, TestThread) {
  TestRunner runner;
  wchar_t command[1024] = {0};

  wsprintf(command, L"OpenThreadCmd %d", ::GetCurrentThreadId());
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(command));
}

// Tests if an over-limit allocation will be denied.
TEST(ValidationSuite, TestMemoryLimit) {
  TestRunner runner;
  wchar_t command[1024] = {0};
  const int kAllocationSize = 256 * 1024 * 1024;

  wsprintf(command, L"AllocateCmd %d", kAllocationSize);
  runner.GetPolicy()->GetConfig()->SetJobMemoryLimit(kAllocationSize);
  EXPECT_EQ(SBOX_FATAL_MEMORY_EXCEEDED, runner.RunTest(command));
}

// Tests a large allocation will succeed absent limits.
TEST(ValidationSuite, TestMemoryNoLimit) {
  TestRunner runner;
  wchar_t command[1024] = {0};
  const int kAllocationSize = 256 * 1024 * 1024;

  wsprintf(command, L"AllocateCmd %d", kAllocationSize);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(command));
}

}  // namespace sandbox
