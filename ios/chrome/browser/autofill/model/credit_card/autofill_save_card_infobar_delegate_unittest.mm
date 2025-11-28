// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>
#import <string>
#import <variant>

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/string_number_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/payments/autofill_save_card_delegate.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/autofill/ios/common/features.h"
#import "ios/chrome/browser/autofill/model/credit_card/autofill_save_card_infobar_delegate_ios.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace autofill {

namespace {

using SaveCardPromptOffer = autofill::autofill_metrics::SaveCardPromptOffer;
using SaveCreditCardPromptResultIOS =
    autofill_metrics::SaveCreditCardPromptResultIOS;
using SaveCreditCardPromptOverlayType =
    autofill_metrics::SaveCreditCardPromptOverlayType;
using SaveCreditCardOptions =
    payments::PaymentsAutofillClient::SaveCreditCardOptions;

constexpr int kNavEntryId = 10;
constexpr std::string_view kSaveCreditCardPromptOfferBaseHistogram =
    "Autofill.SaveCreditCardPromptOffer.IOS";
const std::string kSaveCreditCardPromptResultIOSPrefix =
    "Autofill.SaveCreditCardPromptResult.IOS";
}  // namespace

class AutofillSaveCardInfoBarDelegateTest : public PlatformTest {
 public:
  void LocalSaveCardPromptCallbackFn(
      payments::PaymentsAutofillClient::SaveCardOfferUserDecision
          user_decision) {
    last_user_decision_ = user_decision;
  }

  void UploadSaveCardPromptCallbackFn(
      payments::PaymentsAutofillClient::SaveCardOfferUserDecision user_decision,
      const payments::PaymentsAutofillClient::UserProvidedCardDetails&
          user_provided_card_details) {
    last_user_decision_ = user_decision;
    last_user_provided_card_details_ = user_provided_card_details;
  }

  void CreditCardUploadCompletionCallbackFn(bool card_saved) {
    card_saved_ = card_saved;
  }

  void OnConfirmationClosedCallbackFn() {
    ran_on_confirmation_closed_callback_ = true;
  }

 protected:
  ~AutofillSaveCardInfoBarDelegateTest() override = default;

  void SetUp() override {
    delegate_ = CreateDelegate(
        /*callback=*/static_cast<
            payments::PaymentsAutofillClient::LocalSaveCardPromptCallback>(
            base::DoNothing()));
  }

  std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> CreateDelegate(
      std::variant<
          payments::PaymentsAutofillClient::LocalSaveCardPromptCallback,
          payments::PaymentsAutofillClient::UploadSaveCardPromptCallback>
          save_card_callback,
      SaveCreditCardOptions options = {}) {
    auto save_card_delegate = std::make_unique<AutofillSaveCardDelegate>(
        std::move(save_card_callback), options);
    return std::make_unique<AutofillSaveCardInfoBarDelegateIOS>(
        AutofillSaveCardUiInfo(), std::move(save_card_delegate));
  }

  base::test::ScopedFeatureList feature_list_;
  infobars::InfoBarDelegate::NavigationDetails nav_details_that_expire_{
      .entry_id = kNavEntryId,
      .is_navigation_to_different_page = true,
      .did_replace_entry = false,
      .is_reload = true,
      .is_redirect = false,
      .is_form_submission = false,
      .has_user_gesture = true};
  std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> delegate_;
  std::optional<payments::PaymentsAutofillClient::SaveCardOfferUserDecision>
      last_user_decision_;
  std::optional<payments::PaymentsAutofillClient::UserProvidedCardDetails>
      last_user_provided_card_details_;
  std::optional<bool> card_saved_;
  bool ran_on_confirmation_closed_callback_ = false;
};

// Tests that the user decision is propagated when accepting local save.
TEST_F(AutofillSaveCardInfoBarDelegateTest, UpdateAndAccept_Local) {
  payments::PaymentsAutofillClient::LocalSaveCardPromptCallback callback =
      base::BindOnce(
          &AutofillSaveCardInfoBarDelegateTest::LocalSaveCardPromptCallbackFn,
          base::Unretained(this));
  std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> delegate =
      CreateDelegate(std::move(callback));

  EXPECT_TRUE(delegate->UpdateAndAccept(
      /*cardholder_name=*/u"Test User",
      /*expiration_date_month=*/u"04",
      /*expiration_date_year=*/u"24",
      /*cvc=*/u""));

  ASSERT_TRUE(last_user_decision_);
  EXPECT_EQ(
      payments::PaymentsAutofillClient::SaveCardOfferUserDecision::kAccepted,
      last_user_decision_);
}

// Tests that the user decision is propagated when accepting upload.
TEST_F(AutofillSaveCardInfoBarDelegateTest, UpdateAndAccept_Upload) {
  payments::PaymentsAutofillClient::UploadSaveCardPromptCallback callback =
      base::BindOnce(
          &AutofillSaveCardInfoBarDelegateTest::UploadSaveCardPromptCallbackFn,
          base::Unretained(this));
  std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> delegate =
      CreateDelegate(std::move(callback));

  const std::u16string cardholder_name = u"Test User";
  const std::u16string expiration_date_month = u"04";
  const std::u16string expiration_date_year = u"24";
  const std::u16string card_cvc = u"123";

  EXPECT_TRUE(delegate->UpdateAndAccept(
      /*cardholder_name=*/cardholder_name,
      /*expiration_date_month=*/expiration_date_month,
      /*expiration_date_year=*/expiration_date_year,
      /*cvc=*/card_cvc));

  ASSERT_TRUE(last_user_decision_ && last_user_provided_card_details_);
  EXPECT_EQ(
      payments::PaymentsAutofillClient::SaveCardOfferUserDecision::kAccepted,
      last_user_decision_);
  EXPECT_THAT(
      *last_user_provided_card_details_,
      ::testing::FieldsAre(/*cardholder_name=*/cardholder_name,
                           /*expiration_date_month=*/expiration_date_month,
                           /*expiration_date_year=*/expiration_date_year,
                           /*cvc=*/card_cvc));
}

// Tests that CreditCardUploadCompleted() runs
// credit_card_upload_completion_callback with card successfully saved.
TEST_F(AutofillSaveCardInfoBarDelegateTest,
       CreditCardUploadCompleted_CardSaved) {
  payments::PaymentsAutofillClient::UploadSaveCardPromptCallback callback =
      base::BindOnce(
          &AutofillSaveCardInfoBarDelegateTest::UploadSaveCardPromptCallbackFn,
          base::Unretained(this));
  std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> delegate =
      CreateDelegate(std::move(callback));

  base::OnceCallback<void(bool card_saved)>
      credit_card_upload_completion_callback =
          base::BindOnce(&AutofillSaveCardInfoBarDelegateTest::
                             CreditCardUploadCompletionCallbackFn,
                         base::Unretained(this));

  delegate->SetCreditCardUploadCompletionCallback(
      std::move(credit_card_upload_completion_callback));

  delegate->CreditCardUploadCompleted(
      /*card_saved=*/true, /*on_confirmation_closed_callback=*/std::nullopt);
  EXPECT_TRUE(card_saved_.value());
}

// Tests that CreditCardUploadCompleted() runs
// credit_card_upload_completion_callback with card not successfully saved.
TEST_F(AutofillSaveCardInfoBarDelegateTest,
       CreditCardUploadCompleted_CardNotSaved) {
  payments::PaymentsAutofillClient::UploadSaveCardPromptCallback callback =
      base::BindOnce(
          &AutofillSaveCardInfoBarDelegateTest::UploadSaveCardPromptCallbackFn,
          base::Unretained(this));
  std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> delegate =
      CreateDelegate(std::move(callback));

  base::OnceCallback<void(bool card_saved)>
      credit_card_upload_completion_callback =
          base::BindOnce(&AutofillSaveCardInfoBarDelegateTest::
                             CreditCardUploadCompletionCallbackFn,
                         base::Unretained(this));

  delegate->SetCreditCardUploadCompletionCallback(
      std::move(credit_card_upload_completion_callback));

  delegate->CreditCardUploadCompleted(
      /*card_saved=*/false, /*on_confirmation_closed_callback=*/std::nullopt);
  EXPECT_FALSE(card_saved_.value());
}

// Tests that CreditCardUploadCompleted() runs
// `on_confirmation_closed_callback_` when infobar is not presenting.
TEST_F(AutofillSaveCardInfoBarDelegateTest,
       CreditCardUploadCompleted_InfobarNotPresenting) {
  payments::PaymentsAutofillClient::UploadSaveCardPromptCallback callback =
      base::BindOnce(
          &AutofillSaveCardInfoBarDelegateTest::UploadSaveCardPromptCallbackFn,
          base::Unretained(this));
  std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> delegate =
      CreateDelegate(std::move(callback));

  delegate->SetInfobarIsPresenting(false);

  delegate->CreditCardUploadCompleted(
      /*card_saved=*/true, /*on_confirmation_closed_callback=*/base::BindOnce(
          &AutofillSaveCardInfoBarDelegateTest::OnConfirmationClosedCallbackFn,
          base::Unretained(this)));

  EXPECT_FALSE(card_saved_.has_value());
  EXPECT_TRUE(ran_on_confirmation_closed_callback_);
}

// Tests that `OnConfirmationClosed()` runs
// `on_confirmation_closed_callback_` when it holds a value.
TEST_F(AutofillSaveCardInfoBarDelegateTest, OnConfirmationClosedCallbackSet) {
  std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> delegate = CreateDelegate(
      static_cast<
          payments::PaymentsAutofillClient::UploadSaveCardPromptCallback>(
          base::DoNothing()));

  delegate->CreditCardUploadCompleted(
      /*card_saved=*/true, /*on_confirmation_closed_callback=*/base::BindOnce(
          &AutofillSaveCardInfoBarDelegateTest::OnConfirmationClosedCallbackFn,
          base::Unretained(this)));

  delegate->OnConfirmationClosed();
  EXPECT_TRUE(ran_on_confirmation_closed_callback_);
}

// Tests that `OnConfirmationClosed()` doesn't crash when
// `on_confirmation_closed_callback_` doesn't hold a value.
TEST_F(AutofillSaveCardInfoBarDelegateTest,
       OnConfirmationClosedCallbackNotSet) {
  std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> delegate = CreateDelegate(
      static_cast<
          payments::PaymentsAutofillClient::UploadSaveCardPromptCallback>(
          base::DoNothing()));

  // `on_confirmation_closed_callback_` doesn't hold a value when
  // card upload is unsuccessful.
  delegate->CreditCardUploadCompleted(
      /*card_saved=*/false, /*on_confirmation_closed_callback=*/std::nullopt);

  delegate->OnConfirmationClosed();
  EXPECT_FALSE(ran_on_confirmation_closed_callback_);
}

// Tests that the infobar expires when reloading the page.
TEST_F(AutofillSaveCardInfoBarDelegateTest, ShouldExpire_True_WhenReload) {
  nav_details_that_expire_.is_reload = true;
  nav_details_that_expire_.entry_id = kNavEntryId;
  delegate_->set_nav_entry_id(kNavEntryId);
  EXPECT_TRUE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that the infobar expires when new navigation ID.
TEST_F(AutofillSaveCardInfoBarDelegateTest,
       ShouldExpire_True_WhenDifferentNavEntryId) {
  nav_details_that_expire_.is_reload = false;
  nav_details_that_expire_.entry_id = kNavEntryId;
  const int different_nav_id = kNavEntryId - 1;
  delegate_->set_nav_entry_id(different_nav_id);

  EXPECT_TRUE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that when the sticky infobar is disabled, having a user gesture isn't
// used as a condition to expire the infobar, hence setting the user gesture bit
// to false shouldn't change the returned value.
TEST_F(AutofillSaveCardInfoBarDelegateTest,
       ShouldExpire_True_WhenNoStickyInfobarAndNoUserGesture) {
  feature_list_.InitAndDisableFeature(kAutofillStickyInfobarIos);
  nav_details_that_expire_.has_user_gesture = false;

  EXPECT_TRUE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that when the sticky infobar is enabled, having a user gesture is
// used as a condition to expire the infobar, hence setting the user gesture bit
// to true should return true.
TEST_F(AutofillSaveCardInfoBarDelegateTest,
       ShouldExpire_True_WhenStickyInfobarAndUserGesture) {
  nav_details_that_expire_.has_user_gesture = true;
  EXPECT_TRUE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that the infobar doesn't expire when the page is the same.
TEST_F(AutofillSaveCardInfoBarDelegateTest,
       ShouldExpire_False_WhenNoDifferentPage) {
  nav_details_that_expire_.is_navigation_to_different_page = false;
  EXPECT_FALSE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that the infobar doesn't expire when the page is the same.
TEST_F(AutofillSaveCardInfoBarDelegateTest,
       ShouldExpire_False_WhenDidReplaceEntry) {
  nav_details_that_expire_.did_replace_entry = true;
  EXPECT_FALSE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that the infobar doesn't expire when redirect.
TEST_F(AutofillSaveCardInfoBarDelegateTest, ShouldExpire_False_WhenRedirect) {
  nav_details_that_expire_.is_redirect = true;
  EXPECT_FALSE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that the infobar doesn't expire when form submission.
TEST_F(AutofillSaveCardInfoBarDelegateTest,
       ShouldExpire_False_WhenFormSubmission) {
  nav_details_that_expire_.is_form_submission = true;
  EXPECT_FALSE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that the infobar expires when no reload and the navigation entry ID
// didn't change.
TEST_F(AutofillSaveCardInfoBarDelegateTest,
       ShouldExpire_False_WhenNoReloadAndSameNavEntryId) {
  nav_details_that_expire_.is_reload = false;
  nav_details_that_expire_.entry_id = kNavEntryId;
  delegate_->set_nav_entry_id(kNavEntryId);
  EXPECT_FALSE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that when the sticky infobar is enabled, having a user gesture is
// used as a condition to expire the infobar, hence setting the user gesture bit
// to false should return false.
TEST_F(AutofillSaveCardInfoBarDelegateTest,
       ShouldExpire_False_WhenStickyInfobar) {
  nav_details_that_expire_.has_user_gesture = false;
  EXPECT_FALSE(delegate_->ShouldExpire(nav_details_that_expire_));
}

TEST_F(AutofillSaveCardInfoBarDelegateTest, LogPromptOfferMetric_ForLocalSave) {
  payments::PaymentsAutofillClient::LocalSaveCardPromptCallback
      save_card_callback = base::BindOnce(
          &AutofillSaveCardInfoBarDelegateTest::LocalSaveCardPromptCallbackFn,
          base::Unretained(this));
  std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> delegate =
      CreateDelegate(std::move(save_card_callback),
                     SaveCreditCardOptions().with_num_strikes(0));

  base::HistogramTester histogram_tester;
  delegate->LogPromptOfferMetric(SaveCardPromptOffer::kShown,
                                 SaveCreditCardPromptOverlayType::kBanner);

  histogram_tester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram, ".Local.Banner"}),
      SaveCardPromptOffer::kShown, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram,
                    ".Local.Banner.NumStrikes.0.NoFixFlow"}),
      SaveCardPromptOffer::kShown, 1);
}

TEST_F(AutofillSaveCardInfoBarDelegateTest,
       LogPromptOfferMetric_ForServerSave) {
  payments::PaymentsAutofillClient::UploadSaveCardPromptCallback
      save_card_callback = base::BindOnce(
          &AutofillSaveCardInfoBarDelegateTest::UploadSaveCardPromptCallbackFn,
          base::Unretained(this));
  std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> delegate =
      CreateDelegate(std::move(save_card_callback),
                     SaveCreditCardOptions().with_num_strikes(0));

  base::HistogramTester histogram_tester;
  delegate->LogPromptOfferMetric(SaveCardPromptOffer::kShown,
                                 SaveCreditCardPromptOverlayType::kBanner);

  histogram_tester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram, ".Server.Banner"}),
      SaveCardPromptOffer::kShown, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram,
                    ".Server.Banner.NumStrikes.0.NoFixFlow"}),
      SaveCardPromptOffer::kShown, 1);
}

TEST_F(AutofillSaveCardInfoBarDelegateTest,
       LogSaveCreditCardInfoBarResultMetric_WithLocalSave) {
  payments::PaymentsAutofillClient::LocalSaveCardPromptCallback
      save_card_callback = base::BindOnce(
          &AutofillSaveCardInfoBarDelegateTest::LocalSaveCardPromptCallbackFn,
          base::Unretained(this));
  std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> delegate =
      CreateDelegate(std::move(save_card_callback),
                     SaveCreditCardOptions().with_num_strikes(0));

  base::HistogramTester histogram_tester;
  delegate->LogSaveCreditCardInfoBarResultMetric(
      SaveCreditCardPromptResultIOS::kShown,
      SaveCreditCardPromptOverlayType::kBanner);

  histogram_tester.ExpectUniqueSample(
      base::StrCat({kSaveCreditCardPromptResultIOSPrefix,
                    ".Local.Banner.NumStrikes.0.NoFixFlow"}),
      SaveCreditCardPromptResultIOS::kShown, 1);
}

TEST_F(AutofillSaveCardInfoBarDelegateTest,
       LogSaveCreditCardInfoBarResultMetric_WithServerSave) {
  payments::PaymentsAutofillClient::UploadSaveCardPromptCallback
      save_card_callback = base::BindOnce(
          &AutofillSaveCardInfoBarDelegateTest::UploadSaveCardPromptCallbackFn,
          base::Unretained(this));
  std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> delegate =
      CreateDelegate(std::move(save_card_callback),
                     SaveCreditCardOptions().with_num_strikes(0));

  base::HistogramTester histogram_tester;
  delegate->LogSaveCreditCardInfoBarResultMetric(
      SaveCreditCardPromptResultIOS::kShown,
      SaveCreditCardPromptOverlayType::kBanner);

  histogram_tester.ExpectUniqueSample(
      base::StrCat({kSaveCreditCardPromptResultIOSPrefix,
                    ".Server.Banner.NumStrikes.0.NoFixFlow"}),
      SaveCreditCardPromptResultIOS::kShown, 1);
}

class AutofillSaveCardInfoBarDelegateMetricsTest
    : public AutofillSaveCardInfoBarDelegateTest,
      public testing::WithParamInterface<
          std::tuple<autofill_metrics::SaveCreditCardPromptResultIOS,
                     autofill_metrics::SaveCreditCardPromptOverlayType>> {
 protected:
  SaveCreditCardPromptResultIOS Metric() const {
    return std::get<0>(GetParam());
  }
  SaveCreditCardPromptOverlayType OverlayType() const {
    return std::get<1>(GetParam());
  }
  std::string_view SaveCreditCardPromptOverlayTypeToMetricSuffix(
      SaveCreditCardPromptOverlayType type) {
    return type == SaveCreditCardPromptOverlayType::kBanner ? ".Banner"
                                                            : ".Modal";
  }
};

TEST_P(AutofillSaveCardInfoBarDelegateMetricsTest,
       LogSaveCreditCardOfferInfoBarResultMetric) {
  payments::PaymentsAutofillClient::UploadSaveCardPromptCallback
      save_card_callback = base::BindOnce(
          &AutofillSaveCardInfoBarDelegateTest::UploadSaveCardPromptCallbackFn,
          base::Unretained(this));
  std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> delegate =
      CreateDelegate(std::move(save_card_callback),
                     SaveCreditCardOptions().with_num_strikes(0));

  base::HistogramTester histogram_tester;
  delegate->LogSaveCreditCardInfoBarResultMetric(Metric(), OverlayType());

  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {kSaveCreditCardPromptResultIOSPrefix, ".Server",
           SaveCreditCardPromptOverlayTypeToMetricSuffix(OverlayType()),
           ".NumStrikes.0.NoFixFlow"}),
      Metric(), 1);
}

INSTANTIATE_TEST_SUITE_P(
    AutofillSaveCardInfoBarDelegateTest,
    AutofillSaveCardInfoBarDelegateMetricsTest,
    testing::Values(std::make_tuple(SaveCreditCardPromptResultIOS::kShown,
                                    SaveCreditCardPromptOverlayType::kBanner),
                    std::make_tuple(SaveCreditCardPromptResultIOS::kAccepted,
                                    SaveCreditCardPromptOverlayType::kBanner),
                    std::make_tuple(SaveCreditCardPromptResultIOS::kSwiped,
                                    SaveCreditCardPromptOverlayType::kBanner),
                    std::make_tuple(SaveCreditCardPromptResultIOS::KTimedOut,
                                    SaveCreditCardPromptOverlayType::kBanner),
                    std::make_tuple(SaveCreditCardPromptResultIOS::kShown,
                                    SaveCreditCardPromptOverlayType::kModal),
                    std::make_tuple(SaveCreditCardPromptResultIOS::kAccepted,
                                    SaveCreditCardPromptOverlayType::kModal),
                    std::make_tuple(SaveCreditCardPromptResultIOS::kLinkClicked,
                                    SaveCreditCardPromptOverlayType::kModal),
                    std::make_tuple(SaveCreditCardPromptResultIOS::kDenied,
                                    SaveCreditCardPromptOverlayType::kModal),
                    std::make_tuple(SaveCreditCardPromptResultIOS::kIgnored,
                                    SaveCreditCardPromptOverlayType::kModal)));

class AutofillSaveCardInfoBarDelegateMetricsTestWithFixFlow
    : public AutofillSaveCardInfoBarDelegateTest,
      public testing::WithParamInterface<
          std::tuple</*request_cardholder_name*/ bool,
                     /*request_expiry_date*/ bool>> {
 protected:
  bool RequestingCardHolderName() const { return std::get<0>(GetParam()); }
  bool RequestingExpiryDate() const { return std::get<1>(GetParam()); }

  std::string_view SaveCreditCardPromptFixFlowSuffix(
      bool request_cardholder_name,
      bool request_expiration_date) {
    if (request_cardholder_name && request_expiration_date) {
      return ".RequestingCardHolderNameAndExpiryDate";
    } else if (request_cardholder_name) {
      return ".RequestingCardHolderName";
    } else if (request_expiration_date) {
      return ".RequestingExpiryDate";
    }
    return ".NoFixFlow";
  }
};

TEST_P(AutofillSaveCardInfoBarDelegateMetricsTestWithFixFlow,
       LogPromptOfferMetric) {
  payments::PaymentsAutofillClient::UploadSaveCardPromptCallback
      save_card_callback = base::BindOnce(
          &AutofillSaveCardInfoBarDelegateTest::UploadSaveCardPromptCallbackFn,
          base::Unretained(this));
  std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> delegate = CreateDelegate(
      std::move(save_card_callback),
      SaveCreditCardOptions()
          .with_num_strikes(0)
          .with_should_request_name_from_user(RequestingCardHolderName())
          .with_should_request_expiration_date_from_user(
              RequestingExpiryDate()));

  base::HistogramTester histogram_tester;
  delegate->LogPromptOfferMetric(SaveCardPromptOffer::kShown,
                                 SaveCreditCardPromptOverlayType::kModal);

  histogram_tester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram, ".Server.Modal"}),
      SaveCardPromptOffer::kShown, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram,
                    ".Server.Modal.NumStrikes.0",
                    SaveCreditCardPromptFixFlowSuffix(
                        RequestingCardHolderName(), RequestingExpiryDate())}),
      SaveCardPromptOffer::kShown, 1);
}

TEST_P(AutofillSaveCardInfoBarDelegateMetricsTestWithFixFlow,
       LogSaveCreditCardInfoBarResultMetric) {
  payments::PaymentsAutofillClient::UploadSaveCardPromptCallback
      save_card_callback = base::BindOnce(
          &AutofillSaveCardInfoBarDelegateTest::UploadSaveCardPromptCallbackFn,
          base::Unretained(this));
  std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> delegate = CreateDelegate(
      std::move(save_card_callback),
      SaveCreditCardOptions()
          .with_num_strikes(0)
          .with_should_request_name_from_user(RequestingCardHolderName())
          .with_should_request_expiration_date_from_user(
              RequestingExpiryDate()));

  base::HistogramTester histogram_tester;
  delegate->LogSaveCreditCardInfoBarResultMetric(
      SaveCreditCardPromptResultIOS::kShown,
      SaveCreditCardPromptOverlayType::kModal);

  histogram_tester.ExpectUniqueSample(
      base::StrCat({kSaveCreditCardPromptResultIOSPrefix,
                    ".Server.Modal.NumStrikes.0",
                    SaveCreditCardPromptFixFlowSuffix(
                        RequestingCardHolderName(), RequestingExpiryDate())}),
      SaveCreditCardPromptResultIOS::kShown, 1);
}

INSTANTIATE_TEST_SUITE_P(
    AutofillSaveCardInfoBarDelegateTest,
    AutofillSaveCardInfoBarDelegateMetricsTestWithFixFlow,
    testing::Combine(/*request_cardholder_name*/ testing::Bool(),
                     /*request_expiration_date*/ testing::Bool()));

class AutofillSaveCardInfoBarDelegateMetricsTestWithNumStrikes
    : public AutofillSaveCardInfoBarDelegateTest,
      public testing::WithParamInterface</*strike_count*/ int> {};

TEST_P(AutofillSaveCardInfoBarDelegateMetricsTestWithNumStrikes,
       LogPromptOfferMetric) {
  payments::PaymentsAutofillClient::UploadSaveCardPromptCallback
      save_card_callback = base::BindOnce(
          &AutofillSaveCardInfoBarDelegateTest::UploadSaveCardPromptCallbackFn,
          base::Unretained(this));
  std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> delegate =
      CreateDelegate(std::move(save_card_callback),
                     SaveCreditCardOptions().with_num_strikes(GetParam()));

  base::HistogramTester histogram_tester;
  delegate->LogPromptOfferMetric(SaveCardPromptOffer::kShown,
                                 SaveCreditCardPromptOverlayType::kModal);

  histogram_tester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram, ".Server.Modal"}),
      SaveCardPromptOffer::kShown, 1);

  // Strike counts that are out of the range [0, 2] should be ignored.
  int expected_samples = GetParam() > 2 ? 0 : 1;

  histogram_tester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram,
                    ".Server.Modal.NumStrikes.",
                    base::NumberToString(GetParam()), ".NoFixFlow"}),
      SaveCardPromptOffer::kShown, expected_samples);
}

TEST_P(AutofillSaveCardInfoBarDelegateMetricsTestWithNumStrikes,
       LogSaveCreditCardInfoBarResultMetric) {
  payments::PaymentsAutofillClient::UploadSaveCardPromptCallback
      save_card_callback = base::BindOnce(
          &AutofillSaveCardInfoBarDelegateTest::UploadSaveCardPromptCallbackFn,
          base::Unretained(this));
  std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> delegate =
      CreateDelegate(std::move(save_card_callback),
                     SaveCreditCardOptions().with_num_strikes(GetParam()));

  base::HistogramTester histogram_tester;
  delegate->LogSaveCreditCardInfoBarResultMetric(
      SaveCreditCardPromptResultIOS::kShown,
      SaveCreditCardPromptOverlayType::kModal);

  // `LogSaveCreditCardInfoBarResultMetric_WithNumStrikes` should ignore strike
  // counts that are out of the range [0, 2].
  int expected_samples = GetParam() > 2 ? 0 : 1;

  histogram_tester.ExpectUniqueSample(
      base::StrCat({kSaveCreditCardPromptResultIOSPrefix,
                    ".Server.Modal.NumStrikes.",
                    base::NumberToString(GetParam()), ".NoFixFlow"}),
      SaveCreditCardPromptResultIOS::kShown, expected_samples);
}

INSTANTIATE_TEST_SUITE_P(
    AutofillSaveCardInfoBarDelegateTest,
    AutofillSaveCardInfoBarDelegateMetricsTestWithNumStrikes,
    /*strike_count*/ testing::Values(0, 1, 2, 3));

class AutofillSaveCardInfoBarDelegateMetricsTestWithCardSaveType
    : public AutofillSaveCardInfoBarDelegateTest,
      public testing::WithParamInterface<
          payments::PaymentsAutofillClient::CardSaveType> {
 protected:
  std::string_view CardSaveTypeToMetricSuffix(
      payments::PaymentsAutofillClient::CardSaveType save_type) {
    switch (save_type) {
      case payments::PaymentsAutofillClient::CardSaveType::kCardSaveWithCvc:
      case payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly:
        return ".SavingWithCvc";
      case payments::PaymentsAutofillClient::CardSaveType::kCardSaveOnly:
        return "";
    }
  }
};

TEST_P(AutofillSaveCardInfoBarDelegateMetricsTestWithCardSaveType,
       LogPromptOfferMetric) {
  base::HistogramTester histogram_tester;
  payments::PaymentsAutofillClient::UploadSaveCardPromptCallback
      save_card_callback = base::DoNothing();

  payments::PaymentsAutofillClient::CardSaveType save_type = GetParam();

  std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> delegate = CreateDelegate(
      std::move(save_card_callback),
      SaveCreditCardOptions().with_num_strikes(0).with_card_save_type(
          save_type));

  histogram_tester.ExpectUniqueSample(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram, ".Server.Banner",
                    CardSaveTypeToMetricSuffix(save_type)}),
      SaveCardPromptOffer::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram,
                    ".Server.Banner.NumStrikes.0.NoFixFlow",
                    CardSaveTypeToMetricSuffix(save_type)}),
      SaveCardPromptOffer::kShown, 1);
}

TEST_P(AutofillSaveCardInfoBarDelegateMetricsTestWithCardSaveType,
       LogSaveCreditCardInfoBarResultMetric) {
  payments::PaymentsAutofillClient::UploadSaveCardPromptCallback
      save_card_callback = base::BindOnce(
          &AutofillSaveCardInfoBarDelegateTest::UploadSaveCardPromptCallbackFn,
          base::Unretained(this));

  payments::PaymentsAutofillClient::CardSaveType save_type = GetParam();

  std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> delegate = CreateDelegate(
      std::move(save_card_callback),
      SaveCreditCardOptions().with_num_strikes(0).with_card_save_type(
          save_type));

  base::HistogramTester histogram_tester;
  delegate->LogSaveCreditCardInfoBarResultMetric(
      SaveCreditCardPromptResultIOS::kShown,
      SaveCreditCardPromptOverlayType::kBanner);

  histogram_tester.ExpectUniqueSample(
      base::StrCat({kSaveCreditCardPromptResultIOSPrefix,
                    ".Server.Banner.NumStrikes.0.NoFixFlow",
                    CardSaveTypeToMetricSuffix(save_type)}),
      SaveCreditCardPromptResultIOS::kShown, 1);
}

INSTANTIATE_TEST_SUITE_P(
    AutofillSaveCardInfoBarDelegateTest,
    AutofillSaveCardInfoBarDelegateMetricsTestWithCardSaveType,
    testing::Values(
        payments::PaymentsAutofillClient::CardSaveType::kCardSaveOnly,
        payments::PaymentsAutofillClient::CardSaveType::kCardSaveWithCvc));

}  // namespace autofill
