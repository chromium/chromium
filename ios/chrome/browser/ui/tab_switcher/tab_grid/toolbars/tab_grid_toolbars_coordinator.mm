// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_coordinator.h"

#import <ostream>

#import "base/check.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_bottom_toolbar.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_page_control.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_action_wrangler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_top_toolbar.h"

@implementation TabGridToolbarsCoordinator {
  // Mediator of all tab grid toolbars.
  TabGridToolbarsMediator* _mediator;
}

- (void)start {
  _mediator = [[TabGridToolbarsMediator alloc] init];

  [self setupTopToolbar];
  [self setupBottomToolbar];

  _mediator.topToolbarConsumer = self.topToolbar;
  _mediator.bottomToolbarConsumer = self.bottomToolbar;
}

#pragma mark - Property Implementation.

- (id<GridToolbarsMutator>)toolbarsMutator {
  CHECK(_mediator)
      << "TabGridToolbarsCoordinator's -start should be called before.";
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
}

#pragma mark - Control actions

- (void)pageControlChangedValue:(id)sender {
  [self.actionWrangler pageControlChangedValue:sender];
}

- (void)pageControlChangedPageByDrag:(id)sender {
  [self.actionWrangler pageControlChangedPageByDrag:sender];
}

- (void)pageControlChangedPageByTap:(id)sender {
  [self.actionWrangler pageControlChangedPageByTap:sender];
}

@end
