// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_COLLECTION_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_COLLECTION_COMMANDS_H_

#import <UIKit/UIKit.h>

// Commands for updating tab collection views.
@protocol TabCollectionCommands

// Tells the receiver to select the item with identifier `itemID`. If there is
// no item with that identifier, no change in selection should be made.
- (void)selectItemWithID:(NSString*)itemID;

// Tells the receiver to close the item with identifier `itemID`. If there is
// no pinned item with that identifier, no item is closed.
- (void)closeItemWithID:(NSString*)itemID;

// Tells the receiver to pin or unpin the tab with identifier `identifier`.
- (void)setPinState:(BOOL)pinState forItemWithIdentifier:(NSString*)identifier;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_COLLECTION_COMMANDS_H_
