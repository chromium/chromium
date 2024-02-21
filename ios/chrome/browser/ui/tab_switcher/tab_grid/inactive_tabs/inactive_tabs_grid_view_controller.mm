// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_grid_view_controller.h"

#import "ios/web/public/web_state_id.h"

@implementation InactiveGridViewController

#pragma mark - Parent's functions

- (void)closeButtonTappedForCell:(GridCell*)cell {
  [self.delegate gridViewController:self
                 didCloseItemWithID:cell.itemIdentifier];
}

@end
