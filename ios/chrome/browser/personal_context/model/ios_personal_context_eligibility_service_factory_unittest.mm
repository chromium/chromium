// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/personal_context/model/ios_personal_context_eligibility_service_factory.h"

#import "base/test/scoped_feature_list.h"
#import "components/personal_context/core/personal_context_eligibility_service.h"
#import "components/personal_context/core/personal_context_features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Test fixture for IOSPersonalContextEligibilityServiceFactory.
class IOSPersonalContextEligibilityServiceFactoryTest : public PlatformTest {
 public:
  IOSPersonalContextEligibilityServiceFactoryTest() = default;

 protected:
  web::WebTaskEnvironment task_environment_;
};

// Tests that PersonalContextEligibilityService is instantiated for a regular
// profile when the kPersonalContext feature flag is enabled.
TEST_F(IOSPersonalContextEligibilityServiceFactoryTest,
       CheckServiceNotNullWhenFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      personal_context::features::kPersonalContext);
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();

  EXPECT_NE(nullptr, IOSPersonalContextEligibilityServiceFactory::GetForProfile(
                         profile.get()));
}

// Tests that PersonalContextEligibilityService is not instantiated (returns
// nullptr) when the kPersonalContext feature flag is disabled.
TEST_F(IOSPersonalContextEligibilityServiceFactoryTest,
       CheckServiceNullWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      personal_context::features::kPersonalContext);
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();

  EXPECT_EQ(nullptr, IOSPersonalContextEligibilityServiceFactory::GetForProfile(
                         profile.get()));
}

// Tests that PersonalContextEligibilityService is not created for an
// Off-The-Record (Incognito) profile as specified by
// ProfileSelection::kOriginalOnly policy.
TEST_F(IOSPersonalContextEligibilityServiceFactoryTest,
       CheckServiceNullForIncognitoProfile) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      personal_context::features::kPersonalContext);
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();
  ProfileIOS* otr_profile = profile->GetOffTheRecordProfile();

  EXPECT_EQ(nullptr, IOSPersonalContextEligibilityServiceFactory::GetForProfile(
                         otr_profile));
}

}  // namespace
