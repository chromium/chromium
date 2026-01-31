// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILES_PLUS_BUTTON_ITEM_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILES_PLUS_BUTTON_ITEM_H_

#import "ios/chrome/browser/content_suggestions/ui/cells/content_suggestions_action_item.h"

@protocol MostVisitedTilesCommands;

/// The item for the "add pinned site" button at the end of the most visited
/// tile.
@interface MostVisitedTilesPlusButtonItem
    : ContentSuggestionsActionItem <UIContentConfiguration>

/// Command handler for the plus button.
@property(nonatomic, weak) id<MostVisitedTilesCommands> mostVisitedTilesHandler;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILES_PLUS_BUTTON_ITEM_H_
