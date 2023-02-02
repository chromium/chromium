// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_coordinator.h"

#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_mediator.h"
#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CredentialProviderPromoCoordinator ()

// Main mediator for this coordinator.
@property(nonatomic, strong) CredentialProviderPromoMediator* mediator;

// Main view controller for this coordinator.
@property(nonatomic, strong)
    CredentialProviderPromoViewController* viewController;

@end

@implementation CredentialProviderPromoCoordinator

- (void)start {
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(CredentialProviderPromoCommands)];
  self.viewController = [[CredentialProviderPromoViewController alloc] init];
  self.mediator = [[CredentialProviderPromoMediator alloc]
      initWithConsumer:self.viewController
           prefService:self.browser->GetBrowserState()->GetPrefs()];
}

- (void)stop {
  [super stop];
  [self.browser->GetCommandDispatcher()
      stopDispatchingForProtocol:@protocol(CredentialProviderPromoCommands)];
  self.mediator = nil;
  self.viewController = nil;
}

#pragma mark - CredentialProviderPromoCommands

- (void)showCredentialProviderPromoWithTrigger:
    (CredentialProviderPromoTrigger)trigger {
  if ([self.mediator canShowCredentialProviderPromo]) {
    [self.mediator configureConsumerWithTrigger:trigger];
    [self.baseViewController presentViewController:self.viewController
                                          animated:YES
                                        completion:nil];
  }
}

@end
