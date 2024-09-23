// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_REGULAR_REGULAR_GRID_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_REGULAR_REGULAR_GRID_VIEW_CONTROLLER_H_

#import <map>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller+subclassing.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_info_consumer.h"

@protocol GridCommands;
namespace tab_groups {
class TabGroupId;
}  // namespace tab_groups

// A view controller that contains a grid of regular items.
@interface RegularGridViewController
    : BaseGridViewController <InactiveTabsInfoConsumer>

// Triggers the tabs closure animation on the tab grid for the WebStates in
// `tabsToClose`, for the groups in `groupsWithTabsToClose`, and if
// `animateAllInactiveTabs` is true, then for the inactive tabs banner. It also
// triggers the closure of the selected WebStates through `completionHandler`
// after running the animation.
- (void)animateTabsClosureForTabs:(std::set<web::WebStateID>)tabsToClose
                           groups:
                               (std::map<tab_groups::TabGroupId, std::set<int>>)
                                   groupsWithTabsToClose
                  allInactiveTabs:(BOOL)animateAllInactiveTabs
                completionHandler:(ProceduralBlock)completionHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_REGULAR_REGULAR_GRID_VIEW_CONTROLLER_H_
