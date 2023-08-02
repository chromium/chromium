// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_coordinator.h"

#import "ios/chrome/browser/ui/menu/action_factory.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_bottom_toolbar.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_page_control.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_action_wrangler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_delegate_wrangler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_top_toolbar.h"

@implementation TabGridToolbarsCoordinator {
  // Mediator of all tab grid toolbars.
  TabGridToolbarsMediator* _mediator;
}

- (void)start {
  [self setupTopToolbar];
  [self setupBottomToolbar];
  [self updateToolbarButtons];
}

#pragma mark - Property Implementation.

- (id<GridToolbarsMutator>)toolbarsMutator {
  if (!_mediator) {
    _mediator = [[TabGridToolbarsMediator alloc] init];
  }
  return _mediator;
}

#pragma mark - Private

- (void)setupTopToolbar {
  // In iOS 13+, constraints break if the UIToolbar is initialized with a null
  // or zero rect frame. An arbitrary non-zero frame fixes this issue.
  TabGridTopToolbar* topToolbar =
      [[TabGridTopToolbar alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  self.topToolbar = topToolbar;
  topToolbar.translatesAutoresizingMaskIntoConstraints = NO;

  [topToolbar setCloseAllButtonTarget:self
                               action:@selector(closeAllButtonTapped:)];
  [topToolbar setDoneButtonTarget:self action:@selector(doneButtonTapped:)];
  [topToolbar setSelectAllButtonTarget:self
                                action:@selector(selectAllButtonTapped:)];
  [topToolbar setSearchButtonTarget:self action:@selector(searchButtonTapped:)];
  [topToolbar setCancelSearchButtonTarget:self
                                   action:@selector(cancelSearchButtonTapped:)];
  [topToolbar setSearchBarDelegate:self.searchDelegate];

  // Configure and initialize the page control.
  [topToolbar.pageControl addTarget:self
                             action:@selector(pageControlChangedValue:)
                   forControlEvents:UIControlEventValueChanged];
  [topToolbar.pageControl addTarget:self
                             action:@selector(pageControlChangedPageByDrag:)
                   forControlEvents:TabGridPageChangeByDragEvent];
  [topToolbar.pageControl addTarget:self
                             action:@selector(pageControlChangedPageByTap:)
                   forControlEvents:TabGridPageChangeByTapEvent];
}

- (void)setupBottomToolbar {
  TabGridBottomToolbar* bottomToolbar = [[TabGridBottomToolbar alloc] init];
  self.bottomToolbar = bottomToolbar;
  bottomToolbar.translatesAutoresizingMaskIntoConstraints = NO;

  [bottomToolbar setCloseAllButtonTarget:self
                                  action:@selector(closeAllButtonTapped:)];
  [bottomToolbar setDoneButtonTarget:self action:@selector(doneButtonTapped:)];

  [bottomToolbar setNewTabButtonTarget:self
                                action:@selector(newTabButtonTapped:)];
  [bottomToolbar setCloseTabsButtonTarget:self
                                   action:@selector(closeSelectedTabs:)];
  [bottomToolbar setShareTabsButtonTarget:self
                                   action:@selector(shareSelectedTabs:)];
}

#pragma mark - Control actions

- (void)closeAllButtonTapped:(id)sender {
  [self.actionWrangler closeAllButtonTapped:sender];
}

- (void)doneButtonTapped:(id)sender {
  [self.actionWrangler doneButtonTapped:sender];
}

- (void)newTabButtonTapped:(id)sender {
  [self.actionWrangler newTabButtonTapped:sender];
}

- (void)selectAllButtonTapped:(id)sender {
  [self.actionWrangler selectAllButtonTapped:sender];
}

- (void)searchButtonTapped:(id)sender {
  [self.actionWrangler searchButtonTapped:sender];
}

- (void)cancelSearchButtonTapped:(id)sender {
  [self.actionWrangler cancelSearchButtonTapped:sender];
}

- (void)closeSelectedTabs:(id)sender {
  [self.actionWrangler closeSelectedTabs:sender];
}

- (void)shareSelectedTabs:(id)sender {
  [self.actionWrangler shareSelectedTabs:sender];
}

- (void)pageControlChangedValue:(id)sender {
  [self.actionWrangler pageControlChangedValue:sender];
}

- (void)pageControlChangedPageByDrag:(id)sender {
  [self.actionWrangler pageControlChangedPageByDrag:sender];
}

- (void)pageControlChangedPageByTap:(id)sender {
  [self.actionWrangler pageControlChangedPageByTap:sender];
}

- (void)selectTabsButtonTapped:(id)sender {
  [self.actionWrangler selectTabsButtonTapped:sender];
}

#pragma mark - TabGridToolbarsCommandsWrangler

- (void)updateToolbarButtons {
  ActionFactory* actionFactory = [[ActionFactory alloc]
      initWithScenario:MenuScenarioHistogram::kTabGridEdit];
  __weak TabGridToolbarsCoordinator* weakSelf = self;
  NSMutableArray<UIMenuElement*>* menuElements =
      [@[ [actionFactory actionToCloseAllTabsWithBlock:^{
        [weakSelf closeAllButtonTapped:nil];
      }] ] mutableCopy];
  // Disable the "Select All" option from the edit button when there are no tabs
  // in the regular tab grid. "Close All" can still be called if there are
  // inactive tabs.
  BOOL disabledSelectAll = [self.delegateWrangler isCurrentGridEmpty];
  if (!disabledSelectAll) {
    [menuElements addObject:[actionFactory actionToSelectTabsWithBlock:^{
                    [weakSelf selectTabsButtonTapped:nil];
                  }]];
  }

  UIMenu* menu = [UIMenu menuWithChildren:menuElements];
  [self.topToolbar setEditButtonMenu:menu];
  [self.bottomToolbar setEditButtonMenu:menu];
}

@end
