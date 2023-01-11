// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_recorder.h"

#import <Foundation/Foundation.h>

#import "base/test/metrics/histogram_tester.h"
#import "components/feed/core/v2/public/common_enums.h"
#import "ios/chrome/browser/ui/ntp/feed_control_delegate.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_recorder+testing.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#import "base/build_time.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_mock_clock_override.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using feed::FeedEngagementType;

class FeedMetricsRecorderTest : public PlatformTest {
 public:
  FeedMetricsRecorderTest() {
    recorder = [[FeedMetricsRecorder alloc] init];
    // Mock Delegate to change currently used feed.
    mockedDelegate = OCMProtocolMock(@protocol(FeedControlDelegate));
    recorder.feedControlDelegate = mockedDelegate;
    histogram_tester_.reset(new base::HistogramTester());
  }

 protected:
  // Constant used to set a standard minimum scroll for testing.
  const int kMinScrollForGoodVisitTests = 50;
  // Used for time based tests.
  const base::TimeDelta kAddedTimeForMockClock = base::Seconds(5);
  void TearDown() override {
    [recorder resetGoodVisitSession];
    PlatformTest::TearDown();
  }
  FeedMetricsRecorder* recorder;
  id<FeedControlDelegate> mockedDelegate;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

#pragma mark - All Feeds Good Visit tests

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

// Tests for switching feeds

// Tests that a Good Visit is recorded when a url is added to Read Later and
// switching between feeds.
TEST_F(FeedMetricsRecorderTest,
       GoodExplicitInteraction_SeparateFeedGoodVisits) {
  // All histograms should be 0
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // Default feed is Discover
  // Add URL to Read Later constitutes a Good Visit for AllFeeds and Discover.
  [recorder recordAddURLToReadLater];
  // There should be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
  // Change feed to Following.
  OCMStub([mockedDelegate selectedFeed]).andReturn(FeedTypeFollowing);

  // Add URL to Read Later constitutes a Good Visit for AllFeeds (not counted as
  // one has been triggered already this session) and Following. The Discover
  // histogram should still have 1 Good Visit reported.
  [recorder recordAddURLToReadLater];
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

#pragma mark - AllFeeds Time Based Tests

TEST_F(FeedMetricsRecorderTest, GoodVisit_GoodTimeInFeed) {
  // Fast forward the clock to build time.
  base::ScopedMockClockOverride mock_clock;

  [recorder recordNTPDidChangeVisibility:YES];
  [recorder recordFeedScrolled:kMinScrollForGoodVisitTests];

  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  mock_clock.Advance(base::Seconds(kGoodVisitTimeInFeedSeconds) +
                     kAddedTimeForMockClock);
  // Calling an arbitrary GV action. This action should not trigger a GV by
  // itself, but cycles the checks for other GV paths.
  [recorder recordFeedScrolled:kMinScrollForGoodVisitTests];
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that a Short Click Visit is recorded appropriately.
TEST_F(FeedMetricsRecorderTest, GoodVisit_ShortClickVisit) {
  // Fast forward the clock to build time.
  base::ScopedMockClockOverride mock_clock;
  // Trigger article click
  [recorder recordOpenURLInSameTab];
  [recorder recordNTPDidChangeVisibility:NO];
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  mock_clock.Advance(
      (base::Seconds(kNonShortClickSeconds) + kAddedTimeForMockClock));
  // Coming back to the main feed. There should be a Good Visit.
  [recorder recordNTPDidChangeVisibility:YES];
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that the session expires accordingly.
TEST_F(FeedMetricsRecorderTest, GoodVisit_SessionExpiration) {
  // Fast forward the clock to build time.
  base::ScopedMockClockOverride mock_clock;
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // Trigger Good Visit
  [recorder recordAddURLToReadLater];
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
  [recorder recordAddURLToReadLater];
  // Check it's not double logged
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
  [recorder recordNTPDidChangeVisibility:NO];

  // Trigger session expiration by waiting `kMinutesBetweenSessions`
  mock_clock.Advance(
      (base::Minutes(kMinutesBetweenSessions) + kAddedTimeForMockClock));
  // Coming back to the main feed. Session should have been reset so there
  // should be 2 histograms.
  [recorder recordNTPDidChangeVisibility:YES];
  [recorder recordAddURLToReadLater];
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 2);
}

#pragma mark - Discover Feed Good Visit tests

// Tests that a Good Visit is recorded when a url is added to Read Later.
TEST_F(FeedMetricsRecorderTest, GoodExplicitInteraction_Discover) {
  // Default feed should be Discover.
  // There should not be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // Add URL to Read Later constitutes a Good Visit by itself.
  [recorder recordAddURLToReadLater];
  // There should be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that a Good Visit is recorded when we open a url in a new incognito
// tab.
TEST_F(FeedMetricsRecorderTest, GoodVisit_OpenInNewIncognitoTab_Discover) {
  // Default feed should be Discover.
  // There should not be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // After Action, Good Visit should be recorded.
  [recorder recordOpenURLInIncognitoTab];
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that a Good Visit is recorded when we do a long press on a card.
TEST_F(FeedMetricsRecorderTest, GoodVisit_LongPress_Discover) {
  // Default feed should be Discover.
  // There should not be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // After Action, Good Visit should be recorded.
  [recorder recordNativeContextMenuVisibilityChanged:YES];
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that a Good Visit is only logged once for each Good Visit session.
TEST_F(FeedMetricsRecorderTest, GoodVisit_OnlyLoggedOncePerVisit_Discover) {
  // Default feed should be Discover.
  // There should not be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // Start with a Good Visit.
  [recorder recordAddURLToReadLater];
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
  // Adding to Read Later should count as a Good Visit, but we only log one Good
  // Visit per session, so the histogram count should remain at 1.
  [recorder recordAddURLToReadLater];
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that a Good Visit is not logged when a non-Good Visit action is
// triggered.
TEST_F(FeedMetricsRecorderTest,
       GoodVisit_NonGoodVisitActionTriggered_Discover) {
  // Default feed should be Discover.
  // There should not be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // Trigger a non-Good Visit action.
  [recorder recordHeaderMenuManageTapped];
  // There should not be a Good Visit recorded as the action was not a trigger
  // for a Good Visit.
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
}

#pragma mark - Discover Feed Time Based Good Visit tests

TEST_F(FeedMetricsRecorderTest, GoodVisit_GoodTimeInFeedDiscover) {
  // Fast forward the clock to build time.
  base::ScopedMockClockOverride mock_clock;

  [recorder recordNTPDidChangeVisibility:YES];
  [recorder recordFeedScrolled:kMinScrollForGoodVisitTests];
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  mock_clock.Advance(base::Seconds(kGoodVisitTimeInFeedSeconds) +
                     kAddedTimeForMockClock);
  [recorder recordFeedScrolled:kMinScrollForGoodVisitTests];
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that a Short Click Visit is recorded appropriately.
TEST_F(FeedMetricsRecorderTest, GoodVisit_ShortClickVisitDiscover) {
  // Fast forward the clock to build time.
  base::ScopedMockClockOverride mock_clock;
  // Trigger article click
  [recorder recordOpenURLInSameTab];
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  mock_clock.Advance(
      (base::Seconds(kNonShortClickSeconds) + kAddedTimeForMockClock));
  // Coming back to the main feed. There should be a Good Visit.
  [recorder recordNTPDidChangeVisibility:YES];
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

#pragma mark - Following Feed Good Visit tests

// Tests that a Good Visit is recorded when a url is added to Read Later.
TEST_F(FeedMetricsRecorderTest, GoodExplicitInteraction_Following) {
  // Change feed to Following.
  OCMStub([mockedDelegate selectedFeed]).andReturn(FeedTypeFollowing);
  // There should not be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // Add URL to Read Later constitutes a Good Visit by itself.
  [recorder recordAddURLToReadLater];
  // There should be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that a Good Visit is recorded when we open a url in a new incognito
// tab.
TEST_F(FeedMetricsRecorderTest, GoodVisit_OpenInNewIncognitoTab_Following) {
  // Change feed to Following.
  OCMStub([mockedDelegate selectedFeed]).andReturn(FeedTypeFollowing);
  // There should not be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // After Action, Good Visit should be recorded.
  [recorder recordOpenURLInIncognitoTab];
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that a Good Visit is recorded when we do a long press on a card.
TEST_F(FeedMetricsRecorderTest, GoodVisit_LongPress_Following) {
  // Change feed to Following.
  OCMStub([mockedDelegate selectedFeed]).andReturn(FeedTypeFollowing);
  // There should not be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // After Action, Good Visit should be recorded.
  [recorder recordNativeContextMenuVisibilityChanged:YES];
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that a Good Visit is only logged once for each Good Visit session.
TEST_F(FeedMetricsRecorderTest, GoodVisit_OnlyLoggedOncePerVisit_Following) {
  // Change feed to Following.
  OCMStub([mockedDelegate selectedFeed]).andReturn(FeedTypeFollowing);
  // There should not be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // Start with a Good Visit.
  [recorder recordAddURLToReadLater];
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
  // Adding to Read Later should count as a Good Visit, but we only log one Good
  // Visit per session, so the histogram count should remain at 1.
  [recorder recordAddURLToReadLater];
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that a Good Visit is not logged when a non-Good Visit action is
// triggered.
TEST_F(FeedMetricsRecorderTest,
       GoodVisit_NonGoodVisitActionTriggered_Following) {
  // Change feed to Following.
  OCMStub([mockedDelegate selectedFeed]).andReturn(FeedTypeFollowing);
  // There should not be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // Trigger a non-Good Visit action.
  [recorder recordHeaderMenuManageTapped];
  // There should not be a Good Visit recorded as the action was not a trigger
  // for a Good Visit.
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
}

#pragma mark - Following Feed Time Based Good Visit tests

TEST_F(FeedMetricsRecorderTest, GoodVisit_GoodTimeInFeedFollowing) {
  // Fast forward the clock to build time.
  base::ScopedMockClockOverride mock_clock;
  // Change feed to Following.
  OCMStub([mockedDelegate selectedFeed]).andReturn(FeedTypeFollowing);

  [recorder recordNTPDidChangeVisibility:YES];
  [recorder recordFeedScrolled:kMinScrollForGoodVisitTests];
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  mock_clock.Advance(base::Seconds(kGoodVisitTimeInFeedSeconds) +
                     kAddedTimeForMockClock);
  [recorder recordFeedScrolled:kMinScrollForGoodVisitTests];
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that a Short Click Visit is recorded appropriately.
TEST_F(FeedMetricsRecorderTest, GoodVisit_ShortClickVisitFollowing) {
  // Fast forward the clock to build time.
  base::ScopedMockClockOverride mock_clock;
  // Change feed to Following.
  OCMStub([mockedDelegate selectedFeed]).andReturn(FeedTypeFollowing);
  // Trigger article click
  [recorder recordOpenURLInSameTab];
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  mock_clock.Advance(
      (base::Seconds(kNonShortClickSeconds) + kAddedTimeForMockClock));
  // Coming back to the main feed. There should be a Good Visit.
  [recorder recordNTPDidChangeVisibility:YES];
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}
