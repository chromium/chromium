// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/model/enterprise_utils.h"

#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

bool HasManagedSyncDataType(syncer::SyncService* sync_service) {
  if (!sync_service) {
    return false;
  }

  for (syncer::UserSelectableType type : syncer::UserSelectableTypeSet::All()) {
    if (sync_service->GetUserSettings()->IsTypeManagedByPolicy(type)) {
      return true;
    }
  }
  return false;
}
