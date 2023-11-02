// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using TableViewMultiDetailTextItemTest = PlatformTest;

// Tests that the UILabels are set properly after a call to `configureCell:`.
TEST_F(TableViewMultiDetailTextItemTest, TextLabels) {
  TableViewMultiDetailTextItem* item =
      [[TableViewMultiDetailTextItem alloc] initWithType:0];
  NSString* mainText = @"Main text";
  NSString* leadingDetailText = @"Leading detail text";
  NSString* trailingDetailText = @"Trailing detail text";

  item.text = mainText;
  item.leadingDetailText = leadingDetailText;
  item.trailingDetailText = trailingDetailText;
  item.accessoryType = UITableViewCellAccessoryCheckmark;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewMultiDetailTextCell class]]);

  TableViewMultiDetailTextCell* TableViewMultiDetailTextCell = cell;
  EXPECT_FALSE(TableViewMultiDetailTextCell.textLabel.text);
  EXPECT_FALSE(TableViewMultiDetailTextCell.leadingDetailTextLabel.text);
  EXPECT_FALSE(TableViewMultiDetailTextCell.trailingDetailTextLabel.text);
  EXPECT_EQ(UITableViewCellAccessoryNone,
            TableViewMultiDetailTextCell.accessoryType);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSEQ(mainText, TableViewMultiDetailTextCell.textLabel.text);
  EXPECT_NSEQ(leadingDetailText,
              TableViewMultiDetailTextCell.leadingDetailTextLabel.text);
  EXPECT_NSEQ(trailingDetailText,
              TableViewMultiDetailTextCell.trailingDetailTextLabel.text);
  EXPECT_EQ(UITableViewCellAccessoryCheckmark,
            TableViewMultiDetailTextCell.accessoryType);
}
