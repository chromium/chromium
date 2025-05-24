// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_SIGNIN_TEST_UTIL_H_
#define IOS_CHROME_TEST_APP_SIGNIN_TEST_UTIL_H_

#import "base/ios/block_types.h"
#import "components/policy/core/browser/signin/profile_separation_policies.h"

@protocol SystemIdentity;

namespace chrome_test_util {

// Sets up mock authentication that will bypass the real ChromeIdentityService
// and install a fake one.
void SetUpMockAuthentication();

// Tears down the fake ChromeIdentityService and restores the real one.
void TearDownMockAuthentication();

// Signs the user out and starts clearing all identities from the
// ChromeIdentityService.
void SignOutAndClearIdentities(ProceduralBlock completion);

// Returns true when there are no identities in the ChromeIdentityService.
bool HasIdentities();

// Resets mock authentication.
void ResetMockAuthentication();

// Resets Sign-in promo impression preferences for bookmarks and settings view,
// and resets kIosBookmarkPromoAlreadySeen flag for bookmarks.
void ResetSigninPromoPreferences();

// The user will be in the signed-in state.
void SignIn(id<SystemIdentity> identity);

// Resets all preferences related to History Sync Opt-In.
void ResetHistorySyncPreferencesForTesting();

// Resets all the selected data types to be turned on in the sync engine. And
// clear per-account passphrases.
void ResetSyncAccountSettingsPrefs();

// Set/clear a global flag to return fake default responses for all profile
// separation policy fetch requests (unless a specific response is set for the
// next request, see `setPolicyResponseForNextProfileSeparationPolicyRequest:`).
// If a test sets this (typically in `setUpForTestCase`), it must also unset it
// again (in `tearDown`).
void SetUseFakeResponsesForProfileSeparationPolicyRequests();
void ClearUseFakeResponsesForProfileSeparationPolicyRequests();

// Stores a policy that will be returned for the next fetch profile separation
// policy request.
void SetPolicyResponseForNextProfileSeparationPolicyRequest(
    policy::ProfileSeparationDataMigrationSettings
        profileSeparationDataMigrationSettings);

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_APP_SIGNIN_TEST_UTIL_H_
