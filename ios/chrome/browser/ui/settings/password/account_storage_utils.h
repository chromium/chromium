// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_ACCOUNT_STORAGE_UTILS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_ACCOUNT_STORAGE_UTILS_H_

namespace password_manager {

class AffiliatedGroup;
enum class SyncState;
struct CredentialUIEntry;

// Whether the top-level passwords view should show a special icon next to
// `credential` that indicates it's not backed up to any account.
bool ShouldShowLocalOnlyIcon(const CredentialUIEntry& credential,
                             SyncState sync_state);

bool ShouldShowLocalOnlyIconForGroup(const AffiliatedGroup& credential,
                                     SyncState sync_state);

}  // namespace password_manager

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_ACCOUNT_STORAGE_UTILS_H_
