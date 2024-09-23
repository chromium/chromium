// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_SHARED_METRICS_FEED_METRICS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_NTP_SHARED_METRICS_FEED_METRICS_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import "base/time/time.h"

// If cached user setting info is older than this, it will not be reported.
constexpr base::TimeDelta kUserSettingsMaxAge = base::Days(14);

// Minimum scrolling amount to record a FeedEngagementType::kFeedEngaged due to
// scrolling.
extern const int kMinScrollThreshold;

// Time spent by the user in feed to consider it a
// FeedEngagementType::kFeedGoodVisit.
extern const int kGoodVisitTimeInFeedSeconds;

// Minimum time spent in an article to be considered a non-short click. A
// non-short click is any click on an article lasting more than the value
// assigned to this constant. Calculated when back in the feed.
extern const int kNonShortClickSeconds;

// Time between two metrics recorded to consider it a new session.
extern const int kMinutesBetweenSessions;

// The max amount of cards in the Discover Feed.
extern const int kMaxCardsInFeed;

// Stores the time when the user visits an article on the feed.
extern const char kArticleVisitTimestampKey[];
// Stores the time elapsed on the feed when the user leaves.
extern const char kLongFeedVisitTimeAggregateKey[];
extern const char kLongFollowingFeedVisitTimeAggregateKey[];
extern const char kLongDiscoverFeedVisitTimeAggregateKey[];
extern const char kLastUsedFeedForGoodVisitsKey[];
// Stores the last interaction time for Good Visits (NSDate).
extern const char kLastInteractionTimeForGoodVisits[];
extern const char kLastInteractionTimeForDiscoverGoodVisits[];
extern const char kLastInteractionTimeForFollowingGoodVisits[];
// Stores the last day the Time in Feed was reported on UMA. It stores the
// midnight (beginning of the day) of the last interaction.
extern const char kLastDayTimeInFeedReportedKey[];
// Stores the time spent on the feed for a day.
extern const char kTimeSpentInFeedAggregateKey[];
// Stores the last time the activity bucket was reported.
extern const char kActivityBucketLastReportedDateKey[];
// Stores the last 28 days of activity bucket reported days.
extern const char kActivityBucketLastReportedDateArrayKey[];

#pragma mark - Enums

// DO NOT CHANGE. Values are from enums.xml representing what could be broken in
// the NTP view hierarchy. These values are persisted to logs. Entries should
// not be renumbered and numeric values should never be reused.
enum class BrokenNTPHierarchyRelationship {
  kContentSuggestionsParent = 0,
  kELMCollectionParent = 1,
  kDiscoverFeedParent = 2,
  kDiscoverFeedWrapperParent = 3,
  kContentSuggestionsReset = 4,
  kFeedHeaderParent = 5,
  kContentSuggestionsHeaderParent = 6,

  // Change this to match max value.
  kMaxValue = 6,
};

// Values from enums.xml that represent the triggers where feed refreshes are
// requested. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class FeedRefreshTrigger {
  kOther = 0,
  kBackgroundColdStart = 1,
  kBackgroundWarmStart = 2,
  kForegroundFeedStart = 3,
  kForegroundAccountChange = 4,
  kForegroundUserTriggered = 5,
  kForegroundFeedVisibleOther = 6,
  kForegroundNotForced = 7,
  kForegroundFeedNotVisible = 8,
  kForegroundNewFeedViewController = 9,
  kForegroundAppClose = 10,
  kBackgroundColdStartAppClose = 11,
  kBackgroundWarmStartAppClose = 12,

  // Change this to match max value.
  kMaxValue = kBackgroundWarmStartAppClose,
};

// Enum class contains values indicating the type of follow request. Ex.
// kFollowRequestFollow means the user has sent a request to follow a website.
enum class FollowRequestType {
  kFollowRequestFollow = 0,
  kFollowRequestUnfollow = 1,

  // Change this to match max value.
  kMaxValue = kFollowRequestUnfollow,
};

// Enum class contains values indicating the type of follow confirmation type.
// Ex. kFollowSucceedSnackbarShown means a confirmation is shown after the user
// has successfully followed a website.
enum class FollowConfirmationType {
  kFollowSucceedSnackbarShown = 0,
  kFollowErrorSnackbarShown = 1,
  kUnfollowSucceedSnackbarShown = 2,
  kUnfollowErrorSnackbarShown = 3,

  // Change this to match max value.
  kMaxValue = kUnfollowErrorSnackbarShown,
};

// Enum class contains values indicating the type of snackbar action button.
enum class FollowSnackbarActionType {
  kSnackbarActionGoToFeed = 0,
  kSnackbarActionUndo = 1,
  kSnackbarActionRetryFollow = 2,
  kSnackbarActionRetryUnfollow = 3,

  // Change this to match max value.
  kMaxValue = kSnackbarActionRetryUnfollow,
};

// Enum class for the times when we log the user's follow count.
// To be kept in sync with the ContentSuggestions.Feed.WebFeed.FollowCount
// variants.
typedef NS_ENUM(NSInteger, FollowCountLogReason) {
  FollowCountLogReasonContentShown = 0,
  FollowCountLogReasonNoContentShown,
  FollowCountLogReasonAfterFollow,
  FollowCountLogReasonAfterUnfollow,
  FollowCountLogReasonEngaged
};

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

// The values for the Feed Activity Buckets metric.
enum class FeedActivityBucket {
  // No activity bucket for users active 0/28 days.
  kNoActivity = 0,
  // Low activity bucket for users active 1-7/28 days.
  kLowActivity = 1,
  // Medium activity bucket for users active 8-15/28 days.
  kMediumActivity = 2,
  // High activity bucket for users active 16+/28 days.
  kHighActivity = 3,
  // Highest enumerator. Recommended by Histogram metrics best practices.
  kMaxValue = kHighActivity,
};

#pragma mark - Histograms

// Histogram name for the Time Spent in Feed.
extern const char kTimeSpentInFeedHistogram[];
// Histogram name for the Discover feed user actions.
extern const char kDiscoverFeedUserActionHistogram[];

// Histogram name for the Discover feed user actions commands.
extern const char kDiscoverFeedUserActionCommandHistogram[];

// Histogram name for the feed engagement types.
extern const char kDiscoverFeedEngagementTypeHistogram[];
extern const char kFollowingFeedEngagementTypeHistogram[];
extern const char kAllFeedsEngagementTypeHistogram[];

// Histogram name for the feed activity bucket metric.
extern const char kAllFeedsActivityBucketsHistogram[];

// Histogram name for a Discover feed card shown at index.
extern const char kDiscoverFeedCardShownAtIndex[];

// Histogram name for a Following feed card shown at index.
extern const char kFollowingFeedCardShownAtIndex[];

// Histogram name to capture Feed Notice card impressions.
extern const char kDiscoverFeedNoticeCardFulfilled[];

// Histogram name to measure the time it took the Feed to fetch articles
// successfully.
extern const char kDiscoverFeedArticlesFetchNetworkDurationSuccess[];

// Histogram name to measure the time it took the Feed to fetch articles
// unsuccessfully.
extern const char kDiscoverFeedArticlesFetchNetworkDurationFailure[];

// Histogram name to measure the time it took the Feed to fetch more articles
// successfully.
extern const char kDiscoverFeedMoreArticlesFetchNetworkDurationSuccess[];

// Histogram name to measure the time it took the Feed to fetch more articles
// unsuccessfully.
extern const char kDiscoverFeedMoreArticlesFetchNetworkDurationFailure[];

// Histogram name to measure the time it took the Feed to upload actions
// successfully.
extern const char kDiscoverFeedUploadActionsNetworkDurationSuccess[];

// Histogram name to measure the time it took the Feed to upload actions
// unsuccessfully.
extern const char kDiscoverFeedUploadActionsNetworkDurationFailure[];

// Histogram name to measure the time it took the Feed to perform a network
// operation.
extern const char kDiscoverFeedNetworkDuration[];

// Histogram name to track opened articles from the Discover feed.
extern const char kDiscoverFeedURLOpened[];

// Histogram name to track opened articles from the Following feed.
extern const char kFollowingFeedURLOpened[];

// Histogram name to capture if the last Feed fetch had logging enabled.
extern const char kDiscoverFeedActivityLoggingEnabled[];

// Histogram name for broken NTP view hierarchy logs.
// TODO(crbug.com/40799579): Remove this when issue is fixed.
extern const char kDiscoverFeedBrokenNTPHierarchy[];

// Histogram name for triggers causing feed refreshes.
extern const char kDiscoverFeedRefreshTrigger[];

// Histogram name for the value of Discover uniformity flag.
extern const char kDiscoverUniformityFlag[];

// Histogram name for the Feed settings when the App is being start.
extern const char kFeedUserSettingsOnStart[];

// Histogram name for selecting Following feed sort types.
extern const char kFollowingFeedSortType[];

// Histogram name for the selected sort type when engaging with the Following
// feed.
extern const char kFollowingFeedSortTypeWhenEngaged[];

// Histogram names for logging followed publisher count after certain events.
// After showing Following feed with content.
extern const char kFollowCountFollowingContentShown[];
// After showing Following feed without content.
extern const char kFollowCountFollowingNoContentShown[];
// After following a channel.
extern const char kFollowCountAfterFollow[];
// After unfollowing a channel.
extern const char kFollowCountAfterUnfollow[];

// Histogram name for last visible card when switching from Discover to
// Following feed.
extern const char kDiscoverIndexWhenSwitchingFeed[];
// Histogram name for last visible card when switching from Following to
// Discover feed.
extern const char kFollowingIndexWhenSwitchingFeed[];

// Histogram name for sign-in related UI triggered by Feed entry points.
extern const char kFeedSignInUI[];

// Histogram name for Feed sync related UI triggered by Feed entry points.
extern const char kFeedSyncPromo[];

#pragma mark - User Actions

// User action names for the device orientation having changed.
extern const char kDiscoverFeedHistogramDeviceOrientationChangedToPortrait[];
extern const char kDiscoverFeedHistogramDeviceOrientationChangedToLandscape[];

// User action names for toggling the feed visibility from the header menu.
extern const char kDiscoverFeedUserActionTurnOn[];
extern const char kDiscoverFeedUserActionTurnOff[];

// User action names for feed back of card items.
extern const char kDiscoverFeedUserActionLearnMoreTapped[];
extern const char kDiscoverFeedUserActionOpenSameTab[];
extern const char kDiscoverFeedUserActionOpenIncognitoTab[];
extern const char kDiscoverFeedUserActionOpenNewTab[];
extern const char kDiscoverFeedUserActionReadLaterTapped[];
extern const char kDiscoverFeedUserActionSendFeedbackOpened[];
extern const char kDiscoverFeedUserActionContextMenuOpened[];
extern const char kDiscoverFeedUserActionHideStory[];
extern const char kDiscoverFeedUserActionCloseContextMenu[];
extern const char kDiscoverFeedUserActionNativeActionSheetOpened[];
extern const char kDiscoverFeedUserActionNativeContextMenuOpened[];
extern const char kDiscoverFeedUserActionNativeContextMenuClosed[];
extern const char kDiscoverFeedUserActionNativePulldownMenuOpened[];
extern const char kDiscoverFeedUserActionNativePulldownMenuClosed[];
extern const char kDiscoverFeedUserActionReportContentOpened[];
extern const char kDiscoverFeedUserActionReportContentClosed[];
extern const char kDiscoverFeedUserActionPreviewTapped[];

// User action names for feed header menu.
extern const char kDiscoverFeedUserActionManageTapped[];
extern const char kDiscoverFeedUserActionManageActivityTapped[];
extern const char kDiscoverFeedUserActionManageInterestsTapped[];
extern const char kDiscoverFeedUserActionManageHiddenTapped[];
extern const char kDiscoverFeedUserActionManageFollowingTapped[];

// User action names for following operations.
extern const char kFollowRequested[];
extern const char kUnfollowRequested[];
extern const char kSnackbarGoToFeedButtonTapped[];
extern const char kSnackbarUndoButtonTapped[];
extern const char kSnackbarRetryFollowButtonTapped[];
extern const char kSnackbarRetryUnfollowButtonTapped[];

// User action names for management surface.
extern const char kDiscoverFeedUserActionManagementTappedUnfollow[];
extern const char
    kDiscoverFeedUserActionManagementTappedRefollowAfterUnfollowOnSnackbar[];
extern const char
    kDiscoverFeedUserActionManagementTappedUnfollowTryAgainOnSnackbar[];

// User action names for first follow surface.
extern const char kFirstFollowGoToFeedButtonTapped[];
extern const char kFirstFollowGotItButtonTapped[];

// User action name for engaging with feed.
extern const char kDiscoverFeedUserActionEngaged[];

// User action indicating that the feed will refresh.
extern const char kFeedWillRefresh[];

// User action indicating that the Discover feed was selected.
extern const char kDiscoverFeedSelected[];

// User action indicating that the Following feed was selected.
extern const char kFollowingFeedSelected[];

// User action triggered when the NTP view hierarchy was fixed after being
// detected as broken.
// TODO(crbug.com/40799579): Remove this when issue is fixed.
extern const char kNTPViewHierarchyFixed[];

// User actions for following and unfollowing publishers from the overflow menu.
extern const char kFollowFromMenu[];
extern const char kUnfollowFromMenu[];

// User action triggered when a Following feed sort type selected from the sort
// menu.
extern const char kFollowingFeedGroupByPublisher[];
extern const char kFollowingFeedSortByLatest[];

#pragma mark - User Actions for Feed Sign-in Promo

// User actions triggered when a user taps on Feed Back of Card menu
// personalization options when not signed in.
extern const char kShowFeedSignInOnlyUIWithUserId[];
extern const char kShowFeedSignInOnlyUIWithoutUserId[];

// User actions triggered when a user taps on Feed personalization controls and
// a corresponding sign-in related UI is shown. Ex. a sign-in only flow, or a
// disabled toast is shown.
extern const char kShowSignInOnlyFlowFromFeed[];
extern const char kShowSignInDisableToastFromFeed[];

#pragma mark - User Actions for Feed Sync Promo

// User actions triggered when a user taps on the Feed sync promo and a sync
// related UI is shown.
extern const char kShowSyncFlowFromFeed[];
extern const char kShowDisableToastFromFeed[];

#endif  // IOS_CHROME_BROWSER_NTP_SHARED_METRICS_FEED_METRICS_CONSTANTS_H_
