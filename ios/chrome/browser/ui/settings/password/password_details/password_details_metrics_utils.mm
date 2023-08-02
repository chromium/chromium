// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_metrics_utils.h"

namespace password_manager {

WarningType GetWarningTypeForDetailsContext(DetailsContext details_context) {
  switch (details_context) {
      // Compromised issues are displayed as such both when navigating from the
      // compromised UI in Password Checkup and from the Password Manager list.
      // User actions in password details are only associated to other
      // insecurity types (weak, reused, etc) when navigating from the
      // corresponding Password Checkup UI.
    case DetailsContext::kPasswordSettings:
    case DetailsContext::kOutsideSettings:
    case DetailsContext::kCompromisedIssues:
      return WarningType::kCompromisedPasswordsWarning;
    case DetailsContext::kWeakIssues:
      return WarningType::kWeakPasswordsWarning;
    case DetailsContext::kReusedIssues:
      return WarningType::kReusedPasswordsWarning;
    case DetailsContext::kDismissedWarnings:
      return WarningType::kDismissedWarningsWarning;
  }
}

bool ShouldRecordPasswordCheckUserAction(DetailsContext details_context,
                                         bool is_password_compromised) {
  switch (details_context) {
    case DetailsContext::kPasswordSettings:
    case DetailsContext::kOutsideSettings:
      return is_password_compromised;
    case DetailsContext::kCompromisedIssues:
    case DetailsContext::kDismissedWarnings:
    case DetailsContext::kReusedIssues:
    case DetailsContext::kWeakIssues:
      return true;
  }
}

}  // namespace password_manager
