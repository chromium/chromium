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
  // The user is signed in to Chrome and can enable Sync on the primary account.
  SigninPromoViewModeSyncWithPrimaryAccount,
};

typedef NS_ENUM(NSInteger, SigninPromoViewStyle) {
  // Standard style used for most surfaces. It contains user avatar/ chromium
  // logo, text body, rounded corners colored button and an optional secondary
  // plain button, all stacked vertically.
  SigninPromoViewStyleStandard = 0,
  // Compact style that does not support personalization. Horizontal layout with
  // generic icon and title.
  SigninPromoViewStyleCompactTitled = 1,
  // Compact style with horizontal layout and avatar/logo.
  SigninPromoViewStyleCompactHorizontal = 2,
  // Compact style with vertical layout and avatar/logo.
  SigninPromoViewStyleCompactVertical = 3,
};

extern NSString* const kSigninPromoViewId;
extern NSString* const kSigninPromoPrimaryButtonId;
extern NSString* const kSigninPromoSecondaryButtonId;
extern NSString* const kSigninPromoCloseButtonId;
extern NSString* const kSigninPromoActivityIndicatorId;

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_SIGNIN_PROMO_VIEW_CONSTANTS_H_
