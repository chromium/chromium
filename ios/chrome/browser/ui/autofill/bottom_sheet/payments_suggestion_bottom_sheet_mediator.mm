// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_consumer.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_data.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Structure which contains all the required information to display about a
// credit card.
@interface PaymentsSuggestionBottomSheetCreditCardInfo
    : NSObject <PaymentsSuggestionBottomSheetData>

- (instancetype)initWithCreditCard:(const autofill::CreditCard*)creditCard
                              icon:(UIImage*)icon;

@property(nonatomic, assign) const autofill::CreditCard* creditCard;
@property(nonatomic, strong) UIImage* icon;

@end

@implementation PaymentsSuggestionBottomSheetCreditCardInfo

- (instancetype)initWithCreditCard:(const autofill::CreditCard*)creditCard
                              icon:(UIImage*)icon {
  if (self = [super init]) {
    _creditCard = creditCard;
    _icon = icon;
  }
  return self;
}

@end

@interface PaymentsSuggestionBottomSheetMediator () <CRWWebStateObserver,
                                                     WebStateListObserving>

@end

@implementation PaymentsSuggestionBottomSheetMediator {
  // The WebStateList observed by this mediator and the observer bridge.
  raw_ptr<WebStateList> _webStateList;
  std::unique_ptr<web::WebStateObserverBridge> _observer;
  std::unique_ptr<ActiveWebStateObservationForwarder> _forwarder;

  // Personal Data Manager from which we can get Credit Card information.
  raw_ptr<autofill::PersonalDataManager> _personalDataManager;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                 personalDataManager:
                     (autofill::PersonalDataManager*)personalDataManager {
  if (self = [super init]) {
    _webStateList = webStateList;
    _personalDataManager = personalDataManager;

    // Create and register the observers.
    _observer = std::make_unique<web::WebStateObserverBridge>(self);
    _forwarder = std::make_unique<ActiveWebStateObservationForwarder>(
        webStateList, _observer.get());
  }
  return self;
}

- (void)disconnect {
  _forwarder = nullptr;
  _observer = nullptr;
  _webStateList = nullptr;
}

#pragma mark - Accessors

- (void)setConsumer:(id<PaymentsSuggestionBottomSheetConsumer>)consumer {
  _consumer = consumer;

  if (!_consumer || !_personalDataManager) {
    return;
  }

  const auto& creditCards = _personalDataManager->GetCreditCardsToSuggest();
  if (creditCards.empty()) {
    return;
  }

  NSMutableArray<id<PaymentsSuggestionBottomSheetData>>* creditCardData =
      [[NSMutableArray alloc] initWithCapacity:creditCards.size()];
  for (const auto* creditCard : creditCards) {
    CHECK(creditCard);
    [creditCardData
        addObject:[[PaymentsSuggestionBottomSheetCreditCardInfo alloc]
                      initWithCreditCard:creditCard
                                    icon:[self iconForCreditCard:creditCard]]];
  }

  [consumer setCreditCardData:creditCardData];
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                    selection:(const WebStateSelection&)selection {
  DCHECK_EQ(_webStateList, webStateList);
  switch (change.type()) {
    case WebStateListChange::Type::kSelectionOnly:
      // TODO(crbug.com/1442546): Move the implementation from
      // webStateList:didChangeActiveWebState:oldWebState:atIndex:reason to
      // here. Note that here is reachable only when `reason` ==
      // ActiveWebStateChangeReason::Activated.
      break;
    case WebStateListChange::Type::kDetach:
      // Do nothing when a WebState is detached.
      break;
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      if (selection.index == webStateList->active_index()) {
        [self onWebStateChange];
      }
      break;
    }
    case WebStateListChange::Type::kInsert:
      // Do nothing when a new WebState is inserted.
      break;
  }
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  DCHECK_EQ(_webStateList, webStateList);
  [self onWebStateChange];
}

- (void)webStateListDestroyed:(WebStateList*)webStateList {
  DCHECK_EQ(webStateList, _webStateList);
  _forwarder = nullptr;
  _observer = nullptr;
  _webStateList = nullptr;
  [self onWebStateChange];
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  [self onWebStateChange];
}

- (void)renderProcessGoneForWebState:(web::WebState*)webState {
  [self onWebStateChange];
}

#pragma mark - Private

- (void)onWebStateChange {
  // TODO(crbug.com/1450214): Handle changes in web state
}

// Returns the icon associated with the provided credit card.
- (UIImage*)iconForCreditCard:(const autofill::CreditCard*)creditCard {
  // Check if custom card art is available.
  GURL cardArtURL = _personalDataManager->GetCardArtURL(*creditCard);
  if (!cardArtURL.is_empty() && cardArtURL.is_valid()) {
    gfx::Image* image =
        _personalDataManager->GetCreditCardArtImageForUrl(cardArtURL);
    if (image) {
      return image->ToUIImage();
    }
  }

  // Otherwise, try to get the default card icon
  std::string icon = creditCard->CardIconStringForAutofillSuggestion();
  return icon.empty() ? nil
                      : ui::ResourceBundle::GetSharedInstance()
                            .GetNativeImageNamed(
                                autofill::CreditCard::IconResourceId(icon))
                            .ToUIImage();
}

@end
