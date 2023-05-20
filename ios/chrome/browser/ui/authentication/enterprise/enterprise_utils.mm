// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"

#import "base/containers/fixed_flat_map.h"
#import "base/values.h"
#import "components/policy/policy_constants.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

bool IsRestrictAccountsToPatternsEnabled() {
  return !GetApplicationContext()
              ->GetLocalState()
              ->GetList(prefs::kRestrictAccountsToPatterns)
              .empty();
}

bool IsManagedSyncDataType(syncer::SyncService* sync_service,
                           syncer::UserSelectableType data_type) {
  return sync_service &&
         sync_service->GetUserSettings()->IsTypeManagedByPolicy(data_type);
}

bool HasManagedSyncDataType(syncer::SyncService* sync_service) {
  for (syncer::UserSelectableType type : syncer::UserSelectableTypeSet::All()) {
    if (IsManagedSyncDataType(sync_service, type)) {
      return true;
    }
  }
  return false;
}

bool IsSyncDisabledByPolicy(syncer::SyncService* sync_service) {
  return sync_service->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
}
