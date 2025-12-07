// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_coordinator.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_constants.h"
#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_mediator.h"
#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_metrics.h"
#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_view_controller.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/promos_manager/ui_bundled/promos_manager_ui_handler.h"
#import "ios/chrome/browser/shared/coordinator/utils/credential_provider_settings_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/top_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

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
      PromosManagerFactory::GetForProfile(self.profile);
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
  self.mediator.tracker =
      feature_engagement::TrackerFactory::GetForProfile(self.profile);
  self.viewController.actionHandler = self;
  self.viewController.presentationController.delegate = self;
  self.promoContext = [self promoContextFromTrigger:trigger];
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

  GetApplicationContext()->GetLocalState()->SetTime(
      prefs::kIosCredentialProviderPromoDisplayTime, base::Time::Now());
  credential_provider_promo::RecordImpression(
      [self.mediator promoOriginalSource],
      self.trigger == CredentialProviderPromoTrigger::RemindMeLater);
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self hidePromo];
  if (self.promoContext == CredentialProviderPromoContext::kFirstStep) {
    if (@available(iOS 18.0, *)) {
      // Show the prompt to allow the app to be turned on as a credential
      // provider.
      __weak __typeof(self) weakSelf = self;
      [ASSettingsHelper
          requestToTurnOnCredentialProviderExtensionWithCompletionHandler:^(
              BOOL appWasEnabledForAutoFill) {
            [weakSelf recordTurnOnCredentialProviderExtensionPromptOutcome:
                          appWasEnabledForAutoFill];
          }];
      [self recordAction:IOSCredentialProviderPromoAction::kTurnOnAutofill];
      return;
    }

    // Show the screen informing the user on how they can set the app as a
    // credential provider.
    [self presentLearnMore];
    [self recordAction:IOSCredentialProviderPromoAction::kLearnMore];
  } else {
    [self openIOSCredentialProviderSettings];
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
  // The 'learn more' step shouldn't be presented on iOS 18+.
  if (@available(iOS 18.0, *)) {
    NOTREACHED();
  }

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

// Records whether the user has accepted the in-app prompt to set the app as a
// credential provider.
- (void)recordTurnOnCredentialProviderExtensionPromptOutcome:(BOOL)outcome {
  RecordTurnOnCredentialProviderExtensionPromptOutcome(
      TurnOnCredentialProviderExtensionPromptSource::
          kCredentialProviderExtensionPromo,
      outcome);
}

// Opens the iOS credential provider settings. Delegates this task to
// `settingsOpenerDelegate` when valid.
- (void)openIOSCredentialProviderSettings {
  if (self.settingsOpenerDelegate) {
    [self.settingsOpenerDelegate
        credentialProviderPromoCoordinatorOpenIOSCredentialProviderSettings:
            self];
    return;
  }
  OpenIOSCredentialProviderSettings();
}

// Returns the promo context for the given trigger. For SetUpList the first
// step is skipped because some context is already provided in the SetUpList
// item's description. But on iOS 18, the first step allows the user to enable
// the CPE directly in-app, so this is preferred.
- (CredentialProviderPromoContext)promoContextFromTrigger:
    (CredentialProviderPromoTrigger)trigger {
  if (trigger == CredentialProviderPromoTrigger::SetUpList) {
    if (@available(iOS 18.0, *)) {
      if (IsIOSExpandedTipsEnabled()) {
        // Go to the first step, which allows enabling CPE in-app.
        return CredentialProviderPromoContext::kFirstStep;
      }
    }
    return CredentialProviderPromoContext::kLearnMore;
  }
  return CredentialProviderPromoContext::kFirstStep;
}

@end
