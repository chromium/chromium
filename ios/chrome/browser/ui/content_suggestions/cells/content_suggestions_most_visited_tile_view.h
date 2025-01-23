// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_TILE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_TILE_VIEW_H_

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_view.h"

@protocol ContentSuggestionsMenuElementsProvider;
@class ContentSuggestionsMostVisitedItem;
@class FaviconView;
@protocol MagicStackModuleContentViewDelegate;

// NTP Tile representing a most visited website. Displays a favicon and a title.
@interface ContentSuggestionsMostVisitedTileView
    : ContentSuggestionsTileView <UIContextMenuInteractionDelegate>

// Initializes and configures the view with `config`. If `inMagicStack`, the
// view will be inside the magic stack, otherwise it will be in content
// suggestions view.
- (instancetype)initInMagicStack:(BOOL)inMagicStack
               withConfiguration:(ContentSuggestionsMostVisitedItem*)config;

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

// Delegate object to control the magic stack module. Should only be set when
// the most visited tiles resides in the magic stack.
@property(nonatomic, weak) id<MagicStackModuleContentViewDelegate>
    magicStackModuleDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_TILE_VIEW_H_
