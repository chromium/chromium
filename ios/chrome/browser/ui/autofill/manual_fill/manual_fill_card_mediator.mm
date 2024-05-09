// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_card_mediator.h"

#import <vector>

#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/browser_autofill_manager.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_consumer.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_list_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/full_card_request_result_delegate_bridge.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_action_cell.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_card_cell.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_content_injector.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_credit_card+CreditCard.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_credit_card.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/registry_controlled_domains/registry_controlled_domain.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

using autofill::CreditCard;
using manual_fill::PaymentFieldType;

namespace manual_fill {

NSString* const kManagePaymentMethodsAccessibilityIdentifier =
    @"kManagePaymentMethodsAccessibilityIdentifier";
NSString* const kAddPaymentMethodAccessibilityIdentifier =
    @"kAddPaymentMethodAccessibilityIdentifier";

}  // namespace manual_fill

@interface ManualFillCardMediator ()

// All available credit cards.
@property(nonatomic, assign) std::vector<CreditCard*> cards;

@end

@implementation ManualFillCardMediator

- (instancetype)initWithCards:(std::vector<CreditCard*>)cards {
  self = [super init];
  if (self) {
    _cards = cards;
  }
  return self;
}

- (void)setConsumer:(id<ManualFillCardConsumer>)consumer {
  if (consumer == _consumer) {
    return;
  }
  _consumer = consumer;
  [self postCardsToConsumer];
  [self postActionsToConsumer];
}

- (const CreditCard*)findCreditCardfromGUID:(NSString*)GUID {
  for (CreditCard* card : self.cards) {
    NSString* cppGUID =
        base::SysUTF16ToNSString(base::ASCIIToUTF16(card->guid()));
    if ([cppGUID isEqualToString:GUID])
      return card;
  }
  return nil;
}

- (void)reloadWithCards:(std::vector<CreditCard*>)cards {
  self.cards = cards;
  if (self.consumer) {
    [self postCardsToConsumer];
    [self postActionsToConsumer];
  }
}

#pragma mark - Private

// Posts the cards to the consumer.
- (void)postCardsToConsumer {
  if (!self.consumer) {
    return;
  }

  NSMutableArray* cardItems =
      [[NSMutableArray alloc] initWithCapacity:self.cards.size()];
  for (CreditCard* card : self.cards) {
    // If this card is enrolled to have a virtual card, create the virtual card
    // and order it directly before the original card.
    if (base::FeatureList::IsEnabled(
            autofill::features::kAutofillEnableVirtualCards) &&
        card->virtual_card_enrollment_state() ==
            CreditCard::VirtualCardEnrollmentState::kEnrolled) {
      CreditCard virtualCard = CreditCard::CreateVirtualCard(*card);
      [cardItems addObject:[self createManualFillCardItemForCard:virtualCard]];
    }
    [cardItems addObject:[self createManualFillCardItemForCard:*card]];
  }

  [self.consumer presentCards:cardItems];
}

// Creates a ManualFillCardItem for the given `card`.
- (ManualFillCardItem*)createManualFillCardItemForCard:(CreditCard)card {
  ManualFillCreditCard* manualFillCreditCard =
      [[ManualFillCreditCard alloc] initWithCreditCard:card];
  NSArray<UIAction*>* menuActions =
      IsKeyboardAccessoryUpgradeEnabled() ? [self createMenuActions] : @[];

  return [[ManualFillCardItem alloc] initWithCreditCard:manualFillCreditCard
                                        contentInjector:self.contentInjector
                                     navigationDelegate:self.navigationDelegate
                                            menuActions:menuActions];
}

- (void)postActionsToConsumer {
  if (!self.consumer) {
    return;
  }
  NSString* manageCreditCardsTitle =
      l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_MANAGE_CREDIT_CARDS);
  __weak __typeof(self) weakSelf = self;
  ManualFillActionItem* manageCreditCardsItem = [[ManualFillActionItem alloc]
      initWithTitle:manageCreditCardsTitle
             action:^{
               base::RecordAction(base::UserMetricsAction(
                   "ManualFallback_CreditCard_OpenManageCreditCard"));
               [weakSelf.navigationDelegate openCardSettings];
             }];
  manageCreditCardsItem.accessibilityIdentifier =
      manual_fill::kManagePaymentMethodsAccessibilityIdentifier;

  NSString* addCreditCardsTitle =
      l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_ADD_PAYMENT_METHOD);

  ManualFillActionItem* addCreditCardsItem = [[ManualFillActionItem alloc]
      initWithTitle:addCreditCardsTitle
             action:^{
               base::RecordAction(base::UserMetricsAction(
                   "ManualFallback_CreditCard_OpenAddCreditCard"));
               [weakSelf.navigationDelegate openAddCreditCard];
             }];
  addCreditCardsItem.accessibilityIdentifier =
      manual_fill::kAddPaymentMethodAccessibilityIdentifier;
  [self.consumer presentActions:@[ addCreditCardsItem, manageCreditCardsItem ]];
}

// Creates an "Edit" and a "Show Details" UIAction to be used with a UIMenu.
- (NSArray<UIAction*>*)createMenuActions {
  ActionFactory* actionFactory = [[ActionFactory alloc]
      initWithScenario:
          kMenuScenarioHistogramAutofillManualFallbackPaymentEntry];
  UIAction* editAction = [actionFactory actionToEditWithBlock:^{
      // TODO(crbug.com/326413453): Handle tap.
  }];

  UIAction* showDetailsAction = [actionFactory actionToShowDetailsWithBlock:^{
      // TODO(crbug.com/326413453): Handle tap.
  }];

  return @[ editAction, showDetailsAction ];
}

#pragma mark - FullCardRequestResultDelegateObserving

- (void)onFullCardRequestSucceeded:(const CreditCard&)card
                         fieldType:(manual_fill::PaymentFieldType)fieldType {
  // Credit card are not shown as 'Secure'.
  ManualFillCreditCard* manualFillCreditCard =
      [[ManualFillCreditCard alloc] initWithCreditCard:card];
  NSString* fillValue;
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableVirtualCards)) {
    switch (fieldType) {
      case PaymentFieldType::kCardNumber:
        fillValue = manualFillCreditCard.number;
        break;
      case PaymentFieldType::kExpirationMonth:
        fillValue = manualFillCreditCard.expirationMonth;
        break;
      case PaymentFieldType::kExpirationYear:
        fillValue = manualFillCreditCard.expirationYear;
        break;
    }
  } else {
    fillValue = manualFillCreditCard.number;
  }

  // Don't replace the locked card with the unlocked one, so the user will
  // have to unlock it again, if needed.
  [self.contentInjector userDidPickContent:fillValue
                             passwordField:NO
                             requiresHTTPS:YES];
}

- (void)onFullCardRequestFailed {
  // This is called on user cancelling, so there's nothing to do here.
}

@end
