// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"

@implementation TableViewItem

- (instancetype)initWithType:(NSInteger)type {
  if ((self = [super initWithType:type])) {
    _useCustomSeparator = NO;

    self.cellClass = [TableViewCell class];
  }
  return self;
}

- (void)configureCell:(TableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  DCHECK(styler);
  DCHECK([cell class] == self.cellClass);
  DCHECK([cell isKindOfClass:[TableViewCell class]]);
  cell.accessoryType = self.accessoryType;
  cell.editingAccessoryType = self.editingAccessoryType;
  cell.accessoryView = self.accessoryView;
  cell.useCustomSeparator = self.useCustomSeparator;
  cell.accessibilityTraits = self.accessibilityTraits;
  cell.accessibilityIdentifier = self.accessibilityIdentifier;
  if (!cell.backgroundView) {
    if (styler.cellBackgroundColor) {
      cell.backgroundColor = styler.cellBackgroundColor;
    } else {
      cell.backgroundColor = styler.tableViewBackgroundColor;
    }
  }
  // Since this Cell might get reconfigured while it's being highlighted,
  // re-setting the selectedBackgroundView will interrupt the higlight
  // animation. Make sure that if the cell already has the correct
  // selectedBackgroundView it doesn't get set again.
  if (styler.cellHighlightColor && ![cell.selectedBackgroundView.backgroundColor
                                       isEqual:styler.cellHighlightColor]) {
    UIView* selectedBackgroundView = [[UIView alloc] init];
    selectedBackgroundView.backgroundColor = styler.cellHighlightColor;
    cell.selectedBackgroundView = selectedBackgroundView;
  }
}

@end
