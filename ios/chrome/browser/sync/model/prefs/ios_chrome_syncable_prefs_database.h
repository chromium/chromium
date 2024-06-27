// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_PREFS_IOS_CHROME_SYNCABLE_PREFS_DATABASE_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_PREFS_IOS_CHROME_SYNCABLE_PREFS_DATABASE_H_

#include <map>
#include <string_view>

#include "components/sync_preferences/common_syncable_prefs_database.h"
#include "components/sync_preferences/syncable_prefs_database.h"

namespace browser_sync {

class IOSChromeSyncablePrefsDatabase
    : public sync_preferences::SyncablePrefsDatabase {
 public:
  // Returns the metadata associated to the pref or null if `pref_name` is not
  // syncable.
  std::optional<sync_preferences::SyncablePrefMetadata> GetSyncablePrefMetadata(
      std::string_view pref_name) const override;

  std::map<std::string_view, sync_preferences::SyncablePrefMetadata>
  GetAllSyncablePrefsForTest() const;

 private:
  // This defines the list of preferences that are syncable across all
  // platforms.
  sync_preferences::CommonSyncablePrefsDatabase common_syncable_prefs_database_;
};

}  // namespace browser_sync

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_PREFS_IOS_CHROME_SYNCABLE_PREFS_DATABASE_H_
