// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_CONSTANTS_H_

#import <UIKit/UIKit.h>

// Intent when the user begins a sign-in flow.
typedef NS_ENUM(NSUInteger, UserSigninIntent) {
  // Upgrade sign-in flow.
  UserSigninIntentUpgrade,
  // Sign-in flow.
  UserSigninIntentSignin,
};

// Key in the UserDefaults to record the version of the application when the
// sign-in promo has been displayed. The value is set on the first cold start to
// make sure the sign-in promo is not triggered right after the FRE.
// Exposed for testing.
extern NSString* kDisplayedSSORecallForMajorVersionKey;
// Key in the UserDefaults to record the GAIA id list when the sign-in promo
// was shown.
// Exposed for testing.
extern NSString* kLastShownAccountGaiaIdVersionKey;
// Key in the UserDefaults to record the number of time the sign-in promo has
// been shown.
// TODO(crbug.com/1312345): Need to merge with kDisplayedSSORecallPromoCountKey.
// Exposed for testing.
extern NSString* kSigninPromoViewDisplayCountKey;
// Key in the UserDefaults to track how many times the SSO Recall promo has been
// displayed.
// TODO(crbug.com/1312345): Need to merge with kSigninPromoViewDisplayCountKey.
// Exposed for testing.
extern NSString* kDisplayedSSORecallPromoCountKey;

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_CONSTANTS_H_
