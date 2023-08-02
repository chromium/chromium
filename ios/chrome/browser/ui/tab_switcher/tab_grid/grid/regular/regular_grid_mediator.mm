// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_mediator.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/sessions/core/tab_restore_service.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"
#import "ios/chrome/browser/web_state_list/web_state_list_serialization.h"

@implementation RegularGridMediator {
  // The saved session window just before close all tabs is called.
  SessionWindowIOS* _closedSessionWindow;
  // The number of tabs in `closedSessionWindow` that are synced by
  // TabRestoreService.
  int _syncedClosedTabsCount;
}

#pragma mark - GridCommands

// TODO(crbug.com/1457146): Refactor the grid commands to have the same function
// name to close all.
- (void)closeAllItems {
  NOTREACHED_NORETURN() << "Regular tabs should be saved before close all.";
}

- (void)saveAndCloseAllItems {
  RecordTabGridCloseTabsCount(self.webStateList->count());
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridCloseAllRegularTabs"));

  if (self.webStateList->empty()) {
    return;
  }

  int old_size =
      self.tabRestoreService ? self.tabRestoreService->entries().size() : 0;

  if (IsPinnedTabsEnabled()) {
    BOOL hasPinnedWebStatesOnly =
        self.webStateList->GetIndexOfFirstNonPinnedWebState() ==
        self.webStateList->count();

    if (hasPinnedWebStatesOnly) {
      return;
    }

    _closedSessionWindow = SerializeWebStateList(self.webStateList);
    self.webStateList->CloseAllNonPinnedWebStates(
        WebStateList::CLOSE_USER_ACTION);
  } else {
    _closedSessionWindow = SerializeWebStateList(self.webStateList);
    self.webStateList->CloseAllWebStates(WebStateList::CLOSE_USER_ACTION);
  }

  _syncedClosedTabsCount =
      self.tabRestoreService
          ? self.tabRestoreService->entries().size() - old_size
          : 0;

  // Update toolbar's buttons as the number of tabs changed so the options
  // changed ("Undo" may be available now).
  [self configureToolbarsButtons];
}

- (void)undoCloseAllItems {
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridUndoCloseAllRegularTabs"));
  if (!_closedSessionWindow) {
    return;
  }
  SessionRestorationBrowserAgent::FromBrowser(self.browser)
      ->RestoreSessionWindow(_closedSessionWindow,
                             SessionRestorationScope::kRegularOnly);
  _closedSessionWindow = nil;
  [self removeEntriesFromTabRestoreService];
  _syncedClosedTabsCount = 0;

  // Update toolbar's buttons as the number of tabs changed so the options
  // changed.
  [self configureToolbarsButtons];
}

- (void)discardSavedClosedItems {
  if (!_closedSessionWindow) {
    return;
  }
  _syncedClosedTabsCount = 0;
  _closedSessionWindow = nil;
  SnapshotBrowserAgent::FromBrowser(self.browser)->RemoveAllSnapshots();

  // Update toolbar's buttons as the number of tabs changed so the options
  // changed.
  [self configureToolbarsButtons];
}

#pragma mark - TabGridPageMutator

- (void)currentlySelectedGrid:(BOOL)selected {
  if (selected) {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridSelectRegularPanel"));

    [self configureToolbarsButtons];
  }
  // TODO(crbug.com/1457146): Implement.
}

#pragma mark - Private

// Creates and send a tab grid toolbar configuration with button that should be
// displayed when regular grid is selected.
- (void)configureToolbarsButtons {
  TabGridToolbarsConfiguration* toolbarsConfiguration =
      [[TabGridToolbarsConfiguration alloc] init];
  // TODO(crbug.com/1457146): This should take into account inactive tabs too.
  toolbarsConfiguration.closeAllButton = !self.webStateList->empty();
  toolbarsConfiguration.doneButton = YES;
  toolbarsConfiguration.searchButton = YES;
  toolbarsConfiguration.selectTabsButton = !self.webStateList->empty();
  // TODO(crbug.com/1457146): This should take into account inactive tabs too.
  toolbarsConfiguration.undoButton = _closedSessionWindow;
  [self.toolbarsMutator setToolbarConfiguration:toolbarsConfiguration];
}

// Removes `self.syncedClosedTabsCount` most recent entries from the
// TabRestoreService.
- (void)removeEntriesFromTabRestoreService {
  if (!self.tabRestoreService) {
    return;
  }
  std::vector<SessionID> identifiers;
  auto iter = self.tabRestoreService->entries().begin();
  auto end = self.tabRestoreService->entries().end();
  for (int i = 0; i < _syncedClosedTabsCount && iter != end; i++) {
    identifiers.push_back(iter->get()->id);
    iter++;
  }
  for (const SessionID sessionID : identifiers) {
    self.tabRestoreService->RemoveTabEntryById(sessionID);
  }
}

@end
