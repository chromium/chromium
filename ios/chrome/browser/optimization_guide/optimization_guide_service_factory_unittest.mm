// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/optimization_guide_service_factory.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "ios/chrome/browser/optimization_guide/optimization_guide_service.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class OptimizationGuideServiceFactoryTest : public PlatformTest {
 public:
  OptimizationGuideServiceFactoryTest() = default;
  ~OptimizationGuideServiceFactoryTest() override = default;

  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitWithFeatures(
        {optimization_guide::features::kOptimizationHints}, {});
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    browser_state_ = builder.Build();
    OptimizationGuideServiceFactory::GetForBrowserState(browser_state_.get())
        ->DoFinalInit(BackgroundDownloadServiceFactory::GetForBrowserState(
            browser_state_.get()));

    ChromeBrowserState* otr_browser_state =
        browser_state_->CreateOffTheRecordBrowserStateWithTestingFactories(
            {std::make_pair(
                OptimizationGuideServiceFactory::GetInstance(),
                OptimizationGuideServiceFactory::GetDefaultFactory())});
    OptimizationGuideServiceFactory::GetForBrowserState(otr_browser_state)
        ->DoFinalInit();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(OptimizationGuideServiceFactoryTest, CheckNormalServiceNotNull) {
  EXPECT_NE(nullptr, OptimizationGuideServiceFactory::GetForBrowserState(
                         browser_state_.get()));
}

TEST_F(OptimizationGuideServiceFactoryTest, CheckIncognitoServiceNotNull) {
  EXPECT_NE(nullptr, OptimizationGuideServiceFactory::GetForBrowserState(
                         browser_state_->GetOffTheRecordChromeBrowserState()));
}

class OptimizationGuideServiceFactoryFeatureDisabledTest : public PlatformTest {
 public:
  OptimizationGuideServiceFactoryFeatureDisabledTest() = default;
  ~OptimizationGuideServiceFactoryFeatureDisabledTest() override = default;

  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitAndDisableFeature(
        {optimization_guide::features::kOptimizationHints});
    TestChromeBrowserState::Builder builder;
    browser_state_ = builder.Build();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ChromeBrowserState> browser_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(OptimizationGuideServiceFactoryFeatureDisabledTest,
       CheckServiceNullWithoutOptimizationGuideHintsFeature) {
  EXPECT_EQ(nullptr, OptimizationGuideServiceFactory::GetForBrowserState(
                         browser_state_.get()));
}

}  // namespace
