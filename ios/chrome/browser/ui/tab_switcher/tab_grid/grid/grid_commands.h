// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_COMMANDS_H_

#import <UIKit/UIKit.h>

@class GridViewController;

// Commands issued to a model backing a grid UI.
@protocol GridCommands
// Tells the receiver to insert a new item at the end of the list.
- (void)addNewItem;
// Tells the receiver to insert a new item at |index|. It is an error to call
// this method with an |index| greater than the number of items in the model.
- (void)insertNewItemAtIndex:(NSUInteger)index;
// Tells the receiver to select the item with identifier |itemID|. If there is
// no item with that identifier, no change in selection should be made.
- (void)selectItemWithID:(NSString*)itemID;
// Asks the receiver whether the item with identifier |itemID| is already
// selected.
- (BOOL)isItemWithIDSelected:(NSString*)itemID;
// Tells the receiver to move the item with identifier |itemID| to |index|. If
// there is no item with that identifier, no move should be made. It is an error
// to pass a value for |index| outside of the bounds of the items array.
- (void)moveItemWithID:(NSString*)itemID toIndex:(NSUInteger)index;
// Tells the receiver to close the item with identifier |itemID|. If there is
// no item with that identifier, no item is closed.
- (void)closeItemWithID:(NSString*)itemID;
// Tells the receiver to close the items with the identifiers in |itemIDs|.
// ItemIDs which are not associated with any item are ignored.
- (void)closeItemsWithIDs:(NSArray<NSString*>*)itemIDs;
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
// Shows an action sheet, anchored to the UIBarButtonItem, that asks for
// confirmation when 'Close All' button is tapped.
- (void)showCloseAllConfirmationActionSheetWithAnchor:
    (UIBarButtonItem*)buttonAnchor;
// Shows an action sheet, anchored to the UIBarButtonItem, that asks for
// confirmation when 'Close Items' button is tapped.
- (void)
    showCloseItemsConfirmationActionSheetWithItems:(NSArray<NSString*>*)items
                                            anchor:
                                                (UIBarButtonItem*)buttonAnchor;
// Shows a share sheet to share |items|, anchored to the |buttonAnchor|.
- (void)shareItems:(NSArray<NSString*>*)items
            anchor:(UIBarButtonItem*)buttonAnchor;
// Returns the items to display in the menu presented when the Add To button is
// selected.
- (NSArray<UIMenuElement*>*)addToButtonMenuElementsForGridViewController:
    (GridViewController*)gridViewController;
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_COMMANDS_H_
