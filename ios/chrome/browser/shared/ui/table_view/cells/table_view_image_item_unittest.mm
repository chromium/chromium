// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {
using TableViewImageItemTest = PlatformTest;
}

// Tests that the UILabel is set properly after a call to
// `configureCell:` and the image are visible.
TEST_F(TableViewImageItemTest, ItemProperties) {
  NSString* text = @"Cell text";
  NSString* detailText = @"Detail text";

  TableViewImageItem* item = [[TableViewImageItem alloc] initWithType:0];
  item.title = text;
  item.detailText = detailText;
  item.image = [[UIImage alloc] init];

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[LegacyTableViewCell class]]);

  LegacyTableViewCell* imageCell =
      base::apple::ObjCCastStrict<LegacyTableViewCell>(cell);
  EXPECT_EQ(nil, imageCell.contentConfiguration);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NE(nil, imageCell.contentConfiguration);
  ASSERT_TRUE([imageCell.contentConfiguration
      isMemberOfClass:TableViewCellContentConfiguration.class]);

  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          imageCell.contentConfiguration);
  EXPECT_NSEQ(text, configuration.title);
  EXPECT_NSEQ(detailText, configuration.subtitle);
  EXPECT_NE(nil, configuration.leadingConfiguration);
}

// Tests that the imageView is not visible if no image is set.
TEST_F(TableViewImageItemTest, ItemImageViewHidden) {
  NSString* text = @"Cell text";

  TableViewImageItem* item = [[TableViewImageItem alloc] initWithType:0];
  item.title = text;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[LegacyTableViewCell class]]);

  LegacyTableViewCell* imageCell =
      base::apple::ObjCCastStrict<LegacyTableViewCell>(cell);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  ASSERT_TRUE([imageCell.contentConfiguration
      isMemberOfClass:TableViewCellContentConfiguration.class]);

  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          imageCell.contentConfiguration);
  EXPECT_EQ(nil, configuration.leadingConfiguration);
}
