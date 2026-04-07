// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_ENTERPRISE_PUBLIC_MANAGED_PROFILE_CREATION_CONSTANTS_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_ENTERPRISE_PUBLIC_MANAGED_PROFILE_CREATION_CONSTANTS_H_

namespace signin {

// The state representing a managed profile creation process.
// If multiple cases hold true, the first enum that applies is selected.
enum class ManagedAccountSigninMode {
  // In the four cases below, the user has no choice to make.

  // Migration was done during chrome start-up. We’re only informing the user
  // about it.
  kInformOfForcedMigration,
  // The user is signing-in from the FRE. No migration questions are asked as
  // there is no local data to merge. If the user confirms they want to sign-in,
  // the personal profile will become the managed one.
  kAutoMergeDuringFRE,
  // The user is currently signed-in. Data has to be separated.
  kMustSeparateBecauseSignedIn,
  // Device or account policy forces data to be separated. The user must be
  // informed that this is what stops local data to be merged with his managed
  // account.
  kForceSeparateProfileDataByPolicy,

  // The two cases where the user have a choice to make.

  // By default, profile data will be separated. The user can change this
  // option to kMergeProfileData.
  kSeparateProfileData,
  // By default, profile data will be merged. The user can change this option to
  // kSeparateProfileData.
  kMergeProfileData,
};

}  // namespace signin

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_ENTERPRISE_PUBLIC_MANAGED_PROFILE_CREATION_CONSTANTS_H_
