// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <atlsecurity.h>

#include <optional>

#include "base/process/process_info.h"
#include "base/win/access_token.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

SBOX_TESTS_COMMAND int CheckUntrustedIntegrityLevel(int argc, wchar_t** argv) {
  return (base::GetCurrentProcessIntegrityLevel() == base::UNTRUSTED_INTEGRITY)
             ? SBOX_TEST_SUCCEEDED
             : SBOX_TEST_FAILED;
}

SBOX_TESTS_COMMAND int CheckLowIntegrityLevel(int argc, wchar_t** argv) {
  return (base::GetCurrentProcessIntegrityLevel() == base::LOW_INTEGRITY)
             ? SBOX_TEST_SUCCEEDED
             : SBOX_TEST_FAILED;
}

SBOX_TESTS_COMMAND int CheckIntegrityLevel(int argc, wchar_t** argv) {
  std::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromEffective();
  if (!token)
    return SBOX_TEST_FAILED;

  if (token->IntegrityLevel() == SECURITY_MANDATORY_LOW_RID)
    return SBOX_TEST_SUCCEEDED;

  return SBOX_TEST_DENIED;
}

std::unique_ptr<TestRunner> LowILRealRunner() {
  auto runner = std::make_unique<TestRunner>(
      JobLevel::kLockdown, USER_INTERACTIVE, USER_INTERACTIVE);
  runner->SetTimeout(INFINITE);
  EXPECT_EQ(SBOX_ALL_OK, runner->broker()->CreateAlternateDesktop(
                             Desktop::kAlternateWinstation));
  runner->GetPolicy()->GetConfig()->SetDesktop(Desktop::kAlternateWinstation);
  EXPECT_EQ(SBOX_ALL_OK, runner->GetPolicy()->GetConfig()->SetIntegrityLevel(
                             INTEGRITY_LEVEL_LOW));
  return runner;
}

TEST(IntegrityLevelTest, TestLowILReal) {
  auto runner = LowILRealRunner();
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest(L"CheckIntegrityLevel"));

  runner = LowILRealRunner();
  runner->SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest(L"CheckIntegrityLevel"));
}

std::unique_ptr<TestRunner> LowILDelayedRunner() {
  auto runner = std::make_unique<TestRunner>(
      JobLevel::kLockdown, USER_INTERACTIVE, USER_INTERACTIVE);
  runner->SetTimeout(INFINITE);
  runner->GetPolicy()->GetConfig()->SetDelayedIntegrityLevel(
      INTEGRITY_LEVEL_LOW);
  return runner;
}

TEST(DelayedIntegrityLevelTest, TestLowILDelayed) {
  auto runner = LowILDelayedRunner();
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest(L"CheckIntegrityLevel"));

  runner = LowILDelayedRunner();
  runner->SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_DENIED, runner->RunTest(L"CheckIntegrityLevel"));
}

TEST(IntegrityLevelTest, TestNoILChange) {
  TestRunner runner(JobLevel::kLockdown, USER_INTERACTIVE, USER_INTERACTIVE);

  runner.SetTimeout(INFINITE);

  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"CheckIntegrityLevel"));
}

TEST(IntegrityLevelTest, TestUntrustedIL) {
  TestRunner runner(JobLevel::kLockdown, USER_RESTRICTED_SAME_ACCESS,
                    USER_LOCKDOWN);
  auto* config = runner.GetPolicy()->GetConfig();
  EXPECT_EQ(SBOX_ALL_OK, config->SetIntegrityLevel(INTEGRITY_LEVEL_LOW));
  config->SetDelayedIntegrityLevel(INTEGRITY_LEVEL_UNTRUSTED);
  config->SetLockdownDefaultDacl();

  runner.SetTimeout(INFINITE);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"CheckUntrustedIntegrityLevel"));
}

TEST(IntegrityLevelTest, TestLowIL) {
  TestRunner runner(JobLevel::kLockdown, USER_RESTRICTED_SAME_ACCESS,
                    USER_LOCKDOWN);
  auto* config = runner.GetPolicy()->GetConfig();
  EXPECT_EQ(SBOX_ALL_OK, config->SetIntegrityLevel(INTEGRITY_LEVEL_LOW));
  config->SetDelayedIntegrityLevel(INTEGRITY_LEVEL_LOW);
  config->SetLockdownDefaultDacl();

  runner.SetTimeout(INFINITE);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckLowIntegrityLevel"));
}

}  // namespace sandbox
