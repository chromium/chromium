// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view.h"

#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/gtest_support.h"

using SigninPromoViewTest = PlatformTest;

TEST_F(SigninPromoViewTest, ChromiumLogoImage) {
  UIWindow* currentWindow = GetAnyKeyWindow();
  SigninPromoView* view =
      [[SigninPromoView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  [view configurePrimaryButtonWithTitle:@"Test"];
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
  view.mode = SigninPromoViewModeSignedInWithPrimaryAccount;
  EXPECT_NE(nil, view.imageView.image);
  // The image should has been changed from the logo.
  EXPECT_NE(chromiumLogo, view.imageView.image);
  // The image should be different than the one set, since a circular background
  // should have been added.
  EXPECT_NE(customImage, view.imageView.image);
  [view removeFromSuperview];
}

TEST_F(SigninPromoViewTest, SecondaryButtonVisibility) {
  UIWindow* currentWindow = GetAnyKeyWindow();
  SigninPromoView* view =
      [[SigninPromoView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  [view configurePrimaryButtonWithTitle:@"Test"];
  view.mode = SigninPromoViewModeNoAccounts;
  [currentWindow.rootViewController.view addSubview:view];
  EXPECT_TRUE(view.secondaryButton.hidden);
  view.mode = SigninPromoViewModeSigninWithAccount;
  EXPECT_FALSE(view.secondaryButton.hidden);
  view.mode = SigninPromoViewModeSignedInWithPrimaryAccount;
  EXPECT_TRUE(view.secondaryButton.hidden);
  [view removeFromSuperview];
}

// Tests the accessibility label (based on the `textLabel` and the primary
// button title) as SigninPromoViewStyleStandard.
TEST_F(SigninPromoViewTest, AccessibilityLabelSigninPromoViewStyleStandard) {
  UIWindow* currentWindow = GetAnyKeyWindow();
  SigninPromoView* view =
      [[SigninPromoView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  [currentWindow.rootViewController.view addSubview:view];
  UIButtonConfiguration* buttonConfigutation = view.primaryButton.configuration;
  NSString* primaryButtonTitle = @"Primary Button Title";
  buttonConfigutation.title = primaryButtonTitle;
  view.primaryButton.configuration = buttonConfigutation;
  NSString* promoText = @"This is the promo text.";
  view.textLabel.text = promoText;
  view.promoViewStyle = SigninPromoViewStyleStandard;
  NSString* expectedAccessibilityLabel =
      [NSString stringWithFormat:@"%@ %@", promoText, primaryButtonTitle];
  EXPECT_NSEQ(view.accessibilityLabel, expectedAccessibilityLabel);
  [view removeFromSuperview];
}

// Tests the accessibility label (based on the `textLabel` and the primary
// button title) as SigninPromoViewStyleCompact.
TEST_F(SigninPromoViewTest, AccessibilityLabelSigninPromoViewStyleCompact) {
  UIWindow* currentWindow = GetAnyKeyWindow();
  SigninPromoView* view =
      [[SigninPromoView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  [currentWindow.rootViewController.view addSubview:view];
  UIButtonConfiguration* buttonConfigutation = view.primaryButton.configuration;
  NSString* primaryButtonTitle = @"Primary Button Title";
  buttonConfigutation.title = primaryButtonTitle;
  view.primaryButton.configuration = buttonConfigutation;
  NSString* promoText = @"This is the promo text.";
  view.textLabel.text = promoText;
  view.promoViewStyle = SigninPromoViewStyleCompact;
  NSString* expectedAccessibilityLabel =
      [NSString stringWithFormat:@"%@ %@", promoText, primaryButtonTitle];
  EXPECT_NSEQ(view.accessibilityLabel, expectedAccessibilityLabel);
  [view removeFromSuperview];
}

// Tests the accessibility label (based on the `textLabel` and the primary
// button title) as SigninPromoViewStyleOnlyButton.
TEST_F(SigninPromoViewTest, AccessibilityLabelSigninPromoViewStyleOnlyButton) {
  UIWindow* currentWindow = GetAnyKeyWindow();
  SigninPromoView* view =
      [[SigninPromoView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  [currentWindow.rootViewController.view addSubview:view];
  UIButtonConfiguration* buttonConfigutation = view.primaryButton.configuration;
  NSString* primaryButtonTitle = @"Primary Button Title";
  buttonConfigutation.title = primaryButtonTitle;
  view.primaryButton.configuration = buttonConfigutation;
  NSString* promoText = @"This is the promo text.";
  view.textLabel.text = promoText;
  view.promoViewStyle = SigninPromoViewStyleOnlyButton;
  NSString* expectedAccessibilityLabel = primaryButtonTitle;
  EXPECT_NSEQ(view.accessibilityLabel, expectedAccessibilityLabel);
  [view removeFromSuperview];
}

// Tests that signin is created on non-compact layout and that setting compact
// layout changes the primary button styling.
TEST_F(SigninPromoViewTest, ChangeLayout) {
  UIWindow* currentWindow = GetAnyKeyWindow();
  SigninPromoView* view =
      [[SigninPromoView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  [view configurePrimaryButtonWithTitle:@"Test"];
  view.mode = SigninPromoViewModeNoAccounts;
  [currentWindow.rootViewController.view addSubview:view];
  // The default mode should be CompactVertical.
  EXPECT_EQ(view.promoViewStyle, SigninPromoViewStyleStandard);
  // In full layout, the primary button is rounded with background color.
  EXPECT_TRUE(view.primaryButton.backgroundColor);
  EXPECT_GT(view.primaryButton.layer.cornerRadius, 0.0);

  // Switch to compact layout.
  view.promoViewStyle = SigninPromoViewStyleCompact;
  EXPECT_EQ(view.promoViewStyle, SigninPromoViewStyleCompact);
  // In compact layout, the primary button has a background color.
  EXPECT_TRUE(view.primaryButton.backgroundColor);
  EXPECT_GT(view.primaryButton.layer.cornerRadius, 0.0);
  // The secondary button should be hidden.
  EXPECT_TRUE(view.secondaryButton.hidden);
  [view removeFromSuperview];
}

// Tests that buttons are disabled or enabled when the spinner started or
// stopped.
TEST_F(SigninPromoViewTest, StartAndStopSpinner) {
  UIWindow* currentWindow = GetAnyKeyWindow();
  SigninPromoView* view =
      [[SigninPromoView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  [view configurePrimaryButtonWithTitle:@"Test"];
  view.mode = SigninPromoViewModeNoAccounts;
  [currentWindow.rootViewController.view addSubview:view];
  view.mode = SigninPromoViewModeSigninWithAccount;
  EXPECT_TRUE(view.primaryButton.enabled);
  EXPECT_TRUE(view.secondaryButton.enabled);
  EXPECT_TRUE(view.closeButton.enabled);
  [view startSignInSpinner];
  EXPECT_FALSE(view.primaryButton.enabled);
  EXPECT_FALSE(view.secondaryButton.enabled);
  EXPECT_FALSE(view.closeButton.enabled);
  [view stopSignInSpinner];
  EXPECT_TRUE(view.primaryButton.enabled);
  EXPECT_TRUE(view.secondaryButton.enabled);
  EXPECT_TRUE(view.closeButton.enabled);
  [view removeFromSuperview];
}
