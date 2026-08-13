// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILES_PLUS_BUTTON_TILE_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILES_PLUS_BUTTON_TILE_VIEW_H_

#import "ios/chrome/browser/content_suggestions/ui/cells/content_suggestions_action_tile_view.h"

/// The cell content view of the add pinned site button. It is subclassed from
/// the most-visited-action button so it shares the same color theme with the
/// shortcuts tiles.
@interface MostVisitedTilesPlusButtonTileView
    : ContentSuggestionsActionTileView <UIContentView>

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILES_PLUS_BUTTON_TILE_VIEW_H_
