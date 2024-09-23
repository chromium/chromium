// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_CHECKUP_UTILS_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_CHECKUP_UTILS_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "base/observer_list.h"

namespace base {
class Time;
}  // namespace base

namespace password_manager {

struct CredentialUIEntry;

// Enum which represents possible warnings of Password Checkup on UI.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class WarningType {
  kCompromisedPasswordsWarning = 0,
  kReusedPasswordsWarning = 1,
  kWeakPasswordsWarning = 2,
  kDismissedWarningsWarning = 3,
  kNoInsecurePasswordsWarning = 4,
  kMaxValue = kNoInsecurePasswordsWarning,
};

// Struct used to obtain the password counts associated with the different
// insecure types.
struct InsecurePasswordCounts {
  int compromised_count;
  int dismissed_count;
  int reused_count;
  int weak_count;
};

// Operator overload for the InsecurePasswordCounts struct.
bool operator==(const InsecurePasswordCounts& lhs,
                const InsecurePasswordCounts& rhs);

// Helper function to determine if a credential is compromised but not muted.
bool IsCredentialUnmutedCompromised(const CredentialUIEntry& credential);

// Returns the type of warning with the highest priority using
// `insecure_password_counts`, the descending order of priority being:
//  1. Compromised password warnings
//  2. Reused password warnings
//  3. Weak password warnings
//  4. Muted warnings warning
//  5. No insecure password warning
WarningType GetWarningOfHighestPriority(
    InsecurePasswordCounts insecure_password_counts);

// Returns the type of warning with the highest priority using
// `insecure_credentials`, the descending order of priority being:
//  1. Compromised password warnings
//  2. Reused password warnings
//  3. Weak password warnings
//  4. Muted warnings warning
//  5. No insecure password warning
WarningType GetWarningOfHighestPriority(
    const std::vector<password_manager::CredentialUIEntry>&
        insecure_credentials);

// Returns the number of saved passwords associated with each of the insecure
// types.
InsecurePasswordCounts CountInsecurePasswordsPerInsecureType(
    const std::vector<password_manager::CredentialUIEntry>&
        insecure_credentials);

// Returns the number of saved passwords associated with the warning type passed
// in parameters.
int GetPasswordCountForWarningType(
    WarningType warning_type,
    const std::vector<password_manager::CredentialUIEntry>&
        insecure_credentials);

// Returns saved passwords associated with a warning type.
std::vector<password_manager::CredentialUIEntry> GetPasswordsForWarningType(
    WarningType warning_type,
    const std::vector<password_manager::CredentialUIEntry>&
        insecure_credentials);

// Returns string containing the timestamp of the `last_completed_check`. If the
// check finished less than 1 minute ago string will look like "Checked just
// now", otherwise "Last check X minutes/hours... ago.". If check never run
// string will be "Check never run".
NSString* FormatElapsedTimeSinceLastCheck(
    std::optional<base::Time> last_completed_check);

}  // namespace password_manager

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_CHECKUP_UTILS_H_
