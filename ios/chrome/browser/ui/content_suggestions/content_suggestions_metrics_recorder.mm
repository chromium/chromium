// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ContentSuggestionsMetricsRecorder

- (void)recordReturnToRecentTabTileShown {
  base::RecordAction(
      base::UserMetricsAction("IOS.StartSurface.ShowReturnToRecentTabTile"));
}

- (void)recordShortcutTileTapped:(NTPCollectionShortcutType)shortcutType {
  switch (shortcutType) {
    case NTPCollectionShortcutTypeBookmark:
      base::RecordAction(base::UserMetricsAction("MobileNTPShowBookmarks"));
      break;
    case NTPCollectionShortcutTypeReadingList:
      base::RecordAction(base::UserMetricsAction("MobileNTPShowReadingList"));
      break;
    case NTPCollectionShortcutTypeRecentTabs:
      base::RecordAction(base::UserMetricsAction("MobileNTPShowRecentTabs"));
      break;
    case NTPCollectionShortcutTypeHistory:
      base::RecordAction(base::UserMetricsAction("MobileNTPShowHistory"));
      break;
    case NTPCollectionShortcutTypeWhatsNew:
      base::RecordAction(base::UserMetricsAction("MobileNTPShowWhatsNew"));
      break;
    case NTPCollectionShortcutTypeCount:
      NOTREACHED();
      break;
  }
}

- (void)recordTrendingQueryTappedAtIndex:(int)index {
  UMA_HISTOGRAM_ENUMERATION("IOS.TrendingQueries", index, kMaxTrendingQueries);
}

- (void)recordMostRecentTabOpened {
  base::RecordAction(
      base::UserMetricsAction("IOS.StartSurface.OpenMostRecentTab"));
}

- (void)recordMostVisitedTilesShown {
  base::RecordAction(base::UserMetricsAction("MobileNTPShowMostVisited"));
}

- (void)recordMostVisitedTileOpened {
  base::RecordAction(base::UserMetricsAction("MobileNTPMostVisited"));
}

- (void)recordMostVisitedTileRemoved {
  base::RecordAction(base::UserMetricsAction("MostVisited_UrlBlacklisted"));
}

@end
