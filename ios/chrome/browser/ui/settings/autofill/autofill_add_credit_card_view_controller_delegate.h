// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_ADD_CREDIT_CARD_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_ADD_CREDIT_CARD_VIEW_CONTROLLER_DELEGATE_H_

@class AutofillAddCreditCardViewController;

// Delegate manages adding a new credit card.
@protocol AddCreditCardViewControllerDelegate

// Receives a credit card data. Implement this method to save a new credit card.
- (void)addCreditCardViewController:
            (AutofillAddCreditCardViewController*)viewController
        addCreditCardWithHolderName:(NSString*)cardHolderName
                         cardNumber:(NSString*)cardNumber
                    expirationMonth:(NSString*)expirationMonth
                     expirationYear:(NSString*)expirationYear
                       cardNickname:(NSString*)cardNickname;

// Notifies the class which conform this delegate for cancel button tap in
// received view controller.
- (void)addCreditCardViewControllerDidCancel:
    (AutofillAddCreditCardViewController*)viewController;

// Checks if a credit card has a valid `cardNumber`.
- (bool)addCreditCardViewController:
            (AutofillAddCreditCardViewController*)viewController
            isValidCreditCardNumber:(NSString*)cardNumber;

// Checks if a credit card has a valid `expirationMonth`.
- (bool)addCreditCardViewController:
            (AutofillAddCreditCardViewController*)viewController
    isValidCreditCardExpirationMonth:(NSString*)expirationMonth;

// Checks if a credit card has a valid `expirationYear`.
- (bool)addCreditCardViewController:
            (AutofillAddCreditCardViewController*)viewController
    isValidCreditCardExpirationYear:(NSString*)expirationYear;

// Checks if a credit card has a valid `cardNickname`.
- (bool)addCreditCardViewController:
            (AutofillAddCreditCardViewController*)viewController
                isValidCardNickname:(NSString*)cardNickname;

// Checks if a credit card has a valid `cardNumber`, `expirationMonth`, a
// `expirationYear`, and `cardNickname`.
- (bool)addCreditCardViewController:
            (AutofillAddCreditCardViewController*)viewController
            isValidCreditCardNumber:(NSString*)cardNumber
                    expirationMonth:(NSString*)expirationMonth
                     expirationYear:(NSString*)expirationYear
                       cardNickname:(NSString*)cardNickname;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_ADD_CREDIT_CARD_VIEW_CONTROLLER_DELEGATE_H_
