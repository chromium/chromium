// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/safety_check/utils.h"

#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_state.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/types.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/utils.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/l10n/time_format.h"
#import "url/gurl.h"

namespace {

// The amount of time after which the last run timestamp is shown, instead of
// displaying the last run "just now" text.
constexpr base::TimeDelta kDisplayTimestampThreshold = base::Minutes(1);

// Returns the number of unique warning types found in
// `insecure_password_counts`.
//
// NOTE: Only considers compromised, reused, and weak passwords. (Does not
// consider dismissed passwords.)
int UniqueWarningTypeCount(
    password_manager::InsecurePasswordCounts insecure_password_counts) {
  int type_count = 0;

  if (insecure_password_counts.compromised_count > 0) {
    type_count++;
  }

  if (insecure_password_counts.reused_count > 0) {
    type_count++;
  }

  if (insecure_password_counts.weak_count > 0) {
    type_count++;
  }

  return type_count;
}

// Returns the number of unique warning types found in
// `insecure_credentials`.
//
// NOTE: Only considers compromised, reused, and weak passwords. (Does not
// consider dismissed passwords.)
int UniqueWarningTypeCount(
    const std::vector<password_manager::CredentialUIEntry>&
        insecure_credentials) {
  password_manager::InsecurePasswordCounts insecure_password_counts =
      password_manager::CountInsecurePasswordsPerInsecureType(
          insecure_credentials);

  return UniqueWarningTypeCount(insecure_password_counts);
}

}  // namespace

using password_manager::WarningType;
using password_manager::WarningType::kCompromisedPasswordsWarning;

void HandleSafetyCheckUpdateChromeTap(
    const GURL& chrome_upgrade_url,
    id<ApplicationCommands> applicationHandler) {
  switch (::GetChannel()) {
    case version_info::Channel::STABLE:
    case version_info::Channel::BETA:
    case version_info::Channel::DEV:
    case version_info::Channel::CANARY: {
      OpenNewTabCommand* command =
          [OpenNewTabCommand commandWithURLFromChrome:chrome_upgrade_url];

      [applicationHandler openURLInNewTab:command];

      break;
    }
    case version_info::Channel::UNKNOWN:
      break;
  }
}

void HandleSafetyCheckPasswordTap(
    std::vector<password_manager::CredentialUIEntry>& insecure_credentials,
    password_manager::InsecurePasswordCounts insecure_password_counts,
    id<ApplicationCommands> applicationHandler,
    id<SettingsCommands> settingsHandler) {
  // If there's only one compromised credential, navigate users to the detail
  // view for that particular credential.
  if (insecure_credentials.size() == 1) {
    password_manager::CredentialUIEntry credential =
        insecure_credentials.front();
    [settingsHandler showPasswordDetailsForCredential:credential inEditMode:NO];
    return;
  }

  int unique_warning_type_count =
      insecure_credentials.empty()
          ? UniqueWarningTypeCount(insecure_password_counts)
          : UniqueWarningTypeCount(insecure_credentials);

  // If there are multiple passwords (of the same warning type),
  // navigate users to the Password Checkup overview screen for that particular
  // warning type.
  if (unique_warning_type_count == 1) {
    WarningType type = insecure_credentials.empty()
                           ? password_manager::GetWarningOfHighestPriority(
                                 insecure_password_counts)
                           : password_manager::GetWarningOfHighestPriority(
                                 insecure_credentials);

    [applicationHandler
        showPasswordIssuesWithWarningType:type
                                 referrer:password_manager::
                                              PasswordCheckReferrer::
                                                  kSafetyCheckMagicStack];

    return;
  }

  // If there are multiple passwords (with multiple warning types), or no
  // compromised credentials at all, navigate users to the Password Checkup
  // overview screen.
  base::RecordAction(
      base::UserMetricsAction("MobileMagicStackOpenPasswordCheckup"));

  [applicationHandler
      showPasswordCheckupPageForReferrer:
          password_manager::PasswordCheckReferrer::kSafetyCheckMagicStack];
}

bool InvalidUpdateChromeState(UpdateChromeSafetyCheckState state) {
  return state == UpdateChromeSafetyCheckState::kOutOfDate;
}

bool InvalidPasswordState(PasswordSafetyCheckState state) {
  return state == PasswordSafetyCheckState::kUnmutedCompromisedPasswords ||
         state == PasswordSafetyCheckState::kReusedPasswords ||
         state == PasswordSafetyCheckState::kWeakPasswords;
}

bool InvalidSafeBrowsingState(SafeBrowsingSafetyCheckState state) {
  return state == SafeBrowsingSafetyCheckState::kUnsafe;
}

bool CanRunSafetyCheck(std::optional<base::Time> last_run_time) {
  // The Safety Check should be run if it's never been run before.
  if (!last_run_time.has_value()) {
    return true;
  }

  base::TimeDelta last_run_age = base::Time::Now() - last_run_time.value();

  return last_run_age > TimeDelayForSafetyCheckAutorun();
}

NSString* FormatElapsedTimeSinceLastSafetyCheck(
    std::optional<base::Time> last_run_time) {
  if (!last_run_time.has_value()) {
    return l10n_util::GetNSString(IDS_IOS_CHECK_NEVER_RUN);
  }

  base::TimeDelta elapsed_time = base::Time::Now() - last_run_time.value();

  // If the latest Safety Check run happened less than
  // `kDisplayTimestampThreshold` ago, show "Checked just now" instead of the
  // timestamp.
  if (elapsed_time < kDisplayTimestampThreshold) {
    return l10n_util::GetNSString(IDS_IOS_CHECK_FINISHED_JUST_NOW);
  }

  std::u16string timestamp = ui::TimeFormat::SimpleWithMonthAndYear(
      ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_SHORT,
      elapsed_time, true);

  return l10n_util::GetNSStringF(IDS_IOS_SAFETY_CHECK_LAST_COMPLETED_CHECK,
                                 timestamp);
}

NSString* NameForSafetyCheckItemType(SafetyCheckItemType item_type) {
  switch (item_type) {
    case SafetyCheckItemType::kAllSafe:
      return @"SafetyCheckItemType::kAllSafe";
    case SafetyCheckItemType::kRunning:
      return @"SafetyCheckItemType::kRunning";
    case SafetyCheckItemType::kUpdateChrome:
      return @"SafetyCheckItemType::kUpdateChrome";
    case SafetyCheckItemType::kPassword:
      return @"SafetyCheckItemType::kPassword";
    case SafetyCheckItemType::kSafeBrowsing:
      return @"SafetyCheckItemType::kSafeBrowsing";
    case SafetyCheckItemType::kDefault:
      return @"SafetyCheckItemType::kDefault";
  }
}

SafetyCheckItemType SafetyCheckItemTypeForName(NSString* name) {
  if ([name isEqualToString:@"SafetyCheckItemType::kAllSafe"]) {
    return SafetyCheckItemType::kAllSafe;
  }

  if ([name isEqualToString:@"SafetyCheckItemType::kRunning"]) {
    return SafetyCheckItemType::kRunning;
  }

  if ([name isEqualToString:@"SafetyCheckItemType::kUpdateChrome"]) {
    return SafetyCheckItemType::kUpdateChrome;
  }

  if ([name isEqualToString:@"SafetyCheckItemType::kPassword"]) {
    return SafetyCheckItemType::kPassword;
  }

  if ([name isEqualToString:@"SafetyCheckItemType::kSafeBrowsing"]) {
    return SafetyCheckItemType::kSafeBrowsing;
  }

  if (![name isEqualToString:@"SafetyCheckItemType::kDefault"]) {
    NOTREACHED();
  }

  return SafetyCheckItemType::kDefault;
}
