// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/favicon_base/favicon_types.h"
#import "components/ntp_tiles/metrics.h"
#import "components/ntp_tiles/ntp_tile_impression.h"
#import "components/ntp_tiles/tile_visual_type.h"
#import "ios/chrome/browser/ntp/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/set_up_list_metrics.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_constants.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes_with_payload.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ContentSuggestionsMetricsRecorder

#pragma mark - Public

- (void)recordReturnToRecentTabTileShown {
  base::RecordAction(base::UserMetricsAction(kShowReturnToRecentTabTileAction));
}

- (void)recordShortcutTileTapped:(NTPCollectionShortcutType)shortcutType {
  switch (shortcutType) {
    case NTPCollectionShortcutTypeBookmark:
      base::RecordAction(base::UserMetricsAction(kShowBookmarksAction));
      break;
    case NTPCollectionShortcutTypeReadingList:
      base::RecordAction(base::UserMetricsAction(kShowReadingListAction));
      break;
    case NTPCollectionShortcutTypeRecentTabs:
      base::RecordAction(base::UserMetricsAction(kShowRecentTabsAction));
      break;
    case NTPCollectionShortcutTypeHistory:
      base::RecordAction(base::UserMetricsAction(kShowHistoryAction));
      break;
    case NTPCollectionShortcutTypeWhatsNew:
      base::RecordAction(base::UserMetricsAction(kShowWhatsNewAction));
      break;
    case NTPCollectionShortcutTypeCount:
      NOTREACHED();
      break;
  }
}

- (void)recordTrendingQueryTappedAtIndex:(int)index {
  UMA_HISTOGRAM_ENUMERATION(kTrendingQueriesHistogram, index,
                            kMaxTrendingQueries);
}

- (void)recordMostRecentTabOpened {
  base::RecordAction(base::UserMetricsAction(kOpenMostRecentTabAction));
}

- (void)recordMostVisitedTilesShown {
  base::RecordAction(base::UserMetricsAction(kShowMostVisitedAction));
}

- (void)recordMostVisitedTileShown:(ContentSuggestionsMostVisitedItem*)item
                           atIndex:(NSInteger)index {
  ntp_tiles::metrics::RecordTileImpression(ntp_tiles::NTPTileImpression(
      index, item.source, item.titleSource,
      [self getVisualTypeFromAttributes:item.attributes],
      [self getIconTypeFromAttributes:item.attributes], item.URL));
}

- (void)recordMostVisitedTileOpened:(ContentSuggestionsMostVisitedItem*)item
                            atIndex:(NSInteger)index
                           webState:(web::WebState*)webState {
  base::RecordAction(base::UserMetricsAction(kMostVisitedAction));

  ntp_tiles::metrics::RecordTileClick(ntp_tiles::NTPTileImpression(
      index, item.source, item.titleSource,
      [self getVisualTypeFromAttributes:item.attributes],
      [self getIconTypeFromAttributes:item.attributes], item.URL));

  new_tab_page_uma::RecordAction(
      false, webState, new_tab_page_uma::ACTION_OPENED_MOST_VISITED_ENTRY);
}

- (void)recordMostVisitedTileRemoved {
  base::RecordAction(base::UserMetricsAction(kMostVisitedUrlBlacklistedAction));
}

- (void)recordSetUpListShown {
  set_up_list_metrics::RecordDisplayed();
}

- (void)recordSetUpListItemShown:(SetUpListItemType)type {
  set_up_list_metrics::RecordItemDisplayed(type);
}

- (void)recordSetUpListItemSelected:(SetUpListItemType)type {
  set_up_list_metrics::RecordItemSelected(type);
}

#pragma mark - Private

// Returns the visual type of a favicon for metrics logging.
- (ntp_tiles::TileVisualType)getVisualTypeFromAttributes:
    (FaviconAttributes*)attributes {
  if (!attributes) {
    return ntp_tiles::TileVisualType::NONE;
  } else if (attributes.faviconImage) {
    return ntp_tiles::TileVisualType::ICON_REAL;
  }
  return attributes.defaultBackgroundColor
             ? ntp_tiles::TileVisualType::ICON_DEFAULT
             : ntp_tiles::TileVisualType::ICON_COLOR;
}

// Returns the icon type of a favicon for metrics logging.
- (favicon_base::IconType)getIconTypeFromAttributes:
    (FaviconAttributes*)attributes {
  favicon_base::IconType icon_type = favicon_base::IconType::kInvalid;
  if (attributes.faviconImage) {
    FaviconAttributesWithPayload* favicon_attributes =
        base::mac::ObjCCastStrict<FaviconAttributesWithPayload>(attributes);
    icon_type = favicon_attributes.iconType;
  }
  return icon_type;
}

@end
