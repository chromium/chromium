// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_mediator.h"

#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/credential_provider_promo/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CredentialProviderPromoMediator ()

// The main consumer for this mediator.
@property(nonatomic, weak) id<CredentialProviderPromoConsumer> consumer;

// The PrefService used by this mediator.
@property(nonatomic, assign) PrefService* prefService;

@end

@implementation CredentialProviderPromoMediator

- (instancetype)initWithConsumer:(id<CredentialProviderPromoConsumer>)consumer
                     prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _consumer = consumer;
    _prefService = prefService;
  }
  return self;
}

- (BOOL)canShowCredentialProviderPromo {
  // TODO(crbug.com/1392116): check for user action and impression counts
  return IsCredentialProviderExtensionPromoEnabled() &&
         !password_manager_util::IsCredentialProviderEnabledOnStartup(
             self.prefService);
}

- (void)configureConsumerWithTrigger:(CredentialProviderPromoTrigger)trigger {
  // TODO(crbug.com/1392116): configure view controller
}

@end
