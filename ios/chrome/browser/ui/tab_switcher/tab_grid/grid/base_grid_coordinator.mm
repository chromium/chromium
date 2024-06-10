// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/check.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_toolbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_container_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/create_tab_group_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/legacy_grid_transition_layout.h"
#import "ios/web/public/web_state.h"

@implementation BaseGridCoordinator {
  // Mutator that handle toolbars changes.
  __weak id<GridToolbarsMutator> _toolbarsMutator;
  // Delegate to handle presenting the action sheet.
  __weak id<GridMediatorDelegate> _gridMediatorDelegate;
  // Tab Groups Coordinator used to display the tab group UI;
  TabGroupCoordinator* _tabGroupCoordinator;
  // Handles the creation of a new tab group.
  CreateTabGroupCoordinator* _tabGroupCreator;
}

#pragma mark - Public

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

- (BaseGridMediator*)mediator {
  NOTREACHED_NORETURN() << "This should be implemented in subclasses.";
}

- (BaseGridViewController*)gridViewController {
  NOTREACHED_NORETURN() << "This should be implemented in subclasses.";
}

- (void)showTabGroupForTabGridOpening:(const TabGroup*)tabGroup {
  [self showTabGroup:tabGroup forTabGridOpening:YES];
}

- (LegacyGridTransitionLayout*)transitionLayout {
  NOTREACHED_NORETURN() << "This should be implemented in subclasses.";
}

- (BOOL)isSelectedCellVisible {
  if (IsTabGroupInGridEnabled()) {
    if (_tabGroupCoordinator) {
      return _tabGroupCoordinator.viewController.gridViewController
          .selectedCellVisible;
    }
  }
  return self.gridViewController.selectedCellVisible;
}

- (UIView*)gridView {
  if (IsTabGroupInGridEnabled()) {
    if (_tabGroupCoordinator) {
      return _tabGroupCoordinator.viewController.gridViewController.view;
    }
  }
  return self.gridContainerViewController.view;
}

- (UIView*)gridContainerForAnimation {
  if (IsTabGroupInGridEnabled()) {
    if (_tabGroupCoordinator) {
      return _tabGroupCoordinator.viewController.gridViewController.view;
    }
  }
  return nil;
}

#pragma mark - Subclassing properties

- (id<GridToolbarsMutator>)toolbarsMutator {
  return _toolbarsMutator;
}

- (id<GridMediatorDelegate>)gridMediatorDelegate {
  return _gridMediatorDelegate;
}

- (TabGroupCoordinator*)tabGroupCoordinator {
  return _tabGroupCoordinator;
}

- (LegacyGridTransitionLayout*)
    combineTransitionLayout:(LegacyGridTransitionLayout*)primaryLayout
       withTransitionLayout:(LegacyGridTransitionLayout*)secondaryLayout {
  NSArray<LegacyGridTransitionItem*>* primaryInactiveItems =
      primaryLayout.inactiveItems;
  NSArray<LegacyGridTransitionItem*>* secondaryInactiveItems =
      secondaryLayout.inactiveItems;

  NSArray<LegacyGridTransitionItem*>* inactiveItems =
      [self combineInactiveItems:primaryInactiveItems
               withInactiveItems:secondaryInactiveItems];

  LegacyGridTransitionActiveItem* primaryActiveItem = primaryLayout.activeItem;
  LegacyGridTransitionActiveItem* secondaryActiveItem =
      secondaryLayout.activeItem;

  // Prefer primary active item.
  LegacyGridTransitionActiveItem* activeItem =
      primaryActiveItem ? primaryActiveItem : secondaryActiveItem;

  LegacyGridTransitionItem* primarySelectionItem = primaryLayout.selectionItem;
  LegacyGridTransitionItem* secondarySelectionItem =
      secondaryLayout.selectionItem;

  // Prefer primary selection item.
  LegacyGridTransitionItem* selectionItem =
      primarySelectionItem ? primarySelectionItem : secondarySelectionItem;

  return [LegacyGridTransitionLayout layoutWithInactiveItems:inactiveItems
                                                  activeItem:activeItem
                                               selectionItem:selectionItem];
}

#pragma mark - ChromeCoordinator

- (void)start {
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  [dispatcher startDispatchingToTarget:self
                           forProtocol:@protocol(TabGroupsCommands)];

  self.mediator.tabGroupsHandler = self;
  if (!self.browser->GetBrowserState()->IsOffTheRecord()) {
    self.mediator.tabGridToolbarHandler =
        HandlerForProtocol(dispatcher, TabGridToolbarCommands);
  }
  self.mediator.browser = self.browser;
  self.mediator.delegate = self.gridMediatorDelegate;
  self.mediator.toolbarsMutator = self.toolbarsMutator;
}

- (void)stop {
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  if (_tabGroupCoordinator) {
    [self hideTabGroup];
  }

  [self.mediator disconnect];
}

#pragma mark - TabGroupsCommands

- (void)showTabGroup:(const TabGroup*)tabGroup {
  if (_tabGroupCoordinator) {
    [self hideTabGroup];
  }

  // When entering the tab group, disable scrolls-to-top gesture for the
  // view controller that is going to stay behind the screen being presented.
  self.gridViewController.gridScrollsToTopEnabled = NO;

  [self showTabGroup:tabGroup forTabGridOpening:NO];
}

- (void)hideTabGroup {
  // When the tab group is hidden, re-enable the scrolls-to-top gesture on the
  // regular grid view controller.
  self.gridViewController.gridScrollsToTopEnabled = YES;

  [_tabGroupCoordinator stop];
  _tabGroupCoordinator = nil;
}

- (void)showTabGroupCreationForTabs:
    (const std::set<web::WebStateID>&)identifiers {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group outside the Tab Groups "
         "experiment.";
  CHECK(!_tabGroupCreator) << "There is an atemps to create a tab group when a "
                              "creation process is still running.";

  _tabGroupCreator = [[CreateTabGroupCoordinator alloc]
      initTabGroupCreationWithBaseViewController:self.baseViewController
                                         browser:self.browser
                                    selectedTabs:identifiers];
  _tabGroupCreator.delegate = self;
  [_tabGroupCreator start];
}

- (void)hideTabGroupCreationAnimated:(BOOL)animated {
  _tabGroupCreator.animatedDismissal = animated;
  _tabGroupCreator.delegate = nil;
  [_tabGroupCreator stop];
  _tabGroupCreator = nil;
}

- (void)showTabGroupEditionForGroup:(const TabGroup*)tabGroup {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to edit a tab group outside the Tab Groups "
         "experiment.";
  CHECK(!_tabGroupCreator) << "There is an attempt to edit a tab group when a "
                              "creation process is still running.";
  CHECK(tabGroup) << "To edit a tab group you should pass a group.";

  UIViewController* backgroundView = _tabGroupCoordinator
                                         ? _tabGroupCoordinator.viewController
                                         : self.baseViewController;
  _tabGroupCreator = [[CreateTabGroupCoordinator alloc]
      initTabGroupEditionWithBaseViewController:backgroundView
                                        browser:self.browser
                                       tabGroup:tabGroup];
  _tabGroupCreator.delegate = self;
  [_tabGroupCreator start];
}

- (void)showActiveTab {
  [self.mediator displayActiveTab];
}

#pragma mark - CreateOrEditTabGroupCoordinatorDelegate

- (void)createOrEditTabGroupCoordinatorDidDismiss:
            (CreateTabGroupCoordinator*)coordinator
                                         animated:(BOOL)animated {
  CHECK(coordinator == _tabGroupCreator);
  [self hideTabGroupCreationAnimated:animated];
}

#pragma mark - Private

// Shows the `tabGroup` with animations for `tabGridOpening` or not.
- (void)showTabGroup:(const TabGroup*)tabGroup
    forTabGridOpening:(BOOL)tabGridOpening {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to show a tab group UI outside the "
         "Tab Groups experiment.";
  CHECK(!_tabGroupCoordinator) << "There is an attempt to display a tab group "
                                  "when one is already presented.";
  // TODO(crbug.com/40942154): Replace base view controller by view controller
  // when the base grid coordinator will have access to the grid view
  // controller.
  _tabGroupCoordinator = [[TabGroupCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                        tabGroup:tabGroup];
  _tabGroupCoordinator.tabContextMenuDelegate = self.tabContextMenuDelegate;
  _tabGroupCoordinator.animatedPresentation = !tabGridOpening;
  _tabGroupCoordinator.tabGroupPositioner = self.tabGroupPositioner;
  _tabGroupCoordinator.tabGridIdleStatusHandler =
      self.mediator.tabGridIdleStatusHandler;
  [_tabGroupCoordinator start];
}

// Combines two arrays of inactive items into one. The `primaryInactiveItems`
// (if any) would be placed in the front of the resulting array, whether the
// `secondaryInactiveItems` would be placed in the back.
- (NSArray<LegacyGridTransitionItem*>*)
    combineInactiveItems:
        (NSArray<LegacyGridTransitionItem*>*)primaryInactiveItems
       withInactiveItems:
           (NSArray<LegacyGridTransitionItem*>*)secondaryInactiveItems {
  if (primaryInactiveItems == nil) {
    primaryInactiveItems = @[];
  }

  return [primaryInactiveItems
      arrayByAddingObjectsFromArray:secondaryInactiveItems];
}

@end
