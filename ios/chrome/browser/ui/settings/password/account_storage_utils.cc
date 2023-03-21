// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/account_storage_utils.h"

#import "base/containers/flat_set.h"
#import "base/containers/span.h"
#import "base/feature_list.h"
#import "base/notreached.h"
#import "base/ranges/algorithm.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager_client.h"
#import "components/password_manager/core/browser/ui/affiliated_group.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_features.h"

namespace password_manager {

bool ShouldShowLocalOnlyIcon(const CredentialUIEntry& credential,
                             SyncState sync_state) {
  if (credential.blocked_by_user) {
    // The account/local concept is harder to grasp for "Never saved" pages, do
    // not distinguish. It's also less important to back those up anyway.
    return false;
  }

  switch (sync_state) {
    case SyncState::kNotSyncing:
      return base::FeatureList::IsEnabled(
          features::kEnablePasswordsAccountStorage);
    case SyncState::kSyncingNormalEncryption:
    case SyncState::kSyncingWithCustomPassphrase:
      return false;
    case SyncState::kAccountPasswordsActiveNormalEncryption:
    case SyncState::kAccountPasswordsActiveWithCustomPassphrase:
      // If the data is stored both in kProfileStore and kAccountStore, it's
      // backed up, no need to bother the user.
      return !credential.stored_in.contains(
                 PasswordForm::Store::kAccountStore) &&
             base::FeatureList::IsEnabled(
                 features::kEnablePasswordsAccountStorage);
    default:
      NOTREACHED();
      return false;
  }
}

bool ShouldShowLocalOnlyIconForGroup(const AffiliatedGroup& affiliated_group,
                                     SyncState sync_state) {
  return base::ranges::any_of(affiliated_group.GetCredentials(),
                              [&](const CredentialUIEntry& c) {
                                return ShouldShowLocalOnlyIcon(c, sync_state);
                              });
}

}  // namespace password_manager
