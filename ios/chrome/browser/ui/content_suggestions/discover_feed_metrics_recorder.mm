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

namespace {
// Histogram name for the infinite feed trigger.
const char kDiscoverFeedInfiniteFeedTriggered[] =
    "ContentSuggestions.Feed.LoadStreamStatus.LoadMore";

}  // namespace

@implementation DiscoverFeedMetricsRecorder

- (void)recordInfiniteFeedTriggered {
  UMA_HISTOGRAM_ENUMERATION(kDiscoverFeedInfiniteFeedTriggered,
                            FeedLoadStreamStatus::kLoadedFromNetwork);
}

@end
