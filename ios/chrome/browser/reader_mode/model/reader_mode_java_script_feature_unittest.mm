// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_java_script_feature.h"

#import "base/test/metrics/histogram_tester.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class ReaderModeJavaScriptFeatureTest : public PlatformTest {
 public:
  ReaderModeJavaScriptFeatureTest() : valid_url_(GURL("https://example.com")) {}

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

  web::WebState* web_state() { return &web_state_; }

  const GURL& valid_url() { return valid_url_; }

 protected:
  std::unique_ptr<base::Value> ValidDerivedFeatures() {
    return std::make_unique<base::Value>(base::Value::Dict()
                                             .Set("time", 0.0)
                                             .Set("numElements", 0.0)
                                             .Set("numAnchors", 0.0)
                                             .Set("numForms", 0.0)
                                             .Set("mozScore", 0.0)
                                             .Set("mozScoreAllSqrt", 0.0)
                                             .Set("mozScoreAllLinear", 0.0));
  }

  web::WebTaskEnvironment task_environment_;
  web::FakeWebState web_state_;
  base::HistogramTester histogram_tester_;
  GURL valid_url_;
};

// Tests that an empty url is not eligible for Reader Mode heuristics.
TEST_F(ReaderModeJavaScriptFeatureTest, EmptyUrlNotEligible) {
  ReaderModeJavaScriptFeature::GetInstance()->ScriptMessageReceived(
      web_state(), ScriptMessageForInvalidUrl());
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicLatencyHistogram, 0);
}

// Tests that a Chrome scheme is not eligible for Reader Mode heuristics.
TEST_F(ReaderModeJavaScriptFeatureTest, ChromeUrlNotEligible) {
  ReaderModeJavaScriptFeature::GetInstance()->ScriptMessageReceived(
      web_state(), ScriptMessageForUrl(GURL("chrome://internals")));
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicLatencyHistogram, 0);
}

// Tests that the about:blank url is not eligible for Reader Mode heuristics.
TEST_F(ReaderModeJavaScriptFeatureTest, AboutUrlNotEligible) {
  ReaderModeJavaScriptFeature::GetInstance()->ScriptMessageReceived(
      web_state(), ScriptMessageForUrl(GURL("about:blank")));
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicLatencyHistogram, 0);
}

// Tests that a JavaScript response with an incorrect data type records
// malformed response.
TEST_F(ReaderModeJavaScriptFeatureTest, MalformedResponseNotDict) {
  auto invalid_body =
      std::make_unique<base::Value>(base::Value("invalid_because_expect_dict"));
  web::ScriptMessage script_message(std::move(invalid_body),
                                    /*is_user_interacting=*/true,
                                    /*is_main_frame=*/true,
                                    /*request_url=*/valid_url());
  ReaderModeJavaScriptFeature::GetInstance()->ScriptMessageReceived(
      web_state(), script_message);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeHeuristicResultHistogram),
      base::BucketsAre(
          base::Bucket(ReaderModeHeuristicResult::kMalformedResponse, 1)));
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicLatencyHistogram, 0);
}

// Tests that a JavaScript response with missing features records malformed
// response.
TEST_F(ReaderModeJavaScriptFeatureTest, MalformedResponseMissingFeatures) {
  auto invalid_body =
      std::make_unique<base::Value>(base::Value(base::Value::Dict()));
  web::ScriptMessage script_message(std::move(invalid_body),
                                    /*is_user_interacting=*/true,
                                    /*is_main_frame=*/true,
                                    /*request_url=*/valid_url());
  ReaderModeJavaScriptFeature::GetInstance()->ScriptMessageReceived(
      web_state(), script_message);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeHeuristicResultHistogram),
      base::BucketsAre(
          base::Bucket(ReaderModeHeuristicResult::kMalformedResponse, 1)));
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicLatencyHistogram, 0);
}

// Tests that a set of valid derived features records reader mode eligibility.
TEST_F(ReaderModeJavaScriptFeatureTest, ValidDerivedFeatures) {
  ReaderModeJavaScriptFeature::GetInstance()->ScriptMessageReceived(
      web_state(), ScriptMessageForUrl(valid_url()));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kReaderModeHeuristicResultHistogram),
      base::BucketsAre(base::Bucket(
          ReaderModeHeuristicResult::kReaderModeNotEligibleContentAndLength,
          1)));
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicLatencyHistogram, 1);
}
