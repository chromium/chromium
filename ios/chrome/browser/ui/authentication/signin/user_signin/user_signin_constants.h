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


#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_CONSTANTS_H_
