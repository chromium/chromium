// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_recorder.h"

#import <Foundation/Foundation.h>

#import "base/test/metrics/histogram_tester.h"
#import "components/feed/core/v2/public/common_enums.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_recorder+testing.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using feed::FeedEngagementType;

class FeedMetricsRecorderTest : public PlatformTest {
 public:
  FeedMetricsRecorderTest() {
    recorder = [[FeedMetricsRecorder alloc] init];
    histogram_tester_.reset(new base::HistogramTester());
  }

 protected:
  void TearDown() override {
    [recorder resetGoodVisitSession];
    PlatformTest::TearDown();
  }
  FeedMetricsRecorder* recorder;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Tests that a Good Visit is recorded when a url is added to Read Later.
TEST_F(FeedMetricsRecorderTest, GoodExplicitInteraction) {
  // There should not be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // Add URL to Read Later constitutes a Good Visit by itself.
  [recorder recordAddURLToReadLater];
  // There should be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that a Good Visit is recorded when we open a url in a new incognito
// tab.
TEST_F(FeedMetricsRecorderTest, GoodVisit_OpenInNewIncognitoTab) {
  // There should not be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // After Action, Good Visit should be recorded.
  [recorder recordOpenURLInIncognitoTab];
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that a Good Visit is recorded when we do a long press on a card.
TEST_F(FeedMetricsRecorderTest, GoodVisit_LongPress) {
  // There should not be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // After Action, Good Visit should be recorded.
  [recorder recordNativeContextMenuVisibilityChanged:YES];
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that a Good Visit is only logged once for each Good Visit session.
TEST_F(FeedMetricsRecorderTest, GoodVisit_OnlyLoggedOncePerVisit) {
  // There should not be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // Start with a Good Visit.
  [recorder recordAddURLToReadLater];
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
  // Adding to Read Later should count as a Good Visit, but we only log one Good
  // Visit per session, so the histogram count should remain at 1.
  [recorder recordAddURLToReadLater];
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that a Good Visit is not logged when a non-Good Visit action is
// triggered.
TEST_F(FeedMetricsRecorderTest, GoodVisit_NonGoodVisitActionTriggered) {
  // There should not be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // Trigger a non-Good Visit action.
  [recorder recordHeaderMenuManageTapped];
  // There should not be a Good Visit recorded as the action was not a trigger
  // for a Good Visit.
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
}

// TODO(crbug.com/1399110): Add Time-Based tests in follow up CL.
