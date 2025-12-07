// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/save_cvc/save_cvc_infobar_banner_overlay_mediator.h"

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/uuid.h"
#import "components/autofill/core/browser/data_model/payments/credit_card.h"
#import "components/autofill/core/browser/foundations/autofill_client.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "components/autofill/core/common/autofill_payments_features.h"
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
using SaveCreditCardOptions =
    autofill::payments::PaymentsAutofillClient::SaveCreditCardOptions;
using SaveCvcPromptResultIOS =
    autofill::autofill_metrics::SaveCvcPromptResultIOS;
using autofill::autofill_metrics::SaveCardPromptOffer;

constexpr char kSaveCvcPromptOfferHistogramStringForLocalSave[] =
    "Autofill.SaveCvcPromptOffer.IOS.Local";
constexpr char kSaveCvcPromptOfferHistogramStringForUploadSave[] =
    "Autofill.SaveCvcPromptOffer.IOS.Upload";
constexpr char kSaveCvcPromptResultHistogramStringForLocalSave[] =
    "Autofill.SaveCvcPromptResult.IOS.Local";
constexpr char kSaveCvcPromptResultHistogramStringForUploadSave[] =
    "Autofill.SaveCvcPromptResult.IOS.Upload";
}  // namespace

// Test fixture for SaveCVCInfobarBannerOverlayMediator.
class SaveCVCInfobarBannerOverlayMediatorTest : public PlatformTest {
 public:
  SaveCVCInfobarBannerOverlayMediatorTest() {
    feature_list_.InitAndEnableFeature(
        autofill::features::kAutofillEnableCvcStorageAndFilling);
  }
  ~SaveCVCInfobarBannerOverlayMediatorTest() override {
    EXPECT_OCMOCK_VERIFY((id)mediator_);
  }

  void InitInfobar(const bool for_upload) {
    autofill::CreditCard credit_card(
        base::Uuid::GenerateRandomV4().AsLowercaseString(),
        "https://www.example.com/");
    ;
    SaveCreditCardOptions options;
    options.card_save_type =
        autofill::payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly;
    std::unique_ptr<MockAutofillSaveCardInfoBarDelegateMobile> delegate =
        MockAutofillSaveCardInfoBarDelegateMobileFactory::
            CreateMockAutofillSaveCardInfoBarDelegateMobileFactory(
                for_upload, credit_card, options);
    delegate_ = delegate.get();
    infobar_ = std::make_unique<InfoBarIOS>(InfobarType::kInfobarTypeSaveCvc,
                                            std::move(delegate));
    request_ =
        OverlayRequest::CreateWithConfig<DefaultInfobarOverlayRequestConfig>(
            infobar_.get(), InfobarOverlayType::kBanner);
    consumer_ = [[FakeInfobarBannerConsumer alloc] init];
    mediator_ = OCMPartialMock([[SaveCVCInfobarBannerOverlayMediator alloc]
        initWithRequest:request_.get()]);

    mediator_.consumer = consumer_;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<InfoBarIOS> infobar_;
  std::unique_ptr<OverlayRequest> request_;
  raw_ptr<MockAutofillSaveCardInfoBarDelegateMobile> delegate_ = nil;
  FakeInfobarBannerConsumer* consumer_ = nil;
  SaveCVCInfobarBannerOverlayMediator* mediator_ = nil;
};

// Tests that the consumer is set up with the correct properties from the
// delegate.
TEST_F(SaveCVCInfobarBannerOverlayMediatorTest, SetUpConsumer) {
  InitInfobar(/*for_upload=*/false);

  // Verify that the infobar was set up properly.
  NSString* title = base::SysUTF16ToNSString(delegate_->GetMessageText());
  EXPECT_NSEQ(title, consumer_.titleText);
  EXPECT_NSEQ(base::SysUTF16ToNSString(
                  delegate_->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK)),
              consumer_.buttonText);
  EXPECT_NSEQ(base::SysUTF16ToNSString(delegate_->GetDescriptionText()),
              consumer_.subtitleText);
}

// Tests that tapping on the banner action button does not present the modal.
TEST_F(SaveCVCInfobarBannerOverlayMediatorTest, PresentModal) {
  InitInfobar(/*for_upload=*/false);

  EXPECT_CALL(*delegate_, UpdateAndAccept(delegate_->cardholder_name(),
                                          delegate_->expiration_date_month(),
                                          delegate_->expiration_date_year(),
                                          delegate_->card_cvc()));

  [mediator_ bannerInfobarButtonWasPressed:nil];
}

class SaveCVCInfobarBannerOverlayMediatorOfferTest
    : public SaveCVCInfobarBannerOverlayMediatorTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  // Returns whether the save CVC flow is an upload saving flow.
  bool is_upload() const { return GetParam(); }
};

TEST_P(SaveCVCInfobarBannerOverlayMediatorOfferTest, LogsOfferMetric) {
  base::HistogramTester histogram_tester;
  InitInfobar(is_upload());

  histogram_tester.ExpectUniqueSample(
      is_upload() ? kSaveCvcPromptOfferHistogramStringForUploadSave
                  : kSaveCvcPromptOfferHistogramStringForLocalSave,
      SaveCardPromptOffer::kShown, 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SaveCVCInfobarBannerOverlayMediatorOfferTest,
                         ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "Upload" : "Local";
                         });

struct CvcBannerResultTestCase {
  const std::string name;
  const bool is_upload;
  enum class Action { kAccept, kSwipe, kTimeout };
  const Action action;
  const SaveCvcPromptResultIOS expected_result;
};

class SaveCVCInfobarBannerOverlayMediatorResultTest
    : public SaveCVCInfobarBannerOverlayMediatorTest,
      public ::testing::WithParamInterface<CvcBannerResultTestCase> {
 public:
  void SetUp() override { PlatformTest::SetUp(); }

 protected:
  // Returns whether the save CVC flow is an upload saving flow.
  bool is_upload() const { return GetParam().is_upload; }
  // Returns the user action to be simulated in the test.
  CvcBannerResultTestCase::Action action() const { return GetParam().action; }
  // Returns the expected metric result for the user action.
  SaveCvcPromptResultIOS expected_result() const {
    return GetParam().expected_result;
  }
};

TEST_P(SaveCVCInfobarBannerOverlayMediatorResultTest, LogsResultMetric) {
  base::HistogramTester histogram_tester;
  InitInfobar(is_upload());
  switch (action()) {
    case CvcBannerResultTestCase::Action::kAccept:
      [mediator_ bannerInfobarButtonWasPressed:nil];
      break;
    case CvcBannerResultTestCase::Action::kSwipe:
      [mediator_ dismissInfobarBannerForUserInteraction:YES];
      break;
    case CvcBannerResultTestCase::Action::kTimeout:
      [mediator_ dismissInfobarBannerForUserInteraction:NO];
      break;
  }
  histogram_tester.ExpectUniqueSample(
      is_upload() ? kSaveCvcPromptResultHistogramStringForUploadSave
                  : kSaveCvcPromptResultHistogramStringForLocalSave,
      expected_result(), 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SaveCVCInfobarBannerOverlayMediatorResultTest,
    ::testing::ValuesIn<CvcBannerResultTestCase>({
        {"AcceptedForUploadSave", true,
         CvcBannerResultTestCase::Action::kAccept,
         SaveCvcPromptResultIOS::kAccepted},
        {"AcceptedForLocalSave", false,
         CvcBannerResultTestCase::Action::kAccept,
         SaveCvcPromptResultIOS::kAccepted},
        {"SwipedForUploadSave", true, CvcBannerResultTestCase::Action::kSwipe,
         SaveCvcPromptResultIOS::kSwiped},
        {"SwipedForLocalSave", false, CvcBannerResultTestCase::Action::kSwipe,
         SaveCvcPromptResultIOS::kSwiped},
        {"TimedOutForUploadSave", true,
         CvcBannerResultTestCase::Action::kTimeout,
         SaveCvcPromptResultIOS::kTimedOut},
        {"TimedOutForLocalSave", false,
         CvcBannerResultTestCase::Action::kTimeout,
         SaveCvcPromptResultIOS::kTimedOut},
    }),
    [](const ::testing::TestParamInfo<CvcBannerResultTestCase>& info) {
      return info.param.name;
    });
