// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <string>

#include "base/containers/span.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/target_services.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
namespace {
template <typename T>
bool equals(T lhs, T rhs) {
  return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}
}  // namespace

SBOX_TEST_COMMAND(Process_CheckData) {
  auto* target_services = SandboxFactory::GetTargetServices();
  if (SBOX_ALL_OK != target_services->Init()) {
    return SBOX_TEST_FAILED_SETUP;
  }
  if (args.size() != 1) {
    return SBOX_TEST_INVALID_PARAMETER;
  }
  std::wstring_view param = args[0];
  auto delegate_data = target_services->GetDelegateData();
  if (!delegate_data.has_value()) {
    return SBOX_TEST_FIRST_ERROR;
  }
  if (!equals(delegate_data.value(), base::as_byte_span(param))) {
    return SBOX_TEST_SECOND_ERROR;
  }
  return SBOX_TEST_SUCCEEDED;
}

TEST(ProcessDelegateData, AddDelegateData) {
  Process_CheckDataTestRunner runner(JobLevel::kLockdown, USER_UNPROTECTED,
                                     USER_UNPROTECTED);
  std::wstring message(L"Delegate-Data-For-The-Target");
  runner.GetPolicy()->AddDelegateData(base::as_byte_span(message));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(message));
}

TEST(ProcessDelegateData, AddDelegateDataAndRule) {
  Process_CheckDataTestRunner runner(JobLevel::kLockdown, USER_LIMITED,
                                     USER_LOCKDOWN);
  std::wstring message(L"Delegate-Data-For-The-Target");
  runner.GetPolicy()->AddDelegateData(base::as_byte_span(message));
  // Rule doesn't matter - but exercises having all three target regions.
  runner.AllowFileAccess(FileSemantics::kAllowAny, L"c:\\windows\\*");
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(message));
}

TEST(ProcessDelegateData, NoDelegateData) {
  Process_CheckDataTestRunner runner(JobLevel::kLockdown, USER_LIMITED,
                                     USER_LOCKDOWN);
  std::wstring message(L"Expect-First-Error");
  EXPECT_EQ(SBOX_TEST_FIRST_ERROR, runner.RunTest(message));
}

}  // namespace sandbox
