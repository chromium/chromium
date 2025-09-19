// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_MENU_PUBLIC_ACCOUNT_MENU_CONSTANTS_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_MENU_PUBLIC_ACCOUNT_MENU_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Possible "access points" from where the account menu can be triggered.
enum class AccountMenuAccessPoint {
  // The most common one: The account particle disc on the NTP.
  kNewTabPage,
  // The "Use another account" button in Chrome settings.
  kSettings,
  // A button on a Gaia web page, called something like "Use another account" or
  // "Manage accounts".
  kWeb,
};

// The accessibility identifier of the view controller's view.
extern NSString* const kAccountMenuTableViewId;
// The accessibility identifier of the view controlle's close button.
extern NSString* const kAccountMenuCloseButtonId;
// The accessibility identifier of the view controlle's ellipsis button.
extern NSString* const kAccountMenuSecondaryActionMenuButtonId;
// The accessibility identifier of the Error mmesage.
extern NSString* const kAccountMenuErrorMessageId;
// The accessibility identifier of the Error button.
extern NSString* const kAccountMenuErrorActionButtonId;
// The accessibility identifier of the Add Account.
extern NSString* const kAccountMenuAddAccountButtonId;
// The accessibility identifier of the Sign out.
extern NSString* const kAccountMenuSignoutButtonId;
// The accessibility identifier for the secondary accounts buttons.
extern NSString* const kAccountMenuSecondaryAccountButtonId;
// The accessibility identifier for the account menu activity indicator.
extern NSString* const kAccountMenuActivityIndicatorId;
// The accessibility identifier of manage accounts button.
extern NSString* const kAccountMenuManageAccountsButtonId;
// The accessibility identifier for the "edit account list" menu entry.
extern NSString* const kAccountMenuEditAccountListId;
// The accessibility ideentifier for the "manage your account" menu entry.
extern NSString* const kAccountMenuManageYourGoogleAccountId;

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_MENU_PUBLIC_ACCOUNT_MENU_CONSTANTS_H_
