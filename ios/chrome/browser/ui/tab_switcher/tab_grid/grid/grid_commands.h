// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_COMMANDS_H_

#import <UIKit/UIKit.h>

#import <set>

class TabGroup;

namespace web {
class WebStateID;
}  // namespace web

// Commands issued to a model backing a grid UI.
@protocol GridCommands
// Tells the receiver to insert a new item at the end of the list. Return YES if
// it inserted an element, NO otherwise.
- (BOOL)addNewItem;
// Tells the receiver to select the item with identifier `itemID`. If there is
// no item with that identifier, no change in selection should be made.
- (BOOL)isItemWithIDSelected:(web::WebStateID)itemID;
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

// Tells the receiver to select the item with identifier `itemID`. If there is
// no item with that identifier, no change in selection should be made. `pinned`
// is `YES` If the selected item is a pinned item. `isFirstActionOnTabGrid` is
// whether the itme selection is the first action that happens since the user
// enters tab grid.
- (void)selectItemWithID:(web::WebStateID)itemID
                    pinned:(BOOL)pinned
    isFirstActionOnTabGrid:(BOOL)isFirstActionOnTabGrid;

// Tells the receiver to select the `tabGroup`.
- (void)selectTabGroup:(const TabGroup*)tabGroup;

// Tells the receiver to close the item with identifier `itemID`. If there is
// no item with that identifier, no item is closed.
- (void)closeItemWithID:(web::WebStateID)itemID;

// Tells the receiver to close the `group`.
- (void)closeTabGroup:(const TabGroup*)group;

// Tells the receiver to ungroup the `group`.
- (void)ungroupTabGroup:(const TabGroup*)group;

// Tells the receiver to pin or unpin the tab with identifier `itemID`.
- (void)setPinState:(BOOL)pinState forItemWithID:(web::WebStateID)itemID;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_COMMANDS_H_
