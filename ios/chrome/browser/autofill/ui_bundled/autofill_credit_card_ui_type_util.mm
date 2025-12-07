// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/autofill_credit_card_ui_type_util.h"

#import "base/notreached.h"

autofill::FieldType AutofillTypeFromAutofillUITypeForCard(
    AutofillCreditCardUIType type) {
  switch (type) {
    case AutofillCreditCardUIType::kUnknown:
      return autofill::UNKNOWN_TYPE;
    case AutofillCreditCardUIType::kNumber:
      return autofill::CREDIT_CARD_NUMBER;
    case AutofillCreditCardUIType::kFullName:
      return autofill::CREDIT_CARD_NAME_FULL;
    case AutofillCreditCardUIType::kExpMonth:
      return autofill::CREDIT_CARD_EXP_MONTH;
    case AutofillCreditCardUIType::kExpYear:
      return autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR;
    case AutofillCreditCardUIType::kSecurityCode:
      return autofill::CREDIT_CARD_VERIFICATION_CODE;
    case AutofillCreditCardUIType::kExpDate:
    case AutofillCreditCardUIType::kBillingAddress:
    case AutofillCreditCardUIType::kSaveToChrome:
    default:
      NOTREACHED();
  }
}
