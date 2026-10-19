// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_session_metrics_manager.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"
#import "testing/platform_test.h"

using ios::provider::GeminiViewState;

class GeminiSessionMetricsManagerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    metrics_manager_ = [[GeminiSessionMetricsManager alloc] init];
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
  GeminiSessionMetricsManager* metrics_manager_;
};

// Tests that `geminiWillStart` and `geminiStarted:` record startup latency for
// first run and subsequent sessions.
TEST_F(GeminiSessionMetricsManagerTest, TestGeminiStartupTime) {
  [metrics_manager_ geminiWillStart];
  task_environment_.FastForwardBy(base::Milliseconds(250));
  [metrics_manager_ geminiStarted:YES];

  histogram_tester_.ExpectUniqueTimeSample(kStartupTimeWithFirstRunHistogram,
                                           base::Milliseconds(250), 1);
  histogram_tester_.ExpectTotalCount(kStartupTimeNoFirstRunHistogram, 0);

  // Calling `geminiStarted:` without a new `geminiWillStart` is a no-op.
  [metrics_manager_ geminiStarted:NO];
  histogram_tester_.ExpectTotalCount(kStartupTimeNoFirstRunHistogram, 0);

  [metrics_manager_ geminiWillStart];
  task_environment_.FastForwardBy(base::Milliseconds(120));
  [metrics_manager_ geminiStarted:NO];

  histogram_tester_.ExpectUniqueTimeSample(kStartupTimeNoFirstRunHistogram,
                                           base::Milliseconds(120), 1);
}

// Tests that `sessionDismissedWithViewState:` records the dismissed view state
// and resets the session tab switch count.
TEST_F(GeminiSessionMetricsManagerTest, TestSessionDismissedWithViewState) {
  [metrics_manager_ activeTabSwitched];
  [metrics_manager_ activeTabSwitched];
  EXPECT_EQ(
      2, user_action_tester_.GetActionCount("MobileGeminiFloatyTabSwitched"));

  [metrics_manager_ sessionDismissedWithViewState:GeminiViewState::kCollapsed];
  histogram_tester_.ExpectUniqueSample(
      kFloatyDismissedStateHistogram,
      static_cast<int>(GeminiViewState::kCollapsed), 1);
  histogram_tester_.ExpectUniqueSample(kSessionTabSwitchCountHistogram, 2, 1);

  // Subsequent dismissal should have a reset tab switch count of 0.
  [metrics_manager_ sessionDismissedWithViewState:GeminiViewState::kExpanded];
  histogram_tester_.ExpectBucketCount(
      kFloatyDismissedStateHistogram,
      static_cast<int>(GeminiViewState::kExpanded), 1);
  histogram_tester_.ExpectBucketCount(kSessionTabSwitchCountHistogram, 0, 1);
}

// Tests that `sessionDismissed` records and resets the session tab switch
// count.
TEST_F(GeminiSessionMetricsManagerTest, TestSessionDismissed) {
  [metrics_manager_ activeTabSwitched];
  [metrics_manager_ sessionDismissed];
  histogram_tester_.ExpectBucketCount(kSessionTabSwitchCountHistogram, 1, 1);

  [metrics_manager_ sessionDismissed];
  histogram_tester_.ExpectBucketCount(kSessionTabSwitchCountHistogram, 0, 1);
}
