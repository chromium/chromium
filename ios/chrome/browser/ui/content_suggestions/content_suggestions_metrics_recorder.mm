// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/favicon_base/favicon_types.h"
#import "components/ntp_tiles/metrics.h"
#import "components/ntp_tiles/ntp_tile_impression.h"
#import "components/ntp_tiles/tile_visual_type.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/favicon/ui_bundled/favicon_attributes_with_payload.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/model/set_up_list_metrics.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/utils.h"

namespace {

const float kMaxModuleEngagementIndex = 50;

}

@implementation ContentSuggestionsMetricsRecorder {
  raw_ptr<PrefService> _localState;
}

- (instancetype)initWithLocalState:(PrefService*)localState {
  if ((self = [super init])) {
    _localState = localState;
  }
  return self;
}

- (void)disconnect {
  _localState = nullptr;
}

#pragma mark - Public

- (void)recordMagicStackModuleEngagementForType:
            (ContentSuggestionsModuleType)type
                                        atIndex:(int)index {
  UMA_HISTOGRAM_ENUMERATION(kMagicStackModuleEngagementHistogram, type);
  switch (type) {
    case ContentSuggestionsModuleType::kMostVisited:
      UMA_HISTOGRAM_EXACT_LINEAR(
          kMagicStackModuleEngagementMostVisitedIndexHistogram, index,
          kMaxModuleEngagementIndex);
      break;
    case ContentSuggestionsModuleType::kShortcuts:
      UMA_HISTOGRAM_EXACT_LINEAR(
          kMagicStackModuleEngagementShortcutsIndexHistogram, index,
          kMaxModuleEngagementIndex);
      break;
    case ContentSuggestionsModuleType::kSafetyCheck:
      UMA_HISTOGRAM_EXACT_LINEAR(
          kMagicStackModuleEngagementSafetyCheckIndexHistogram, index,
          kMaxModuleEngagementIndex);
      break;
    case ContentSuggestionsModuleType::kTabResumption:
      UMA_HISTOGRAM_EXACT_LINEAR(
          kMagicStackModuleEngagementTabResumptionIndexHistogram, index,
          kMaxModuleEngagementIndex);
      break;
    case ContentSuggestionsModuleType::kParcelTracking:
      UMA_HISTOGRAM_EXACT_LINEAR(
          kMagicStackModuleEngagementParcelTrackingIndexHistogram, index,
          kMaxModuleEngagementIndex);
      break;
    case ContentSuggestionsModuleType::kPriceTrackingPromo:
      UMA_HISTOGRAM_EXACT_LINEAR(
          kMagicStackModuleEngagementPriceTrackingPromoIndexHistogram, index,
          kMaxModuleEngagementIndex);
      break;
    case ContentSuggestionsModuleType::kSetUpListSync:
    case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
    case ContentSuggestionsModuleType::kSetUpListAutofill:
    case ContentSuggestionsModuleType::kSetUpListNotifications:
    case ContentSuggestionsModuleType::kCompactedSetUpList:
    case ContentSuggestionsModuleType::kSetUpListAllSet:
      UMA_HISTOGRAM_EXACT_LINEAR(
          kMagicStackModuleEngagementSetUpListIndexHistogram, index,
          kMaxModuleEngagementIndex);
      break;
    case ContentSuggestionsModuleType::kTipsWithProductImage:
    case ContentSuggestionsModuleType::kTips:
      UMA_HISTOGRAM_EXACT_LINEAR(kMagicStackModuleEngagementTipsIndexHistogram,
                                 index, kMaxModuleEngagementIndex);
      break;
    case ContentSuggestionsModuleType::kPlaceholder:
    case ContentSuggestionsModuleType::kInvalid:
      break;
  }
}

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
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

- (void)recordTabResumptionTabOpened {
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
                            atIndex:(NSInteger)index {
  base::RecordAction(base::UserMetricsAction(kMostVisitedAction));

  ntp_tiles::metrics::RecordTileClick(ntp_tiles::NTPTileImpression(
      index, item.source, item.titleSource,
      [self getVisualTypeFromAttributes:item.attributes],
      [self getIconTypeFromAttributes:item.attributes], item.URL));

  new_tab_page_uma::RecordNTPAction(
      false, true, new_tab_page_uma::ACTION_OPENED_MOST_VISITED_ENTRY);
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

- (void)recordContentNotificationSnackbarEvent:
    (ContentNotificationSnackbarEvent)event {
  UMA_HISTOGRAM_ENUMERATION(kContentNotificationSnackbarEventHistogram, event);
  if (event == ContentNotificationSnackbarEvent::kActionButtonTapped) {
    base::RecordAction(
        base::UserMetricsAction(kContentNotificationSnackbarAction));
  }
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
        base::apple::ObjCCastStrict<FaviconAttributesWithPayload>(attributes);
    icon_type = favicon_attributes.iconType;
  }
  return icon_type;
}

@end
