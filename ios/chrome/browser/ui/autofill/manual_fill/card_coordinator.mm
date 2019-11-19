// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/card_coordinator.h"

#include "base/memory/ref_counted.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_list_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/full_card_requester.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/web/public/js_messaging/web_frame.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CardCoordinator () <CardListDelegate, PersonalDataManagerObserver> {
  // Personal data manager to be observed.
  autofill::PersonalDataManager* _personalDataManager;

  // C++ to ObjC bridge for PersonalDataManagerObserver.
  std::unique_ptr<autofill::PersonalDataManagerObserverBridge>
      _personalDataManagerObserver;
}

// The view controller presented above the keyboard where the user can select
// one of their cards.
@property(nonatomic, strong) CardViewController* cardViewController;

// Fetches and filters the cards for the view controller.
@property(nonatomic, strong) ManualFillCardMediator* cardMediator;

// Requesters to unlock (through user CVC input) of server side credit cards.
@property(nonatomic, strong) ManualFillFullCardRequester* cardRequester;

@end

@implementation CardCoordinator

// Property tagged dynamic because it overrides super class delegate with and
// extension of the super delegate type (i.e. CardCoordinatorDelegate extends
// FallbackCoordinatorDelegate)
@dynamic delegate;

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                  browserState:(ios::ChromeBrowserState*)browserState
                  webStateList:(WebStateList*)webStateList
              injectionHandler:(ManualFillInjectionHandler*)injectionHandler
                    dispatcher:(id<BrowserCoordinatorCommands>)dispatcher {
  self = [super initWithBaseViewController:viewController
                              browserState:browserState
                          injectionHandler:injectionHandler];
  if (self) {
    _cardViewController = [[CardViewController alloc] init];
    _cardViewController.contentInsetsAlwaysEqualToSafeArea = YES;

    _personalDataManager =
        autofill::PersonalDataManagerFactory::GetForBrowserState(browserState);
    DCHECK(_personalDataManager);

    _personalDataManagerObserver.reset(
        new autofill::PersonalDataManagerObserverBridge(self));
    _personalDataManager->AddObserver(_personalDataManagerObserver.get());

    std::vector<autofill::CreditCard*> cards =
        _personalDataManager->GetCreditCards();

    _cardMediator = [[ManualFillCardMediator alloc] initWithCards:cards
                                                       dispatcher:dispatcher];
    _cardMediator.navigationDelegate = self;
    _cardMediator.contentInjector = self.injectionHandler;
    _cardMediator.consumer = _cardViewController;

    _cardRequester = [[ManualFillFullCardRequester alloc]
        initWithBrowserState:browserState
                webStateList:webStateList
              resultDelegate:_cardMediator];
  }
  return self;
}

- (void)dealloc {
  if (_personalDataManager) {
    _personalDataManager->RemoveObserver(_personalDataManagerObserver.get());
  }
}

#pragma mark - FallbackCoordinator

- (UIViewController*)viewController {
  return self.cardViewController;
}

#pragma mark - CardListDelegate

- (void)openCardSettings {
  __weak id<CardCoordinatorDelegate> delegate = self.delegate;
  [self dismissIfNecessaryThenDoCompletion:^{
    [delegate openCardSettings];
    if (IsIPadIdiom()) {
      // Settings close the popover but don't send a message to reopen it.
      [delegate fallbackCoordinatorDidDismissPopover:self];
    }
  }];
}

- (void)requestFullCreditCard:(ManualFillCreditCard*)card {
  __weak __typeof(self) weakSelf = self;
  __weak ManualFillCreditCard* weakCard = card;
  [self dismissIfNecessaryThenDoCompletion:^{
    if (!weakSelf)
      return;
    const autofill::CreditCard* autofillCreditCard =
        [weakSelf.cardMediator findCreditCardfromGUID:weakCard.GUID];
    if (!autofillCreditCard)
      return;
    [weakSelf.cardRequester requestFullCreditCard:*autofillCreditCard
                           withBaseViewController:weakSelf.baseViewController];
  }];
}

#pragma mark - PersonalDataManagerObserver

- (void)onPersonalDataChanged {
  std::vector<autofill::CreditCard*> cards =
      _personalDataManager->GetCreditCardsToSuggest(true);

  [self.cardMediator reloadWithCards:cards];
}

@end
