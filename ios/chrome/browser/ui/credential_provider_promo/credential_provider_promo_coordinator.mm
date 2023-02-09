// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_coordinator.h"

#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_constants.h"
#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_mediator.h"
#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_view_controller.h"
#import "ios/chrome/browser/ui/util/top_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CredentialProviderPromoCoordinator () <
    ConfirmationAlertActionHandler>

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

@end

@implementation CredentialProviderPromoCoordinator

- (void)start {
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(CredentialProviderPromoCommands)];
  self.mediator = [[CredentialProviderPromoMediator alloc]
      initWithPromosManager:GetApplicationContext()->GetPromosManager()
                prefService:self.browser->GetBrowserState()->GetPrefs()];
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
  if (![self.mediator canShowCredentialProviderPromo] ||
      [self.viewController isBeingPresented]) {
    return;
  }
  self.viewController = [[CredentialProviderPromoViewController alloc] init];
  self.mediator.consumer = self.viewController;
  self.viewController.actionHandler = self;
  self.promoContext = CredentialProviderPromoContext::kFirstStep;
  [self.mediator configureConsumerWithTrigger:trigger
                                      context:self.promoContext];
  self.trigger = trigger;
  UIViewController* topViewController =
      top_view_controller::TopPresentedViewControllerFrom(
          self.baseViewController);
  [topViewController presentViewController:self.viewController
                                  animated:YES
                                completion:nil];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self hidePromo];
  if (self.promoContext == CredentialProviderPromoContext::kFirstStep) {
    [self presentLearnMore];
  } else {
    // Open iOS settings.
    [[UIApplication sharedApplication]
                  openURL:[NSURL
                              URLWithString:UIApplicationOpenSettingsURLString]
                  options:{}
        completionHandler:nil];
  }
}

- (void)confirmationAlertSecondaryAction {
  [self hidePromo];
}

- (void)confirmationAlertTertiaryAction {
  [self hidePromo];
  [self.mediator registerPromoWithPromosManager];
}

#pragma mark - Private

// Presents the 'learn more' step of the feature.
- (void)presentLearnMore {
  self.viewController = [[CredentialProviderPromoViewController alloc] init];
  self.viewController.actionHandler = self;
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

@end
