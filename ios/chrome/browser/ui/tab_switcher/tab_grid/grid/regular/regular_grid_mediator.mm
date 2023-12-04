// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_mediator.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/sessions/core/tab_restore_service.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tabs/model/features.h"
#import "ios/chrome/browser/tabs/model/tabs_closer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_configuration_provider.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"

// TODO(crbug.com/1457146): Needed for `TabPresentationDelegate`, should be
// refactored.
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_view_controller.h"

@implementation RegularGridMediator {
  // TabsClosed used to implement the "close all tabs" operation with support
  // for undoing the operation.
  std::unique_ptr<TabsCloser> _tabsCloser;
}

#pragma mark - GridCommands

// TODO(crbug.com/1457146): Refactor the grid commands to have the same function
// name to close all.
- (void)closeAllItems {
  NOTREACHED_NORETURN() << "Regular tabs should be saved before close all.";
}

- (void)saveAndCloseAllItems {
  if (![self canCloseTabs]) {
    return;
  }

  const int closed_tabs = _tabsCloser->CloseTabs();
  RecordTabGridCloseTabsCount(closed_tabs);
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridCloseAllRegularTabs"));
}

- (void)undoCloseAllItems {
  if (![self canUndoCloseTabs]) {
    return;
  }

  base::RecordAction(
      base::UserMetricsAction("MobileTabGridUndoCloseAllRegularTabs"));
  _tabsCloser->UndoCloseTabs();
}

- (void)discardSavedClosedItems {
  if (![self canUndoCloseTabs]) {
    return;
  }
  _tabsCloser->ConfirmDeletion();
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
  if ([self canUndoCloseRegularOrInactiveTabs]) {
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
  // Shows the tab only if has been created.
  if ([self addNewItem]) {
    [self.gridConsumer setActivePageFromPage:TabGridPageRegularTabs];
    [self.tabPresentationDelegate showActiveTabInPage:TabGridPageRegularTabs
                                         focusOmnibox:NO];
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridCreateRegularTab"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridFailedCreateRegularTab"));
  }
}

#pragma mark - Parent's function

- (void)disconnect {
  _tabsCloser.reset();
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
  toolbarsConfiguration.closeAllButton = [self canCloseRegularOrInactiveTabs];
  toolbarsConfiguration.doneButton = !self.webStateList->empty();
  toolbarsConfiguration.newTabButton = YES;
  toolbarsConfiguration.searchButton = YES;
  toolbarsConfiguration.selectTabsButton = [self hasRegularTabs];
  toolbarsConfiguration.undoButton = [self canUndoCloseRegularOrInactiveTabs];
  [self.toolbarsMutator setToolbarConfiguration:toolbarsConfiguration];
}

#pragma mark - Private

// YES if there are regular tabs in the grid.
- (BOOL)hasRegularTabs {
  return [self canCloseTabs];
}

- (BOOL)canCloseTabs {
  return _tabsCloser && _tabsCloser->CanCloseTabs();
}

- (BOOL)canUndoCloseTabs {
  return _tabsCloser && _tabsCloser->CanUndoCloseTabs();
}

- (BOOL)canCloseRegularOrInactiveTabs {
  if ([self canCloseTabs]) {
    return YES;
  }

  // This is an indirect way to check whether the inactive tabs can close
  // tabs or undo a close tabs action.
  TabGridToolbarsConfiguration* containedGridToolbarsConfiguration =
      [self.containedGridToolbarsProvider toolbarsConfiguration];
  return containedGridToolbarsConfiguration.closeAllButton;
}

- (BOOL)canUndoCloseRegularOrInactiveTabs {
  if ([self canUndoCloseTabs]) {
    return YES;
  }

  // This is an indirect way to check whether the inactive tabs can close
  // tabs or undo a close tabs action.
  TabGridToolbarsConfiguration* containedGridToolbarsConfiguration =
      [self.containedGridToolbarsProvider toolbarsConfiguration];
  return containedGridToolbarsConfiguration.undoButton;
}

#pragma mark - Properties

- (void)setBrowser:(Browser*)browser {
  [super setBrowser:browser];
  if (browser) {
    _tabsCloser = std::make_unique<TabsCloser>(
        browser, TabsCloser::ClosePolicy::kRegularTabs);
  } else {
    _tabsCloser.reset();
  }
}

@end
