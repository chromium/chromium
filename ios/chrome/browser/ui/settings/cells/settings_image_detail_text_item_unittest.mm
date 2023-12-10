// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_cell.h"
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
  ASSERT_TRUE([cell isMemberOfClass:[SettingsImageDetailTextCell class]]);

  SettingsImageDetailTextCell* imageDetailCell =
      static_cast<SettingsImageDetailTextCell*>(cell);

  EXPECT_FALSE(imageDetailCell.textLabel.text);
  EXPECT_FALSE(imageDetailCell.detailTextLabel.text);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSEQ(text, imageDetailCell.textLabel.text);
  EXPECT_NSEQ(detailText, imageDetailCell.detailTextLabel.text);
  EXPECT_NSEQ([UIColor colorNamed:kTextSecondaryColor],
              imageDetailCell.detailTextLabel.textColor);
  EXPECT_NSEQ(image, imageDetailCell.image);
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
  ASSERT_TRUE([cell isMemberOfClass:[SettingsImageDetailTextCell class]]);

  SettingsImageDetailTextCell* imageDetailCell =
      static_cast<SettingsImageDetailTextCell*>(cell);

  EXPECT_FALSE(imageDetailCell.textLabel.attributedText);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSEQ(attributeString, imageDetailCell.textLabel.attributedText);
}

// Tests that the detail text color is updated when detailTextColor is not nil.
TEST_F(SettingsImageDetailTextItemTest, SetDetailTextColor) {
  SettingsImageDetailTextItem* item =
      [[SettingsImageDetailTextItem alloc] initWithType:0];
  NSString* text = @"Test Text";
  NSString* detailText = @"Test Detail Text";
  item.text = text;
  item.detailText = detailText;
  item.detailTextColor = UIColor.blueColor;
  item.image = [[UIImage alloc] init];

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[SettingsImageDetailTextCell class]]);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];

  SettingsImageDetailTextCell* imageDetailCell =
      static_cast<SettingsImageDetailTextCell*>(cell);

  EXPECT_NSEQ(UIColor.blueColor, imageDetailCell.detailTextLabel.textColor);
}

// Tests that the text color is updated when textColor is not nil.
TEST_F(SettingsImageDetailTextItemTest, SetTextColor) {
  SettingsImageDetailTextItem* item =
      [[SettingsImageDetailTextItem alloc] initWithType:0];
  NSString* text = @"Test Text";
  item.text = text;
  item.textColor = UIColor.redColor;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[SettingsImageDetailTextCell class]]);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];

  SettingsImageDetailTextCell* imageDetailCell =
      static_cast<SettingsImageDetailTextCell*>(cell);

  EXPECT_NSEQ(UIColor.redColor, imageDetailCell.textLabel.textColor);
}

// Tests that the text, detail text and image are honoured after a call to
// `configureCell:`, and then a second call.
TEST_F(SettingsImageDetailTextItemTest, ConfigureCellTwice) {
  SettingsImageDetailTextItem* item =
      [[SettingsImageDetailTextItem alloc] initWithType:0];
  NSString* text = @"Test Text";
  NSString* detailText = @"Test Detail Text";
  UIColor* detailTextColor = UIColor.whiteColor;
  UIImage* image = [[UIImage alloc] init];
  item.image = image;
  item.text = text;
  item.detailText = detailText;
  item.detailTextColor = detailTextColor;

  id cell = [[[item cellClass] alloc] init];
  SettingsImageDetailTextCell* imageDetailCell =
      static_cast<SettingsImageDetailTextCell*>(cell);

  EXPECT_FALSE(imageDetailCell.textLabel.text);
  EXPECT_FALSE(imageDetailCell.detailTextLabel.text);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSEQ(text, imageDetailCell.textLabel.text);
  EXPECT_NSEQ(detailText, imageDetailCell.detailTextLabel.text);
  EXPECT_NSEQ(UIColor.whiteColor, imageDetailCell.detailTextLabel.textColor);
  EXPECT_NSEQ(image, imageDetailCell.image);

  // Change the text, the detail text, and the detail text color to new values.
  text = @"Test Text2";
  detailText = @"Test Detail Text2";
  item.text = text;
  item.detailText = detailText;
  item.detailTextColor = nil;
  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSEQ(text, imageDetailCell.textLabel.text);
  EXPECT_NSEQ(detailText, imageDetailCell.detailTextLabel.text);
  EXPECT_NSEQ([UIColor colorNamed:kTextSecondaryColor],
              imageDetailCell.detailTextLabel.textColor);
}
