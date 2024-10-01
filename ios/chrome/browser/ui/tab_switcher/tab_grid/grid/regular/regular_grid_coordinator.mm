// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_coordinator.h"

#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/disabled_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_container_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_empty_state_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_theme.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_group_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_context_menu_helper.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_view_controller.h"

@interface RegularGridCoordinator ()

// Redefined as readwrite.
@property(nonatomic, readwrite, strong)
    RegularGridViewController* gridViewController;

@end

@implementation RegularGridCoordinator {
  // Mediator for pinned Tabs.
  PinnedTabsMediator* _pinnedTabsMediator;
  // Context menu provider.
  TabContextMenuHelper* _contextMenuProvider;
  // Mediator of regular grid.
  RegularGridMediator* _mediator;
}

#pragma mark - Property Implementation.

- (RegularGridMediator*)mediator {
  CHECK(_mediator)
      << "RegularGridCoordinator's -start should be called before.";
  return _mediator;
}

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

#pragma mark - Superclass overrides

- (LegacyGridTransitionLayout*)transitionLayout {
  if (IsTabGroupInGridEnabled()) {
    if (self.tabGroupCoordinator) {
      return [self.tabGroupCoordinator.viewController
                  .gridViewController transitionLayout];
    }
  }

  LegacyGridTransitionLayout* transitionLayout =
      [_gridViewController transitionLayout];

  if (IsPinnedTabsEnabled()) {
    LegacyGridTransitionLayout* pinnedTabsTransitionLayout =
        [self.pinnedTabsViewController transitionLayout];

    return [self combineTransitionLayout:transitionLayout
                    withTransitionLayout:pinnedTabsTransitionLayout];
  }

  return transitionLayout;
}

- (BOOL)isSelectedCellVisible {
  BOOL isSelectedCellVisible = [super isSelectedCellVisible];

  if (IsPinnedTabsEnabled() &&
      !(IsTabGroupInGridEnabled() && self.tabGroupCoordinator)) {
    isSelectedCellVisible |= self.pinnedTabsViewController.selectedCellVisible;
  }

  return isSelectedCellVisible;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(TabsAnimationCommands)];

  BOOL regularModeEnabled =
      !IsIncognitoModeForced(self.browser->GetProfile()->GetPrefs());

  _contextMenuProvider = [[TabContextMenuHelper alloc]
             initWithProfile:self.browser->GetProfile()
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

  _mediator = [[RegularGridMediator alloc] initWithModeHolder:self.modeHolder];
  _mediator.consumer = gridViewController;

  gridViewController.dragDropHandler = _mediator;
  gridViewController.mutator = _mediator;
  gridViewController.gridProvider = _mediator;
  gridViewController.menuProvider = _contextMenuProvider;

  // If regular is enabled then the grid exists and it is not disabled.
  // TODO(crbug.com/40273478): Get disabled status from the mediator.
  if (gridViewController) {
    gridViewController.dragDropHandler = _mediator;
    // TODO(crbug.com/40273478): Move the following lines to the grid itself
    // when specific grid file will be created.
    gridViewController.view.accessibilityIdentifier = kRegularTabGridIdentifier;
    gridViewController.emptyStateView =
        [[TabGridEmptyStateView alloc] initWithPage:TabGridPageRegularTabs];
    gridViewController.emptyStateView.accessibilityIdentifier =
        kTabGridRegularTabsEmptyStateIdentifier;
    gridViewController.theme = GridThemeLight;
    gridViewController.suggestedActionsDelegate = _mediator;

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
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];

  _pinnedTabsMediator = nil;
  _contextMenuProvider = nil;

  [super stop];
}

#pragma mark - TabsAnimationCommands

- (void)animateTabsClosureForTabs:(std::set<web::WebStateID>)tabsToClose
                           groups:
                               (std::map<tab_groups::TabGroupId, std::set<int>>)
                                   groupsWithTabsToClose
                  allInactiveTabs:(BOOL)animateAllInactiveTabs
                completionHandler:(ProceduralBlock)completionHandler {
  [self hideTabGroup];  // Make sure that no tab group is being displayed.
  [_gridViewController animateTabsClosureForTabs:tabsToClose
                                          groups:groupsWithTabsToClose
                                 allInactiveTabs:animateAllInactiveTabs
                               completionHandler:completionHandler];
}

#pragma mark - Public

- (void)stopChildCoordinators {
  [super stopChildCoordinators];
  [self.pinnedTabsViewController dismissModals];
}

@end
