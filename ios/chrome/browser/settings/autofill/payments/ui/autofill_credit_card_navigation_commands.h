// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_PAYMENTS_UI_AUTOFILL_CREDIT_CARD_NAVIGATION_COMMANDS_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_PAYMENTS_UI_AUTOFILL_CREDIT_CARD_NAVIGATION_COMMANDS_H_

namespace autofill {
class CreditCard;
}  // namespace autofill

// Commands related to navigation inside the credit card settings page.
@protocol AutofillCreditCardNavigationCommands

// Called to handle dismissing the credit card settings screen.
- (void)handleDismiss;

// Shows the add credit card / payment method view.
- (void)showAddPaymentMethod;

// Shows the CVC storage settings page.
- (void)showCvcStorage;

// Shows the edit/details page for `creditCard`.
- (void)showCreditCardDetails:(const autofill::CreditCard&)creditCard;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_PAYMENTS_UI_AUTOFILL_CREDIT_CARD_NAVIGATION_COMMANDS_H_
