// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"

#import "base/ranges/algorithm.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/l10n/time_format.h"

using password_manager::CredentialUIEntry;

namespace {

// Amount of time after which the timestamp is shown instead of "just now".
constexpr base::TimeDelta kJustCheckedTimeThreshold = base::Minutes(1);

}  // anonymous namespace

namespace password_manager {

bool operator==(const InsecurePasswordCounts& lhs,
                const InsecurePasswordCounts& rhs) {
  std::tuple lhs_tuple = std::tie(lhs.compromised_count, lhs.dismissed_count,
                                  lhs.reused_count, lhs.weak_count);
  std::tuple rhs_tuple = std::tie(rhs.compromised_count, rhs.dismissed_count,
                                  rhs.reused_count, rhs.weak_count);
  return lhs_tuple == rhs_tuple;
}

bool IsCredentialUnmutedCompromised(const CredentialUIEntry& credential) {
  return IsCompromised(credential) && !credential.IsMuted();
}

WarningType GetWarningOfHighestPriority(
    InsecurePasswordCounts insecure_password_counts) {
  if (insecure_password_counts.compromised_count > 0) {
    return WarningType::kCompromisedPasswordsWarning;
  }

  if (insecure_password_counts.reused_count > 0) {
    return WarningType::kReusedPasswordsWarning;
  }

  if (insecure_password_counts.weak_count > 0) {
    return WarningType::kWeakPasswordsWarning;
  }

  if (insecure_password_counts.dismissed_count > 0) {
    return WarningType::kDismissedWarningsWarning;
  }

  return WarningType::kNoInsecurePasswordsWarning;
}

WarningType GetWarningOfHighestPriority(
    const std::vector<CredentialUIEntry>& insecure_credentials) {
  InsecurePasswordCounts insecure_password_counts =
      CountInsecurePasswordsPerInsecureType(insecure_credentials);

  return GetWarningOfHighestPriority(insecure_password_counts);
}

InsecurePasswordCounts CountInsecurePasswordsPerInsecureType(
    const std::vector<password_manager::CredentialUIEntry>&
        insecure_credentials) {
  InsecurePasswordCounts counts{};
  std::map<std::u16string, int> reused_passwords;
  for (const auto& credential : insecure_credentials) {
    // If a compromised credential is muted, we don't want to take it into
    // account in the compromised count.
    if (credential.IsMuted()) {
      counts.dismissed_count++;
    } else if (IsCompromised(credential)) {
      counts.compromised_count++;
    }
    if (credential.IsReused()) {
      counts.reused_count++;
    }
    if (credential.IsWeak()) {
      counts.weak_count++;
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
      return counts.compromised_count;
    case WarningType::kReusedPasswordsWarning:
      return counts.reused_count;
    case WarningType::kWeakPasswordsWarning:
      return counts.weak_count;
    case WarningType::kDismissedWarningsWarning:
      return counts.dismissed_count;
    case WarningType::kNoInsecurePasswordsWarning:
      return 0;
  }
}

NSString* FormatElapsedTimeSinceLastCheck(
    std::optional<base::Time> last_completed_check) {
  if (!last_completed_check.has_value()) {
    // The title case format is only used in the Password Checkup Homepage as of
    // now and it is currently not possible to reach this page if no check has
    // yet been completed. There is therefore no need for now to have a title
    // case version of "Check never run"
    return l10n_util::GetNSString(IDS_IOS_CHECK_NEVER_RUN);
  }

  base::TimeDelta elapsed_time =
      base::Time::Now() - last_completed_check.value();

  // If check finished in less than `kJustCheckedTimeThreshold` show "Checked
  // just now" instead of timestamp.
  if (elapsed_time < kJustCheckedTimeThreshold) {
    return l10n_util::GetNSString(IDS_IOS_CHECK_FINISHED_JUST_NOW);
  }

  std::u16string timestamp = ui::TimeFormat::SimpleWithMonthAndYear(
      ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_LONG, elapsed_time,
      true);

  return l10n_util::GetNSStringF(IDS_IOS_PASSWORD_CHECKUP_LAST_COMPLETED_CHECK,
                                 timestamp);
}

std::vector<CredentialUIEntry> GetPasswordsForWarningType(
    WarningType warning_type,
    const std::vector<CredentialUIEntry>& insecure_credentials) {
  std::vector<CredentialUIEntry> filtered_credentials;

  switch (warning_type) {
    case WarningType::kCompromisedPasswordsWarning:
      base::ranges::copy_if(insecure_credentials,
                            std::back_inserter(filtered_credentials),
                            IsCredentialUnmutedCompromised);
      break;
    case WarningType::kWeakPasswordsWarning:
      base::ranges::copy_if(insecure_credentials,
                            std::back_inserter(filtered_credentials),
                            std::mem_fn(&CredentialUIEntry::IsWeak));
      break;
    case WarningType::kReusedPasswordsWarning:
      base::ranges::copy_if(insecure_credentials,
                            std::back_inserter(filtered_credentials),
                            std::mem_fn(&CredentialUIEntry::IsReused));
      break;
    case WarningType::kDismissedWarningsWarning:
      base::ranges::copy_if(insecure_credentials,
                            std::back_inserter(filtered_credentials),
                            std::mem_fn(&CredentialUIEntry::IsMuted));
      break;
    case WarningType::kNoInsecurePasswordsWarning:
      NOTREACHED();
  }

  return filtered_credentials;
}

}  // namespace password_manager
