// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_CREDIT_CARD_AUTOFILL_SAVE_CARD_INFOBAR_DELEGATE_IOS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_CREDIT_CARD_AUTOFILL_SAVE_CARD_INFOBAR_DELEGATE_IOS_H_

#import "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"

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
  ~AutofillSaveCardInfoBarDelegateIOS() override = default;

  // AutofillSaveCardInfoBarDelegateMobile overrides:
  bool ShouldExpire(const NavigationDetails& details) const override;

  // Updates and then saves the card using `cardholder_name`,
  // `expiration_date_month` and `expiration_date_year`, which were provided
  // as part of the iOS save card Infobar dialog.
  virtual bool UpdateAndAccept(std::u16string cardholder_name,
                               std::u16string expiration_date_month,
                               std::u16string expiration_date_year);
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_CREDIT_CARD_AUTOFILL_SAVE_CARD_INFOBAR_DELEGATE_IOS_H_
