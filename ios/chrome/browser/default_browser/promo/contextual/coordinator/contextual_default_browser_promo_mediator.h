// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_CONTEXTUAL_COORDINATOR_CONTEXTUAL_DEFAULT_BROWSER_PROMO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_CONTEXTUAL_COORDINATOR_CONTEXTUAL_DEFAULT_BROWSER_PROMO_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/default_browser/promo/contextual/public/contextual_default_browser_promo_constants.h"

@protocol ContextualDefaultBrowserPromoConsumer;

// Mediator for the contextual default browser promo.
@interface ContextualDefaultBrowserPromoMediator : NSObject

// The consumer for this mediator.
@property(nonatomic, weak) id<ContextualDefaultBrowserPromoConsumer> consumer;

// Initializes the mediator with a specific contextual promo type.
- (instancetype)initWithPromoType:(ContextualDefaultBrowserPromoType)promoType
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_CONTEXTUAL_COORDINATOR_CONTEXTUAL_DEFAULT_BROWSER_PROMO_MEDIATOR_H_
