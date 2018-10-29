// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

#import "base/logging.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TableViewItem

- (instancetype)initWithType:(NSInteger)type {
  if ((self = [super initWithType:type])) {
    self.cellClass = [UITableViewCell class];
  }
  return self;
}

- (void)configureCell:(UITableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  DCHECK(styler);
  DCHECK([cell class] == self.cellClass);
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
