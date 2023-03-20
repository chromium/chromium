// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_MATCHERS_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_MATCHERS_H_

#import <Foundation/Foundation.h>

@protocol GREYMatcher;

namespace chrome_test_util {

// Returns a matcher for a TableViewIdentityCell based on the `email`.
id<GREYMatcher> IdentityCellMatcherForEmail(NSString* email);

// Returns a matcher for the done button in advanced settings.
id<GREYMatcher> AdvancedSyncSettingsDoneButtonMatcher();

// Returns a matcher for the link to Advanced Sync Settings options.
id<GREYMatcher> SettingsLink();

// Returns a matcher for the skip button in the web sign-in consistency dialog.
id<GREYMatcher> WebSigninSkipButtonMatcher();

// Returns a matcher for the primary button in the web sign-in consistency
// dialog.
id<GREYMatcher> WebSigninPrimaryButtonMatcher();

// Returns matcher for the Google Sync Settings button on the main Settings
// screen.
id<GREYMatcher> GoogleSyncSettingsButton();

// Matcher for the upgrade sign-in promo.
id<GREYMatcher> UpgradeSigninPromoMatcher();

}  // namespace chrome_test_util

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_MATCHERS_H_
