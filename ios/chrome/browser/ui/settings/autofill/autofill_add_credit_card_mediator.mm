// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_mediator.h"

#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_mediator_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AutofillAddCreditCardMediator ()

// Used for adding new CreditCard object.
@property(nonatomic, assign) autofill::PersonalDataManager* personalDataManager;

// This property is for an interface which sends a response about saving the
// credit card either the credit card is valid or it is invalid.
@property(nonatomic, weak) id<AddCreditCardMediatorDelegate>
    addCreditCardMediatorDelegate;

@end

@implementation AutofillAddCreditCardMediator

- (instancetype)initWithDelegate:(id<AddCreditCardMediatorDelegate>)
                                     addCreditCardMediatorDelegate
             personalDataManager:(autofill::PersonalDataManager*)dataManager {
  self = [super init];

  if (self) {
    DCHECK(dataManager);
    _personalDataManager = dataManager;
    _addCreditCardMediatorDelegate = addCreditCardMediatorDelegate;
  }

  return self;
}

#pragma mark - AddCreditCardViewControllerDelegate

- (void)addCreditCardViewController:
            (AutofillAddCreditCardViewController*)viewController
        addCreditCardWithHolderName:(NSString*)cardHolderName
                         cardNumber:(NSString*)cardNumber
                    expirationMonth:(NSString*)expirationMonth
                     expirationYear:(NSString*)expirationYear
                       cardNickname:(NSString*)cardNickname {
  autofill::CreditCard creditCard =
      [self creditCardWithHolderName:cardHolderName
                          cardNumber:cardNumber
                     expirationMonth:expirationMonth
                      expirationYear:expirationYear
                        cardNickname:cardNickname];

  // Validates the credit card number, expiration date, and nickname.
  if (!creditCard.HasValidCardNumber()) {
    [self.addCreditCardMediatorDelegate
        creditCardMediatorHasInvalidCardNumber:self];
    return;
  }

  if (!creditCard.HasValidExpirationDate()) {
    [self.addCreditCardMediatorDelegate
        creditCardMediatorHasInvalidExpirationDate:self];
    return;
  }

  if (!autofill::CreditCard::IsNicknameValid(
          base::SysNSStringToUTF16(cardNickname))) {
    [self.addCreditCardMediatorDelegate
        creditCardMediatorHasInvalidNickname:self];
    return;
  }

  autofill::CreditCard* savedCreditCard =
      self.personalDataManager->GetCreditCardByNumber(
          base::SysNSStringToUTF8(cardNumber));

  // If the credit card number already exist in saved credit card
  // |savedCreditCard| then update saved credit card |savedCreditCardCopy|
  // with the new data.
  if (savedCreditCard != nil) {
    autofill::CreditCard savedCreditCardCopy(*savedCreditCard);

    [self updateCreditCard:&savedCreditCardCopy
            cardHolderName:cardHolderName
                cardNumber:cardNumber
           expirationMonth:expirationMonth
            expirationYear:expirationYear
              cardNickname:cardNickname];

    self.personalDataManager->UpdateCreditCard(savedCreditCardCopy);
  } else {
    base::RecordAction(
        base::UserMetricsAction("MobileAddCreditCard.CreditCardAdded"));
    self.personalDataManager->AddCreditCard(creditCard);
  }

  [self.addCreditCardMediatorDelegate creditCardMediatorDidFinish:self];
}

- (void)addCreditCardViewControllerDidCancel:
    (AutofillAddCreditCardViewController*)viewController {
  [self.addCreditCardMediatorDelegate creditCardMediatorDidFinish:self];
}

- (bool)addCreditCardViewController:
            (AutofillAddCreditCardViewController*)viewController
            isValidCreditCardNumber:(NSString*)cardNumber {
  return [self isValidCreditCardNumber:cardNumber];
}

- (bool)addCreditCardViewController:
            (AutofillAddCreditCardViewController*)viewController
    isValidCreditCardExpirationMonth:(NSString*)expirationMonth {
  return [self isValidCreditCardExpirationMonth:expirationMonth];
}

- (bool)addCreditCardViewController:
            (AutofillAddCreditCardViewController*)viewController
    isValidCreditCardExpirationYear:(NSString*)expirationYear {
  return [self isValidCreditCardExpirationYear:expirationYear];
}

- (bool)addCreditCardViewController:
            (AutofillAddCreditCardViewController*)viewController
                isValidCardNickname:(NSString*)cardNickname {
  return [self isValidCardNickname:cardNickname];
}

- (bool)addCreditCardViewController:
            (AutofillAddCreditCardViewController*)viewController
            isValidCreditCardNumber:(NSString*)cardNumber
                    expirationMonth:(NSString*)expirationMonth
                     expirationYear:(NSString*)expirationYear
                       cardNickname:(NSString*)cardNickname {
  return ([self isValidCreditCardNumber:cardNumber] &&
          [self isValidCreditCardExpirationMonth:expirationMonth] &&
          [self isValidCreditCardExpirationYear:expirationYear] &&
          [self isValidCardNickname:cardNickname]);
}

#pragma mark - Private

// Returns a new autofill::CreditCard object with |cardHolderName|,
// |cardNumber|, |expirationMonth|, |expirationYear|, |cardNickname|.
- (autofill::CreditCard)creditCardWithHolderName:cardHolderName
                                      cardNumber:cardNumber
                                 expirationMonth:expirationMonth
                                  expirationYear:expirationYear
                                    cardNickname:cardNickname {
  autofill::CreditCard creditCard = autofill::CreditCard();
  [self updateCreditCard:&creditCard
          cardHolderName:cardHolderName
              cardNumber:cardNumber
         expirationMonth:expirationMonth
          expirationYear:expirationYear
            cardNickname:cardNickname];
  return creditCard;
}

// Updates received credit card with received data.
- (void)updateCreditCard:(autofill::CreditCard*)creditCard
          cardHolderName:(NSString*)cardHolderName
              cardNumber:(NSString*)cardNumber
         expirationMonth:(NSString*)expirationMonth
          expirationYear:(NSString*)expirationYear
            cardNickname:(NSString*)cardNickname {
  [self updateCreditCard:creditCard
            cardProperty:cardHolderName
          autofillUIType:AutofillUITypeCreditCardHolderFullName];

  [self updateCreditCard:creditCard
            cardProperty:cardNumber
          autofillUIType:AutofillUITypeCreditCardNumber];

  [self updateCreditCard:creditCard
            cardProperty:expirationMonth
          autofillUIType:AutofillUITypeCreditCardExpMonth];

  [self updateCreditCard:creditCard
            cardProperty:expirationYear
          autofillUIType:AutofillUITypeCreditCardExpYear];

  creditCard->SetNickname(base::SysNSStringToUTF16(cardNickname));
}

// Updates the |AutofillUIType| of the |creditCard| with the value of
// |cardProperty|.
- (void)updateCreditCard:(autofill::CreditCard*)creditCard
            cardProperty:(NSString*)cardValue
          autofillUIType:(AutofillUIType)fieldType {
  const std::string& appLocal = GetApplicationContext()->GetApplicationLocale();

  creditCard->SetInfo(
      autofill::AutofillType(AutofillTypeFromAutofillUIType(fieldType)),
      base::SysNSStringToUTF16(cardValue), appLocal);
}

// Checks if a credit card has a valid |cardNumber|.
- (BOOL)isValidCreditCardNumber:(NSString*)cardNumber {
  autofill::CreditCard creditCard = [self creditCardWithHolderName:nil
                                                        cardNumber:cardNumber
                                                   expirationMonth:nil
                                                    expirationYear:nil
                                                      cardNickname:nil];
  return creditCard.HasValidCardNumber();
}

// Checks if a credit card has a valid |expirationMonth|.
- (BOOL)isValidCreditCardExpirationMonth:(NSString*)expirationMonth {
  return ([expirationMonth integerValue] >= 1 &&
          [expirationMonth integerValue] <= 12);
}

// Checks if a credit card has a valid |expirationYear|.
- (BOOL)isValidCreditCardExpirationYear:(NSString*)expirationYear {
  autofill::CreditCard creditCard =
      [self creditCardWithHolderName:nil
                          cardNumber:nil
                     expirationMonth:nil
                      expirationYear:expirationYear
                        cardNickname:nil];
  return creditCard.HasValidExpirationYear();
}

// Checks if a credit card has a valid |nickname|.
- (BOOL)isValidCardNickname:(NSString*)cardNickname {
  return autofill::CreditCard::IsNicknameValid(
      base::SysNSStringToUTF16(cardNickname));
}

@end
