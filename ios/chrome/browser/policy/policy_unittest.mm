// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/policy/core/browser/policy_pref_mapping_test.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/policy/enterprise_policy_test_helper.h"
#include "ios/chrome/browser/pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class PolicyTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(state_directory_.CreateUniqueTempDir());
    enterprise_policy_helper_ = std::make_unique<EnterprisePolicyTestHelper>(
        state_directory_.GetPath());
    ASSERT_TRUE(enterprise_policy_helper_->GetBrowserState());

    // Multiple tests use policy_test_cases.json, so compute its path once.
    base::FilePath test_data_directory;
    ASSERT_TRUE(
        base::PathService::Get(ios::DIR_TEST_DATA, &test_data_directory));
    policy_test_cases_path_ = test_data_directory.Append(
        FILE_PATH_LITERAL("policy/policy_test_cases.json"));
  }

 protected:
  // Temporary directory to hold preference files.
  base::ScopedTempDir state_directory_;

  // The task environment for this test.
  base::test::TaskEnvironment task_environment_;

  // Enterprise policy boilerplate configuration.
  std::unique_ptr<EnterprisePolicyTestHelper> enterprise_policy_helper_;

  // The path to |policy_test_cases.json|.
  base::FilePath policy_test_cases_path_;
};

}  // namespace

TEST_F(PolicyTest, AllPoliciesHaveATestCase) {
  policy::VerifyAllPoliciesHaveATestCase(policy_test_cases_path_);
}

TEST_F(PolicyTest, PolicyToPrefMappings) {
  policy::VerifyPolicyToPrefMappings(
      policy_test_cases_path_, enterprise_policy_helper_->GetLocalState(),
      enterprise_policy_helper_->GetBrowserState()->GetPrefs(),
      /* signin_profile_prefs= */ nullptr,
      enterprise_policy_helper_->GetPolicyProvider());
}
