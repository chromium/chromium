// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"

#import "base/test/scoped_feature_list.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class OptimizationGuideServiceFactoryTest : public PlatformTest {
 public:
  OptimizationGuideServiceFactoryTest() {
    scoped_feature_list_.InitWithFeatures(
        {optimization_guide::features::kOptimizationHints}, {});
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    profile_->CreateOffTheRecordProfileWithTestingFactories(
        {TestProfileIOS::TestingFactory{
            OptimizationGuideServiceFactory::GetInstance(),
            OptimizationGuideServiceFactory::GetDefaultFactory()}});
  }

 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

TEST_F(OptimizationGuideServiceFactoryTest, CheckNormalServiceNotNull) {
  EXPECT_NE(nullptr,
            OptimizationGuideServiceFactory::GetForProfile(profile_.get()));
}

TEST_F(OptimizationGuideServiceFactoryTest, CheckIncognitoServiceNotNull) {
  EXPECT_NE(nullptr, OptimizationGuideServiceFactory::GetForProfile(
                         profile_->GetOffTheRecordProfile()));
}

class OptimizationGuideServiceFactoryFeatureDisabledTest : public PlatformTest {
 public:
  OptimizationGuideServiceFactoryFeatureDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        {optimization_guide::features::kOptimizationHints});
    profile_ = TestProfileIOS::Builder().Build();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
};

TEST_F(OptimizationGuideServiceFactoryFeatureDisabledTest,
       CheckServiceNullWithoutOptimizationGuideHintsFeature) {
  EXPECT_EQ(nullptr,
            OptimizationGuideServiceFactory::GetForProfile(profile_.get()));
}

}  // namespace
