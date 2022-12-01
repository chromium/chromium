// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/passwords/password_infobar_banner_overlay_mediator.h"

#import <string>

#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/infobars/core/infobar.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/password_infobar_banner_overlay.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.h"
#import "ios/chrome/browser/passwords/test/mock_ios_chrome_save_passwords_infobar_delegate.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/infobars/banners/test/fake_infobar_banner_consumer.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Constants used in tests.
NSString* const kUsername = @"username";
NSString* const kPassword = @"12345";
}  // namespace

// Test fixture for PasswordInfobarBannerOverlayMediator.
using PasswordInfobarBannerOverlayMediatorTest = PlatformTest;

// Tests that a PasswordInfobarBannerOverlayMediator correctly sets up its
// consumer.
TEST_F(PasswordInfobarBannerOverlayMediatorTest, SetUpConsumer) {
  // Create an InfoBarIOS with a IOSChromeSavePasswordInfoBarDelegate.
  std::unique_ptr<IOSChromeSavePasswordInfoBarDelegate> passed_delegate =
      MockIOSChromeSavePasswordInfoBarDelegate::Create(kUsername, kPassword);
  IOSChromeSavePasswordInfoBarDelegate* delegate = passed_delegate.get();
  InfoBarIOS infobar(InfobarType::kInfobarTypePasswordSave,
                     std::move(passed_delegate));
  // Package the infobar into an OverlayRequest, then create a mediator that
  // uses this request in order to set up a fake consumer.
  std::unique_ptr<OverlayRequest> request = OverlayRequest::CreateWithConfig<
      PasswordInfobarBannerOverlayRequestConfig>(&infobar);
  PasswordInfobarBannerOverlayMediator* mediator =
      [[PasswordInfobarBannerOverlayMediator alloc]
          initWithRequest:request.get()];
  FakeInfobarBannerConsumer* consumer =
      [[FakeInfobarBannerConsumer alloc] init];
  mediator.consumer = consumer;

  // Verify that the infobar was set up properly.
  NSString* title = base::SysUTF16ToNSString(delegate->GetMessageText());
  NSString* password = [@"" stringByPaddingToLength:kPassword.length
                                         withString:@"•"
                                    startingAtIndex:0];
  NSString* subtitle =
      [NSString stringWithFormat:@"%@ %@", kUsername, password];
  NSString* bannerAccessibilityLabel =
      [NSString stringWithFormat:@"%@,%@, %@", title, kUsername,
                                 l10n_util::GetNSString(
                                     IDS_IOS_SETTINGS_PASSWORD_HIDDEN_LABEL)];
  EXPECT_NSEQ(bannerAccessibilityLabel, consumer.bannerAccessibilityLabel);
  EXPECT_NSEQ(base::SysUTF16ToNSString(
                  delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK)),
              consumer.buttonText);
  EXPECT_NSEQ(title, consumer.titleText);
  EXPECT_NSEQ(subtitle, consumer.subtitleText);

  EXPECT_TRUE(consumer.presentsModal);
}

// Tests that a PasswordInfobarBannerOverlayMediator correctly sets up its
// consumer's icon with legacy assets.
TEST_F(PasswordInfobarBannerOverlayMediatorTest,
       SetUpConsumerIconNotUseSymbols) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kUseSFSymbols);

  // Create an InfoBarIOS with a IOSChromeSavePasswordInfoBarDelegate.
  std::unique_ptr<IOSChromeSavePasswordInfoBarDelegate> passed_delegate =
      MockIOSChromeSavePasswordInfoBarDelegate::Create(kUsername, kPassword);
  InfoBarIOS infobar(InfobarType::kInfobarTypePasswordSave,
                     std::move(passed_delegate));
  // Package the infobar into an OverlayRequest, then create a mediator that
  // uses this request in order to set up a fake consumer.
  std::unique_ptr<OverlayRequest> request = OverlayRequest::CreateWithConfig<
      PasswordInfobarBannerOverlayRequestConfig>(&infobar);
  PasswordInfobarBannerOverlayMediator* mediator =
      [[PasswordInfobarBannerOverlayMediator alloc]
          initWithRequest:request.get()];
  FakeInfobarBannerConsumer* consumer =
      [[FakeInfobarBannerConsumer alloc] init];
  mediator.consumer = consumer;

  // Verify that the infobar icon was set up properly.
  if (base::FeatureList::IsEnabled(
          password_manager::features::
              kIOSEnablePasswordManagerBrandingUpdate)) {
    EXPECT_NSEQ([UIImage imageNamed:@"password_key"], consumer.iconImage);
  } else {
    EXPECT_NSEQ([UIImage imageNamed:@"legacy_password_key"],
                consumer.iconImage);
  }
}

// Tests that a PasswordInfobarBannerOverlayMediator correctly sets up its
// consumer's icon with SF symbol.
TEST_F(PasswordInfobarBannerOverlayMediatorTest, SetUpConsumerIconUseSymbols) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kUseSFSymbols);

  // Create an InfoBarIOS with a IOSChromeSavePasswordInfoBarDelegate.
  std::unique_ptr<IOSChromeSavePasswordInfoBarDelegate> passed_delegate =
      MockIOSChromeSavePasswordInfoBarDelegate::Create(kUsername, kPassword);
  InfoBarIOS infobar(InfobarType::kInfobarTypePasswordSave,
                     std::move(passed_delegate));
  // Package the infobar into an OverlayRequest, then create a mediator that
  // uses this request in order to set up a fake consumer.
  std::unique_ptr<OverlayRequest> request = OverlayRequest::CreateWithConfig<
      PasswordInfobarBannerOverlayRequestConfig>(&infobar);
  PasswordInfobarBannerOverlayMediator* mediator =
      [[PasswordInfobarBannerOverlayMediator alloc]
          initWithRequest:request.get()];
  FakeInfobarBannerConsumer* consumer =
      [[FakeInfobarBannerConsumer alloc] init];
  mediator.consumer = consumer;

  // Verify that the infobar icon was set up properly.
  EXPECT_NSEQ(
      CustomSymbolWithPointSize(kPasswordSymbol, kInfobarSymbolPointSize),
      consumer.iconImage);
}
