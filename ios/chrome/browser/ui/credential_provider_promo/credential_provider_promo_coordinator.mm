// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_coordinator.h"

#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/credential_provider_promo_commands.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CredentialProviderPromoCoordinator ()

@end

@implementation CredentialProviderPromoCoordinator

- (void)start {
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(CredentialProviderPromoCommands)];
}

- (void)stop {
  [super stop];
  [self.browser->GetCommandDispatcher()
      stopDispatchingForProtocol:@protocol(CredentialProviderPromoCommands)];
}

#pragma mark - CredentialProviderPromoCommands

- (void)showCredentialProviderPromoWithTrigger:
    (CredentialProviderPromoTrigger)trigger {
}

@end
