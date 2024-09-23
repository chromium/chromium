// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/inline_promo_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/settings/cells/inline_promo_cell.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using InlinePromoItemTest = PlatformTest;

// Returns the image of the cell's close button.
UIImage* GetCloseButtonImage() {
  UIImageSymbolConfiguration* symbolConfiguration = [UIImageSymbolConfiguration
      configurationWithPointSize:16
                          weight:UIImageSymbolWeightSemibold
                           scale:UIImageSymbolScaleMedium];
  return DefaultSymbolWithConfiguration(@"xmark", symbolConfiguration);
}

// Returns the font defined in the configuration of the cell's more info button.
UIFont* GetMoreInfoButtonTitleFont(
    UIButtonConfiguration* more_info_button_configuration) {
  return [more_info_button_configuration.attributedTitle
           attribute:NSFontAttributeName
             atIndex:0
      effectiveRange:nullptr];
}

// Tests that the cell is as expected after a call to `configureCell:`.
TEST_F(InlinePromoItemTest, ConfigureCell) {
  InlinePromoItem* item = [[InlinePromoItem alloc] initWithType:0];

  // Set up item.
  UIImage* promo_image = DefaultSymbolWithPointSize(@"tortoise.fill", 16);
  NSString* promo_text = @"Test text";
  NSString* more_info_button_title = @"Button Title";
  item.promoImage = promo_image;
  item.promoText = promo_text;
  item.moreInfoButtonTitle = more_info_button_title;

  // Set up cell.
  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[InlinePromoCell class]]);
  EXPECT_TRUE(item.shouldShowCloseButton);

  InlinePromoCell* promo_cell =
      base::apple::ObjCCastStrict<InlinePromoCell>(cell);

  // Configure cell.
  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];

  // Verify cell configuration.
  UIButton* close_button = promo_cell.closeButton;
  UIImageView* cell_promo_image_view = promo_cell.promoImageView;
  UILabel* cell_promo_text_label = promo_cell.promoTextLabel;
  UIButton* more_info_button = promo_cell.moreInfoButton;
  EXPECT_TRUE(close_button.enabled);
  EXPECT_NSEQ(cell_promo_image_view.image, promo_image);
  EXPECT_NSEQ(cell_promo_text_label.text, promo_text);
  EXPECT_EQ(cell_promo_text_label.font,
            [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]);
  EXPECT_TRUE([cell_promo_text_label.textColor
      isEqual:[UIColor colorNamed:kTextPrimaryColor]]);
  EXPECT_TRUE(cell_promo_text_label.isAccessibilityElement);
  EXPECT_TRUE(more_info_button.enabled);

  UIButtonConfiguration* close_button_configuration =
      close_button.configuration;
  UIButtonConfiguration* more_info_button_configuration =
      more_info_button.configuration;
  EXPECT_NSEQ(close_button_configuration.image, GetCloseButtonImage());
  EXPECT_TRUE([close_button_configuration.baseForegroundColor
      isEqual:[UIColor colorNamed:kTextTertiaryColor]]);
  EXPECT_TRUE([more_info_button_configuration.baseForegroundColor
      isEqual:[UIColor colorNamed:kBlue600Color]]);
  EXPECT_NSEQ(more_info_button_configuration.attributedTitle.string,
              more_info_button_title);
  EXPECT_NSEQ(GetMoreInfoButtonTitleFont(more_info_button_configuration),
              [UIFont preferredFontForTextStyle:UIFontTextStyleBody]);
}

// Tests that the close button visibility follows the item's
// `shouldShowCloseButton` property.
TEST_F(InlinePromoItemTest, CloseButtonVisibility) {
  InlinePromoItem* item = [[InlinePromoItem alloc] initWithType:0];
  item.promoImage = DefaultSymbolWithPointSize(@"tortoise.fill", 16);
  item.promoText = @"Test text";
  item.moreInfoButtonTitle = @"Button Title";

  id cell = [[[item cellClass] alloc] init];
  InlinePromoCell* promo_cell =
      base::apple::ObjCCastStrict<InlinePromoCell>(cell);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_FALSE(promo_cell.closeButton.hidden);

  item.shouldShowCloseButton = false;
  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_TRUE(promo_cell.closeButton.hidden);
}

// Tests that the cell is as expected when disabled.
TEST_F(InlinePromoItemTest, DisableCell) {
  InlinePromoItem* item = [[InlinePromoItem alloc] initWithType:0];
  item.promoImage = DefaultSymbolWithPointSize(@"tortoise.fill", 16);
  item.promoText = @"Test text";
  item.moreInfoButtonTitle = @"Button Title";
  item.enabled = NO;

  id cell = [[[item cellClass] alloc] init];
  InlinePromoCell* promo_cell =
      base::apple::ObjCCastStrict<InlinePromoCell>(cell);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];

  EXPECT_FALSE(promo_cell.closeButton.enabled);
  EXPECT_TRUE([promo_cell.promoTextLabel.textColor
      isEqual:[UIColor colorNamed:kTextSecondaryColor]]);
  EXPECT_FALSE(promo_cell.moreInfoButton.enabled);
}

}  // namespace
