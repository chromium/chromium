// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_SIGNIN_PROMO_VIEW_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_SIGNIN_PROMO_VIEW_CONSTANTS_H_

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, SigninPromoViewMode) {
  // No identity available on the device.
  SigninPromoViewModeNoAccounts,
  // At least one identity is available on the device and the user can sign in
  // without entering their credentials.
  SigninPromoViewModeSigninWithAccount,
  // The user is signed in to Chrome.
  SigninPromoViewModeSignedInWithPrimaryAccount,
};

typedef NS_ENUM(NSInteger, SigninPromoViewStyle) {
  // Standard style used for most surfaces. It contains user avatar/ chromium
  // logo, text body, rounded corners colored button and an optional secondary
  // plain button, all stacked vertically.
  SigninPromoViewStyleStandard = 0,
  // Compact style with vertical layout and avatar/logo.
  SigninPromoViewStyleCompact = 2,
  // Style containing a single rounded corners colored button.
  // TODO(crbug.com/40924554): This is a weird construct used only by recent
  // tabs, where SigninPromoView shows the button and other views show the
  // text/illustration. We should consider adopting SigninPromoViewStyleStandard
  // in that UI, or bringing the text/illustration here.
  SigninPromoViewStyleOnlyButton = 3,
};

extern NSString* const kSigninPromoViewId;
extern NSString* const kSigninPromoPrimaryButtonId;
extern NSString* const kSigninPromoSecondaryButtonId;
extern NSString* const kSigninPromoCloseButtonId;
extern NSString* const kSigninPromoActivityIndicatorId;

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_SIGNIN_PROMO_VIEW_CONSTANTS_H_
