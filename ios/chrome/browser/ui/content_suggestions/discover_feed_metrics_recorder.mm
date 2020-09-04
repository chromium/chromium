// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/discover_feed_metrics_recorder.h"

#import "base/metrics/histogram_macros.h"

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
  kTappedOnCard = 0,
  kShownCard = 1,
  kTappedSendFeedback = 2,
  // Discover feed header menu 'Learn More' tapped.
  kTappedLearnMore = 3,
  kTappedHideStory = 4,
  kTappedNotInterestedIn = 5,
  // Discover feed header menu 'Manage Interests' tapped.
  kTappedManageInterests = 6,
  kTappedDownload = 7,
  kTappedOpenInNewTab = 8,
  kOpenedContextMenu = 9,
  kOpenedFeedSurface = 10,
  kTappedOpenInNewIncognitoTab = 11,
  kEphemeralChange = 12,
  kEphemeralChangeRejected = 13,
  // Discover feed visibility toggled from header menu.
  kTappedTurnOn = 14,
  kTappedTurnOff = 15,
  // Discover feed header menu 'Manage Activity' tapped.
  kTappedManageActivity = 16,
  // Highest enumerator. Recommended by Histogram metrics best practices.
  kMaxValue = kTappedManageActivity,
};

namespace {
// Histogram name for the infinite feed trigger.
const char kDiscoverFeedInfiniteFeedTriggered[] =
    "ContentSuggestions.Feed.LoadStreamStatus.LoadMore";

// Histogram name for the feed header items.
const char kDiscoverFeedHeaderItemTapped[] =
    "ContentSuggestions.Feed.UserActions";

}  // namespace

@implementation DiscoverFeedMetricsRecorder

- (void)recordInfiniteFeedTriggered {
  UMA_HISTOGRAM_ENUMERATION(kDiscoverFeedInfiniteFeedTriggered,
                            FeedLoadStreamStatus::kLoadedFromNetwork);
}

- (void)recordHeaderMenuLearnMoreTapped {
  UMA_HISTOGRAM_ENUMERATION(kDiscoverFeedHeaderItemTapped,
                            FeedUserActionType::kTappedLearnMore);
}

- (void)recordHeaderMenuManageActivityTapped {
  UMA_HISTOGRAM_ENUMERATION(kDiscoverFeedHeaderItemTapped,
                            FeedUserActionType::kTappedManageActivity);
}

- (void)recordHeaderMenuManageInterestsTapped {
  UMA_HISTOGRAM_ENUMERATION(kDiscoverFeedHeaderItemTapped,
                            FeedUserActionType::kTappedManageInterests);
}

- (void)recordDiscoverFeedVisibilityChanged:(BOOL)visible {
  if (visible) {
    UMA_HISTOGRAM_ENUMERATION(kDiscoverFeedHeaderItemTapped,
                              FeedUserActionType::kTappedTurnOn);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kDiscoverFeedHeaderItemTapped,
                              FeedUserActionType::kTappedTurnOff);
  }
}

@end
