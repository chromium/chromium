// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
constexpr CGFloat kTestIconPointSize = 18;
using TableViewDetailIconItemTest = PlatformTest;
}

// Tests that the UILabels and icons are set properly after a call to
// `configureCell:`.
TEST_F(TableViewDetailIconItemTest, ItemProperties) {
  NSString* text = @"Cell text";
  NSString* detail_text = @"Cell detail text";

  TableViewDetailIconItem* item =
      [[TableViewDetailIconItem alloc] initWithType:0];
  item.text = text;
  item.detailText = detail_text;
  item.iconImage =
      SymbolWithPointSize(SymbolMagnifyingglass, kTestIconPointSize);
  item.iconTintColor = UIColor.whiteColor;
  item.iconBackgroundColor = UIColor.blackColor;
  item.textLayoutConstraintAxis = UILayoutConstraintAxisVertical;
  EXPECT_TRUE(item.badgeType == BadgeType::kNone);

  LegacyTableViewCell* cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[LegacyTableViewCell class]]);
  [item configureCell:cell];

  ASSERT_TRUE([cell.contentConfiguration
      isMemberOfClass:TableViewCellContentConfiguration.class]);
  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          cell.contentConfiguration);

  EXPECT_NSEQ(text, configuration.title);
  EXPECT_NSEQ(detail_text, configuration.subtitle);

  NSObject<ChromeContentConfiguration>* leading_config =
      configuration.leadingConfiguration;
  ASSERT_TRUE([leading_config
      isMemberOfClass:[ColorfulSymbolContentConfiguration class]]);

  ColorfulSymbolContentConfiguration* symbol_config =
      base::apple::ObjCCastStrict<ColorfulSymbolContentConfiguration>(
          leading_config);

  // Check image-based property.
  EXPECT_NSEQ(SymbolWithPointSize(SymbolMagnifyingglass, kTestIconPointSize),
              symbol_config.symbolImage);
  EXPECT_EQ(UIColor.whiteColor, symbol_config.symbolTintColor);
  EXPECT_EQ(UIColor.blackColor, symbol_config.symbolBackgroundColor);
}

// Tests that number of lines and text are set properly after a call to
// `configureCell`.
TEST_F(TableViewDetailIconItemTest, ItemDefaultDetailTextNumberOfLines) {
  TableViewDetailIconItem* item =
      [[TableViewDetailIconItem alloc] initWithType:0];
  item.text = @"Jane Doe";
  item.detailText = @"janedoe@gmail.com";
  item.textLayoutConstraintAxis = UILayoutConstraintAxisVertical;

  LegacyTableViewCell* cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[LegacyTableViewCell class]]);
  [item configureCell:cell];

  ASSERT_TRUE([cell.contentConfiguration
      isMemberOfClass:TableViewCellContentConfiguration.class]);
  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          cell.contentConfiguration);

  // Check that the default number of lines for title is two and subtitle is
  // one.
  EXPECT_EQ(2, configuration.titleNumberOfLines);
  EXPECT_EQ(1, configuration.subtitleNumberOfLines);
}

// Tests that number of lines and text are set properly after a call to
// `configureCell`. Check that changing the layout to horizontal works as
// expected.
TEST_F(TableViewDetailIconItemTest, ItemWithDetailTextNumberOfLines) {
  TableViewDetailIconItem* item =
      [[TableViewDetailIconItem alloc] initWithType:0];
  NSString* text = @"Jane Doe";
  NSString* detail_text = @"janedoe@gmail.com";
  item.text = text;
  item.detailText = detail_text;
  item.textLayoutConstraintAxis = UILayoutConstraintAxisVertical;
  item.detailTextNumberOfLines = 0;

  LegacyTableViewCell* cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[LegacyTableViewCell class]]);
  [item configureCell:cell];

  ASSERT_TRUE([cell.contentConfiguration
      isMemberOfClass:TableViewCellContentConfiguration.class]);
  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          cell.contentConfiguration);

  EXPECT_NSEQ(text, configuration.title);
  EXPECT_EQ(2, configuration.titleNumberOfLines);
  EXPECT_NSEQ(detail_text, configuration.subtitle);
  EXPECT_EQ(0, configuration.subtitleNumberOfLines);
  EXPECT_NSEQ(nil, configuration.trailingText);

  item.textLayoutConstraintAxis = UILayoutConstraintAxisHorizontal;
  [item configureCell:cell];

  configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          cell.contentConfiguration);

  // Check that the if layout is set to the horizontal axis, then we ignore the
  // `detailTextNumberOfLines` property.
  EXPECT_NSEQ(text, configuration.title);
  EXPECT_EQ(2, configuration.titleNumberOfLines);
  EXPECT_NSEQ(nil, configuration.subtitle);
  EXPECT_EQ(0, configuration.subtitleNumberOfLines);
  EXPECT_NSEQ(detail_text, configuration.trailingText);
  EXPECT_EQ(1, configuration.trailingTextNumberOfLines);
}

// Tests that verticalAlignment is passed to contentConfiguration.
TEST_F(TableViewDetailIconItemTest, TestVerticalAlignment) {
  TableViewDetailIconItem* item =
      [[TableViewDetailIconItem alloc] initWithType:0];
  item.text = @"Text";
  item.verticalAlignment = UIStackViewAlignmentTop;

  LegacyTableViewCell* cell = [[[item cellClass] alloc] init];
  [item configureCell:cell];

  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          cell.contentConfiguration);
  EXPECT_EQ(UIStackViewAlignmentTop, configuration.verticalAlignment);
}

// Tests that reservesLeadingSpace is passed to contentConfiguration.
TEST_F(TableViewDetailIconItemTest, TestReservesLeadingSpace) {
  TableViewDetailIconItem* item =
      [[TableViewDetailIconItem alloc] initWithType:0];
  item.text = @"Text";
  item.reservesLeadingSpace = YES;

  LegacyTableViewCell* cell = [[[item cellClass] alloc] init];
  [item configureCell:cell];

  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          cell.contentConfiguration);
  EXPECT_TRUE(configuration.reservesLeadingSpace);
}

// Tests that notification dot badge is set properly inline after a call to
// `configureCell:`.
TEST_F(TableViewDetailIconItemTest, ItemWithNotificationDotBadge) {
  TableViewDetailIconItem* item =
      [[TableViewDetailIconItem alloc] initWithType:0];
  item.text = @"New Feature";
  item.badgeType = BadgeType::kNotificationDot;

  LegacyTableViewCell* cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[LegacyTableViewCell class]]);
  [item configureCell:cell];

  ASSERT_TRUE([cell.contentConfiguration
      isMemberOfClass:TableViewCellContentConfiguration.class]);
  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          cell.contentConfiguration);

  EXPECT_NE(nil, configuration.attributedTitle);
  EXPECT_EQ(nil, configuration.trailingConfiguration);

  NSString* notificationHint =
      l10n_util::GetNSString(IDS_IOS_NEW_ITEM_ACCESSIBILITY_HINT);
  NSString* expectedAccessibilityLabel =
      [NSString stringWithFormat:@"%@, %@", configuration.accessibilityLabel,
                                 notificationHint];
  EXPECT_NSEQ(expectedAccessibilityLabel, cell.accessibilityLabel);
}

// Tests that warning badge is automatically set trailing after a call to
// `configureCell:`.
TEST_F(TableViewDetailIconItemTest, ItemWithWarningBadge) {
  TableViewDetailIconItem* item =
      [[TableViewDetailIconItem alloc] initWithType:0];
  item.text = @"Universal Opt-Out";
  item.badgeType = BadgeType::kWarning;

  LegacyTableViewCell* cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[LegacyTableViewCell class]]);
  [item configureCell:cell];

  ASSERT_TRUE([cell.contentConfiguration
      isMemberOfClass:TableViewCellContentConfiguration.class]);
  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          cell.contentConfiguration);

  EXPECT_EQ(nil, configuration.attributedTitle);
  EXPECT_NSEQ(item.text, configuration.title);
  ASSERT_TRUE([configuration.trailingConfiguration
      isMemberOfClass:ImageContentConfiguration.class]);
  ImageContentConfiguration* trailingImage =
      base::apple::ObjCCastStrict<ImageContentConfiguration>(
          configuration.trailingConfiguration);
  EXPECT_NE(nil, trailingImage.image);
  EXPECT_EQ(kTableViewIconImageSize, trailingImage.imageSize.width);
  EXPECT_EQ(kTableViewIconImageSize, trailingImage.imageSize.height);

  NSString* warningHint =
      l10n_util::GetNSString(IDS_IOS_ITEM_WARNING_BADGE_ACCESSIBILITY_HINT);
  NSString* expectedAccessibilityLabel =
      [NSString stringWithFormat:@"%@, %@", item.text, warningHint];
  EXPECT_NSEQ(expectedAccessibilityLabel, cell.accessibilityLabel);
}
