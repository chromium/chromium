// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_item.h"

#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_cell.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TableViewSwitchItem

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
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;
  cell.switchView.enabled = self.enabled;
  cell.switchView.on = self.on;
  cell.switchView.accessibilityIdentifier =
      [NSString stringWithFormat:@"%@, %@", self.text, @"switch"];
  cell.textLabel.textColor =
      [TableViewSwitchCell defaultTextColorForState:cell.switchView.state];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;

  [cell setIconImage:self.iconImage
            tintColor:self.iconTintColor
      backgroundColor:self.iconBackgroundColor
         cornerRadius:self.iconCornerRadius];
}

@end
