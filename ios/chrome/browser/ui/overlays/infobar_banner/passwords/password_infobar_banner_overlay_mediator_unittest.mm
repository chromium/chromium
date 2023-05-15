// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/passwords/password_infobar_banner_overlay_mediator.h"

#import <string>

#import "base/strings/utf_string_conversions.h"
#import "build/build_config.h"
#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/password_infobar_banner_overlay.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.h"
#import "ios/chrome/browser/passwords/test/mock_ios_chrome_save_passwords_infobar_delegate.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/infobars/banners/test/fake_infobar_banner_consumer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Constants used in tests.
NSString* const kUsername = @"username";
NSString* const kPassword = @"12345";
const char kAccount[] = "foobar@gmail.com";
}  // namespace

// Test fixture for PasswordInfobarBannerOverlayMediator.
using PasswordInfobarBannerOverlayMediatorTest = PlatformTest;

TEST_F(PasswordInfobarBannerOverlayMediatorTest,
       SetUpConsumerWithLocalStorage) {
  // Create an InfoBarIOS with a IOSChromeSavePasswordInfoBarDelegate.
  InfoBarIOS infobar(InfobarType::kInfobarTypePasswordSave,
                     MockIOSChromeSavePasswordInfoBarDelegate::Create(
                         kUsername, kPassword, GURL::EmptyGURL(),
                         /*account_to_store_password=*/absl::nullopt));
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
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_SAVE_BUTTON),
              consumer.buttonText);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT),
      consumer.titleText);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_LOCAL_SAVE_SUBTITLE),
      consumer.subtitleText);

#if !BUILDFLAG(IS_IOS_MACCATALYST)
  // Verify that the multi-color infobar icon was set up properly.
  EXPECT_NSEQ(MakeSymbolMulticolor(CustomSymbolWithPointSize(
                  kMulticolorPasswordSymbol, kInfobarSymbolPointSize)),
              consumer.iconImage);
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

TEST_F(PasswordInfobarBannerOverlayMediatorTest,
       SetUpConsumerWithAccountStorage) {
  // Create an InfoBarIOS with a IOSChromeSavePasswordInfoBarDelegate.
  InfoBarIOS infobar(InfobarType::kInfobarTypePasswordSave,
                     MockIOSChromeSavePasswordInfoBarDelegate::Create(
                         kUsername, kPassword, GURL::EmptyGURL(), kAccount));
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
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_SAVE_BUTTON),
              consumer.buttonText);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT),
      consumer.titleText);
  EXPECT_NSEQ(
      l10n_util::GetNSStringF(IDS_IOS_PASSWORD_MANAGER_ON_ACCOUNT_SAVE_SUBTITLE,
                              base::UTF8ToUTF16(std::string(kAccount))),
      consumer.subtitleText);

#if !BUILDFLAG(IS_IOS_MACCATALYST)
  // Verify that the multi-color infobar icon was set up properly.
  EXPECT_NSEQ(MakeSymbolMulticolor(CustomSymbolWithPointSize(
                  kMulticolorPasswordSymbol, kInfobarSymbolPointSize)),
              consumer.iconImage);
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}
