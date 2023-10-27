// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_COMMANDS_H_

#import <UIKit/UIKit.h>

#import <set>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_collection_commands.h"

namespace web {
class WebStateID;
}  // namespace web

// Commands issued to a model backing a grid UI.
@protocol GridCommands <TabCollectionCommands>
// Tells the receiver to insert a new item at the end of the list.
- (void)addNewItem;
// Tells the receiver to insert a new item at `index`. It is an error to call
// this method with an `index` greater than the number of items in the model.
- (void)insertNewItemAtIndex:(NSUInteger)index;
// Tells the receiver to select the item with identifier `itemID`. If there is
// no item with that identifier, no change in selection should be made.
- (BOOL)isItemWithIDSelected:(web::WebStateID)itemID;
// Tells the receiver to move the item with identifier `itemID` to `index`. If
// there is no item with that identifier, no move should be made. It is an error
// to pass a value for `index` outside of the bounds of the items array.
- (void)moveItemWithID:(web::WebStateID)itemID toIndex:(NSUInteger)index;
// Tells the receiver to close the items with the identifiers in `itemIDs`.
// ItemIDs which are not associated with any item are ignored.
- (void)closeItemsWithIDs:(const std::set<web::WebStateID>&)itemIDs;
// Tells the receiver to close all items.
- (void)closeAllItems;
// Tells the receiver to save all items for an undo operation, then close all
// items.
- (void)saveAndCloseAllItems;
// Tells the receiver to restore saved closed items, and then discard the saved
// items. If there are no saved closed items, this is a no-op.
- (void)undoCloseAllItems;
// Tells the receiver to discard saved closed items. If the consumer has saved
// closed items, it will discard them. Otherwise, this is a no-op.
- (void)discardSavedClosedItems;
// Returns the menu to display when the Add To button is selected for `items`.
- (NSArray<UIMenuElement*>*)addToButtonMenuElementsForItems:
    (const std::set<web::WebStateID>&)itemIDs;

// Tells the receiver to perform a search using `searchText` and update the list
// of visible items based on the result.
- (void)searchItemsWithText:(NSString*)searchText;

// Tells the receiver to reset grid to contain all the items and select the
// active item.
- (void)resetToAllItems;

// Tells the receiver to fetch the search history results count for `searchText`
// and provide it to the `completion` block.
- (void)fetchSearchHistoryResultsCountForText:(NSString*)searchText
                                   completion:(void (^)(size_t))completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_COMMANDS_H_
