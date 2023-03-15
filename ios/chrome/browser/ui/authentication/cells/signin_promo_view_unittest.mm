// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"

#import <MaterialComponents/MaterialOverlayWindow.h>

#import "ios/chrome/browser/signin/signin_util.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/gtest_support.h"

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

// Tests the accessibility label (based on the `textLabel` and the primary
// button title).
TEST_F(SigninPromoViewTest, AccessibilityLabel) {
  UIWindow* currentWindow = GetAnyKeyWindow();
  SigninPromoView* view =
      [[SigninPromoView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  [currentWindow.rootViewController.view addSubview:view];
  NSString* primaryButtonTitle = @"Primary Button Title";
  [view.primaryButton setTitle:primaryButtonTitle
                      forState:UIControlStateNormal];
  NSString* promoText = @"This is the promo text.";
  view.textLabel.text = promoText;
  NSString* expectedAccessibilityLabel =
      [NSString stringWithFormat:@"%@ %@", promoText, primaryButtonTitle];
  EXPECT_TRUE(
      [view.accessibilityLabel isEqualToString:expectedAccessibilityLabel]);
}

// Tests that signin is created on non-compact layout and that setting compact
// layout changes the primary button styling.
TEST_F(SigninPromoViewTest, ChangeLayout) {
  UIWindow* currentWindow = GetAnyKeyWindow();
  SigninPromoView* view =
      [[SigninPromoView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  view.mode = SigninPromoViewModeNoAccounts;
  [currentWindow.rootViewController.view addSubview:view];
  // The default mode should be standard.
  EXPECT_EQ(view.promoViewStyle, SigninPromoViewStyleStandard);
  // In full layout, the primary button is rounded with background color.
  EXPECT_TRUE(view.primaryButton.backgroundColor);
  EXPECT_GT(view.primaryButton.layer.cornerRadius, 0.0);

  // Switch to compact layout.
  view.promoViewStyle = SigninPromoViewStyleCompactTitled;
  EXPECT_EQ(view.promoViewStyle, SigninPromoViewStyleCompactTitled);
  // In compact layout, the primary button is plain.
  EXPECT_FALSE(view.primaryButton.backgroundColor);
  EXPECT_EQ(view.primaryButton.layer.cornerRadius, 0.0);
  // The secondary button should be hidden.
  EXPECT_TRUE(view.secondaryButton.hidden);

  // TODO(crbug.com/1412758): Test new promo styles.
}
