// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/fullscreen_signin_screen/ui/fullscreen_signin_screen_view_controller.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/fullscreen_signin_screen/ui/fullscreen_signin_screen_consumer.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_context_style.h"
#import "ios/chrome/browser/authentication/ui_bundled/views/identity_button_control.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using FullscreenSigninScreenViewControllerTest = PlatformTest;

// Creates a test avatar image of `size`.
UIImage* CreateTestAvatar(CGSize size) {
  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:size];
  return [renderer imageWithActions:^(UIGraphicsImageRendererContext* context){
  }];
}

// Tests that the default context style configures standard banner, interactive
// identity control, and "Stay Signed Out" secondary action.
TEST_F(FullscreenSigninScreenViewControllerTest, DefaultContextStyle) {
  FullscreenSigninScreenViewController* viewController =
      [[FullscreenSigninScreenViewController alloc]
          initWithContextStyle:SigninContextStyle::kDefault];
  viewController.screenIntent = SigninScreenConsumerScreenIntentSigninOnly;
  viewController.signinStatus = SigninScreenConsumerSigninStatusAvailable;
  [viewController noIdentityAvailable];

  [viewController loadViewIfNeeded];

  IdentityButtonControl* identityControl =
      chrome_test_util::FindViewByClass<IdentityButtonControl>(
          viewController.view);
  ASSERT_NE(identityControl, nil);
  EXPECT_FALSE(viewController.shouldHideBanner);
  EXPECT_TRUE(identityControl.interactive);
  EXPECT_TRUE(identityControl.userInteractionEnabled);
  EXPECT_NSEQ(viewController.configuration.secondaryActionString,
              l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_STAY_SIGNED_OUT));
}

// Tests that the Account Switching promo context style configures hidden
// banner, avatar header, custom title and bold email in subtitle (preserving
// center alignment and surviving dynamic subtitle updates), "Not Now" secondary
// action, and non-interactive identity control with hidden chevron.
TEST_F(FullscreenSigninScreenViewControllerTest,
       AccountSwitchingPromoContextStyle) {
  FullscreenSigninScreenViewController* viewController =
      [[FullscreenSigninScreenViewController alloc]
          initWithContextStyle:SigninContextStyle::kAccountSwitchingPromo];
  viewController.screenIntent = SigninScreenConsumerScreenIntentSigninOnly;
  viewController.signinStatus = SigninScreenConsumerSigninStatusAvailable;
  UIImage* avatar = CreateTestAvatar(CGSizeMake(40, 40));
  UIImage* headerAvatar = CreateTestAvatar(CGSizeMake(48, 48));
  NSString* email = @"user@example.com";
  NSString* expectedSubtitle =
      l10n_util::GetNSStringF(IDS_IOS_ACCOUNT_SWITCHING_PROMO_SUBTITLE,
                              base::SysNSStringToUTF16(email));
  viewController.subtitleOverride = expectedSubtitle;
  [viewController setSelectedIdentityUserName:@"User Name"
                                        email:email
                                    givenName:@"User"
                                       avatar:avatar
                                 headerAvatar:headerAvatar
                                      managed:NO];

  [viewController loadViewIfNeeded];

  IdentityButtonControl* identityControl =
      chrome_test_util::FindViewByClass<IdentityButtonControl>(
          viewController.view);
  ASSERT_NE(identityControl, nil);
  EXPECT_TRUE(viewController.shouldHideBanner);
  EXPECT_EQ(viewController.headerImageType, PromoStyleImageType::kAvatar);
  EXPECT_EQ(viewController.headerImage, headerAvatar);
  EXPECT_NSEQ(viewController.titleText,
              l10n_util::GetNSString(IDS_IOS_ACCOUNT_SWITCHING_PROMO_TITLE));
  EXPECT_NSEQ(viewController.subtitleText, expectedSubtitle);
  EXPECT_NSEQ(viewController.subtitleLabel.text, expectedSubtitle);
  EXPECT_EQ(viewController.subtitleLabel.textAlignment, NSTextAlignmentCenter);

  UIFont* expectedBaseFont =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  NSAttributedString* attributedSubtitle =
      viewController.subtitleLabel.attributedText;
  NSRange emailRange = [expectedSubtitle rangeOfString:email];
  ASSERT_NE(emailRange.location, static_cast<NSUInteger>(NSNotFound));
  UIFont* emailFont = [attributedSubtitle attribute:NSFontAttributeName
                                            atIndex:emailRange.location
                                     effectiveRange:nullptr];
  EXPECT_TRUE(emailFont.fontDescriptor.symbolicTraits &
              UIFontDescriptorTraitBold);
  EXPECT_FLOAT_EQ(emailFont.pointSize, expectedBaseFont.pointSize);
  if (emailRange.location > 0) {
    UIFont* prefixFont = [attributedSubtitle attribute:NSFontAttributeName
                                               atIndex:0
                                        effectiveRange:nullptr];
    EXPECT_FALSE(prefixFont.fontDescriptor.symbolicTraits &
                 UIFontDescriptorTraitBold);
  }

  // Updating `subtitleOverride` when the email is at index 0 must not leak bold
  // font onto the suffix on a subsequent update.
  NSString* leadingEmailSubtitle =
      [NSString stringWithFormat:@"%@ suffix", email];
  viewController.subtitleOverride = leadingEmailSubtitle;
  viewController.subtitleOverride = expectedSubtitle;
  attributedSubtitle = viewController.subtitleLabel.attributedText;
  emailFont = [attributedSubtitle attribute:NSFontAttributeName
                                    atIndex:emailRange.location
                             effectiveRange:nullptr];
  EXPECT_TRUE(emailFont.fontDescriptor.symbolicTraits &
              UIFontDescriptorTraitBold);
  if (emailRange.location > 0) {
    UIFont* prefixFont = [attributedSubtitle attribute:NSFontAttributeName
                                               atIndex:0
                                        effectiveRange:nullptr];
    EXPECT_FALSE(prefixFont.fontDescriptor.symbolicTraits &
                 UIFontDescriptorTraitBold);
  }

  EXPECT_NSEQ(viewController.configuration.secondaryActionString,
              l10n_util::GetNSString(IDS_NOT_NOW));
  EXPECT_EQ(0u, viewController.disclaimerText.length);
  EXPECT_FALSE(identityControl.interactive);
  EXPECT_FALSE(identityControl.userInteractionEnabled);
}

// Tests that the Account Switching promo context style shows the platform
// policy disclaimer when `hasPlatformPolicies` is `YES`.
TEST_F(FullscreenSigninScreenViewControllerTest,
       AccountSwitchingPromoPlatformPolicyDisclaimer) {
  FullscreenSigninScreenViewController* viewController =
      [[FullscreenSigninScreenViewController alloc]
          initWithContextStyle:SigninContextStyle::kAccountSwitchingPromo];
  viewController.screenIntent = SigninScreenConsumerScreenIntentSigninOnly;
  viewController.signinStatus = SigninScreenConsumerSigninStatusAvailable;
  viewController.hasPlatformPolicies = YES;
  NSString* email = @"user@example.com";
  viewController.subtitleOverride =
      l10n_util::GetNSStringF(IDS_IOS_ACCOUNT_SWITCHING_PROMO_SUBTITLE,
                              base::SysNSStringToUTF16(email));
  [viewController
      setSelectedIdentityUserName:@"User Name"
                            email:email
                        givenName:@"User"
                           avatar:CreateTestAvatar(CGSizeMake(40, 40))
                     headerAvatar:CreateTestAvatar(CGSizeMake(48, 48))
                          managed:YES];

  [viewController loadViewIfNeeded];

  EXPECT_NSEQ(
      viewController.disclaimerText,
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_BROWSER_MANAGED));
}

// Tests that the Account Switching promo context style uses `titleOverride`
// when set before the view is loaded.
TEST_F(FullscreenSigninScreenViewControllerTest,
       AccountSwitchingPromoTitleOverride) {
  FullscreenSigninScreenViewController* viewController =
      [[FullscreenSigninScreenViewController alloc]
          initWithContextStyle:SigninContextStyle::kAccountSwitchingPromo];
  viewController.screenIntent = SigninScreenConsumerScreenIntentSigninOnly;
  viewController.signinStatus = SigninScreenConsumerSigninStatusAvailable;
  NSString* expectedTitle = l10n_util::GetNSString(
      IDS_IOS_ACCOUNT_SWITCHING_PROMO_TITLE_FOR_DEVICE_TYPE);
  viewController.titleOverride = expectedTitle;
  NSString* email = @"user@example.com";
  viewController.subtitleOverride =
      l10n_util::GetNSStringF(IDS_IOS_ACCOUNT_SWITCHING_PROMO_SUBTITLE,
                              base::SysNSStringToUTF16(email));
  [viewController
      setSelectedIdentityUserName:@"User Name"
                            email:email
                        givenName:@"User"
                           avatar:CreateTestAvatar(CGSizeMake(40, 40))
                     headerAvatar:CreateTestAvatar(CGSizeMake(48, 48))
                          managed:NO];

  [viewController loadViewIfNeeded];

  EXPECT_NSEQ(viewController.titleText, expectedTitle);
}

}  // namespace
