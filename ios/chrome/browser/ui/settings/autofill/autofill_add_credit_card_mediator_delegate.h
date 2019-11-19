// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_ADD_CREDIT_CARD_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_ADD_CREDIT_CARD_MEDIATOR_DELEGATE_H_

@class AutofillAddCreditCardMediator;

// This delegate is notified of the result of saving a credit card.
@protocol AddCreditCardMediatorDelegate

// Notifies that the credit card number is invalid.
- (void)creditCardMediatorHasInvalidCardNumber:
    (AutofillAddCreditCardMediator*)mediator;

// Notifies that the credit card expiration date is invalid.
- (void)creditCardMediatorHasInvalidExpirationDate:
    (AutofillAddCreditCardMediator*)mediator;

// Notifies that the credit card scanner needs to be shown.
- (void)creditCardMediatorShowScanner:(AutofillAddCreditCardMediator*)mediator;

// Notifies that the credit card is valid or the user cancel the view
// controller.
- (void)creditCardMediatorDidFinish:(AutofillAddCreditCardMediator*)mediator;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_ADD_CREDIT_CARD_MEDIATOR_DELEGATE_H_
