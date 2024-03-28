// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/credit_card/autofill_save_card_infobar_delegate_ios.h"

#import "base/feature_list.h"
#import "components/autofill/ios/common/features.h"

namespace autofill {

AutofillSaveCardInfoBarDelegateIOS::AutofillSaveCardInfoBarDelegateIOS(
    AutofillSaveCardUiInfo ui_info,
    std::unique_ptr<AutofillSaveCardDelegate> common_delegate)
    : AutofillSaveCardInfoBarDelegateMobile(std::move(ui_info),
                                            std::move(common_delegate)) {}

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
    std::u16string expiration_date_year) {
  AutofillClient::UserProvidedCardDetails user_provided_details;
  user_provided_details.cardholder_name = cardholder_name;
  user_provided_details.expiration_date_month = expiration_date_month;
  user_provided_details.expiration_date_year = expiration_date_year;
  delegate()->OnUiUpdatedAndAccepted(user_provided_details);
  return true;
}

}  // namespace autofill
