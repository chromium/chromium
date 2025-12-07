// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/cells/sync_switch_item.h"

#import "ios/chrome/browser/shared/ui/table_view/cells/legacy_table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/switch_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"

@implementation SyncSwitchItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [LegacyTableViewCell class];
    self.enabled = YES;
  }
  return self;
}

#pragma mark TableViewItem

- (void)configureCell:(LegacyTableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  TableViewCellContentConfiguration* contentConfiguration =
      [[TableViewCellContentConfiguration alloc] init];
  contentConfiguration.title = self.text;
  contentConfiguration.subtitle = self.detailText;
  contentConfiguration.textDisabled = !self.enabled;

  SwitchContentConfiguration* switchConfiguration =
      [[SwitchContentConfiguration alloc] init];
  switchConfiguration.enabled = self.enabled;
  switchConfiguration.on = self.on;
  switchConfiguration.target = self.target;
  switchConfiguration.selector = self.selector;
  switchConfiguration.tag = self.tag;

  contentConfiguration.trailingConfiguration = switchConfiguration;
  cell.contentConfiguration = contentConfiguration;
  cell.accessibilityLabel = contentConfiguration.accessibilityLabel;
  cell.accessibilityValue = contentConfiguration.accessibilityValue;
  cell.accessibilityHint = contentConfiguration.accessibilityHint;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
}

- (LegacyTableViewCell*)cellForTableView:(UITableView*)tableView {
  [TableViewCellContentConfiguration legacyRegisterCellForTableView:tableView];
  return
      [TableViewCellContentConfiguration legacyDequeueTableViewCell:tableView];
}

@end
