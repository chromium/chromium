// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_REGULAR_REGULAR_GRID_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_REGULAR_REGULAR_GRID_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class Browser;
@protocol GridMediatorDelegate;
@protocol GridToolbarsMutator;
@class PinnedTabsMediator;
@class RegularGridMediator;
@class TabGridViewController;

// Coordinator to manage regular grid.
@interface RegularGridCoordinator : ChromeCoordinator

// Regular view controller.
// TODO(crbug.com/1457146): Replace this once the regular grid view controller
// is created.
@property(nonatomic, weak) TabGridViewController* regularViewController;
// Regular grid mediator.
@property(nonatomic, readonly, weak) RegularGridMediator* regularGridMediator;
// Pinned tabs mediator.
// TODO(crbug.com/1457146): Remove when it is fully moved.
@property(nonatomic, readonly, weak) PinnedTabsMediator* pinnedTabsMediator;

// Init method. Parameters can't be nil.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                           toolbarsMutator:
                               (id<GridToolbarsMutator>)toolbarsMutator
                      gridMediatorDelegate:(id<GridMediatorDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_REGULAR_REGULAR_GRID_COORDINATOR_H_
