// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_constants.h"

@protocol CredentialProviderPromoConsumer;

class PromosManager;

namespace feature_engagement {
class Tracker;
}

// Manages the state and interactions of the CredentialProviderPromoConsumer.
@interface CredentialProviderPromoMediator : NSObject

// The main consumer for this mediator.
@property(nonatomic, weak) id<CredentialProviderPromoConsumer> consumer;

// The feature engagement tracker to alert of promo events.
@property(nonatomic, assign) feature_engagement::Tracker* tracker;

// Designated initializer. Initializes the mediator with the
// PromosManager.
- (instancetype)initWithPromosManager:(PromosManager*)promosManager
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

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_MEDIATOR_H_
