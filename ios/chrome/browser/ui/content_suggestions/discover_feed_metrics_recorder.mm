// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/discover_feed_metrics_recorder.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

// Values for the UMA ContentSuggestions.Feed.UserActions
// histogram. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused. This must be kept
// in sync with FeedUserActionType in enums.xml.
enum class FeedUserActionType {
  // User tapped on card, opening the article in the same tab.
  kTappedOnCard = 0,
  kShownCard = 1,
  // User tapped on 'Send Feedback' in the back of card menu.
  kTappedSendFeedback = 2,
  // Discover feed header menu 'Learn More' tapped.
  kTappedLearnMore = 3,
  kTappedHideStory = 4,
  kTappedNotInterestedIn = 5,
  // Discover feed header menu 'Manage Interests' tapped.
  kTappedManageInterests = 6,
  kTappedDownload = 7,
  // User opened the article in a new tab from the back of card menu.
  kTappedOpenInNewTab = 8,
  kOpenedContextMenu = 9,
  kOpenedFeedSurface = 10,
  // User opened the article in an incognito tab from the back of card menu.
  kTappedOpenInNewIncognitoTab = 11,
  kEphemeralChange = 12,
  kEphemeralChangeRejected = 13,
  // Discover feed visibility toggled from header menu.
  kTappedTurnOn = 14,
  kTappedTurnOff = 15,
  // Discover feed header menu 'Manage Activity' tapped.
  kTappedManageActivity = 16,
  // User added article to 'Read Later' list.
  kAddedToReadLater = 17,
  // Highest enumerator. Recommended by Histogram metrics best practices.
  kMaxValue = kAddedToReadLater,
};

// Values for the UMA ContentSuggestions.Feed.EngagementType
// histogram. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused. This must be kept
// in sync with FeedEngagementType in enums.xml.
enum class FeedEngagementType {
  kFeedEngaged = 0,
  kFeedEngagedSimple = 1,
  kFeedInteracted = 2,
  kDeprecatedFeedScrolled = 3,
  kFeedScrolled = 4,
  kMaxValue = kFeedScrolled,
};

namespace {
// Histogram name for the infinite feed trigger.
const char kDiscoverFeedInfiniteFeedTriggered[] =
    "ContentSuggestions.Feed.LoadStreamStatus.LoadMore";

// Histogram name for the Discover feed user actions.
const char kDiscoverFeedUserActionHistogram[] =
    "ContentSuggestions.Feed.UserActions";

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
}

- (void)recordOpenURLInNewTab {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kTappedOpenInNewTab];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionOpenNewTab));
}

- (void)recordOpenURLInIncognitoTab {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kTappedOpenInNewIncognitoTab];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionOpenIncognitoTab));
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

@end
