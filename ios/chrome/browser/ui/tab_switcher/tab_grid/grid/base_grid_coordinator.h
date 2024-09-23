// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/create_or_edit_tab_group_coordinator_delegate.h"

class Browser;
@protocol DisabledGridViewControllerDelegate;
@class GridContainerViewController;
@protocol GridMediatorDelegate;
@protocol GridToolbarsMutator;
@class LegacyGridTransitionLayout;
@protocol TabContextMenuDelegate;
@class TabGridModeHolder;
@protocol TabGroupPositioner;

@interface BaseGridCoordinator
    : ChromeCoordinator <CreateOrEditTabGroupCoordinatorDelegate,
                         TabGroupsCommands>

// Grid view controller container.
@property(nonatomic, readonly, strong)
    GridContainerViewController* gridContainerViewController;
// The view controller to displayed when incognito is disabled.
// TODO(crbug.com/40273478): Make it private.
@property(nonatomic, readonly, strong) UIViewController* disabledViewController;
// Delegate for when this is presenting the Disable View Controller.
// TODO(crbug.com/40273478): This protocol should be implemented by this object.
@property(nonatomic, weak) id<DisabledGridViewControllerDelegate>
    disabledTabViewControllerDelegate;
// Delegate for the context menu.
// TODO(crbug.com/40273478): Make it private.
@property(nonatomic, weak) id<TabContextMenuDelegate> tabContextMenuDelegate;

// Positioner providing layer information for Tab Group.
@property(nonatomic, weak) id<TabGroupPositioner> tabGroupPositioner;

// Holder for the current Tab Grid mode.
@property(nonatomic, strong) TabGridModeHolder* modeHolder;

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                           toolbarsMutator:
                               (id<GridToolbarsMutator>)toolbarsMutator
                      gridMediatorDelegate:(id<GridMediatorDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Shows the TabGroup view while the TabGrid is being opened at the same time.
- (void)showTabGroupForTabGridOpening:(const TabGroup*)tabGroup;

// Brings the `tabGroup` into view by making it part of the visible element of
// its grid, if present. Returns whether the group was present.
- (BOOL)bringTabGroupIntoViewIfPresent:(const TabGroup*)tabGroup
                              animated:(BOOL)animated;

// Returns the transition layout for this grid.
- (LegacyGridTransitionLayout*)transitionLayout;

// Returns whether the selected cell is visible.
- (BOOL)isSelectedCellVisible;

// The view displaying the grid. Used for layout purpose.
- (UIView*)gridView;

// Returns the container for the grid, to be used during transitions.
- (UIView*)gridContainerForAnimation;

// Stops all child coordinators.
- (void)stopChildCoordinators;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_COORDINATOR_H_
