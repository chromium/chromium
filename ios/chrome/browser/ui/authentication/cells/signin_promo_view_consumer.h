// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_SIGNIN_PROMO_VIEW_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_SIGNIN_PROMO_VIEW_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/commands/show_signin_command.h"

@class ChromeIdentity;
@class SigninPromoViewConfigurator;
@class SigninPromoViewMediator;

// Handles identity update notifications.
@protocol SigninPromoViewConsumer <NSObject>

// Called when the default identity is changed or updated. This method is not
// called when the sign-in is in progress.
// |configurator|, new instance set each time, to configure a SigninPromoView.
// |identityChanged| is set to YES when the default identity is changed.
- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                             identityChanged:(BOOL)identityChanged;

@optional

// Called when the sign-in is finished.
- (void)signinDidFinish;

// Called when one of the sign-in promo view button is being tapped. This method
// is optional. If it is not implementated, the mediator will open the sign-in
// view using Chrome commands. If this method is implemented, it is the
// responsability to the consumer to open the sign-in view (either using Chrome
// commands or directly using the SigninInteractionCoordinator).
// The consumer also has the responsability to make sure |completion| is called
// once the sign-in is done.
// |mediator| is in charge to record all histograms and user actions.
- (void)signinPromoViewMediator:(SigninPromoViewMediator*)mediator
    shouldOpenSigninWithIdentity:(ChromeIdentity*)identity
                     promoAction:(signin_metrics::PromoAction)promoAction
                      completion:
                          (ShowSigninCommandCompletionCallback)completion;

// Called when the close button is tapped.
- (void)signinPromoViewMediatorCloseButtonWasTapped:
    (SigninPromoViewMediator*)mediator;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_SIGNIN_PROMO_VIEW_CONSUMER_H_
