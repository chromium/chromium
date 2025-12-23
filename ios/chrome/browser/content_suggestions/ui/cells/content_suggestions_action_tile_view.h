// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_CELLS_CONTENT_SUGGESTIONS_ACTION_TILE_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_CELLS_CONTENT_SUGGESTIONS_ACTION_TILE_VIEW_H_

#import "ios/chrome/browser/content_suggestions/ui/cells/content_suggestions_tile_view.h"

@class ContentSuggestionsActionItem;

// A tile view displaying an action in content suggestions section. Accepts a
// simple icon and optionally supports a badge, for example for reading list new
// item count.
@interface ContentSuggestionsActionTileView : ContentSuggestionsTileView

// View for action icon.
@property(nonatomic, strong, readonly) UIImageView* iconView;

// Container view for `countLabel`.
@property(nonatomic, strong, readonly) UIView* countContainer;

// Number shown in badge that is on the top trailing side of cell.
@property(nonatomic, strong, readonly) UILabel* countLabel;

// Configuration for this view.
@property(nonatomic, strong, readonly) ContentSuggestionsActionItem* config;

// Tap gesture recognizer for this view.
@property(nonatomic, strong) UITapGestureRecognizer* tapRecognizer;

// Initializes and configures the view with `config`.
- (instancetype)initWithConfiguration:(ContentSuggestionsActionItem*)config;

// Updates the configuration for this view to the new `config`.
- (void)updateConfiguration:(ContentSuggestionsActionItem*)config;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_CELLS_CONTENT_SUGGESTIONS_ACTION_TILE_VIEW_H_
