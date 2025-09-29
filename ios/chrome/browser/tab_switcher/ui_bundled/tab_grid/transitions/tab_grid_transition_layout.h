// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_TAB_GRID_TRANSITION_LAYOUT_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_TAB_GRID_TRANSITION_LAYOUT_H_

#import <UIKit/UIKit.h>

@class TabGridTransitionItem;

// Transition layout for the tab grid.
@interface TabGridTransitionLayout : NSObject

// Active cell transition item.
@property(nonatomic, readonly) TabGridTransitionItem* activeCell;

// The currently active grid of the tab grid (regular, incognito, etc.).
@property(nonatomic, strong) UIViewController* activeGrid;

// The view controller of the pinned tabs.
@property(nonatomic, strong) UIViewController* pinnedTabs;

// Whether the active cell is from a pinned tab.
@property(nonatomic, assign) BOOL isActiveCellPinned;

// Creates a new TabGridTransitionLayout instance with the given `activeCell`
// and `activeGrid`.
+ (instancetype)layoutWithActiveCell:(TabGridTransitionItem*)activeCell
                          activeGrid:(UIViewController*)activeGrid;
@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_TAB_GRID_TRANSITION_LAYOUT_H_
