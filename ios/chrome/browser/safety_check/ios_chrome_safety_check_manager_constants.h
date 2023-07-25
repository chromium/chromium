// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFETY_CHECK_IOS_CHROME_SAFETY_CHECK_MANAGER_CONSTANTS_H_
#define IOS_CHROME_BROWSER_SAFETY_CHECK_IOS_CHROME_SAFETY_CHECK_MANAGER_CONSTANTS_H_

#include "base/time/time.h"

// The amount of time (inclusive) to wait for an Omaha response before
// considering the request an Omaha error.
const base::TimeDelta kOmahaNetworkWaitTime = base::Seconds(30);

// Enum with all possible states of the update check.
enum class UpdateChromeSafetyCheckState {
  // When the user is up to date.
  kUpToDate,
  // When the check has not been run yet.
  kDefault,
  // When the user is out of date.
  kOutOfDate,
  // When the user is managed.
  kManaged,
  // When the check is running.
  kRunning,
  // When Omaha encountered an error.
  kOmahaError,
  // When there is a connectivity issue.
  kNetError,
  // When the device is on a non-supported channel.
  kChannel,
};

// Enum with all possible states of the password check.
enum class PasswordSafetyCheckState {
  // When no compromised passwords were detected.
  kSafe,
  // When user has unmuted compromised passwords.
  kUnmutedCompromisedPasswords,
  // When user has reused passwords.
  kReusedPasswords,
  // When user has weak passwords.
  kWeakPasswords,
  // When user has dismissed warnings.
  kDismissedWarnings,
  // When check has not been run yet.
  kDefault,
  // When password check is running.
  kRunning,
  // When user has no passwords and check can't be performed.
  kDisabled,
  // When password check failed due to network issues, quota limit or others.
  kError,
  // When password check failed due to user being signed out.
  kSignedOut,
};

// Enum with all possible states of the Safe Browsing check.
enum class SafeBrowsingSafetyCheckState {
  // When check was not run yet.
  kDefault,
  // When Safe Browsing is managed by admin.
  kManaged,
  // When the Safe Browsing check is running.
  kRunning,
  // When Safe Browsing is enabled.
  kSafe,
  // When Safe Browsing is disabled.
  kUnsafe,
};

// Enum with all possible states of the trigger to start the check (e.g. "Check
// Now" button). NOTE: SafetyCheckManager clients will determine which UI
// element(s) will use this enum.
enum class RunningSafetyCheckState {
  // When the check is not running.
  kDefault,
  // When the check is running.
  kRunning,
};

#endif  // IOS_CHROME_BROWSER_SAFETY_CHECK_IOS_CHROME_SAFETY_CHECK_MANAGER_CONSTANTS_H_
