// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_ENTERPRISE_UTILS_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_ENTERPRISE_UTILS_H_

#import <UIKit/UIKit.h>

#include "components/sync/base/user_selectable_type.h"

namespace syncer {
class SyncService;
}

// Returns YES if some account restrictions are set.
bool IsRestrictAccountsToPatternsEnabled();

// Returns true if the `dataType` is managed by policies (i.e. is not syncable).
bool IsManagedSyncDataType(syncer::SyncService* sync_service,
                           syncer::UserSelectableType dataType);

// Returns true if any data type is managed by policies (i.e. is not syncable).
bool HasManagedSyncDataType(syncer::SyncService* sync_service);

// true if sync is disabled.
bool IsSyncDisabledByPolicy(syncer::SyncService* sync_service);

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_ENTERPRISE_UTILS_H_
