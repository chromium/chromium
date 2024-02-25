// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_ENTERPRISE_UTILS_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_ENTERPRISE_UTILS_H_

namespace syncer {
class SyncService;
}  // namespace syncer

// Returns true if any data type is managed by policies (i.e. is not syncable).
bool HasManagedSyncDataType(syncer::SyncService* sync_service);

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_ENTERPRISE_UTILS_H_
