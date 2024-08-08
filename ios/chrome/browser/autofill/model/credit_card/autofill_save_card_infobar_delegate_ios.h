// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_CREDIT_CARD_AUTOFILL_SAVE_CARD_INFOBAR_DELEGATE_IOS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_CREDIT_CARD_AUTOFILL_SAVE_CARD_INFOBAR_DELEGATE_IOS_H_

#import "base/functional/callback.h"
#import "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace autofill {

// Infobar delegate that prompts to save or update credit card information upon
// submission of credit card form.
class AutofillSaveCardInfoBarDelegateIOS
    : public AutofillSaveCardInfoBarDelegateMobile {
 public:
  AutofillSaveCardInfoBarDelegateIOS(
      AutofillSaveCardUiInfo ui_info,
      std::unique_ptr<AutofillSaveCardDelegate> common_delegate);

  AutofillSaveCardInfoBarDelegateIOS(
      const AutofillSaveCardInfoBarDelegateIOS&) = delete;
  AutofillSaveCardInfoBarDelegateIOS& operator=(
      const AutofillSaveCardInfoBarDelegateIOS&) = delete;
  ~AutofillSaveCardInfoBarDelegateIOS() override;

  // Returns `delegate` as an AutofillSaveCardInfoBarDelegateIOS, or
  // nullptr if it is of another type.
  static AutofillSaveCardInfoBarDelegateIOS* FromInfobarDelegate(
      infobars::InfoBarDelegate* delegate);

  // AutofillSaveCardInfoBarDelegateMobile overrides:
  bool ShouldExpire(const NavigationDetails& details) const override;

  // Updates and then saves the card using `cardholder_name`,
  // `expiration_date_month` and `expiration_date_year`, which were provided
  // as part of the iOS save card Infobar dialog.
  virtual bool UpdateAndAccept(std::u16string cardholder_name,
                               std::u16string expiration_date_month,
                               std::u16string expiration_date_year);

  // Runs `credit_card_upload_completion_callback_` with `card_saved` to show
  // credit card upload server result and adds
  // `on_confirmation_closed_callback` to run after dialog showing the result
  // is closed. If the infobar is already closed, runs
  // `on_confirmation_closed_callback` immediately.
  virtual void CreditCardUploadCompleted(
      bool card_saved,
      std::optional<autofill::payments::PaymentsAutofillClient::
                        OnConfirmationClosedCallback>
          on_confirmation_closed_callback);

  // Called after infobar, showing success confirmation is closed.
  // Runs `on_confirmation_closed_callback` if it holds a value.
  virtual void OnConfirmationClosed();

  // Returns whether credit card upload is complete.
  virtual bool IsCreditCardUploadComplete();

  // Sets `credit_card_upload_completion_callback_` that is executed when credit
  // card upload result is received.
  virtual void SetCreditCardUploadCompletionCallback(
      base::OnceCallback<void(bool card_saved)>
          credit_card_upload_completion_callback);

  // Informs the delegate when the Infobar view is presenting or is gone.
  virtual void SetInfobarIsPresenting(bool is_presenting);

 private:
  base::OnceCallback<void(bool card_saved)>
      credit_card_upload_completion_callback_;

  // `on_confirmation_closed_callback_` holds a value when card is successfully
  // uploaded and is eligible for virtual card enrollment.
  std::optional<payments::PaymentsAutofillClient::OnConfirmationClosedCallback>
      on_confirmation_closed_callback_;

  // Indicates whether credit card upload is complete.
  bool credit_card_upload_completed_;

  // True when infobar is presenting.
  bool infobar_is_presenting_ = false;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_CREDIT_CARD_AUTOFILL_SAVE_CARD_INFOBAR_DELEGATE_IOS_H_
