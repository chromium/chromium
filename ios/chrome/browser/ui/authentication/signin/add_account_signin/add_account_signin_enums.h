// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ADD_ACCOUNT_SIGNIN_ADD_ACCOUNT_SIGNIN_ENUMS_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ADD_ACCOUNT_SIGNIN_ADD_ACCOUNT_SIGNIN_ENUMS_H_

#import <Foundation/Foundation.h>

// Intent for the add account sign-in flow.
typedef NS_ENUM(NSUInteger, AddAccountSigninIntent) {
  // Adds a secondary account that will only be used for the web.
  AddAccountSigninIntentAddSecondaryAccount,
  // Reauthenticates with the previous primary account. Since it is
  // a primary account, sign-in consent is required.
  AddAccountSigninIntentReauthPrimaryAccount,
};

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ADD_ACCOUNT_SIGNIN_ADD_ACCOUNT_SIGNIN_ENUMS_H_
