// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"

@implementation SyncSwitchItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewSwitchCell class];
    self.enabled = YES;
  }
  return self;
}

#pragma mark TableViewItem

- (void)configureCell:(TableViewSwitchCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  [cell configureCellWithTitle:self.text
                      subtitle:self.detailText
                 switchEnabled:self.enabled
                            on:self.on];
}

@end
