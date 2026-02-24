// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_other_data/ui/quick_delete_other_data_view_controller.h"

#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_other_data/public/quick_delete_other_data_commands.h"

@interface QuickDeleteOtherDataViewController () {
  // The title for the "Quick Delete Other Data" page.
  NSString* _otherDataPageTitle;
  // The subtitle for the "Search history" cell.
  NSString* _searchHistoryCellSubtitle;
  // Tells if the "My Activity" cell is visible in the table view.
  BOOL _shouldShowMyActivityCell;
  // Tells if the "Search history" cell is visible in the table view.
  BOOL _shouldShowSearchHistoryCell;
}
@end

@implementation QuickDeleteOtherDataViewController

// TODO(crbug.com/464551506): Add the implementation for the view controller.
- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.quickDeleteOtherDataHandler hideQuickDeleteOtherDataPage];
  }
}

#pragma mark - QuickDeleteOtherDataConsumer

// TODO(crbug.com/471025894) Make the table view use the consumer's methods to
// show different UI.
- (void)setOtherDataPageTitle:(NSString*)title {
  _otherDataPageTitle = title;
}

- (void)setSearchHistoryCellSubtitle:(NSString*)subtitle {
  _searchHistoryCellSubtitle = subtitle;
}

- (void)setShouldShowMyActivityCell:(BOOL)shouldShowMyActivityCell {
  _shouldShowMyActivityCell = shouldShowMyActivityCell;
}

- (void)setShouldShowSearchHistoryCell:(BOOL)shouldShowSearchHistoryCell {
  _shouldShowSearchHistoryCell = shouldShowSearchHistoryCell;
}

@end
