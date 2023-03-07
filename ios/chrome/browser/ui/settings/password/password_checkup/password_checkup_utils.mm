// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_utils.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/l10n/time_format.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Amount of time after which the timestamp is shown instead of "just now".
constexpr base::TimeDelta kJustCheckedTimeThreshold = base::Minutes(1);

}  // anonymous namespace

namespace password_manager {

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
    } else if (IsCompromised(credential)) {
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

// TODO(crbug.com/1406540): Title case returned string for Password Checkup
// homepage.
NSString* FormatElapsedTimeSinceLastCheck(base::Time last_completed_check) {
  // `last_completed_check` is 0.0 when the check has never completely run
  // before.
  if (last_completed_check == base::Time()) {
    return l10n_util::GetNSString(IDS_IOS_CHECK_NEVER_RUN);
  }

  base::TimeDelta elapsed_time = base::Time::Now() - last_completed_check;

  std::u16string timestamp;
  // If check finished in less than `kJustCheckedTimeThreshold` show
  // "just now" instead of timestamp.
  if (elapsed_time < kJustCheckedTimeThreshold) {
    timestamp = l10n_util::GetStringUTF16(IDS_IOS_CHECK_FINISHED_JUST_NOW);
  } else {
    timestamp = ui::TimeFormat::SimpleWithMonthAndYear(
        ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_LONG,
        elapsed_time, true);
  }

  return l10n_util::GetNSStringF(IDS_IOS_LAST_COMPLETED_CHECK, timestamp);
}

}  // namespace password_manager
