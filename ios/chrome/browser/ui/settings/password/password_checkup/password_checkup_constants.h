// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_CONSTANTS_H_

#import <Foundation/Foundation.h>

namespace password_manager {

// Name of the green header image shown in the password checkup homepage.
extern NSString* const kPasswordCheckupHeaderImageGreen;

// Name of the loading header image shown in the password checkup homepage.
extern NSString* const kPasswordCheckupHeaderImageLoading;

// Name of the red header image shown in the password checkup homepage.
extern NSString* const kPasswordCheckupHeaderImageRed;

// Name of the yellow header image shown in the password checkup homepage.
extern NSString* const kPasswordCheckupHeaderImageYellow;

// Accessibility identifier for the header image view shown in the password
// checkup homepage.
extern NSString* const kPasswordCheckupHeaderImageViewId;

// Accessibility identifier for the UITableView in the password checkup
// homepage.
extern NSString* const kPasswordCheckupTableViewId;

// Accessibility identifier for the compromised passwords item in the password
// checkup homepage.
extern NSString* const kPasswordCheckupCompromisedPasswordsItemId;

// Accessibility identifier for the reused passwords item in the password
// checkup homepage.
extern NSString* const kPasswordCheckupReusedPasswordsItemId;

// Accessibility identifier for the weak passwords item in the password checkup
// homepage.
extern NSString* const kPasswordCheckupWeakPasswordsItemId;

// URL to the help center article about changing unsafe passwords.
extern const char kPasswordManagerHelpCenterChangeUnsafePasswordsURL[];

// URL to the help center article about creating strong passwords.
extern const char kPasswordManagerHelpCenterCreateStrongPasswordsURL[];

}  // namespace password_manager

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_CONSTANTS_H_
