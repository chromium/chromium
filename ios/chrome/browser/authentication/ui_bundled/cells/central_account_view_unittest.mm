// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/cells/central_account_view.h"

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/policy/model/management_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

using CentralAccountViewTest = PlatformTest;

// Tests that the UIImageView and UILabels are set properly in the view.
TEST_F(CentralAccountViewTest, ImageViewAndTextLabels) {
  UIImage* image = ios::provider::GetSigninDefaultAvatar();
  image = ResizeImage(image,
                      GetSizeForIdentityAvatarSize(IdentityAvatarSize::Large),
                      ProjectionMode::kAspectFit);
  NSString* mainText = @"Main text";
  NSString* detailText = @"Detail text";

  CentralAccountView* accountView =
      [[CentralAccountView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)
                                    avatarImage:image
                                           name:mainText
                                          email:detailText
                          managementDescription:nil
                                useLargeMargins:YES];

  EXPECT_NSEQ(accountView.avatarImage, image);
  EXPECT_NSEQ(accountView.name, mainText);
  EXPECT_NSEQ(accountView.email, detailText);
  EXPECT_EQ(accountView.managed, false);
  EXPECT_NSEQ([accountView managementDescription], nil);
}

// Tests that the UIImageView and UILabels are set properly in the view if the
// account name is not provided.
TEST_F(CentralAccountViewTest, ImageViewAndTextLabelsWithoutGivenName) {
  UIImage* image = ios::provider::GetSigninDefaultAvatar();
  image = ResizeImage(image,
                      GetSizeForIdentityAvatarSize(IdentityAvatarSize::Large),
                      ProjectionMode::kAspectFit);
  NSString* mainText = @"Main text";

  CentralAccountView* accountView =
      [[CentralAccountView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)
                                    avatarImage:image
                                           name:nil
                                          email:mainText
                          managementDescription:nil
                                useLargeMargins:YES];

  EXPECT_NSEQ(accountView.avatarImage, image);
  EXPECT_NSEQ(accountView.name, mainText);
  EXPECT_NSEQ(accountView.email, nil);
  EXPECT_EQ(accountView.managed, false);
  EXPECT_NSEQ([accountView managementDescription], nil);
}

// Tests that the UIImageView and UILabels are set properly in the view if the
// machine policy domain is provided.
TEST_F(CentralAccountViewTest,
       ImageViewAndTextLabelsWithManagementDescription) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kSeparateProfilesForManagedAccounts);

  UIImage* image = ios::provider::GetSigninDefaultAvatar();
  image = ResizeImage(image,
                      GetSizeForIdentityAvatarSize(IdentityAvatarSize::Large),
                      ProjectionMode::kAspectFit);
  NSString* mainText = @"Main text";
  NSString* detailText = @"Detail text";
  NSString* managementDescription = @"A management label";

  CentralAccountView* accountView =
      [[CentralAccountView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)
                                    avatarImage:image
                                           name:mainText
                                          email:detailText
                          managementDescription:managementDescription
                                useLargeMargins:YES];

  EXPECT_NSEQ(accountView.avatarImage, image);
  EXPECT_NSEQ(accountView.name, mainText);
  EXPECT_NSEQ(accountView.email, detailText);
  EXPECT_EQ(accountView.managed, true);
  EXPECT_NSEQ([accountView managementDescription], managementDescription);
}
