// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_credit_card_util.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_type.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"

@implementation AutofillCreditCardUtil

+ (autofill::CreditCard)creditCardWithHolderName:(NSString*)cardHolderName
                                      cardNumber:(NSString*)cardNumber
                                 expirationMonth:(NSString*)expirationMonth
                                  expirationYear:(NSString*)expirationYear
                                    cardNickname:(NSString*)cardNickname
                                        appLocal:(const std::string&)appLocal {
  autofill::CreditCard creditCard = autofill::CreditCard();
  [self updateCreditCard:&creditCard
          cardHolderName:cardHolderName
              cardNumber:cardNumber
         expirationMonth:expirationMonth
          expirationYear:expirationYear
            cardNickname:cardNickname
                appLocal:appLocal];
  return creditCard;
}

+ (bool)isValidCreditCard:(NSString*)cardNumber
          expirationMonth:(NSString*)expirationMonth
           expirationYear:(NSString*)expirationYear
             cardNickname:(NSString*)cardNickname
                 appLocal:(const std::string&)appLocal {
  return ([self isValidCreditCardNumber:cardNumber appLocal:appLocal] &&
          [self isValidCreditCardExpirationMonth:expirationMonth] &&
          [self isValidCreditCardExpirationYear:expirationYear
                                       appLocal:appLocal] &&
          [self isValidCardNickname:cardNickname]);
}

+ (void)updateCreditCard:(autofill::CreditCard*)creditCard
          cardHolderName:(NSString*)cardHolderName
              cardNumber:(NSString*)cardNumber
         expirationMonth:(NSString*)expirationMonth
          expirationYear:(NSString*)expirationYear
            cardNickname:(NSString*)cardNickname
                appLocal:(const std::string&)appLocal {
  [self updateCreditCard:creditCard
            cardProperty:cardHolderName
          autofillUIType:AutofillUITypeCreditCardHolderFullName
                appLocal:appLocal];

  [self updateCreditCard:creditCard
            cardProperty:cardNumber
          autofillUIType:AutofillUITypeCreditCardNumber
                appLocal:appLocal];

  [self updateCreditCard:creditCard
            cardProperty:expirationMonth
          autofillUIType:AutofillUITypeCreditCardExpMonth
                appLocal:appLocal];

  [self updateCreditCard:creditCard
            cardProperty:expirationYear
          autofillUIType:AutofillUITypeCreditCardExpYear
                appLocal:appLocal];

  creditCard->SetNickname(base::SysNSStringToUTF16(cardNickname));
}

+ (BOOL)isValidCreditCardNumber:(NSString*)cardNumber
                       appLocal:(const std::string&)appLocal {
  autofill::CreditCard creditCard = [self creditCardWithHolderName:nil
                                                        cardNumber:cardNumber
                                                   expirationMonth:nil
                                                    expirationYear:nil
                                                      cardNickname:nil
                                                          appLocal:appLocal];
  return creditCard.HasValidCardNumber();
}

+ (BOOL)isValidCreditCardExpirationMonth:(NSString*)expirationMonth {
  return ([expirationMonth integerValue] >= 1 &&
          [expirationMonth integerValue] <= 12);
}

+ (BOOL)isValidCreditCardExpirationYear:(NSString*)expirationYear
                               appLocal:(const std::string&)appLocal {
  autofill::CreditCard creditCard =
      [self creditCardWithHolderName:nil
                          cardNumber:nil
                     expirationMonth:nil
                      expirationYear:expirationYear
                        cardNickname:nil
                            appLocal:appLocal];
  return creditCard.HasValidExpirationYear();
}

+ (BOOL)isValidCardNickname:(NSString*)cardNickname {
  return autofill::CreditCard::IsNicknameValid(
      base::SysNSStringToUTF16(cardNickname));
}

#pragma mark - Private

// Updates the `AutofillUIType` of the `creditCard` with the value of
// `cardProperty`.
+ (void)updateCreditCard:(autofill::CreditCard*)creditCard
            cardProperty:(NSString*)cardValue
          autofillUIType:(AutofillUIType)fieldType
                appLocal:(const std::string&)appLocal {
  creditCard->SetInfo(
      autofill::AutofillType(AutofillTypeFromAutofillUIType(fieldType)),
      base::SysNSStringToUTF16(cardValue), appLocal);
}

@end
