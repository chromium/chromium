// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_mediator.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/sessions/core/tab_restore_service.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tabs/model/tabs_closer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_configuration_provider.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_idle_status_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mode_holder.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"
#import "ios/web/public/web_state.h"

// TODO(crbug.com/40273478): Needed for `TabPresentationDelegate`, should be
// refactored.
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_view_controller.h"

@implementation RegularGridMediator {
  // TabsClosed used to implement the "close all tabs" operation with support
  // for undoing the operation.
  std::unique_ptr<TabsCloser> _tabsCloser;
  // Whether the current grid is selected.
  BOOL _selected;
}

#pragma mark - GridCommands

- (void)closeItemWithID:(web::WebStateID)itemID {
  // Record when a regular tab is closed.
  base::RecordAction(base::UserMetricsAction("MobileTabGridCloseRegularTab"));
  [super closeItemWithID:itemID];
}

// TODO(crbug.com/40273478): Refactor the grid commands to have the same
// function name to close all.
- (void)closeAllItems {
  NOTREACHED() << "Regular tabs should be saved before close all.";
}

- (void)saveAndCloseAllItems {
  [self.inactiveTabsGridCommands saveAndCloseAllItems];
  if (![self canCloseTabs]) {
    return;
  }
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridCloseAllRegularTabs"));

  const int tabGroupCount = self.webStateList->GetGroups().size();

  const int closedTabs = _tabsCloser->CloseTabs();
  RecordTabGridCloseTabsCount(closedTabs);

  [self showTabGroupSnackbarOrIPH:tabGroupCount];
}

- (void)undoCloseAllItems {
  [self.inactiveTabsGridCommands undoCloseAllItems];
  if (![self canUndoCloseTabs]) {
    return;
  }

  base::RecordAction(
      base::UserMetricsAction("MobileTabGridUndoCloseAllRegularTabs"));

  _tabsCloser->UndoCloseTabs();
}

- (void)discardSavedClosedItems {
  [self.inactiveTabsGridCommands discardSavedClosedItems];
  if (![self canUndoCloseTabs]) {
    return;
  }
  _tabsCloser->ConfirmDeletion();
  [self configureToolbarsButtons];
}

#pragma mark - TabGridPageMutator

- (void)currentlySelectedGrid:(BOOL)selected {
  _selected = selected;

  if (selected) {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridSelectRegularPanel"));

    [self configureToolbarsButtons];
  }
}

- (void)setPageAsActive {
  [self.gridConsumer setActivePageFromPage:TabGridPageRegularTabs];
}

#pragma mark - TabGridToolbarsGridDelegate

- (void)closeAllButtonTapped:(id)sender {
  // TODO(crbug.com/40273478): Clean this in order to have "Close All" and
  // "Undo" separated actions. This was saved as a stack: first save the
  // inactive tabs, then the active tabs. So undo in the reverse order: first
  // undo the active tabs, then the inactive tabs.
  if ([self canUndoCloseRegularOrInactiveTabs]) {
    [self.consumer willUndoCloseAll];
    [self undoCloseAllItems];
    [self.consumer didUndoCloseAll];
  } else {
    [self.consumer willCloseAll];
    [self saveAndCloseAllItems];
    [self.consumer didCloseAll];
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
  if (IsIncognitoModeForced(self.browser->GetProfile()->GetPrefs())) {
    return;
  }

  [self.tabGridIdleStatusHandler
      tabGridDidPerformAction:TabGridActionType::kInPageAction];
  base::RecordAction(base::UserMetricsAction("MobileTabNewTab"));
  [self.gridConsumer prepareForDismissal];
  // Shows the tab only if has been created.
  if ([self addNewItem]) {
    [self displayActiveTab];
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
  if (!_selected) {
    return;
  }
  // Start to configure the delegate, so configured buttons will depend on the
  // correct delegate.
  [self.toolbarsMutator setToolbarsButtonsDelegate:self];

  if (IsIncognitoModeForced(self.browser->GetProfile()->GetPrefs())) {
    [self.toolbarsMutator
        setToolbarConfiguration:
            [TabGridToolbarsConfiguration
                disabledConfigurationForPage:TabGridPageRegularTabs]];
    return;
  }

  TabGridToolbarsConfiguration* toolbarsConfiguration =
      [[TabGridToolbarsConfiguration alloc]
          initWithPage:TabGridPageRegularTabs];

  if (self.modeHolder.mode == TabGridMode::kSelection) {
    [self configureButtonsInSelectionMode:toolbarsConfiguration];
  } else {
    toolbarsConfiguration.closeAllButton = [self canCloseRegularOrInactiveTabs];
    toolbarsConfiguration.doneButton = !self.webStateList->empty();
    toolbarsConfiguration.newTabButton = YES;
    toolbarsConfiguration.searchButton = YES;
    toolbarsConfiguration.selectTabsButton = [self hasRegularTabs];
    toolbarsConfiguration.undoButton = [self canUndoCloseRegularOrInactiveTabs];
  }

  [self.toolbarsMutator setToolbarConfiguration:toolbarsConfiguration];
}

- (void)displayActiveTab {
  [self.gridConsumer setActivePageFromPage:TabGridPageRegularTabs];
  [self.tabPresentationDelegate showActiveTabInPage:TabGridPageRegularTabs
                                       focusOmnibox:NO];
}

- (void)updateForTabInserted {
  if (!self.webStateList->empty()) {
    [self discardSavedClosedItems];
  }
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
