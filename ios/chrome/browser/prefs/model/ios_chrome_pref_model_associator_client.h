// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PREFS_MODEL_IOS_CHROME_PREF_MODEL_ASSOCIATOR_CLIENT_H_
#define IOS_CHROME_BROWSER_PREFS_MODEL_IOS_CHROME_PREF_MODEL_ASSOCIATOR_CLIENT_H_

#include <string_view>

#include "components/sync_preferences/pref_model_associator_client.h"
#include "ios/chrome/browser/sync/model/prefs/ios_chrome_syncable_prefs_database.h"

class IOSChromePrefModelAssociatorClient
    : public sync_preferences::PrefModelAssociatorClient {
 public:
  IOSChromePrefModelAssociatorClient();
  IOSChromePrefModelAssociatorClient(
      const IOSChromePrefModelAssociatorClient&) = delete;
  IOSChromePrefModelAssociatorClient& operator=(
      const IOSChromePrefModelAssociatorClient&) = delete;

 private:
  ~IOSChromePrefModelAssociatorClient() override;

  // sync_preferences::PrefModelAssociatorClient implementation.
  base::Value MaybeMergePreferenceValues(
      std::string_view pref_name,
      const base::Value& local_value,
      const base::Value& server_value) const override;
  const sync_preferences::SyncablePrefsDatabase& GetSyncablePrefsDatabase()
      const override;

  browser_sync::IOSChromeSyncablePrefsDatabase
      ios_chrome_syncable_prefs_database_;
};

#endif  // IOS_CHROME_BROWSER_PREFS_MODEL_IOS_CHROME_PREF_MODEL_ASSOCIATOR_CLIENT_H_
