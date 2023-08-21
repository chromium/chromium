// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_ACTION_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_ACTION_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_constants.h"

// Item containing a most visited action button. These buttons belong to the
// collection section as most visited items, but have static placement (the last
// four) and cannot be removed.
@interface ContentSuggestionsMostVisitedActionItem : NSObject

- (nonnull instancetype)initWithCollectionShortcutType:
    (NTPCollectionShortcutType)type;

// Text for the title of the cell.
@property(nonatomic, copy, nonnull) NSString* title;

// The accessibility label of the cell.  If none is provided, self.title is used
// as the label.
@property(nonatomic, copy, nullable) NSString* accessibilityLabel;

// The collection that this item acts as a shortcut for.
@property(nonatomic, assign) NTPCollectionShortcutType collectionShortcutType;

// Reading list count passed to the most visited cell.
@property(nonatomic, assign) NSInteger count;

// Index position of this item.
@property(nonatomic, assign) NTPCollectionShortcutType index;

// Indicate if this suggestion is (temporary) disabled.
@property(nonatomic, assign) BOOL disabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_ACTION_ITEM_H_
