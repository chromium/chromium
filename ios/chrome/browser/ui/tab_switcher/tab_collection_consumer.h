// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_COLLECTION_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_COLLECTION_CONSUMER_H_

#import <Foundation/Foundation.h>

@class GridItemIdentifier;
enum class TabGridMode;

namespace web {
class WebStateID;
}  // namespace web

// Supports idempotent insert/delete/updates tabs to a collection view.
@protocol TabCollectionConsumer <NSObject>

// Sets the mode of the grid.
- (void)setTabGridMode:(TabGridMode)mode;

// Many of the following methods pass a `selectedItemID` as a parameter,
// indicating the identifier of the item that should be in the selected state
// after the method is called. In every such case, a nil `selectedItemID`
// indicates that no item should be selected (typically because there are no
// items). It is up to the consumer to determine how to handle a
// `selectedItemID` that is not the identifier of any current items.

// Tells the consumer to replace its current set of items with `items` and
// update the selected item to be `selectedItem`. It's an error to pass an
// `items` array containing items without unique IDs.
- (void)populateItems:(NSArray<GridItemIdentifier*>*)items
    selectedItemIdentifier:(GridItemIdentifier*)selectedItemIdentifier;

// Tells the consumer to insert `item` before the given `nextItemIdentifier` and
// update the selected item identifier to be `selectedItemIdentifier`. It's an
// error if `item`'s duplicates an item already passed to the consumer (and not
// yet removed). `nextItemIdentifier` can be nul, `item` will be append to the
// end of the section.
- (void)insertItem:(GridItemIdentifier*)item
              beforeItemID:(GridItemIdentifier*)nextItemIdentifier
    selectedItemIdentifier:(GridItemIdentifier*)selectedItemIdentifier;

// Tells the consumer to remove the item with identifier `removedItem` and
// update the selected item identifier to be `selectedItemIdentifier`.
- (void)removeItemWithIdentifier:(GridItemIdentifier*)removedItem
          selectedItemIdentifier:(GridItemIdentifier*)selectedItemIdentifier;

// Tells the consumer to update the selected item identifier to be
// `selectedItemIdentifier`.
- (void)selectItemWithIdentifier:(GridItemIdentifier*)selectedItemIdentifier;

// Tells the consumer to replace the GridItemIdentifier `item` with
// `replacementItem`. The consumer should ignore this call if `item`'s ID has
// not yet been inserted. Note that the type of item should be either `Tab` or
// `Group`.
- (void)replaceItem:(GridItemIdentifier*)item
    withReplacementItem:(GridItemIdentifier*)replacementItem;

// Tells the consumer to move the `item` before `nextItemIdentifier`. If
// `nextItemIdentifier` is nil, `item` is moved to the end of the collection
// view. Note that the selected item isn't changed by this method, although the
// index of that item might be.
- (void)moveItem:(GridItemIdentifier*)item
      beforeItem:(GridItemIdentifier*)nextItemIdentifier;

// Brings `item` into view (in the visible elements of the grid).
- (void)bringItemIntoView:(GridItemIdentifier*)item animated:(BOOL)animated;

// Dismisses any presented modal UI.
- (void)dismissModals;

// Notifies the grid that all items will be closed.
- (void)willCloseAll;

// Notifies the grid that all items have been closed.
- (void)didCloseAll;

// Notifies the grid that all closed items will be restored.
- (void)willUndoCloseAll;

// Notifies the grid that all closed items have been restored.
- (void)didUndoCloseAll;

// Reloads the view.
- (void)reload;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_COLLECTION_CONSUMER_H_
