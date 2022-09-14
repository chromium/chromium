// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_recorder.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/time/time.h"
#import "components/feed/core/v2/public/common_enums.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_metrics.h"
#import "ios/chrome/browser/ui/ntp/feed_control_delegate.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_session_recorder.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_follow_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
// The time when the first metric is being recorded for this session.
@property(nonatomic, assign) base::Time sessionStartTime;

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

- (void)recordFeedScrolled:(int)scrollDistance {
  [self recordEngagement:scrollDistance interacted:NO];

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

- (void)recordCardShownAtIndex:(int)index {
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

- (void)recordCardTappedAtIndex:(int)index {
  switch ([self.feedControlDelegate selectedFeed]) {
    case FeedTypeDiscover:
      UMA_HISTOGRAM_EXACT_LINEAR(kDiscoverFeedCardOpenedAtIndex, index,
                                 kMaxCardsInFeed);
      break;
    case FeedTypeFollowing:
      UMA_HISTOGRAM_EXACT_LINEAR(kFollowingFeedCardOpenedAtIndex, index,
                                 kMaxCardsInFeed);
  }
}

- (void)recordNoticeCardShown:(BOOL)shown {
  base::UmaHistogramBoolean(kDiscoverFeedNoticeCardFulfilled, shown);
}

- (void)recordFeedArticlesFetchDurationInSeconds:
            (NSTimeInterval)durationInSeconds
                                         success:(BOOL)success {
  if (success) {
    UMA_HISTOGRAM_MEDIUM_TIMES(kDiscoverFeedArticlesFetchNetworkDurationSuccess,
                               base::Seconds(durationInSeconds));
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES(kDiscoverFeedArticlesFetchNetworkDurationFailure,
                               base::Seconds(durationInSeconds));
  }
  [self recordNetworkRequestDurationInSeconds:durationInSeconds];
}

- (void)recordFeedMoreArticlesFetchDurationInSeconds:
            (NSTimeInterval)durationInSeconds
                                             success:(BOOL)success {
  if (success) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        kDiscoverFeedMoreArticlesFetchNetworkDurationSuccess,
        base::Seconds(durationInSeconds));
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        kDiscoverFeedMoreArticlesFetchNetworkDurationFailure,
        base::Seconds(durationInSeconds));
  }
  [self recordNetworkRequestDurationInSeconds:durationInSeconds];
}

- (void)recordFeedUploadActionsDurationInSeconds:
            (NSTimeInterval)durationInSeconds
                                         success:(BOOL)success {
  if (success) {
    UMA_HISTOGRAM_MEDIUM_TIMES(kDiscoverFeedUploadActionsNetworkDurationSuccess,
                               base::Seconds(durationInSeconds));
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES(kDiscoverFeedUploadActionsNetworkDurationFailure,
                               base::Seconds(durationInSeconds));
  }
  [self recordNetworkRequestDurationInSeconds:durationInSeconds];
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
}

- (void)recordFeedSelected:(FeedType)feedType {
  DCHECK(self.followDelegate);
  switch (feedType) {
    case FeedTypeDiscover:
      [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                      kDiscoverFeedSelected
                                    asInteraction:NO];
      base::RecordAction(base::UserMetricsAction(kDiscoverFeedSelected));
      break;
    case FeedTypeFollowing:
      [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                      kFollowingFeedSelected
                                    asInteraction:NO];
      base::RecordAction(base::UserMetricsAction(kFollowingFeedSelected));
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
  // interacted with the card, and we have not already reported it for this
  // chrome run.
  if (scrollDistance > 0 || interacted) {
    [self recordEngagedSimple];
  }

  // Report the user as engaged if they have scrolled more than the threshold or
  // interacted with the card, and we have not already reported it this chrome
  // run.
  if (scrollDistance > kMinScrollThreshold || interacted) {
    [self recordEngaged];
  }

  [self.sessionRecorder recordUserInteractionOrScrolling];
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
    // If the user has engaged with a feed, we record this as a user default.
    // This can be used for things which require feed engagement as a condition,
    // such as the top-of-feed signin promo.
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    [defaults setBool:YES forKey:kEngagedWithFeedKey];

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
    // article, since NTPCoordinator is stopped first. When this is fixed, we
    // should call `recordFollowCount` here.
  }

  // TODO(crbug.com/1322640): Separate user action for Following feed
  base::RecordAction(base::UserMetricsAction(kDiscoverFeedUserActionEngaged));
}

// Resets the session tracking values, this occurs if there's been
// kMinutesBetweenSessions minutes between sessions.
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

// Records the `durationInSeconds` it took to Discover feed to perform any
// network operation.
- (void)recordNetworkRequestDurationInSeconds:
    (NSTimeInterval)durationInSeconds {
  UMA_HISTOGRAM_MEDIUM_TIMES(kDiscoverFeedNetworkDuration,
                             base::Seconds(durationInSeconds));
}

// Records that a URL was opened regardless of the target surface (e.g. New Tab,
// Same Tab, Incognito Tab, etc.)
- (void)recordOpenURL {
  if (self.isShownOnStartSurface) {
    UMA_HISTOGRAM_ENUMERATION(kActionOnStartSurface,
                              IOSContentSuggestionsActionType::kFeedCard);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kActionOnNTP,
                              IOSContentSuggestionsActionType::kFeedCard);
  }

  // TODO(crbug.com/1174088): Add card Index and the max number of suggestions.
  UMA_HISTOGRAM_EXACT_LINEAR(kDiscoverFeedURLOpened, 0, 1);
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
