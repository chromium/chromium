// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFETY_CHECK_MODEL_IOS_CHROME_SAFETY_CHECK_MANAGER_CONSTANTS_H_
#define IOS_CHROME_BROWSER_SAFETY_CHECK_MODEL_IOS_CHROME_SAFETY_CHECK_MANAGER_CONSTANTS_H_

#include <optional>

#include "base/time/time.h"

// The amount of time (inclusive) to wait for an Omaha response before
// considering the request an Omaha error.
const base::TimeDelta kOmahaNetworkWaitTime = base::Seconds(30);

// The amount of time that must pass since the last Safety Check run before an
// automatic run can be triggered.
const base::TimeDelta kSafetyCheckAutorunDelay = base::Days(30);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(IOSSafetyCheckFreshnessTrigger)
enum class IOSSafetyCheckFreshnessTrigger {
  kPasswordCheckStateChanged = 0,
  kUpdateChromeCheckStateChanged = 1,
  kMaxValue = kUpdateChromeCheckStateChanged,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSSafetyCheckFreshnessTrigger)

// Enum with all possible states of the update check.
enum class UpdateChromeSafetyCheckState {
  // When the check has not been run yet.
  kDefault,
  // When the user is up to date.
  kUpToDate,
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
  // When check has not been run yet.
  kDefault,
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

// Returns string representation of UpdateChromeSafetyCheckState `check_state`.
const std::string NameForSafetyCheckState(
    UpdateChromeSafetyCheckState check_state);

// Returns string representation of PasswordSafetyCheckState `check_state`.
const std::string NameForSafetyCheckState(PasswordSafetyCheckState check_state);

// Returns string representation of SafeBrowsingSafetyCheckState `check_state`.
const std::string NameForSafetyCheckState(
    SafeBrowsingSafetyCheckState check_state);

// Returns UpdateChromeSafetyCheckState given its string representation
// `check_state`.
std::optional<UpdateChromeSafetyCheckState> UpdateChromeSafetyCheckStateForName(
    const std::string& check_state);

// Returns PasswordSafetyCheckState given its string representation
// `check_state`.
std::optional<PasswordSafetyCheckState> PasswordSafetyCheckStateForName(
    const std::string& check_state);

// Returns SafeBrowsingSafetyCheckState given its string representation
// `check_state`.
std::optional<SafeBrowsingSafetyCheckState> SafeBrowsingSafetyCheckStateForName(
    const std::string& check_state);

#endif  // IOS_CHROME_BROWSER_SAFETY_CHECK_MODEL_IOS_CHROME_SAFETY_CHECK_MANAGER_CONSTANTS_H_
