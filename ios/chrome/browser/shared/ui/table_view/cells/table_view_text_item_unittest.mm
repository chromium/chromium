// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {
using TableViewTextItemTest = PlatformTest;
}

// Tests that the UILabels are set properly after a call to `configureCell:`.
TEST_F(TableViewTextItemTest, TextLabels) {
  NSString* text = @"Cell text";

  TableViewTextItem* item = [[TableViewTextItem alloc] initWithType:0];
  item.text = text;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewTextCell class]]);

  TableViewTextCell* textCell =
      base::apple::ObjCCastStrict<TableViewTextCell>(cell);
  EXPECT_FALSE(textCell.textLabel.text);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:textCell withStyler:styler];
  EXPECT_NSEQ(text, textCell.textLabel.text);
}

// Tests that item's text is shown as masked string in UILabel after a call to
// `configureCell:` with item.masked set to YES.
TEST_F(TableViewTextItemTest, MaskedTextLabels) {
  NSString* text = @"Cell text";

  TableViewTextItem* item = [[TableViewTextItem alloc] initWithType:0];
  item.text = text;
  item.masked = YES;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewTextCell class]]);

  TableViewTextCell* textCell =
      base::apple::ObjCCastStrict<TableViewTextCell>(cell);
  EXPECT_FALSE(textCell.textLabel.text);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:textCell withStyler:styler];
  EXPECT_NSEQ(kMaskedPassword, textCell.textLabel.text);
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
  EXPECT_NSEQ([UIColor colorNamed:kTextPrimaryColor], cell.textLabel.textColor);
}
