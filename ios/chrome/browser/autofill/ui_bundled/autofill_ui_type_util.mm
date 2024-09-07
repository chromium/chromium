// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/autofill_ui_type_util.h"

#import "base/notreached.h"
#import "components/autofill/core/common/autofill_features.h"

AutofillCreditCardUIType AutofillUITypeFromAutofillTypeForCard(
    autofill::FieldType type) {
  switch (type) {
    case autofill::UNKNOWN_TYPE:
      return AutofillCreditCardUIType::kUnknown;
    case autofill::CREDIT_CARD_NUMBER:
      return AutofillCreditCardUIType::kNumber;
    case autofill::CREDIT_CARD_NAME_FULL:
      return AutofillCreditCardUIType::kFullName;
    case autofill::CREDIT_CARD_EXP_MONTH:
      return AutofillCreditCardUIType::kExpMonth;
    case autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return AutofillCreditCardUIType::kExpYear;
    default:
      NOTREACHED_IN_MIGRATION();
      return AutofillCreditCardUIType::kUnknown;
  }
}

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
    case AutofillCreditCardUIType::kExpDate:
    case AutofillCreditCardUIType::kBillingAddress:
    case AutofillCreditCardUIType::kSaveToChrome:
    default:
      NOTREACHED_IN_MIGRATION();
      return autofill::UNKNOWN_TYPE;
  }
}

bool FieldIsUsedInAddress(autofill::FieldType autofillType,
                          NSString* countryCode) {
  // TODO(crbug.com/40281788): Replace all this with libaddressinput.

  if (autofillType == autofill::ADDRESS_HOME_DEPENDENT_LOCALITY) {
    // List of countries which require the dependent locality field.
    NSArray<NSString*>* countryCodes = @[
      @"BR", @"CN", @"CO", @"IE", @"IR", @"KR", @"MX", @"MY", @"NG", @"NZ",
      @"PH", @"PK", @"TH", @"ZA"
    ];

    return ([countryCodes indexOfObject:countryCode] != NSNotFound);
  }

  if (autofillType == autofill::ADDRESS_HOME_ADMIN_LEVEL2) {
    // Admin Level 2 is only available in Mexico.
    return [countryCode isEqualToString:@"MX"];
  }

  return true;
}
