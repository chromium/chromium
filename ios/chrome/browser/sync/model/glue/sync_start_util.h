// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_GLUE_SYNC_START_UTIL_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_GLUE_SYNC_START_UTIL_H_

#import "components/sync/model/syncable_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

// Various utilities for kicking off sync initialization from data types or
// other services.
namespace ios {
namespace sync_start_util {

// Creates a StartSyncFlare that a SyncableService can use to tell
// syncer::SyncService it needs sync to start ASAP.  Typically this would
// be given to the SyncableService on construction.
//
// The flare built by this function is designed to be Run()able from any thread
// so that non-UI types don't have to deal with posting tasks.
syncer::SyncableService::StartSyncFlare GetFlareForSyncableService(
    ProfileIOS* profile);

}  // namespace sync_start_util
}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_GLUE_SYNC_START_UTIL_H_
