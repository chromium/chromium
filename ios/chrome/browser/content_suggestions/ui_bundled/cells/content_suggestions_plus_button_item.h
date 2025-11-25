// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_CONTENT_SUGGESTIONS_PLUS_BUTTON_ITEM_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_CONTENT_SUGGESTIONS_PLUS_BUTTON_ITEM_H_

#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_most_visited_action_item.h"

/// The item for the "add pinned site" button at the endo of the most visited
/// tile.
@interface ContentSuggestionsPlusButtonItem
    : ContentSuggestionsMostVisitedActionItem <UIContentConfiguration>

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_CONTENT_SUGGESTIONS_PLUS_BUTTON_ITEM_H_
