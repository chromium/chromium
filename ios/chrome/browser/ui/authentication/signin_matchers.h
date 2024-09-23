// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_MATCHERS_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_MATCHERS_H_

#import <Foundation/Foundation.h>

@protocol GREYAction;
@protocol GREYMatcher;

namespace chrome_test_util {

// Returns a matcher for a TableViewIdentityCell based on the `email`.
id<GREYMatcher> IdentityCellMatcherForEmail(NSString* email);

// Returns a matcher for the skip button in the web sign-in consistency dialog.
id<GREYMatcher> WebSigninSkipButtonMatcher();

// Returns a matcher for the primary button in the web sign-in consistency
// dialog.
id<GREYMatcher> WebSigninPrimaryButtonMatcher();

// Returns matcher for the Sync Settings button on the main Settings screen.
// For users who are signed-in but not syncing, this button leads to the sync
// consent dialog instead.
id<GREYMatcher> GoogleSyncSettingsButton();

// Matcher for the sign-in screens (like history sync opt-in, upgrade promo…).
id<GREYMatcher> SigninScreenPromoMatcher();

// Matcher for the primary button ("Yes, I'm In") in sign-in screens (like
// history sync opt-in, upgrade promo…).
id<GREYMatcher> SigninScreenPromoPrimaryButtonMatcher();

// Matcher for the secondary button ("No Thanks") in sign-in screens (like
// history sync opt-in, upgrade promo…).
id<GREYMatcher> SigninScreenPromoSecondaryButtonMatcher();

// Matcher for the Settings row which, upon tap, leads the user to sign-in. The
// row is only shown to signed-out users.
id<GREYMatcher> SettingsSignInRowMatcher();

// Matcher for the history opt-in screen.
id<GREYMatcher> HistoryOptInPromoMatcher();

// Action for searching an UI element in the history opt-in screen..
id<GREYAction> HistoryOptInScrollDown();

}  // namespace chrome_test_util

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_MATCHERS_H_
