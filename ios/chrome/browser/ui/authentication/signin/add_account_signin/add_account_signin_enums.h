// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ADD_ACCOUNT_SIGNIN_ADD_ACCOUNT_SIGNIN_ENUMS_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ADD_ACCOUNT_SIGNIN_ADD_ACCOUNT_SIGNIN_ENUMS_H_

#import <Foundation/Foundation.h>

// Intent for the add account sign-in flow.
enum class AddAccountSigninIntent {
  // Adds a new account to the device. This can happen to add a secondary
  // account (when there is a primary account) or to add an account when the
  // user is signed out.
  kAddAccount,
  // Reauthenticates with the current primary account, using the SSOAuth add
  // account dialog.
  // This intent can only be used when there is a primary account with sync
  // consent.
  kPrimaryAccountReauth,
  // Signs in again with the previous primary account, using the SSOAuth add
  // account dialog. After that dialog, the history sync consent dialog will be
  // presented to the user. This intent can only be used when there is no
  // primary account.
  kResignin,
};

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ADD_ACCOUNT_SIGNIN_ADD_ACCOUNT_SIGNIN_ENUMS_H_
