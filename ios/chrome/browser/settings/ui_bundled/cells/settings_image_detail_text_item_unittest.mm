// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/cells/settings_image_detail_text_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/legacy_table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using SettingsImageDetailTextItemTest = PlatformTest;

// Tests that the text, detail text and image are honoured after a call to
// `configureCell:`.
TEST_F(SettingsImageDetailTextItemTest, ConfigureCell) {
  SettingsImageDetailTextItem* item =
      [[SettingsImageDetailTextItem alloc] initWithType:0];
  NSString* text = @"Test Text";
  NSString* detailText = @"Test Detail Text";
  UIImage* image = [[UIImage alloc] init];
  item.image = image;
  item.text = text;
  item.detailText = detailText;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[LegacyTableViewCell class]]);

  LegacyTableViewCell* imageDetailCell =
      base::apple::ObjCCastStrict<LegacyTableViewCell>(cell);
  EXPECT_EQ(nil, imageDetailCell.contentConfiguration);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NE(nil, imageDetailCell.contentConfiguration);
  ASSERT_TRUE([imageDetailCell.contentConfiguration
      isMemberOfClass:TableViewCellContentConfiguration.class]);

  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          imageDetailCell.contentConfiguration);
  EXPECT_NSEQ(text, configuration.title);
  EXPECT_NSEQ(detailText, configuration.subtitle);
  EXPECT_NE(nil, configuration.leadingConfiguration);

  ASSERT_TRUE([configuration.leadingConfiguration
      isMemberOfClass:ImageContentConfiguration.class]);

  ImageContentConfiguration* image_configuration =
      base::apple::ObjCCastStrict<ImageContentConfiguration>(
          configuration.leadingConfiguration);

  EXPECT_NSEQ(image, image_configuration.image);
}

// Tests that the attributed text is honoured after a call to
// `configureCell:`.
TEST_F(SettingsImageDetailTextItemTest, ConfigureAttributedText) {
  SettingsImageDetailTextItem* item =
      [[SettingsImageDetailTextItem alloc] initWithType:0];
  NSAttributedString* attributeString =
      [[NSAttributedString alloc] initWithString:@"Test Attributed Text"];
  item.attributedText = attributeString;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[LegacyTableViewCell class]]);

  LegacyTableViewCell* imageDetailCell =
      base::apple::ObjCCastStrict<LegacyTableViewCell>(cell);
  EXPECT_EQ(nil, imageDetailCell.contentConfiguration);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NE(nil, imageDetailCell.contentConfiguration);
  ASSERT_TRUE([imageDetailCell.contentConfiguration
      isMemberOfClass:TableViewCellContentConfiguration.class]);

  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          imageDetailCell.contentConfiguration);
  EXPECT_NSEQ(attributeString, configuration.attributedTitle);
}

// Tests that the detail text color is updated when text colors are not nil.
TEST_F(SettingsImageDetailTextItemTest, SetTextColor) {
  SettingsImageDetailTextItem* item =
      [[SettingsImageDetailTextItem alloc] initWithType:0];
  NSString* text = @"Test Text";
  NSString* detailText = @"Test Detail Text";
  item.text = text;
  item.textColor = UIColor.redColor;
  item.detailText = detailText;
  item.detailTextColor = UIColor.blueColor;
  item.image = [[UIImage alloc] init];

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[LegacyTableViewCell class]]);

  LegacyTableViewCell* imageDetailCell =
      base::apple::ObjCCastStrict<LegacyTableViewCell>(cell);
  EXPECT_EQ(nil, imageDetailCell.contentConfiguration);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NE(nil, imageDetailCell.contentConfiguration);
  ASSERT_TRUE([imageDetailCell.contentConfiguration
      isMemberOfClass:TableViewCellContentConfiguration.class]);

  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          imageDetailCell.contentConfiguration);
  EXPECT_NSEQ(UIColor.redColor, configuration.titleColor);
  EXPECT_NSEQ(UIColor.blueColor, configuration.subtitleColor);
}
