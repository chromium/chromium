// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_PINNED_TAB_COLLECTION_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_PINNED_TAB_COLLECTION_CONSUMER_H_

#import <Foundation/Foundation.h>

@class TabSwitcherItem;
namespace web {
class WebStateID;
}  // namespace web

// Supports idempotent insert/delete/updates tabs to a collection view.
@protocol PinnedTabCollectionConsumer <NSObject>

// Many of the following methods pass a `selectedItemID` as a parameter,
// indicating the identifier of the item that should be in the selected state
// after the method is called. In every such case, a nil `selectedItemID`
// indicates that no item should be selected (typically because there are no
// items). It is up to the consumer to determine how to handle a
// `selectedItemID` that is not the identifier of any current items.

// Tells the consumer to replace its current set of items with `items` and
// update the selected item ID to be `selectedItemID`. It's an error to pass
// an `items` array containing items without unique IDs.
- (void)populateItems:(NSArray<TabSwitcherItem*>*)items
       selectedItemID:(web::WebStateID)selectedItemID;

// Tells the consumer to insert `item` at `index` and update the selected item
// ID to be `selectedItemID`. It's an error if `item`'s ID duplicates an
// ID already passed to the consumer (and not yet removed).
- (void)insertItem:(TabSwitcherItem*)item
           atIndex:(NSUInteger)index
    selectedItemID:(web::WebStateID)selectedItemID;

// Tells the consumer to remove the item with ID `removedItemID` and update the
// selected item ID to be `selectedItemID`.
- (void)removeItemWithID:(web::WebStateID)removedItemID
          selectedItemID:(web::WebStateID)selectedItemID;

// Tells the consumer to update the selected item ID to be `selectedItemID`.
- (void)selectItemWithID:(web::WebStateID)selectedItemID;

// Tells the consumer to replace the item with ID `itemID` with `item`.
// It's an error if `item`'s ID duplicates any other item's ID besides `itemID`.
// The consumer should ignore this call if `itemID` has not yet been inserted.
- (void)replaceItemID:(web::WebStateID)itemID withItem:(TabSwitcherItem*)item;

// Tells the consumer to move the item with id `itemID` to `toIndex`. Note that
// the ID of the selected item isn't changed by this method, although the index
// of that item might be.
- (void)moveItemWithID:(web::WebStateID)itemID toIndex:(NSUInteger)toIndex;

// Dismisses any presented modal UI.
- (void)dismissModals;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_PINNED_TAB_COLLECTION_CONSUMER_H_
