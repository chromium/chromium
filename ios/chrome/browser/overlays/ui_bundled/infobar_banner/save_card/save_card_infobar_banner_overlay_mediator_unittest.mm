// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/save_card/save_card_infobar_banner_overlay_mediator.h"

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/uuid.h"
#import "components/autofill/core/browser/data_model/payments/credit_card.h"
#import "components/autofill/core/browser/foundations/autofill_client.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_card_infobar_delegate_mobile.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_delegate.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/test/fake_infobar_banner_consumer.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

using testing::_;
using SaveCreditCardPromptResultIOS =
    autofill::autofill_metrics::SaveCreditCardPromptResultIOS;
using SaveCreditCardOptions =
    autofill::payments::PaymentsAutofillClient::SaveCreditCardOptions;

constexpr char kSaveCreditCardPromptResultHistogramStringForLocalSave[] =
    "Autofill.SaveCreditCardPromptResult.IOS.Local.Banner.NumStrikes.0."
    "NoFixFlow";
constexpr char kSaveCreditCardPromptResultHistogramStringForServerSave[] =
    "Autofill.SaveCreditCardPromptResult.IOS.Server.Banner.NumStrikes.0."
    "NoFixFlow";

}  // namespace

// Test fixture for SaveCardInfobarBannerOverlayMediator.
class SaveCardInfobarBannerOverlayMediatorTest : public PlatformTest {
 public:
  SaveCardInfobarBannerOverlayMediatorTest() {}

  void InitInfobar(const bool for_upload) {
    autofill::CreditCard credit_card(
        base::Uuid::GenerateRandomV4().AsLowercaseString(),
        "https://www.example.com/");
    ;
    std::unique_ptr<MockAutofillSaveCardInfoBarDelegateMobile> delegate =
        MockAutofillSaveCardInfoBarDelegateMobileFactory::
            CreateMockAutofillSaveCardInfoBarDelegateMobileFactory(
                for_upload, credit_card,
                SaveCreditCardOptions().with_num_strikes(0));
    delegate_ = delegate.get();
    infobar_ = std::make_unique<InfoBarIOS>(InfobarType::kInfobarTypeSaveCard,
                                            std::move(delegate));
    request_ =
        OverlayRequest::CreateWithConfig<DefaultInfobarOverlayRequestConfig>(
            infobar_.get(), InfobarOverlayType::kBanner);
    consumer_ = [[FakeInfobarBannerConsumer alloc] init];
    mediator_ = OCMPartialMock([[SaveCardInfobarBannerOverlayMediator alloc]
        initWithRequest:request_.get()]);

    mediator_.consumer = consumer_;
  }

 protected:
  std::unique_ptr<InfoBarIOS> infobar_;
  std::unique_ptr<OverlayRequest> request_;
  raw_ptr<MockAutofillSaveCardInfoBarDelegateMobile> delegate_ = nil;
  FakeInfobarBannerConsumer* consumer_ = nil;
  SaveCardInfobarBannerOverlayMediator* mediator_ = nil;
};

TEST_F(SaveCardInfobarBannerOverlayMediatorTest, SetUpConsumer) {
  InitInfobar(/*for_upload=*/false);

  // Verify that the infobar was set up properly.
  NSString* title = base::SysUTF16ToNSString(delegate_->GetMessageText());
  EXPECT_NSEQ(title, consumer_.titleText);
  EXPECT_NSEQ(base::SysUTF16ToNSString(
                  delegate_->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK)),
              consumer_.buttonText);
  EXPECT_NSEQ(base::SysUTF16ToNSString(delegate_->card_label()),
              consumer_.subtitleText);
}

// Tests that when upload is turned on, tapping on the banner action button
// presents the modal.
TEST_F(SaveCardInfobarBannerOverlayMediatorTest, PresentModalWhenUploadOn) {
  InitInfobar(/*for_upload=*/true);

  OCMExpect([mediator_ presentInfobarModalFromBanner]);
  [mediator_ bannerInfobarButtonWasPressed:nil];
}

// Tests that when upload is turned off, tapping on the banner action button
// does not present the modal.
TEST_F(SaveCardInfobarBannerOverlayMediatorTest, PresentModalWhenUploadOff) {
  InitInfobar(/*for_upload=*/false);

  EXPECT_CALL(*delegate_, UpdateAndAccept(delegate_->cardholder_name(),
                                          delegate_->expiration_date_month(),
                                          delegate_->expiration_date_year()));
  [mediator_ bannerInfobarButtonWasPressed:nil];
}

// Verifies histogram entries for server save infobar banner shown and accepted.
TEST_F(SaveCardInfobarBannerOverlayMediatorTest,
       LogInfoBarBannerShownAndAcceptedForUploadSave) {
  base::HistogramTester histogram_tester;
  InitInfobar(/*for_upload=*/true);
  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave,
      SaveCreditCardPromptResultIOS::kShown, 1);

  [mediator_ bannerInfobarButtonWasPressed:nil];
  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave,
      SaveCreditCardPromptResultIOS::kAccepted, 1);

  histogram_tester.ExpectTotalCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave, 2);
}

// Verifies histogram entries for local save infobar banner shown and accepted.
TEST_F(SaveCardInfobarBannerOverlayMediatorTest,
       LogInfoBarBannerShownAndAcceptedForLocalSave) {
  base::HistogramTester histogram_tester;
  InitInfobar(/*for_upload=*/false);
  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForLocalSave,
      SaveCreditCardPromptResultIOS::kShown, 1);

  [mediator_ bannerInfobarButtonWasPressed:nil];
  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForLocalSave,
      SaveCreditCardPromptResultIOS::kAccepted, 1);

  // Verify that local save banner's button when pressed is not recorded as user
  // initiated dismissal.
  [mediator_ dismissInfobarBannerForUserInteraction:YES];
  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForLocalSave,
      SaveCreditCardPromptResultIOS::kSwiped, 0);

  histogram_tester.ExpectTotalCount(
      kSaveCreditCardPromptResultHistogramStringForLocalSave, 2);
}

// Verifies histogram entries for server save banner shown and gets swiped up.
TEST_F(SaveCardInfobarBannerOverlayMediatorTest,
       LogInfoBarBannerShownAndSwipedForUploadSave) {
  base::HistogramTester histogram_tester;
  InitInfobar(/*for_upload=*/true);
  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave,
      SaveCreditCardPromptResultIOS::kShown, 1);

  [mediator_ dismissInfobarBannerForUserInteraction:YES];
  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave,
      SaveCreditCardPromptResultIOS::kSwiped, 1);

  histogram_tester.ExpectTotalCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave, 2);
}

// Verifies histogram entries for local save banner shown and gets swiped up.
TEST_F(SaveCardInfobarBannerOverlayMediatorTest,
       LogInfoBarBannerShownAndSwipedForLocalSave) {
  base::HistogramTester histogram_tester;
  InitInfobar(/*for_upload=*/false);
  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForLocalSave,
      SaveCreditCardPromptResultIOS::kShown, 1);

  [mediator_ dismissInfobarBannerForUserInteraction:YES];
  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForLocalSave,
      SaveCreditCardPromptResultIOS::kSwiped, 1);

  histogram_tester.ExpectTotalCount(
      kSaveCreditCardPromptResultHistogramStringForLocalSave, 2);
}

// Verifies histogram entries for server save banner shown and then times out.
TEST_F(SaveCardInfobarBannerOverlayMediatorTest,
       LogInfoBarBannerShownAndTimedOutForUploadSave) {
  base::HistogramTester histogram_tester;
  InitInfobar(/*for_upload=*/true);
  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave,
      SaveCreditCardPromptResultIOS::kShown, 1);

  [mediator_ dismissInfobarBannerForUserInteraction:NO];
  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave,
      SaveCreditCardPromptResultIOS::KTimedOut, 1);

  histogram_tester.ExpectTotalCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave, 2);
}

// Verifies histogram entries for local save banner shown and then times out.
TEST_F(SaveCardInfobarBannerOverlayMediatorTest,
       LogInfoBarBannerShownAndTimedOutForLocalSave) {
  base::HistogramTester histogram_tester;
  InitInfobar(/*for_upload=*/false);
  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForLocalSave,
      SaveCreditCardPromptResultIOS::kShown, 1);

  [mediator_ dismissInfobarBannerForUserInteraction:NO];
  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForLocalSave,
      SaveCreditCardPromptResultIOS::KTimedOut, 1);

  histogram_tester.ExpectTotalCount(
      kSaveCreditCardPromptResultHistogramStringForLocalSave, 2);
}
