// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_service_factory.h"

#import "base/test/scoped_feature_list.h"
#import "components/safe_browsing/core/common/features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

class ClientSideDetectionServiceFactoryTest : public PlatformTest {
 protected:
  ClientSideDetectionServiceFactoryTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<ProfileIOS> profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ClientSideDetectionServiceFactoryTest, FeatureEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      safe_browsing::kClientSideDetectionEnabledIos);

  // There should be a non-null instance for a regular profile.
  EXPECT_THAT(ClientSideDetectionServiceFactory::GetForProfile(profile_.get()),
              testing::NotNull());
}

TEST_F(ClientSideDetectionServiceFactoryTest, FeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      safe_browsing::kClientSideDetectionEnabledIos);

  // The factory should return null when the feature is disabled.
  EXPECT_THAT(ClientSideDetectionServiceFactory::GetForProfile(profile_.get()),
              testing::IsNull());
}

TEST_F(ClientSideDetectionServiceFactoryTest, OffTheRecordReturnsNull) {
  scoped_feature_list_.InitAndEnableFeature(
      safe_browsing::kClientSideDetectionEnabledIos);

  // The factory should return null for an off-the-record profile.
  EXPECT_THAT(ClientSideDetectionServiceFactory::GetForProfile(
                  profile_->GetOffTheRecordProfile()),
              testing::IsNull());
}
