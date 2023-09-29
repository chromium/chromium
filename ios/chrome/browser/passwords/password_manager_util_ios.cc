// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/password_manager_util_ios.h"

#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/sync/service/sync_service.h"

namespace password_manager_util {

bool IsPasswordSyncNormalEncryptionEnabled(
    const syncer::SyncService* sync_service) {
  return password_manager::sync_util::GetPasswordSyncState(sync_service) ==
         password_manager::SyncState::kSyncingNormalEncryption;
}

}  // namespace password_manager_util
