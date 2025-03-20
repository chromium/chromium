// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"

#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/scoped_mock_clock_override.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_java_script_feature.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

class ReaderModeTabHelperTest : public PlatformTest {
 public:
  ReaderModeTabHelperTest()
      : web_client_(std::make_unique<web::FakeWebClient>()) {}

  void SetUp() override {
    PlatformTest::SetUp();

    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);

    web::FakeWebClient* web_client =
        static_cast<web::FakeWebClient*>(web_client_.Get());
    web_client->SetJavaScriptFeatures(
        {ReaderModeJavaScriptFeature::GetInstance()});

    ReaderModeTabHelper::CreateForWebState(web_state_.get());
  }

  void LoadWebpage() {
    GURL test_url("https://test.url/");
    web::test::LoadHtml(@"<html><body>Content</body></html>", test_url,
                        web_state_.get());
  }

 protected:
  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestProfileIOS> profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<web::WebState> web_state_;
  base::HistogramTester histogram_tester_;
};

// Tests that the TabHelper that triggers Reader Mode heuristics records
// metrics on page load.
TEST_F(ReaderModeTabHelperTest, TriggerHeuristicOnPageLoaded) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kEnableReaderModeDistillerHeuristic,
      {
          {kReaderModeDistillerPageLoadProbabilityName, "1.0"},
          {kReaderModeDistillerPageLoadDelayDurationStringName, "0"},
      });

  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  LoadWebpage();

  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 1);
}

// Tests that a misconfigured page load probability does not trigger a
// heuristic.
TEST_F(ReaderModeTabHelperTest, TriggerHeuristicMisconfiguredProbabilityLow) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kEnableReaderModeDistillerHeuristic,
      {
          {kReaderModeDistillerPageLoadProbabilityName, "0.0"},
          {kReaderModeDistillerPageLoadDelayDurationStringName, "0"},
      });

  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  LoadWebpage();

  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
}

// Tests that a misconfigured page load probability does not trigger a
// heuristic.
TEST_F(ReaderModeTabHelperTest, TriggerHeuristicMisconfiguredProbabilityHigh) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kEnableReaderModeDistillerHeuristic,
      {
          {kReaderModeDistillerPageLoadProbabilityName, "1.1"},
          {kReaderModeDistillerPageLoadDelayDurationStringName, "0"},
      });

  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  LoadWebpage();

  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
}

// Tests that multiple navigations before the trigger heuristic delay only
// records metrics from the latest navigation.
TEST_F(ReaderModeTabHelperTest, TriggerHeuristicSkippedOnNewNavigation) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kEnableReaderModeDistillerHeuristic,
      {
          {kReaderModeDistillerPageLoadProbabilityName, "1.0"},
          {kReaderModeDistillerPageLoadDelayDurationStringName, "1s"},
      });

  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  LoadWebpage();

  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);

  LoadWebpage();
  task_environment_.AdvanceClock(base::Seconds(2));
  task_environment_.RunUntilIdle();

  // TODO(crbug.com/399378832): Update this condition to use a fake delegate
  // once this is exposed in ReaderModeJavaScriptFeature.
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return histogram_tester_.GetAllSamples(kReaderModeHeuristicResultHistogram)
               .size() == 1;
  }));
}

// TODO(crbug.com/399378832): Add tests for individual heuristic values that
// are not dependent on the model implementation.
