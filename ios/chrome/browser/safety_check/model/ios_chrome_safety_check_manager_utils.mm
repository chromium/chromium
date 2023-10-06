// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_utils.h"

#import "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
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
      if (!password_manager::features::IsPasswordCheckupEnabled() &&
          !insecure_credentials.empty()) {
        return PasswordSafetyCheckState::kUnmutedCompromisedPasswords;
      }
      return PasswordSafetyCheckState::kSignedOut;
    case PasswordCheckState::kOffline:
    case PasswordCheckState::kQuotaLimit:
    case PasswordCheckState::kOther:
      if (!password_manager::features::IsPasswordCheckupEnabled() &&
          !insecure_credentials.empty()) {
        return PasswordSafetyCheckState::kUnmutedCompromisedPasswords;
      }
      return PasswordSafetyCheckState::kError;
    case PasswordCheckState::kCanceled:
      if (!password_manager::features::IsPasswordCheckupEnabled() &&
          !insecure_credentials.empty()) {
        return PasswordSafetyCheckState::kUnmutedCompromisedPasswords;
      }
      return PasswordSafetyCheckState::kDefault;
    case PasswordCheckState::kIdle:
      if (!password_manager::features::IsPasswordCheckupEnabled() &&
          !insecure_credentials.empty()) {
        return PasswordSafetyCheckState::kUnmutedCompromisedPasswords;
      }

      if (previous_check_state == PasswordSafetyCheckState::kRunning &&
          !insecure_credentials.empty()) {
        return PasswordSafetyCheckStateForHighestPriorityWarningType(
            insecure_credentials);
      }

      if (previous_check_state == PasswordSafetyCheckState::kRunning &&
          insecure_credentials.empty()) {
        return PasswordSafetyCheckState::kSafe;
      }

      return PasswordSafetyCheckState::kDefault;
    default:
      return PasswordSafetyCheckState::kDefault;
  }
}
