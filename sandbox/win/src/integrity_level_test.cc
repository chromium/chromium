// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <optional>

#include "base/process/process_info.h"
#include "base/win/access_token.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

// Must be last or StrCat defn conflicts.
#include <atlsecurity.h>

namespace sandbox {

SBOX_TEST_COMMAND(CheckUntrustedIntegrityLevel) {
  return (base::GetCurrentProcessIntegrityLevel() == base::UNTRUSTED_INTEGRITY)
             ? SBOX_TEST_SUCCEEDED
             : SBOX_TEST_FAILED;
}

SBOX_TEST_COMMAND(CheckLowIntegrityLevel) {
  return (base::GetCurrentProcessIntegrityLevel() == base::LOW_INTEGRITY)
             ? SBOX_TEST_SUCCEEDED
             : SBOX_TEST_FAILED;
}

SBOX_TEST_COMMAND(CheckIntegrityLevel) {
  std::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromEffective();
  if (!token) {
    return SBOX_TEST_FAILED;
  }

  if (token->IntegrityLevel() == SECURITY_MANDATORY_LOW_RID) {
    return SBOX_TEST_SUCCEEDED;
  }

  return SBOX_TEST_DENIED;
}

std::unique_ptr<CheckIntegrityLevelTestRunner> LowILRealRunner() {
  auto runner = std::make_unique<CheckIntegrityLevelTestRunner>(
      JobLevel::kLockdown, USER_INTERACTIVE, USER_INTERACTIVE);
  runner->SetTimeout(INFINITE);
  EXPECT_EQ(SBOX_ALL_OK, runner->broker()->CreateAlternateDesktop(
                             Desktop::kAlternateWinstation));
  runner->GetConfig()->SetDesktop(Desktop::kAlternateWinstation);
  EXPECT_EQ(SBOX_ALL_OK,
            runner->GetConfig()->SetIntegrityLevel(INTEGRITY_LEVEL_LOW));
  return runner;
}

TEST(IntegrityLevelTest, TestLowILReal) {
  auto runner = LowILRealRunner();
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest());

  runner = LowILRealRunner();
  runner->SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest());
}

std::unique_ptr<CheckIntegrityLevelTestRunner> LowILDelayedRunner() {
  auto runner = std::make_unique<CheckIntegrityLevelTestRunner>(
      JobLevel::kLockdown, USER_INTERACTIVE, USER_INTERACTIVE);
  runner->SetTimeout(INFINITE);
  runner->GetConfig()->SetDelayedIntegrityLevel(INTEGRITY_LEVEL_LOW);
  return runner;
}

TEST(DelayedIntegrityLevelTest, TestLowILDelayed) {
  auto runner = LowILDelayedRunner();
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner->RunTest());

  runner = LowILDelayedRunner();
  runner->SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_DENIED, runner->RunTest());
}

TEST(IntegrityLevelTest, TestNoILChange) {
  CheckIntegrityLevelTestRunner runner(JobLevel::kLockdown, USER_INTERACTIVE,
                                       USER_INTERACTIVE);

  runner.SetTimeout(INFINITE);

  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest());
}

TEST(IntegrityLevelTest, TestUntrustedIL) {
  CheckUntrustedIntegrityLevelTestRunner runner(
      JobLevel::kLockdown, USER_RESTRICTED_SAME_ACCESS, USER_LOCKDOWN);
  auto* config = runner.GetConfig();
  EXPECT_EQ(SBOX_ALL_OK, config->SetIntegrityLevel(INTEGRITY_LEVEL_LOW));
  config->SetDelayedIntegrityLevel(INTEGRITY_LEVEL_UNTRUSTED);
  config->SetLockdownDefaultDacl();

  runner.SetTimeout(INFINITE);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest());
}

TEST(IntegrityLevelTest, TestLowIL) {
  CheckLowIntegrityLevelTestRunner runner(
      JobLevel::kLockdown, USER_RESTRICTED_SAME_ACCESS, USER_LOCKDOWN);
  auto* config = runner.GetConfig();
  EXPECT_EQ(SBOX_ALL_OK, config->SetIntegrityLevel(INTEGRITY_LEVEL_LOW));
  config->SetDelayedIntegrityLevel(INTEGRITY_LEVEL_LOW);
  config->SetLockdownDefaultDacl();

  runner.SetTimeout(INFINITE);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest());
}

}  // namespace sandbox
