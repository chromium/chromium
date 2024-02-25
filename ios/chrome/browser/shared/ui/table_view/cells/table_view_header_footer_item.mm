// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"

@implementation TableViewHeaderFooterItem

- (instancetype)initWithType:(NSInteger)type {
  if ((self = [super initWithType:type])) {
    self.cellClass = [UITableViewHeaderFooterView class];
  }
  return self;
}

- (void)configureHeaderFooterView:(UITableViewHeaderFooterView*)headerFooter
                       withStyler:(ChromeTableViewStyler*)styler {
  DCHECK([headerFooter class] == self.cellClass);
  headerFooter.accessibilityTraits = self.accessibilityTraits;
  headerFooter.accessibilityIdentifier = self.accessibilityIdentifier;
  // Use the styler tableViewBackgroundColor (as a performance optimization) if
  // available.
  if (styler.tableViewBackgroundColor) {
    UIView* backgroundView = [[UIView alloc] init];
    backgroundView.backgroundColor = styler.tableViewBackgroundColor;
    headerFooter.backgroundView = backgroundView;
  }
}

@end
