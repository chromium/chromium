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
    } else if (credential.IsPhished() || credential.IsLeaked()) {
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

int GetPasswordCountForWarningType(
    WarningType warningType,
    const std::vector<password_manager::CredentialUIEntry>&
        insecure_credentials) {
  int passwordCount = 0;
  switch (warningType) {
    case WarningType::kCompromisedPasswordsWarning:
      passwordCount = std::count_if(
          insecure_credentials.begin(), insecure_credentials.end(),
          [](const auto& credential) {
            return (credential.IsLeaked() || credential.IsPhished()) &&
                   !credential.IsMuted();
          });
      break;
    case WarningType::kReusedPasswordsWarning:
      passwordCount = std::count_if(
          insecure_credentials.begin(), insecure_credentials.end(),
          [](const auto& credential) { return credential.IsReused(); });
      break;
    case WarningType::kWeakPasswordsWarning:
      passwordCount = std::count_if(
          insecure_credentials.begin(), insecure_credentials.end(),
          [](const auto& credential) { return credential.IsWeak(); });
      break;
    case WarningType::kDismissedWarningsWarning:
      passwordCount = std::count_if(
          insecure_credentials.begin(), insecure_credentials.end(),
          [](const auto& credential) { return credential.IsMuted(); });
      break;
    case WarningType::kNoInsecurePasswordsWarning:
      break;
  }

  return passwordCount;
}
