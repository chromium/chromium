// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"

#import "base/ranges/algorithm.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_features.h"
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
    const std::vector<CredentialUIEntry>& insecure_credentials) {
  bool has_reused_passwords = false;
  bool has_weak_passwords = false;
  bool has_muted_warnings = false;

  for (const auto& credential : insecure_credentials) {
    if (credential.IsMuted()) {
      has_muted_warnings = true;
    } else if (IsCompromised(credential)) {
      return WarningType::kCompromisedPasswordsWarning;
    }

    // A reused password warning is of higher priority than a weak password
    // warning. So, if the credential is reused, there is no need to verify if
    // it is also weak.
    if (credential.IsReused()) {
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
    absl::optional<base::Time> last_completed_check,
    bool use_title_case) {
  if (!last_completed_check.has_value()) {
    // The title case format is only used in the Password Checkup Homepage as of
    // now and it is currently not possible to reach this page if no check has
    // yet been completed. There is therefore no need for now to have a title
    // case version of "Check never run."
    return l10n_util::GetNSString(IDS_IOS_CHECK_NEVER_RUN);
  }

  base::TimeDelta elapsed_time =
      base::Time::Now() - last_completed_check.value();

  std::u16string timestamp;
  // If check finished in less than `kJustCheckedTimeThreshold` show
  // "just now" instead of timestamp.
  if (elapsed_time < kJustCheckedTimeThreshold) {
    timestamp = l10n_util::GetStringUTF16(
        use_title_case ? IDS_IOS_CHECK_FINISHED_JUST_NOW_TITLE_CASE
                       : IDS_IOS_CHECK_FINISHED_JUST_NOW);
  } else {
    timestamp = ui::TimeFormat::SimpleWithMonthAndYear(
        use_title_case ? ui::TimeFormat::FORMAT_TITLE_CASE_ELAPSED
                       : ui::TimeFormat::FORMAT_ELAPSED,
        ui::TimeFormat::LENGTH_LONG, elapsed_time, true);
  }

  return features::IsPasswordCheckupEnabled()
             ? l10n_util::GetNSStringF(
                   IDS_IOS_PASSWORD_CHECKUP_LAST_COMPLETED_CHECK, timestamp)
             : l10n_util::GetNSStringF(IDS_IOS_LAST_COMPLETED_CHECK, timestamp);
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
      NOTREACHED_NORETURN();
  }

  return filtered_credentials;
}

}  // namespace password_manager
