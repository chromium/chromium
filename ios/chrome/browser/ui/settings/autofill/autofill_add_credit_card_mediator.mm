// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_mediator.h"

#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_credit_card_util.h"

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
  const std::string& appLocal = GetApplicationContext()->GetApplicationLocale();
  autofill::CreditCard creditCard =
      [AutofillCreditCardUtil creditCardWithHolderName:cardHolderName
                                            cardNumber:cardNumber
                                       expirationMonth:expirationMonth
                                        expirationYear:expirationYear
                                          cardNickname:cardNickname
                                              appLocal:appLocal];

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
  // `savedCreditCard` then update saved credit card `savedCreditCardCopy`
  // with the new data.
  if (savedCreditCard != nil) {
    autofill::CreditCard savedCreditCardCopy(*savedCreditCard);

    [AutofillCreditCardUtil updateCreditCard:&savedCreditCardCopy
                              cardHolderName:cardHolderName
                                  cardNumber:cardNumber
                             expirationMonth:expirationMonth
                              expirationYear:expirationYear
                                cardNickname:cardNickname
                                    appLocal:appLocal];

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
  return [AutofillCreditCardUtil
      isValidCreditCardNumber:cardNumber
                     appLocal:GetApplicationContext()->GetApplicationLocale()];
}

- (bool)addCreditCardViewController:
            (AutofillAddCreditCardViewController*)viewController
    isValidCreditCardExpirationMonth:(NSString*)expirationMonth {
  return
      [AutofillCreditCardUtil isValidCreditCardExpirationMonth:expirationMonth];
}

- (bool)addCreditCardViewController:
            (AutofillAddCreditCardViewController*)viewController
    isValidCreditCardExpirationYear:(NSString*)expirationYear {
  return [AutofillCreditCardUtil
      isValidCreditCardExpirationYear:expirationYear
                             appLocal:GetApplicationContext()
                                          ->GetApplicationLocale()];
}

- (bool)addCreditCardViewController:
            (AutofillAddCreditCardViewController*)viewController
                isValidCardNickname:(NSString*)cardNickname {
  return [AutofillCreditCardUtil isValidCardNickname:cardNickname];
}

- (bool)addCreditCardViewController:
            (AutofillAddCreditCardViewController*)viewController
            isValidCreditCardNumber:(NSString*)cardNumber
                    expirationMonth:(NSString*)expirationMonth
                     expirationYear:(NSString*)expirationYear
                       cardNickname:(NSString*)cardNickname {
  const std::string& appLocal = GetApplicationContext()->GetApplicationLocale();
  return ([AutofillCreditCardUtil isValidCreditCardNumber:cardNumber
                                                 appLocal:appLocal] &&
          [AutofillCreditCardUtil
              isValidCreditCardExpirationMonth:expirationMonth] &&
          [AutofillCreditCardUtil isValidCreditCardExpirationYear:expirationYear
                                                         appLocal:appLocal] &&
          [AutofillCreditCardUtil isValidCardNickname:cardNickname]);
}

@end
