// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_CREDIT_CARD_UTIL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_CREDIT_CARD_UTIL_H_

#import <Foundation/Foundation.h>

#import "components/autofill/core/browser/data_model/credit_card.h"

@interface AutofillCreditCardUtil : NSObject

// Returns a new autofill::CreditCard object with `cardHolderName`,
// `cardNumber`, `expirationMonth`, `expirationYear`, `cardNickname`.
+ (autofill::CreditCard)creditCardWithHolderName:(NSString*)cardHolderName
                                      cardNumber:(NSString*)cardNumber
                                 expirationMonth:(NSString*)expirationMonth
                                  expirationYear:(NSString*)expirationYear
                                    cardNickname:(NSString*)cardNickname
                                        appLocal:(const std::string&)appLocal;

// Returns true if the card details are valid.
+ (bool)isValidCreditCard:(NSString*)cardNumber
          expirationMonth:(NSString*)expirationMonth
           expirationYear:(NSString*)expirationYear
             cardNickname:(NSString*)cardNickname
                 appLocal:(const std::string&)appLocal;

// Updates received credit card with received data.
+ (void)updateCreditCard:(autofill::CreditCard*)creditCard
          cardHolderName:(NSString*)cardHolderName
              cardNumber:(NSString*)cardNumber
         expirationMonth:(NSString*)expirationMonth
          expirationYear:(NSString*)expirationYear
            cardNickname:(NSString*)cardNickname
                appLocal:(const std::string&)appLocal;

// Checks if a credit card has a valid `cardNumber`.
+ (BOOL)isValidCreditCardNumber:(NSString*)cardNumber
                       appLocal:(const std::string&)appLocal;

// Checks if a credit card has a valid `expirationMonth`.
+ (BOOL)isValidCreditCardExpirationMonth:(NSString*)expirationMonth;

// Checks if a credit card has a valid `expirationYear`.
+ (BOOL)isValidCreditCardExpirationYear:(NSString*)expirationYear
                               appLocal:(const std::string&)appLocal;

// Checks if a credit card has a valid `nickname`.
+ (BOOL)isValidCardNickname:(NSString*)cardNickname;

// Evaluates whether the passed `card` should be edited from the Payments web
// page.
+ (BOOL)shouldEditCardFromPaymentsWebPage:(const autofill::CreditCard&)card;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_CREDIT_CARD_UTIL_H_
