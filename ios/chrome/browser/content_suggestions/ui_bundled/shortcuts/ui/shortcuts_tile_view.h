// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHORTCUTS_UI_SHORTCUTS_TILE_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHORTCUTS_UI_SHORTCUTS_TILE_VIEW_H_

#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_most_visited_action_tile_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shortcuts/ui/shortcuts_consumer.h"

@class ShortcutsActionItem;

// A tile view displaying a collection shortcut. Accepts a simple icon and
// optionally supports a badge, for example for reading list new item count.
@interface ContentSuggestionsShortcutTileView
    : ContentSuggestionsMostVisitedActionTileView <ShortcutsConsumer>

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHORTCUTS_UI_SHORTCUTS_TILE_VIEW_H_
