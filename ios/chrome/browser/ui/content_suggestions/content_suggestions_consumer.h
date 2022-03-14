// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSUMER_H_

@class ContentSuggestionsMostVisitedActionItem;
@class ContentSuggestionsMostVisitedItem;
@class ContentSuggestionsReturnToRecentTabItem;
@class ContentSuggestionsWhatsNewItem;

// Supports adding/removing/updating UI elements to the ContentSuggestions
// UIViewController.
@protocol ContentSuggestionsConsumer

// Indicates to the consumer to present the WhatsNew tile with |config|.
- (void)showWhatsNewViewWithConfig:(ContentSuggestionsWhatsNewItem*)config;

// Indicates to the consumer to hide the WhatsNew tile.
- (void)hideWhatsNewView;

// Indicates to the consumer to present the Return to Recent Tab tile with
// |config|.
- (void)showReturnToRecentTabTileWithConfig:
    (ContentSuggestionsReturnToRecentTabItem*)config;

// Indicates to the consumer to hide the Return to Recent Tab tile.
- (void)hideReturnToRecentTabTile;

// Indicates to the consumer the current Most Visited tiles to show with
// |configs|.
- (void)setMostVisitedTilesWithConfigs:
    (NSArray<ContentSuggestionsMostVisitedItem*>*)configs;

// Indicates to the consumer the current Shortcuts tiles to show with |configs|.
- (void)setShortcutTilesWithConfigs:
    (NSArray<ContentSuggestionsMostVisitedActionItem*>*)configs;

// Indicates to the consumer to update the Reading List count badge with
// |count|.
- (void)updateReadingListCount:(NSInteger)count;

// Indicates to the consumer update the Most Visited tile associated with
// |config|.
- (void)updateMostVisitedTileConfig:(ContentSuggestionsMostVisitedItem*)config;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSUMER_H_
