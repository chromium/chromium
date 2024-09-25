// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_recorder.h"

#import <Foundation/Foundation.h>

#import "base/build_time.h"
#import "base/json/values_util.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_mock_clock_override.h"
#import "components/feed/core/v2/public/common_enums.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/metrics/model/constants.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_state.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_recorder+testing.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_control_delegate.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#define EXPECT_ACTION(action, method_call)                 \
  {                                                        \
    EXPECT_EQ(actions_tester_->GetActionCount(action), 0); \
    [recorder_ method_call];                               \
    EXPECT_EQ(actions_tester_->GetActionCount(action), 1); \
  }

using feed::FeedEngagementType;

class FeedMetricsRecorderTest : public PlatformTest {
 public:
  FeedMetricsRecorderTest() {
    RegisterProfilePrefs(test_pref_service_.registry());
    recorder_ =
        [[FeedMetricsRecorder alloc] initWithPrefService:&test_pref_service_];
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    actions_tester_ = std::make_unique<base::UserActionTester>();
    NTP_state_ = [[NewTabPageState alloc] init];
    recorder_.NTPState = NTP_state_;
  }

 protected:
  // Constant used to set a standard minimum scroll for testing.
  const int kMinScrollForGoodVisitTests = 50;
  // Used for time based tests.
  const base::TimeDelta kAddedTimeForMockClock = base::Seconds(5);
  const base::TimeDelta kTimeForFeedTimeMetric = base::Minutes(2);
  const base::TimeDelta kOneDay = base::Hours(24);
  void TearDown() override {
    [recorder_ resetGoodVisitSession];
    PlatformTest::TearDown();
  }
  sync_preferences::TestingPrefServiceSyncable test_pref_service_;
  FeedMetricsRecorder* recorder_;
  NewTabPageState* NTP_state_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<base::UserActionTester> actions_tester_;
};

#pragma mark - All Feeds Good Visit tests

// Tests that a Good Visit is recorded when a url is added to Read Later.
TEST_F(FeedMetricsRecorderTest, GoodExplicitInteraction) {
  // There should not be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // Add URL to Read Later constitutes a Good Visit by itself.
  [recorder_ recordAddURLToReadLater];
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
  [recorder_ recordOpenURLInIncognitoTab];
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that a Good Visit is recorded when we do a long press on a card.
TEST_F(FeedMetricsRecorderTest, GoodVisit_LongPress) {
  // There should not be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // After Action, Good Visit should be recorded.
  [recorder_ recordNativeContextMenuVisibilityChanged:YES];
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that a Good Visit is only logged once for each Good Visit session.
TEST_F(FeedMetricsRecorderTest, GoodVisit_OnlyLoggedOncePerVisit) {
  // There should not be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // Start with a Good Visit.
  [recorder_ recordAddURLToReadLater];
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
  // Adding to Read Later should count as a Good Visit, but we only log one Good
  // Visit per session, so the histogram count should remain at 1.
  [recorder_ recordAddURLToReadLater];
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
  [recorder_ recordHeaderMenuManageTapped];
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
  [recorder_ recordAddURLToReadLater];
  // There should be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
  // Change feed to Following.
  NTP_state_.selectedFeed = FeedTypeFollowing;

  // Add URL to Read Later constitutes a Good Visit for AllFeeds (not counted as
  // one has been triggered already this session) and Following. The Discover
  // histogram should still have 1 Good Visit reported.
  [recorder_ recordAddURLToReadLater];
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

#pragma mark - AllFeeds Time Based Tests

TEST_F(FeedMetricsRecorderTest, GoodVisit_GoodTimeInFeed) {
  base::ScopedMockClockOverride mock_clock;

  [recorder_ recordNTPDidChangeVisibility:YES];
  [recorder_ recordFeedScrolled:kMinScrollForGoodVisitTests];

  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  mock_clock.Advance(base::Seconds(kGoodVisitTimeInFeedSeconds) +
                     kAddedTimeForMockClock);
  // Calling an arbitrary GV action. This action should not trigger a GV by
  // itself, but cycles the checks for other GV paths.
  [recorder_ recordFeedScrolled:kMinScrollForGoodVisitTests];
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that a Short Click Visit is recorded appropriately.
TEST_F(FeedMetricsRecorderTest, GoodVisit_ShortClickVisit) {
  base::ScopedMockClockOverride mock_clock;
  // Trigger article click
  [recorder_ recordOpenURLInSameTab];
  [recorder_ recordNTPDidChangeVisibility:NO];
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  mock_clock.Advance(
      (base::Seconds(kNonShortClickSeconds) + kAddedTimeForMockClock));
  // Coming back to the main feed. There should be a Good Visit.
  [recorder_ recordNTPDidChangeVisibility:YES];
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that the session expires accordingly.
TEST_F(FeedMetricsRecorderTest, GoodVisit_SessionExpiration) {
  base::ScopedMockClockOverride mock_clock;
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // Trigger Good Visit
  [recorder_ recordAddURLToReadLater];
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
  [recorder_ recordAddURLToReadLater];
  // Check it's not double logged
  histogram_tester_->ExpectBucketCount(kAllFeedsEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
  [recorder_ recordNTPDidChangeVisibility:NO];

  // Trigger session expiration by waiting `kMinutesBetweenSessions`
  mock_clock.Advance(
      (base::Minutes(kMinutesBetweenSessions) + kAddedTimeForMockClock));
  // Coming back to the main feed. Session should have been reset so there
  // should be 2 histograms.
  [recorder_ recordNTPDidChangeVisibility:YES];
  [recorder_ recordAddURLToReadLater];
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
  [recorder_ recordAddURLToReadLater];
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
  [recorder_ recordOpenURLInIncognitoTab];
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
  [recorder_ recordNativeContextMenuVisibilityChanged:YES];
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
  [recorder_ recordAddURLToReadLater];
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
  // Adding to Read Later should count as a Good Visit, but we only log one Good
  // Visit per session, so the histogram count should remain at 1.
  [recorder_ recordAddURLToReadLater];
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
  [recorder_ recordHeaderMenuManageTapped];
  // There should not be a Good Visit recorded as the action was not a trigger
  // for a Good Visit.
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
}

#pragma mark - Discover Feed Time Based Good Visit tests

TEST_F(FeedMetricsRecorderTest, GoodVisit_GoodTimeInFeedDiscover) {
  base::ScopedMockClockOverride mock_clock;

  [recorder_ recordNTPDidChangeVisibility:YES];
  [recorder_ recordFeedScrolled:kMinScrollForGoodVisitTests];
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  mock_clock.Advance(base::Seconds(kGoodVisitTimeInFeedSeconds) +
                     kAddedTimeForMockClock);
  [recorder_ recordFeedScrolled:kMinScrollForGoodVisitTests];
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that a Short Click Visit is recorded appropriately.
TEST_F(FeedMetricsRecorderTest, GoodVisit_ShortClickVisitDiscover) {
  base::ScopedMockClockOverride mock_clock;
  // Trigger article click
  [recorder_ recordOpenURLInSameTab];
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  mock_clock.Advance(
      (base::Seconds(kNonShortClickSeconds) + kAddedTimeForMockClock));
  // Coming back to the main feed. There should be a Good Visit.
  [recorder_ recordNTPDidChangeVisibility:YES];
  histogram_tester_->ExpectBucketCount(kDiscoverFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

#pragma mark - Following Feed Good Visit tests

// Tests that a Good Visit is recorded when a url is added to Read Later.
TEST_F(FeedMetricsRecorderTest, GoodExplicitInteraction_Following) {
  // Change feed to Following.
  NTP_state_.selectedFeed = FeedTypeFollowing;
  // There should not be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // Add URL to Read Later constitutes a Good Visit by itself.
  [recorder_ recordAddURLToReadLater];
  // There should be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that a Good Visit is recorded when we open a url in a new incognito
// tab.
TEST_F(FeedMetricsRecorderTest, GoodVisit_OpenInNewIncognitoTab_Following) {
  // Change feed to Following.
  NTP_state_.selectedFeed = FeedTypeFollowing;
  // There should not be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // After Action, Good Visit should be recorded.
  [recorder_ recordOpenURLInIncognitoTab];
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that a Good Visit is recorded when we do a long press on a card.
TEST_F(FeedMetricsRecorderTest, GoodVisit_LongPress_Following) {
  // Change feed to Following.
  NTP_state_.selectedFeed = FeedTypeFollowing;
  // There should not be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // After Action, Good Visit should be recorded.
  [recorder_ recordNativeContextMenuVisibilityChanged:YES];
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that a Good Visit is only logged once for each Good Visit session.
TEST_F(FeedMetricsRecorderTest, GoodVisit_OnlyLoggedOncePerVisit_Following) {
  // Change feed to Following.
  NTP_state_.selectedFeed = FeedTypeFollowing;
  // There should not be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // Start with a Good Visit.
  [recorder_ recordAddURLToReadLater];
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
  // Adding to Read Later should count as a Good Visit, but we only log one Good
  // Visit per session, so the histogram count should remain at 1.
  [recorder_ recordAddURLToReadLater];
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that a Good Visit is not logged when a non-Good Visit action is
// triggered.
TEST_F(FeedMetricsRecorderTest,
       GoodVisit_NonGoodVisitActionTriggered_Following) {
  // Change feed to Following.
  NTP_state_.selectedFeed = FeedTypeFollowing;
  // There should not be a Good Visit recorded.
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  // Trigger a non-Good Visit action.
  [recorder_ recordHeaderMenuManageTapped];
  // There should not be a Good Visit recorded as the action was not a trigger
  // for a Good Visit.
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
}

#pragma mark - Following Feed Time Based Good Visit tests

TEST_F(FeedMetricsRecorderTest, GoodVisit_GoodTimeInFeedFollowing) {
  base::ScopedMockClockOverride mock_clock;
  // Change feed to Following.
  NTP_state_.selectedFeed = FeedTypeFollowing;

  [recorder_ recordNTPDidChangeVisibility:YES];
  [recorder_ recordFeedScrolled:kMinScrollForGoodVisitTests];
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  mock_clock.Advance(base::Seconds(kGoodVisitTimeInFeedSeconds) +
                     kAddedTimeForMockClock);
  [recorder_ recordFeedScrolled:kMinScrollForGoodVisitTests];
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

// Tests that a Short Click Visit is recorded appropriately.
TEST_F(FeedMetricsRecorderTest, GoodVisit_ShortClickVisitFollowing) {
  base::ScopedMockClockOverride mock_clock;
  // Change feed to Following.
  NTP_state_.selectedFeed = FeedTypeFollowing;
  // Trigger article click
  [recorder_ recordOpenURLInSameTab];
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 0);
  mock_clock.Advance(
      (base::Seconds(kNonShortClickSeconds) + kAddedTimeForMockClock));
  // Coming back to the main feed. There should be a Good Visit.
  [recorder_ recordNTPDidChangeVisibility:YES];
  histogram_tester_->ExpectBucketCount(kFollowingFeedEngagementTypeHistogram,
                                       FeedEngagementType::kGoodVisit, 1);
}

#pragma mark - Time Spent in Feed tests.

// Tests that the time spent in feed is recorded appropriately.
TEST_F(FeedMetricsRecorderTest, TimeSpent_RecordedCorrectly) {
  base::ScopedMockClockOverride mock_clock;
  // Make the feed visible.
  [recorder_ recordNTPDidChangeVisibility:YES];
  // Advance clock.
  mock_clock.Advance(kTimeForFeedTimeMetric);
  // Hide feed again.
  [recorder_ recordNTPDidChangeVisibility:NO];
  EXPECT_EQ(kTimeForFeedTimeMetric, recorder_.timeSpentInFeed);
}

// TODO(crbug.com/40885127) Add test to check if the histogram is recorded
// appropriately.

#pragma mark - Unit tests of histogram methods.

// Test if a refresh trigger has been actioned and recorded properly.
TEST_F(FeedMetricsRecorderTest, Histograms_RecordRefreshTrigger) {
  // Testing the kOther Refresh Trigger.
  FeedRefreshTrigger trigger = FeedRefreshTrigger::kOther;
  // There should not be a trigger recorded yet.
  histogram_tester_->ExpectBucketCount(kDiscoverFeedRefreshTrigger, trigger, 0);
  // Trigger the Refresh.
  [FeedMetricsRecorder recordFeedRefreshTrigger:FeedRefreshTrigger::kOther];
  // There should be one metric reported.
  histogram_tester_->ExpectBucketCount(kDiscoverFeedRefreshTrigger, trigger, 1);
}

#pragma mark - Unit tests of action methods.

// Test if a change in device orientation is recorded correctly.
TEST_F(FeedMetricsRecorderTest, Actions_RecordChangeOrientation) {
  // There should not be a user action recorded yet.
  EXPECT_EQ(actions_tester_->GetActionCount(
                kDiscoverFeedHistogramDeviceOrientationChangedToPortrait),
            0);
  // Change orientation to portrait.
  [recorder_ recordDeviceOrientationChanged:UIDeviceOrientationPortrait];
  // `kDiscoverFeedHistogramDeviceOrientationChangedToLandscape` should be 0.
  EXPECT_EQ(actions_tester_->GetActionCount(
                kDiscoverFeedHistogramDeviceOrientationChangedToPortrait),
            1);
  EXPECT_NE(actions_tester_->GetActionCount(
                kDiscoverFeedHistogramDeviceOrientationChangedToLandscape),
            1);
  // Change orientation to Landscape.
  [recorder_ recordDeviceOrientationChanged:UIDeviceOrientationLandscapeRight];
  // Both actions should be 1.
  EXPECT_EQ(actions_tester_->GetActionCount(
                kDiscoverFeedHistogramDeviceOrientationChangedToPortrait),
            1);
  EXPECT_EQ(actions_tester_->GetActionCount(
                kDiscoverFeedHistogramDeviceOrientationChangedToLandscape),
            1);
}

// Testing `recordDiscoverFeedPreviewTapped`.
TEST_F(FeedMetricsRecorderTest, Actions_PreviewTapped) {
  EXPECT_ACTION(kDiscoverFeedUserActionPreviewTapped,
                recordDiscoverFeedPreviewTapped);
}

// Testing `recordHeaderMenuManageFollowingTapped`.
TEST_F(FeedMetricsRecorderTest, Actions_ManageFollowingTapped) {
  EXPECT_ACTION(kDiscoverFeedUserActionManageFollowingTapped,
                recordHeaderMenuManageFollowingTapped);
}

// Testing `recordOpenURLInNewTab`.
TEST_F(FeedMetricsRecorderTest, Actions_OpenNewTab) {
  EXPECT_ACTION(kDiscoverFeedUserActionOpenNewTab, recordOpenURLInNewTab);
}

// Testing `recordTapSendFeedback`.
TEST_F(FeedMetricsRecorderTest, Actions_SendFeedbackOpened) {
  EXPECT_ACTION(kDiscoverFeedUserActionSendFeedbackOpened,
                recordTapSendFeedback);
}

// Testing `recordOpenBackOfCardMenu`.
TEST_F(FeedMetricsRecorderTest, Actions_ContextMenuOpened) {
  EXPECT_ACTION(kDiscoverFeedUserActionContextMenuOpened,
                recordOpenBackOfCardMenu);
}

// Testing `recordCloseBackOfCardMenu`.
TEST_F(FeedMetricsRecorderTest, Actions_CloseContextMenu) {
  EXPECT_ACTION(kDiscoverFeedUserActionCloseContextMenu,
                recordCloseBackOfCardMenu);
}

// Testing `recordOpenNativeBackOfCardMenu`.
TEST_F(FeedMetricsRecorderTest, Actions_NativeActionSheetOpened) {
  EXPECT_ACTION(kDiscoverFeedUserActionNativeActionSheetOpened,
                recordOpenNativeBackOfCardMenu);
}

// Testing `recordShowDialog`.
TEST_F(FeedMetricsRecorderTest, Actions_ReportContentOpened) {
  EXPECT_ACTION(kDiscoverFeedUserActionReportContentOpened, recordShowDialog);
}

// Testing `recordDismissDialog`.
TEST_F(FeedMetricsRecorderTest, Actions_ReportContentClosed) {
  EXPECT_ACTION(kDiscoverFeedUserActionReportContentClosed,
                recordDismissDialog);
}

// Testing `recordDismissCard`.
TEST_F(FeedMetricsRecorderTest, Actions_HideStory) {
  EXPECT_ACTION(kDiscoverFeedUserActionHideStory, recordDismissCard);
}

// Testing `recordBrokenNTPHierarchy`.
TEST_F(FeedMetricsRecorderTest, Actions_kNTPViewHierarchyFixed) {
  EXPECT_ACTION(
      kNTPViewHierarchyFixed, recordBrokenNTPHierarchy
      : BrokenNTPHierarchyRelationship::kContentSuggestionsHeaderParent);
}

// Testing `recordFeedWillRefresh`.
TEST_F(FeedMetricsRecorderTest, Actions_kFeedWillRefresh) {
  EXPECT_ACTION(kFeedWillRefresh, recordFeedWillRefresh);
}

// Testing `recordFollowFromMenu`.
TEST_F(FeedMetricsRecorderTest, Actions_kFollowFromMenu) {
  EXPECT_ACTION(kFollowFromMenu, recordFollowFromMenu);
}

// Testing `recordUnfollowFromMenu`.
TEST_F(FeedMetricsRecorderTest, Actions_kUnfollowFromMenu) {
  EXPECT_ACTION(kUnfollowFromMenu, recordUnfollowFromMenu);
}

// Testing `recordManagementTappedUnfollow`.
TEST_F(FeedMetricsRecorderTest, Actions_ManagementTappedUnfollow) {
  EXPECT_ACTION(kDiscoverFeedUserActionManagementTappedUnfollow,
                recordManagementTappedUnfollow);
}

// Testing `recordManagementTappedRefollowAfterUnfollowOnSnackbar`.
TEST_F(FeedMetricsRecorderTest,
       Actions_ManagementTappedRefollowAfterUnfollowOnSnackbar) {
  EXPECT_ACTION(
      kDiscoverFeedUserActionManagementTappedRefollowAfterUnfollowOnSnackbar,
      recordManagementTappedRefollowAfterUnfollowOnSnackbar);
}

// Testing `recordFirstFollowTappedGoToFeed`.
TEST_F(FeedMetricsRecorderTest, Actions_FirstFollowGoToFeedButtonTapped) {
  EXPECT_ACTION(kFirstFollowGoToFeedButtonTapped,
                recordFirstFollowTappedGoToFeed);
}

// Testing `recordFirstFollowTappedGotIt`.
TEST_F(FeedMetricsRecorderTest, Actions_FirstFollowGotItButtonTapped) {
  EXPECT_ACTION(kFirstFollowGotItButtonTapped, recordFirstFollowTappedGotIt);
}

// Testing `recordShowSignInOnlyUIWithUserId` with user Id.
TEST_F(FeedMetricsRecorderTest, Actions_ShowFeedSignInOnlyUIWithUserId) {
  EXPECT_ACTION(kShowFeedSignInOnlyUIWithUserId,
                recordShowSignInOnlyUIWithUserId
                : YES);
}
// Testing `recordShowSignInOnlyUIWithUserId` without User Id.
TEST_F(FeedMetricsRecorderTest, Actions_ShowFeedSignInOnlyUIWithoutUserId) {
  EXPECT_ACTION(kShowFeedSignInOnlyUIWithoutUserId,
                recordShowSignInOnlyUIWithUserId
                : NO);
}

#pragma mark - ComputeActivityBuckets Tests

// Tests that kNoActivity is correctly saved in kActivityBucketKey.
TEST_F(FeedMetricsRecorderTest, TestComputeActivityBuckets_kNoActivity) {
  // Make sure that the activity was not reported recently.
  base::Time last_activity_bucket = base::Time() - base::Days(10);
  test_pref_service_.SetTime(kActivityBucketLastReportedDateKey,
                             last_activity_bucket);
  // Make sure LastReportedDateArray is empty.
  test_pref_service_.ClearPref(kActivityBucketLastReportedDateArrayKey);

  [recorder_ recordNTPDidChangeVisibility:YES];

  DCHECK_EQ(test_pref_service_.GetInteger(kActivityBucketKey),
            static_cast<int>(FeedActivityBucket::kNoActivity));
}

// Tests that kLowActivity is correctly saved in kActivityBucketKey.
TEST_F(FeedMetricsRecorderTest, TestComputeActivityBuckets_kLowActivity) {
  // Make sure that the activity was not reported recently.
  base::Time last_activity_bucket = base::Time() - base::Days(10);
  test_pref_service_.SetTime(kActivityBucketLastReportedDateKey,
                             last_activity_bucket);
  // Make sure LastReportedDateArray is in range 1 to 7.
  base::Value::List listOfDates;
  for (size_t i = 0; i < 5; ++i) {
    listOfDates.Append(TimeToValue(base::Time::Now()));
  }
  test_pref_service_.SetList(kActivityBucketLastReportedDateArrayKey,
                             std::move(listOfDates));

  [recorder_ recordNTPDidChangeVisibility:YES];

  DCHECK_EQ(test_pref_service_.GetInteger(kActivityBucketKey),
            static_cast<int>(FeedActivityBucket::kLowActivity));
}

// Tests that kMediumActivity is correctly saved in kActivityBucketKey.
TEST_F(FeedMetricsRecorderTest, TestComputeActivityBuckets_kMediumActivity) {
  // Make sure that the activity was not reported recently.
  base::Time last_activity_bucket = base::Time() - base::Days(10);
  test_pref_service_.SetTime(kActivityBucketLastReportedDateKey,
                             last_activity_bucket);
  // Make sure LastReportedDateArray is in range 8 to 15.
  base::Value::List listOfDates;
  for (size_t i = 0; i < 9; ++i) {
    listOfDates.Append(TimeToValue(base::Time::Now()));
  }
  test_pref_service_.SetList(kActivityBucketLastReportedDateArrayKey,
                             std::move(listOfDates));

  [recorder_ recordNTPDidChangeVisibility:YES];

  DCHECK_EQ(test_pref_service_.GetInteger(kActivityBucketKey),
            static_cast<int>(FeedActivityBucket::kMediumActivity));
}
