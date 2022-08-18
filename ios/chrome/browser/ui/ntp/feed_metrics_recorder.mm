// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_metrics_recorder.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/time/time.h"
#import "components/feed/core/v2/public/common_enums.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_metrics.h"
#import "ios/chrome/browser/ui/ntp/feed_control_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_session_recorder.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_follow_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using feed::FeedEngagementType;
using feed::FeedUserActionType;

// Values for the UMA ContentSuggestions.Feed.LoadStreamStatus.LoadMore
// histogram. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused. This must be kept
// in sync with FeedLoadStreamStatus in enums.xml.
enum class FeedLoadStreamStatus {
  kNoStatus = 0,
  kLoadedFromStore = 1,
  // Bottom of feed was reached, triggering infinite feed.
  kLoadedFromNetwork = 2,
  kFailedWithStoreError = 3,
  kNoStreamDataInStore = 4,
  kModelAlreadyLoaded = 5,
  kNoResponseBody = 6,
  kProtoTranslationFailed = 7,
  kDataInStoreIsStale = 8,
  kDataInStoreIsStaleTimestampInFuture = 9,
  kCannotLoadFromNetworkSupressedForHistoryDelete_DEPRECATED = 10,
  kCannotLoadFromNetworkOffline = 11,
  kCannotLoadFromNetworkThrottled = 12,
  kLoadNotAllowedEulaNotAccepted = 13,
  kLoadNotAllowedArticlesListHidden = 14,
  kCannotParseNetworkResponseBody = 15,
  kLoadMoreModelIsNotLoaded = 16,
  kLoadNotAllowedDisabledByEnterprisePolicy = 17,
  kNetworkFetchFailed = 18,
  kCannotLoadMoreNoNextPageToken = 19,
  // Highest enumerator. Recommended by Histogram metrics best practices.
  kMaxValue = kCannotLoadMoreNoNextPageToken,
};

// Values for the UMA ContentSuggestions.Feed.UserSettingsOnStart
// histogram. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused. This must be kept
// in sync with FeedUserSettingsOnStart in enums.xml.
// Reports last known state of user settings which affect Feed content.
// This includes WAA (whether activity is recorded), and DP (whether
// Discover personalization is enabled).
enum class UserSettingsOnStart {
  // The Feed is disabled by enterprise policy.
  kFeedNotEnabledByPolicy = 0,
  // The Feed is enabled by enterprise policy, but the user has hidden and
  // disabled the Feed, so other user settings beyond sign-in status are not
  // available.
  kFeedNotVisibleSignedOut = 1,
  kFeedNotVisibleSignedIn = 2,
  // The Feed is enabled, the user is not signed in.
  kSignedOut = 3,
  // The Feed is enabled, the user is signed in, and setting states are known.
  kSignedInWaaOnDpOn = 4,
  kSignedInWaaOnDpOff = 5,
  kSignedInWaaOffDpOn = 6,
  kSignedInWaaOffDpOff = 7,
  // The Feed is enabled, but there is no recent Feed data, so user settings
  // state is unknown.
  kSignedInNoRecentData = 8,
  // Highest enumerator. Recommended by Histogram metrics best practices.
  kMaxValue = kSignedInNoRecentData,
};

// Values for UMA ContentSuggestions.Feed.WebFeed.SortType* histograms.
// This enum is a copy of FollowingFeedSortType in feed_constants.h, used
// exclusively for metrics recording. This should always be kept in sync with
// that enum and FollowingFeedSortType in enums.xml.
enum class FeedSortType {
  // The sort type is unspecified. With the Following feed selected, this log
  // indicates a problem.
  kUnspecifiedSortType = 0,
  // The feed is grouped by publisher.
  kGroupedByPublisher = 1,
  // The feed is sorted in reverse-chronological order.
  kSortedByLatest = 2,
  // Highest enumerator. Recommended by Histogram metrics best practices.
  kMaxValue = kSortedByLatest,
};

namespace {
// User action names for the device orientation having changed.
const char kDiscoverFeedHistogramDeviceOrientationChangedToPortrait[] =
    "ContentSuggestions.Feed.DeviceOrientationChanged.Portrait";
const char kDiscoverFeedHistogramDeviceOrientationChangedToLandscape[] =
    "ContentSuggestions.Feed.DeviceOrientationChanged.Landscape";

// Histogram name for the Discover feed user actions.
const char kDiscoverFeedUserActionHistogram[] =
    "ContentSuggestions.Feed.UserActions";

// Histogram name for the Discover feed user actions commands.
const char kDiscoverFeedUserActionCommandHistogram[] =
    "ContentSuggestions.Feed.UserActions.Commands";

// User action names for toggling the feed visibility from the header menu.
const char kDiscoverFeedUserActionTurnOn[] =
    "Suggestions.ExpandableHeader.Expanded";
const char kDiscoverFeedUserActionTurnOff[] =
    "Suggestions.ExpandableHeader.Collapsed";

// User action names for feed back of card items.
const char kDiscoverFeedUserActionLearnMoreTapped[] =
    "ContentSuggestions.Feed.CardAction.LearnMore";
const char kDiscoverFeedUserActionOpenSameTab[] =
    "ContentSuggestions.Feed.CardAction.Open";
const char kDiscoverFeedUserActionOpenIncognitoTab[] =
    "ContentSuggestions.Feed.CardAction.OpenInNewIncognitoTab";
const char kDiscoverFeedUserActionOpenNewTab[] =
    "ContentSuggestions.Feed.CardAction.OpenInNewTab";
const char kDiscoverFeedUserActionReadLaterTapped[] =
    "ContentSuggestions.Feed.CardAction.ReadLater";
const char kDiscoverFeedUserActionSendFeedbackOpened[] =
    "ContentSuggestions.Feed.CardAction.SendFeedback";
const char kDiscoverFeedUserActionContextMenuOpened[] =
    "ContentSuggestions.Feed.CardAction.ContextMenu";
const char kDiscoverFeedUserActionHideStory[] =
    "ContentSuggestions.Feed.CardAction.HideStory";
const char kDiscoverFeedUserActionCloseContextMenu[] =
    "ContentSuggestions.Feed.CardAction.ClosedContextMenu";
const char kDiscoverFeedUserActionNativeActionSheetOpened[] =
    "ContentSuggestions.Feed.CardAction.OpenNativeActionSheet";
const char kDiscoverFeedUserActionNativeContextMenuOpened[] =
    "ContentSuggestions.Feed.CardAction.OpenNativeContextMenu";
const char kDiscoverFeedUserActionNativeContextMenuClosed[] =
    "ContentSuggestions.Feed.CardAction.CloseNativeContextMenu";
const char kDiscoverFeedUserActionNativePulldownMenuOpened[] =
    "ContentSuggestions.Feed.CardAction.OpenNativePulldownMenu";
const char kDiscoverFeedUserActionNativePulldownMenuClosed[] =
    "ContentSuggestions.Feed.CardAction.CloseNativePulldownMenu";
const char kDiscoverFeedUserActionReportContentOpened[] =
    "ContentSuggestions.Feed.CardAction.ReportContent";
const char kDiscoverFeedUserActionReportContentClosed[] =
    "ContentSuggestions.Feed.CardAction.ClosedReportContent";
const char kDiscoverFeedUserActionPreviewTapped[] =
    "ContentSuggestions.Feed.CardAction.TapPreview";

// User action names for feed header menu.
const char kDiscoverFeedUserActionManageTapped[] =
    "ContentSuggestions.Feed.HeaderAction.Manage";
const char kDiscoverFeedUserActionManageActivityTapped[] =
    "ContentSuggestions.Feed.HeaderAction.ManageActivity";
const char kDiscoverFeedUserActionManageInterestsTapped[] =
    "ContentSuggestions.Feed.HeaderAction.ManageInterests";
const char kDiscoverFeedUserActionManageHiddenTapped[] =
    "ContentSuggestions.Feed.HeaderAction.ManageHidden";
const char kDiscoverFeedUserActionManageFollowingTapped[] =
    "ContentSuggestions.Feed.HeaderAction.ManageFollowing";

// User action names for following operations.
const char kFollowRequested[] = "ContentSuggestions.Follow.FollowRequested";
const char kUnfollowRequested[] = "ContentSuggestions.Follow.UnfollowRequested";
const char kSnackbarGoToFeedButtonTapped[] =
    "ContentSuggestions.Follow.SnackbarGoToFeedButtonTapped";
const char kSnackbarUndoButtonTapped[] =
    "ContentSuggestions.Follow.SnackbarUndoButtonTapped";
const char kSnackbarRetryFollowButtonTapped[] =
    "ContentSuggestions.Follow.SnackbarRetryFollowButtonTapped";
const char kSnackbarRetryUnfollowButtonTapped[] =
    "ContentSuggestions.Follow.SnackbarRetryUnfollowButtonTapped";

// User action names for management surface.
const char kDiscoverFeedUserActionManagementTappedUnfollow[] =
    "ContentSuggestions.Feed.Management.TappedUnfollow";
const char
    kDiscoverFeedUserActionManagementTappedRefollowAfterUnfollowOnSnackbar[] =
        "ContentSuggestions.Feed.Management."
        "TappedRefollowAfterUnfollowOnSnackbar";
const char kDiscoverFeedUserActionManagementTappedUnfollowTryAgainOnSnackbar[] =
    "ContentSuggestions.Feed.Management.TappedUnfollowTryAgainOnSnackbar";

// User action names for first follow surface.
const char kFirstFollowGoToFeedButtonTapped[] =
    "ContentSuggestions.Follow.FirstFollow.GoToFeedButtonTapped";
const char kFirstFollowGotItButtonTapped[] =
    "ContentSuggestions.Follow.FirstFollow.GotItButtonTapped";

// User action name for engaging with feed.
const char kDiscoverFeedUserActionEngaged[] = "ContentSuggestions.Feed.Engaged";

// User action indicating that the feed will refresh.
const char kFeedWillRefresh[] = "ContentSuggestions.Feed.WillRefresh";

// User action indicating that the Discover feed was selected.
const char kDiscoverFeedSelected[] = "ContentSuggestions.Feed.Selected";

// User action indicating that the Following feed was selected.
const char kFollowingFeedSelected[] =
    "ContentSuggestions.Feed.WebFeed.Selected";

// User action triggered when the NTP view hierarchy was fixed after being
// detected as broken.
// TODO(crbug.com/1262536): Remove this when issue is fixed.
const char kNTPViewHierarchyFixed[] = "NewTabPage.ViewHierarchyFixed";

// User action triggered when a Following feed sort type selected from the sort
// menu.
const char kFollowingFeedGroupByPublisher[] =
    "ContentSuggestions.Feed.WebFeed.SortType.GroupByPublisher";
const char kFollowingFeedSortByLatest[] =
    "ContentSuggestions.Feed.WebFeed.SortType.SortByLatest";

// Histogram name for selecting Following feed sort types.
const char kFollowingFeedSortType[] =
    "ContentSuggestions.Feed.WebFeed.SortType";

// Histogram name for the feed engagement types.
const char kDiscoverFeedEngagementTypeHistogram[] =
    "ContentSuggestions.Feed.EngagementType";
const char kFollowingFeedEngagementTypeHistogram[] =
    "ContentSuggestions.Feed.WebFeed.EngagementType";
const char kAllFeedsEngagementTypeHistogram[] =
    "ContentSuggestions.Feed.AllFeeds.EngagementType";

// Histogram name for the selected sort type when engaging with the Following
// feed.
const char kFollowingFeedSortTypeWhenEngaged[] =
    "ContentSuggestions.Feed.WebFeed.SortTypeWhenEngaged";

// Histogram name for a Discover feed card shown at index.
const char kDiscoverFeedCardShownAtIndex[] =
    "NewTabPage.ContentSuggestions.Shown";

// Histogram name for a Following feed card shown at index.
const char kFollowingFeedCardShownAtIndex[] =
    "ContentSuggestions.Feed.WebFeed.Shown";

// Histogram name for a Discover feed card tapped at index.
const char kDiscoverFeedCardOpenedAtIndex[] =
    "NewTabPage.ContentSuggestions.Opened";

// Histogram name for a Following feed card tapped at index.
const char kFollowingFeedCardOpenedAtIndex[] =
    "ContentSuggestions.Feed.WebFeed.Opened";

// Histogram name to capture Feed Notice card impressions.
const char kDiscoverFeedNoticeCardFulfilled[] =
    "ContentSuggestions.Feed.NoticeCardFulfilled2";

// Histogram name to measure the time it took the Feed to fetch articles
// successfully.
const char kDiscoverFeedArticlesFetchNetworkDurationSuccess[] =
    "ContentSuggestions.Feed.Network.Duration.ArticlesFetchSuccess";

// Histogram name to measure the time it took the Feed to fetch articles
// unsuccessfully.
const char kDiscoverFeedArticlesFetchNetworkDurationFailure[] =
    "ContentSuggestions.Feed.Network.Duration.ArticlesFetchFailure";

// Histogram name to measure the time it took the Feed to fetch more articles
// successfully.
const char kDiscoverFeedMoreArticlesFetchNetworkDurationSuccess[] =
    "ContentSuggestions.Feed.Network.Duration.MoreArticlesFetchSuccess";

// Histogram name to measure the time it took the Feed to fetch more articles
// unsuccessfully.
const char kDiscoverFeedMoreArticlesFetchNetworkDurationFailure[] =
    "ContentSuggestions.Feed.Network.Duration.MoreArticlesFetchFailure";

// Histogram name to measure the time it took the Feed to upload actions
// successfully.
const char kDiscoverFeedUploadActionsNetworkDurationSuccess[] =
    "ContentSuggestions.Feed.Network.Duration.ActionUploadSuccess";

// Histogram name to measure the time it took the Feed to upload actions
// unsuccessfully.
const char kDiscoverFeedUploadActionsNetworkDurationFailure[] =
    "ContentSuggestions.Feed.Network.Duration.ActionUploadFailure";

// Histogram name to measure the time it took the Feed to perform a network
// operation.
const char kDiscoverFeedNetworkDuration[] =
    "ContentSuggestions.Feed.Network.Duration";

// Histogram name to measure opened URL's regardless of the surface they were
// opened in.
const char kDiscoverFeedURLOpened[] = "NewTabPage.ContentSuggestions.Opened";

// Histogram name to capture if the last Feed fetch had logging enabled.
const char kDiscoverFeedActivityLoggingEnabled[] =
    "ContentSuggestions.Feed.ActivityLoggingEnabled";

// Histogram name for broken NTP view hierarchy logs.
// TODO(crbug.com/1262536): Remove this when issue is fixed.
const char kDiscoverFeedBrokenNTPHierarchy[] =
    "ContentSuggestions.Feed.BrokenNTPHierarchy";

// Histogram name for the Feed settings when the App is being start.
const char kFeedUserSettingsOnStart[] =
    "ContentSuggestions.Feed.UserSettingsOnStart";

// Histogram names for logging followed publisher count after certain events.
// After showing Following feed with content.
const char kFollowCountFollowingContentShown[] =
    "ContentSuggestions.Feed.WebFeed.FollowCount.ContentShown";
// After showing Following feed without content.
const char kFollowCountFollowingNoContentShown[] =
    "ContentSuggestions.Feed.WebFeed.FollowCount.NoContentShown";
// After following a channel.
const char kFollowCountAfterFollow[] =
    "ContentSuggestions.Feed.WebFeed.FollowCount.AfterFollow";
// After unfollowing a channel.
const char kFollowCountAfterUnfollow[] =
    "ContentSuggestions.Feed.WebFeed.FollowCount.AfterUnfollow";
// After engaging with the Following feed.
const char kFollowCountWhenEngaged[] =
    "ContentSuggestions.Feed.WebFeed.FollowCount.Engaged";

// Minimum scrolling amount to record a FeedEngagementType::kFeedEngaged due to
// scrolling.
const int kMinScrollThreshold = 160;

// Time between two metrics recorded to consider it a new session.
const int kMinutesBetweenSessions = 5;

// The max amount of cards in the Discover Feed.
const int kMaxCardsInFeed = 50;

// If cached user setting info is older than this, it will not be reported.
constexpr base::TimeDelta kUserSettingsMaxAge = base::Days(14);
}  // namespace

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
  base::RecordAction(base::UserMetricsAction("MobileMenuFollow"));
}

- (void)recordUnfollowFromMenu {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kTappedUnfollowButton
                                asInteraction:NO];
  base::RecordAction(base::UserMetricsAction("MobileMenuUnfollow"));
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

// Records histogram metrics for Discover feed user actions. If |isInteraction|,
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
    UMA_HISTOGRAM_ENUMERATION("IOS.ContentSuggestions.ActionOnStartSurface",
                              IOSContentSuggestionsActionType::kFeedCard);
  } else {
    UMA_HISTOGRAM_ENUMERATION("IOS.ContentSuggestions.ActionOnNTP",
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
