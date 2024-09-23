// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "base/command_line.h"
#import "base/files/file_path.h"
#import "base/files/scoped_temp_dir.h"
#import "base/functional/bind.h"
#import "base/path_service.h"
#import "base/run_loop.h"
#import "base/test/task_environment.h"
#import "components/policy/core/browser/policy_pref_mapping_test.h"
#import "components/policy/core/common/mock_configuration_policy_provider.h"
#import "components/policy/core/common/policy_map.h"
#import "components/policy/policy_constants.h"
#import "ios/chrome/browser/policy/model/enterprise_policy_test_helper.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

class PolicyTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(state_directory_.CreateUniqueTempDir());
    enterprise_policy_helper_ = std::make_unique<EnterprisePolicyTestHelper>(
        state_directory_.GetPath());
    ASSERT_TRUE(enterprise_policy_helper_->GetProfile());

    // Multiple tests use policy/pref_mapping, so compute its path
    // once.
    base::FilePath test_data_directory;
    ASSERT_TRUE(
        base::PathService::Get(ios::DIR_TEST_DATA, &test_data_directory));
    test_case_dir_ = test_data_directory.Append(FILE_PATH_LITERAL("policy"))
                         .Append(FILE_PATH_LITERAL("pref_mapping"));
  }

 protected:
  // Temporary directory to hold preference files.
  base::ScopedTempDir state_directory_;

  // The task environment for this test.
  base::test::TaskEnvironment task_environment_;

  // Enterprise policy boilerplate configuration.
  std::unique_ptr<EnterprisePolicyTestHelper> enterprise_policy_helper_;

  // The path to components/policy/test/data/pref_mapping/.
  base::FilePath test_case_dir_;
};

}  // namespace

TEST_F(PolicyTest, AllPoliciesHaveATestCase) {
  policy::VerifyAllPoliciesHaveATestCase(test_case_dir_);
}

TEST_F(PolicyTest, PolicyToPrefMappings) {
  policy::VerifyPolicyToPrefMappings(
      test_case_dir_, enterprise_policy_helper_->GetLocalState(),
      enterprise_policy_helper_->GetProfile()->GetPrefs(),
      /* signin_profile_prefs= */ nullptr,
      enterprise_policy_helper_->GetPolicyProvider());
}
