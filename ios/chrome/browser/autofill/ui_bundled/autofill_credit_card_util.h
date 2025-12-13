// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_CREDIT_CARD_UTIL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_CREDIT_CARD_UTIL_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "components/autofill/core/browser/data_model/payments/credit_card.h"
#import "ios/chrome/browser/autofill/model/message/save_card_message_with_links.h"

@interface AutofillCreditCardUtil : NSObject

// Returns a new autofill::CreditCard object with `cardHolderName`,
// `cardNumber`, `expirationMonth`, `expirationYear`, `cardNickname`
// `cardCvc`
+ (autofill::CreditCard)creditCardWithHolderName:(NSString*)cardHolderName
                                      cardNumber:(NSString*)cardNumber
                                 expirationMonth:(NSString*)expirationMonth
                                  expirationYear:(NSString*)expirationYear
                                    cardNickname:(NSString*)cardNickname
                                         cardCvc:(NSString*)cardCvc
                                        appLocal:(const std::string&)appLocal;

// Returns true if the card details are valid.
+ (bool)isValidCreditCard:(NSString*)cardNumber
          expirationMonth:(NSString*)expirationMonth
           expirationYear:(NSString*)expirationYear
             cardNickname:(NSString*)cardNickname
                  cardCvc:(NSString*)cardCvc
                 appLocal:(const std::string&)appLocal;

// Updates received credit card with received data.
+ (void)updateCreditCard:(autofill::CreditCard*)creditCard
          cardHolderName:(NSString*)cardHolderName
              cardNumber:(NSString*)cardNumber
         expirationMonth:(NSString*)expirationMonth
          expirationYear:(NSString*)expirationYear
            cardNickname:(NSString*)cardNickname
                 cardCvc:(NSString*)cardCvc
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

// Checks if a credit card has a valid `cardCvc`.
+ (BOOL)isValidCardCvc:(NSString*)cardCvc;

// Evaluates whether the passed `card` should be edited from the Payments web
// page.
+ (BOOL)shouldEditCardFromPaymentsWebPage:(const autofill::CreditCard&)card;

// Creates a text view suitable for payment flows to display
// SaveCardMessageWithLinks. Adds hyperlinks in the specified range of text. If
// the text view is expected to allow user interaction with the hyperlinks, then
// the caller is responsible to set the UITextViewDelegate of the text view to
// respond to user's actions.
// TODO(crbug.com/413056780): Rename SaveCardMessageWithLinks to
// LegalMessageLine.
+ (UITextView*)createTextViewForLegalMessage:
    (SaveCardMessageWithLinks*)legalMessage;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_CREDIT_CARD_UTIL_H_
