// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_card_mediator.h"

#import <vector>

#import "base/i18n/message_formatter.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/browser_autofill_manager.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/card_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/card_list_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/full_card_request_result_delegate_bridge.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_action_cell.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_card_cell.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_content_injector.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_credit_card+CreditCard.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_credit_card.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/registry_controlled_domains/registry_controlled_domain.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/resource/resource_bundle.h"
#import "url/gurl.h"

using autofill::CreditCard;
using manual_fill::PaymentFieldType;

namespace manual_fill {

NSString* const kManagePaymentMethodsAccessibilityIdentifier =
    @"kManagePaymentMethodsAccessibilityIdentifier";
NSString* const kAddPaymentMethodAccessibilityIdentifier =
    @"kAddPaymentMethodAccessibilityIdentifier";

// Returns `true` if overflow menu actions should be made available in the
// manual fill cell of a card with the given `record_type`.
bool ShouldShowMenuActionsInManualFallback(CreditCard::RecordType record_type) {
  switch (record_type) {
    case autofill::CreditCard::RecordType::kLocalCard:
    case autofill::CreditCard::RecordType::kFullServerCard:
    case autofill::CreditCard::RecordType::kMaskedServerCard:
      return IsKeyboardAccessoryUpgradeEnabled();
    case autofill::CreditCard::RecordType::kVirtualCard:
      return NO;
  }
}

}  // namespace manual_fill

@interface ManualFillCardMediator () <PersonalDataManagerObserver>

// All available credit cards.
@property(nonatomic, assign) std::vector<CreditCard*> cards;

@end

@implementation ManualFillCardMediator {
  // Personal data manager to be observed.
  raw_ptr<autofill::PersonalDataManager> _personalDataManager;

  // C++ to ObjC bridge for PersonalDataManagerObserver.
  std::unique_ptr<autofill::PersonalDataManagerObserverBridge>
      _personalDataManagerObserver;

  // Reauthentication Module used for re-authentication.
  ReauthenticationModule* _reauthenticationModule;

  // Indicates whether to show the autofill button for the items.
  BOOL _showAutofillFormButton;
}

- (instancetype)initWithPersonalDataManager:
                    (autofill::PersonalDataManager*)personalDataManager
                     reauthenticationModule:
                         (ReauthenticationModule*)reauthenticationModule
                     showAutofillFormButton:(BOOL)showAutofillFormButton {
  self = [super init];
  if (self) {
    _personalDataManager = personalDataManager;
    _personalDataManagerObserver.reset(
        new autofill::PersonalDataManagerObserverBridge(self));
    _personalDataManager->AddObserver(_personalDataManagerObserver.get());
    _cards = _personalDataManager->payments_data_manager().GetCreditCards();
    _reauthenticationModule = reauthenticationModule;
    _showAutofillFormButton = showAutofillFormButton;
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

- (void)disconnect {
  if (_personalDataManager && _personalDataManagerObserver.get()) {
    _personalDataManager->RemoveObserver(_personalDataManagerObserver.get());
    _personalDataManagerObserver.reset();
  }
}

#pragma mark - PersonalDataManagerObserver

- (void)onPersonalDataChanged {
  self.cards =
      _personalDataManager->payments_data_manager().GetCreditCardsToSuggest();
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

  int cardCount = self.cards.size();
  NSMutableArray* cardItems =
      [[NSMutableArray alloc] initWithCapacity:cardCount];
  for (int i = 0; i < cardCount; i++) {
    CreditCard* card = self.cards[i];
    NSString* cellIndexAccessibilityLabel = base::SysUTF16ToNSString(
        base::i18n::MessageFormatter::FormatWithNamedArgs(
            l10n_util::GetStringUTF16(
                IDS_IOS_MANUAL_FALLBACK_PAYMENT_CELL_INDEX),
            "count", cardCount, "position", i + 1));

    // If this card is enrolled to have a virtual card, create the virtual card
    // and order it directly before the original card.
    if (base::FeatureList::IsEnabled(
            autofill::features::kAutofillEnableVirtualCards) &&
        card->virtual_card_enrollment_state() ==
            CreditCard::VirtualCardEnrollmentState::kEnrolled) {
      CreditCard virtualCard = CreditCard::CreateVirtualCard(*card);
      [cardItems addObject:[self createManualFillCardItemForCard:&virtualCard
                                     cellIndexAccessibilityLabel:
                                         cellIndexAccessibilityLabel]];
    }
    [cardItems addObject:[self createManualFillCardItemForCard:card
                                   cellIndexAccessibilityLabel:
                                       cellIndexAccessibilityLabel]];
  }

  [self.consumer presentCards:cardItems];
}

// Creates a ManualFillCardItem for the given `card`.
- (ManualFillCardItem*)createManualFillCardItemForCard:(CreditCard*)card
                           cellIndexAccessibilityLabel:
                               (NSString*)cellIndexAccessibilityLabel {
  ManualFillCreditCard* manualFillCreditCard = [[ManualFillCreditCard alloc]
      initWithCreditCard:*card
                    icon:[self iconForCreditCard:card]];
  NSArray<UIAction*>* menuActions =
      manual_fill::ShouldShowMenuActionsInManualFallback(card->record_type())
          ? [self createMenuActionsForCard:card]
          : @[];

  return
      [[ManualFillCardItem alloc] initWithCreditCard:manualFillCreditCard
                                     contentInjector:self.contentInjector
                                  navigationDelegate:self.navigationDelegate
                                         menuActions:menuActions
                         cellIndexAccessibilityLabel:cellIndexAccessibilityLabel
                              showAutofillFormButton:_showAutofillFormButton];
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
- (NSArray<UIAction*>*)createMenuActionsForCard:(const CreditCard*)card {
  ActionFactory* actionFactory = [[ActionFactory alloc]
      initWithScenario:
          kMenuScenarioHistogramAutofillManualFallbackPaymentEntry];

  __weak __typeof(self) weakSelf = self;
  UIAction* editAction = [actionFactory actionToEditWithBlock:^{
    [weakSelf.navigationDelegate openCardDetails:card inEditMode:YES];
  }];

  UIAction* showDetailsAction = [actionFactory actionToShowDetailsWithBlock:^{
    [weakSelf.navigationDelegate openCardDetails:card inEditMode:NO];
  }];

  return @[ editAction, showDetailsAction ];
}

// Returns the icon associated with the provided credit card.
- (UIImage*)iconForCreditCard:(const autofill::CreditCard*)creditCard {
  // Check if custom card art is available.
  GURL cardArtURL =
      _personalDataManager->payments_data_manager().GetCardArtURL(*creditCard);
  if (IsKeyboardAccessoryUpgradeEnabled() && !cardArtURL.is_empty() &&
      cardArtURL.is_valid()) {
    gfx::Image* image = _personalDataManager->payments_data_manager()
                            .GetCreditCardArtImageForUrl(cardArtURL);
    if (image) {
      return image->ToUIImage();
    }
  }

  // Otherwise, try to get the default card icon
  autofill::Suggestion::Icon icon = creditCard->CardIconForAutofillSuggestion();
  return icon == autofill::Suggestion::Icon::kNoIcon
             ? nil
             : ui::ResourceBundle::GetSharedInstance()
                   .GetNativeImageNamed(
                       autofill::CreditCard::IconResourceId(icon))
                   .ToUIImage();
}

#pragma mark - FullCardRequestResultDelegateObserving

- (void)onFullCardRequestSucceeded:(const CreditCard&)card
                         fieldType:(manual_fill::PaymentFieldType)fieldType {
  // Credit card are not shown as 'Secure'.
  ManualFillCreditCard* manualFillCreditCard = [[ManualFillCreditCard alloc]
      initWithCreditCard:card
                    icon:[self iconForCreditCard:&card]];
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
      case PaymentFieldType::kCVC:
        fillValue = manualFillCreditCard.CVC;
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
