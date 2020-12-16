// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_UTILS_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_UTILS_H_

#import <UIKit/UIKit.h>

class ChromeBrowserState;
class PrefService;

namespace base {
class Version;
}

namespace signin {

// Returns true if this user sign-in upgrade should be shown for |browserState|.
bool ShouldPresentUserSigninUpgrade(ChromeBrowserState* browserState);

// Records in user defaults:
//   + the Chromium current version.
//   + increases the sign-in promo display count.
//   + Gaia ids list.
// Separated out into a discrete function to allow overriding when testing.
void RecordVersionSeen();

// Set the Chromium current version for sign-in. Used for tests only.
void SetCurrentVersionForTesting(base::Version* version);

// Returns a boolean indicating whether browser sign-in is allowed by policy.
bool IsSigninAllowed(const PrefService* prefs);

}  // namespace signin

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_UTILS_H_
