// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"

#import <memory>
#import <utility>

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Test fixture for GeminiServiceFactory.
class GeminiServiceFactoryTest : public PlatformTest {
 protected:
  // Creates a test profile with necessary testing factories.
  std::unique_ptr<TestProfileIOS> CreateProfile() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    return std::move(builder).Build();
  }

  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
};

// Tests that the factory successfully creates a service instance when the
// feature is enabled.
TEST_F(GeminiServiceFactoryTest, ServiceCreatedWhenFeatureEnabled) {
  scoped_feature_list_.InitAndEnableFeature(kPageActionMenu);
  auto profile = CreateProfile();
  EXPECT_THAT(GeminiServiceFactory::GetForProfile(profile.get()),
              testing::NotNull());
}

// Tests that the factory returns null when the feature is disabled.
TEST_F(GeminiServiceFactoryTest, ServiceNotCreatedWhenFeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(kPageActionMenu);
  auto profile = CreateProfile();
  EXPECT_THAT(GeminiServiceFactory::GetForProfile(profile.get()),
              testing::IsNull());
}

// Tests that the factory returns null for Off-The-Record profiles.
TEST_F(GeminiServiceFactoryTest, ServiceNotCreatedForOffTheRecordProfile) {
  scoped_feature_list_.InitAndEnableFeature(kPageActionMenu);
  auto profile = CreateProfile();
  ProfileIOS* otr_profile = profile->GetOffTheRecordProfile();
  EXPECT_THAT(GeminiServiceFactory::GetForProfile(otr_profile),
              testing::IsNull());
}

}  // namespace
