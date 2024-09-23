// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_CONSUMER_H_

#import <Foundation/Foundation.h>

@class TabGroupsPanelItem;

// Consumer to allow the Tab Group Sync model to send information to the tab
// groups panel UI.
@protocol TabGroupsPanelConsumer

// Replace the Tab Groups panel's items with `items`.
- (void)populateItems:(NSArray<TabGroupsPanelItem*>*)items;

// Reconfigures the given Tab Groups panel's item.
- (void)reconfigureItem:(TabGroupsPanelItem*)item;

// Dismisses any modal UI which may be presented.
- (void)dismissModals;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_CONSUMER_H_
