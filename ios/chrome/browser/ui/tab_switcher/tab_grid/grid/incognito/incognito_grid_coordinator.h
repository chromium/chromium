// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_INCOGNITO_INCOGNITO_GRID_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_INCOGNITO_INCOGNITO_GRID_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_mediator_delegate.h"

@protocol DisabledGridViewControllerDelegate;
@class GridContainerViewController;
@protocol GridCoordinatorAudience;
@protocol GridMediatorDelegate;
@protocol GridToolbarsMutator;
@class IncognitoGridViewController;
@class IncognitoGridMediator;
@protocol TabContextMenuDelegate;
@class TabGridViewController;

@interface IncognitoGridCoordinator
    : ChromeCoordinator <IncognitoGridMediatorDelegate>

// Grid view controller container.
@property(nonatomic, readonly, strong)
    GridContainerViewController* gridContainerViewController;
// The Grid view controller.
// TODO(crbug.com/1457146): Make it private.
@property(nonatomic, readonly, strong)
    IncognitoGridViewController* gridViewController;
// The view controller to displayed when incognito is disabled.
// TODO(crbug.com/1457146): Make it private.
@property(nonatomic, readonly, strong) UIViewController* disabledViewController;
// Incognito grid mediator.
// TODO(crbug.com/1457146): Make it private.
@property(nonatomic, readonly, strong)
    IncognitoGridMediator* incognitoGridMediator;

// Audience for this coordinator.
@property(nonatomic, weak) id<GridCoordinatorAudience> audience;

// The overall TabGrid.
// TODO(crbug.com/1457146): Remove this.
@property(nonatomic, weak) TabGridViewController* tabGridViewController;
// TODO(crbug.com/1457146): This protocol should be implemented by this object.
// Delegate for when this is presenting the Disable View Controller.
@property(nonatomic, weak) id<DisabledGridViewControllerDelegate>
    disabledTabViewControllerDelegate;
// Tab Context Menu delegate.
// TODO(crbug.com/1457146): This protocol should be implemented by this object.
@property(nonatomic, weak) id<TabContextMenuDelegate> tabContextMenuDelegate;

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                           toolbarsMutator:
                               (id<GridToolbarsMutator>)toolbarsMutator
                      gridMediatorDelegate:(id<GridMediatorDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// The incognito browser can be reset during the execution of the app.
- (void)setIncognitoBrowser:(Browser*)incognitoBrowser;

// Stops all child coordinators.
- (void)stopChidCoordinators;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_INCOGNITO_INCOGNITO_GRID_COORDINATOR_H_
