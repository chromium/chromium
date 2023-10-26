// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_REGULAR_REGULAR_GRID_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_REGULAR_REGULAR_GRID_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class Browser;
@protocol DisabledGridViewControllerDelegate;
@class GridContainerViewController;
@protocol GridMediatorDelegate;
@protocol GridToolbarsMutator;
@class PinnedTabsMediator;
@class RegularGridMediator;
@class RegularGridViewController;
@class TabGridViewController;

// Coordinator to manage regular grid.
@interface RegularGridCoordinator : ChromeCoordinator

// Grid view controller container.
@property(nonatomic, readonly, strong)
    GridContainerViewController* gridContainerViewController;
// Grid view controller.
// TODO(crbug.com/1457146): Replace with RegularGridViewController when
// possible.
// TODO(crbug.com/1457146): Make it private.
@property(nonatomic, readonly, strong)
    RegularGridViewController* gridViewController;
// The view controller to displayed when regular is disabled.
// TODO(crbug.com/1457146): Make it private.
@property(nonatomic, readonly, strong) UIViewController* disabledViewController;
// Regular grid mediator.
// TODO(crbug.com/1457146): Make it private.
@property(nonatomic, readonly, strong) RegularGridMediator* regularGridMediator;

// The overall TabGrid.
// TODO(crbug.com/1457146): Remove this.
@property(nonatomic, weak) TabGridViewController* tabGridViewController;
// Pinned tabs mediator.
// TODO(crbug.com/1457146): Remove when it is fully moved.
@property(nonatomic, readonly, weak) PinnedTabsMediator* pinnedTabsMediator;
// Delegate for when this is presenting the Disable View Controller.
// TODO(crbug.com/1457146): This protocol should be implemented by this object.
@property(nonatomic, weak) id<DisabledGridViewControllerDelegate>
    disabledTabViewControllerDelegate;

// Init method. Parameters can't be nil.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                           toolbarsMutator:
                               (id<GridToolbarsMutator>)toolbarsMutator
                      gridMediatorDelegate:(id<GridMediatorDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Stops all child coordinators.
- (void)stopChidCoordinators;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_REGULAR_REGULAR_GRID_COORDINATOR_H_
