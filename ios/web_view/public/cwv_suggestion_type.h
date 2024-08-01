// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_SUGGESTION_TYPE_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_SUGGESTION_TYPE_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// CWVAutofillSuggestion suggestion type.
//
// Implementation comment: This enum mirrors autofill::SuggestionType,
// including the integer values. This allows an autofill:SuggestionType to
// be casted directly into a CWVSuggestionType.
typedef NS_ENUM(NSInteger, CWVSuggestionType) {
  // Autocomplete suggestions.
  CWVSuggestionTypeAutocompleteEntry = 0,

  // Autofill profile suggestions.
  // Fill the whole for the current address. On Desktop, it is triggered from
  // the main (i.e. root popup) suggestion.
  CWVSuggestionTypeAddressEntry = 1,
  // Fills all address related fields, e.g ADDRESS_HOME_LINE1,
  // ADDRESS_HOME_HOUSE_NUMBER etc.
  CWVSuggestionTypeFillFullAddress = 2,
  // Fills all name related fields, e.g NAME_FIRST, NAME_MIDDLE, NAME_LAST
  // etc.
  CWVSuggestionTypeFillFullName = 3,
  // Same as above, however it is triggered from the subpopup. This option
  // is displayed once the users is on group filling level or field by field
  // level. It is used as a way to allow users to go back to filling the whole
  // form. We need it as a separate id from `kAddressEntry` because it has a
  // different UI and for logging.
  CWVSuggestionTypeFillEverythingFromAddressProfile = 4,
  // When triggered from a phone number field this suggestion will fill every
  // phone number field.
  CWVSuggestionTypeFillFullPhoneNumber = 5,
  // Same as above, when triggered from an email address field this suggestion
  // will fill every email field.
  CWVSuggestionTypeFillFullEmail = 6,
  CWVSuggestionTypeAddressFieldByFieldFilling = 7,
  CWVSuggestionTypeEditAddressProfile = 8,
  CWVSuggestionTypeDeleteAddressProfile = 9,
  CWVSuggestionTypeManageAddress = 10,
  CWVSuggestionTypeManageCreditCard = 11,
  CWVSuggestionTypeManageIban = 12,
  CWVSuggestionTypeManagePlusAddress = 13,

  // Compose popup suggestion shown when no Compose session exists.
  CWVSuggestionTypeComposeProactiveNudge = 14,
  // Compose popup suggestion shown when there is an existing Compose session.
  CWVSuggestionTypeComposeResumeNudge = 15,
  // Compose popup suggestion shown after the Compose dialog closes.
  CWVSuggestionTypeComposeSavedStateNotification = 16,
  // Compose sub-menu suggestions
  CWVSuggestionTypeComposeDisable = 17,
  CWVSuggestionTypeComposeGoToSettings = 18,
  CWVSuggestionTypeComposeNeverShowOnThisSiteAgain = 19,

  // Datalist suggestions.
  CWVSuggestionTypeDatalistEntry = 20,

  // Password suggestions.
  CWVSuggestionTypePasswordEntry = 21,
  CWVSuggestionTypeAllSavedPasswordsEntry = 22,
  CWVSuggestionTypeGeneratePasswordEntry = 23,
  CWVSuggestionTypeShowAccountCards = 24,
  CWVSuggestionTypePasswordAccountStorageOptIn = 25,
  CWVSuggestionTypePasswordAccountStorageOptInAndGenerate = 26,
  CWVSuggestionTypeAccountStoragePasswordEntry = 27,
  CWVSuggestionTypePasswordAccountStorageReSignin = 28,
  CWVSuggestionTypePasswordAccountStorageEmpty = 29,
  CWVSuggestionTypePasswordFieldByFieldFilling = 30,
  CWVSuggestionTypeFillPassword = 31,
  CWVSuggestionTypeViewPasswordDetails = 32,

  // Payment suggestions.
  CWVSuggestionTypeCreditCardEntry = 33,
  CWVSuggestionTypeInsecureContextPaymentDisabledMessage = 34,
  CWVSuggestionTypeScanCreditCard = 35,
  CWVSuggestionTypeVirtualCreditCardEntry = 36,
  CWVSuggestionTypeCreditCardFieldByFieldFilling = 37,
  CWVSuggestionTypeIbanEntry = 38,

  // Plus address suggestions.
  CWVSuggestionTypeCreateNewPlusAddress = 39,
  CWVSuggestionTypeFillExistingPlusAddress = 40,

  // Promotion suggestions.
  CWVSuggestionTypeMerchantPromoCodeEntry = 41,
  CWVSuggestionTypeSeePromoCodeDetails = 42,

  // Webauthn suggestions.
  CWVSuggestionTypeWebauthnCredential = 43,
  CWVSuggestionTypeWebauthnSignInWithAnotherDevice = 44,

  // Other suggestions.
  CWVSuggestionTypeTitle = 45,
  CWVSuggestionTypeSeparator = 46,
  CWVSuggestionTypeUndoOrClear = 47,
  CWVSuggestionTypeMixedFormMessage = 48,

  // Top level suggestion rendered when test addresses are available. Shown only
  // when DevTools is open.
  CWVSuggestionTypeDevtoolsTestAddresses = 49,
  // Test address option that specifies a full address for a country
  // so that users can test their form with it.
  CWVSuggestionTypeDevtoolsTestAddressEntry = 50,

  CWVSuggestionTypeUnknown = -1
};

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_SUGGESTION_TYPE_H_
