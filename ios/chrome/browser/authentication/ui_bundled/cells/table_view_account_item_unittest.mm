// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_account_item.h"

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/signin/ui/avatar/ai_tier_avatar_content_configuration.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

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
// leading avatar configuration after a call to `configureCell:`.
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
}
