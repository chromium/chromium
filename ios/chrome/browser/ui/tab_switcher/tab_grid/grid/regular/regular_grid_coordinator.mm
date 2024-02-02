// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_coordinator.h"

#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/tabs/model/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/disabled_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_container_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_empty_state_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_theme.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_context_menu_helper.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_view_controller.h"

@interface RegularGridCoordinator ()

// Redefined as readwrite.
@property(nonatomic, readwrite, strong)
    RegularGridViewController* gridViewController;

@end

@implementation RegularGridCoordinator {
  // Mediator of regular grid.
  RegularGridMediator* _mediator;
  // Mediator for pinned Tabs.
  PinnedTabsMediator* _pinnedTabsMediator;
  // Context menu provider.
  TabContextMenuHelper* _contextMenuProvider;
}

#pragma mark - Property Implementation.

- (RegularGridMediator*)regularGridMediator {
  CHECK(_mediator)
      << "RegularGridCoordinator's -start should be called before.";
  return _mediator;
}

- (PinnedTabsMediator*)pinnedTabsMediator {
  CHECK(_pinnedTabsMediator)
      << "RegularGridCoordinator's -start should be called before.";
  return _pinnedTabsMediator;
}

- (id<GridCommands>)gridHandler {
  CHECK(_mediator);
  return _mediator;
}

#pragma mark - ChromeCoordinator

- (void)start {
  BOOL regularModeEnabled =
      !IsIncognitoModeForced(self.browser->GetBrowserState()->GetPrefs());

  _contextMenuProvider = [[TabContextMenuHelper alloc]
        initWithBrowserState:self.browser->GetBrowserState()
      tabContextMenuDelegate:self.tabContextMenuDelegate];

  GridContainerViewController* container =
      [[GridContainerViewController alloc] init];
  self.gridContainerViewController = container;

  RegularGridViewController* gridViewController;
  if (regularModeEnabled) {
    gridViewController = [[RegularGridViewController alloc] init];
    container.containedViewController = gridViewController;
  } else {
    DisabledGridViewController* disabledViewController =
        [[DisabledGridViewController alloc]
            initWithPage:TabGridPageRegularTabs];
    disabledViewController.delegate = self.disabledTabViewControllerDelegate;
    self.disabledViewController = disabledViewController;
    container.containedViewController = self.disabledViewController;
  }

  self.gridViewController = gridViewController;

  _mediator = [[RegularGridMediator alloc] init];
  _mediator.consumer = gridViewController;
  _mediator.browser = self.browser;
  _mediator.delegate = self.gridMediatorDelegate;
  _mediator.toolbarsMutator = self.toolbarsMutator;
  _mediator.dispatcher = self;

  gridViewController.dragDropHandler = _mediator;
  gridViewController.mutator = _mediator;
  gridViewController.gridProvider = _mediator;
  gridViewController.menuProvider = _contextMenuProvider;

  // If regular is enabled then the grid exists and it is not disabled.
  // TODO(crbug.com/1457146): Get disabled status from the mediator.
  if (gridViewController) {
    gridViewController.dragDropHandler = _mediator;
    // TODO(crbug.com/1457146): Move the following lines to the grid itself when
    // specific grid file will be created.
    gridViewController.view.accessibilityIdentifier = kRegularTabGridIdentifier;
    gridViewController.emptyStateView =
        [[TabGridEmptyStateView alloc] initWithPage:TabGridPageRegularTabs];
    gridViewController.emptyStateView.accessibilityIdentifier =
        kTabGridRegularTabsEmptyStateIdentifier;
    gridViewController.theme = GridThemeLight;

    self.gridContainerViewController.containedViewController =
        gridViewController;
  }

  if (IsPinnedTabsEnabled()) {
    PinnedTabsViewController* pinnedTabsViewController =
        [[PinnedTabsViewController alloc] init];
    self.pinnedTabsViewController = pinnedTabsViewController;

    _pinnedTabsMediator =
        [[PinnedTabsMediator alloc] initWithConsumer:pinnedTabsViewController];

    _pinnedTabsMediator.browser = self.browser;
    pinnedTabsViewController.menuProvider = _contextMenuProvider;
    pinnedTabsViewController.dragDropHandler = _pinnedTabsMediator;
  }

  [super start];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;

  _pinnedTabsMediator = nil;
  _contextMenuProvider = nil;

  [super stop];
}

#pragma mark - Public

- (void)stopChildCoordinators {
  [self.gridViewController dismissModals];
  [self.pinnedTabsViewController dismissModals];
}

@end
