// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/passwords/password_infobar_banner_overlay_mediator.h"

#import <string>

#import "base/strings/utf_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "build/build_config.h"
#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/credential_provider_promo/features.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.h"
#import "ios/chrome/browser/passwords/test/mock_ios_chrome_save_passwords_infobar_delegate.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/infobars/banners/test/fake_infobar_banner_consumer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
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

// Test fixture for PasswordInfobarBannerOverlayMediatorTest.
class PasswordInfobarBannerOverlayMediatorTest : public PlatformTest {
 public:
  PasswordInfobarBannerOverlayMediatorTest() {}

  MockIOSChromeSavePasswordInfoBarDelegate& mock_delegate() {
    return *static_cast<MockIOSChromeSavePasswordInfoBarDelegate*>(
        infobar_->delegate());
  }

  void InitInfobar(
      absl::optional<std::string> account_to_store_password = absl::nullopt) {
    infobar_ = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypePasswordSave,
        MockIOSChromeSavePasswordInfoBarDelegate::Create(
            kUsername, kPassword, GURL::EmptyGURL(),
            account_to_store_password));
    request_ =
        OverlayRequest::CreateWithConfig<DefaultInfobarOverlayRequestConfig>(
            infobar_.get(), InfobarOverlayType::kBanner);
    consumer_ = [[FakeInfobarBannerConsumer alloc] init];
    mediator_ = [[PasswordInfobarBannerOverlayMediator alloc]
        initWithRequest:request_.get()];
    ;
    mediator_.consumer = consumer_;
  }

 protected:
  std::unique_ptr<InfoBarIOS> infobar_;
  std::unique_ptr<OverlayRequest> request_;
  FakeInfobarBannerConsumer* consumer_ = nil;
  PasswordInfobarBannerOverlayMediator* mediator_ = nil;
};

TEST_F(PasswordInfobarBannerOverlayMediatorTest,
       SetUpConsumerWithLocalStorage) {
  InitInfobar();

  // Verify that the infobar was set up properly.
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_SAVE_BUTTON),
              consumer_.buttonText);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT),
      consumer_.titleText);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_LOCAL_SAVE_SUBTITLE),
      consumer_.subtitleText);

#if !BUILDFLAG(IS_IOS_MACCATALYST)
  // Verify that the multi-color infobar icon was set up properly.
  EXPECT_NSEQ(MakeSymbolMulticolor(CustomSymbolWithPointSize(
                  kMulticolorPasswordSymbol, kInfobarSymbolPointSize)),
              consumer_.iconImage);
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

TEST_F(PasswordInfobarBannerOverlayMediatorTest,
       SetUpConsumerWithAccountStorage) {
  InitInfobar(kAccount);

  // Verify that the infobar was set up properly.
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_SAVE_BUTTON),
              consumer_.buttonText);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT),
      consumer_.titleText);
  EXPECT_NSEQ(
      l10n_util::GetNSStringF(IDS_IOS_PASSWORD_MANAGER_ON_ACCOUNT_SAVE_SUBTITLE,
                              base::UTF8ToUTF16(std::string(kAccount))),
      consumer_.subtitleText);

#if !BUILDFLAG(IS_IOS_MACCATALYST)
  // Verify that the multi-color infobar icon was set up properly.
  EXPECT_NSEQ(MakeSymbolMulticolor(CustomSymbolWithPointSize(
                  kMulticolorPasswordSymbol, kInfobarSymbolPointSize)),
              consumer_.iconImage);
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

// Tests that tapping the main button calls the `Accept()` delegate method.
TEST_F(PasswordInfobarBannerOverlayMediatorTest, MainButtonTapped) {
  InitInfobar();
  EXPECT_CALL(mock_delegate(), Accept()).WillOnce(testing::Return(true));
  [mediator_ bannerInfobarButtonWasPressed:nil];
}

// Tests that tapping the main button sends CredentialProviderPromo command.
TEST_F(PasswordInfobarBannerOverlayMediatorTest,
       MainButtonTriggersCredentialProviderPromo) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kCredentialProviderExtensionPromo,
      {{"enable_promo_on_password_saved", "true"}});
  InitInfobar();

  id commands_handler =
      OCMStrictProtocolMock(@protocol(CredentialProviderPromoCommands));
  [mock_delegate().GetDispatcher()
      startDispatchingToTarget:commands_handler
                   forProtocol:@protocol(CredentialProviderPromoCommands)];

  [[commands_handler expect] showCredentialProviderPromoWithTrigger:
                                 CredentialProviderPromoTrigger::PasswordSaved];

  [mediator_ bannerInfobarButtonWasPressed:nil];
  [commands_handler verify];
}
