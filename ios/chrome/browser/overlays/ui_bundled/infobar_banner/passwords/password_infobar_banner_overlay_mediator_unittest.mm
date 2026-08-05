// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/passwords/password_infobar_banner_overlay_mediator.h"

#import <string>

#import "base/strings/utf_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "build/build_config.h"
#import "components/infobars/core/infobar.h"
#import "components/metrics/profile_metrics_service.h"
#import "components/password_manager/core/browser/password_form_metrics_recorder.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/authentication/signin/non_modal_promo/coordinator/non_modal_signin_promo_types.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/test/fake_infobar_banner_consumer.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/passwords/infobars/model/ios_chrome_save_password_infobar_delegate.h"
#import "ios/chrome/browser/passwords/infobars/test/mock_ios_chrome_save_passwords_infobar_delegate.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/non_modal_signin_promo_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

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
      std::optional<std::string> account_to_store_password = std::nullopt) {
    if (account_to_store_password.has_value()) {
      CoreAccountInfo account_info;
      account_info.email = *account_to_store_password;
      sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account_info);
    } else {
      sync_service_.SetSignedOut();
    }

    metrics_recorder_ =
        base::MakeRefCounted<password_manager::PasswordFormMetricsRecorder>(
            /*is_main_frame_secure=*/true, ukm::kInvalidSourceId,
            /*pref_service=*/nullptr, &profile_metrics_service_);
    infobar_ = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypePasswordSave,
        MockIOSChromeSavePasswordInfoBarDelegate::Create(
            kUsername, kPassword, GURL(), &sync_service_,
            metrics_recorder_.get()));
    request_ =
        OverlayRequest::CreateWithConfig<DefaultInfobarOverlayRequestConfig>(
            infobar_.get(), InfobarOverlayType::kBanner);
    consumer_ = [[FakeInfobarBannerConsumer alloc] init];
    mediator_ = [[PasswordInfobarBannerOverlayMediator alloc]
        initWithRequest:request_.get()];
    mediator_.consumer = consumer_;
  }

 protected:
  metrics::ProfileMetricsService profile_metrics_service_;
  syncer::TestSyncService sync_service_;
  scoped_refptr<password_manager::PasswordFormMetricsRecorder>
      metrics_recorder_;
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
  EXPECT_NSEQ(MakeSymbolMulticolor(SymbolWithPointSize(
                  SymbolMulticolorPassword, kInfobarSymbolPointSize)),
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
  EXPECT_NSEQ(MakeSymbolMulticolor(SymbolWithPointSize(
                  SymbolMulticolorPassword, kInfobarSymbolPointSize)),
              consumer_.iconImage);
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

// Tests that tapping the main button calls the `Accept()` delegate method.
TEST_F(PasswordInfobarBannerOverlayMediatorTest, MainButtonTapped) {
  InitInfobar();
  EXPECT_CALL(mock_delegate(), Accept()).WillOnce(testing::Return(true));
  [mediator_ bannerInfobarButtonWasPressed:nil];
}

// Ensures that calling the -bannerInfobarButtonWasPressed: after the infobar
// has been removed does not cause a crash. This could happen if the infobar is
// removed before the banner has finished appearing.
TEST_F(PasswordInfobarBannerOverlayMediatorTest,
       BannerInfobarButtonWasPressedAfterRemoval) {
  InitInfobar();

  // Removes the infobar.
  infobar_ = nullptr;

  [mediator_ bannerInfobarButtonWasPressed:nil];
}

// Tests that the infobar delegate is called on -finishDismissal when the
// delegate is set.
TEST_F(PasswordInfobarBannerOverlayMediatorTest, InfobarDone) {
  InitInfobar();
  base::HistogramTester histogram_tester;
  [mediator_ finishDismissal];
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveUIDismissalReason",
      password_manager::metrics_util::NO_DIRECT_INTERACTION, 1);
}

// Tests that the infobar delegate isn't called on -finishDismissal when the
// infobar delegate is deleted.
TEST_F(PasswordInfobarBannerOverlayMediatorTest,
       InfobarDoneWhenInfobarDelegateDeleted) {
  InitInfobar();

  // Delete the infobar to return a nullptr delegate.
  infobar_.reset();

  base::HistogramTester histogram_tester;
  [mediator_ finishDismissal];
  histogram_tester.ExpectTotalCount("PasswordManager.SaveUIDismissalReason", 0);
}

// Tests that -finishDismissal triggers -showNonModalSignInPromoWithType: on
// nonModalSignInPromoHandler when the infobar delegate is set.
TEST_F(PasswordInfobarBannerOverlayMediatorTest,
       FinishDismissalShowsNonModalPromo) {
  InitInfobar();
  id mock_handler = OCMProtocolMock(@protocol(NonModalSignInPromoCommands));
  mediator_.nonModalSignInPromoHandler = mock_handler;
  OCMExpect([mock_handler
      showNonModalSignInPromoWithType:NonModalSignInPromoType::kPassword]);
  [mediator_ finishDismissal];
  EXPECT_OCMOCK_VERIFY(mock_handler);
}
