// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/cells/whats_new_table_view_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/symbols/chrome_icon.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {
using WhatsNewTableViewItemTest = PlatformTest;
}  // namespace

// Tests that the UILabels and icon are set properly after a call to
// configureCell.
TEST_F(WhatsNewTableViewItemTest, ItemProperties) {
  NSString* title = @"Title";
  NSString* detail_text = @"Detail text";

  WhatsNewTableViewItem* item = [[WhatsNewTableViewItem alloc] initWithType:0];
  item.title = title;
  item.detailText = detail_text;
  item.iconImage = [UIImage imageNamed:@"ic_search"];
  item.iconBackgroundColor = UIColor.blueColor;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[WhatsNewTableViewCell class]]);

  WhatsNewTableViewCell* whats_new_cell =
      base::apple::ObjCCastStrict<WhatsNewTableViewCell>(cell);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];

  // Check text label (title).
  EXPECT_NSEQ(title, whats_new_cell.textLabel.text);
  EXPECT_EQ(NO,
            whats_new_cell.textLabel.translatesAutoresizingMaskIntoConstraints);
  UIFont* font = CreateDynamicFont(UIFontTextStyleBody, UIFontWeightSemibold);
  EXPECT_EQ(font, whats_new_cell.textLabel.font);
  EXPECT_EQ(YES, whats_new_cell.textLabel.adjustsFontForContentSizeCategory);
  EXPECT_EQ(2, whats_new_cell.textLabel.numberOfLines);

  // Check detail text label.
  EXPECT_NSEQ(detail_text, whats_new_cell.detailTextLabel.text);
  EXPECT_EQ(
      NO,
      whats_new_cell.detailTextLabel.translatesAutoresizingMaskIntoConstraints);
  UIFont* detailFont =
      CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightRegular);
  EXPECT_EQ(detailFont, whats_new_cell.detailTextLabel.font);
  EXPECT_EQ(YES,
            whats_new_cell.detailTextLabel.adjustsFontForContentSizeCategory);
  EXPECT_EQ(3, whats_new_cell.detailTextLabel.numberOfLines);

  // Check that the main background is set properly.
  UIImageView* main_background_image_view =
      base::apple::ObjCCastStrict<UIImageView>(
          whats_new_cell.contentView.subviews[0]);
  EXPECT_NSEQ([[UIImage imageNamed:@"whats_new_icon_tile"]
                  imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate],
              main_background_image_view.image);

  // Check that the background icon view is set properly.
  UIImageView* icon_background_image_view =
      whats_new_cell.iconBackgroundImageView;
  EXPECT_NSEQ([[UIImage imageNamed:@"whats_new_icon_tile"]
                  imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate],
              icon_background_image_view.image);
  EXPECT_EQ(UIColor.blueColor, icon_background_image_view.tintColor);

  // Check that icon view is set properly .
  UIImageView* icon_view = whats_new_cell.iconView;
  EXPECT_NSEQ([ChromeIcon searchIcon], icon_view.image);
}

// Tests that the icon background in hidden when iconBackgroundImageView is not
// set.
TEST_F(WhatsNewTableViewItemTest, ItemWithoutBackgroundImageView) {
  NSString* title = @"Title";
  NSString* detail_text = @"Detail text";

  WhatsNewTableViewItem* item = [[WhatsNewTableViewItem alloc] initWithType:0];
  item.title = title;
  item.detailText = detail_text;
  item.iconImage = [UIImage imageNamed:@"ic_search"];

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[WhatsNewTableViewCell class]]);

  WhatsNewTableViewCell* whats_new_cell =
      base::apple::ObjCCastStrict<WhatsNewTableViewCell>(cell);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];

  // Check text label (title).
  EXPECT_NSEQ(title, whats_new_cell.textLabel.text);

  EXPECT_EQ(NO,
            whats_new_cell.textLabel.translatesAutoresizingMaskIntoConstraints);
  UIFont* font = CreateDynamicFont(UIFontTextStyleBody, UIFontWeightSemibold);
  EXPECT_EQ(font, whats_new_cell.textLabel.font);
  EXPECT_EQ(YES, whats_new_cell.textLabel.adjustsFontForContentSizeCategory);
  EXPECT_EQ(2, whats_new_cell.textLabel.numberOfLines);

  // Check detail text label.
  EXPECT_NSEQ(detail_text, whats_new_cell.detailTextLabel.text);
  EXPECT_EQ(
      NO,
      whats_new_cell.detailTextLabel.translatesAutoresizingMaskIntoConstraints);
  UIFont* detailFont =
      CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightRegular);
  EXPECT_EQ(detailFont, whats_new_cell.detailTextLabel.font);
  EXPECT_EQ(YES,
            whats_new_cell.detailTextLabel.adjustsFontForContentSizeCategory);
  EXPECT_EQ(3, whats_new_cell.detailTextLabel.numberOfLines);

  // Check that the main background is set properly.
  UIImageView* main_background_image_view =
      base::apple::ObjCCastStrict<UIImageView>(
          whats_new_cell.contentView.subviews[0]);
  EXPECT_NSEQ([[UIImage imageNamed:@"whats_new_icon_tile"]
                  imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate],
              main_background_image_view.image);

  // Check that the background icon view is hidden.
  UIImageView* icon_background_image_view =
      whats_new_cell.iconBackgroundImageView;
  EXPECT_NSEQ([[UIImage imageNamed:@"whats_new_icon_tile"]
                  imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate],
              icon_background_image_view.image);
  EXPECT_EQ(YES, icon_background_image_view.hidden);

  // Check that icon view is set properly .
  UIImageView* icon_view = whats_new_cell.iconView;

  CGSize expectedSize = CGSizeMake(30, 30);
  EXPECT_EQ(expectedSize.height, [icon_view.image size].height);
  EXPECT_EQ(expectedSize.width, [icon_view.image size].width);
}
