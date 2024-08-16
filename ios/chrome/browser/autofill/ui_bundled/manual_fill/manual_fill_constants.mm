// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"

namespace manual_fill {

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

NSString* const kPaymentManualFillGPayLogoID = @"PaymentManualFillGPayLogoID";

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
