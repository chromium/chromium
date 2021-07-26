// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_UTILS_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_UTILS_H_

#import <UIKit/UIKit.h>
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

class ChromeAccountManagerService;
class ChromeBrowserState;
class PrefService;

namespace base {
class TimeDelta;
class Version;
}

namespace signin {

// Returns the maximum allowed waiting time for the Account Capabilities API.
base::TimeDelta GetWaitThresholdForCapabilities();

// Returns true if this user sign-in upgrade should be shown for |browserState|.
bool ShouldPresentUserSigninUpgrade(ChromeBrowserState* browser_state,
                                    const base::Version& current_version);

// Records in user defaults:
//   + the Chromium current version.
//   + increases the sign-in promo display count.
//   + Gaia ids list.
// Separated out into a discrete function to allow overriding when testing.
void RecordVersionSeen(ChromeAccountManagerService* account_manager_service,
                       const base::Version& current_version);

// Returns a boolean indicating whether browser sign-in is allowed across the
// app.
bool IsSigninAllowed(const PrefService* prefs);

// Returns a boolean indicating whether policy allows browser sign-in.
bool IsSigninAllowedByPolicy(const PrefService* prefs);

// Returns the current sign-in state of primary identity.
IdentitySigninState GetPrimaryIdentitySigninState(
    ChromeBrowserState* browser_state);

}  // namespace signin

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_UTILS_H_
