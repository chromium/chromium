// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import "components/autofill/core/browser/filling_product.h"

namespace manual_fill {

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

// Accessibility identifier for the GPay logo shown in the payment manual fill
// cells when the corresponding card is a server card.
extern NSString* const kPaymentManualFillGPayLogoID;

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
