// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_GLUE_SYNC_START_UTIL_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_GLUE_SYNC_START_UTIL_H_

#include "components/sync/model/syncable_service.h"

namespace base {
class FilePath;
}

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
//
// `browser_state_path` is used to get a hold of the actual ChromeBrowserState*
// once the request to start sync is safely in UI Thread land.
syncer::SyncableService::StartSyncFlare GetFlareForSyncableService(
    const base::FilePath& browser_state_path);

}  // namespace sync_start_util
}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_GLUE_SYNC_START_UTIL_H_
