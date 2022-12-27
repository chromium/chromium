// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_PINNED_TABS_PINNED_TABS_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_PINNED_TABS_PINNED_TABS_COMMANDS_H_

#import <UIKit/UIKit.h>

// Protocol used to update the model layer.
@protocol PinnedTabsCommands

// Tells the receiver to select the item with identifier `itemID`. If there is
// no item with that identifier, no change in selection should be made.
- (void)selectItemWithID:(NSString*)itemID;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_PINNED_TABS_PINNED_TABS_COMMANDS_H_
