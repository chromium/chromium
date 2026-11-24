// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_PASSWORD_MODEL_ACCOUNT_STORAGE_UTILS_H_
#define IOS_CHROME_BROWSER_SETTINGS_PASSWORD_MODEL_ACCOUNT_STORAGE_UTILS_H_

namespace syncer {
class SyncService;
}

namespace password_manager {
class AffiliatedGroup;
struct CredentialUIEntry;
}  // namespace password_manager

// Whether the top-level passwords view should show a special icon next to
// `credential` that indicates the credential is only saved on this device (not
// in the user's Google account).
// Note that even if no such icon is shown, it's not guaranteed that the
// credential was already uploaded to the account - only that it will be once
// any sync errors are resolved (e.g. pending custom passphrase).
bool ShouldShowLocalOnlyIcon(
    const password_manager::CredentialUIEntry& credential,
    const syncer::SyncService* sync_service);

bool ShouldShowLocalOnlyIconForGroup(
    const password_manager::AffiliatedGroup& affiliated_group,
    const syncer::SyncService* sync_service);

#endif  // IOS_CHROME_BROWSER_SETTINGS_PASSWORD_MODEL_ACCOUNT_STORAGE_UTILS_H_
