// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CREDIT_CARD_CREDITCARD_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CREDIT_CARD_CREDITCARD_H_

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_credit_card.h"

namespace autofill {
class CreditCard;
}

@interface ManualFillCreditCard (CreditCard)

// Convenience initializer from a autofill::CreditCard. It also prepares some
// fields for user presentation, like creating an obfuscated version of the
// credit card number and formatting month/year fields.
- (instancetype)initWithCreditCard:(const autofill::CreditCard&)creditCard
                              icon:(UIImage*)icon;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CREDIT_CARD_CREDITCARD_H_
