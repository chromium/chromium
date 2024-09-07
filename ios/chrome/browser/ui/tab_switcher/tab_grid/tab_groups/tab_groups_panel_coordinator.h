// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class Browser;
@class DisabledGridViewController;
@protocol DisabledGridViewControllerDelegate;
@class GridContainerViewController;
@protocol GridToolbarsMutator;
@class TabGroupsPanelMediator;
@class TabGroupsPanelViewController;

// Coordinator to manage the Tab Groups panel in Tab Grid.
@interface TabGroupsPanelCoordinator : ChromeCoordinator

// `regularBrowser` must not be null and not be OffTheRecord, aka incognito.
// Upon starting this coordinator, `toolbarsMutator` and toolbarTabGridDelegate`
// will be set on the mediator, and `disabledViewControllerDelegate` will be set
// on `disabledViewController`. `disabledViewControllerDelegate` must not be
// nil.
- (instancetype)
        initWithBaseViewController:(UIViewController*)baseViewController
                    regularBrowser:(Browser*)regularBrowser
                   toolbarsMutator:(id<GridToolbarsMutator>)toolbarsMutator
    disabledViewControllerDelegate:
        (id<DisabledGridViewControllerDelegate>)disabledViewControllerDelegate
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// The mediator managing the Tab Groups panel in Tab Grid.
@property(nonatomic, readonly, strong) TabGroupsPanelMediator* mediator;

// The view controller showing all Tab Groups.
@property(nonatomic, readonly, strong)
    TabGroupsPanelViewController* gridViewController;

// The view controller to displayed when incognito is disabled.
@property(nonatomic, readonly, strong)
    DisabledGridViewController* disabledViewController;

// The container for one of the displayed view controllers: either
// `gridViewController` or `disabledViewController`.
@property(nonatomic, readonly, strong)
    GridContainerViewController* gridContainerViewController;

// Updates the visible cells to make sure that the interval since creation is
// updated.
- (void)prepareForAppearance;

// Stops all child coordinators.
- (void)stopChildCoordinators;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_COORDINATOR_H_
