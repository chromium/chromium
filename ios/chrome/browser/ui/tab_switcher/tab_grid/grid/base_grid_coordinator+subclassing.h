// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_COORDINATOR_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_COORDINATOR_SUBCLASSING_H_

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_coordinator.h"

@protocol GridToolbarsMutator;
@protocol GridMediatorDelegate;

@interface BaseGridCoordinator ()

// Redefined as readwrite.
@property(nonatomic, strong, readwrite)
    UIViewController* disabledViewController;
@property(nonatomic, strong, readwrite)
    GridContainerViewController* gridContainerViewController;

@end

@interface BaseGridCoordinator (Subclassing)

// Make it accessible from subclass
@property(nonatomic, weak, readonly) id<GridToolbarsMutator> toolbarsMutator;
@property(nonatomic, weak, readonly) id<GridMediatorDelegate>
    gridMediatorDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_COORDINATOR_SUBCLASSING_H_
