// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_SIGNIN_NON_MODAL_PROMO_COORDINATOR_NON_MODAL_SIGNIN_PROMO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_SIGNIN_NON_MODAL_PROMO_COORDINATOR_NON_MODAL_SIGNIN_PROMO_MEDIATOR_H_

#import <Foundation/Foundation.h>

class AuthenticationService;
class BrowserViewVisibilityNotifierBrowserAgent;
class OverlayPresenter;
class WebStateList;
namespace feature_engagement {
class Tracker;
}
namespace signin {
class IdentityManager;
}

enum class NonModalSignInPromoType;

@class NonModalSignInPromoMediator;

// Delegate for `NonModalSignInPromoMediator` to check view visibility and
// handle presentation lifecycle events.
@protocol NonModalSignInPromoMediatorDelegate <NSObject>

// Returns whether the promo can be displayed on screen right now (e.g. the
// browser view is in a window). Called when the display delay ends, before the
// promo is counted as shown by the feature engagement tracker.
- (BOOL)nonModalSignInPromoMediatorCanDisplayPromo:
    (NonModalSignInPromoMediator*)mediator;

// Called when the user accepts the promo banner and sign-in should start.
- (void)nonModalSignInPromoMediatorDidAccept:
    (NonModalSignInPromoMediator*)mediator;

// Called when the promo ends (cancelled, skipped, timed out, or dismissed).
- (void)nonModalSignInPromoMediatorShouldDismiss:
    (NonModalSignInPromoMediator*)mediator;

@end

// Mediator that handles the business logic, infobar lifecycle, dismissal
// triggers, metrics and feature engagement bookkeeping for the non-modal
// sign-in promo.
@interface NonModalSignInPromoMediator : NSObject

// Initializes the mediator with its model dependencies and promo type.
// `visibilityNotifier` may be null.
- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authService
                  identityManager:(signin::IdentityManager*)identityManager
         featureEngagementTracker:(feature_engagement::Tracker*)tracker
                     webStateList:(WebStateList*)webStateList
                 overlayPresenter:(OverlayPresenter*)overlayPresenter
    browserViewVisibilityNotifier:
        (BrowserViewVisibilityNotifierBrowserAgent*)visibilityNotifier
                        promoType:(NonModalSignInPromoType)promoType
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects observers, removes any active promo infobar, and notifies the
// feature engagement tracker if still pending.
- (void)disconnect;

// Starts the delay timer before showing the promo, if eligible.
- (void)startPromoDisplayTimer;

// The delegate that responds to the mediator's actions.
@property(nonatomic, weak) id<NonModalSignInPromoMediatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_SIGNIN_NON_MODAL_PROMO_COORDINATOR_NON_MODAL_SIGNIN_PROMO_MEDIATOR_H_
