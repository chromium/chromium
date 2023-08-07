// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_mediator.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"

@implementation IncognitoGridMediator

// TODO(crbug.com/1457146): Refactor the grid commands to have the same function
// name to close all.
#pragma mark - GridCommands

- (void)closeAllItems {
  RecordTabGridCloseTabsCount(self.webStateList->count());
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridCloseAllIncognitoTabs"));
  // This is a no-op if `webStateList` is already empty.
  self.webStateList->CloseAllWebStates(WebStateList::CLOSE_USER_ACTION);
  SnapshotBrowserAgent::FromBrowser(self.browser)->RemoveAllSnapshots();
}

- (void)saveAndCloseAllItems {
  NOTREACHED_NORETURN() << "Incognito tabs should not be saved before closing.";
}

- (void)undoCloseAllItems {
  NOTREACHED_NORETURN() << "Incognito tabs are not saved before closing.";
}

- (void)discardSavedClosedItems {
  NOTREACHED_NORETURN() << "Incognito tabs cannot be saved.";
}

#pragma mark - TabGridPageMutator

- (void)currentlySelectedGrid:(BOOL)selected {
  if (selected) {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridSelectIncognitoPanel"));

    [self configureToolbarsButtons];
  }
  // TODO(crbug.com/1457146): Implement.
}

#pragma mark - Parent's function

- (void)configureToolbarsButtons {
  TabGridToolbarsConfiguration* toolbarsConfiguration =
      [[TabGridToolbarsConfiguration alloc] init];
  toolbarsConfiguration.closeAllButton = !self.webStateList->empty();
  toolbarsConfiguration.doneButton = YES;
  toolbarsConfiguration.searchButton = YES;
  toolbarsConfiguration.selectTabsButton = !self.webStateList->empty();
  [self.toolbarsMutator setToolbarConfiguration:toolbarsConfiguration];
}

@end
