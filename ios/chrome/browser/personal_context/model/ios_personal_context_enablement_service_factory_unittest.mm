// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/personal_context/model/ios_personal_context_enablement_service_factory.h"

#import "base/test/scoped_feature_list.h"
#import "components/personal_context/core/personal_context_enablement_service.h"
#import "components/personal_context/core/personal_context_features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Test fixture for IOSPersonalContextEnablementServiceFactory.
class IOSPersonalContextEnablementServiceFactoryTest : public PlatformTest {
 public:
  IOSPersonalContextEnablementServiceFactoryTest() = default;

 protected:
  web::WebTaskEnvironment task_environment_;
};

// Tests that PersonalContextEnablementService is instantiated for a regular
// profile when the kPersonalContext feature flag is enabled.
TEST_F(IOSPersonalContextEnablementServiceFactoryTest,
       CheckServiceNotNullWhenFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      personal_context::features::kPersonalContext);
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();

  EXPECT_NE(nullptr, IOSPersonalContextEnablementServiceFactory::GetForProfile(
                         profile.get()));
}

// Tests that PersonalContextEnablementService is not instantiated (returns
// nullptr) when the kPersonalContext feature flag is disabled.
TEST_F(IOSPersonalContextEnablementServiceFactoryTest,
       CheckServiceNullWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      personal_context::features::kPersonalContext);
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();

  EXPECT_EQ(nullptr, IOSPersonalContextEnablementServiceFactory::GetForProfile(
                         profile.get()));
}

// Tests that PersonalContextEnablementService is not created for an
// Off-The-Record (Incognito) profile as specified by
// ProfileSelection::kOriginalOnly policy.
TEST_F(IOSPersonalContextEnablementServiceFactoryTest,
       CheckServiceNullForIncognitoProfile) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      personal_context::features::kPersonalContext);
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();
  ProfileIOS* otr_profile = profile->GetOffTheRecordProfile();

  EXPECT_EQ(nullptr, IOSPersonalContextEnablementServiceFactory::GetForProfile(
                         otr_profile));
}

}  // namespace
