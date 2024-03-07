// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_SHORTCUT_TILE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_SHORTCUT_TILE_VIEW_H_

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/shortcuts_consumer.h"

@class ContentSuggestionsMostVisitedActionItem;

// A tile view displaying a collection shortcut. Accepts a simple icon and
// optionally supports a badge, for example for reading list new item count.
@interface ContentSuggestionsShortcutTileView
    : ContentSuggestionsTileView <ShortcutsConsumer>

// Initializes and configures the view with `config`.
- (instancetype)initWithConfiguration:
    (ContentSuggestionsMostVisitedActionItem*)config;

// Updates the configuration for this view to the new `config`.
- (void)updateConfiguration:(ContentSuggestionsMostVisitedActionItem*)config;

// View for action icon.
@property(nonatomic, strong, readonly) UIImageView* iconView;

// Container view for `countLabel`.
@property(nonatomic, strong, readonly) UIView* countContainer;

// Number shown in badge that is on the top trailing side of cell.
@property(nonatomic, strong, readonly) UILabel* countLabel;

// Configuration for this view.
@property(nonatomic, strong, readonly)
    ContentSuggestionsMostVisitedActionItem* config;

// Tap gesture recognizer for this view.
@property(nonatomic, strong) UITapGestureRecognizer* tapRecognizer;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_SHORTCUT_TILE_VIEW_H_
