// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_THEME_SYNCABLE_SERVICE_IOS_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_THEME_SYNCABLE_SERVICE_IOS_H_

#import <memory>
#import <optional>
#import <string>

#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "components/sync/model/sync_change.h"
#import "components/sync/model/sync_change_processor.h"
#import "components/sync/model/sync_data.h"
#import "components/sync/model/syncable_service.h"

// A `SyncableService` implementation for `THEMES_IOS`.
//
// This service acts as the bridge between the Chrome Sync engine and the
// `HomeBackgroundCustomizationService`. It handles applying remote changes to
// the local client and uploading local changes to the sync server.
class ThemeSyncableServiceIOS : public syncer::SyncableService {
 public:
  ThemeSyncableServiceIOS();
  ThemeSyncableServiceIOS(const ThemeSyncableServiceIOS&) = delete;
  ThemeSyncableServiceIOS& operator=(const ThemeSyncableServiceIOS&) = delete;
  ~ThemeSyncableServiceIOS() override;

  // `syncer::SyncableService` overrides.
  void WaitUntilReadyToSync(base::OnceClosure done) override;
  void WillStartInitialSync() override;
  std::optional<syncer::ModelError> MergeDataAndStartSyncing(
      syncer::DataType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor) override;
  void StopSyncing(syncer::DataType type) override;
  void OnBrowserShutdown(syncer::DataType type) override;
  void StayStoppedAndMaybeClearData(syncer::DataType type) override;
  std::optional<syncer::ModelError> ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override;
  std::string GetClientTag(
      const syncer::EntityData& entity_data) const override;
  base::WeakPtr<syncer::SyncableService> AsWeakPtr() override;

  // Returns true if actively syncing.
  bool IsSyncing() const { return sync_processor_ != nullptr; }

 private:
  // Sync's handler for outgoing changes. Non-null between
  // `MergeDataAndStartSyncing()` and `StopSyncing()`.
  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;

  // Validates that operations occur on the same sequence this object was
  // created.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ThemeSyncableServiceIOS> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_THEME_SYNCABLE_SERVICE_IOS_H_
