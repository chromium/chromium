// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/autofill_credit_card_util.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_type.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_credit_card_ui_type.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_ui_type_util.h"

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
      autofillCreditCardUIType:AutofillCreditCardUIType::kFullName
                      appLocal:appLocal];

  [self updateCreditCard:creditCard
                  cardProperty:cardNumber
      autofillCreditCardUIType:AutofillCreditCardUIType::kNumber
                      appLocal:appLocal];

  [self updateCreditCard:creditCard
                  cardProperty:expirationMonth
      autofillCreditCardUIType:AutofillCreditCardUIType::kExpMonth
                      appLocal:appLocal];

  [self updateCreditCard:creditCard
                  cardProperty:expirationYear
      autofillCreditCardUIType:AutofillCreditCardUIType::kExpYear
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

+ (BOOL)shouldEditCardFromPaymentsWebPage:(const autofill::CreditCard&)card {
  switch (card.record_type()) {
    case autofill::CreditCard::RecordType::kLocalCard:
    case autofill::CreditCard::RecordType::kVirtualCard:
      return NO;
    case autofill::CreditCard::RecordType::kMaskedServerCard:
      return YES;
    case autofill::CreditCard::RecordType::kFullServerCard:
      // Full server cards are a temporary cached state and should not be
      // offered for edit (from payments web page or otherwise).
      NOTREACHED();
  }
}

#pragma mark - Private

// Updates the `AutofillUIType` of the `creditCard` with the value of
// `cardProperty`.
+ (void)updateCreditCard:(autofill::CreditCard*)creditCard
                cardProperty:(NSString*)cardValue
    autofillCreditCardUIType:(AutofillCreditCardUIType)autofillCreditCardUIType
                    appLocal:(const std::string&)appLocal {
  creditCard->SetInfo(
      autofill::AutofillType(
          AutofillTypeFromAutofillUITypeForCard(autofillCreditCardUIType)),
      base::SysNSStringToUTF16(cardValue), appLocal);
}

@end
