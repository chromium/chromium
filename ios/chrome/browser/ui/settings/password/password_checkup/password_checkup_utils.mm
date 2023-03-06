// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_utils.h"

#import "components/password_manager/core/browser/ui/credential_ui_entry.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WarningType GetWarningOfHighestPriority(
    const std::vector<password_manager::CredentialUIEntry>&
        insecure_credentials) {
  bool has_reused_passwords = false;
  bool has_weak_passwords = false;
  bool has_muted_warnings = false;

  for (const auto& credential : insecure_credentials) {
    if (credential.IsMuted()) {
      has_muted_warnings = true;
    } else if (IsCompromised(credential)) {
      return WarningType::kCompromisedPasswordsWarning;
    } else if (credential.IsReused()) {
      has_reused_passwords = true;
    } else if (credential.IsWeak()) {
      has_weak_passwords = true;
    }
  }

  if (has_reused_passwords) {
    return WarningType::kReusedPasswordsWarning;
  } else if (has_weak_passwords) {
    return WarningType::kWeakPasswordsWarning;
  } else if (has_muted_warnings) {
    return WarningType::kDismissedWarningsWarning;
  }

  return WarningType::kNoInsecurePasswordsWarning;
}

InsecurePasswordCounts CountInsecurePasswordsPerInsecureType(
    const std::vector<password_manager::CredentialUIEntry>&
        insecure_credentials) {
  InsecurePasswordCounts counts{};
  for (const auto& credential : insecure_credentials) {
    // If a compromised credential is muted, we don't want to take it into
    // account in the compromised count.
    if (credential.IsMuted()) {
      counts.dismissedCount++;
    } else if (credential.IsPhished() || credential.IsLeaked()) {
      counts.compromisedCount++;
    }
    if (credential.IsReused()) {
      counts.reusedCount++;
    }
    if (credential.IsWeak()) {
      counts.weakCount++;
    }
  }
  return counts;
}

int GetPasswordCountForWarningType(
    WarningType warningType,
    const std::vector<password_manager::CredentialUIEntry>&
        insecure_credentials) {
  InsecurePasswordCounts counts =
      CountInsecurePasswordsPerInsecureType(insecure_credentials);
  switch (warningType) {
    case WarningType::kCompromisedPasswordsWarning:
      return counts.compromisedCount;
    case WarningType::kReusedPasswordsWarning:
      return counts.reusedCount;
    case WarningType::kWeakPasswordsWarning:
      return counts.weakCount;
    case WarningType::kDismissedWarningsWarning:
      return counts.dismissedCount;
    case WarningType::kNoInsecurePasswordsWarning:
      return 0;
  }
}
