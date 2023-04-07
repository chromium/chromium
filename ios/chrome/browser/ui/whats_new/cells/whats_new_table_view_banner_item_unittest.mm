// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/cells/whats_new_table_view_banner_item.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/shared/ui/symbols/chrome_icon.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using WhatsNewTableViewBannerItemTest = PlatformTest;
}  // namespace

// Tests that the UILabels and banner image are set properly after a call to
// configureCell.
TEST_F(WhatsNewTableViewBannerItemTest, ItemProperties) {
  NSString* section = @"Featured";
  NSString* title = @"Title";
  NSString* detail_text = @"Detail text";

  WhatsNewTableViewBannerItem* item =
      [[WhatsNewTableViewBannerItem alloc] initWithType:0];
  item.sectionTitle = section;
  item.title = title;
  item.detailText = detail_text;
  item.bannerImage = [UIImage imageNamed:@"ic_search"];
  item.isBannerAtBottom = false;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[WhatsNewTableViewBannerCell class]]);

  WhatsNewTableViewBannerCell* banner_cell =
      base::mac::ObjCCastStrict<WhatsNewTableViewBannerCell>(cell);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];

  // Check section text label.
  EXPECT_NSEQ(section, banner_cell.sectionTextLabel.text);
  EXPECT_EQ(
      NO,
      banner_cell.sectionTextLabel.translatesAutoresizingMaskIntoConstraints);
  UIFont* sectionLabelFont =
      CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightSemibold);
  EXPECT_EQ(sectionLabelFont, banner_cell.sectionTextLabel.font);
  EXPECT_EQ(1, banner_cell.sectionTextLabel.numberOfLines);

  // Check text label (title).
  EXPECT_NSEQ(title, banner_cell.textLabel.text);
  EXPECT_EQ(NO,
            banner_cell.textLabel.translatesAutoresizingMaskIntoConstraints);
  UIFont* textLabelFont =
      CreateDynamicFont(UIFontTextStyleTitle1, UIFontWeightBold);
  EXPECT_EQ(textLabelFont, banner_cell.textLabel.font);
  EXPECT_EQ(2, banner_cell.textLabel.numberOfLines);

  // Check detail text label.
  EXPECT_NSEQ(detail_text, banner_cell.detailTextLabel.text);
  EXPECT_EQ(
      NO,
      banner_cell.detailTextLabel.translatesAutoresizingMaskIntoConstraints);
  UIFont* detailLabelFont =
      CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightRegular);
  EXPECT_EQ(detailLabelFont, banner_cell.detailTextLabel.font);
  EXPECT_EQ(5, banner_cell.detailTextLabel.numberOfLines);

  // Check that the banner image is at the top of the stack view.
  UIStackView* stack_view = base::mac::ObjCCastStrict<UIStackView>(
      banner_cell.contentView.subviews[0]);
  UIView* banner_view =
      base::mac::ObjCCastStrict<UIView>(stack_view.arrangedSubviews[0]);
  UIImageView* banner_image_view =
      base::mac::ObjCCastStrict<UIImageView>(banner_view.subviews[0]);
  EXPECT_NSEQ([ChromeIcon searchIcon], banner_image_view.image);
}

// Tests that the banner image is at the bottom of the stack view when
// isBannerAtBottom = false.
TEST_F(WhatsNewTableViewBannerItemTest, ItemPropertiesBannerAtBottom) {
  NSString* section = @"Featured";
  NSString* title = @"Title";
  NSString* detail_text = @"Detail text";

  WhatsNewTableViewBannerItem* item =
      [[WhatsNewTableViewBannerItem alloc] initWithType:0];
  item.sectionTitle = section;
  item.title = title;
  item.detailText = detail_text;
  item.bannerImage = [UIImage imageNamed:@"ic_search"];
  item.isBannerAtBottom = true;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[WhatsNewTableViewBannerCell class]]);

  WhatsNewTableViewBannerCell* banner_cell =
      base::mac::ObjCCastStrict<WhatsNewTableViewBannerCell>(cell);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];

  // Check section text label.
  EXPECT_NSEQ(section, banner_cell.sectionTextLabel.text);
  EXPECT_EQ(
      NO,
      banner_cell.sectionTextLabel.translatesAutoresizingMaskIntoConstraints);
  UIFont* sectionLabelFont =
      CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightSemibold);
  EXPECT_EQ(sectionLabelFont, banner_cell.sectionTextLabel.font);
  EXPECT_EQ(1, banner_cell.sectionTextLabel.numberOfLines);

  // Check text label (title).
  EXPECT_NSEQ(title, banner_cell.textLabel.text);
  EXPECT_EQ(NO,
            banner_cell.textLabel.translatesAutoresizingMaskIntoConstraints);
  UIFont* textLabelFont =
      CreateDynamicFont(UIFontTextStyleTitle1, UIFontWeightBold);
  EXPECT_EQ(textLabelFont, banner_cell.textLabel.font);
  EXPECT_EQ(2, banner_cell.textLabel.numberOfLines);

  // Check detail text label.
  EXPECT_NSEQ(detail_text, banner_cell.detailTextLabel.text);
  EXPECT_EQ(
      NO,
      banner_cell.detailTextLabel.translatesAutoresizingMaskIntoConstraints);
  UIFont* detailLabelFont =
      CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightRegular);
  EXPECT_EQ(detailLabelFont, banner_cell.detailTextLabel.font);
  EXPECT_EQ(5, banner_cell.detailTextLabel.numberOfLines);

  // Check that the banner image is at the top of the stack view.
  UIStackView* stack_view = base::mac::ObjCCastStrict<UIStackView>(
      banner_cell.contentView.subviews[0]);
  int stack_view_size = [stack_view.arrangedSubviews count];
  UIView* banner_view = base::mac::ObjCCastStrict<UIView>(
      stack_view.arrangedSubviews[stack_view_size - 1]);
  UIImageView* banner_image_view =
      base::mac::ObjCCastStrict<UIImageView>(banner_view.subviews[0]);
  EXPECT_NSEQ([ChromeIcon searchIcon], banner_image_view.image);
}
