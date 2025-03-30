// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"

#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/scoped_mock_clock_override.h"
#import "components/dom_distiller/core/extraction_utils.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "components/ukm/test_ukm_recorder.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_java_script_feature.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "testing/platform_test.h"
#import "third_party/dom_distiller_js/dom_distiller.pb.h"

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using IOS_ReaderMode_Distiller_Latency =
    ukm::builders::IOS_ReaderMode_Distiller_Latency;
using IOS_ReaderMode_Distiller_Result =
    ukm::builders::IOS_ReaderMode_Distiller_Result;
using IOS_ReaderMode_Heuristic_Result =
    ukm::builders::IOS_ReaderMode_Heuristic_Result;

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
    ukm::InitializeSourceUrlRecorderForWebState(web_state());
  }

  void TearDown() override { test_ukm_recorder_.Purge(); }

  void WaitForMainFrame() {
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
      web::WebFramesManager* frames_manager =
          web_state()->GetPageWorldWebFramesManager();
      return frames_manager->GetMainWebFrame() != nullptr;
    }));
  }

  ReaderModeTabHelper* reader_mode_tab_helper() {
    return ReaderModeTabHelper::FromWebState(web_state());
  }

  void LoadWebpage() {
    GURL test_url("https://test.url/");
    web::test::LoadHtml(@"<html><body>Content</body></html>", test_url,
                        web_state());
    WaitForMainFrame();
  }

  // Expects the recorded distiller latency UKM event entries to have
  // `expected_count` elements.
  void ExpectDistillerLatencyEntriesCount(size_t expected_count) {
    EXPECT_EQ(
        expected_count,
        test_ukm_recorder_
            .GetEntriesByName(IOS_ReaderMode_Distiller_Latency::kEntryName)
            .size());
  }
  // Expects the recorded distiller result UKM event entries to have
  // `expected_count` elements.
  void ExpectDistillerResultEntriesCount(size_t expected_count) {
    EXPECT_EQ(expected_count,
              test_ukm_recorder_
                  .GetEntriesByName(IOS_ReaderMode_Distiller_Result::kEntryName)
                  .size());
  }
  // Expects the recorded heuristic result UKM event entries to have
  // `expected_count` elements.
  void ExpectHeuristicResultEntriesCount(size_t expected_count) {
    EXPECT_EQ(expected_count,
              test_ukm_recorder_
                  .GetEntriesByName(IOS_ReaderMode_Heuristic_Result::kEntryName)
                  .size());
  }

 protected:
  web::WebState* web_state() { return web_state_.get(); }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestProfileIOS> profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<web::WebState> web_state_;
  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
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
  ExpectHeuristicResultEntriesCount(0u);
  LoadWebpage();

  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 1);
  ExpectHeuristicResultEntriesCount(1u);
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
  ExpectHeuristicResultEntriesCount(0u);
  LoadWebpage();

  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  ExpectHeuristicResultEntriesCount(0u);
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
  ExpectHeuristicResultEntriesCount(0u);
  LoadWebpage();

  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  ExpectHeuristicResultEntriesCount(0u);
}

// Tests that multiple navigations before the trigger heuristic delay only
// records metrics from the latest navigation.
TEST_F(ReaderModeTabHelperTest, TriggerHeuristicSkippedOnNewNavigation) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kEnableReaderModeDistillerHeuristic,
      {
          {kReaderModeDistillerPageLoadProbabilityName, "1.0"},
          {kReaderModeDistillerPageLoadDelayDurationStringName, "10s"},
      });

  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  ExpectHeuristicResultEntriesCount(0u);
  LoadWebpage();
  // Wait for asynchronous activity from page load to stop before advancing the
  // clock.
  task_environment_.RunUntilIdle();
  task_environment_.AdvanceClock(base::Seconds(1));

  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  ExpectHeuristicResultEntriesCount(0u);

  LoadWebpage();
  task_environment_.RunUntilIdle();
  task_environment_.AdvanceClock(base::Seconds(20));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    task_environment_.RunUntilIdle();
    return histogram_tester_.GetAllSamples(kReaderModeHeuristicResultHistogram)
                   .size() == 1 &&
           test_ukm_recorder_
                   .GetEntriesByName(
                       IOS_ReaderMode_Heuristic_Result::kEntryName)
                   .size() == 1u;
  }));
}

// Tests that histograms related to distillation results are recorded after the
// JavaScript execution.
TEST_F(ReaderModeTabHelperTest, TriggerDistillerJs) {
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{kEnableReaderModeDistillerHeuristic,
        {
            {kReaderModeDistillerPageLoadProbabilityName, "1.0"},
            {kReaderModeDistillerPageLoadDelayDurationStringName, "0"},
        }},
       {kEnableReaderModeDistiller, {}}},
      /*disabled_features=*/{});

  auto test_web_state = std::make_unique<web::FakeWebState>();
  auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
  web::FakeWebFramesManager* frames_manager_ptr = frames_manager.get();
  test_web_state->SetWebFramesManager(
      std::make_unique<web::FakeWebFramesManager>());
  test_web_state->SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                      std::move(frames_manager));
  const GURL test_url = GURL("https://test.url/");
  test_web_state->SetVisibleURL(test_url);

  // Use a fake web frame to return a custom result after JS execution.
  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(test_url);
  web::FakeWebFrame* main_frame_ptr = main_frame.get();
  frames_manager_ptr->AddWebFrame(std::move(main_frame));

  ReaderModeTabHelper::CreateForWebState(test_web_state.get());
  ukm::InitializeSourceUrlRecorderForWebState(test_web_state.get());

  // Record committed navigation so the UKM URL recorder works.
  web::FakeNavigationContext navigation_context;
  navigation_context.SetHasCommitted(true);
  test_web_state->OnNavigationStarted(&navigation_context);
  test_web_state->OnNavigationFinished(&navigation_context);

  // Recreate DOM distiller script with empty result on execution.
  dom_distiller::proto::DomDistillerOptions options;
  std::u16string script =
      base::UTF8ToUTF16(dom_distiller::GetDistillerScriptWithOptions(options));
  base::Value empty_value;
  main_frame_ptr->AddResultForExecutedJs(&empty_value, script);

  ReaderModeTabHelper::FromWebState(test_web_state.get())
      ->HandleReaderModeHeuristicResult(
          test_url, ReaderModeHeuristicResult::kReaderModeEligible);
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 1);
  ExpectHeuristicResultEntriesCount(1u);
  histogram_tester_.ExpectTotalCount(
      kReaderModeHeuristicClassificationHistogram, 1);
  histogram_tester_.ExpectTotalCount(kReaderModeDistillerLatencyHistogram, 1);
  histogram_tester_.ExpectTotalCount(kReaderModeAmpClassificationHistogram, 1);
  ExpectDistillerLatencyEntriesCount(1u);
  ExpectDistillerResultEntriesCount(1u);
}

// TODO(crbug.com/399378832): Add tests for individual heuristic values that
// are not dependent on the model implementation.
