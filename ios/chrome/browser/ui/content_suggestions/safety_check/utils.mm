// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/safety_check/utils.h"

#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/passwords/password_checkup_utils.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/common/channel_info.h"
#import "url/gurl.h"

namespace {

// Returns the number of unique warning types found in `counts`.
//
// NOTE: Only considers compromised, reused, and weak passwords. (Does not
// consider dismissed passwords.)
int UniqueWarningTypeCount(
    const std::vector<password_manager::CredentialUIEntry>&
        compromised_credentials) {
  password_manager::InsecurePasswordCounts counts =
      password_manager::CountInsecurePasswordsPerInsecureType(
          compromised_credentials);

  int type_count = 0;

  if (counts.compromised_count > 0) {
    type_count++;
  }

  if (counts.reused_count > 0) {
    type_count++;
  }

  if (counts.weak_count > 0) {
    type_count++;
  }

  return type_count;
}

}  // namespace

void HandleSafetyCheckUpdateChromeTap(const GURL& chrome_upgrade_url,
                                      id<ApplicationCommands> handler) {
  switch (::GetChannel()) {
    case version_info::Channel::STABLE:
    case version_info::Channel::BETA:
    case version_info::Channel::DEV:
    case version_info::Channel::CANARY: {
      OpenNewTabCommand* command =
          [OpenNewTabCommand commandWithURLFromChrome:chrome_upgrade_url];

      [handler openURLInNewTab:command];

      break;
    }
    case version_info::Channel::UNKNOWN:
      break;
  }
}

void HandleSafetyCheckPasswordTap(
    std::vector<password_manager::CredentialUIEntry>& compromised_credentials,
    id<ApplicationCommands> handler) {
  // If the new Password Checkup is disabled, always navigate users to the
  // Password Issues overview screen for compromised passwords.
  if (!password_manager::features::IsPasswordCheckupEnabled()) {
    [handler showPasswordIssuesWithWarningType:password_manager::WarningType::
                                                   kCompromisedPasswordsWarning
                                      referrer:password_manager::
                                                   PasswordCheckReferrer::
                                                       kSafetyCheckMagicStack];

    return;
  }

  // If there's only one compromised credential, navigate users to the detail
  // view for that particular credential.
  if (compromised_credentials.size() == 1) {
    password_manager::CredentialUIEntry credential =
        compromised_credentials.front();

    [handler showPasswordDetailsForCredential:credential showCancelButton:YES];

    return;
  }

  int unique_warning_type_count =
      UniqueWarningTypeCount(compromised_credentials);

  // If there are multiple passwords (of the same warning type),
  // navigate users to the Password Checkup overview screen for that particular
  // warning type.
  if (unique_warning_type_count == 1) {
    password_manager::WarningType type =
        password_manager::GetWarningOfHighestPriority(compromised_credentials);

    [handler showPasswordIssuesWithWarningType:type
                                      referrer:password_manager::
                                                   PasswordCheckReferrer::
                                                       kSafetyCheckMagicStack];
  }

  // If there are multiple passwords (with multiple warning types), or no
  // compromised credentials at all, navigate users to the Password Checkup
  // overview screen.
  [handler showPasswordCheckupPageForReferrer:
               password_manager::PasswordCheckReferrer::kSafetyCheckMagicStack];
}
