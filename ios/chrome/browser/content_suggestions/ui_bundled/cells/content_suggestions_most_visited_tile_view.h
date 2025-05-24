// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_TILE_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_TILE_VIEW_H_

#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_tile_view.h"

@protocol ContentSuggestionsMenuElementsProvider;
@class ContentSuggestionsMostVisitedItem;
@class FaviconView;
@protocol MagicStackModuleContentViewDelegate;

// NTP Tile representing a most visited website. Displays a favicon and a title.
@interface ContentSuggestionsMostVisitedTileView
    : ContentSuggestionsTileView <UIContextMenuInteractionDelegate>

// Initializes and configures the view with `config`.
- (instancetype)initWithConfiguration:
    (ContentSuggestionsMostVisitedItem*)config;

// FaviconView displaying the favicon.
@property(nonatomic, strong, readonly) FaviconView* faviconView;

// Provider of menu actions for this tile.
@property(nonatomic, weak) id<ContentSuggestionsMenuElementsProvider>
    menuElementsProvider;

// Tap gesture recognizer for this view.
@property(nonatomic, strong) UITapGestureRecognizer* tapRecognizer;

// Configuration for this view.
@property(nonatomic, strong, readonly)
    ContentSuggestionsMostVisitedItem* config;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_TILE_VIEW_H_
