// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/utils/autofill_settings_ui_util.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Returns whether the given `item` can be deleted.
bool IsDeletableItem(TableViewItem* item) {
  AutofillAIEntityItem* aiItem =
      base::apple::ObjCCast<AutofillAIEntityItem>(item);
  return aiItem && !aiItem.isServerWalletItem;
}

}  // namespace

// Returns a deletion confirmation string.
NSString* GetDeletionConfirmationString(bool has_local_profile,
                                        bool has_account_profile,
                                        bool has_home_work_name_email_profile,
                                        const std::u16string& user_email) {
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

NSString* GetDeletionConfirmationStringWithEntities() {
  return l10n_util::GetNSString(IDS_IOS_SETTINGS_AUTOFILL_DELETE_TITLE_LOCAL);
}

BOOL ContainsLocalEntity(NSArray<TableViewItem*>* items) {
  for (TableViewItem* item in items) {
    if (IsDeletableItem(item)) {
      return YES;
    }
  }
  return NO;
}
