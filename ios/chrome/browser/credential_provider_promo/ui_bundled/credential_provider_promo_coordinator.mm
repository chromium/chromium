// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_coordinator.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_constants.h"
#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_mediator.h"
#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_metrics.h"
#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_view_controller.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "ios/chrome/browser/shared/ui/util/top_view_controller.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_ui_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/public/provider/chrome/browser/password_auto_fill/password_auto_fill_api.h"

@interface CredentialProviderPromoCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate>

// Main mediator for this coordinator.
@property(nonatomic, strong) CredentialProviderPromoMediator* mediator;

// Main view controller for this coordinator.
@property(nonatomic, strong)
    CredentialProviderPromoViewController* viewController;

// Represents how the feature was triggered.
@property(nonatomic, assign) CredentialProviderPromoTrigger trigger;

// Indicates whether the 'first step' or 'learn more' version of the promo is
// being presented.
@property(nonatomic, assign) CredentialProviderPromoContext promoContext;

// Indicates whether the user has already seen the promo in the current
// app session.
@property(nonatomic, assign) BOOL promoSeenInCurrentSession;

@end

using credential_provider_promo::IOSCredentialProviderPromoAction;

@implementation CredentialProviderPromoCoordinator

- (void)start {
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(CredentialProviderPromoCommands)];
  PromosManager* promosManager =
      PromosManagerFactory::GetForProfile(self.browser->GetProfile());
  self.mediator = [[CredentialProviderPromoMediator alloc]
      initWithPromosManager:promosManager];
}

- (void)stop {
  [super stop];
  [self.browser->GetCommandDispatcher()
      stopDispatchingForProtocol:@protocol(CredentialProviderPromoCommands)];
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.mediator = nil;
  self.viewController = nil;
}

#pragma mark - CredentialProviderPromoCommands

- (void)showCredentialProviderPromoWithTrigger:
    (CredentialProviderPromoTrigger)trigger {
  // If the user is not eligible to be shown the promo, or the VC is already
  // being presented, return early.
  if (![self.mediator
          canShowCredentialProviderPromoWithTrigger:trigger
                                          promoSeen:
                                              self.promoSeenInCurrentSession] ||
      [self.viewController isBeingPresented]) {
    return;
  }
  self.viewController = [[CredentialProviderPromoViewController alloc] init];
  self.mediator.consumer = self.viewController;
  self.mediator.tracker = feature_engagement::TrackerFactory::GetForProfile(
      self.browser->GetProfile());
  self.viewController.actionHandler = self;
  self.viewController.presentationController.delegate = self;
  if (trigger == CredentialProviderPromoTrigger::SetUpList) {
    // If this is coming from the SetUpList, force to go directly to LearnMore.
    self.promoContext = CredentialProviderPromoContext::kLearnMore;
  } else {
    self.promoContext = CredentialProviderPromoContext::kFirstStep;
  }
  [self.mediator configureConsumerWithTrigger:trigger
                                      context:self.promoContext];
  self.trigger = trigger;
  UIViewController* topViewController =
      top_view_controller::TopPresentedViewControllerFrom(
          self.baseViewController);
  [topViewController presentViewController:self.viewController
                                  animated:YES
                                completion:nil];
  self.promoSeenInCurrentSession = YES;

  credential_provider_promo::RecordImpression(
      [self.mediator promoOriginalSource],
      self.trigger == CredentialProviderPromoTrigger::RemindMeLater);
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self hidePromo];
  if (self.promoContext == CredentialProviderPromoContext::kFirstStep) {
    [self presentLearnMore];
    [self recordAction:IOSCredentialProviderPromoAction::kLearnMore];
  } else {
    // Open iOS settings.
    ios::provider::PasswordsInOtherAppsOpensSettings();
    [self recordAction:IOSCredentialProviderPromoAction::kGoToSettings];
    [self promoWasDismissed];
  }
}

- (void)confirmationAlertSecondaryAction {
  [self hidePromo];

  GetApplicationContext()->GetLocalState()->SetBoolean(
      prefs::kIosCredentialProviderPromoStopPromo, true);

  [self recordAction:IOSCredentialProviderPromoAction::kNo];
  [self promoWasDismissed];
}

- (void)confirmationAlertTertiaryAction {
  [self hidePromo];
  [self.mediator registerPromoWithPromosManager];
  [self recordAction:IOSCredentialProviderPromoAction::kRemindMeLater];
  [self promoWasDismissed];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self promoWasDismissed];
}

#pragma mark - Private

// Presents the 'learn more' step of the feature.
- (void)presentLearnMore {
  self.viewController = [[CredentialProviderPromoViewController alloc] init];
  self.viewController.actionHandler = self;
  self.viewController.presentationController.delegate = self;
  self.mediator.consumer = self.viewController;
  self.promoContext = CredentialProviderPromoContext::kLearnMore;
  [self.mediator configureConsumerWithTrigger:self.trigger
                                      context:self.promoContext];
  UIViewController* topViewController =
      top_view_controller::TopPresentedViewControllerFrom(
          self.baseViewController);
  [topViewController presentViewController:self.viewController
                                  animated:YES
                                completion:nil];
}

// Dismisses the feature.
- (void)hidePromo {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
}

// Does any clean up for when the promo is fully dismissed.
- (void)promoWasDismissed {
  if (self.trigger == CredentialProviderPromoTrigger::RemindMeLater) {
    [self.promosUIHandler promoWasDismissed];
  }
}

// Help function for metrics.
- (void)recordAction:(IOSCredentialProviderPromoAction)action {
  credential_provider_promo::RecordAction(
      [self.mediator promoOriginalSource],
      self.trigger == CredentialProviderPromoTrigger::RemindMeLater, action);
  GetApplicationContext()->GetLocalState()->SetInteger(
      prefs::kIosCredentialProviderPromoLastActionTaken,
      static_cast<int>(action));
}

@end
