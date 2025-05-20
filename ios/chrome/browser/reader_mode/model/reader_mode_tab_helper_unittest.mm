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
#import "ios/chrome/browser/dom_distiller/model/distiller_service_factory.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_java_script_feature.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
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
      : web_state_(std::make_unique<web::FakeWebState>()),
        test_url_(GURL("https://test.url/")) {}

  void SetUp() override {
    PlatformTest::SetUp();

    profile_ = TestProfileIOS::Builder().Build();

    web_state_->SetVisibleURL(test_url_);
    web_state_->SetBrowserState(profile_.get());

    // Set up the fake web frames manager.
    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web_frames_manager_ = frames_manager.get();
    web_state_->SetWebFramesManager(
        std::make_unique<web::FakeWebFramesManager>());
    web_state_->SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                    std::move(frames_manager));

    // Set up the fake web frame to return a custom result after executing
    // the heuristic Javascript callback.
    auto main_frame = web::FakeWebFrame::CreateMainWebFrame(test_url_);
    main_frame->set_browser_state(profile_.get());
    web_frame_ = main_frame.get();
    web_frames_manager_->AddWebFrame(std::move(main_frame));
    web_frame()->set_call_java_script_function_callback(base::BindRepeating(^{
      reader_mode_tab_helper()->HandleReaderModeHeuristicResult(
          test_url_, ReaderModeHeuristicResult::kReaderModeEligible);
    }));

    // Set up the fake web frame to return a custom result after executing
    // the DOM distiller Javascript.
    dom_distiller::proto::DomDistillerOptions options;
    std::u16string script = base::UTF8ToUTF16(
        dom_distiller::GetDistillerScriptWithOptions(options));
    web_frame()->AddResultForExecutedJs(&empty_distiller_result_, script);

    // Configure the web state resources.
    web::JavaScriptFeatureManager::FromBrowserState(profile_.get())
        ->ConfigureFeatures({ReaderModeJavaScriptFeature::GetInstance()});
    CreateTabHelperForWebState(web_state_.get());
    ukm::InitializeSourceUrlRecorderForWebState(web_state());
  }

  void TearDown() override { test_ukm_recorder_.Purge(); }

  void CreateTabHelperForWebState(web::WebState* web_state) {
    ReaderModeTabHelper::CreateForWebState(
        web_state, DistillerServiceFactory::GetForProfile(profile_.get()));
  }

  ReaderModeTabHelper* reader_mode_tab_helper() {
    return ReaderModeTabHelper::FromWebState(web_state());
  }

  void LoadWebpage() {
    web::FakeNavigationContext navigation_context;
    navigation_context.SetHasCommitted(true);
    web_state()->OnNavigationStarted(&navigation_context);
    web_state()->LoadSimulatedRequest(test_url_,
                                      @"<html><body>Content</body></html>");
    web_state()->OnNavigationFinished(&navigation_context);
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
  web::FakeWebState* web_state() { return web_state_.get(); }
  web::FakeWebFrame* web_frame() { return web_frame_.get(); }

  raw_ptr<web::FakeWebFramesManager> web_frames_manager_;
  raw_ptr<web::FakeWebFrame> web_frame_;

  std::unique_ptr<web::FakeWebState> web_state_;
  GURL test_url_;

  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestProfileIOS> profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
  base::Value empty_distiller_result_;
};

// Tests that a misconfigured page load probability does not trigger a
// heuristic.
TEST_F(ReaderModeTabHelperTest, TriggerHeuristicMisconfiguredProbabilityLow) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kEnableReaderModeDistillerHeuristicForMetrics,
      {
          {kReaderModeDistillerPageLoadProbabilityName, "0.0"},
          {kReaderModeDistillerPageLoadDelayDurationStringName, "0"},
      });

  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  ExpectHeuristicResultEntriesCount(0u);
  LoadWebpage();
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  ExpectHeuristicResultEntriesCount(0u);
}

// Tests that a misconfigured page load probability does not trigger a
// heuristic.
TEST_F(ReaderModeTabHelperTest, TriggerHeuristicMisconfiguredProbabilityHigh) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kEnableReaderModeDistillerHeuristicForMetrics,
      {
          {kReaderModeDistillerPageLoadProbabilityName, "1.1"},
          {kReaderModeDistillerPageLoadDelayDurationStringName, "0"},
      });

  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  ExpectHeuristicResultEntriesCount(0u);
  LoadWebpage();
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  ExpectHeuristicResultEntriesCount(0u);
}

// Tests that multiple navigations before the trigger heuristic delay only
// records metrics from the latest navigation.
TEST_F(ReaderModeTabHelperTest, TriggerHeuristicSkippedOnNewNavigation) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kEnableReaderModeDistillerHeuristicForMetrics,
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

  task_environment_.RunUntilIdle();
  ExpectHeuristicResultEntriesCount(1u);
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 1);
}

// Tests that histograms related to heuristic results are recorded after the
// JavaScript execution.
TEST_F(ReaderModeTabHelperTest, TriggerHeuristicOnPageLoaded) {
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{kEnableReaderModeDistillerHeuristicForMetrics,
        {
            {kReaderModeDistillerPageLoadProbabilityName, "1.0"},
            {kReaderModeDistillerPageLoadDelayDurationStringName, "0"},
        }},
       {kEnableReaderModeDistillerForMetrics, {}}},
      /*disabled_features=*/{});

  LoadWebpage();
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 1);
  ExpectHeuristicResultEntriesCount(1u);
}

// Tests that histograms related to the distillation are recorded when the
// Reader Mode becomes active.
TEST_F(ReaderModeTabHelperTest, TriggerDistillationOnActive) {
  scoped_feature_list_.InitAndEnableFeature(kEnableReaderMode);

  LoadWebpage();
  task_environment_.RunUntilIdle();

  reader_mode_tab_helper()->SetActive(true);
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectTotalCount(kReaderModeDistillerLatencyHistogram, 1);
  histogram_tester_.ExpectTotalCount(kReaderModeAmpClassificationHistogram, 1);
  ExpectDistillerLatencyEntriesCount(1u);
  ExpectDistillerResultEntriesCount(1u);
}

// Tests that distillation heuristic is canceled after a web state is destroyed.
TEST_F(ReaderModeTabHelperTest, WebStateDestructionCancelsHeuristic) {
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{kEnableReaderModeDistillerHeuristicForMetrics,
        {
            {kReaderModeDistillerPageLoadProbabilityName, "1.0"},
            {kReaderModeDistillerPageLoadDelayDurationStringName, "10s"},
        }},
       {kEnableReaderModeDistillerForMetrics, {}}},
      /*disabled_features=*/{});

  LoadWebpage();
  task_environment_.RunUntilIdle();

  // Destroy the web state.
  web_state_.reset();
  task_environment_.AdvanceClock(base::Seconds(20));
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
}

// TODO(crbug.com/399378832): Add tests for individual heuristic values that
// are not dependent on the model implementation.
