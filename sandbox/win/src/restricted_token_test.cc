// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// Integration tests for restricted tokens.

#include <stddef.h>
#include <string>

#include <optional>
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/access_token.h"
#include "base/win/scoped_handle.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/target_services.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

int RunOpenProcessTest(bool unsandboxed,
                       bool lockdown_dacl,
                       DWORD access_mask) {
  TestRunner runner(JobLevel::kUnprotected, USER_RESTRICTED_SAME_ACCESS,
                    USER_LOCKDOWN);
  auto* config = runner.GetPolicy()->GetConfig();
  config->SetDelayedIntegrityLevel(INTEGRITY_LEVEL_UNTRUSTED);
  ResultCode result = config->SetIntegrityLevel(INTEGRITY_LEVEL_LOW);
  if (result != SBOX_ALL_OK)
    return SBOX_TEST_FAILED_SETUP;
  if (lockdown_dacl)
    config->SetLockdownDefaultDacl();
  runner.SetAsynchronous(true);
  // This spins up a renderer level process, we don't care about the result.
  runner.RunTest(L"IntegrationTestsTest_args 1");

  TestRunner runner2(JobLevel::kUnprotected, USER_RESTRICTED_SAME_ACCESS,
                     USER_LIMITED);
  auto* config2 = runner2.GetPolicy()->GetConfig();
  config2->SetDelayedIntegrityLevel(INTEGRITY_LEVEL_LOW);
  result = config2->SetIntegrityLevel(INTEGRITY_LEVEL_LOW);
  if (result != SBOX_ALL_OK)
    return SBOX_TEST_FAILED_SETUP;
  runner2.SetUnsandboxed(unsandboxed);
  return runner2.RunTest(
      base::ASCIIToWide(
          base::StringPrintf("RestrictedTokenTest_openprocess %lu 0X%08lX",
                             runner.process_id(), access_mask))
          .c_str());
}

int RunRestrictedOpenProcessTest(bool unsandboxed,
                                 bool lockdown_dacl,
                                 DWORD access_mask) {
  TestRunner runner(JobLevel::kUnprotected, USER_RESTRICTED_SAME_ACCESS,
                    USER_LIMITED);
  auto* config = runner.GetPolicy()->GetConfig();
  config->SetDelayedIntegrityLevel(INTEGRITY_LEVEL_LOW);
  ResultCode result = config->SetIntegrityLevel(INTEGRITY_LEVEL_LOW);
  if (result != SBOX_ALL_OK)
    return SBOX_TEST_FAILED_SETUP;

  if (lockdown_dacl) {
    config->SetLockdownDefaultDacl();
    config->AddRestrictingRandomSid();
  }
  runner.SetAsynchronous(true);
  // This spins up a GPU level process, we don't care about the result.
  runner.RunTest(L"IntegrationTestsTest_args 1");

  TestRunner runner2(JobLevel::kUnprotected, USER_RESTRICTED_SAME_ACCESS,
                     USER_LIMITED);
  auto* config2 = runner2.GetPolicy()->GetConfig();
  config2->SetDelayedIntegrityLevel(INTEGRITY_LEVEL_LOW);
  result = config2->SetIntegrityLevel(INTEGRITY_LEVEL_LOW);
  if (result != SBOX_ALL_OK)
    return SBOX_TEST_FAILED_SETUP;
  runner2.SetUnsandboxed(unsandboxed);
  return runner2.RunTest(
      base::ASCIIToWide(
          base::StringPrintf("RestrictedTokenTest_openprocess %lu 0X%08lX",
                             runner.process_id(), access_mask))
          .c_str());
}

int RunRestrictedSelfOpenProcessTest(bool add_random_sid, DWORD access_mask) {
  TestRunner runner(JobLevel::kUnprotected, USER_RESTRICTED_SAME_ACCESS,
                    USER_LIMITED);
  auto* config = runner.GetPolicy()->GetConfig();
  config->SetDelayedIntegrityLevel(INTEGRITY_LEVEL_LOW);
  ResultCode result = config->SetIntegrityLevel(INTEGRITY_LEVEL_LOW);
  if (result != SBOX_ALL_OK) {
    return SBOX_TEST_FAILED_SETUP;
  }
  config->SetLockdownDefaultDacl();
  if (add_random_sid) {
    config->AddRestrictingRandomSid();
  }

  return runner.RunTest(
      base::ASCIIToWide(
          base::StringPrintf("RestrictedTokenTest_currentprocess_dup 0X%08lX",
                             access_mask))
          .c_str());
}

}  // namespace

// Opens a process based on a PID and access mask passed on the command line.
// Returns SBOX_TEST_SUCCEEDED if process opened successfully.
SBOX_TESTS_COMMAND int RestrictedTokenTest_openprocess(int argc,
                                                       wchar_t** argv) {
  if (argc < 2)
    return SBOX_TEST_NOT_FOUND;
  DWORD pid = _wtoi(argv[0]);
  if (pid == 0)
    return SBOX_TEST_NOT_FOUND;
  DWORD desired_access = wcstoul(argv[1], nullptr, 0);
  base::win::ScopedHandle process_handle(
      ::OpenProcess(desired_access, false, pid));
  if (process_handle.is_valid()) {
    return SBOX_TEST_SUCCEEDED;
  }

  return SBOX_TEST_DENIED;
}

// Opens a process through duplication. This is to avoid the OpenProcess hook.
SBOX_TESTS_COMMAND int RestrictedTokenTest_currentprocess_dup(int argc,
                                                              wchar_t** argv) {
  if (argc < 1)
    return SBOX_TEST_NOT_FOUND;
  DWORD desired_access = wcstoul(argv[0], nullptr, 0);

  HANDLE dup_handle;
  if (!::DuplicateHandle(::GetCurrentProcess(), ::GetCurrentProcess(),
                         ::GetCurrentProcess(), &dup_handle, 0, FALSE, 0)) {
    return SBOX_TEST_FIRST_ERROR;
  }
  base::win::ScopedHandle process_handle(dup_handle);
  if (::DuplicateHandle(::GetCurrentProcess(), process_handle.get(),
                        ::GetCurrentProcess(), &dup_handle, desired_access,
                        FALSE, 0)) {
    ::CloseHandle(dup_handle);
    return SBOX_TEST_SUCCEEDED;
  }

  if (::GetLastError() != ERROR_ACCESS_DENIED)
    return SBOX_TEST_SECOND_ERROR;
  return SBOX_TEST_DENIED;
}

// Opens a the process token and checks if it's restricted.
SBOX_TESTS_COMMAND int RestrictedTokenTest_IsRestricted(int argc,
                                                        wchar_t** argv) {
  std::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromCurrentProcess();
  if (!token)
    return SBOX_TEST_FIRST_ERROR;
  return token->RestrictedSids().size() > 0 ? SBOX_TEST_SUCCEEDED
                                            : SBOX_TEST_FAILED;
}

TEST(RestrictedTokenTest, OpenLowPrivilegedProcess) {
  // Test limited privilege to renderer open.
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            RunOpenProcessTest(false, false, GENERIC_READ | GENERIC_WRITE));
  // Test limited privilege to renderer open with lockdowned DACL.
  ASSERT_EQ(SBOX_TEST_DENIED,
            RunOpenProcessTest(false, true, GENERIC_READ | GENERIC_WRITE));
  // Ensure we also can't get any access to the process.
  ASSERT_EQ(SBOX_TEST_DENIED, RunOpenProcessTest(false, true, MAXIMUM_ALLOWED));
  // Also check for explicit owner allowed WRITE_DAC right.
  ASSERT_EQ(SBOX_TEST_DENIED, RunOpenProcessTest(false, true, WRITE_DAC));
  // Ensure unsandboxed process can still open the renderer for all access.
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            RunOpenProcessTest(true, true, PROCESS_ALL_ACCESS));
}

TEST(RestrictedTokenTest, CheckNonAdminRestricted) {
  TestRunner runner(JobLevel::kUnprotected, USER_RESTRICTED_SAME_ACCESS,
                    USER_RESTRICTED_NON_ADMIN);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"RestrictedTokenTest_IsRestricted"));
}

TEST(RestrictedTokenTest, OpenProcessSameSandboxRandomSid) {
  // Test process to process open when not using random SID.
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            RunRestrictedOpenProcessTest(false, false, GENERIC_ALL));
  // Test process to process open when using random SID.
  ASSERT_EQ(SBOX_TEST_DENIED,
            RunRestrictedOpenProcessTest(false, true, MAXIMUM_ALLOWED));
  // Test process to process open when not using random SID and opening from
  // unsandboxed.
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            RunRestrictedOpenProcessTest(true, false, GENERIC_ALL));
  // Test process to process open when using random SID and opening from
  // unsandboxed.
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            RunRestrictedOpenProcessTest(true, true, GENERIC_ALL));
}

TEST(RestrictedTokenTest, OpenProcessSelfRandomSid) {
  // Test process can't open self when not using random SID.
  ASSERT_EQ(SBOX_TEST_DENIED,
            RunRestrictedSelfOpenProcessTest(false, PROCESS_ALL_ACCESS));
  // Test process can open self when using random SID.
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            RunRestrictedSelfOpenProcessTest(true, PROCESS_ALL_ACCESS));
}

}  // namespace sandbox
