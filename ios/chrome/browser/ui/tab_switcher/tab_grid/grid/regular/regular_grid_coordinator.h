// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_REGULAR_REGULAR_GRID_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_REGULAR_REGULAR_GRID_COORDINATOR_H_

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_coordinator+subclassing.h"

@class PinnedTabsMediator;
@class RegularGridMediator;
@class RegularGridViewController;

// Coordinator to manage regular grid.
@interface RegularGridCoordinator : BaseGridCoordinator

// Grid view controller.
// TODO(crbug.com/1457146): Replace with RegularGridViewController when
// possible.
// TODO(crbug.com/1457146): Make it private.
@property(nonatomic, readonly, strong)
    RegularGridViewController* gridViewController;
// Regular grid mediator.
// TODO(crbug.com/1457146): Make it private.
@property(nonatomic, readonly, strong) RegularGridMediator* regularGridMediator;
// Pinned tabs mediator.
// TODO(crbug.com/1457146): Remove when it is fully moved.
@property(nonatomic, readonly, weak) PinnedTabsMediator* pinnedTabsMediator;

// Stops all child coordinators.
- (void)stopChildCoordinators;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_REGULAR_REGULAR_GRID_COORDINATOR_H_
