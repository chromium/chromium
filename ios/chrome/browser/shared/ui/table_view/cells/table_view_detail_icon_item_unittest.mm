// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/symbols/chrome_icon.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {
using TableViewDetailIconItemTest = PlatformTest;

// Returns the UIImageView containing the icon within the cell.
UIImageView* GetImageView(TableViewDetailIconCell* cell) {
  return base::apple::ObjCCastStrict<UIImageView>(
      cell.contentView.subviews[0].subviews[0]);
}

// Returns the UIView containing the Image View.
UIView* GetImageBackgroundView(TableViewDetailIconCell* cell) {
  return cell.contentView.subviews[0];
}

}  // namespace

// Tests that the UILabels and icons are set properly after a call to
// `configureCell:`.
TEST_F(TableViewDetailIconItemTest, ItemProperties) {
  NSString* text = @"Cell text";
  NSString* detail_text = @"Cell detail text";

  TableViewDetailIconItem* item =
      [[TableViewDetailIconItem alloc] initWithType:0];
  item.text = text;
  item.detailText = detail_text;
  item.iconImage = [UIImage imageNamed:@"ic_search"];
  item.iconTintColor = UIColor.whiteColor;
  item.iconBackgroundColor = UIColor.blackColor;
  item.iconCornerRadius = 8;
  item.textLayoutConstraintAxis = UILayoutConstraintAxisVertical;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewDetailIconCell class]]);

  TableViewDetailIconCell* detail_cell =
      base::apple::ObjCCastStrict<TableViewDetailIconCell>(cell);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];

  // Check text-based properties.
  EXPECT_NSEQ(text, detail_cell.textLabel.text);
  EXPECT_NSEQ(detail_text, detail_cell.detailTextLabel.text);
  EXPECT_EQ(UILayoutConstraintAxisVertical,
            detail_cell.textLayoutConstraintAxis);
  EXPECT_EQ([UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
            detail_cell.detailTextLabel.font);

  // Check image-based property.
  UIImageView* image_view = GetImageView(detail_cell);
  UIView* image_background = GetImageBackgroundView(detail_cell);
  EXPECT_NSEQ([[ChromeIcon searchIcon]
                  imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate],
              image_view.image);
  EXPECT_EQ(UIColor.whiteColor, image_view.tintColor);
  EXPECT_EQ(UIColor.blackColor, image_background.backgroundColor);
  EXPECT_EQ(8, image_background.layer.cornerRadius);
}

// Tests that the icon image is updated when set from cell.
TEST_F(TableViewDetailIconItemTest, iconImageUpdate) {
  TableViewDetailIconItem* item =
      [[TableViewDetailIconItem alloc] initWithType:0];
  item.iconImage = [UIImage imageNamed:@"ic_search"];

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewDetailIconCell class]]);

  TableViewDetailIconCell* detail_cell =
      base::apple::ObjCCastStrict<TableViewDetailIconCell>(cell);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];

  // Check original image is set.
  UIImageView* image_view = GetImageView(detail_cell);
  UIView* image_background = GetImageBackgroundView(detail_cell);
  EXPECT_NSEQ([ChromeIcon searchIcon], image_view.image);
  EXPECT_NE(UIColor.whiteColor, image_view.tintColor);
  EXPECT_EQ(nil, image_background.backgroundColor);
  EXPECT_EQ(0, image_background.layer.cornerRadius);

  [detail_cell setIconImage:[ChromeIcon infoIcon]
                  tintColor:UIColor.whiteColor
            backgroundColor:UIColor.blackColor
               cornerRadius:8];

  // Check new image is set.
  EXPECT_NSEQ([[ChromeIcon infoIcon]
                  imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate],
              image_view.image);
  EXPECT_EQ(UIColor.whiteColor, image_view.tintColor);
  EXPECT_EQ(UIColor.blackColor, image_background.backgroundColor);
  EXPECT_EQ(8, image_background.layer.cornerRadius);
}

// Tests that the icon image is removed when icon is set to nil from cell.
TEST_F(TableViewDetailIconItemTest, iconImageNilUpdate) {
  TableViewDetailIconItem* item =
      [[TableViewDetailIconItem alloc] initWithType:0];
  item.iconImage = [UIImage imageNamed:@"ic_search"];
  item.iconTintColor = UIColor.whiteColor;
  item.iconBackgroundColor = UIColor.blackColor;
  item.iconCornerRadius = 8;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewDetailIconCell class]]);

  TableViewDetailIconCell* detail_cell =
      base::apple::ObjCCastStrict<TableViewDetailIconCell>(cell);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];

  // Check original image is set.
  UIImageView* image_view = GetImageView(detail_cell);
  UIView* image_background = GetImageBackgroundView(detail_cell);
  EXPECT_NSEQ([[ChromeIcon searchIcon]
                  imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate],
              image_view.image);
  EXPECT_EQ(UIColor.whiteColor, image_view.tintColor);
  EXPECT_EQ(UIColor.blackColor, image_background.backgroundColor);
  EXPECT_EQ(8, image_background.layer.cornerRadius);

  [detail_cell setIconImage:nil
                  tintColor:nil
            backgroundColor:nil
               cornerRadius:0];

  // Check image is set to nil.
  EXPECT_NSEQ(nil, image_view.image);
  EXPECT_NE(UIColor.whiteColor, image_view.tintColor);
  EXPECT_EQ(nil, image_background.backgroundColor);
  EXPECT_EQ(0, image_background.layer.cornerRadius);
}

// Tests that the UI layout constraint axis for the text labels is updated to
// vertical when set from cell.
TEST_F(TableViewDetailIconItemTest, ItemUpdateUILayoutConstraintAxisVertical) {
  TableViewDetailIconItem* item =
      [[TableViewDetailIconItem alloc] initWithType:0];
  item.text = @"Jane Doe";
  item.detailText = @"janedoe@gmail.com";

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewDetailIconCell class]]);

  TableViewDetailIconCell* detail_cell =
      base::apple::ObjCCastStrict<TableViewDetailIconCell>(cell);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];

  // Check that the default layout is set to the horizontal axis.
  EXPECT_EQ(UILayoutConstraintAxisHorizontal,
            detail_cell.textLayoutConstraintAxis);
  EXPECT_EQ([UIFont preferredFontForTextStyle:UIFontTextStyleBody],
            detail_cell.detailTextLabel.font);

  [detail_cell setTextLayoutConstraintAxis:UILayoutConstraintAxisVertical];

  EXPECT_EQ(UILayoutConstraintAxisVertical,
            detail_cell.textLayoutConstraintAxis);
  EXPECT_EQ([UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
            detail_cell.detailTextLabel.font);
}

// Tests that `detailTextNumberOfLines` and the detailText's
// UILabel.numberOfLines are set properly after a call to `configureCell`.
TEST_F(TableViewDetailIconItemTest, ItemDefaultDetailTextNumberOfLines) {
  TableViewDetailIconItem* item =
      [[TableViewDetailIconItem alloc] initWithType:0];
  item.text = @"Jane Doe";
  item.detailText = @"janedoe@gmail.com";
  item.textLayoutConstraintAxis = UILayoutConstraintAxisVertical;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewDetailIconCell class]]);

  TableViewDetailIconCell* detail_cell =
      base::apple::ObjCCastStrict<TableViewDetailIconCell>(cell);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];

  // Check that the default detailText's UILabel has one as the default number
  // of lines.
  EXPECT_EQ(1, detail_cell.detailTextNumberOfLines);
  EXPECT_EQ(1, detail_cell.detailTextLabel.numberOfLines);
}

// Tests that the detailText's UILabel.numberOfLines is set to the value of
// `detailTextNumberOfLines`. It also tests that `detailTextNumberOfLines` is
// ignored when the UI Layout is horizontal.
TEST_F(TableViewDetailIconItemTest, ItemWithDetailTextNumberOfLines) {
  TableViewDetailIconItem* item =
      [[TableViewDetailIconItem alloc] initWithType:0];
  item.text = @"Jane Doe";
  item.detailText = @"janedoe@gmail.com";
  item.textLayoutConstraintAxis = UILayoutConstraintAxisVertical;
  item.detailTextNumberOfLines = 0;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewDetailIconCell class]]);

  TableViewDetailIconCell* detail_cell =
      base::apple::ObjCCastStrict<TableViewDetailIconCell>(cell);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];

  // Check that if the layout is set to the vertical axis, and if we set
  // `detailTextNumberOfLines` to 0, then the detailText's UILabel.numberOfLines
  // is set to 0 as well.
  EXPECT_EQ(0, detail_cell.detailTextNumberOfLines);
  EXPECT_EQ(0, detail_cell.detailTextLabel.numberOfLines);

  [detail_cell setTextLayoutConstraintAxis:UILayoutConstraintAxisHorizontal];

  // Check that the if layout is set to the horizontal axis, then we ignore the
  // `detailTextNumberOfLines` property.
  EXPECT_EQ(0, detail_cell.detailTextNumberOfLines);
  EXPECT_EQ(1, detail_cell.detailTextLabel.numberOfLines);
}
