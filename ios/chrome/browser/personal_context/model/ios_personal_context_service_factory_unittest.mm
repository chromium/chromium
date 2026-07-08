// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/personal_context/model/ios_personal_context_service_factory.h"

#import "base/test/scoped_feature_list.h"
#import "components/personal_context/core/personal_context_features.h"
#import "components/personal_context/core/personal_context_service.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Test fixture for IOSPersonalContextServiceFactory.
class IOSPersonalContextServiceFactoryTest : public PlatformTest {
 public:
  IOSPersonalContextServiceFactoryTest() = default;

 protected:
  web::WebTaskEnvironment task_environment_;
};

// Tests that PersonalContextService is instantiated for a regular profile when
// the kPersonalContext feature flag is enabled.
TEST_F(IOSPersonalContextServiceFactoryTest,
       CheckServiceNotNullWhenFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      personal_context::features::kPersonalContext);
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();

  EXPECT_NE(nullptr,
            IOSPersonalContextServiceFactory::GetForProfile(profile.get()));
}

// Tests that PersonalContextService is not instantiated (returns nullptr) when
// the kPersonalContext feature flag is disabled.
TEST_F(IOSPersonalContextServiceFactoryTest,
       CheckServiceNullWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      personal_context::features::kPersonalContext);
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();

  EXPECT_EQ(nullptr,
            IOSPersonalContextServiceFactory::GetForProfile(profile.get()));
}

// Tests that PersonalContextService is not created for an Off-The-Record
// (Incognito) profile.
TEST_F(IOSPersonalContextServiceFactoryTest,
       CheckServiceNullForIncognitoProfile) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      personal_context::features::kPersonalContext);
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();
  ProfileIOS* otr_profile = profile->GetOffTheRecordProfile();

  EXPECT_EQ(nullptr,
            IOSPersonalContextServiceFactory::GetForProfile(otr_profile));
}

}  // namespace
