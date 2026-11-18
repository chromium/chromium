// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/ui/manual_fill_summary_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/favicon_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Registers and dequeues a cell backed by `TableViewCell` for summary rows.
UITableViewCell* DequeueSummaryCell() {
  UITableView* table_view = [[UITableView alloc] init];
  [TableViewCellContentConfiguration registerCellForTableView:table_view];
  return [TableViewCellContentConfiguration dequeueTableViewCell:table_view];
}

}  // namespace

using ManualFillSummaryItemTest = PlatformTest;

// Tests that configuring a cell sets the title, subtitle, disclosure chevron,
// background color, accessibility properties, and
// `ColorfulSymbolContentConfiguration` for `ManualFillIconStyle::kSymbol`.
TEST_F(ManualFillSummaryItemTest, ConfiguresSymbolSummaryCell) {
  UIImage* symbol = [[UIImage alloc] init];
  ManualFillSummaryItem* item = [[ManualFillSummaryItem alloc]
      initWithIdentifier:@"address_1"
                   title:@"123 Main St"
                subtitle:@"Mountain View, CA"
               iconStyle:ManualFillIconStyle::kSymbol
               iconImage:symbol];
  item.cellIndexAccessibilityLabel = @"1 of 2";

  UITableViewCell* cell = DequeueSummaryCell();
  ASSERT_NE(cell, nil);
  [item configureCell:cell];

  EXPECT_NSEQ(item.identifier, @"address_1");
  EXPECT_NSEQ(cell.backgroundColor,
              [UIColor colorNamed:kGroupedSecondaryBackgroundColor]);
  EXPECT_EQ(cell.accessoryType, UITableViewCellAccessoryDisclosureIndicator);
  EXPECT_EQ(cell.accessibilityTraits, UIAccessibilityTraitButton);
  EXPECT_NSEQ(cell.accessibilityIdentifier,
              manual_fill::kExpandedManualFillSummaryCellID);
  EXPECT_NSEQ(cell.accessibilityLabel,
              @"1 of 2, 123 Main St, Mountain View, CA");

  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCast<TableViewCellContentConfiguration>(
          cell.contentConfiguration);
  ASSERT_NE(configuration, nil);
  EXPECT_NSEQ(configuration.title, @"123 Main St");
  EXPECT_EQ(configuration.titleNumberOfLines, 1);
  EXPECT_EQ(configuration.titleLineBreakMode, NSLineBreakByTruncatingTail);
  EXPECT_NSEQ(configuration.subtitle, @"Mountain View, CA");
  EXPECT_EQ(configuration.subtitleNumberOfLines, 1);
  EXPECT_EQ(configuration.subtitleLineBreakMode, NSLineBreakByTruncatingTail);

  ColorfulSymbolContentConfiguration* leading =
      base::apple::ObjCCast<ColorfulSymbolContentConfiguration>(
          configuration.leadingConfiguration);
  ASSERT_NE(leading, nil);
  EXPECT_EQ(leading.symbolImage, symbol);
  EXPECT_NSEQ(leading.symbolTintColor, [UIColor colorNamed:kTextPrimaryColor]);
  EXPECT_NSEQ(leading.symbolBackgroundColor,
              [UIColor colorNamed:kGroupedPrimaryBackgroundColor]);
}

// Tests that `ManualFillIconStyle::kCardIcon` configures an
// `ImageContentConfiguration` and normalizes both empty and `nil` subtitles to
// `nil`.
TEST_F(ManualFillSummaryItemTest, ConfiguresCardIconSummaryCell) {
  UIImage* card_icon = [[UIImage alloc] init];
  ManualFillSummaryItem* empty_subtitle_item = [[ManualFillSummaryItem alloc]
      initWithIdentifier:@"card_1"
                   title:@"Visa •••• 4242"
                subtitle:@""
               iconStyle:ManualFillIconStyle::kCardIcon
               iconImage:card_icon];

  UITableViewCell* cell = DequeueSummaryCell();
  ASSERT_NE(cell, nil);
  [empty_subtitle_item configureCell:cell];

  EXPECT_EQ(empty_subtitle_item.subtitle, nil);
  EXPECT_NSEQ(cell.accessibilityLabel, @"Visa •••• 4242");

  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCast<TableViewCellContentConfiguration>(
          cell.contentConfiguration);
  ASSERT_NE(configuration, nil);
  EXPECT_EQ(configuration.subtitle, nil);

  ImageContentConfiguration* leading =
      base::apple::ObjCCast<ImageContentConfiguration>(
          configuration.leadingConfiguration);
  ASSERT_NE(leading, nil);
  EXPECT_EQ(leading.image, card_icon);
  EXPECT_EQ(leading.imageSize.width, 40);
  EXPECT_EQ(leading.imageSize.height, 24);
  EXPECT_EQ(leading.imageContentMode, UIViewContentModeScaleAspectFit);

  ManualFillSummaryItem* nil_subtitle_item = [[ManualFillSummaryItem alloc]
      initWithIdentifier:@"card_2"
                   title:@"Mastercard •••• 1111"
                subtitle:nil
               iconStyle:ManualFillIconStyle::kCardIcon
               iconImage:card_icon];
  [nil_subtitle_item configureCell:cell];

  TableViewCellContentConfiguration* nil_subtitle_config =
      base::apple::ObjCCast<TableViewCellContentConfiguration>(
          cell.contentConfiguration);
  ASSERT_NE(nil_subtitle_config, nil);
  EXPECT_EQ(nil_subtitle_config.subtitle, nil);
  EXPECT_NSEQ(cell.accessibilityLabel, @"Mastercard •••• 1111");
}

// Tests that `ManualFillIconStyle::kFavicon` uses `FaviconContentConfiguration`
// by default and falls back to a centered `ImageContentConfiguration` sized to
// `kTableViewIconImageSize` when a symbol override (`iconImage`) is provided.
TEST_F(ManualFillSummaryItemTest, ConfiguresFaviconSummaryCell) {
  FaviconAttributes* attributes =
      [FaviconAttributes attributesWithImage:[[UIImage alloc] init]];
  ManualFillSummaryItem* favicon_item = [[ManualFillSummaryItem alloc]
      initWithIdentifier:@"cred_1"
                   title:@"user@example.com"
                subtitle:@"Password"
               iconStyle:ManualFillIconStyle::kFavicon
               iconImage:nil];
  favicon_item.faviconAttributes = attributes;

  UITableViewCell* cell = DequeueSummaryCell();
  ASSERT_NE(cell, nil);
  [favicon_item configureCell:cell];

  TableViewCellContentConfiguration* favicon_config =
      base::apple::ObjCCast<TableViewCellContentConfiguration>(
          cell.contentConfiguration);
  ASSERT_NE(favicon_config, nil);
  FaviconContentConfiguration* favicon_leading =
      base::apple::ObjCCast<FaviconContentConfiguration>(
          favicon_config.leadingConfiguration);
  ASSERT_NE(favicon_leading, nil);
  EXPECT_EQ(favicon_leading.faviconAttributes, attributes);

  UIImage* symbol_override = [[UIImage alloc] init];
  ManualFillSummaryItem* override_item = [[ManualFillSummaryItem alloc]
      initWithIdentifier:@"passkey_1"
                   title:@"user@example.com"
                subtitle:@"Passkey"
               iconStyle:ManualFillIconStyle::kFavicon
               iconImage:symbol_override];
  [override_item configureCell:cell];

  TableViewCellContentConfiguration* override_config =
      base::apple::ObjCCast<TableViewCellContentConfiguration>(
          cell.contentConfiguration);
  ASSERT_NE(override_config, nil);
  ImageContentConfiguration* override_leading =
      base::apple::ObjCCast<ImageContentConfiguration>(
          override_config.leadingConfiguration);
  ASSERT_NE(override_leading, nil);
  EXPECT_EQ(override_leading.image, symbol_override);
  EXPECT_EQ(override_leading.imageSize.width, kTableViewIconImageSize);
  EXPECT_EQ(override_leading.imageSize.height, kTableViewIconImageSize);
  EXPECT_EQ(override_leading.imageContentMode, UIViewContentModeCenter);
  EXPECT_NSEQ(override_leading.imageTintColor,
              [UIColor colorNamed:kTextPrimaryColor]);
}
