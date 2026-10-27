// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_account_item.h"

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/signin/ui/avatar/ai_tier_avatar_content_configuration.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

using AccountControlTableViewItemTest = PlatformTest;

// Test that the avatar configuration and text labels are set properly after a
// call to `configureCell:`.
TEST_F(AccountControlTableViewItemTest, ImageViewAndTextLabels) {
  TableViewAccountItem* item = [[TableViewAccountItem alloc] initWithType:0];
  UIImage* image = [[UIImage alloc] init];
  NSString* name = @"Name";
  NSString* email = @"email";

  item.image = image;
  item.name = name;
  item.email = email;

  LegacyTableViewCell* cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[LegacyTableViewCell class]]);

  [item configureCell:cell];

  ASSERT_TRUE([cell.contentConfiguration
      isMemberOfClass:TableViewCellContentConfiguration.class]);
  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          cell.contentConfiguration);

  EXPECT_NSEQ(name, configuration.title);
  EXPECT_NSEQ(email, configuration.subtitle);
  EXPECT_NSEQ(@"Name, email", cell.accessibilityLabel);

  NSObject<ChromeContentConfiguration>* leading_config =
      configuration.leadingConfiguration;
  ASSERT_TRUE([leading_config
      isMemberOfClass:[AITierAvatarContentConfiguration class]]);

  AITierAvatarContentConfiguration* avatar_config =
      base::apple::ObjCCastStrict<AITierAvatarContentConfiguration>(
          leading_config);
  EXPECT_NSEQ(image, avatar_config.avatarImage);
  EXPECT_EQ(kTableViewIconImageSize, avatar_config.diameter);
  EXPECT_EQ(AITierAvatarSizeType::kFullView, avatar_config.sizeType);
  EXPECT_FALSE(avatar_config.showsAITierRing);
  EXPECT_TRUE(CGSizeEqualToSize(
      CGSizeMake(kTableViewIconImageSize, kTableViewIconImageSize),
      [avatar_config contentSize]));
}

// Test that `TableViewAccountDetailImage::kAITierRing` is forwarded to the
// leading avatar configuration, preserves the avatar center, and appends
// "Google membership" to the accessibility label after a call to
// `configureCell:`.
TEST_F(AccountControlTableViewItemTest, ShowsAITierRing) {
  TableViewAccountItem* item = [[TableViewAccountItem alloc] initWithType:0];
  UIImage* image = [[UIImage alloc] init];
  NSString* name = @"Name";
  NSString* email = @"email";

  item.image = image;
  item.name = name;
  item.email = email;
  item.detailImage = TableViewAccountDetailImage::kAITierRing;

  LegacyTableViewCell* cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[LegacyTableViewCell class]]);

  [item configureCell:cell];

  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          cell.contentConfiguration);
  AITierAvatarContentConfiguration* avatar_config =
      base::apple::ObjCCastStrict<AITierAvatarContentConfiguration>(
          configuration.leadingConfiguration);
  EXPECT_NSEQ(image, avatar_config.avatarImage);
  EXPECT_EQ(kTableViewIconImageSize, avatar_config.diameter);
  EXPECT_EQ(AITierAvatarSizeType::kFullView, avatar_config.sizeType);
  EXPECT_TRUE(avatar_config.showsAITierRing);
  EXPECT_TRUE(CGSizeEqualToSize(
      CGSizeMake(kTableViewIconImageSize, kTableViewIconImageSize),
      [avatar_config contentSize]));
  EXPECT_NSEQ(
      l10n_util::GetNSStringF(
          IDS_IOS_ACCOUNT_TABLE_VIEW_ITEM_WITH_NAME_AND_EMAIL_GOOGLE_MEMBERSHIP,
          base::SysNSStringToUTF16(name), base::SysNSStringToUTF16(email)),
      cell.accessibilityLabel);

  // Verify accessibility label when only `email` is present.
  item.name = nil;
  [item configureCell:cell];
  EXPECT_NSEQ(l10n_util::GetNSStringF(
                  IDS_IOS_ACCOUNT_TABLE_VIEW_ITEM_WITH_EMAIL_GOOGLE_MEMBERSHIP,
                  base::SysNSStringToUTF16(email)),
              cell.accessibilityLabel);
}
