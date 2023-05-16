// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_MANAGER_UTIL_IOS_H_
#define IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_MANAGER_UTIL_IOS_H_

#include "components/sync/service/sync_service.h"

namespace syncer {
class SyncService;
}

namespace password_manager_util {

// Returns true if the user is syncing passwords to Google Account with normal
// encryption.
bool IsPasswordSyncNormalEncryptionEnabled(
    const syncer::SyncService* sync_service);

}  // namespace password_manager_util

#endif  // IOS_CHROME_COMMON_UI_UTIL_PASSWORD_MANAGER_UTIL_IOS_H_
