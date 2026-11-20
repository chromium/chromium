// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_policy_utils.h"

#import <memory>

#import "base/test/scoped_feature_list.h"
#import "base/values.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/enterprise/data_protection/public/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using policy::policy_prefs::kDisableScreenshots;

// Test fixture for `IsScreenshotsDisabledByPolicy()`.
class DataProtectionPolicyUtilsTest : public PlatformTest {
 protected:
  DataProtectionPolicyUtilsTest() {
    profile_ = TestProfileIOS::Builder().Build();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that `IsScreenshotsDisabledByPolicy()` returns false for a null
// profile.
TEST_F(DataProtectionPolicyUtilsTest, NullProfileReturnsFalse) {
  EXPECT_FALSE(IsScreenshotsDisabledByPolicy(nullptr));
}

// Tests that `IsScreenshotsDisabledByPolicy()` returns false when the policy
// pref is unset.
TEST_F(DataProtectionPolicyUtilsTest, DefaultUnsetReturnsFalse) {
  EXPECT_FALSE(IsScreenshotsDisabledByPolicy(profile_.get()));
  EXPECT_FALSE(
      IsScreenshotsDisabledByPolicy(profile_->GetOffTheRecordProfile()));
}

// Tests that `IsScreenshotsDisabledByPolicy()` returns true for both regular
// and off-the-record profiles when the policy pref is managed true.
TEST_F(DataProtectionPolicyUtilsTest,
       ManagedTrueReturnsTrueForRegularAndIncognito) {
  profile_->GetTestingPrefService()->SetManagedPref(kDisableScreenshots,
                                                    base::Value(true));
  EXPECT_TRUE(IsScreenshotsDisabledByPolicy(profile_.get()));
  EXPECT_TRUE(
      IsScreenshotsDisabledByPolicy(profile_->GetOffTheRecordProfile()));
}

// Tests that `IsScreenshotsDisabledByPolicy()` returns false when the policy
// pref is managed false.
TEST_F(DataProtectionPolicyUtilsTest, ManagedFalseReturnsFalse) {
  profile_->GetTestingPrefService()->SetManagedPref(kDisableScreenshots,
                                                    base::Value(false));
  EXPECT_FALSE(IsScreenshotsDisabledByPolicy(profile_.get()));
  EXPECT_FALSE(
      IsScreenshotsDisabledByPolicy(profile_->GetOffTheRecordProfile()));
}

// Tests that `IsScreenshotsDisabledByPolicy()` returns false when the
// `kDisableScreenshotsPolicyIOS` kill switch is disabled, even if the policy
// pref is managed true.
TEST_F(DataProtectionPolicyUtilsTest, KillSwitchDisabledReturnsFalse) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kDisableScreenshotsPolicyIOS);

  profile_->GetTestingPrefService()->SetManagedPref(kDisableScreenshots,
                                                    base::Value(true));
  EXPECT_FALSE(IsScreenshotsDisabledByPolicy(profile_.get()));
  EXPECT_FALSE(
      IsScreenshotsDisabledByPolicy(profile_->GetOffTheRecordProfile()));
}

}  // namespace
