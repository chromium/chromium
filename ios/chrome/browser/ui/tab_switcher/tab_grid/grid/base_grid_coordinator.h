// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class Browser;
@protocol DisabledGridViewControllerDelegate;
@class GridContainerViewController;
@protocol GridMediatorDelegate;
@protocol GridToolbarsMutator;
@class TabGridViewController;

@interface BaseGridCoordinator : ChromeCoordinator

// Grid view controller container.
@property(nonatomic, readonly, strong)
    GridContainerViewController* gridContainerViewController;
// The view controller to displayed when incognito is disabled.
// TODO(crbug.com/1457146): Make it private.
@property(nonatomic, readonly, strong) UIViewController* disabledViewController;
// The overall TabGrid.
// TODO(crbug.com/1457146): Remove this.
@property(nonatomic, weak) TabGridViewController* tabGridViewController;
// Delegate for when this is presenting the Disable View Controller.
// TODO(crbug.com/1457146): This protocol should be implemented by this object.
@property(nonatomic, weak) id<DisabledGridViewControllerDelegate>
    disabledTabViewControllerDelegate;

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                           toolbarsMutator:
                               (id<GridToolbarsMutator>)toolbarsMutator
                      gridMediatorDelegate:(id<GridMediatorDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_COORDINATOR_H_
