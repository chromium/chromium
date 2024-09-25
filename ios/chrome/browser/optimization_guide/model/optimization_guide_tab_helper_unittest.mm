// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/optimization_guide_tab_helper.h"

#import "base/command_line.h"
#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/optimization_guide/core/optimization_guide_navigation_data.h"
#import "components/optimization_guide/core/optimization_guide_switches.h"
#import "components/optimization_guide/core/optimization_guide_test_util.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_test_utils.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using testing::ElementsAre;

namespace {

constexpr int64_t kNavigationID = 1;
constexpr char kHintsURL[] = "https://hints.com/with_hints.html";
constexpr char kNoHintsURL[] = "https://nohints.com/no_hints.html";

class IOSOptimizationGuideNavigationDataTest : public PlatformTest {
 public:
  IOSOptimizationGuideNavigationDataTest()
      : test_navigation_data_(kNavigationID) {}

 protected:
  IOSOptimizationGuideNavigationData test_navigation_data_;
};

TEST_F(IOSOptimizationGuideNavigationDataTest, CheckNavigationId) {
  EXPECT_EQ(kNavigationID, test_navigation_data_.navigation_id());
}

TEST_F(IOSOptimizationGuideNavigationDataTest, CheckNavigationURL) {
  GURL kFooURL("https://foo.com");
  test_navigation_data_.NotifyNavigationStart(kFooURL);
  EXPECT_EQ(kFooURL, test_navigation_data_.navigation_url());
  EXPECT_THAT(test_navigation_data_.redirect_chain(), ElementsAre(kFooURL));
}

TEST_F(IOSOptimizationGuideNavigationDataTest, CheckNavigationRedirect) {
  GURL kFooURL("https://foo.com");
  GURL kRedirectBarURL("https://bar.com");
  GURL kRedirectBazURL("https://baz.com");

  test_navigation_data_.NotifyNavigationStart(kFooURL);
  EXPECT_EQ(kFooURL, test_navigation_data_.navigation_url());
  EXPECT_THAT(test_navigation_data_.redirect_chain(), ElementsAre(kFooURL));

  test_navigation_data_.NotifyNavigationRedirect(kRedirectBarURL);
  EXPECT_EQ(kRedirectBarURL, test_navigation_data_.navigation_url());
  EXPECT_THAT(test_navigation_data_.redirect_chain(),
              ElementsAre(kFooURL, kRedirectBarURL));

  test_navigation_data_.NotifyNavigationRedirect(kRedirectBazURL);
  EXPECT_EQ(kRedirectBazURL, test_navigation_data_.navigation_url());
  EXPECT_THAT(test_navigation_data_.redirect_chain(),
              ElementsAre(kFooURL, kRedirectBarURL, kRedirectBazURL));
}

TEST_F(IOSOptimizationGuideNavigationDataTest,
       CheckNavigationStartCancelsRedirect) {
  GURL kFooURL("https://foo.com");
  GURL kBarURL("https://bar.com");
  GURL kBazURL("https://baz.com");

  test_navigation_data_.NotifyNavigationStart(kFooURL);
  test_navigation_data_.NotifyNavigationRedirect(kBarURL);
  EXPECT_EQ(kBarURL, test_navigation_data_.navigation_url());
  EXPECT_THAT(test_navigation_data_.redirect_chain(),
              ElementsAre(kFooURL, kBarURL));

  test_navigation_data_.NotifyNavigationStart(kBazURL);
  EXPECT_EQ(kBazURL, test_navigation_data_.navigation_url());
  EXPECT_THAT(test_navigation_data_.redirect_chain(), ElementsAre(kBazURL));
}

class OptimizationGuideTabHelperTest : public PlatformTest {
 public:
  OptimizationGuideTabHelperTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        optimization_guide::switches::kPurgeHintsStore);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        optimization_guide::switches::kHintsProtoOverride,
        optimization_guide::CreateHintsConfig(
            GURL(kHintsURL), optimization_guide::proto::NOSCRIPT,
            /*metadata=*/nullptr));
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {optimization_guide::features::kOptimizationHints}, {});

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    optimization_guide_service_ =
        OptimizationGuideServiceFactory::GetForProfile(profile_.get());
    optimization_guide_service_->DoFinalInit();

    web_state_.SetBrowserState(profile_.get());
    optimization_guide_service_ =
        OptimizationGuideServiceFactory::GetForProfile(profile_.get());

    OptimizationGuideTabHelper::CreateForWebState(&web_state_);

    // Wait for the hints override from CLI is picked up.
    RetryForHistogramUntilCountReached(
        &histogram_tester_, "OptimizationGuide.UpdateComponentHints.Result", 1);
  }

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<OptimizationGuideService> optimization_guide_service_;
  web::FakeWebState web_state_;
};

TEST_F(OptimizationGuideTabHelperTest, NavigationToURLWithHints) {
  optimization_guide_service_->RegisterOptimizationTypes(
      {optimization_guide::proto::NOSCRIPT});

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kHintsURL));
  web_state_.OnNavigationStarted(&context);
  web_state_.OnNavigationFinished(&context);
  RunUntilIdle();

  auto decision = optimization_guide_service_->CanApplyOptimization(
      GURL(kHintsURL), optimization_guide::proto::NOSCRIPT,
      /*optimization_metadata=*/nullptr);

  EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kTrue, decision);
}

TEST_F(OptimizationGuideTabHelperTest, NavigationToURLWithNoHints) {
  optimization_guide_service_->RegisterOptimizationTypes(
      {optimization_guide::proto::NOSCRIPT});

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kNoHintsURL));
  web_state_.OnNavigationStarted(&context);
  web_state_.OnNavigationFinished(&context);
  RunUntilIdle();

  auto decision = optimization_guide_service_->CanApplyOptimization(
      GURL(kNoHintsURL), optimization_guide::proto::NOSCRIPT,
      /*optimization_metadata=*/nullptr);

  EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kFalse, decision);
}

}  // namespace
