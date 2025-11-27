// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_FEATURES_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_FEATURES_H_

#import "base/feature_list.h"

// THIS CANNOT BE USED FROM EARLGREY TESTS.

// Returns whether the feature to put each managed account into its own separate
// profile is enabled. This is the case if `kSeparateProfilesForManagedAccounts`
// is enabled.
bool AreSeparateProfilesForManagedAccountsEnabled();

// YES if Profile-specific push notification handling is enabled.
bool IsMultiProfilePushNotificationHandlingEnabled();

// Whether to destroy the OTR profile early in ~ProfileIOSImpl.
BASE_DECLARE_FEATURE(kDestroyOTRProfileEarly);

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_FEATURES_H_
