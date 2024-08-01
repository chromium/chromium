// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFETY_CHECK_MODEL_IOS_CHROME_SAFETY_CHECK_MANAGER_UTILS_H_
#define IOS_CHROME_BROWSER_SAFETY_CHECK_MODEL_IOS_CHROME_SAFETY_CHECK_MANAGER_UTILS_H_

#import <optional>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"

enum class PasswordCheckState;
enum class PasswordSafetyCheckState;

// The Pref dictionary key used for storing compromised password counts.
extern const char kSafetyCheckCompromisedPasswordsCountKey[];

// The Pref dictionary key used for storing dismissed password counts.
extern const char kSafetyCheckDismissedPasswordsCountKey[];

// The Pref dictionary key used for storing reused password counts.
extern const char kSafetyCheckReusedPasswordsCountKey[];

// The Pref dictionary key used for storing weak password counts.
extern const char kSafetyCheckWeakPasswordsCountKey[];

// Returns the Password check state (Safety Check version) given a
// `check_state`, insecure credentials list, and previous Password check state
// (Safety Check version).
PasswordSafetyCheckState CalculatePasswordSafetyCheckState(
    PasswordCheckState check_state,
    const std::vector<password_manager::CredentialUIEntry>&
        insecure_credentials,
    PasswordSafetyCheckState previous_check_state);

// Creates an `InsecurePasswordCounts` from a dictionary.
//
// The dictionary should contain counts associated with the following keys:
// * kSafetyCheckCompromisedPasswordsCountKey
// * kSafetyCheckDismissedPasswordsCountKey
// * kSafetyCheckReusedPasswordsCountKey
// * kSafetyCheckWeakPasswordsCountKey
//
// If a key is missing, its corresponding count is assumed to be zero.
password_manager::InsecurePasswordCounts DictToInsecurePasswordCounts(
    const base::Value::Dict& dict);

// Returns true if the Safety Check is due for an automatic run. This
// happens if the check has never been run or if the last run time
// exceeds `kSafetyCheckAutorunDelay`.
bool CanAutomaticallyRunSafetyCheck(std::optional<base::Time> last_run_time);

// Returns the time of the latest Safety Check run, if ever, across all Safety
// Check entrypoints.
std::optional<base::Time> GetLatestSafetyCheckRunTimeAcrossAllEntrypoints(
    raw_ptr<PrefService> local_pref_service);

#endif  // IOS_CHROME_BROWSER_SAFETY_CHECK_MODEL_IOS_CHROME_SAFETY_CHECK_MANAGER_UTILS_H_
