// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFETY_CHECK_MODEL_IOS_CHROME_SAFETY_CHECK_MANAGER_UTILS_H_
#define IOS_CHROME_BROWSER_SAFETY_CHECK_MODEL_IOS_CHROME_SAFETY_CHECK_MANAGER_UTILS_H_

#import <vector>

#import "base/values.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
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

#endif  // IOS_CHROME_BROWSER_SAFETY_CHECK_MODEL_IOS_CHROME_SAFETY_CHECK_MANAGER_UTILS_H_
