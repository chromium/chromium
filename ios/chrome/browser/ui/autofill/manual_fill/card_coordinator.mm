// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/card_coordinator.h"

#import "base/memory/ref_counted.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#import "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_list_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_card_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_full_card_requester.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_injection_handler.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ui/base/device_form_factor.h"

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

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                          injectionHandler:
                              (ManualFillInjectionHandler*)injectionHandler {
  self = [super initWithBaseViewController:viewController
                                   browser:browser
                          injectionHandler:injectionHandler];
  if (self) {
    _cardViewController = [[CardViewController alloc] init];

    // Service must use regular browser state, even if the Browser has an
    // OTR browser state.
    _personalDataManager =
        autofill::PersonalDataManagerFactory::GetForBrowserState(
            super.browser->GetBrowserState()->GetOriginalChromeBrowserState());
    DCHECK(_personalDataManager);

    _personalDataManagerObserver.reset(
        new autofill::PersonalDataManagerObserverBridge(self));
    _personalDataManager->AddObserver(_personalDataManagerObserver.get());

    std::vector<autofill::CreditCard*> cards =
        _personalDataManager->GetCreditCards();

    _cardMediator = [[ManualFillCardMediator alloc] initWithCards:cards];
    _cardMediator.navigationDelegate = self;
    _cardMediator.contentInjector = super.injectionHandler;
    _cardMediator.consumer = _cardViewController;

    _cardRequester = [[ManualFillFullCardRequester alloc]
        initWithBrowserState:super.browser->GetBrowserState()
                                 ->GetOriginalChromeBrowserState()
                webStateList:super.browser->GetWebStateList()
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
  __weak id<CardCoordinatorDelegate> weakDelegate = self.delegate;
  [self dismissIfNecessaryThenDoCompletion:^{
    [weakDelegate openCardSettings];
  }];
}

- (void)openAddCreditCard {
  __weak id<CardCoordinatorDelegate> weakDelegate = self.delegate;
  [self dismissIfNecessaryThenDoCompletion:^{
    [weakDelegate openAddCreditCard];
  }];
}

- (void)requestFullCreditCard:(ManualFillCreditCard*)card {
  __weak __typeof(self) weakSelf = self;
  [self dismissIfNecessaryThenDoCompletion:^{
    if (!weakSelf)
      return;
    const autofill::CreditCard* autofillCreditCard =
        [weakSelf.cardMediator findCreditCardfromGUID:card.GUID];
    if (!autofillCreditCard)
      return;
    [weakSelf.cardRequester requestFullCreditCard:*autofillCreditCard
                           withBaseViewController:weakSelf.baseViewController];
  }];
}

#pragma mark - PersonalDataManagerObserver

- (void)onPersonalDataChanged {
  std::vector<autofill::CreditCard*> cards =
      _personalDataManager->GetCreditCardsToSuggest();

  [self.cardMediator reloadWithCards:cards];
}

@end
