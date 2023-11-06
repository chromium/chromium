// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_coordinator.h"

#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
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
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_view_controller.h"

@interface RegularGridCoordinator ()

// Redefined as readwrite.
@property(nonatomic, readwrite, strong)
    RegularGridViewController* gridViewController;
@property(nonatomic, readwrite, strong)
    UIViewController* disabledViewController;
@property(nonatomic, readwrite, strong)
    GridContainerViewController* gridContainerViewController;

@end

@implementation RegularGridCoordinator {
  // Mediator of regular grid.
  RegularGridMediator* _mediator;
  // Mutator that handles toolbars changes.
  __weak id<GridToolbarsMutator> _toolbarsMutator;
  // Delegate to handle presenting the action sheet.
  __weak id<GridMediatorDelegate> _gridMediatorDelegate;
  // Mediator for pinned Tabs.
  PinnedTabsMediator* _pinnedTabsMediator;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                           toolbarsMutator:
                               (id<GridToolbarsMutator>)toolbarsMutator
                      gridMediatorDelegate:(id<GridMediatorDelegate>)delegate {
  CHECK(baseViewController);
  CHECK(browser);
  if (self = [super initWithBaseViewController:baseViewController
                                       browser:browser]) {
    CHECK(toolbarsMutator);
    CHECK(delegate);
    _toolbarsMutator = toolbarsMutator;
    _gridMediatorDelegate = delegate;
  }
  return self;
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

#pragma mark - ChromeCoordinator

- (void)start {
  BOOL regularModeEnabled =
      !IsIncognitoModeForced(self.browser->GetBrowserState()->GetPrefs());

  GridContainerViewController* container =
      [[GridContainerViewController alloc] init];
  self.gridContainerViewController = container;

  if (regularModeEnabled) {
    self.gridViewController = [[RegularGridViewController alloc] init];
    container.containedViewController = self.gridViewController;
  } else {
    DisabledGridViewController* disabledViewController =
        [[DisabledGridViewController alloc]
            initWithPage:TabGridPageRegularTabs];
    disabledViewController.delegate = self.disabledTabViewControllerDelegate;
    self.disabledViewController = disabledViewController;
    container.containedViewController = self.disabledViewController;
  }

  _mediator = [[RegularGridMediator alloc] init];
  _mediator.consumer = self.gridViewController;
  _mediator.browser = self.browser;
  _mediator.delegate = _gridMediatorDelegate;
  _mediator.toolbarsMutator = _toolbarsMutator;
  _mediator.actionWrangler = self.tabGridViewController;
  _mediator.itemProvider = self.gridViewController;

  // TODO(crbug.com/1457146): As browser state should never be nil, it should be
  // safe to remove the check.
  ChromeBrowserState* regularBrowserState = self.browser->GetBrowserState();
  if (regularBrowserState) {
    _mediator.tabRestoreService =
        IOSChromeTabRestoreServiceFactory::GetForBrowserState(
            regularBrowserState);
  }

  self.tabGridViewController.regularTabsDelegate = _mediator;
  self.gridViewController.dragDropHandler = _mediator;
  self.gridViewController.shareableItemsProvider = _mediator;

  // If regular is enabled then the grid exists and it is not disabled.
  // TODO(crbug.com/1457146): Get disabled status from the mediator.
  if (self.gridViewController) {
    self.gridViewController.dragDropHandler = _mediator;
    // TODO(crbug.com/1457146): Move the following lines to the grid itself when
    // specific grid file will be created.
    self.gridViewController.view.accessibilityIdentifier =
        kRegularTabGridIdentifier;
    self.gridViewController.emptyStateView =
        [[TabGridEmptyStateView alloc] initWithPage:TabGridPageRegularTabs];
    self.gridViewController.emptyStateView.accessibilityIdentifier =
        kTabGridRegularTabsEmptyStateIdentifier;
    self.gridViewController.theme = GridThemeLight;

    self.gridContainerViewController.containedViewController =
        self.gridViewController;
  }

  if (IsPinnedTabsEnabled()) {
    _pinnedTabsMediator = [[PinnedTabsMediator alloc]
        initWithConsumer:self.tabGridViewController.pinnedTabsConsumer];
    _pinnedTabsMediator.browser = self.browser;
    self.tabGridViewController.pinnedTabsDelegate = _pinnedTabsMediator;
    self.tabGridViewController.pinnedTabsDragDropHandler = _pinnedTabsMediator;
  }
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
}

#pragma mark - Public

- (void)stopChidCoordinators {
  [self.gridViewController dismissModals];
}

@end
