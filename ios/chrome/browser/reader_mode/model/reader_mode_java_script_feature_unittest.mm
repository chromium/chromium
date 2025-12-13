// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_java_script_feature.h"

#import "base/test/metrics/histogram_tester.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "components/ukm/test_ukm_recorder.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service_factory.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/snapshots/model/snapshot_source_tab_helper.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "testing/platform_test.h"

using IOS_ReaderMode_Heuristic_Result =
    ukm::builders::IOS_ReaderMode_Heuristic_Result;

class ReaderModeJavaScriptFeatureTest : public PlatformTest {
 public:
  ReaderModeJavaScriptFeatureTest() : valid_url_(GURL("https://example.com")) {
    profile_ = TestProfileIOS::Builder().Build();

    ReaderModeTabHelper::CreateForWebState(
        web_state(), DistillerServiceFactory::GetForProfile(profile_.get()));
    SnapshotSourceTabHelper::CreateForWebState(web_state());
    ukm::InitializeSourceUrlRecorderForWebState(web_state());
  }

  void TearDown() override { test_ukm_recorder_.Purge(); }

  web::ScriptMessage ScriptMessageForUrl(const GURL& url) {
    return web::ScriptMessage(ValidDerivedFeatures(),
                              /*is_user_interacting=*/true,
                              /*is_main_frame=*/true,
                              /*request_url=*/url);
  }

  web::ScriptMessage ScriptMessageForInvalidUrl() {
    return web::ScriptMessage(ValidDerivedFeatures(),
                              /*is_user_interacting=*/true,
                              /*is_main_frame=*/true,
                              /*request_url=*/std::nullopt);
  }

  web::FakeWebState* web_state() { return &web_state_; }

  const GURL& valid_url() { return valid_url_; }

  // Starts and finishes a committed navigation in `web_state()`.
  void CommitNavigation() {
    web::FakeNavigationContext navigation_context;
    navigation_context.SetHasCommitted(true);
    web_state()->OnNavigationStarted(&navigation_context);
    web_state()->OnNavigationFinished(&navigation_context);
  }

  // Expects the recorded heuristic result UKM event entries to have
  // `expected_count` elements.
  void ExpectHeuristicResultEntriesCount(size_t expected_count) {
    EXPECT_EQ(expected_count,
              test_ukm_recorder_
                  .GetEntriesByName(IOS_ReaderMode_Heuristic_Result::kEntryName)
                  .size());
  }

  // Asserts that a unique heuristic result event was recorded with the result
  // value equal to `result`.
  void AssertHeuristicResultUniqueUKM(ReaderModeHeuristicResult result) {
    const auto entries = test_ukm_recorder_.GetEntriesByName(
        IOS_ReaderMode_Heuristic_Result::kEntryName);
    ASSERT_EQ(1u, entries.size());
    const ukm::mojom::UkmEntry* entry = entries[0];
    ukm::TestUkmRecorder::ExpectEntryMetric(
        entry, IOS_ReaderMode_Heuristic_Result::kResultName,
        static_cast<int64_t>(result));
  }

 protected:
  std::unique_ptr<base::Value> ValidDerivedFeatures() {
    return std::make_unique<base::Value>(base::Value::Dict()
                                             .Set("time", 10.0)
                                             .Set("numElements", 0.0)
                                             .Set("numAnchors", 0.0)
                                             .Set("numForms", 0.0)
                                             .Set("mozScore", 0.0)
                                             .Set("mozScoreAllSqrt", 0.0)
                                             .Set("mozScoreAllLinear", 0.0));
  }

  web::WebTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  GURL valid_url_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
  std::unique_ptr<TestProfileIOS> profile_;
  // Ensure that `web_state_` is declared after `profile_` so that the correct
  // shutdown order is enforced.
  web::FakeWebState web_state_;
};

// Tests that an empty url is not eligible for Reader Mode heuristics.
TEST_F(ReaderModeJavaScriptFeatureTest, EmptyUrlNotEligible) {
  ReaderModeJavaScriptFeature::GetInstance()->ScriptMessageReceived(
      web_state(), ScriptMessageForInvalidUrl());
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  ExpectHeuristicResultEntriesCount(0u);
}

// Tests that a Chrome scheme is not eligible for Reader Mode heuristics.
TEST_F(ReaderModeJavaScriptFeatureTest, ChromeUrlNotEligible) {
  ReaderModeJavaScriptFeature::GetInstance()->ScriptMessageReceived(
      web_state(), ScriptMessageForUrl(GURL("chrome://internals")));
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  ExpectHeuristicResultEntriesCount(0u);
}

// Tests that the about:blank url is not eligible for Reader Mode heuristics.
TEST_F(ReaderModeJavaScriptFeatureTest, AboutUrlNotEligible) {
  ReaderModeJavaScriptFeature::GetInstance()->ScriptMessageReceived(
      web_state(), ScriptMessageForUrl(GURL("about:blank")));
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  ExpectHeuristicResultEntriesCount(0u);
}

// Tests that a JavaScript response with an incorrect data type records
// malformed response.
TEST_F(ReaderModeJavaScriptFeatureTest, MalformedResponseNotDict) {
  CommitNavigation();
  auto invalid_body =
      std::make_unique<base::Value>(base::Value("invalid_because_expect_dict"));
  web::ScriptMessage script_message(std::move(invalid_body),
                                    /*is_user_interacting=*/true,
                                    /*is_main_frame=*/true,
                                    /*request_url=*/valid_url());
  ReaderModeJavaScriptFeature::GetInstance()->ScriptMessageReceived(
      web_state(), script_message);
  // Test heuristic result histogram.
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeHeuristicResultHistogram),
      base::BucketsAre(
          base::Bucket(ReaderModeHeuristicResult::kMalformedResponse, 1)));
  // Test heuristic result UKM.
  AssertHeuristicResultUniqueUKM(ReaderModeHeuristicResult::kMalformedResponse);
}

// Tests that a JavaScript response with missing features records malformed
// response.
TEST_F(ReaderModeJavaScriptFeatureTest, MalformedResponseMissingFeatures) {
  CommitNavigation();
  auto invalid_body =
      std::make_unique<base::Value>(base::Value(base::Value::Dict()));
  web::ScriptMessage script_message(std::move(invalid_body),
                                    /*is_user_interacting=*/true,
                                    /*is_main_frame=*/true,
                                    /*request_url=*/valid_url());
  ReaderModeJavaScriptFeature::GetInstance()->ScriptMessageReceived(
      web_state(), script_message);
  // Test heuristic result histogram.
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeHeuristicResultHistogram),
      base::BucketsAre(
          base::Bucket(ReaderModeHeuristicResult::kMalformedResponse, 1)));
  // Test heuristic result UKM.
  AssertHeuristicResultUniqueUKM(ReaderModeHeuristicResult::kMalformedResponse);
}

// Tests that a set of valid derived features records reader mode eligibility.
TEST_F(ReaderModeJavaScriptFeatureTest, ValidDerivedFeatures) {
  CommitNavigation();
  ReaderModeJavaScriptFeature::GetInstance()->ScriptMessageReceived(
      web_state(), ScriptMessageForUrl(valid_url()));
  // Test heuristic result histogram.
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeHeuristicResultHistogram),
      base::BucketsAre(base::Bucket(
          ReaderModeHeuristicResult::kReaderModeNotEligibleContentAndLength,
          1)));
}
