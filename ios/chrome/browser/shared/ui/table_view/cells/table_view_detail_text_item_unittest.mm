// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
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
  ASSERT_TRUE([cell isMemberOfClass:[LegacyTableViewCell class]]);
  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];

  ASSERT_TRUE([[cell contentConfiguration]
      isMemberOfClass:TableViewCellContentConfiguration.class]);

  TableViewCellContentConfiguration* content_configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          [cell contentConfiguration]);

  EXPECT_NSEQ(text, content_configuration.title);
  EXPECT_NSEQ(detailText, content_configuration.subtitle);
  EXPECT_NSEQ(textColor, content_configuration.titleColor);
  EXPECT_NSEQ(detailTextColor, content_configuration.subtitleColor);
}

// Tests the accessory symbol is set and unset.
TEST_F(TableViewDetailTextItemTest, ItemPropertiesAccessorySymbol) {
  TableViewDetailTextItem* item =
      [[TableViewDetailTextItem alloc] initWithType:0];
  LegacyTableViewCell* cell = [[[item cellClass] alloc] init];

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
  LegacyTableViewCell* cell = [[[item cellClass] alloc] init];

  item.accessorySymbol = TableViewDetailTextCellAccessorySymbolExternalLink;
  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSNE(nil, cell.accessoryView);

  [cell prepareForReuse];
  EXPECT_NSEQ(nil, cell.accessoryView);
}
