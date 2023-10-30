// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_mediator.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/sessions/core/tab_restore_service.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/sessions/web_state_list_serialization.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/tabs/model/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_configuration_provider.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"

// To get access to UseSessionSerializationOptimizations().
// TODO(crbug.com/1383087): remove once the feature is fully launched.
#import "ios/web/common/features.h"

// TODO(crbug.com/1457146): Needed for `TabPresentationDelegate`, should be
// refactored.
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_view_controller.h"

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
        self.webStateList->pinned_tabs_count() == self.webStateList->count();

    if (hasPinnedWebStatesOnly) {
      return;
    }

    if (!web::features::UseSessionSerializationOptimizations()) {
      _closedSessionWindow = SerializeWebStateList(self.webStateList);
    }
    self.webStateList->CloseAllNonPinnedWebStates(
        WebStateList::CLOSE_USER_ACTION);
  } else {
    if (!web::features::UseSessionSerializationOptimizations()) {
      _closedSessionWindow = SerializeWebStateList(self.webStateList);
    }
    self.webStateList->CloseAllWebStates(WebStateList::CLOSE_USER_ACTION);
  }

  _syncedClosedTabsCount =
      self.tabRestoreService
          ? self.tabRestoreService->entries().size() - old_size
          : 0;
}

- (void)undoCloseAllItems {
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridUndoCloseAllRegularTabs"));
  if (!web::features::UseSessionSerializationOptimizations()) {
    if (!_closedSessionWindow) {
      return;
    }
    SessionRestorationBrowserAgent::FromBrowser(self.browser)
        ->RestoreSessionWindow(_closedSessionWindow,
                               SessionRestorationScope::kRegularOnly);
    _closedSessionWindow = nil;
  }
  [self removeEntriesFromTabRestoreService];
  _syncedClosedTabsCount = 0;
}

- (void)discardSavedClosedItems {
  if (!_closedSessionWindow) {
    return;
  }
  _syncedClosedTabsCount = 0;
  _closedSessionWindow = nil;
  SnapshotBrowserAgent::FromBrowser(self.browser)->RemoveAllSnapshots();
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

#pragma mark - TabGridToolbarsButtonsDelegate

- (void)closeAllButtonTapped:(id)sender {
  // TODO(crbug.com/1457146): Clean this in order to have "Close All" and "Undo"
  // separated actions.
  // This was saved as a stack: first save the inactive tabs, then the active
  // tabs. So undo in the reverse order: first undo the active tabs, then the
  // inactive tabs.
  if (_closedSessionWindow ||
      [self.containedGridToolbarsProvider didSavedClosedTabs]) {
    if ([self.consumer respondsToSelector:@selector(willUndoCloseAll)]) {
      [self.consumer willUndoCloseAll];
    }
    [self undoCloseAllItems];
    [self.inactiveTabsGridCommands undoCloseAllItems];
    if ([self.consumer respondsToSelector:@selector(didUndoCloseAll)]) {
      [self.consumer didUndoCloseAll];
    }
  } else {
    if ([self.consumer respondsToSelector:@selector(willCloseAll)]) {
      [self.consumer willCloseAll];
    }
    [self.inactiveTabsGridCommands saveAndCloseAllItems];
    [self saveAndCloseAllItems];
    if ([self.consumer respondsToSelector:@selector(didCloseAll)]) {
      [self.consumer didCloseAll];
    }
  }
  // This is needed because configure button is called (web state list observer
  // in base grid mediator) when regular tabs are modified but not when inactive
  // tabs are modified.
  [self configureToolbarsButtons];
}

- (void)newTabButtonTapped:(id)sender {
  // Ignore the tap if the current page is disabled for some reason, by policy
  // for instance. This is to avoid situations where the tap action from an
  // enabled page can make it to a disabled page by releasing the
  // button press after switching to the disabled page (b/273416844 is an
  // example).
  if (IsIncognitoModeForced(self.browser->GetBrowserState()->GetPrefs())) {
    return;
  }

  [self.gridConsumer setPageIdleStatus:NO];
  base::RecordAction(base::UserMetricsAction("MobileTabNewTab"));
  [self.gridConsumer prepareForDismissal];
  [self addNewItem];
  [self.gridConsumer setActivePageFromPage:TabGridPageRegularTabs];
  [self.tabPresentationDelegate showActiveTabInPage:TabGridPageRegularTabs
                                       focusOmnibox:NO];
  base::RecordAction(base::UserMetricsAction("MobileTabGridCreateRegularTab"));
}

#pragma mark - Parent's function

- (void)disconnect {
  _closedSessionWindow = nil;
  _syncedClosedTabsCount = 0;
  [super disconnect];
}

- (void)configureToolbarsButtons {
  // Start to configure the delegate, so configured buttons will depend on the
  // correct delegate.
  [self.toolbarsMutator setToolbarsButtonsDelegate:self];

  if (IsIncognitoModeForced(self.browser->GetBrowserState()->GetPrefs())) {
    [self.toolbarsMutator setToolbarConfiguration:[TabGridToolbarsConfiguration
                                                      disabledConfiguration]];
    return;
  }

  TabGridToolbarsConfiguration* toolbarsConfiguration =
      [[TabGridToolbarsConfiguration alloc] init];
  toolbarsConfiguration.closeAllButton = [self canCloseAll];
  toolbarsConfiguration.doneButton = YES;
  toolbarsConfiguration.newTabButton = YES;
  toolbarsConfiguration.searchButton = YES;
  toolbarsConfiguration.selectTabsButton = [self isTabsInGrid];
  toolbarsConfiguration.undoButton = [self canUndo];
  [self.toolbarsMutator setToolbarConfiguration:toolbarsConfiguration];
}

#pragma mark - Private

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

// YES if there are tabs that can be closed.
- (BOOL)canCloseAll {
  TabGridToolbarsConfiguration* containedGridToolbarsConfiguration =
      [self.containedGridToolbarsProvider toolbarsConfiguration];

  return
      [self isTabsInGrid] || containedGridToolbarsConfiguration.closeAllButton;
}

// YES if there are tabs that can be restored.
- (BOOL)canUndo {
  return (_closedSessionWindow != nil) ||
         [self.containedGridToolbarsProvider didSavedClosedTabs];
}

// YES if there are tabs in regular grid only (not pinned, not in inactive tabs,
// etc.).
- (BOOL)isTabsInGrid {
  BOOL onlyPinnedTabs =
      self.webStateList->pinned_tabs_count() == self.webStateList->count();
  return !self.webStateList->empty() && !onlyPinnedTabs;
}

@end
