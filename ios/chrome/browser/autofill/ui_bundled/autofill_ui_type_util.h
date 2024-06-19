// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_UI_TYPE_UTIL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_UI_TYPE_UTIL_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "components/autofill/core/browser/field_types.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_credit_card_ui_type.h"
#import "ios/chrome/grit/ios_strings.h"

// Defines types for the fields that are used by the edit profile screens.
struct AutofillProfileFieldDisplayInfo {
  autofill::FieldType autofillType;
  int displayStringID;
  UIReturnKeyType returnKeyType;
  UIKeyboardType keyboardType;
  UITextAutocapitalizationType autoCapitalizationType;
};

// Stores info for the fields that are used in the edit profile screens.
static const AutofillProfileFieldDisplayInfo kProfileFieldsToDisplay[] = {
    {autofill::NAME_FULL, IDS_IOS_AUTOFILL_FULLNAME, UIReturnKeyNext,
     UIKeyboardTypeDefault, UITextAutocapitalizationTypeSentences},
    {autofill::COMPANY_NAME, IDS_IOS_AUTOFILL_COMPANY_NAME, UIReturnKeyNext,
     UIKeyboardTypeDefault, UITextAutocapitalizationTypeSentences},
    {autofill::ADDRESS_HOME_LINE1, IDS_IOS_AUTOFILL_ADDRESS1, UIReturnKeyNext,
     UIKeyboardTypeDefault, UITextAutocapitalizationTypeSentences},
    {autofill::ADDRESS_HOME_LINE2, IDS_IOS_AUTOFILL_ADDRESS2, UIReturnKeyNext,
     UIKeyboardTypeDefault, UITextAutocapitalizationTypeSentences},
    {autofill::ADDRESS_HOME_DEPENDENT_LOCALITY,
     IDS_IOS_AUTOFILL_DEPENDENT_LOCALITY, UIReturnKeyNext,
     UIKeyboardTypeDefault, UITextAutocapitalizationTypeSentences},
    {autofill::ADDRESS_HOME_CITY, IDS_IOS_AUTOFILL_CITY, UIReturnKeyNext,
     UIKeyboardTypeDefault, UITextAutocapitalizationTypeSentences},
    {autofill::ADDRESS_HOME_ADMIN_LEVEL2, IDS_IOS_AUTOFILL_ADMIN_LEVEL2,
     UIReturnKeyNext, UIKeyboardTypeDefault,
     UITextAutocapitalizationTypeSentences},
    {autofill::ADDRESS_HOME_STATE, IDS_IOS_AUTOFILL_STATE, UIReturnKeyNext,
     UIKeyboardTypeDefault, UITextAutocapitalizationTypeSentences},
    {autofill::ADDRESS_HOME_ZIP, IDS_IOS_AUTOFILL_ZIP, UIReturnKeyNext,
     UIKeyboardTypeDefault, UITextAutocapitalizationTypeAllCharacters},
    {autofill::ADDRESS_HOME_COUNTRY, IDS_IOS_AUTOFILL_COUNTRY, UIReturnKeyNext,
     UIKeyboardTypeDefault, UITextAutocapitalizationTypeSentences},
    {autofill::PHONE_HOME_WHOLE_NUMBER, IDS_IOS_AUTOFILL_PHONE, UIReturnKeyNext,
     UIKeyboardTypePhonePad, UITextAutocapitalizationTypeSentences},
    {autofill::EMAIL_ADDRESS, IDS_IOS_AUTOFILL_EMAIL, UIReturnKeyDone,
     UIKeyboardTypeEmailAddress, UITextAutocapitalizationTypeNone}};

// Returns the `AutofillCreditCardUIType` equivalent to `type`.
AutofillCreditCardUIType AutofillUITypeFromAutofillTypeForCard(
    autofill::FieldType type);

// Returns the autofill::FieldType equivalent to `type`.
autofill::FieldType AutofillTypeFromAutofillUITypeForCard(
    AutofillCreditCardUIType type);

// Returns whether the provided field is used in the provided country's address.
bool FieldIsUsedInAddress(autofill::FieldType autofillType,
                          NSString* countryCode);

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_UI_TYPE_UTIL_H_
