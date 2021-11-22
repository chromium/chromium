// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"

#import <MaterialComponents/MaterialOverlayWindow.h>

#import "ios/chrome/browser/signin/signin_util.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using SigninPromoViewTest = PlatformTest;

TEST_F(SigninPromoViewTest, ChromiumLogoImage) {
  UIWindow* currentWindow = GetAnyKeyWindow();
  SigninPromoView* view =
      [[SigninPromoView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  view.mode = SigninPromoViewModeNoAccounts;
  [currentWindow.rootViewController.view addSubview:view];
  UIImage* chromiumLogo = view.imageView.image;
  EXPECT_NE(nil, chromiumLogo);
  view.mode = SigninPromoViewModeSigninWithAccount;
  UIImage* customImage = ios::provider::GetSigninDefaultAvatar();
  CGSize size = GetSizeForIdentityAvatarSize(IdentityAvatarSize::SmallSize);
  customImage = ResizeImage(customImage, size, ProjectionMode::kAspectFit);
  [view setProfileImage:customImage];
  EXPECT_NE(nil, view.imageView.image);
  // The image should has been changed from the logo.
  EXPECT_NE(chromiumLogo, view.imageView.image);
  // The image should be different than the one set, since a circular background
  // should have been added.
  EXPECT_NE(customImage, view.imageView.image);
  view.mode = SigninPromoViewModeSyncWithPrimaryAccount;
  EXPECT_NE(nil, view.imageView.image);
  // The image should has been changed from the logo.
  EXPECT_NE(chromiumLogo, view.imageView.image);
  // The image should be different than the one set, since a circular background
  // should have been added.
  EXPECT_NE(customImage, view.imageView.image);
}

TEST_F(SigninPromoViewTest, SecondaryButtonVisibility) {
  UIWindow* currentWindow = GetAnyKeyWindow();
  SigninPromoView* view =
      [[SigninPromoView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  view.mode = SigninPromoViewModeNoAccounts;
  [currentWindow.rootViewController.view addSubview:view];
  EXPECT_TRUE(view.secondaryButton.hidden);
  view.mode = SigninPromoViewModeSigninWithAccount;
  EXPECT_FALSE(view.secondaryButton.hidden);
  view.mode = SigninPromoViewModeSyncWithPrimaryAccount;
  EXPECT_TRUE(view.secondaryButton.hidden);
}
