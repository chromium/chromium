// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_PROMO_COORDINATOR_NON_MODAL_SIGNIN_PROMO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_PROMO_COORDINATOR_NON_MODAL_SIGNIN_PROMO_MEDIATOR_H_

#import <Foundation/Foundation.h>

namespace signin {
class IdentityManager;
}

class AuthenticationService;

@class NonModalSignInPromoMediator;

// Protocol for mediator to set the delay timer for the promo.
@protocol NonModalSignInPromoMediatorDelegate <NSObject>
// Handles mediator timer expiration event.
- (bool)nonModalSignInPromoMediatorTimerExpired:
    (NonModalSignInPromoMediator*)mediator;
@end

// Mediator that handles the business logic for the non-modal sign-in promo.
@interface NonModalSignInPromoMediator : NSObject

// Init Mediator with auth service and identity manager.
- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authService
                  identityManager:(signin::IdentityManager*)identityManager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator and identity manager.
- (void)disconnect;

// Starts showing the promo based on promoType.
- (void)startPromoDisplayTimer;

// Stops showing the promo.
- (void)stopShowingPromo;

// Handles the user tapping the sign-in button.
- (void)handleSignInButtonTapped;

// The delegate that responds to the mediator's actions.
@property(nonatomic, weak) id<NonModalSignInPromoMediatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_PROMO_COORDINATOR_NON_MODAL_SIGNIN_PROMO_MEDIATOR_H_
