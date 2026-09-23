// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_intelligent_scan_delegate_factory.h"

#import <memory>
#import <utility>

#import "base/test/scoped_feature_list.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/safe_browsing/core/browser/intelligent_scan_delegate.h"
#import "components/safe_browsing/core/common/features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

class ClientSideDetectionIntelligentScanDelegateFactoryTest
    : public PlatformTest {
 protected:
  ClientSideDetectionIntelligentScanDelegateFactoryTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<ProfileIOS> profile_;
};

// Tests that the factory creates an `IntelligentScanDelegate` when CSD is
// enabled.
TEST_F(ClientSideDetectionIntelligentScanDelegateFactoryTest, FeatureEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      safe_browsing::kClientSideDetectionEnabledIos);

  EXPECT_THAT(ClientSideDetectionIntelligentScanDelegateFactory::GetForProfile(
                  profile_.get()),
              testing::NotNull());
}

// Tests that the factory returns `nullptr` when CSD is disabled.
TEST_F(ClientSideDetectionIntelligentScanDelegateFactoryTest, FeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      safe_browsing::kClientSideDetectionEnabledIos);

  EXPECT_THAT(ClientSideDetectionIntelligentScanDelegateFactory::GetForProfile(
                  profile_.get()),
              testing::IsNull());
}

// Tests that the factory returns `nullptr` when `OptimizationGuideService` is
// unavailable.
TEST_F(ClientSideDetectionIntelligentScanDelegateFactoryTest,
       NullOptimizationGuideReturnsNull) {
  // Disabling `kOptimizationHints` prevents an `OptimizationGuideService`
  // instance from being created when the profile is built.
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{safe_browsing::kClientSideDetectionEnabledIos},
      /*disabled_features=*/{optimization_guide::features::kOptimizationHints});

  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(
      OptimizationGuideServiceFactory::GetInstance(),
      OptimizationGuideServiceFactory::GetDefaultFactory());
  std::unique_ptr<TestProfileIOS> profile_without_opt_guide =
      std::move(builder).Build();
  ASSERT_THAT(OptimizationGuideServiceFactory::GetForProfile(
                  profile_without_opt_guide.get()),
              testing::IsNull());

  EXPECT_THAT(ClientSideDetectionIntelligentScanDelegateFactory::GetForProfile(
                  profile_without_opt_guide.get()),
              testing::IsNull());
}

// Tests that the factory returns `nullptr` for off-the-record profiles.
TEST_F(ClientSideDetectionIntelligentScanDelegateFactoryTest,
       OffTheRecordReturnsNull) {
  scoped_feature_list_.InitAndEnableFeature(
      safe_browsing::kClientSideDetectionEnabledIos);

  EXPECT_THAT(ClientSideDetectionIntelligentScanDelegateFactory::GetForProfile(
                  profile_->GetOffTheRecordProfile()),
              testing::IsNull());
}

// Tests that the factory returns the same service instance across calls.
TEST_F(ClientSideDetectionIntelligentScanDelegateFactoryTest,
       ReturnsSameInstance) {
  scoped_feature_list_.InitAndEnableFeature(
      safe_browsing::kClientSideDetectionEnabledIos);

  safe_browsing::IntelligentScanDelegate* first_instance =
      ClientSideDetectionIntelligentScanDelegateFactory::GetForProfile(
          profile_.get());
  ASSERT_THAT(first_instance, testing::NotNull());

  EXPECT_EQ(first_instance,
            ClientSideDetectionIntelligentScanDelegateFactory::GetForProfile(
                profile_.get()));
}
