// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/cells/table_view_central_account_item.h"

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/signin/constants.h"
#import "ios/chrome/browser/signin/signin_util.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using CentralAccountControlTableViewItemTest = PlatformTest;

// Tests that the UIImageView and UILabels are set properly after a call to
// `configureCell:`.
TEST_F(CentralAccountControlTableViewItemTest, ImageViewAndTextLabels) {
  TableViewCentralAccountItem* item =
      [[TableViewCentralAccountItem alloc] initWithType:0];
  UIImage* image = ios::provider::GetSigninDefaultAvatar();
  image = ResizeImage(
      image, GetSizeForIdentityAvatarSize(IdentityAvatarSize::ExtraLarge),
      ProjectionMode::kAspectFit);
  NSString* mainText = @"Main text";
  NSString* detailText = @"Detail text";

  item.avatarImage = image;
  item.name = mainText;
  item.email = detailText;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewCentralAccountCell class]]);

  TableViewCentralAccountCell* accountCell = cell;
  EXPECT_FALSE(accountCell.avatarImageView.image);
  EXPECT_FALSE(accountCell.nameLabel.text);
  EXPECT_FALSE(accountCell.emailLabel.text);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSEQ(image, accountCell.avatarImageView.image);
  EXPECT_NSEQ(mainText, accountCell.nameLabel.text);
  EXPECT_NSEQ(detailText, accountCell.emailLabel.text);
}

// Tests that the UIImageView and UILabels are set properly after a call to
// `configureCell:` if the name is not provided.
TEST_F(CentralAccountControlTableViewItemTest,
       ImageViewAndTextLabelsWithoutGivenName) {
  TableViewCentralAccountItem* item =
      [[TableViewCentralAccountItem alloc] initWithType:0];
  UIImage* image = ios::provider::GetSigninDefaultAvatar();
  image = ResizeImage(
      image, GetSizeForIdentityAvatarSize(IdentityAvatarSize::ExtraLarge),
      ProjectionMode::kAspectFit);
  NSString* mainlText = @"Detail text";

  item.avatarImage = image;
  item.name = nil;
  item.email = mainlText;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewCentralAccountCell class]]);

  TableViewCentralAccountCell* accountCell = cell;
  EXPECT_FALSE(accountCell.avatarImageView.image);
  EXPECT_FALSE(accountCell.nameLabel.text);
  EXPECT_FALSE(accountCell.emailLabel.text);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSEQ(image, accountCell.avatarImageView.image);
  EXPECT_NSEQ(mainlText, accountCell.nameLabel.text);
  EXPECT_NSEQ(nil, accountCell.emailLabel.text);
}
