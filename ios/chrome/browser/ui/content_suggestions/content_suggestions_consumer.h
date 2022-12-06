// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSUMER_H_

@class ContentSuggestionsMostVisitedActionItem;
@class ContentSuggestionsMostVisitedItem;
@class ContentSuggestionsReturnToRecentTabItem;
@class ContentSuggestionsWhatsNewItem;
@class QuerySuggestionConfig;

// Supports adding/removing/updating UI elements to the ContentSuggestions
// UIViewController.
@protocol ContentSuggestionsConsumer

// Indicates to the consumer to present the Return to Recent Tab tile with
// `config`.
- (void)showReturnToRecentTabTileWithConfig:
    (ContentSuggestionsReturnToRecentTabItem*)config;

// Indicates to the consumer to update the Return to Recent Tab tile with
// `config`.
- (void)updateReturnToRecentTabTileWithConfig:
    (ContentSuggestionsReturnToRecentTabItem*)config;

// Indicates to the consumer to hide the Return to Recent Tab tile.
- (void)hideReturnToRecentTabTile;

// Indicates to the consumer the current Most Visited tiles to show with
// `configs`.
- (void)setMostVisitedTilesWithConfigs:
    (NSArray<ContentSuggestionsMostVisitedItem*>*)configs;

// Indicates to the consumer the current Shortcuts tiles to show with `configs`.
- (void)setShortcutTilesWithConfigs:
    (NSArray<ContentSuggestionsMostVisitedActionItem*>*)configs;

// Indicates to the consumer the current Trending Queries to show with
// `configs`.
- (void)setTrendingQueriesWithConfigs:(NSArray<QuerySuggestionConfig*>*)configs;

// Indicates to the consumer that the given `config` has updated data.
- (void)updateShortcutTileConfig:
    (ContentSuggestionsMostVisitedActionItem*)config;

// Indicates to the consumer update the Most Visited tile associated with
// `config`.
- (void)updateMostVisitedTileConfig:(ContentSuggestionsMostVisitedItem*)config;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSUMER_H_
