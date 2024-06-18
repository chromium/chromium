// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_METRICS_RECORDER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_METRICS_RECORDER_H_

#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"

#import "ios/chrome/browser/metrics/model/new_tab_page_uma.h"

class PrefService;

typedef NS_ENUM(NSInteger, NTPCollectionShortcutType);

@class ContentSuggestionsMostVisitedItem;
enum class ContentNotificationSnackbarEvent;
enum class ContentSuggestionsModuleType;
enum class SetUpListItemType;

// Metrics recorder for the content suggestions.
@interface ContentSuggestionsMetricsRecorder : NSObject

- (instancetype)initWithLocalState:(PrefService*)localState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Cleans up this class's saved properties before deallocation.
- (void)disconnect;

// Logs a metric for when the user taps on a module of `type` in the Magic
// Stack.
- (void)recordMagicStackModuleEngagementForType:
            (ContentSuggestionsModuleType)type
                                        atIndex:(int)index;

// Logs a metric for the "Return to Recent Tab" tile being shown.
- (void)recordReturnToRecentTabTileShown;

// Logs a metric for a shortcut tile being tapped.
- (void)recordShortcutTileTapped:(NTPCollectionShortcutType)shortcutType;

// Logs a tab resumption tab opened.
- (void)recordTabResumptionTabOpened;

// Logs the most visited tiles being shown.
- (void)recordMostVisitedTilesShown;

// Logs a single most visited tile `item` being shown at `index`.
- (void)recordMostVisitedTileShown:(ContentSuggestionsMostVisitedItem*)item
                           atIndex:(NSInteger)index;

// Logs a most visited tile `item` being opened at `index` in `webState`.
- (void)recordMostVisitedTileOpened:(ContentSuggestionsMostVisitedItem*)item
                            atIndex:(NSInteger)index;

// Logs a most visited tile being removed.
- (void)recordMostVisitedTileRemoved;

// Logs the Set Up List being shown.
- (void)recordSetUpListShown;

// Logs a Set Up List item being shown.
- (void)recordSetUpListItemShown:(SetUpListItemType)type;

// Logs a Set Up List item being selected.
- (void)recordSetUpListItemSelected:(SetUpListItemType)type;

- (void)recordContentNotificationSnackbarEvent:
    (ContentNotificationSnackbarEvent)event;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_METRICS_RECORDER_H_
