// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using TableViewItemTest = PlatformTest;

TEST_F(TableViewItemTest, ConfigureCellPortsAccessibilityProperties) {
  TableViewItem* item = [[TableViewItem alloc] initWithType:0];
  item.accessibilityIdentifier = @"test_identifier";
  item.accessibilityTraits = UIAccessibilityTraitButton;
  TableViewCell* cell = [[[item cellClass] alloc] init];
  EXPECT_TRUE([cell isMemberOfClass:[TableViewCell class]]);
  EXPECT_EQ(UIAccessibilityTraitNone, [cell accessibilityTraits]);
  EXPECT_FALSE([cell accessibilityIdentifier]);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];
  EXPECT_EQ(UIAccessibilityTraitButton, [cell accessibilityTraits]);
  EXPECT_NSEQ(@"test_identifier", [cell accessibilityIdentifier]);
}

TEST_F(TableViewItemTest, NoBackgroundColorIfBackgroundViewIsPresent) {
  TableViewItem* item = [[TableViewItem alloc] initWithType:0];
  TableViewCell* cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewCell class]]);

  // If a background view is present on the cell, the styler's background color
  // should be ignored.
  cell.backgroundView = [[UIView alloc] init];

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  UIColor* testColor = UIColor.redColor;
  styler.tableViewBackgroundColor = testColor;
  [item configureCell:cell withStyler:styler];
  EXPECT_FALSE([testColor isEqual:cell.backgroundColor]);
}

TEST_F(TableViewItemTest, ConfigureCellAccessoryViewProperties) {
  UIImageView* expectedImage = [[UIImageView alloc]
      initWithImage:DefaultSymbolTemplateWithPointSize(
                        kChevronForwardSymbol, kSymbolAccessoryPointSize)];
  TableViewItem* item = [[TableViewItem alloc] initWithType:0];
  item.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  item.accessoryView = expectedImage;

  TableViewCell* cell = [[[item cellClass] alloc] init];
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];
  // Internally in UITableViewCell, accessoryView takes precedence over
  // accessoryType property.
  EXPECT_EQ(cell.accessoryType, UITableViewCellAccessoryDisclosureIndicator);
  EXPECT_EQ(cell.accessoryView, expectedImage);
}

}  // namespace
