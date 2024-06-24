// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_mediator.h"

#import "base/memory/weak_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_grid_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_main_tab_grid_delegate.h"

@interface TabGroupsPanelMediator () <TabGridToolbarsGridDelegate>
@end

@implementation TabGroupsPanelMediator {
  // The regular WebStateList, to check if there are tabs to go back to when
  // pressing the Done button.
  base::WeakPtr<WebStateList> _regularWebStateList;
  // Whether this screen is disabled by policy.
  BOOL _isDisabled;
  // Whether this screen is selected in the TabGrid.
  BOOL _selectedGrid;
}

- (instancetype)initWithRegularWebStateList:(WebStateList*)regularWebStateList
                           disabledByPolicy:(BOOL)disabled {
  self = [super init];
  if (self) {
    _regularWebStateList = regularWebStateList->AsWeakPtr();
    _isDisabled = disabled;
  }
  return self;
}

#pragma mark - TabGridPageMutator

- (void)currentlySelectedGrid:(BOOL)selected {
  _selectedGrid = selected;

  if (selected) {
    base::RecordAction(base::UserMetricsAction("MobileTabGridSelectTabGroups"));

    [self configureToolbarsButtons];
  }
}

- (void)switchToMode:(TabGridMode)mode {
  CHECK(mode == TabGridModeNormal)
      << "Tab Groups panel should only support Normal mode.";
}

#pragma mark - TabGridToolbarsGridDelegate

- (void)closeAllButtonTapped:(id)sender {
  NOTREACHED_NORETURN() << "Should not be called in Tab Groups.";
}

- (void)doneButtonTapped:(id)sender {
  [self.toolbarTabGridDelegate doneButtonTapped:sender];
}

- (void)newTabButtonTapped:(id)sender {
  NOTREACHED_NORETURN() << "Should not be called in Tab Groups.";
}

- (void)selectAllButtonTapped:(id)sender {
  NOTREACHED_NORETURN() << "Should not be called in Tab Groups.";
}

- (void)searchButtonTapped:(id)sender {
  NOTREACHED_NORETURN() << "Should not be called in Tab Groups.";
}

- (void)cancelSearchButtonTapped:(id)sender {
  NOTREACHED_NORETURN() << "Should not be called in Tab Groups.";
}

- (void)closeSelectedTabs:(id)sender {
  NOTREACHED_NORETURN() << "Should not be called in Tab Groups.";
}

- (void)shareSelectedTabs:(id)sender {
  NOTREACHED_NORETURN() << "Should not be called in Tab Groups.";
}

- (void)selectTabsButtonTapped:(id)sender {
  NOTREACHED_NORETURN() << "Should not be called in Tab Groups.";
}

#pragma mark - Private

// Creates and send a tab grid toolbar configuration with button that should be
// displayed when Tab Groups is selected.
- (void)configureToolbarsButtons {
  if (!_selectedGrid) {
    return;
  }
  // Start to configure the delegate, so configured buttons will depend on the
  // correct delegate.
  [self.toolbarsMutator setToolbarsButtonsDelegate:self];

  if (_isDisabled) {
    [self.toolbarsMutator
        setToolbarConfiguration:
            [TabGridToolbarsConfiguration
                disabledConfigurationForPage:TabGridPageTabGroups]];
    return;
  }

  TabGridToolbarsConfiguration* toolbarsConfiguration =
      [[TabGridToolbarsConfiguration alloc] initWithPage:TabGridPageTabGroups];
  toolbarsConfiguration.mode = TabGridModeNormal;
  // Done button is enabled if there is at least one Regular tab.
  toolbarsConfiguration.doneButton =
      _regularWebStateList && !_regularWebStateList->empty();
  [self.toolbarsMutator setToolbarConfiguration:toolbarsConfiguration];
}

@end
