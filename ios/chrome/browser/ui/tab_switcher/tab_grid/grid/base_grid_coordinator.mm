// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_coordinator+subclassing.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/create_tab_group_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_coordinator.h"
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

#pragma mark - Subclassing properties

- (id<GridToolbarsMutator>)toolbarsMutator {
  return _toolbarsMutator;
}

- (id<GridMediatorDelegate>)gridMediatorDelegate {
  return _gridMediatorDelegate;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(TabGroupsCommands)];
}

- (void)stop {
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  if (_tabGroupCoordinator) {
    [self hideTabGroup];
  }
}

#pragma mark - TabGroupsCommands

- (void)showTabGroupWithID {
  CHECK(base::FeatureList::IsEnabled(kTabGroupsInGrid))
      << "You should not be able to show a tab group UI outside the "
         "Tab Groups experiment.";
  CHECK(!_tabGroupCoordinator) << "There is an atemps to display a tab group "
                                  "when one is already presented.";
  // TODO(crbug.com/1501837): Replace base view controller by view controller
  // when the base grid coordinator will have access to the grid view
  // controller.
  _tabGroupCoordinator = [[TabGroupCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser];
  [_tabGroupCoordinator start];
}

- (void)hideTabGroup {
  [_tabGroupCoordinator stop];
  _tabGroupCoordinator = nil;
}

- (void)showTabGroupCreationForTabs:
    (const std::set<web::WebStateID>&)identifiers {
  CHECK(base::FeatureList::IsEnabled(kTabGroupsInGrid))
      << "You should not be able to create a tab group outside the Tab Groups "
         "experiment.";
  CHECK(!_tabGroupCreator) << "There is an atemps to create a tab group when a "
                              "creation process is still running.";

  // TODO(crbug.com/1501837): Replace base view controller by view controller
  // when the base grid coordinator will have access to the grid view
  // controller.
  _tabGroupCreator = [[CreateTabGroupCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                    selectedTabs:identifiers];
  [_tabGroupCreator start];
}

- (void)hideTabGroupCreation {
  [_tabGroupCreator stop];
  _tabGroupCreator = nil;
}

@end
