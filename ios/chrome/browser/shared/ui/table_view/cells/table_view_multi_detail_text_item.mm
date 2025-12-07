// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation TableViewMultiDetailTextItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [LegacyTableViewCell class];
  }
  return self;
}

#pragma mark - TableViewItem

- (void)configureCell:(LegacyTableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  TableViewCellContentConfiguration* contentConfiguration =
      [[TableViewCellContentConfiguration alloc] init];
  contentConfiguration.title = self.text;
  contentConfiguration.subtitle = self.leadingDetailText;
  contentConfiguration.trailingText = self.trailingDetailText;
  contentConfiguration.trailingTextColor = self.trailingDetailTextColor;
  cell.contentConfiguration = contentConfiguration;
  cell.accessibilityLabel = contentConfiguration.accessibilityLabel;
  cell.accessibilityValue = contentConfiguration.accessibilityValue;
  cell.accessibilityHint = contentConfiguration.accessibilityHint;
}

- (LegacyTableViewCell*)cellForTableView:(UITableView*)tableView {
  [TableViewCellContentConfiguration legacyRegisterCellForTableView:tableView];
  return
      [TableViewCellContentConfiguration legacyDequeueTableViewCell:tableView];
}

@end
