// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_util.h"

#import <memory>

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class DeviceTrustUtilTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        enterprise_connectors::features::kEnableIOSDeviceTrustConnector);
    profile_ = TestProfileIOS::Builder().Build();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Verifies that Device Trust is registered for a regular profile when the
// feature is enabled.
TEST_F(DeviceTrustUtilTest, RegularProfile) {
  EXPECT_TRUE(ShouldRegisterDeviceTrust(profile_.get()));
}

// Verifies that Device Trust is not registered when the feature is disabled.
TEST_F(DeviceTrustUtilTest, FeatureDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(
      enterprise_connectors::features::kEnableIOSDeviceTrustConnector);
  EXPECT_FALSE(ShouldRegisterDeviceTrust(profile_.get()));
}

// Verifies that Device Trust is not registered for an off-the-record profile.
TEST_F(DeviceTrustUtilTest, OffTheRecordProfile) {
  ProfileIOS* otr_profile = profile_->GetOffTheRecordProfile();
  ASSERT_TRUE(otr_profile);
  EXPECT_FALSE(ShouldRegisterDeviceTrust(otr_profile));
}

}  // namespace
