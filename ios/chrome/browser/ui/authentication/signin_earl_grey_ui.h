// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_UI_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_UI_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"

@class ChromeIdentity;

// Methods used for the EarlGrey tests, related to UI.
@interface SigninEarlGreyUI : NSObject

// Calls [SigninEarlGreyUI signinWithIdentity:identity isManagedAccount:NO].
+ (void)signinWithIdentity:(ChromeIdentity*)identity;

// Adds the identity (if not already added), and perform a sign-in. if
// |isManagedAccount| is true, |identity| needs to be a managed account and the
// managed dialog is expected while signing in.
+ (void)signinWithIdentity:(ChromeIdentity*)identity
          isManagedAccount:(BOOL)isManagedAccount;

// Taps on the settings link in the sign-in view. The sign-in view has to be
// opened before calling this method.
+ (void)tapSettingsLink;

// Selects an identity when the identity chooser dialog is presented. The dialog
// is confirmed, but it doesn't validated the user consent page.
+ (void)selectIdentityWithEmail:(NSString*)userEmail;

// Confirms the managed account dialog with signing in.
+ (void)confirmSigninWithManagedAccount;

// Confirms the sign in confirmation page, scrolls first to make the OK button
// visible on short devices (e.g. iPhone 5s).
+ (void)confirmSigninConfirmationDialog;

// Taps on the "ADD ACCOUNT" button in the unified consent, to display the
// SSO dialog.
// This method should only be used with UnifiedConsent flag.
+ (void)tapAddAccountButton;

// Checks that the sign-in promo view (with a close button) is visible using the
// right mode.
+ (void)checkSigninPromoVisibleWithMode:(SigninPromoViewMode)mode;

// Checks that the sign-in promo view is visible using the right mode. If
// |closeButton| is set to YES, the close button in the sign-in promo has to be
// visible.
+ (void)checkSigninPromoVisibleWithMode:(SigninPromoViewMode)mode
                            closeButton:(BOOL)closeButton;

// Checks that the sign-in promo view is not visible.
+ (void)checkSigninPromoNotVisible;

// Signs out from the current identity. if |isManagedAccount| is true, the
// confirmed managed dialog is confirmed while signing out.
+ (void)signOutWithManagedAccount:(BOOL)isManagedAccount;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_UI_H_
