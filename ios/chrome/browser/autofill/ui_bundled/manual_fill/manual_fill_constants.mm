// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"

namespace manual_fill {

// Passwords

NSString* const kAccessoryPasswordAccessibilityIdentifier =
    @"ManualFillAccessoryPasswordAccessibilityIdentifier";

NSString* const kPasswordDoneButtonAccessibilityIdentifier =
    @"ManualFillPasswordDoneButtonAccessibilityIdentifier";

NSString* const kPasswordSearchBarAccessibilityIdentifier =
    @"ManualFillPasswordSearchBarAccessibilityIdentifier";

NSString* const kPasswordTableViewAccessibilityIdentifier =
    @"ManualFillPasswordTableViewAccessibilityIdentifier";

NSString* const kManagePasswordsAccessibilityIdentifier =
    @"ManualFillManagePasswordsAccessibilityIdentifier";

NSString* const kManageSettingsAccessibilityIdentifier =
    @"ManualFillManageSettingsAccessibilityIdentifier";

NSString* const kOtherPasswordsAccessibilityIdentifier =
    @"ManualFillOtherPasswordsAccessibilityIdentifier";

NSString* const kSuggestPasswordAccessibilityIdentifier =
    @"ManualFillSuggestPasswordAccessibilityIdentifier";

NSString* const kMaskedPasswordButtonText = @"••••••••";

// Payments

NSString* const kAccessoryAddressAccessibilityIdentifier =
    @"ManualFillAccessoryAddressAccessibilityIdentifier";

NSString* const kPaymentManualFillGPayLogoID = @"PaymentManualFillGPayLogoID";

NSString* const kCardTableViewAccessibilityIdentifier =
    @"ManualFillCardTableViewAccessibilityIdentifier";

NSString* const kManagePaymentMethodsAccessibilityIdentifier =
    @"ManualFillManagePaymentMethodsAccessibilityIdentifier";

NSString* const kAddPaymentMethodAccessibilityIdentifier =
    @"ManualFillAddPaymentMethodAccessibilityIdentifier";

// Addresses

NSString* const kAccessoryCreditCardAccessibilityIdentifier =
    @"ManualFillAccessoryCreditCardAccessibilityIdentifier";

NSString* const kAddressTableViewAccessibilityIdentifier =
    @"ManualFillManualFillAddressTableViewAccessibilityIdentifier";

NSString* const kManageAddressAccessibilityIdentifier =
    @"ManualFillManageAddressAccessibilityIdentifier";

// Plus Addresses

NSString* const kManagePlusAddressAccessibilityIdentifier =
    @"ManagePlusAddressAccessibilityIdentifier";

NSString* const kCreatePlusAddressAccessibilityIdentifier =
    @"CreatePlusAddressAccessibilityIdentifier";

NSString* const kSelectPlusAddressAccessibilityIdentifier =
    @"SelectPlusAddressAccessibilityIdentifier";

NSString* const kExpandedManualFillPlusAddressOverflowMenuID =
    @"ExpandedManualFillPlusAddressOverflowMenuID";

NSString* const kPlusAddressDoneButtonAccessibilityIdentifier =
    @"PlusAddressDoneButtonAccessibilityIdentifier";

NSString* const kPlusAddressSearchBarAccessibilityIdentifier =
    @"PlusAddressSearchBarAccessibilityIdentifier";

// Miscellaneous

NSString* const kExpandedManualFillViewID = @"ExpandedManualFillViewID";

NSString* const kExpandedManualFillHeaderViewID =
    @"ExpandedManualFillHeaderViewID";

NSString* const kExpandedManualFillHeaderTopViewID =
    @"ExpandedManualFillHeaderTopViewID";

NSString* const kExpandedManualFillChromeLogoID =
    @"ExpandedManualFillChromeLogoID";

NSString* const kExpandedManualFillAutofillFormButtonID =
    @"ExpandedManualFillAutofillFormButtonID";

NSString* const kExpandedManualFillOverflowMenuID =
    @"ExpandedManualFillOverflowMenuID";

NSString* const kAccessoryKeyboardAccessibilityIdentifier =
    @"ManualFillAccessoryKeyboardAccessibilityIdentifier";

}  // namespace manual_fill

@implementation ManualFillUtil

+ (manual_fill::ManualFillDataType)manualFillDataTypeFromFillingProduct:
    (autofill::FillingProduct)fillingProduct {
  switch (fillingProduct) {
    case autofill::FillingProduct::kAddress:
    case autofill::FillingProduct::kPlusAddresses:
      return manual_fill::ManualFillDataType::kAddress;
    case autofill::FillingProduct::kCreditCard:
    case autofill::FillingProduct::kIban:
    case autofill::FillingProduct::kStandaloneCvc:
      return manual_fill::ManualFillDataType::kPaymentMethod;
    case autofill::FillingProduct::kPassword:
      return manual_fill::ManualFillDataType::kPassword;
    case autofill::FillingProduct::kAutocomplete:
    case autofill::FillingProduct::kNone:
      return manual_fill::ManualFillDataType::kOther;
    case autofill::FillingProduct::kCompose:
    case autofill::FillingProduct::kPredictionImprovements:
    case autofill::FillingProduct::kMerchantPromoCode:
      // These cases are currently not available on iOS.
      NOTREACHED();
  }
}

@end
