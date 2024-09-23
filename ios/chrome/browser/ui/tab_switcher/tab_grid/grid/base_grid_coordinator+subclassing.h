// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_COORDINATOR_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_COORDINATOR_SUBCLASSING_H_

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_coordinator.h"

@class BaseGridMediator;
@class BaseGridViewController;
@protocol GridToolbarsMutator;
@protocol GridMediatorDelegate;
@class TabGroupCoordinator;

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
@property(nonatomic, strong, readonly) TabGroupCoordinator* tabGroupCoordinator;

// Mediator of the grid. The subclasses should implement the getter.
@property(nonatomic, strong, readonly) BaseGridMediator* mediator;

// Base Grid View Controller. The subclasses should implement the getter.
@property(nonatomic, strong, readonly)
    BaseGridViewController* gridViewController;

// Combines two transition layouts into one. The `primaryLayout` has the
// priority over `secondaryLayout`. This means that in case there are two
// activeItems and/or two selectionItems available, only the ones from
// `primaryLayout` would be picked for a combined layout.
- (LegacyGridTransitionLayout*)
    combineTransitionLayout:(LegacyGridTransitionLayout*)primaryLayout
       withTransitionLayout:(LegacyGridTransitionLayout*)secondaryLayout;

// Hides the potentially displayed Tab Group view.
- (void)hideTabGroup;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_COORDINATOR_SUBCLASSING_H_
