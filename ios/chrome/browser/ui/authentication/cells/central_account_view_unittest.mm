// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/cells/central_account_view.h"

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/policy/model/management_state.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

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
                                managementState:ManagementState()
                                useLargeMargins:YES];

  EXPECT_NSEQ(accountView.avatarImage, image);
  EXPECT_NSEQ(accountView.name, mainText);
  EXPECT_NSEQ(accountView.email, detailText);
  EXPECT_EQ(accountView.managed, false);
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
                                managementState:ManagementState()
                                useLargeMargins:YES];

  EXPECT_NSEQ(accountView.avatarImage, image);
  EXPECT_NSEQ(accountView.name, mainText);
  EXPECT_NSEQ(accountView.email, nil);
  EXPECT_EQ(accountView.managed, false);
}

// Tests that the UIImageView and UILabels are set properly in the view if the
// machine policy domain is provided.
TEST_F(CentralAccountViewTest, ImageViewAndTextLabelsWithMachinePolicyDomain) {
  UIImage* image = ios::provider::GetSigninDefaultAvatar();
  image = ResizeImage(image,
                      GetSizeForIdentityAvatarSize(IdentityAvatarSize::Large),
                      ProjectionMode::kAspectFit);
  NSString* mainText = @"Main text";
  NSString* detailText = @"Detail text";
  ManagementState managementState;
  managementState.machine_level_domain = "somethingcorp.com";

  CentralAccountView* accountView =
      [[CentralAccountView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)
                                    avatarImage:image
                                           name:mainText
                                          email:detailText
                                managementState:std::move(managementState)
                                useLargeMargins:YES];

  EXPECT_NSEQ(accountView.avatarImage, image);
  EXPECT_NSEQ(accountView.name, mainText);
  EXPECT_NSEQ(accountView.email, detailText);
  EXPECT_EQ(accountView.managed, false);
}

// Tests that the UIImageView and UILabels are set properly in the view if the
// user policy domain is provided.
TEST_F(CentralAccountViewTest, ImageViewAndTextLabelsWithUserPolicyDomain) {
  UIImage* image = ios::provider::GetSigninDefaultAvatar();
  image = ResizeImage(image,
                      GetSizeForIdentityAvatarSize(IdentityAvatarSize::Large),
                      ProjectionMode::kAspectFit);
  NSString* mainText = @"Main text";
  NSString* detailText = @"Detail text";
  ManagementState managementState;
  managementState.user_level_domain = "acme.com";

  CentralAccountView* accountView =
      [[CentralAccountView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)
                                    avatarImage:image
                                           name:mainText
                                          email:detailText
                                managementState:std::move(managementState)
                                useLargeMargins:YES];

  EXPECT_NSEQ(accountView.avatarImage, image);
  EXPECT_NSEQ(accountView.name, mainText);
  EXPECT_NSEQ(accountView.email, detailText);
  EXPECT_EQ(accountView.managed, true);
}
