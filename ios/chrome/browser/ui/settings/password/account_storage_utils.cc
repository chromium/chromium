// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/account_storage_utils.h"

#import "base/containers/flat_set.h"
#import "base/containers/span.h"
#import "base/notreached.h"
#import "base/ranges/algorithm.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager_client.h"
#import "components/password_manager/core/browser/ui/affiliated_group.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"

namespace password_manager {

bool ShouldShowLocalOnlyIcon(const CredentialUIEntry& credential,
                             const syncer::SyncService* sync_service) {
  if (credential.blocked_by_user) {
    // The account/local concept is harder to grasp for "Never saved" pages, do
    // not distinguish. It's also less important to back those up anyway.
    return false;
  }

  if (credential.stored_in.contains(PasswordForm::Store::kAccountStore)) {
    // If the data is stored both in kProfileStore and kAccountStore, it's
    // backed up, no need to bother the user.
    return false;
  }

  if (!credential.passkey_credential_id.empty()) {
    // The local -> account migration flow only covers passwords.
    return false;
  }

  // Syncing and signed-out users shouldn't see the icon.
  // TODO(crbug.com/40066949): Remove usage of IsSyncFeatureEnabled() after
  // kSync users are migrated to kSignin in phase 3. See ConsentLevel::kSync
  // documentation for details.
  if (sync_service->IsSyncFeatureEnabled() ||
      sync_service->HasDisableReason(
          syncer::SyncService::DisableReason::DISABLE_REASON_NOT_SIGNED_IN)) {
    return false;
  }

  return true;
}

bool ShouldShowLocalOnlyIconForGroup(const AffiliatedGroup& affiliated_group,
                                     const syncer::SyncService* sync_service) {
  return base::ranges::any_of(affiliated_group.GetCredentials(),
                              [&](const CredentialUIEntry& c) {
                                return ShouldShowLocalOnlyIcon(c, sync_service);
                              });
}

}  // namespace password_manager
