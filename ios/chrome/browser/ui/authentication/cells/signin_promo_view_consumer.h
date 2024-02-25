// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_SIGNIN_PROMO_VIEW_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_SIGNIN_PROMO_VIEW_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"

@class SigninPromoViewConfigurator;
@class SigninPromoViewMediator;
@protocol SystemIdentity;

// Handles identity update notifications.
@protocol SigninPromoViewConsumer <NSObject>

// Called when the default identity is changed or updated. This method is not
// called when the sign-in is in progress.
// `configurator`, new instance set each time, to configure a SigninPromoView.
// `identityChanged` is set to YES when the default identity is changed.
- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                             identityChanged:(BOOL)identityChanged;

@optional

// Called when the sign-in in progress status changes.
- (void)promoProgressStateDidChange;

// Called when the sign-in is finished.
- (void)signinDidFinish;

// Called when the close button is tapped.
- (void)signinPromoViewMediatorCloseButtonWasTapped:
    (SigninPromoViewMediator*)mediator;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_SIGNIN_PROMO_VIEW_CONSUMER_H_
