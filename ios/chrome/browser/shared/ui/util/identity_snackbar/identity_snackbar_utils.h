// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_IDENTITY_SNACKBAR_IDENTITY_SNACKBAR_UTILS_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_IDENTITY_SNACKBAR_IDENTITY_SNACKBAR_UTILS_H_

#import <Foundation/Foundation.h>

class Browser;
@class SnackbarMessage;
@protocol SystemIdentity;

// Creates and returns a snackbar message with the user's identity information.
// `identity` must correspond to an account on the device that matches
// `prefs::kRestrictAccountsToPatterns`.
SnackbarMessage* CreateIdentitySnackbarMessage(id<SystemIdentity> identity,
                                               Browser* browser);

// Displays the identity confirmation snackbar with `identity`.
// `identity` must correspond to an account on the device that matches
// `prefs::kRestrictAccountsToPatterns`.
void TriggerAccountSwitchSnackbarWithIdentity(id<SystemIdentity> identity,
                                              Browser* browser);

// Displays the identity confirmation snackbar with `identity` and a custom
// title.
void TriggerSigninConfirmationSnackbarWithCustomTitle(
    id<SystemIdentity> identity,
    Browser* browser,
    NSString* custom_title);

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_IDENTITY_SNACKBAR_IDENTITY_SNACKBAR_UTILS_H_
