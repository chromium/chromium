// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_UTILS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_UTILS_H_

#include "base/observer_list.h"

namespace password_manager {
struct CredentialUIEntry;
}

// Enum which represents possible warnings of Password Checkup on UI.
enum class WarningType {
  kCompromisedPasswordsWarning,
  kReusedPasswordsWarning,
  kWeakPasswordsWarning,
  kDismissedWarningsWarning,
  kNoInsecurePasswordsWarning,
};

// Returns the type of warning with the highest priority, the descending order
// of priority being:
//  1. Compromised password warnings
//  2. Reused password warnings
//  3. Weak password warnings
//  4. Muted warnings warning
//  5. No insecure password warning
WarningType GetWarningOfHighestPriority(
    const std::vector<password_manager::CredentialUIEntry>&
        insecure_credentials);

// Returns the number of saved passwords associated with the warning type passed
// in parameters.
int GetPasswordCountForWarningType(
    WarningType warningType,
    const std::vector<password_manager::CredentialUIEntry>&
        insecure_credentials);

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_UTILS_H_
