// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_mediator.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_buttons_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"

#import "ios/chrome/browser/ui/menu/action_factory.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_bottom_toolbar.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_top_toolbar.h"

@implementation TabGridToolbarsMediator {
  // Configuration that provides all buttons to display.
  TabGridToolbarsConfiguration* _configuration;
  id<TabGridToolbarsButtonsDelegate> _buttonsDelegate;
}

#pragma mark - GridToolbarsMutator

- (void)setToolbarConfiguration:(TabGridToolbarsConfiguration*)configuration {
  _configuration = configuration;

  // TODO(crbug.com/1457146): Add all buttons management.
  [self configureEditOrUndoButton];
  [self.bottomToolbarConsumer
      setNewTabButtonEnabled:_configuration.newTabButton];
}

- (void)setToolbarsButtonsDelegate:
    (id<TabGridToolbarsButtonsDelegate>)delegate {
  _buttonsDelegate = delegate;
  self.topToolbarConsumer.buttonsDelegate = delegate;
  self.bottomToolbarConsumer.buttonsDelegate = delegate;
}

#pragma mark - Private

// Helpers to determine which button should be selected between "Edit" or "Undo"
// and if the "Edit" button should be enabled.
// TODO(crbug.com/1457146): Send buttons configuration directly to the correct
// consumer instead of send information to object when it is not necessary.
- (void)configureEditOrUndoButton {
  [self.topToolbarConsumer useUndoCloseAll:_configuration.undoButton];
  [self.bottomToolbarConsumer useUndoCloseAll:_configuration.undoButton];

  // TODO(crbug.com/1457146): Separate "Close All" and "Undo".
  [self.topToolbarConsumer
      setCloseAllButtonEnabled:_configuration.closeAllButton ||
                               _configuration.undoButton];
  [self.bottomToolbarConsumer
      setCloseAllButtonEnabled:_configuration.closeAllButton ||
                               _configuration.undoButton];

  BOOL shouldEnableEditButton =
      _configuration.closeAllButton || _configuration.selectTabsButton;
  if (shouldEnableEditButton) {
    [self configureEditButtons];
  }
  [self.bottomToolbarConsumer setEditButtonEnabled:shouldEnableEditButton];
  [self.topToolbarConsumer setEditButtonEnabled:shouldEnableEditButton];
}

// Configures buttons that are available under the edit menu.
- (void)configureEditButtons {
  ActionFactory* actionFactory = [[ActionFactory alloc]
      initWithScenario:MenuScenarioHistogram::kTabGridEdit];
  __weak id<TabGridToolbarsButtonsDelegate> weakButtonDelegate =
      _buttonsDelegate;
  NSMutableArray<UIMenuElement*>* menuElements =
      [@[ [actionFactory actionToCloseAllTabsWithBlock:^{
        [weakButtonDelegate closeAllButtonTapped:nil];
      }] ] mutableCopy];
  // Disable the "Select All" option from the edit button when there are no tabs
  // in the regular tab grid. "Close All" can still be called if there are
  // inactive tabs.
  if (_configuration.selectTabsButton) {
    [menuElements addObject:[actionFactory actionToSelectTabsWithBlock:^{
                    [weakButtonDelegate selectTabsButtonTapped:nil];
                  }]];
  }

  UIMenu* menu = [UIMenu menuWithChildren:menuElements];
  [self.topToolbarConsumer setEditButtonMenu:menu];
  [self.bottomToolbarConsumer setEditButtonMenu:menu];
}

@end
