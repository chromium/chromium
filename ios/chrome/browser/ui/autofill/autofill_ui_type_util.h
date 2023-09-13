// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_UI_TYPE_UTIL_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_UI_TYPE_UTIL_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include <vector>

#include "components/autofill/core/browser/field_types.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#include "ios/chrome/grit/ios_strings.h"

// Defines types for the fields that are used by the edit profile screens.
struct AutofillProfileFieldDisplayInfo {
  autofill::ServerFieldType autofillType;
  int displayStringID;
  UIReturnKeyType returnKeyType;
  UIKeyboardType keyboardType;
  UITextAutocapitalizationType autoCapitalizationType;
};

// Stores info for the fields that are used in the edit profile screens.
static const AutofillProfileFieldDisplayInfo kProfileFieldsToDisplay[] = {
    {autofill::NAME_HONORIFIC_PREFIX, IDS_IOS_AUTOFILL_HONORIFIC_PREFIX,
     UIReturnKeyNext, UIKeyboardTypeDefault,
     UITextAutocapitalizationTypeSentences},
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

// Returns the AutofillUIType equivalent to `type`.
AutofillUIType AutofillUITypeFromAutofillType(autofill::ServerFieldType type);

// Returns the autofill::ServerFieldType equivalent to `type`.
autofill::ServerFieldType AutofillTypeFromAutofillUIType(AutofillUIType type);

// Returns the list of autofill::ServerFieldType used by the edit profile
// screens.
std::vector<autofill::ServerFieldType> GetAutofillTypeForProfileEdit();

// Returns whether the provided field is used in the provided country's address.
bool FieldIsUsedInAddress(autofill::ServerFieldType autofillType,
                          NSString* countryCode);

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_UI_TYPE_UTIL_H_
