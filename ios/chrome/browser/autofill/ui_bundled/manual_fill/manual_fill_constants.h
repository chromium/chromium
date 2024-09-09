// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import "components/autofill/core/browser/filling_product.h"

namespace manual_fill {

// Passwords

// Accessibility identifier of the password button.
extern NSString* const kAccessoryPasswordAccessibilityIdentifier;

// Accessibility Identifier for the done button in the select password sheet.
extern NSString* const kPasswordDoneButtonAccessibilityIdentifier;

// Accessibility Identifier for the search bar in the select password sheet.
extern NSString* const kPasswordSearchBarAccessibilityIdentifier;

// Accessibility Identifier for select password sheet and the password manual
// fill view.
extern NSString* const kPasswordTableViewAccessibilityIdentifier;

// Accessibility identifier for the manage password action.
extern NSString* const kManagePasswordsAccessibilityIdentifier;

// Accessibility identifier for the manage settings action in the password view.
extern NSString* const kManageSettingsAccessibilityIdentifier;

// Accessibility identifier for the other password action.
extern NSString* const kOtherPasswordsAccessibilityIdentifier;

// Accessibility identifier for the suggest password action.
extern NSString* const kSuggestPasswordAccessibilityIdentifier;

extern NSString* const kMaskedPasswordButtonText;

// Payments

// Accessibility identifier of the address button.
extern NSString* const kAccessoryAddressAccessibilityIdentifier;

// Accessibility identifier for the GPay logo shown in the payment manual fill
// cells when the corresponding card is a server card.
extern NSString* const kPaymentManualFillGPayLogoID;

// Accessibility identifier for the card manual fill view.
extern NSString* const kCardTableViewAccessibilityIdentifier;

// Accessibility identifier for the manage card action.
extern NSString* const kManagePaymentMethodsAccessibilityIdentifier;

// Accessibility identifier for the add payment method action.
extern NSString* const kAddPaymentMethodAccessibilityIdentifier;

// Addresses

// Accessibility identifier of the credit card button.
extern NSString* const kAccessoryCreditCardAccessibilityIdentifier;

// Accessibility identifier for the address manual fill view.
extern NSString* const kAddressTableViewAccessibilityIdentifier;

// Accessibility identifier for the manage address action.
extern NSString* const kManageAddressAccessibilityIdentifier;

// Plus Addresses

// Accessibility identifier for the manage plus address action.
extern NSString* const kManagePlusAddressAccessibilityIdentifier;

// Accessibility identifier for the create plus address action.
extern NSString* const kCreatePlusAddressAccessibilityIdentifier;

// Accessibility identifier for the select plus address action.
extern NSString* const kSelectPlusAddressAccessibilityIdentifier;

// Accessibility identifier for the overflow menu in plus address cell.
extern NSString* const kExpandedManualFillPlusAddressOverflowMenuID;

// Accessibility Identifier for the done button in the select plus address
// sheet.
extern NSString* const kPlusAddressDoneButtonAccessibilityIdentifier;

// Accessibility Identifier for the search bar in the select plus address sheet.
extern NSString* const kPlusAddressSearchBarAccessibilityIdentifier;

// Miscellaneous

// Accessibility identifier for the expanded manual fill view.
extern NSString* const kExpandedManualFillViewID;

// Accessibility identifier for the header view of the expanded manual fill
// view.
extern NSString* const kExpandedManualFillHeaderViewID;

// Accessibility identifier for the header top view of the expanded manual fill
// view.
extern NSString* const kExpandedManualFillHeaderTopViewID;

// Accessibility identifier for the Chrome logo of the expanded manual fill
// view.
extern NSString* const kExpandedManualFillChromeLogoID;

// Accessibility identifier for the "Autofill Form" button shown in the entity
// cells of the expanded manual fill view.
extern NSString* const kExpandedManualFillAutofillFormButtonID;

// Accessibility identifier for the overflow menu shown in the entity cells of
// the expanded manual fill view.
extern NSString* const kExpandedManualFillOverflowMenuID;

// Accessibility identifier of the keyboard button.
extern NSString* const kAccessoryKeyboardAccessibilityIdentifier;

// Possible data types when manually filling a form.
enum class ManualFillDataType {
  kPassword = 0,
  kPaymentMethod,
  kAddress,
  kOther,
};

// Possible payment field types when manually filling a form.
enum class PaymentFieldType {
  kCardNumber = 0,
  kExpirationMonth,
  kExpirationYear,
  kCVC,
};

}  // namespace manual_fill

@interface ManualFillUtil : NSObject

// Returns a ManualFillDataType based on the provided FillingProduct.
+ (manual_fill::ManualFillDataType)manualFillDataTypeFromFillingProduct:
    (autofill::FillingProduct)fillingProduct;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CONSTANTS_H_
