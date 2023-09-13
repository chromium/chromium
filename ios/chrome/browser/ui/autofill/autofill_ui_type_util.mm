// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"

#import "base/notreached.h"
#import "components/autofill/core/common/autofill_features.h"

AutofillUIType AutofillUITypeFromAutofillType(autofill::ServerFieldType type) {
  switch (type) {
    case autofill::UNKNOWN_TYPE:
      return AutofillUITypeUnknown;
    case autofill::CREDIT_CARD_NUMBER:
      return AutofillUITypeCreditCardNumber;
    case autofill::CREDIT_CARD_NAME_FULL:
      return AutofillUITypeCreditCardHolderFullName;
    case autofill::CREDIT_CARD_EXP_MONTH:
      return AutofillUITypeCreditCardExpMonth;
    case autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return AutofillUITypeCreditCardExpYear;
    case autofill::NAME_HONORIFIC_PREFIX:
      return AutofillUITypeProfileHonorificPrefix;
    case autofill::NAME_FULL:
      return AutofillUITypeProfileFullName;
    case autofill::COMPANY_NAME:
      return AutofillUITypeProfileCompanyName;
    case autofill::ADDRESS_HOME_STREET_ADDRESS:
      return AutofillUITypeProfileHomeAddressStreet;
    case autofill::ADDRESS_HOME_LINE1:
      return AutofillUITypeProfileHomeAddressLine1;
    case autofill::ADDRESS_HOME_LINE2:
      return AutofillUITypeProfileHomeAddressLine2;
    case autofill::ADDRESS_HOME_DEPENDENT_LOCALITY:
      return AutofillUITypeProfileHomeAddressDependentLocality;
    case autofill::ADDRESS_HOME_CITY:
      return AutofillUITypeProfileHomeAddressCity;
    case autofill::ADDRESS_HOME_ADMIN_LEVEL2:
      return AutofillUITypeProfileHomeAddressAdminLevel2;
    case autofill::ADDRESS_HOME_STATE:
      return AutofillUITypeProfileHomeAddressState;
    case autofill::ADDRESS_HOME_ZIP:
      return AutofillUITypeProfileHomeAddressZip;
    case autofill::ADDRESS_HOME_SORTING_CODE:
      return AutofillUITypeProfileHomeAddressSortingCode;
    case autofill::ADDRESS_HOME_COUNTRY:
      return AutofillUITypeProfileHomeAddressCountry;
    case autofill::PHONE_HOME_WHOLE_NUMBER:
      return AutofillUITypeProfileHomePhoneWholeNumber;
    case autofill::EMAIL_ADDRESS:
      return AutofillUITypeProfileEmailAddress;
    case autofill::NAME_FULL_WITH_HONORIFIC_PREFIX:
      return AutofillUITypeNameFullWithHonorificPrefix;
    case autofill::ADDRESS_HOME_ADDRESS:
      return AutofillUITypeAddressHomeAddress;
    default:
      NOTREACHED();
      return AutofillUITypeUnknown;
  }
}

autofill::ServerFieldType AutofillTypeFromAutofillUIType(AutofillUIType type) {
  switch (type) {
    case AutofillUITypeUnknown:
      return autofill::UNKNOWN_TYPE;
    case AutofillUITypeCreditCardNumber:
      return autofill::CREDIT_CARD_NUMBER;
    case AutofillUITypeCreditCardHolderFullName:
      return autofill::CREDIT_CARD_NAME_FULL;
    case AutofillUITypeCreditCardExpMonth:
      return autofill::CREDIT_CARD_EXP_MONTH;
    case AutofillUITypeCreditCardExpYear:
      return autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR;
    case AutofillUITypeProfileHonorificPrefix:
      return autofill::NAME_HONORIFIC_PREFIX;
    case AutofillUITypeProfileFullName:
      return autofill::NAME_FULL;
    case AutofillUITypeProfileCompanyName:
      return autofill::COMPANY_NAME;
    case AutofillUITypeProfileHomeAddressStreet:
      return autofill::ADDRESS_HOME_STREET_ADDRESS;
    case AutofillUITypeProfileHomeAddressLine1:
      return autofill::ADDRESS_HOME_LINE1;
    case AutofillUITypeProfileHomeAddressLine2:
      return autofill::ADDRESS_HOME_LINE2;
    case AutofillUITypeProfileHomeAddressDependentLocality:
      return autofill::ADDRESS_HOME_DEPENDENT_LOCALITY;
    case AutofillUITypeProfileHomeAddressCity:
      return autofill::ADDRESS_HOME_CITY;
    case AutofillUITypeProfileHomeAddressAdminLevel2:
      return autofill::ADDRESS_HOME_ADMIN_LEVEL2;
    case AutofillUITypeProfileHomeAddressState:
      return autofill::ADDRESS_HOME_STATE;
    case AutofillUITypeProfileHomeAddressZip:
      return autofill::ADDRESS_HOME_ZIP;
    case AutofillUITypeProfileHomeAddressSortingCode:
      return autofill::ADDRESS_HOME_SORTING_CODE;
    case AutofillUITypeProfileHomeAddressCountry:
      return autofill::ADDRESS_HOME_COUNTRY;
    case AutofillUITypeProfileHomePhoneWholeNumber:
      return autofill::PHONE_HOME_WHOLE_NUMBER;
    case AutofillUITypeProfileEmailAddress:
      return autofill::EMAIL_ADDRESS;
    case AutofillUITypeNameFullWithHonorificPrefix:
      return autofill::NAME_FULL_WITH_HONORIFIC_PREFIX;
    case AutofillUITypeAddressHomeAddress:
      return autofill::ADDRESS_HOME_ADDRESS;
    case AutofillUITypeCreditCardExpDate:
    case AutofillUITypeCreditCardBillingAddress:
    case AutofillUITypeCreditCardSaveToChrome:
    default:
      NOTREACHED();
      return autofill::UNKNOWN_TYPE;
  }
}

std::vector<autofill::ServerFieldType> GetAutofillTypeForProfileEdit() {
  std::vector<autofill::ServerFieldType> all_visible_types;
  for (const AutofillProfileFieldDisplayInfo& row : kProfileFieldsToDisplay)
    all_visible_types.push_back(row.autofillType);

  return all_visible_types;
}

bool FieldIsUsedInAddress(autofill::ServerFieldType autofillType,
                          NSString* countryCode) {
  // TODO(crbug.com/1482269): Replace all this with libaddressinput.

  if (autofillType == autofill::ADDRESS_HOME_DEPENDENT_LOCALITY) {
    // List of countries which require the dependent locality field.
    NSArray<NSString*>* countryCodes = @[
      @"BR", @"CN", @"CO", @"IE", @"IR", @"KR", @"MX", @"MY", @"NG", @"NZ",
      @"PH", @"PK", @"TH", @"ZA"
    ];

    const bool is_enabled_dependent_locality_parsing =
        base::FeatureList::IsEnabled(
            autofill::features::kAutofillEnableDependentLocalityParsing);

    return is_enabled_dependent_locality_parsing &&
           ([countryCodes indexOfObject:countryCode] != NSNotFound);
  }

  if (autofillType == autofill::ADDRESS_HOME_ADMIN_LEVEL2) {
    const bool is_enabled_support_for_admin_level_2 =
        base::FeatureList::IsEnabled(
            autofill::features::kAutofillEnableSupportForAdminLevel2);

    // Admin Level 2 is only available in Mexico.
    return is_enabled_support_for_admin_level_2 &&
           [countryCode isEqualToString:@"MX"];
  }

  return true;
}
