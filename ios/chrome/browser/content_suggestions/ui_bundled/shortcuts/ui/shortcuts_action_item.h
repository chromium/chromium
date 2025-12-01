// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHORTCUTS_UI_SHORTCUTS_ACTION_ITEM_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHORTCUTS_UI_SHORTCUTS_ACTION_ITEM_H_

#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_most_visited_action_item.h"

// Item containing an action button in shortcuts tile in the magic stack.
@interface ShortcutsActionItem : ContentSuggestionsMostVisitedActionItem

- (nonnull instancetype)initWithCollectionShortcutType:
    (NTPCollectionShortcutType)type;

// The collection that this item acts as a shortcut for.
@property(nonatomic, assign) NTPCollectionShortcutType collectionShortcutType;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHORTCUTS_UI_SHORTCUTS_ACTION_ITEM_H_
