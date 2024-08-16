// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol TabContextMenuDelegate;
@protocol TabGridIdleStatusHandler;
@class TabGridModeHolder;
class TabGroup;
@protocol TabGroupPositioner;
@class TabGroupViewController;

// Coordinator to display the given tab group.
@interface TabGroupCoordinator : ChromeCoordinator

// View controller for tab groups.
@property(nonatomic, weak, readonly) TabGroupViewController* viewController;

// Whether this coordinator should be presented with animations. Default YES.
@property(nonatomic, assign) BOOL animatedPresentation;

// Handler that trackes and updates the idle status of the tab grid.
@property(nonatomic, weak) id<TabGridIdleStatusHandler>
    tabGridIdleStatusHandler;

// Tab Context Menu delegate.
@property(nonatomic, weak) id<TabContextMenuDelegate> tabContextMenuDelegate;

// Positioner providing layer information for Tab Group.
@property(nonatomic, weak) id<TabGroupPositioner> tabGroupPositioner;

// Holder for the current Tab Grid mode.
@property(nonatomic, strong) TabGridModeHolder* modeHolder;

// Init the coordinator with the tab group to display.
// - `tabGroup` should not be nil.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  tabGroup:(const TabGroup*)tabGroup
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Stops all child coordinators.
- (void)stopChildCoordinators;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_COORDINATOR_H_
