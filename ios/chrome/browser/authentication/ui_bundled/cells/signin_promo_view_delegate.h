// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CELLS_SIGNIN_PROMO_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CELLS_SIGNIN_PROMO_VIEW_DELEGATE_H_

#import <UIKit/UIKit.h>

@class SigninPromoView;
@class SigninPromoViewConfigurator;

@protocol SigninPromoViewDelegate <NSObject>

// Whether the sign-in promo is showing a spinner.
@property(nonatomic, assign, readonly) BOOL spinnerVisible;

// Called by SigninPromoView when the user taps the primary button with no
// identities on the device.
- (void)signinPromoViewDidTapSigninWithNewAccount:(SigninPromoView*)view;

// Called by SigninPromoView when the user taps the primary button with one
// or more identities on the device.
- (void)signinPromoViewDidTapPrimaryButtonWithDefaultAccount:
    (SigninPromoView*)view;

// Called by SigninPromoView when the user taps the secondary button with one
// or more identities on the device.
- (void)signinPromoViewDidTapSigninWithOtherAccount:(SigninPromoView*)view;

// Called by SigninPromoView when the user taps the close button.
- (void)signinPromoViewCloseButtonWasTapped:(SigninPromoView*)view;

// Creates and returns a configurator for the sign-in promo view.
- (SigninPromoViewConfigurator*)createConfigurator;

// Notifies that the sign-in promo view is visible.
- (void)signingPromoDidBecomeVisible;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CELLS_SIGNIN_PROMO_VIEW_DELEGATE_H_
