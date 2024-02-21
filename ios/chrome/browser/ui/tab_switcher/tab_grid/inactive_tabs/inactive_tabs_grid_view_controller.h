// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_GRID_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_GRID_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_view_controller.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_info_consumer.h"

@protocol InactiveTabsCoordinatorDelegate;

// A view controller that contains a grid of inactive items.
@interface InactiveGridViewController : RegularGridViewController

// Delegate for notify inactive tabs coordinator.
@property(nonatomic, weak) id<InactiveTabsCoordinatorDelegate>
    inactiveTabsDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_GRID_VIEW_CONTROLLER_H_
