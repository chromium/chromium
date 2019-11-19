// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_mediator.h"

#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
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
                     expirationYear:(NSString*)expirationYear {
  autofill::CreditCard creditCard =
      [self creditCardWithHolderName:cardHolderName
                          cardNumber:cardNumber
                     expirationMonth:expirationMonth
                      expirationYear:expirationYear];

  // Validates the credit card number and expiration date.
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
            expirationYear:expirationYear];

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

- (void)addCreditCardViewControllerDidUseCamera:
    (AutofillAddCreditCardViewController*)viewController {
  [self.addCreditCardMediatorDelegate creditCardMediatorShowScanner:self];
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
            isValidCreditCardNumber:(NSString*)cardNumber
                    expirationMonth:(NSString*)expirationMonth
                     expirationYear:(NSString*)expirationYear {
  BOOL isValidCardNumber = [self isValidCreditCardNumber:cardNumber];
  BOOL isValidCardExpiryMonth =
      [self isValidCreditCardExpirationMonth:expirationMonth];
  BOOL isValidCardExpiryYear =
      [self isValidCreditCardExpirationYear:expirationYear];

  return (isValidCardNumber && isValidCardExpiryMonth && isValidCardExpiryYear);
}

#pragma mark - Private

// Returns a new autofill::CreditCard object with |cardHolderName|,
// |cardNumber|, |expirationMonth|, |expirationYear|.
- (autofill::CreditCard)creditCardWithHolderName:cardHolderName
                                      cardNumber:cardNumber
                                 expirationMonth:expirationMonth
                                  expirationYear:expirationYear {
  autofill::CreditCard creditCard = autofill::CreditCard();
  [self updateCreditCard:&creditCard
          cardHolderName:cardHolderName
              cardNumber:cardNumber
         expirationMonth:expirationMonth
          expirationYear:expirationYear];
  return creditCard;
}

// Updates received credit card with received data.
- (void)updateCreditCard:(autofill::CreditCard*)creditCard
          cardHolderName:(NSString*)cardHolderName
              cardNumber:(NSString*)cardNumber
         expirationMonth:(NSString*)expirationMonth
          expirationYear:(NSString*)expirationYear {
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
                                                    expirationYear:nil];
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
                      expirationYear:expirationYear];
  return creditCard.HasValidExpirationYear();
}

@end
