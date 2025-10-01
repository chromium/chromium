// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/credit_card/autofill_save_card_infobar_delegate_ios.h"

#import "base/feature_list.h"
#import "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/autofill/ios/browser/credit_card_save_metrics_ios.h"
#import "components/autofill/ios/common/features.h"
#import "components/infobars/core/infobar_manager.h"

namespace autofill {

AutofillSaveCardInfoBarDelegateIOS::AutofillSaveCardInfoBarDelegateIOS(
    AutofillSaveCardUiInfo ui_info,
    std::unique_ptr<AutofillSaveCardDelegate> common_delegate)
    : AutofillSaveCardInfoBarDelegateMobile(std::move(ui_info),
                                            std::move(common_delegate)) {
  const auto& options = delegate()->GetSaveCreditCardOptions();
  if (options.card_save_type ==
      payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly) {
    autofill_metrics::LogSaveCvcPromptOfferedIOS(delegate()->is_for_upload());
  } else {
    // `AutofillSaveCardInfoBarDelegateIOS` is created when the banner is to be
    // shown, so record the metric from here.
    LogPromptOfferMetric(
        autofill_metrics::SaveCardPromptOffer::kShown,
        autofill_metrics::SaveCreditCardPromptOverlayType::kBanner);

    LogSaveCreditCardInfoBarResultMetric(
        autofill_metrics::SaveCreditCardPromptResultIOS::kShown,
        autofill_metrics::SaveCreditCardPromptOverlayType::kBanner);
  }
}

AutofillSaveCardInfoBarDelegateIOS::~AutofillSaveCardInfoBarDelegateIOS() {
  // By convention, ensure all pending callbacks have been run.

  // Presence of `credit_card_upload_completion_callback_` indicates that
  // card upload has not completed. Thus
  // `credit_card_upload_completion_callback_` should be called with
  // `card_saved` as `NO` before destructing if still present.
  if (credit_card_upload_completion_callback_) {
    std::move(credit_card_upload_completion_callback_)
        .Run(/*card_saved=*/false);
  }

  // Presence of `on_confirmation_closed_callback_` indicates that the
  // action, that should have run on closing the save card confirmation,
  // is still pending. Thus `on_confirmation_closed_callback_` should be
  // called before destructing if still present.
  if (on_confirmation_closed_callback_) {
    (*std::exchange(on_confirmation_closed_callback_, std::nullopt)).Run();
  }
}

// static
AutofillSaveCardInfoBarDelegateIOS*
AutofillSaveCardInfoBarDelegateIOS::FromInfobarDelegate(
    infobars::InfoBarDelegate* delegate) {
  return delegate->GetIdentifier() == AUTOFILL_CC_INFOBAR_DELEGATE_MOBILE
             ? static_cast<AutofillSaveCardInfoBarDelegateIOS*>(delegate)
             : nullptr;
}

bool AutofillSaveCardInfoBarDelegateIOS::ShouldExpire(
    const NavigationDetails& details) const {
  if (base::FeatureList::IsEnabled(kAutofillStickyInfobarIos)) {
    return !details.is_form_submission && !details.is_redirect &&
           details.has_user_gesture &&
           ConfirmInfoBarDelegate::ShouldExpire(details);
  }
  return !details.is_form_submission && !details.is_redirect;
}

bool AutofillSaveCardInfoBarDelegateIOS::UpdateAndAccept(
    std::u16string cardholder_name,
    std::u16string expiration_date_month,
    std::u16string expiration_date_year,
    std::u16string cvc) {
  payments::PaymentsAutofillClient::UserProvidedCardDetails
      user_provided_details;
  user_provided_details.cardholder_name = std::move(cardholder_name);
  user_provided_details.expiration_date_month =
      std::move(expiration_date_month);
  user_provided_details.expiration_date_year = std::move(expiration_date_year);
  user_provided_details.cvc = std::move(cvc);
  delegate()->OnUiUpdatedAndAccepted(user_provided_details);
  return true;
}

void AutofillSaveCardInfoBarDelegateIOS::CreditCardUploadCompleted(
    bool card_saved,
    std::optional<autofill::payments::PaymentsAutofillClient::
                      OnConfirmationClosedCallback>
        on_confirmation_closed_callback) {
  credit_card_upload_completed_ = true;
  on_confirmation_closed_callback_ = std::move(on_confirmation_closed_callback);
  if (credit_card_upload_completion_callback_) {
    std::move(credit_card_upload_completion_callback_).Run(card_saved);
  }

  // `on_confirmation_closed_callback_` is executed when infobar showing
  // confirmation gets closed. When there is no infobar showing, this callback
  // should be executed immediately otherwise it will be pending till the
  // delegate gets destructed, which could be when user navigates to a different
  // web page.
  if (!infobar_is_presenting_ && on_confirmation_closed_callback_) {
    (*std::exchange(on_confirmation_closed_callback_, std::nullopt)).Run();
  }
}

void AutofillSaveCardInfoBarDelegateIOS::OnConfirmationClosed() {
  if (on_confirmation_closed_callback_) {
    (*std::exchange(on_confirmation_closed_callback_, std::nullopt)).Run();
  }
}

bool AutofillSaveCardInfoBarDelegateIOS::IsCreditCardUploadComplete() {
  return credit_card_upload_completed_;
}

void AutofillSaveCardInfoBarDelegateIOS::SetCreditCardUploadCompletionCallback(
    base::OnceCallback<void(bool card_saved)> callback) {
  if (callback.is_null()) {
    credit_card_upload_completion_callback_.Reset();
    return;
  }
  CHECK(credit_card_upload_completion_callback_.is_null());
  credit_card_upload_completion_callback_ = std::move(callback);
}

void AutofillSaveCardInfoBarDelegateIOS::SetInfobarIsPresenting(
    bool is_presenting) {
  infobar_is_presenting_ = is_presenting;
}

void AutofillSaveCardInfoBarDelegateIOS::LogSaveCreditCardInfoBarResultMetric(
    autofill_metrics::SaveCreditCardPromptResultIOS metric,
    autofill_metrics::SaveCreditCardPromptOverlayType overlay_type) {
  autofill_metrics::LogSaveCreditCardPromptResultIOS(
      metric, delegate()->is_for_upload(),
      delegate()->GetSaveCreditCardOptions(), overlay_type);
}

void AutofillSaveCardInfoBarDelegateIOS::LogSaveCvcInfoBarResultMetric(
    autofill_metrics::SaveCvcPromptResultIOS metric) {
  autofill_metrics::LogSaveCvcPromptResultIOS(
      metric, delegate()->is_for_upload(),
      delegate()->GetSaveCreditCardOptions());
}

void AutofillSaveCardInfoBarDelegateIOS::LogPromptOfferMetric(
    autofill_metrics::SaveCardPromptOffer metric,
    autofill_metrics::SaveCreditCardPromptOverlayType overlay_type) {
  autofill_metrics::LogSaveCreditCardPromptOfferMetricIos(
      metric, delegate()->is_for_upload(),
      delegate()->GetSaveCreditCardOptions(), overlay_type);
}

}  // namespace autofill
