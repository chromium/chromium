// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class InactiveTabsCoordinator;
@protocol GridCommands;
@protocol TabContextMenuProvider;

// Delegate for the coordinator.
@protocol InactiveTabsCoordinatorDelegate

// Tells the delegate that the user selected an item.
- (void)inactiveTabsCoordinator:
            (InactiveTabsCoordinator*)inactiveTabsCoordinator
            didSelectItemWithID:(NSString*)itemID;

// Tells the delegate that the coordinator should be dismissed.
- (void)inactiveTabsCoordinatorDidFinish:
    (InactiveTabsCoordinator*)inactiveTabsCoordinator;

@end

// Handles interaction with the inactive tabs view controller.
//
// This coordinator lifetime starts when the regular tab grid is started, and
// stops only when the regular tab grid is stopped. `start` creates the relevant
// objects (VC, mediator, etc.), but doesn't show the VC. Call `show`/`hide` to
// display/hide the inactive tabs grid. By having this coordinator alive, the
// mediator can react to "Close All" signals, and the VC can be re-shown as is
// (i.e. same scroll position).
@interface InactiveTabsCoordinator : ChromeCoordinator

// The GridCommands receiver handling "Close All"-related commands.
@property(nonatomic, weak, readonly) id<GridCommands> gridCommandsHandler;

// Init the inactive tabs coordinator, all parameters should *not* be nil.
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                      delegate:(id<InactiveTabsCoordinatorDelegate>)delegate
                  menuProvider:(id<TabContextMenuProvider>)menuProvider
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Animates in the grid of inactive tabs.
- (void)show;

// Animates out the grid of inactive tabs.
- (void)hide;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_COORDINATOR_H_
