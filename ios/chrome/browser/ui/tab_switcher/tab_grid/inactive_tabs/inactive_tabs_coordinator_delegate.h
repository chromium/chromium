// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_COORDINATOR_DELEGATE_H_

@class InactiveTabsCoordinator;
namespace web {
class WebStateID;
}  // namespace web

// Delegate for the coordinator.
@protocol InactiveTabsCoordinatorDelegate

// Tells the delegate that the user selected an item.
- (void)inactiveTabsCoordinator:
            (InactiveTabsCoordinator*)inactiveTabsCoordinator
            didSelectItemWithID:(web::WebStateID)itemID;

// Tells the delegate that the coordinator should be dismissed.
- (void)inactiveTabsCoordinatorDidFinish:
    (InactiveTabsCoordinator*)inactiveTabsCoordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_COORDINATOR_DELEGATE_H_
