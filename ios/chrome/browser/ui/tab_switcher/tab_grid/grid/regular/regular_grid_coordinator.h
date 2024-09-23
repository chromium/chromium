// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_REGULAR_REGULAR_GRID_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_REGULAR_REGULAR_GRID_COORDINATOR_H_

#import "ios/chrome/browser/shared/public/commands/tabs_animation_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_coordinator+subclassing.h"

@protocol GridCommands;
@class PinnedTabsMediator;
@class PinnedTabsViewController;
@class RegularGridMediator;
@class RegularGridViewController;
@protocol TabContextMenuDelegate;

// Coordinator to manage regular grid.
@interface RegularGridCoordinator : BaseGridCoordinator <TabsAnimationCommands>

// The command handler to handle commands related to this grid. This is exposed
// to make sure other can use it.
@property(nonatomic, weak, readonly) id<GridCommands> gridHandler;

// Grid view controller.
// TODO(crbug.com/40273478): Make it private.
@property(nonatomic, readonly, strong)
    RegularGridViewController* gridViewController;
// Pinned tabs view controller.
// TODO(crbug.com/40273478): Make it private.
@property(nonatomic, strong) PinnedTabsViewController* pinnedTabsViewController;
// Regular grid mediator.
// TODO(crbug.com/40273478): Make it private.
@property(nonatomic, readonly, strong) RegularGridMediator* regularGridMediator;

// Stops all child coordinators.
- (void)stopChildCoordinators;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_REGULAR_REGULAR_GRID_COORDINATOR_H_
