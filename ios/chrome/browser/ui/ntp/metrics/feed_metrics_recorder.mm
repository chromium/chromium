// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_recorder.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/time/time.h"
#import "ios/chrome/browser/discover_feed/discover_feed_refresher.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/ui/ntp/feed_control_delegate.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_session_recorder.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_follow_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_metrics_delegate.h"

using feed::FeedEngagementType;
using feed::FeedUserActionType;

@interface FeedMetricsRecorder ()
// Helper for recording session time metrics.
@property(nonatomic, strong) FeedSessionRecorder* sessionRecorder;
// Tracking property to avoid duplicate recordings of
// FeedEngagementType::kFeedEngagedSimple.
@property(nonatomic, assign) BOOL engagedSimpleReportedDiscover;
@property(nonatomic, assign) BOOL engagedSimpleReportedFollowing;
// Tracking property to avoid duplicate recordings of
// FeedEngagementType::kFeedEngaged.
@property(nonatomic, assign) BOOL engagedReportedDiscover;
@property(nonatomic, assign) BOOL engagedReportedFollowing;
// Tracking property to avoid duplicate recordings of
// FeedEngagementType::kFeedScrolled.
@property(nonatomic, assign) BOOL scrolledReportedDiscover;
@property(nonatomic, assign) BOOL scrolledReportedFollowing;

// Tracking property to avoid duplicate recordings of
// FeedEngagementType::kGoodVisit.
@property(nonatomic, assign) BOOL goodVisitReportedAllFeeds;
@property(nonatomic, assign) BOOL goodVisitReportedDiscover;
@property(nonatomic, assign) BOOL goodVisitReportedFollowing;

// Tracking property to avoid duplicate recordings of the Activity Buckets
// metric.
@property(nonatomic, assign) NSDate* activityBucketLastReportedDate;

// Tracks whether user has engaged with the latest refreshed content. The term
// "engaged" is defined by its usage in this file. For example, it may be
// similar to `engagedSimpleReportedDiscover`.
@property(nonatomic, assign, getter=hasEngagedWithLatestRefreshedContent)
    BOOL engagedWithLatestRefreshedContent;

// Tracking property to record a scroll for Good Visits.
// TODO(crbug.com/1373650) separate the property below in two, one for each
// feed.
@property(nonatomic, assign) BOOL goodVisitScroll;
// The timestamp when the first metric is being recorded for this session.
@property(nonatomic, assign) base::Time sessionStartTime;
// The timestamp when the last interaction happens for Good Visits.
@property(nonatomic, assign) base::Time lastInteractionTimeForGoodVisits;
@property(nonatomic, assign)
    base::Time lastInteractionTimeForDiscoverGoodVisits;
@property(nonatomic, assign)
    base::Time lastInteractionTimeForFollowingGoodVisits;
// The timestamp when the feed becomes visible again for Good Visits. It
// is reset when a new Good Visit session starts
@property(nonatomic, assign) base::Time feedBecameVisibleTime;
// The time the user has spent in the feed during a Good Visit session.
// This property is preserved across NTP usages if they are part of the same
// Good Visit Session.
@property(nonatomic, assign)
    NSTimeInterval previousTimeInFeedForGoodVisitSession;
@property(nonatomic, assign) NSTimeInterval discoverPreviousTimeInFeedGV;
@property(nonatomic, assign) NSTimeInterval followingPreviousTimeInFeedGV;

// The aggregate of time a user has spent in the feed for
// `ContentSuggestions.Feed.TimeSpentInFeed`
@property(nonatomic, assign) base::TimeDelta timeSpentInFeed;

// Timer to refresh the feed.
@property(nonatomic, strong) NSTimer* refreshTimer;

// YES if the NTP is visible.
@property(nonatomic, assign) BOOL isNTPVisible;

@end

@implementation FeedMetricsRecorder

#pragma mark - Properties

- (FeedSessionRecorder*)sessionRecorder {
  if (!_sessionRecorder) {
    _sessionRecorder = [[FeedSessionRecorder alloc] init];
  }
  return _sessionRecorder;
}

#pragma mark - Public

- (void)dealloc {
  [self.refreshTimer invalidate];
  self.refreshTimer = nil;
}

+ (void)recordFeedRefreshTrigger:(FeedRefreshTrigger)trigger {
  base::UmaHistogramEnumeration(kDiscoverFeedRefreshTrigger, trigger);
}

- (void)recordFeedScrolled:(int)scrollDistance {
  self.goodVisitScroll = YES;
  [self checkEngagementGoodVisitWithInteraction:NO];

  // If neither feed has been scrolled into, log "AllFeeds" scrolled.
  if (!self.scrolledReportedDiscover && !self.scrolledReportedFollowing) {
    UMA_HISTOGRAM_ENUMERATION(kAllFeedsEngagementTypeHistogram,
                              FeedEngagementType::kFeedScrolled);
  }

  // Log scrolled into Discover feed.
  if ([self.feedControlDelegate selectedFeed] == FeedTypeDiscover &&
      !self.scrolledReportedDiscover) {
    UMA_HISTOGRAM_ENUMERATION(kDiscoverFeedEngagementTypeHistogram,
                              FeedEngagementType::kFeedScrolled);
    self.scrolledReportedDiscover = YES;
  }

  // Log scrolled into Following feed.
  if ([self.feedControlDelegate selectedFeed] == FeedTypeFollowing &&
      !self.scrolledReportedFollowing) {
    UMA_HISTOGRAM_ENUMERATION(kFollowingFeedEngagementTypeHistogram,
                              FeedEngagementType::kFeedScrolled);
    self.scrolledReportedFollowing = YES;
  }

  [self recordEngagement:scrollDistance interacted:NO];
}

- (void)recordDeviceOrientationChanged:(UIDeviceOrientation)orientation {
  if (orientation == UIDeviceOrientationPortrait) {
    base::RecordAction(base::UserMetricsAction(
        kDiscoverFeedHistogramDeviceOrientationChangedToPortrait));
  } else if (orientation == UIDeviceOrientationLandscapeLeft ||
             orientation == UIDeviceOrientationLandscapeRight) {
    base::RecordAction(base::UserMetricsAction(
        kDiscoverFeedHistogramDeviceOrientationChangedToLandscape));
  }
}

- (void)recordFeedTypeChangedFromFeed:(FeedType)previousFeed {
  // Recalculate time spent in previous surface.
  [self timeSpentForCurrentGoodVisitSessionInFeed:previousFeed];
}

- (void)recordNTPDidChangeVisibility:(BOOL)visible {
  self.isNTPVisible = visible;
  // Invalidate the timer when the user returns to the feed since the feed
  // should not be refreshed when the user is viewing it.
  if (visible) {
    [self.refreshTimer invalidate];
    [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                    kOpenedFeedSurface
                                  asInteraction:NO];
  }

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  if (visible) {
    NSDate* lastInteractionTimeForGoodVisitsDate =
        base::apple::ObjCCast<NSDate>(
            [defaults objectForKey:kLastInteractionTimeForGoodVisits]);
    if (lastInteractionTimeForGoodVisitsDate != nil) {
      self.lastInteractionTimeForGoodVisits =
          base::Time::FromNSDate(lastInteractionTimeForGoodVisitsDate);
    }

    NSDate* lastInteractionTimeForDiscoverGoodVisitsDate =
        base::apple::ObjCCast<NSDate>(
            [defaults objectForKey:kLastInteractionTimeForDiscoverGoodVisits]);
    if (lastInteractionTimeForDiscoverGoodVisitsDate != nil) {
      self.lastInteractionTimeForDiscoverGoodVisits =
          base::Time::FromNSDate(lastInteractionTimeForDiscoverGoodVisitsDate);
    }

    NSDate* lastInteractionTimeForFollowingGoodVisitsDate =
        base::apple::ObjCCast<NSDate>(
            [defaults objectForKey:kLastInteractionTimeForFollowingGoodVisits]);
    if (lastInteractionTimeForFollowingGoodVisitsDate != nil) {
      self.lastInteractionTimeForFollowingGoodVisits =
          base::Time::FromNSDate(lastInteractionTimeForFollowingGoodVisitsDate);
    }

    // Total time spent in feed metrics.
    self.timeSpentInFeed =
        base::Seconds([defaults doubleForKey:kTimeSpentInFeedAggregateKey]);
    [self computeActivityBuckets];
    [self recordTimeSpentInFeedIfDayIsDone];

    self.previousTimeInFeedForGoodVisitSession =
        [defaults doubleForKey:kLongFeedVisitTimeAggregateKey];
    self.discoverPreviousTimeInFeedGV =
        [defaults doubleForKey:kLongDiscoverFeedVisitTimeAggregateKey];
    self.followingPreviousTimeInFeedGV =
        [defaults doubleForKey:kLongFollowingFeedVisitTimeAggregateKey];

    // Checks if there is a timestamp in defaults for when a user clicked
    // on an article in order to be able to trigger a non-short click
    // interaction.
    NSDate* articleVisitStart = base::apple::ObjCCast<NSDate>(
        [defaults objectForKey:kArticleVisitTimestampKey]);
    self.feedBecameVisibleTime = base::Time::Now();

    if (articleVisitStart) {
      // Report Good Visit if user came back to the NTP after spending
      // kNonShortClickSeconds in a feed article.
      if (base::Time::FromNSDate(articleVisitStart) - base::Time::Now() >
          base::Seconds(kNonShortClickSeconds)) {
        // Trigger a GV for a specific feed.
        [self
            recordEngagedGoodVisits:
                (FeedType)[defaults integerForKey:kLastUsedFeedForGoodVisitsKey]
                       allFeedsOnly:NO];
      }
      // Clear defaults for new session.
      [defaults setObject:nil forKey:kArticleVisitTimestampKey];
    }
  } else {
    // Once the NTP becomes hidden, check for Good Visit which updates
    // `self.previousTimeInFeedForGoodVisitSession` and then we save it to
    // defaults.

    // Also calculate total aggregate for the time in feed aggregate metric.
    self.timeSpentInFeed = base::Time::Now() - self.feedBecameVisibleTime;

    [self checkEngagementGoodVisitWithInteraction:NO];
    [defaults setDouble:self.timeSpentInFeed.InSecondsF()
                 forKey:kTimeSpentInFeedAggregateKey];
    [defaults setDouble:self.previousTimeInFeedForGoodVisitSession
                 forKey:kLongFeedVisitTimeAggregateKey];
    [defaults setDouble:self.discoverPreviousTimeInFeedGV
                 forKey:kLongDiscoverFeedVisitTimeAggregateKey];
    [defaults setDouble:self.followingPreviousTimeInFeedGV
                 forKey:kLongFollowingFeedVisitTimeAggregateKey];
  }
}

- (void)recordDiscoverFeedPreviewTapped {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kTappedDiscoverFeedPreview
                                asInteraction:YES];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionPreviewTapped));
}

- (void)recordHeaderMenuLearnMoreTapped {
  [self
      recordDiscoverFeedUserActionHistogram:FeedUserActionType::kTappedLearnMore
                              asInteraction:NO];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionLearnMoreTapped));
}

- (void)recordHeaderMenuManageTapped {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::kTappedManage
                                asInteraction:NO];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionManageTapped));
}

- (void)recordHeaderMenuManageActivityTapped {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kTappedManageActivity
                                asInteraction:NO];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionManageActivityTapped));
}

- (void)recordHeaderMenuManageInterestsTapped {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kTappedManageInterests
                                asInteraction:NO];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionManageInterestsTapped));
}

- (void)recordHeaderMenuManageHiddenTapped {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kTappedManageHidden
                                asInteraction:NO];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionManageHiddenTapped));
}

- (void)recordHeaderMenuManageFollowingTapped {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kTappedManageFollowing
                                asInteraction:NO];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionManageFollowingTapped));
}

- (void)recordDiscoverFeedVisibilityChanged:(BOOL)visible {
  if (visible) {
    [self
        recordDiscoverFeedUserActionHistogram:FeedUserActionType::kTappedTurnOn
                                asInteraction:NO];
    base::RecordAction(base::UserMetricsAction(kDiscoverFeedUserActionTurnOn));
  } else {
    [self
        recordDiscoverFeedUserActionHistogram:FeedUserActionType::kTappedTurnOff
                                asInteraction:NO];
    base::RecordAction(base::UserMetricsAction(kDiscoverFeedUserActionTurnOff));
  }
}

- (void)recordOpenURLInSameTab {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::kTappedOnCard
                                asInteraction:YES];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionOpenSameTab));
  [self recordOpenURL];
}

- (void)recordOpenURLInNewTab {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kTappedOpenInNewTab
                                asInteraction:YES];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionOpenNewTab));
  [self recordOpenURL];
}

- (void)recordOpenURLInIncognitoTab {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kTappedOpenInNewIncognitoTab
                                asInteraction:YES];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionOpenIncognitoTab));
  [self recordOpenURL];
}

- (void)recordAddURLToReadLater {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kAddedToReadLater
                                asInteraction:YES];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionReadLaterTapped));
}

- (void)recordTapSendFeedback {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kTappedSendFeedback
                                asInteraction:YES];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionSendFeedbackOpened));
}

- (void)recordOpenBackOfCardMenu {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kOpenedContextMenu
                                asInteraction:YES];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionContextMenuOpened));
}

- (void)recordCloseBackOfCardMenu {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kClosedContextMenu
                                asInteraction:YES];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionCloseContextMenu));
}

- (void)recordOpenNativeBackOfCardMenu {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kOpenedNativeActionSheet
                                asInteraction:YES];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionNativeActionSheetOpened));
}

- (void)recordShowDialog {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::kOpenedDialog
                                asInteraction:YES];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionReportContentOpened));
}

- (void)recordDismissDialog {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::kClosedDialog
                                asInteraction:YES];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionReportContentClosed));
}

- (void)recordDismissCard {
  [self
      recordDiscoverFeedUserActionHistogram:FeedUserActionType::kEphemeralChange
                              asInteraction:YES];
  base::RecordAction(base::UserMetricsAction(kDiscoverFeedUserActionHideStory));
}

- (void)recordUndoDismissCard {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kEphemeralChangeRejected
                                asInteraction:YES];
}

- (void)recordCommittDismissCard {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kEphemeralChangeCommited
                                asInteraction:YES];
}

- (void)recordShowSnackbar {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::kShowSnackbar
                                asInteraction:NO];
}

- (void)recordCommandID:(int)commandID {
  base::UmaHistogramSparse(kDiscoverFeedUserActionCommandHistogram, commandID);
}

- (void)recordCardShownAtIndex:(NSUInteger)index {
  switch ([self.feedControlDelegate selectedFeed]) {
    case FeedTypeDiscover:
      UMA_HISTOGRAM_EXACT_LINEAR(kDiscoverFeedCardShownAtIndex, index,
                                 kMaxCardsInFeed);
      break;
    case FeedTypeFollowing:
      UMA_HISTOGRAM_EXACT_LINEAR(kFollowingFeedCardShownAtIndex, index,
                                 kMaxCardsInFeed);
  }
}

- (void)recordCardTappedAtIndex:(NSUInteger)index {
  // TODO(crbug.com/1174088): No-op since this function gets called multiple
  // times for a tap. Log index when this is fixed.
}

- (void)recordNoticeCardShown:(BOOL)shown {
  base::UmaHistogramBoolean(kDiscoverFeedNoticeCardFulfilled, shown);
}

- (void)recordFeedArticlesFetchDurationInSeconds:
            (NSTimeInterval)durationInSeconds
                                         success:(BOOL)success {
  [self recordFeedArticlesFetchDuration:base::Seconds(durationInSeconds)
                                success:success];
}

- (void)recordFeedArticlesFetchDuration:(base::TimeDelta)duration
                                success:(BOOL)success {
  if (success) {
    UMA_HISTOGRAM_MEDIUM_TIMES(kDiscoverFeedArticlesFetchNetworkDurationSuccess,
                               duration);
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES(kDiscoverFeedArticlesFetchNetworkDurationFailure,
                               duration);
  }
  [self recordNetworkRequestDuration:duration];
}

- (void)recordFeedMoreArticlesFetchDurationInSeconds:
            (NSTimeInterval)durationInSeconds
                                             success:(BOOL)success {
  [self recordFeedMoreArticlesFetchDuration:base::Seconds(durationInSeconds)
                                    success:success];
}

- (void)recordFeedMoreArticlesFetchDuration:(base::TimeDelta)duration
                                    success:(BOOL)success {
  if (success) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        kDiscoverFeedMoreArticlesFetchNetworkDurationSuccess, duration);
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        kDiscoverFeedMoreArticlesFetchNetworkDurationFailure, duration);
  }
  [self recordNetworkRequestDuration:duration];
}

- (void)recordFeedUploadActionsDurationInSeconds:
            (NSTimeInterval)durationInSeconds
                                         success:(BOOL)success {
  [self recordFeedUploadActionsDuration:base::Seconds(durationInSeconds)
                                success:success];
}

- (void)recordFeedUploadActionsDuration:(base::TimeDelta)duration
                                success:(BOOL)success {
  if (success) {
    UMA_HISTOGRAM_MEDIUM_TIMES(kDiscoverFeedUploadActionsNetworkDurationSuccess,
                               duration);
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES(kDiscoverFeedUploadActionsNetworkDurationFailure,
                               duration);
  }
  [self recordNetworkRequestDuration:duration];
}

- (void)recordNativeContextMenuVisibilityChanged:(BOOL)shown {
  if (shown) {
    [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                    kOpenedNativeContextMenu
                                  asInteraction:YES];
    base::RecordAction(base::UserMetricsAction(
        kDiscoverFeedUserActionNativeContextMenuOpened));
  } else {
    [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                    kClosedNativeContextMenu
                                  asInteraction:YES];
    base::RecordAction(base::UserMetricsAction(
        kDiscoverFeedUserActionNativeContextMenuClosed));
  }
}

- (void)recordNativePulldownMenuVisibilityChanged:(BOOL)shown {
  if (shown) {
    [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                    kOpenedNativePulldownMenu
                                  asInteraction:YES];
    base::RecordAction(base::UserMetricsAction(
        kDiscoverFeedUserActionNativePulldownMenuOpened));
  } else {
    [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                    kClosedNativePulldownMenu
                                  asInteraction:YES];
    base::RecordAction(base::UserMetricsAction(
        kDiscoverFeedUserActionNativePulldownMenuClosed));
  }
}

- (void)recordActivityLoggingEnabled:(BOOL)loggingEnabled {
  base::UmaHistogramBoolean(kDiscoverFeedActivityLoggingEnabled,
                            loggingEnabled);
}

- (void)recordBrokenNTPHierarchy:(BrokenNTPHierarchyRelationship)relationship {
  base::UmaHistogramEnumeration(kDiscoverFeedBrokenNTPHierarchy, relationship);
  base::RecordAction(base::UserMetricsAction(kNTPViewHierarchyFixed));
}

- (void)recordFeedWillRefresh {
  base::RecordAction(base::UserMetricsAction(kFeedWillRefresh));
  // The feed will have new content so reset the engagement tracking variable.
  // TODO(crbug.com/1423467): We need to know whether the feed was actually
  // refreshed, and not just when it was triggered.
  self.engagedWithLatestRefreshedContent = NO;
}

- (void)recordFeedSelected:(FeedType)feedType
    fromPreviousFeedPosition:(NSUInteger)index {
  DCHECK(self.followDelegate);
  switch (feedType) {
    case FeedTypeDiscover:
      [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                      kDiscoverFeedSelected
                                    asInteraction:NO];
      base::RecordAction(base::UserMetricsAction(kDiscoverFeedSelected));
      UMA_HISTOGRAM_EXACT_LINEAR(kFollowingIndexWhenSwitchingFeed, index,
                                 kMaxCardsInFeed);
      break;
    case FeedTypeFollowing:
      [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                      kFollowingFeedSelected
                                    asInteraction:NO];
      base::RecordAction(base::UserMetricsAction(kFollowingFeedSelected));
      UMA_HISTOGRAM_EXACT_LINEAR(kDiscoverIndexWhenSwitchingFeed, index,
                                 kMaxCardsInFeed);
      NSUInteger followCount = [self.followDelegate followedPublisherCount];
      if (followCount > 0 &&
          [self.followDelegate doesFollowingFeedHaveContent]) {
        [self recordFollowCount:followCount
                   forLogReason:FollowCountLogReasonContentShown];
      } else {
        [self recordFollowCount:followCount
                   forLogReason:FollowCountLogReasonNoContentShown];
      }
      break;
  }
}

- (void)recordFollowCount:(NSUInteger)followCount
             forLogReason:(FollowCountLogReason)logReason {
  switch (logReason) {
    case FollowCountLogReasonContentShown:
      base::UmaHistogramSparse(kFollowCountFollowingContentShown, followCount);
      break;
    case FollowCountLogReasonNoContentShown:
      base::UmaHistogramSparse(kFollowCountFollowingNoContentShown,
                               followCount);
      break;
    case FollowCountLogReasonAfterFollow:
      base::UmaHistogramSparse(kFollowCountAfterFollow, followCount);
      break;
    case FollowCountLogReasonAfterUnfollow:
      base::UmaHistogramSparse(kFollowCountAfterUnfollow, followCount);
      break;
    case FollowCountLogReasonEngaged:
      base::UmaHistogramSparse(kFollowCountWhenEngaged, followCount);
      break;
  }
}

- (void)recordFeedSettingsOnStartForEnterprisePolicy:(BOOL)enterprisePolicy
                                         feedVisible:(BOOL)feedVisible
                                            signedIn:(BOOL)signedIn
                                          waaEnabled:(BOOL)waaEnabled
                                         spywEnabled:(BOOL)spywEnabled
                                     lastRefreshTime:
                                         (base::Time)lastRefreshTime {
  UserSettingsOnStart settings =
      [self userSettingsOnStartForEnterprisePolicy:enterprisePolicy
                                       feedVisible:feedVisible
                                          signedIn:signedIn
                                        waaEnabled:waaEnabled
                                   lastRefreshTime:lastRefreshTime];
  base::UmaHistogramEnumeration(kFeedUserSettingsOnStart, settings);
}

- (void)recordFollowingFeedSortTypeSelected:(FollowingFeedSortType)sortType {
  switch (sortType) {
    case FollowingFeedSortTypeByPublisher:
      UMA_HISTOGRAM_ENUMERATION(kFollowingFeedSortType,
                                FeedSortType::kGroupedByPublisher);
      base::RecordAction(
          base::UserMetricsAction(kFollowingFeedGroupByPublisher));
      return;
    case FollowingFeedSortTypeByLatest:
      UMA_HISTOGRAM_ENUMERATION(kFollowingFeedSortType,
                                FeedSortType::kSortedByLatest);
      base::RecordAction(base::UserMetricsAction(kFollowingFeedSortByLatest));
      return;
    case FollowingFeedSortTypeUnspecified:
      UMA_HISTOGRAM_ENUMERATION(kFollowingFeedSortType,
                                FeedSortType::kUnspecifiedSortType);
      return;
  }
}

#pragma mark - Follow

- (void)recordFollowRequestedWithType:(FollowRequestType)followRequestType {
  switch (followRequestType) {
    case FollowRequestType::kFollowRequestFollow:
      base::RecordAction(base::UserMetricsAction(kFollowRequested));
      break;
    case FollowRequestType::kFollowRequestUnfollow:
      base::RecordAction(base::UserMetricsAction(kUnfollowRequested));
      break;
  }
}

- (void)recordFollowFromMenu {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kTappedFollowButton
                                asInteraction:NO];
  base::RecordAction(base::UserMetricsAction(kFollowFromMenu));
}

- (void)recordUnfollowFromMenu {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kTappedUnfollowButton
                                asInteraction:NO];
  base::RecordAction(base::UserMetricsAction(kUnfollowFromMenu));
}

- (void)recordFollowConfirmationShownWithType:
    (FollowConfirmationType)followConfirmationType {
  UMA_HISTOGRAM_ENUMERATION(kDiscoverFeedUserActionHistogram,
                            FeedUserActionType::kShowSnackbar);
  switch (followConfirmationType) {
    case FollowConfirmationType::kFollowSucceedSnackbarShown:
      UMA_HISTOGRAM_ENUMERATION(kDiscoverFeedUserActionHistogram,
                                FeedUserActionType::kShowFollowSucceedSnackbar);
      break;
    case FollowConfirmationType::kFollowErrorSnackbarShown:
      UMA_HISTOGRAM_ENUMERATION(kDiscoverFeedUserActionHistogram,
                                FeedUserActionType::kShowFollowFailedSnackbar);
      break;
    case FollowConfirmationType::kUnfollowSucceedSnackbarShown:
      UMA_HISTOGRAM_ENUMERATION(
          kDiscoverFeedUserActionHistogram,
          FeedUserActionType::kShowUnfollowSucceedSnackbar);
      break;
    case FollowConfirmationType::kUnfollowErrorSnackbarShown:
      UMA_HISTOGRAM_ENUMERATION(
          kDiscoverFeedUserActionHistogram,
          FeedUserActionType::kShowUnfollowFailedSnackbar);
      break;
  }
}

- (void)recordFollowSnackbarTappedWithAction:
    (FollowSnackbarActionType)followSnackbarActionType {
  switch (followSnackbarActionType) {
    case FollowSnackbarActionType::kSnackbarActionGoToFeed:
      [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                      kTappedGoToFeedOnSnackbar
                                    asInteraction:NO];
      base::RecordAction(
          base::UserMetricsAction(kSnackbarGoToFeedButtonTapped));
      break;
    case FollowSnackbarActionType::kSnackbarActionUndo:
      [self recordDiscoverFeedUserActionHistogram:
                FeedUserActionType::kTappedRefollowAfterUnfollowOnSnackbar
                                    asInteraction:NO];
      base::RecordAction(base::UserMetricsAction(kSnackbarUndoButtonTapped));
      break;
    case FollowSnackbarActionType::kSnackbarActionRetryFollow:
      [self recordDiscoverFeedUserActionHistogram:
                FeedUserActionType::kTappedFollowTryAgainOnSnackbar
                                    asInteraction:NO];
      base::RecordAction(
          base::UserMetricsAction(kSnackbarRetryFollowButtonTapped));
      break;
    case FollowSnackbarActionType::kSnackbarActionRetryUnfollow:
      [self recordDiscoverFeedUserActionHistogram:
                FeedUserActionType::kTappedUnfollowTryAgainOnSnackbar
                                    asInteraction:NO];
      base::RecordAction(
          base::UserMetricsAction(kSnackbarRetryUnfollowButtonTapped));
      break;
  }
}

- (void)recordManagementTappedUnfollow {
  [self recordDiscoverFeedUserActionHistogram:
            FeedUserActionType::kTappedUnfollowOnManagementSurface
                                asInteraction:NO];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionManagementTappedUnfollow));
}

- (void)recordManagementTappedRefollowAfterUnfollowOnSnackbar {
  [self recordDiscoverFeedUserActionHistogram:
            FeedUserActionType::kTappedRefollowAfterUnfollowOnSnackbar
                                asInteraction:NO];
  base::RecordAction(base::UserMetricsAction(
      kDiscoverFeedUserActionManagementTappedRefollowAfterUnfollowOnSnackbar));
}

- (void)recordManagementTappedUnfollowTryAgainOnSnackbar {
  [self recordDiscoverFeedUserActionHistogram:
            FeedUserActionType::kTappedUnfollowTryAgainOnSnackbar
                                asInteraction:NO];
  base::RecordAction(base::UserMetricsAction(
      kDiscoverFeedUserActionManagementTappedUnfollowTryAgainOnSnackbar));
}

- (void)recordFirstFollowShown {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kFirstFollowSheetShown
                                asInteraction:NO];
}

- (void)recordFirstFollowTappedGoToFeed {
  [self recordDiscoverFeedUserActionHistogram:
            FeedUserActionType::kFirstFollowSheetTappedGoToFeed
                                asInteraction:NO];
  base::RecordAction(base::UserMetricsAction(kFirstFollowGoToFeedButtonTapped));
}

- (void)recordFirstFollowTappedGotIt {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kFirstFollowSheetTappedGotIt
                                asInteraction:NO];
  base::RecordAction(base::UserMetricsAction(kFirstFollowGotItButtonTapped));
}

- (void)recordFollowRecommendationIPHShown {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kFollowRecommendationIPHShown
                                asInteraction:NO];
}

- (void)recordSignInPromoUIContinueTapped {
  [self recordDiscoverFeedUserActionHistogram:
            FeedUserActionType::kTappedFeedSignInPromoUIContinue
                                asInteraction:NO];
  base::RecordAction(base::UserMetricsAction(kFeedSignInPromoUIContinueTapped));
}

- (void)recordSignInPromoUICancelTapped {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kTappedFeedSignInPromoUICancel
                                asInteraction:NO];
  base::RecordAction(base::UserMetricsAction(kFeedSignInPromoUICancelTapped));
}

- (void)recordShowSignInOnlyUIWithUserId:(BOOL)hasUserId {
  base::RecordAction(
      hasUserId ? base::UserMetricsAction(kShowFeedSignInOnlyUIWithUserId)
                : base::UserMetricsAction(kShowFeedSignInOnlyUIWithoutUserId));
}

- (void)recordShowSignInRelatedUIWithType:(feed::FeedSignInUI)type {
  base::UmaHistogramEnumeration(kFeedSignInUI, type);
  switch (type) {
    case feed::FeedSignInUI::kShowSyncHalfSheet:
      return base::RecordAction(
          base::UserMetricsAction(kShowSyncHalfSheetFromFeed));
    case feed::FeedSignInUI::kShowSignInOnlyFlow:
      return base::RecordAction(
          base::UserMetricsAction(kShowSignInOnlyFlowFromFeed));
    case feed::FeedSignInUI::kShowSignInDisableToast:
      return base::RecordAction(
          base::UserMetricsAction(kShowSignInDisableToastFromFeed));
  }
}

- (void)recordShowSyncnRelatedUIWithType:(feed::FeedSyncPromo)type {
  base::UmaHistogramEnumeration(kFeedSyncPromo, type);
  switch (type) {
    case feed::FeedSyncPromo::kShowSyncFlow:
      return base::RecordAction(base::UserMetricsAction(kShowSyncFlowFromFeed));
    case feed::FeedSyncPromo::kShowDisableToast:
      return base::RecordAction(
          base::UserMetricsAction(kShowDisableToastFromFeed));
  }
}

#pragma mark - Private

// Returns the UserSettingsOnStart value based on the user settings.
- (UserSettingsOnStart)
    userSettingsOnStartForEnterprisePolicy:(BOOL)enterprisePolicy
                               feedVisible:(BOOL)feedVisible
                                  signedIn:(BOOL)signedIn
                                waaEnabled:(BOOL)waaEnabled
                           lastRefreshTime:(base::Time)lastRefreshTime {
  if (!enterprisePolicy) {
    return UserSettingsOnStart::kFeedNotEnabledByPolicy;
  }

  if (!feedVisible) {
    if (signedIn) {
      return UserSettingsOnStart::kFeedNotVisibleSignedIn;
    }
    return UserSettingsOnStart::kFeedNotVisibleSignedOut;
  }

  if (!signedIn) {
    return UserSettingsOnStart::kSignedOut;
  }

  const base::TimeDelta delta = base::Time::Now() - lastRefreshTime;
  const BOOL hasRecentData =
      delta >= base::TimeDelta() && delta <= kUserSettingsMaxAge;
  if (!hasRecentData) {
    return UserSettingsOnStart::kSignedInNoRecentData;
  }

  if (waaEnabled) {
    return UserSettingsOnStart::kSignedInWaaOnDpOff;
  } else {
    return UserSettingsOnStart::kSignedInWaaOffDpOff;
  }
}

// Records histogram metrics for Discover feed user actions. If `isInteraction`,
// also logs an interaction to the visible feed.
- (void)recordDiscoverFeedUserActionHistogram:(FeedUserActionType)actionType
                                asInteraction:(BOOL)isInteraction {
  UMA_HISTOGRAM_ENUMERATION(kDiscoverFeedUserActionHistogram, actionType);
  if (isInteraction) {
    [self recordInteraction];
  }

  // Check if actionType warrants a Good Explicit Visit
  // If actionType is any of the cases below, trigger a Good Explicit
  // interaction by calling recordEngagementGoodVisit
  switch (actionType) {
    case FeedUserActionType::kAddedToReadLater:
    case FeedUserActionType::kOpenedNativeContextMenu:
    case FeedUserActionType::kTappedOpenInNewIncognitoTab:
      [self checkEngagementGoodVisitWithInteraction:YES];
      break;
    default:
      // Default will handle the remaining FeedUserActionTypes that
      // do not trigger a Good Explicit interaction, but might trigger a good
      // visit due to other checks e.g. Using the feed for
      // `kGoodVisitTimeInFeedSeconds`.
      [self checkEngagementGoodVisitWithInteraction:NO];
      break;
  }
}

// Logs engagement daily for the Activity Buckets Calculation.
- (void)logDailyActivity {
  NSDate* now = [NSDate date];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  // Check if the array is initialized.
  NSMutableArray<NSDate*>* lastReportedArray = [[defaults
      arrayForKey:kActivityBucketLastReportedDateArrayKey] mutableCopy];
  if (!lastReportedArray) {
    // Initialized before (could be empty).
    lastReportedArray = [NSMutableArray new];
  }

  // Adds a daily entry to the `lastReportedArray` array
  // only once when the user engages.
  if ([now timeIntervalSinceDate:[lastReportedArray lastObject]] >=
          (24 * 60 * 60) ||
      lastReportedArray.count == 0) {
    [lastReportedArray addObject:now];
    [defaults setObject:lastReportedArray
                 forKey:kActivityBucketLastReportedDateArrayKey];
  }
}

// Calculates the amount of dates the user has been active for the past 28 days.
- (void)computeActivityBuckets {
  NSDate* now = [NSDate date];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  NSDate* lastActivityBucketReported = base::apple::ObjCCast<NSDate>(
      [defaults objectForKey:kActivityBucketLastReportedDateKey]);
  // If the `lastActivityBucketReported` does not exist, set it to now to
  // prevent the first day from logging a metric.
  if (!lastActivityBucketReported) {
    lastActivityBucketReported = now;
    [defaults setObject:lastActivityBucketReported
                 forKey:kActivityBucketLastReportedDateKey];
  }

  // Check if the last time the activity was reported is more than 24 hrs ago,
  // and return for performance.
  if ([now timeIntervalSinceDate:lastActivityBucketReported] < (24 * 60 * 60)) {
    return;
  }

  // Retrieve activity bucket from storage.
  FeedActivityBucket activityBucket =
      (FeedActivityBucket)[defaults integerForKey:kActivityBucketKey];

  // Calculate activity buckets.
  // Check if the array is initialized.
  NSMutableArray<NSDate*>* lastReportedArray = [[defaults
      arrayForKey:kActivityBucketLastReportedDateArrayKey] mutableCopy];
  if (!lastReportedArray) {
    // Initialized before (could be empty).
    lastReportedArray = [NSMutableArray new];
  }

  // Check for dates > 28 days and remove older items.
  NSMutableIndexSet* toDelete = [[NSMutableIndexSet alloc] init];
  for (NSUInteger i = 0; i < lastReportedArray.count; i++) {
    if ([now timeIntervalSinceDate:[lastReportedArray objectAtIndex:i]] /
            (24 * 60 * 60) >
        kRangeForActivityBucketsInDays) {
      [toDelete addIndex:i];
    } else {
      break;
    }
  }

  // The count should never be < 1 for `lastReportedArray` when toDelete > 0 to
  // prevent a crash / out of bounds errors.
  if (toDelete.count > 0) {
    CHECK(lastReportedArray.count >= 1);
    [lastReportedArray removeObjectsAtIndexes:toDelete];
  }
  [defaults setObject:lastReportedArray
               forKey:kActivityBucketLastReportedDateArrayKey];

  // Check how many items in array.
  NSUInteger datesActive = lastReportedArray.count;
  switch (datesActive) {
    case 0:
      activityBucket = FeedActivityBucket::kNoActivity;
      break;
    case 1 ... 7:
      activityBucket = FeedActivityBucket::kLowActivity;
      break;
    case 8 ... 15:
      activityBucket = FeedActivityBucket::kMediumActivity;
      break;
    case 16 ... 28:
      activityBucket = FeedActivityBucket::kHighActivity;
      break;
    default:
      // This should never be reached, as dates should never be > 28 days.
      CHECK(NO);
      break;
  }
  [defaults setInteger:(int)activityBucket forKey:kActivityBucketKey];

  // Activity Buckets Daily Run.
  [self recordActivityBuckets:activityBucket];
  [defaults setObject:now forKey:kActivityBucketLastReportedDateKey];
}

// Records the engagement buckets.
- (void)recordActivityBuckets:(FeedActivityBucket)activityBucket {
  UMA_HISTOGRAM_ENUMERATION(kAllFeedsActivityBucketsHistogram, activityBucket);
}

// Records Feed engagement.
- (void)recordEngagement:(int)scrollDistance interacted:(BOOL)interacted {
  scrollDistance = abs(scrollDistance);

  // Determine if this interaction is part of a new 'session'.
  base::Time now = base::Time::Now();
  base::TimeDelta visitTimeout = base::Minutes(kMinutesBetweenSessions);
  if (now - self.sessionStartTime > visitTimeout) {
    [self finalizeSession];
  }

  // Reset the last active time for session measurement.
  self.sessionStartTime = now;

  // Report the user as engaged-simple if they have scrolled any amount or
  // interacted with the card, and it has not already been reported for this
  // Chrome run.
  if (scrollDistance > 0 || interacted) {
    [self recordEngagedSimple];
    if (GetFeedRefreshEngagementCriteriaType() ==
        FeedRefreshEngagementCriteriaType::kSimpleEngagement) {
      self.engagedWithLatestRefreshedContent = YES;
    }
  }

  // Report the user as engaged if they have scrolled more than the threshold or
  // interacted with the card, and it has not already been reported this
  // Chrome run.
  if (scrollDistance > kMinScrollThreshold || interacted) {
    [self recordEngaged];
    if (GetFeedRefreshEngagementCriteriaType() ==
        FeedRefreshEngagementCriteriaType::kEngagement) {
      self.engagedWithLatestRefreshedContent = YES;
    }
  }

  [self.sessionRecorder recordUserInteractionOrScrolling];

  // This must be called after setting `engagedWithLatestRefreshedContent`
  // properly after scrolling or interactions.
  if (IsFeedSessionCloseForegroundRefreshEnabled() &&
      [self hasEngagedWithLatestRefreshedContent]) {
    [self setOrExtendRefreshTimer];
  }
}

// Checks if a Good Visit should be recorded. `interacted` is YES if it was
// triggered by an explicit interaction. (e.g. Opening a new Tab in Incognito.)
- (void)checkEngagementGoodVisitWithInteraction:(BOOL)interacted {
  // Determine if this interaction is part of a new session.
  base::Time now = base::Time::Now();
  if ((now - self.lastInteractionTimeForGoodVisits) >
      base::Minutes(kMinutesBetweenSessions)) {
    [self resetGoodVisitSession];
  } else {
    // Check if Discover only session has expired.
    if ((now - self.lastInteractionTimeForDiscoverGoodVisits) >
        base::Minutes(kMinutesBetweenSessions)) {
      [self resetGoodVisitSessionForFeed:FeedTypeDiscover];
    }
    // Check if Following only session has expired.
    if ((now - self.lastInteractionTimeForFollowingGoodVisits) >
        base::Minutes(kMinutesBetweenSessions)) {
      [self resetGoodVisitSessionForFeed:FeedTypeFollowing];
    }
  }
  self.lastInteractionTimeForGoodVisits = now;
  if ([self.feedControlDelegate selectedFeed] == FeedTypeDiscover) {
    self.lastInteractionTimeForDiscoverGoodVisits = now;
  }
  if ([self.feedControlDelegate selectedFeed] == FeedTypeFollowing) {
    self.lastInteractionTimeForFollowingGoodVisits = now;
  }
  // If the session hasn't been reset and a GoodVisit has already been
  // reported for all possible surfaces return early as an optimization.
  if (self.goodVisitReportedDiscover && self.goodVisitReportedFollowing &&
      self.goodVisitReportedAllFeeds) {
    return;
  }

  // Report a Good Visit if any of the conditions below is YES and
  // no Good Visit has been recorded for the past `kMinutesBetweenSessions`:
  // 1. Good Explicit Interaction (add to reading list, long press, open in
  // new incognito tab ...).

  if (interacted) {
    [self recordEngagedGoodVisits:[self.feedControlDelegate selectedFeed]
                     allFeedsOnly:NO];
    return;
  }
  // 2. Good time in feed (`kGoodVisitTimeInFeedSeconds` with >= 1 scroll in an
  // entire session).
  if (([self timeSpentForCurrentGoodVisitSessionInFeed:[self.feedControlDelegate
                                                               selectedFeed]] >
       kGoodVisitTimeInFeedSeconds) &&
      self.goodVisitScroll) {
    [self recordEngagedGoodVisits:[self.feedControlDelegate selectedFeed]
                     allFeedsOnly:YES];

    // Check if Good Visit should be triggered for Discover feed.
    if (self.discoverPreviousTimeInFeedGV > kGoodVisitTimeInFeedSeconds) {
      [self recordEngagedGoodVisits:FeedTypeDiscover allFeedsOnly:NO];
    }

    // Check if Good Visit should be triggered for Following feed.
    if (self.followingPreviousTimeInFeedGV > kGoodVisitTimeInFeedSeconds) {
      [self recordEngagedGoodVisits:FeedTypeFollowing allFeedsOnly:NO];
    }
    return;
  }
}

// Records any direct interaction with the Feed, this doesn't include scrolling.
- (void)recordInteraction {
  [self recordEngagement:0 interacted:YES];
  // Log interaction for all feeds
  UMA_HISTOGRAM_ENUMERATION(kAllFeedsEngagementTypeHistogram,
                            FeedEngagementType::kFeedInteracted);

  // Log interaction for Discover feed.
  if ([self.feedControlDelegate selectedFeed] == FeedTypeDiscover) {
    UMA_HISTOGRAM_ENUMERATION(kDiscoverFeedEngagementTypeHistogram,
                              FeedEngagementType::kFeedInteracted);
  }

  // Log interaction for Following feed.
  if ([self.feedControlDelegate selectedFeed] == FeedTypeFollowing) {
    UMA_HISTOGRAM_ENUMERATION(kFollowingFeedEngagementTypeHistogram,
                              FeedEngagementType::kFeedInteracted);
  }
}

// Records simple engagement for the current `selectedFeed`.
- (void)recordEngagedSimple {
  // If neither feed has been engaged with, log "AllFeeds" simple engagement.
  if (!self.engagedSimpleReportedDiscover &&
      !self.engagedSimpleReportedFollowing) {
    UMA_HISTOGRAM_ENUMERATION(kAllFeedsEngagementTypeHistogram,
                              FeedEngagementType::kFeedEngagedSimple);
  }

  // Log simple engagment for Discover feed.
  if ([self.feedControlDelegate selectedFeed] == FeedTypeDiscover &&
      !self.engagedSimpleReportedDiscover) {
    UMA_HISTOGRAM_ENUMERATION(kDiscoverFeedEngagementTypeHistogram,
                              FeedEngagementType::kFeedEngagedSimple);
    self.engagedSimpleReportedDiscover = YES;
  }

  // Log simple engagement for Following feed.
  if ([self.feedControlDelegate selectedFeed] == FeedTypeFollowing &&
      !self.engagedSimpleReportedFollowing) {
    UMA_HISTOGRAM_ENUMERATION(kFollowingFeedEngagementTypeHistogram,
                              FeedEngagementType::kFeedEngagedSimple);
    self.engagedSimpleReportedFollowing = YES;
  }
}

// Records engagement for the currently selected feed.
- (void)recordEngaged {
  // If neither feed has been engaged with, log "AllFeeds" engagement.
  if (!self.engagedReportedDiscover && !self.engagedReportedFollowing) {
    // If the user has engaged with a feed, this is recorded as a user default.
    // This can be used for things which require feed engagement as a condition,
    // such as the top-of-feed signin promo.
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    [defaults setBool:YES forKey:kEngagedWithFeedKey];

    // Log engagement for Activity Buckets.
    [self logDailyActivity];

    UMA_HISTOGRAM_ENUMERATION(kAllFeedsEngagementTypeHistogram,
                              FeedEngagementType::kFeedEngaged);
  }

  // Log engagment for Discover feed.
  if ([self.feedControlDelegate selectedFeed] == FeedTypeDiscover &&
      !self.engagedReportedDiscover) {
    UMA_HISTOGRAM_ENUMERATION(kDiscoverFeedEngagementTypeHistogram,
                              FeedEngagementType::kFeedEngaged);
    self.engagedReportedDiscover = YES;
  }

  // Log engagement for Following feed.
  if ([self.feedControlDelegate selectedFeed] == FeedTypeFollowing &&
      !self.engagedReportedFollowing) {
    UMA_HISTOGRAM_ENUMERATION(kFollowingFeedEngagementTypeHistogram,
                              FeedEngagementType::kFeedEngaged);
    UMA_HISTOGRAM_ENUMERATION(
        kFollowingFeedSortTypeWhenEngaged,
        [self convertFollowingFeedSortTypeForHistogram:
                  [self.feedControlDelegate followingFeedSortType]]);
    self.engagedReportedFollowing = YES;

    // Log follow count when engaging with Following feed.
    // TODO(crbug.com/1322640): `followDelegate` is nil when navigating to an
    // article, since NTPCoordinator is stopped first. When this is fixed,
    // `recordFollowCount` should be called here.
  }

  // TODO(crbug.com/1322640): Separate user action for Following feed.
  base::RecordAction(base::UserMetricsAction(kDiscoverFeedUserActionEngaged));
}

// Records Good Visits for both the Following and Discover feed.
// `allFeedsOnly` will be YES when no individual feed should report a Good
// Visit, but a Good Visit should be triggered for all Feeds.
- (void)recordEngagedGoodVisits:(FeedType)feedType
                   allFeedsOnly:(BOOL)allFeedsOnly {
  // Check if the user has previously engaged with the feed in the same
  // session.
  // If neither feed has been engaged with, log "AllFeeds" engagement.
  if (!self.goodVisitReportedAllFeeds) {
    // Log for the all feeds aggregate.
    UMA_HISTOGRAM_ENUMERATION(kAllFeedsEngagementTypeHistogram,
                              FeedEngagementType::kGoodVisit);
    self.goodVisitReportedAllFeeds = YES;
  }
  if (allFeedsOnly) {
    return;
  }
  // A Good Visit for AllFeeds should have been reported in order to report feed
  // specific Good Visits.
  DCHECK(self.goodVisitReportedAllFeeds);
  // Log interaction for Discover feed.
  if (feedType == FeedTypeDiscover && !self.goodVisitReportedDiscover) {
    UMA_HISTOGRAM_ENUMERATION(kDiscoverFeedEngagementTypeHistogram,
                              FeedEngagementType::kGoodVisit);
    self.goodVisitReportedDiscover = YES;
    if (GetFeedRefreshEngagementCriteriaType() ==
        FeedRefreshEngagementCriteriaType::kGoodVisit) {
      self.engagedWithLatestRefreshedContent = YES;
    }
  }

  // Log interaction for Following feed.
  if (feedType == FeedTypeFollowing && !self.goodVisitReportedFollowing) {
    UMA_HISTOGRAM_ENUMERATION(kFollowingFeedEngagementTypeHistogram,
                              FeedEngagementType::kGoodVisit);
    self.goodVisitReportedFollowing = YES;
  }
}

// Calculates the time the user has spent in the feed during a good
// visit session.
- (NSTimeInterval)timeSpentForCurrentGoodVisitSessionInFeed:
    (FeedType)currentFeed {
  // Add the time spent since last recording.
  base::Time now = base::Time::Now();
  base::TimeDelta additionalTimeInFeed = now - self.feedBecameVisibleTime;
  self.previousTimeInFeedForGoodVisitSession =
      self.previousTimeInFeedForGoodVisitSession +
      additionalTimeInFeed.InSecondsF();

  // Calculate for specific feed.
  switch (currentFeed) {
    case FeedTypeFollowing:
      self.followingPreviousTimeInFeedGV += additionalTimeInFeed.InSecondsF();
      break;
    case FeedTypeDiscover:
      self.discoverPreviousTimeInFeedGV += additionalTimeInFeed.InSecondsF();
      break;
  }

  DCHECK(self.followingPreviousTimeInFeedGV <=
         self.previousTimeInFeedForGoodVisitSession);
  DCHECK(self.discoverPreviousTimeInFeedGV <=
         self.previousTimeInFeedForGoodVisitSession);

  return self.previousTimeInFeedForGoodVisitSession;
}

// Resets the session tracking values, this occurs if there's been
// `kMinutesBetweenSessions` minutes between sessions.
- (void)finalizeSession {
  // If simple engagement hasn't been logged, then there's no session to
  // finalize.
  if (!self.engagedSimpleReportedDiscover &&
      !self.engagedSimpleReportedFollowing) {
    return;
  }

  self.engagedReportedDiscover = NO;
  self.engagedReportedFollowing = NO;

  self.engagedSimpleReportedDiscover = NO;
  self.engagedSimpleReportedFollowing = NO;

  self.scrolledReportedDiscover = NO;
  self.scrolledReportedFollowing = NO;
}

// Resets the Good Visits session tracking values, this occurs if there's been
// kMinutesBetweenSessions minutes between sessions.
- (void)resetGoodVisitSession {
  // Reset defaults for new session.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setObject:nil forKey:kArticleVisitTimestampKey];
  [defaults setDouble:0 forKey:kLongFeedVisitTimeAggregateKey];

  base::Time now = base::Time::Now();

  self.lastInteractionTimeForGoodVisits = now;
  [defaults setObject:now.ToNSDate() forKey:kLastInteractionTimeForGoodVisits];

  self.feedBecameVisibleTime = now;

  self.goodVisitScroll = NO;

  self.goodVisitReportedAllFeeds = NO;
  // Reset individual feeds.
  [self resetGoodVisitSessionForFeed:FeedTypeFollowing];
  [self resetGoodVisitSessionForFeed:FeedTypeDiscover];
}

// Resets a Good Visit session for an individual feed. Used to allow for
// sessions to expire only for specific feeds.
- (void)resetGoodVisitSessionForFeed:(FeedType)feedType {
  base::Time now = base::Time::Now();
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  if (feedType == FeedTypeDiscover) {
    [defaults setDouble:0 forKey:kLongDiscoverFeedVisitTimeAggregateKey];
    self.lastInteractionTimeForDiscoverGoodVisits = now;
    [defaults setObject:now.ToNSDate()
                 forKey:kLastInteractionTimeForDiscoverGoodVisits];
    self.discoverPreviousTimeInFeedGV = 0;
    self.goodVisitReportedDiscover = NO;
  }
  if (feedType == FeedTypeFollowing) {
    [defaults setDouble:0 forKey:kLongFollowingFeedVisitTimeAggregateKey];
    self.lastInteractionTimeForFollowingGoodVisits = now;
    [defaults setObject:now.ToNSDate()
                 forKey:kLastInteractionTimeForFollowingGoodVisits];
    self.followingPreviousTimeInFeedGV = 0;
    self.goodVisitReportedFollowing = NO;
  }
}

// Records the time a user has spent in the feed for a day when 24hrs have
// passed.
- (void)recordTimeSpentInFeedIfDayIsDone {
  // The midnight time for the day in which the
  // `ContentSuggestions.Feed.TimeSpentInFeed` was last recorded.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSDate* lastInteractionReported = base::apple::ObjCCast<NSDate>(
      [defaults objectForKey:kLastDayTimeInFeedReportedKey]);
  base::Time lastInteractionReportedInTime;
  if (lastInteractionReported != nil) {
    lastInteractionReportedInTime =
        base::Time::FromNSDate(lastInteractionReported);
  }

  DCHECK(self.timeSpentInFeed >= base::Seconds(0));

  BOOL shouldResetData = NO;
  if (lastInteractionReported) {
    base::Time now = base::Time::Now();
    base::TimeDelta sinceDayStart = (now - lastInteractionReportedInTime);
    if (sinceDayStart >= base::Days(1)) {
      // Check if the user has spent any time in the feed.
      if (self.timeSpentInFeed > base::Seconds(0)) {
        UMA_HISTOGRAM_LONG_TIMES(kTimeSpentInFeedHistogram,
                                 self.timeSpentInFeed);
      }
      shouldResetData = YES;
    }
  } else {
    shouldResetData = YES;
  }

  if (shouldResetData) {
    lastInteractionReported =
        [NSDate dateWithTimeIntervalSince1970:base::Time::Now().ToDoubleT()];
    // Save to Defaults
    [defaults setObject:lastInteractionReported
                 forKey:kLastDayTimeInFeedReportedKey];
    // Reset time spent in feed aggregate.
    self.timeSpentInFeed = base::Seconds(0);
    [defaults setDouble:self.timeSpentInFeed.InSecondsF()
                 forKey:kTimeSpentInFeedAggregateKey];
  }
}

// Records the `duration` it took to Discover feed to perform any
// network operation.
- (void)recordNetworkRequestDuration:(base::TimeDelta)duration {
  UMA_HISTOGRAM_MEDIUM_TIMES(kDiscoverFeedNetworkDuration, duration);
}

// Records that a URL was opened regardless of the target surface (e.g. New Tab,
// Same Tab, Incognito Tab, etc.).
- (void)recordOpenURL {
  // Save the time of the open so we can then calculate how long the user spent
  // in that page.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setObject:[[NSDate alloc] init] forKey:kArticleVisitTimestampKey];
  [defaults setInteger:[self.feedControlDelegate selectedFeed]
                forKey:kLastUsedFeedForGoodVisitsKey];

  [self.NTPMetricsDelegate feedArticleOpened];

  switch ([self.feedControlDelegate selectedFeed]) {
    case FeedTypeDiscover:
      UMA_HISTOGRAM_EXACT_LINEAR(kDiscoverFeedURLOpened, 0, 1);
      break;
    case FeedTypeFollowing:
      UMA_HISTOGRAM_EXACT_LINEAR(kFollowingFeedURLOpened, 0, 1);
  }
}

// Sets or extends the refresh timer.
- (void)setOrExtendRefreshTimer {
  [self.refreshTimer invalidate];
  __weak FeedMetricsRecorder* weakSelf = self;
  self.refreshTimer = [NSTimer
      scheduledTimerWithTimeInterval:GetFeedRefreshTimerTimeoutInSeconds()
                              target:weakSelf
                            selector:@selector(refreshTimerEnded)
                            userInfo:nil
                             repeats:NO];
}

// Signals that the refresh timer ended.
- (void)refreshTimerEnded {
  [self.refreshTimer invalidate];
  self.refreshTimer = nil;
  if (!self.isNTPVisible) {
    // The feed refresher checks feed engagement criteria.
    self.feedRefresher->RefreshFeed(
        FeedRefreshTrigger::kForegroundFeedNotVisible);
  }
}

#pragma mark - Converters

// Converts a FollowingFeedSortType NSEnum into a FeedSortType enum.
- (FeedSortType)convertFollowingFeedSortTypeForHistogram:
    (FollowingFeedSortType)followingFeedSortType {
  switch (followingFeedSortType) {
    case FollowingFeedSortTypeUnspecified:
      return FeedSortType::kUnspecifiedSortType;
    case FollowingFeedSortTypeByPublisher:
      return FeedSortType::kGroupedByPublisher;
    case FollowingFeedSortTypeByLatest:
      return FeedSortType::kSortedByLatest;
  }
}

@end
