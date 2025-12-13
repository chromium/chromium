// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
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

  id originalCell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([originalCell isMemberOfClass:[LegacyTableViewCell class]]);
  LegacyTableViewCell* cell = originalCell;

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];

  id<UIContentConfiguration> contentConfiguration = cell.contentConfiguration;
  ASSERT_TRUE([contentConfiguration
      isMemberOfClass:TableViewCellContentConfiguration.class]);

  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCast<TableViewCellContentConfiguration>(
          contentConfiguration);
  EXPECT_NSEQ(mainText, configuration.title);
  EXPECT_NSEQ(leadingDetailText, configuration.subtitle);
  EXPECT_NSEQ(trailingDetailText, configuration.trailingText);
  EXPECT_EQ(UITableViewCellAccessoryCheckmark, cell.accessoryType);
}
