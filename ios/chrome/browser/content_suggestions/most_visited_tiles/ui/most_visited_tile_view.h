// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILE_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILE_VIEW_H_

#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_item.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/content_suggestions_tile_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_updating.h"

@protocol ContentSuggestionsMenuElementsProvider;
@class FaviconView;
@protocol MagicStackModuleContentViewDelegate;

// NTP Tile representing a most visited website. Displays a favicon and a title.
@interface MostVisitedTileView
    : ContentSuggestionsTileView <NewTabPageColorUpdating,
                                  UIContentView,
                                  UIContextMenuInteractionDelegate>

// Initializes and configures the view with `config`.
- (instancetype)initWithConfiguration:(MostVisitedItem*)config;

// FaviconView displaying the favicon.
@property(nonatomic, strong, readonly) FaviconView* faviconView;

// Provider of menu actions for this tile.
@property(nonatomic, weak) id<ContentSuggestionsMenuElementsProvider>
    menuElementsProvider;

// Tap gesture recognizer for this view.
@property(nonatomic, strong) UITapGestureRecognizer* tapRecognizer;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILE_VIEW_H_
