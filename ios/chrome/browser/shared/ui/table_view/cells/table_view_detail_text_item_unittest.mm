// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {
using TableViewDetailTextItemTest = PlatformTest;
}

// Tests that the UILabels are set properly after a call to
// `configureCell:`.
TEST_F(TableViewDetailTextItemTest, ItemProperties) {
  NSString* text = @"Cell text";
  NSString* detailText = @"Cell detail text";
  UIColor* textColor = UIColor.yellowColor;
  UIColor* detailTextColor = UIColor.blueColor;

  TableViewDetailTextItem* item =
      [[TableViewDetailTextItem alloc] initWithType:0];
  item.text = text;
  item.detailText = detailText;
  item.textColor = textColor;
  item.detailTextColor = detailTextColor;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewDetailTextCell class]]);

  TableViewDetailTextCell* detailCell =
      base::apple::ObjCCastStrict<TableViewDetailTextCell>(cell);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  styler.cellTitleColor = UIColor.redColor;
  ASSERT_FALSE([styler.cellTitleColor isEqual:textColor]);
  [item configureCell:cell withStyler:styler];
  EXPECT_NSEQ(text, detailCell.textLabel.text);
  EXPECT_NSEQ(detailText, detailCell.detailTextLabel.text);
  EXPECT_NSEQ(textColor, detailCell.textLabel.textColor);
  EXPECT_NSEQ(detailTextColor, detailCell.detailTextLabel.textColor);
}

// Tests that the color of the text label is decided by the styler is the color
// is not set on the item.
TEST_F(TableViewDetailTextItemTest, ItemPropertiesStylerColor) {
  NSString* text = @"Cell text";
  NSString* detailText = @"Cell detail text";
  UIColor* titleColor = UIColor.blueColor;

  TableViewDetailTextItem* item =
      [[TableViewDetailTextItem alloc] initWithType:0];
  item.text = text;
  item.detailText = detailText;

  TableViewDetailTextCell* cell = [[[item cellClass] alloc] init];

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  styler.cellTitleColor = titleColor;
  [item configureCell:cell withStyler:styler];
  EXPECT_NSEQ(text, cell.textLabel.text);
  EXPECT_NSEQ(titleColor, cell.textLabel.textColor);
}

// Tests the default color of the labels is none is set on the item or the
// styler.
TEST_F(TableViewDetailTextItemTest, ItemPropertiesDefaultColor) {
  NSString* text = @"Cell text";
  NSString* detailText = @"Cell detail text";

  TableViewDetailTextItem* item =
      [[TableViewDetailTextItem alloc] initWithType:0];
  item.text = text;
  item.detailText = detailText;

  TableViewDetailTextCell* cell = [[[item cellClass] alloc] init];

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSEQ(text, cell.textLabel.text);
  EXPECT_NSEQ(detailText, cell.detailTextLabel.text);
  EXPECT_NSEQ([UIColor colorNamed:kTextPrimaryColor], cell.textLabel.textColor);
  EXPECT_NSEQ([UIColor colorNamed:kTextSecondaryColor],
              cell.detailTextLabel.textColor);
}

// Tests the accessory symbol is set and unset.
TEST_F(TableViewDetailTextItemTest, ItemPropertiesAccessorySymbol) {
  TableViewDetailTextItem* item =
      [[TableViewDetailTextItem alloc] initWithType:0];
  TableViewDetailTextCell* cell = [[[item cellClass] alloc] init];

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSEQ(nil, cell.accessoryView);

  item.accessorySymbol = TableViewDetailTextCellAccessorySymbolChevron;
  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSNE(nil, cell.accessoryView);

  item.accessorySymbol = TableViewDetailTextCellAccessorySymbolNone;
  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSEQ(nil, cell.accessoryView);
}

// Tests the accessory view is nil after cell prepare for reuse.
TEST_F(TableViewDetailTextItemTest, CellPrepareForReuseAccessorySymbolNil) {
  TableViewDetailTextItem* item =
      [[TableViewDetailTextItem alloc] initWithType:0];
  TableViewDetailTextCell* cell = [[[item cellClass] alloc] init];

  item.accessorySymbol = TableViewDetailTextCellAccessorySymbolExternalLink;
  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSNE(nil, cell.accessoryView);

  [cell prepareForReuse];
  EXPECT_NSEQ(nil, cell.accessoryView);
}
