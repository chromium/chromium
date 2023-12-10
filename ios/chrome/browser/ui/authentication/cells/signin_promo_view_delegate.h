// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_SIGNIN_PROMO_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_SIGNIN_PROMO_VIEW_DELEGATE_H_

#import <UIKit/UIKit.h>

@class SigninPromoView;

@protocol SigninPromoViewDelegate <NSObject>

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

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_SIGNIN_PROMO_VIEW_DELEGATE_H_
