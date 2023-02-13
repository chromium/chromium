// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@class InactiveTabsCoordinator;

// Delegate for the coordinator.
@protocol InactiveTabsCoordinatorDelegate

// Tells the delegate that the coordinator should be dismissed.
- (void)inactiveTabsCoordinatorDidFinish:
    (InactiveTabsCoordinator*)inactiveTabsCoordinator;

@end

// Handles interaction with the inactive tabs view controller.
@interface InactiveTabsCoordinator : ChromeCoordinator

// Delegate for dismissing the coordinator.
@property(nonatomic, weak) id<InactiveTabsCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_COORDINATOR_H_
