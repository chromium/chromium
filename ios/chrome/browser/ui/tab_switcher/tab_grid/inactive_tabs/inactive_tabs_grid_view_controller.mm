// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_grid_view_controller.h"

#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_view_controller_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_coordinator_delegate.h"
#import "ios/web/public/web_state_id.h"

namespace {

// When the inactive tabs grid would be emptied (last inactive tab, or closing
// all inactive tabs via the confirmation dialog), the Inactive Tabs grid is
// popped, but to avoid having it emptied immediately (producing a glitch),
// delay the closing of the tab(s) in the mediator.
const base::TimeDelta kPopUIDelay = base::Seconds(0.3);

}  // namespace

@implementation InactiveGridViewController

#pragma mark - Parent's functions

- (void)closeButtonTappedForCell:(GridCell*)cell {
  __weak __typeof(self) weakSelf = self;
  auto closeItem = ^{
    [weakSelf.mutator closeItemID:cell.itemIdentifier];
  };

  NSInteger numberOfTabs = [self numberOfTabs];
  // If it is the latest item, pop the view (UI change), and defer the model
  // change after the UI is no longer visible.
  if (numberOfTabs <= 1) {
    // Pop the view controller.
    [self.inactiveTabsDelegate inactiveTabsCoordinatorDidFinish:nil];
    // To prevent the Inactive Tabs grid from being immediately emptied, defer
    // the closing to after the view is popped.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(closeItem), kPopUIDelay);
  } else {
    // Otherwise, close the item immediately.
    closeItem();
  }
}

@end
