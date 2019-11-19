// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using TableViewTextItemTest = PlatformTest;
}

// Tests that the UILabels are set properly after a call to |configureCell:|.
TEST_F(TableViewTextItemTest, TextLabels) {
  NSString* text = @"Cell text";

  TableViewTextItem* item = [[TableViewTextItem alloc] initWithType:0];
  item.text = text;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewTextCell class]]);

  TableViewTextCell* textCell =
      base::mac::ObjCCastStrict<TableViewTextCell>(cell);
  EXPECT_FALSE(textCell.textLabel.text);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:textCell withStyler:styler];
  EXPECT_NSEQ(text, textCell.textLabel.text);
}

// Tests that item's text is shown as masked string in UILabel after a call to
// |configureCell:| with item.masked set to YES.
TEST_F(TableViewTextItemTest, MaskedTextLabels) {
  NSString* text = @"Cell text";

  TableViewTextItem* item = [[TableViewTextItem alloc] initWithType:0];
  item.text = text;
  item.masked = YES;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewTextCell class]]);

  TableViewTextCell* textCell =
      base::mac::ObjCCastStrict<TableViewTextCell>(cell);
  EXPECT_FALSE(textCell.textLabel.text);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:textCell withStyler:styler];
  EXPECT_NSEQ(kMaskedPassword, textCell.textLabel.text);
}

TEST_F(TableViewTextItemTest, ConfigureCellWithStyler) {
  TableViewTextItem* item = [[TableViewTextItem alloc] initWithType:0];
  TableViewTextCell* cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewTextCell class]]);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  UIColor* testTextColor = UIColor.redColor;
  styler.cellTitleColor = testTextColor;
  UIColor* testCellBackgroundColor = UIColor.blueColor;
  styler.tableViewBackgroundColor = testCellBackgroundColor;
  [item configureCell:cell withStyler:styler];
  EXPECT_NSEQ(testCellBackgroundColor, cell.backgroundColor);
  // TextLabel.backgroundColor has to be clear in (IOS 13) as the cell
  // background color doesn't apply to the textlabel background color anymore.
  EXPECT_NSEQ(UIColor.clearColor, cell.textLabel.backgroundColor);
  EXPECT_NSEQ(testTextColor, cell.textLabel.textColor);
}

TEST_F(TableViewTextItemTest, ConfigureLabelColorWithProperty) {
  TableViewTextItem* item = [[TableViewTextItem alloc] initWithType:0];
  UIColor* textColor = UIColor.blueColor;
  item.textColor = textColor;
  TableViewTextCell* cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewTextCell class]]);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  UIColor* testColor = UIColor.redColor;
  styler.tableViewBackgroundColor = testColor;
  [item configureCell:cell withStyler:styler];
  EXPECT_NSEQ(textColor, cell.textLabel.textColor);
  EXPECT_NSNE(testColor, cell.textLabel.textColor);
}

TEST_F(TableViewTextItemTest, ConfigureLabelColorWithDefaultColor) {
  TableViewTextItem* item = [[TableViewTextItem alloc] initWithType:0];
  TableViewTextCell* cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewTextCell class]]);
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];
  EXPECT_NSEQ(UIColor.cr_labelColor, cell.textLabel.textColor);
}
