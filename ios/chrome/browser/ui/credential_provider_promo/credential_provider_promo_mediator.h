// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CREDENTIAL_PROVIDER_PROMO_CREDENTIAL_PROVIDER_PROMO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_CREDENTIAL_PROVIDER_PROMO_CREDENTIAL_PROVIDER_PROMO_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_constants.h"

@protocol CredentialProviderPromoConsumer;

class PrefService;
class PromosManager;

// Manages the state and interactions of the CredentialProviderPromoConsumer.
@interface CredentialProviderPromoMediator : NSObject

// The main consumer for this mediator.
@property(nonatomic, weak) id<CredentialProviderPromoConsumer> consumer;

// Designated initializer. Initializes the mediator with the
// PromosManager, presenter, and PrefService.
- (instancetype)initWithPromosManager:(PromosManager*)promosManager
                          prefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithCoder NS_UNAVAILABLE;

// Returns YES if the user conditions are met to present the Credential
// Provider Promo.
- (BOOL)canShowCredentialProviderPromoWithTrigger:
            (CredentialProviderPromoTrigger)trigger
                                        promoSeen:
                                            (BOOL)promoSeenInCurrentSession;

// Configures the consumer.
- (void)configureConsumerWithTrigger:(CredentialProviderPromoTrigger)trigger
                             context:(CredentialProviderPromoContext)context;

// Registers the promo for single display.
- (void)registerPromoWithPromosManager;

// Returns the source for the last time the promo was displayed. ::kUnknown is
// returned by default. This is persisted so subsequent resurfacing of the promo
// can access it.
- (IOSCredentialProviderPromoSource)promoOriginalSource;

@end

#endif  // IOS_CHROME_BROWSER_UI_CREDENTIAL_PROVIDER_PROMO_CREDENTIAL_PROVIDER_PROMO_MEDIATOR_H_
