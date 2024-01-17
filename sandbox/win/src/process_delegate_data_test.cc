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

SBOX_TESTS_COMMAND int Process_CheckData(int argc, wchar_t** argv) {
  auto* target_services = SandboxFactory::GetTargetServices();
  if (SBOX_ALL_OK != target_services->Init()) {
    return SBOX_TEST_FAILED_SETUP;
  }
  if (argc != 1) {
    return SBOX_TEST_INVALID_PARAMETER;
  }
  std::wstring param(argv[0]);
  auto delegate_data = target_services->GetDelegateData();
  if (!delegate_data.has_value()) {
    return SBOX_TEST_FIRST_ERROR;
  }
  if (!equals(delegate_data.value(), base::as_bytes(base::make_span(param)))) {
    return SBOX_TEST_SECOND_ERROR;
  }
  return SBOX_TEST_SUCCEEDED;
}

TEST(ProcessDelegateData, AddDelegateData) {
  TestRunner runner(JobLevel::kLockdown, USER_UNPROTECTED, USER_UNPROTECTED);
  std::wstring message(L"Delegate-Data-For-The-Target");
  runner.GetPolicy()->AddDelegateData(base::as_bytes(base::make_span(message)));
  std::wstring command = L"Process_CheckData ";
  command.append(message);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(command.c_str()));
}

TEST(ProcessDelegateData, AddDelegateDataAndRule) {
  TestRunner runner(JobLevel::kLockdown, USER_LIMITED, USER_LOCKDOWN);
  std::wstring message(L"Delegate-Data-For-The-Target");
  runner.GetPolicy()->AddDelegateData(base::as_bytes(base::make_span(message)));
  // Rule doesn't matter - but exercises having all three target regions.
  runner.AllowFileAccess(FileSemantics::kAllowAny, L"c:\\windows\\*");
  std::wstring command = L"Process_CheckData ";
  command.append(message);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(command.c_str()));
}

TEST(ProcessDelegateData, NoDelegateData) {
  TestRunner runner(JobLevel::kLockdown, USER_LIMITED, USER_LOCKDOWN);
  std::wstring message(L"Expect-First-Error");
  std::wstring command = L"Process_CheckData ";
  command.append(message);
  EXPECT_EQ(SBOX_TEST_FIRST_ERROR, runner.RunTest(command.c_str()));
}

}  // namespace sandbox
