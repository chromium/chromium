// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/card_coordinator.h"

#import "base/memory/raw_ptr.h"
#import "base/memory/ref_counted.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/card_list_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/card_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_card_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_full_card_requester.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_event.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface CardCoordinator () <CardListDelegate> {
  // Opening links on the enrollment bottom sheet is delegated to this
  // dispatcher.
  __weak id<ApplicationCommands> _dispatcher;

  // Reauthentication Module used for re-authentication.
  ReauthenticationModule* _reauthenticationModule;

  // PersonalDataManager
  raw_ptr<autofill::PersonalDataManager> _personalDataManager;
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
                       browser:(Browser*)browser
              injectionHandler:(ManualFillInjectionHandler*)injectionHandler
        reauthenticationModule:(ReauthenticationModule*)reauthenticationModule
        showAutofillFormButton:(BOOL)showAutofillFormButton {
  self = [super initWithBaseViewController:viewController
                                   browser:browser
                          injectionHandler:injectionHandler];
  if (self) {
    _cardViewController = [[CardViewController alloc] init];
    _reauthenticationModule = reauthenticationModule;

    // Service must use regular profile, even if the Browser has an
    // OTR profile.
    _personalDataManager = autofill::PersonalDataManagerFactory::GetForProfile(
        super.browser->GetProfile()->GetOriginalProfile());
    CHECK(_personalDataManager);

    _cardMediator = [[ManualFillCardMediator alloc]
        initWithPersonalDataManager:_personalDataManager
             reauthenticationModule:_reauthenticationModule
             showAutofillFormButton:showAutofillFormButton];
    _cardMediator.navigationDelegate = self;
    _cardMediator.contentInjector = super.injectionHandler;
    _cardMediator.consumer = _cardViewController;

    _cardRequester = [[ManualFillFullCardRequester alloc]
        initWithBrowserState:super.browser->GetProfile()->GetOriginalProfile()
                webStateList:super.browser->GetWebStateList()
              resultDelegate:_cardMediator];
    _dispatcher = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                     ApplicationCommands);
  }
  return self;
}

- (void)stop {
  [_cardMediator disconnect];
  _cardMediator = nil;

  _cardViewController = nil;
}

#pragma mark - FallbackCoordinator

- (UIViewController*)viewController {
  return self.cardViewController;
}

#pragma mark - CardListDelegate

- (void)openAddCreditCard {
  __weak __typeof(self) weakSelf = self;
  [self dismissIfNecessaryThenDoCompletion:^{
    [weakSelf.delegate cardCoordinatorDidTriggerOpenAddCreditCard:weakSelf];
  }];
}

- (void)openCardDetails:(autofill::CreditCard)card inEditMode:(BOOL)editMode {
  CHECK(_personalDataManager);
  if (card.record_type() == autofill::CreditCard::RecordType::kLocalCard &&
      _personalDataManager->payments_data_manager()
          .IsPaymentMethodsMandatoryReauthEnabled() &&
      [_reauthenticationModule canAttemptReauth]) {
    NSString* reason = l10n_util::GetNSString(
        IDS_PAYMENTS_AUTOFILL_SETTINGS_EDIT_MANDATORY_REAUTH);

    __weak __typeof(self) weakSelf = self;
    auto callback = base::BindOnce(
        [](__weak __typeof(self) weak_self, autofill::CreditCard card,
           BOOL edit_mode, ReauthenticationResult result) {
          if (result != ReauthenticationResult::kFailure) {
            [weak_self didTriggerOpenCardDetails:std::move(card)
                                      inEditMode:edit_mode];
          }
        },
        weakSelf, std::move(card), editMode);
    [_reauthenticationModule
        attemptReauthWithLocalizedReason:reason
                    canReusePreviousAuth:YES
                                 handler:base::CallbackToBlock(
                                             std::move(callback))];
    return;
  }

  [self didTriggerOpenCardDetails:card inEditMode:editMode];
}

- (void)openCardSettings {
  __weak __typeof(self) weakSelf = self;
  [self dismissIfNecessaryThenDoCompletion:^{
    [weakSelf.delegate cardCoordinatorDidTriggerOpenCardSettings:weakSelf];
  }];
}

- (void)requestFullCreditCard:(ManualFillCreditCard*)card
                    fieldType:(manual_fill::PaymentFieldType)fieldType {
  __weak __typeof(self) weakSelf = self;
  [self dismissIfNecessaryThenDoCompletion:^{
    std::optional<const autofill::CreditCard> autofillCreditCard =
        [weakSelf.cardMediator findCreditCardfromGUID:card.GUID];
    if (!autofillCreditCard) {
      return;
    }
    [weakSelf.cardRequester requestFullCreditCard:*autofillCreditCard
                           withBaseViewController:weakSelf.baseViewController
                                       recordType:card.recordType
                                        fieldType:fieldType];
  }];
}

- (void)openURL:(CrURL*)url withTitle:(NSString*)title {
  [_dispatcher
      openURLInNewTab:[OpenNewTabCommand
                          commandWithURLFromChrome:url.gurl
                                       inIncognito:self.browser->GetProfile()
                                                       ->IsOffTheRecord()]];
}

#pragma mark - Private

- (void)didTriggerOpenCardDetails:(autofill::CreditCard)card
                       inEditMode:(BOOL)editMode {
  __weak __typeof(self) weakSelf = self;
  auto callback = base::BindOnce(
      [](__weak __typeof(self) weak_self, autofill::CreditCard card,
         BOOL edit_mode) {
        [weak_self.delegate cardCoordinator:weak_self
                  didTriggerOpenCardDetails:std::move(card)
                                 inEditMode:edit_mode];
      },
      weakSelf, std::move(card), editMode);
  [self dismissIfNecessaryThenDoCompletion:base::CallbackToBlock(
                                               std::move(callback))];
}

@end
