// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_UI_TEST_UTIL_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_UI_TEST_UTIL_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"

@class FakeSystemIdentity;

typedef NS_ENUM(NSInteger, SignOutConfirmationChoice) {
  SignOutConfirmationChoiceClearData,
  SignOutConfirmationChoiceKeepData,
  SignOutConfirmationChoiceNotSyncing
};

// Test methods that perform sign in actions on Chrome UI.
@interface SigninEarlGreyUI : NSObject

// Calls
// +[SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableHistorySync:NO].
// `fakeIdentity` is added if it was not added yet.
+ (void)signinWithFakeIdentity:(FakeSystemIdentity*)fakeIdentity;

// Signs the account for `fakeIdentity` into Chrome through the Settings screen,
// with history/tab sync enabled or not according to `enableHistorySync` (most
// tests should not require it).
// There will be a GREYAssert if the tools menus is open when calling this
// method or if the account is not successfully signed in.
// `fakeIdentity` is added if it was not added yet.
+ (void)signinWithFakeIdentity:(FakeSystemIdentity*)fakeIdentity
             enableHistorySync:(BOOL)enableHistorySync;

// Signs the primary account out of Chrome through the accounts list screen.
// Taps the "Sign Out" button and dismisses the confirmation snackbar.
+ (void)signOut;

// dismisses the confirmation snackbar on sign-out.
+ (void)dismissSignoutSnackbar;

// Opens the confirmation dialog to remove an account from the device, without
// confirming it.
+ (void)openRemoveAccountConfirmationDialogWithFakeIdentity:
    (FakeSystemIdentity*)fakeIdentity;

// Taps "Remove from this device" button and follow-up confirmation.
// Assumes the user is on the Settings screen.
+ (void)tapRemoveAccountFromDeviceWithFakeIdentity:
    (FakeSystemIdentity*)fakeIdentity;

// Opens the recent tabs and tap in the primary sign-in button.
+ (void)tapPrimarySignInButtonInRecentTabs;

// Opens the tab switcher and tap in the primary sign-in button.
+ (void)tapPrimarySignInButtonInTabSwitcher;

// Checks that the sign-in promo view (with a close button) is visible using the
// right mode.
+ (void)verifySigninPromoVisibleWithMode:(SigninPromoViewMode)mode;

// Checks that the sign-in promo view is visible using the right mode. If
// `closeButton` is set to YES, the close button in the sign-in promo has to be
// visible.
+ (void)verifySigninPromoVisibleWithMode:(SigninPromoViewMode)mode
                             closeButton:(BOOL)closeButton;

// Checks that the sign-in promo view is not visible.
+ (void)verifySigninPromoNotVisible;

// Checks that the web sign-in consistency sheet visibility matches `isVisible`.
+ (void)verifyWebSigninIsVisible:(BOOL)isVisible;

// Submits encryption passphrase, if the user is on the Encryption page.
+ (void)submitSyncPassphrase:(NSString*)passphrase;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_UI_TEST_UTIL_H_
