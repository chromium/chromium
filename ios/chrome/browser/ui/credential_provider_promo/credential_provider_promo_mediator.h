// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CREDENTIAL_PROVIDER_PROMO_CREDENTIAL_PROVIDER_PROMO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_CREDENTIAL_PROVIDER_PROMO_CREDENTIAL_PROVIDER_PROMO_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/ui/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_consumer.h"

// Manages the state and interactions of the CredentialProviderPromoConsumer.
@interface CredentialProviderPromoMediator : NSObject

// Designated initializer. Initializes the mediator with the consumer and
// PrefService.
- (instancetype)initWithConsumer:(id<CredentialProviderPromoConsumer>)consumer
                     prefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithCoder NS_UNAVAILABLE;

// Returns YES if the user conditions are met to present the Credential
// Provider Promo.
- (BOOL)canShowCredentialProviderPromo;

// Configures the consumer.
- (void)configureConsumerWithTrigger:(CredentialProviderPromoTrigger)trigger;

@end

#endif  // IOS_CHROME_BROWSER_UI_CREDENTIAL_PROVIDER_PROMO_CREDENTIAL_PROVIDER_PROMO_MEDIATOR_H_
