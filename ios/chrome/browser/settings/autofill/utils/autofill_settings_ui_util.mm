// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/utils/autofill_settings_ui_util.h"

#import "base/i18n/message_formatter.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Returns a message for selections that contain local and server data.
NSString* GetDeletionConfirmationTitleWithServerData(
    const std::u16string& user_email) {
  return l10n_util::GetNSStringF(
      IDS_IOS_SETTINGS_AUTOFILL_DELETE_TITLE_LOCAL_AND_SERVER, user_email);
}

// Returns a message for selections that contain only local data.
NSString* GetDeletionConfirmationTitleLocal() {
  return l10n_util::GetNSString(IDS_IOS_SETTINGS_AUTOFILL_DELETE_TITLE_LOCAL);
}

}  // namespace

// Returns a deletion confirmation string. This is the version before
// Autofill AI entities are added.
NSString* GetDeletionConfirmationString(int profile_count,
                                        bool has_local_profile,
                                        bool has_account_profile,
                                        bool has_home_work_name_email_profile,
                                        const std::u16string& user_email) {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableSupportForHomeAndWork)) {
    if (has_account_profile) {
      std::u16string pattern = l10n_util::GetStringUTF16(
          IDS_IOS_SETTINGS_AUTOFILL_DELETE_ACCOUNT_ADDRESS_CONFIRMATION_TITLE);
      std::u16string confirmationString =
          base::i18n::MessageFormatter::FormatWithNamedArgs(
              pattern, "email", user_email, "count", profile_count);
      return base::SysUTF16ToNSString(confirmationString);
    }
    return l10n_util::GetPluralNSStringF(
        IDS_IOS_SETTINGS_AUTOFILL_DELETE_LOCAL_ADDRESS_CONFIRMATION_TITLE,
        profile_count);
  }

  if (has_local_profile && has_account_profile &&
      has_home_work_name_email_profile) {
    return l10n_util::GetNSString(
        IDS_IOS_SETTINGS_AUTOFILL_DELETE_LOCAL_ACCOUNT_HOME_WORK_ADDRESS_CONFIRMATION_TITLE);
  }

  if (has_local_profile && has_home_work_name_email_profile) {
    return l10n_util::GetNSString(
        IDS_IOS_SETTINGS_AUTOFILL_DELETE_LOCAL_HOME_WORK_ADDRESS_CONFIRMATION_TITLE);
  }

  if (has_local_profile && has_account_profile) {
    return l10n_util::GetNSStringF(
        IDS_IOS_SETTINGS_AUTOFILL_DELETE_LOCAL_ACCOUNT_ADDRESS_CONFIRMATION_TITLE,
        user_email);
  }

  if (has_account_profile && has_home_work_name_email_profile) {
    return l10n_util::GetNSString(
        IDS_IOS_SETTINGS_AUTOFILL_DELETE_ACCOUNT_HOME_WORK_ADDRESS_CONFIRMATION_TITLE);
  }

  if (has_account_profile) {
    return l10n_util::GetNSStringF(
        IDS_IOS_SETTINGS_AUTOFILL_DELETE_ACCOUNT_ADDRESSES_CONFIRMATION_TITLE,
        user_email);
  }

  if (has_home_work_name_email_profile) {
    return l10n_util::GetNSString(
        IDS_IOS_SETTINGS_AUTOFILL_DELETE_HOME_WORK_ADDRESS_CONFIRMATION_TITLE);
  }

  return l10n_util::GetNSString(
      IDS_IOS_SETTINGS_AUTOFILL_DELETE_LOCAL_ADDRESSES_CONFIRMATION_TITLE);
}

NSString* GetDeletionConfirmationStringWithEntities(
    bool has_server_data,
    const std::u16string& user_email) {
  if (has_server_data) {
    return GetDeletionConfirmationTitleWithServerData(user_email);
  } else {
    return GetDeletionConfirmationTitleLocal();
  }
}
