// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_utils.h"

#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"

namespace {

// Returns the correct `PasswordSafetyCheckState` based on the highest priority
// `password_manager::WarningType`
PasswordSafetyCheckState PasswordSafetyCheckStateForHighestPriorityWarningType(
    const std::vector<password_manager::CredentialUIEntry>&
        insecure_credentials) {
  switch (GetWarningOfHighestPriority(insecure_credentials)) {
    case password_manager::WarningType::kCompromisedPasswordsWarning:
      return PasswordSafetyCheckState::kUnmutedCompromisedPasswords;
    case password_manager::WarningType::kReusedPasswordsWarning:
      return PasswordSafetyCheckState::kReusedPasswords;
    case password_manager::WarningType::kWeakPasswordsWarning:
      return PasswordSafetyCheckState::kWeakPasswords;
    case password_manager::WarningType::kDismissedWarningsWarning:
      return PasswordSafetyCheckState::kDismissedWarnings;
    case password_manager::WarningType::kNoInsecurePasswordsWarning:
      return PasswordSafetyCheckState::kSafe;
    default:
      return PasswordSafetyCheckState::kDefault;
  }
}

}  // namespace

constexpr char kSafetyCheckCompromisedPasswordsCountKey[] = "compromised";
constexpr char kSafetyCheckDismissedPasswordsCountKey[] = "dismissed";
constexpr char kSafetyCheckReusedPasswordsCountKey[] = "reused";
constexpr char kSafetyCheckWeakPasswordsCountKey[] = "weak";

PasswordSafetyCheckState CalculatePasswordSafetyCheckState(
    PasswordCheckState check_state,
    const std::vector<password_manager::CredentialUIEntry>&
        insecure_credentials,
    PasswordSafetyCheckState previous_check_state) {
  switch (check_state) {
    case PasswordCheckState::kRunning:
      return PasswordSafetyCheckState::kRunning;
    case PasswordCheckState::kNoPasswords:
      return PasswordSafetyCheckState::kDisabled;
    case PasswordCheckState::kSignedOut:
      return PasswordSafetyCheckState::kSignedOut;
    case PasswordCheckState::kOffline:
    case PasswordCheckState::kQuotaLimit:
    case PasswordCheckState::kOther:
      return PasswordSafetyCheckState::kError;
    case PasswordCheckState::kCanceled:
      return PasswordSafetyCheckState::kDefault;
    case PasswordCheckState::kIdle:
      if (previous_check_state == PasswordSafetyCheckState::kRunning &&
          !insecure_credentials.empty()) {
        return PasswordSafetyCheckStateForHighestPriorityWarningType(
            insecure_credentials);
      }

      if (previous_check_state == PasswordSafetyCheckState::kRunning &&
          insecure_credentials.empty()) {
        return PasswordSafetyCheckState::kSafe;
      }

      if (previous_check_state != PasswordSafetyCheckState::kSafe) {
        return previous_check_state;
      }

      return PasswordSafetyCheckState::kDefault;
    default:
      return PasswordSafetyCheckState::kDefault;
  }
}

password_manager::InsecurePasswordCounts DictToInsecurePasswordCounts(
    const base::Value::Dict& dict) {
  password_manager::InsecurePasswordCounts insecure_password_counts = {
      /* compromised */
      dict.FindInt(kSafetyCheckCompromisedPasswordsCountKey).value_or(0),
      /* dismissed */
      dict.FindInt(kSafetyCheckDismissedPasswordsCountKey).value_or(0),
      /* reused */
      dict.FindInt(kSafetyCheckReusedPasswordsCountKey).value_or(0),
      /* weak */
      dict.FindInt(kSafetyCheckWeakPasswordsCountKey).value_or(0)};

  return insecure_password_counts;
}
