// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/discover_feed_metrics_recorder.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/feed/core/v2/common_enums.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"

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

namespace {
// Histogram name for the infinite feed trigger.
const char kDiscoverFeedInfiniteFeedTriggered[] =
    "ContentSuggestions.Feed.LoadStreamStatus.LoadMore";

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

// User action names for feed header menu.
const char kDiscoverFeedUserActionManageActivityTapped[] =
    "ContentSuggestions.Feed.HeaderAction.ManageActivity";
const char kDiscoverFeedUserActionManageInterestsTapped[] =
    "ContentSuggestions.Feed.HeaderAction.ManageInterests";

// User action name for infinite feed triggering.
const char kDiscoverFeedUserActionInfiniteFeedTriggered[] =
    "ContentSuggestions.Feed.InfiniteFeedTriggered";

// Histogram name for the feed engagement types.
const char kDiscoverFeedEngagementTypeHistogram[] =
    "ContentSuggestions.Feed.EngagementType";

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

// Minimum scrolling amount to record a FeedEngagementType::kFeedEngaged due to
// scrolling.
const int kMinScrollThreshold = 160;

// Time between two metrics recorded to consider it a new session.
const int kMinutesBetweenSessions = 5;
}  // namespace

@interface DiscoverFeedMetricsRecorder ()

// Tracking property to avoid duplicate recordings of
// FeedEngagementType::kFeedEngagedSimple.
@property(nonatomic, assign) BOOL engagedSimpleReported;
// Tracking property to avoid duplicate recordings of
// FeedEngagementType::kFeedEngaged.
@property(nonatomic, assign) BOOL engagedReported;
// Tracking property to avoid duplicate recordings of
// FeedEngagementType::kFeedScrolled.
@property(nonatomic, assign) BOOL scrolledReported;
// The time when the first metric is being recorded for this session.
@property(nonatomic, assign) base::Time sessionStartTime;

@end

@implementation DiscoverFeedMetricsRecorder

#pragma mark - Public

- (void)recordFeedScrolled:(int)scrollDistance {
  DCHECK(IsDiscoverFeedEnabled());
  [self recordEngagement:scrollDistance interacted:NO];

  if (!self.scrolledReported) {
    [self recordEngagementTypeHistogram:FeedEngagementType::kFeedScrolled];
    self.scrolledReported = YES;
  }
}

- (void)recordInfiniteFeedTriggered {
  UMA_HISTOGRAM_ENUMERATION(kDiscoverFeedInfiniteFeedTriggered,
                            FeedLoadStreamStatus::kLoadedFromNetwork);
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionInfiniteFeedTriggered));
}

- (void)recordHeaderMenuLearnMoreTapped {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kTappedLearnMore];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionLearnMoreTapped));
}

- (void)recordHeaderMenuManageActivityTapped {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kTappedManageActivity];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionManageActivityTapped));
}

- (void)recordHeaderMenuManageInterestsTapped {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kTappedManageInterests];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionManageInterestsTapped));
}

- (void)recordDiscoverFeedVisibilityChanged:(BOOL)visible {
  if (visible) {
    [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                    kTappedTurnOn];
    base::RecordAction(base::UserMetricsAction(kDiscoverFeedUserActionTurnOn));
  } else {
    [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                    kTappedTurnOff];
    base::RecordAction(base::UserMetricsAction(kDiscoverFeedUserActionTurnOff));
  }
}

- (void)recordOpenURLInSameTab {
  [self
      recordDiscoverFeedUserActionHistogram:FeedUserActionType::kTappedOnCard];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionOpenSameTab));
  [self recordOpenURL];
}

- (void)recordOpenURLInNewTab {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kTappedOpenInNewTab];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionOpenNewTab));
  [self recordOpenURL];
}

- (void)recordOpenURLInIncognitoTab {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kTappedOpenInNewIncognitoTab];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionOpenIncognitoTab));
  [self recordOpenURL];
}

- (void)recordAddURLToReadLater {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kAddedToReadLater];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionReadLaterTapped));
}

- (void)recordTapSendFeedback {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kTappedSendFeedback];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionSendFeedbackOpened));
}

- (void)recordOpenBackOfCardMenu {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kOpenedContextMenu];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionContextMenuOpened));
}

- (void)recordCloseBackOfCardMenu {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kClosedContextMenu];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionCloseContextMenu));
}

- (void)recordOpenNativeBackOfCardMenu {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kOpenedNativeActionSheet];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionNativeActionSheetOpened));
}

- (void)recordShowDialog {
  [self
      recordDiscoverFeedUserActionHistogram:FeedUserActionType::kOpenedDialog];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionReportContentOpened));
}

- (void)recordDismissDialog {
  [self
      recordDiscoverFeedUserActionHistogram:FeedUserActionType::kClosedDialog];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionReportContentClosed));
}

- (void)recordDismissCard {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kEphemeralChange];
  base::RecordAction(base::UserMetricsAction(kDiscoverFeedUserActionHideStory));
}

- (void)recordUndoDismissCard {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kEphemeralChangeRejected];
}

- (void)recordCommittDismissCard {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kEphemeralChangeCommited];
}

- (void)recordShowSnackbar {
  [self
      recordDiscoverFeedUserActionHistogram:FeedUserActionType::kShowSnackbar];
}

- (void)recordCommandID:(int)commandID {
  base::UmaHistogramSparse(kDiscoverFeedUserActionCommandHistogram, commandID);
}

- (void)recordNoticeCardShown:(BOOL)shown {
  base::UmaHistogramBoolean(kDiscoverFeedNoticeCardFulfilled, shown);
}

- (void)recordFeedArticlesFetchDurationInSeconds:
            (NSTimeInterval)durationInSeconds
                                         success:(BOOL)success {
  if (success) {
    UMA_HISTOGRAM_MEDIUM_TIMES(kDiscoverFeedArticlesFetchNetworkDurationSuccess,
                               base::TimeDelta::FromSeconds(durationInSeconds));
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES(kDiscoverFeedArticlesFetchNetworkDurationFailure,
                               base::TimeDelta::FromSeconds(durationInSeconds));
  }
  [self recordNetworkRequestDurationInSeconds:durationInSeconds];
}

- (void)recordFeedMoreArticlesFetchDurationInSeconds:
            (NSTimeInterval)durationInSeconds
                                             success:(BOOL)success {
  if (success) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        kDiscoverFeedMoreArticlesFetchNetworkDurationSuccess,
        base::TimeDelta::FromSeconds(durationInSeconds));
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        kDiscoverFeedMoreArticlesFetchNetworkDurationFailure,
        base::TimeDelta::FromSeconds(durationInSeconds));
  }
  [self recordNetworkRequestDurationInSeconds:durationInSeconds];
}

- (void)recordFeedUploadActionsDurationInSeconds:
            (NSTimeInterval)durationInSeconds
                                         success:(BOOL)success {
  if (success) {
    UMA_HISTOGRAM_MEDIUM_TIMES(kDiscoverFeedUploadActionsNetworkDurationSuccess,
                               base::TimeDelta::FromSeconds(durationInSeconds));
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES(kDiscoverFeedUploadActionsNetworkDurationFailure,
                               base::TimeDelta::FromSeconds(durationInSeconds));
  }
  [self recordNetworkRequestDurationInSeconds:durationInSeconds];
}

- (void)recordNativeContextMenuVisibilityChanged:(BOOL)shown {
  if (shown) {
    [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                    kOpenedNativeContextMenu];
    base::RecordAction(base::UserMetricsAction(
        kDiscoverFeedUserActionNativeContextMenuOpened));
  } else {
    [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                    kClosedNativeContextMenu];
    base::RecordAction(base::UserMetricsAction(
        kDiscoverFeedUserActionNativeContextMenuClosed));
  }
}

- (void)recordNativePulldownMenuVisibilityChanged:(BOOL)shown {
  if (shown) {
    [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                    kOpenedNativePulldownMenu];
    base::RecordAction(base::UserMetricsAction(
        kDiscoverFeedUserActionNativePulldownMenuOpened));
  } else {
    [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                    kClosedNativePulldownMenu];
    base::RecordAction(base::UserMetricsAction(
        kDiscoverFeedUserActionNativePulldownMenuClosed));
  }
}

#pragma mark - Private

// Records histogram metrics for Discover feed user actions.
- (void)recordDiscoverFeedUserActionHistogram:(FeedUserActionType)actionType {
  UMA_HISTOGRAM_ENUMERATION(kDiscoverFeedUserActionHistogram, actionType);
  [self recordInteraction];
}

// Records Feed engagement.
- (void)recordEngagement:(int)scrollDistance interacted:(BOOL)interacted {
  scrollDistance = abs(scrollDistance);

  // Determine if this interaction is part of a new 'session'.
  base::Time now = base::Time::Now();
  base::TimeDelta visitTimeout =
      base::TimeDelta::FromMinutes(kMinutesBetweenSessions);
  if (now - self.sessionStartTime > visitTimeout) {
    [self finalizeSession];
  }
  // Reset the last active time for session measurement.
  self.sessionStartTime = now;

  // Report the user as engaged-simple if they have scrolled any amount or
  // interacted with the card, and we have not already reported it for this
  // chrome run.
  if (!self.engagedSimpleReported && (scrollDistance > 0 || interacted)) {
    [self recordEngagementTypeHistogram:FeedEngagementType::kFeedEngagedSimple];
    self.engagedSimpleReported = YES;
  }

  // Report the user as engaged if they have scrolled more than the threshold or
  // interacted with the card, and we have not already reported it this chrome
  // run.
  if (!self.engagedReported &&
      (scrollDistance > kMinScrollThreshold || interacted)) {
    [self recordEngagementTypeHistogram:FeedEngagementType::kFeedEngaged];
    self.engagedReported = YES;
  }
}

// Records any direct interaction with the Feed, this doesn't include scrolling.
- (void)recordInteraction {
  DCHECK(IsDiscoverFeedEnabled());
  [self recordEngagement:0 interacted:YES];
  [self recordEngagementTypeHistogram:FeedEngagementType::kFeedInteracted];
}

// Records Engagement histograms of |engagementType|.
- (void)recordEngagementTypeHistogram:(FeedEngagementType)engagementType {
  UMA_HISTOGRAM_ENUMERATION(kDiscoverFeedEngagementTypeHistogram,
                            engagementType);
}

// Resets the session tracking values, this occurs if there's been
// kMinutesBetweenSessions minutes between sessions.
- (void)finalizeSession {
  if (!self.engagedSimpleReported)
    return;
  self.engagedReported = NO;
  self.engagedSimpleReported = NO;
  self.scrolledReported = NO;
}

// Records the |durationInSeconds| it took to Discover feed to perform any
// network operation.
- (void)recordNetworkRequestDurationInSeconds:
    (NSTimeInterval)durationInSeconds {
  UMA_HISTOGRAM_MEDIUM_TIMES(kDiscoverFeedNetworkDuration,
                             base::TimeDelta::FromSeconds(durationInSeconds));
}

// Records that a URL was opened regardless of the target surface (e.g. New Tab,
// Same Tab, Incognito Tab, etc.)
- (void)recordOpenURL {
  // TODO(crbug.com/1174088): Add card Index and the max number of suggestions.
  UMA_HISTOGRAM_EXACT_LINEAR(kDiscoverFeedURLOpened, 0, 1);
}

@end
