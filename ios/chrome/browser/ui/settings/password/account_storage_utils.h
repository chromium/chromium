// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_ACCOUNT_STORAGE_UTILS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_ACCOUNT_STORAGE_UTILS_H_

namespace syncer {
class SyncService;
}

namespace password_manager {

class AffiliatedGroup;
struct CredentialUIEntry;

// Whether the top-level passwords view should show a special icon next to
// `credential` that indicates the credential is only saved on this device (not
// in the user's Google account).
// Note that even if no such icon is shown, it's not guaranteed that the
// credential was already uploaded to the account - only that it will be once
// any sync errors are resolved (e.g. pending custom passphrase).
bool ShouldShowLocalOnlyIcon(const CredentialUIEntry& credential,
                             const syncer::SyncService* sync_service);

bool ShouldShowLocalOnlyIconForGroup(const AffiliatedGroup& credential,
                                     const syncer::SyncService* sync_service);

}  // namespace password_manager

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_ACCOUNT_STORAGE_UTILS_H_
