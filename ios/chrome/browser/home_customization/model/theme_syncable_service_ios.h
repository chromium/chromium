// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_THEME_SYNCABLE_SERVICE_IOS_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_THEME_SYNCABLE_SERVICE_IOS_H_

#import <memory>
#import <optional>
#import <string>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "components/sync/model/sync_change.h"
#import "components/sync/model/sync_change_processor.h"
#import "components/sync/model/sync_data.h"
#import "components/sync/model/syncable_service.h"
#import "components/sync/protocol/theme_ios_specifics.pb.h"

// A `SyncableService` implementation for `THEMES_IOS`.
//
// This service acts as the bridge between the Chrome Sync engine and the
// `HomeBackgroundCustomizationService`. It handles applying remote changes to
// the local client and uploading local changes to the sync server.
class ThemeSyncableServiceIOS : public syncer::SyncableService {
 public:
  // Interface defining how this service interacts with themes on the local
  // device.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns the currently active theme on the local device.
    virtual sync_pb::ThemeIosSpecifics GetCurrentTheme() const = 0;

    // Applies `theme` to the local device.
    virtual void ApplyTheme(const sync_pb::ThemeIosSpecifics& theme) = 0;

    // Takes a snapshot of the current local theme.
    virtual void CacheLocalTheme() = 0;

    // Restores the previously snapshotted local theme (e.g., on Sign Out).
    virtual void RestoreCachedTheme() = 0;

    // Returns true if the current theme is valid for syncing.
    // Returns false for local-only theme (e.g., user-uploaded images).
    virtual bool IsCurrentThemeSyncable() const = 0;

    // Returns true if the current theme is managed by enterprise policy.
    // When true, incoming remote changes should be ignored.
    virtual bool IsCurrentThemeManagedByPolicy() const = 0;
  };

  explicit ThemeSyncableServiceIOS(Delegate* delegate);
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

  // Called when the user manually changes the theme locally.
  void OnThemeChanged();

  base::WeakPtr<syncer::SyncableService> AsWeakPtr() override;

  // Returns true if actively syncing.
  bool IsSyncing() const { return sync_processor_ != nullptr; }

 private:
  // Validates the `sync_data` and applies the remote theme to the local device
  // if valid.
  std::optional<syncer::ModelError> ValidateAndApplyRemoteTheme(
      const syncer::SyncData& sync_data);

  // Stops sync and restores the user's pre-sync theme.
  void StopSyncingAndRevertToLocalTheme();

  // Delegate to interact with the local device.
  raw_ptr<Delegate> delegate_ = nullptr;

  // Sync's handler for outgoing changes. Non-null between
  // `MergeDataAndStartSyncing()` and `StopSyncing()`.
  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;

  // Tracks whether changes from the syncer are currently being processed.
  bool processing_syncer_changes_ = false;

  // Validates that operations occur on the same sequence this object was
  // created.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ThemeSyncableServiceIOS> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_THEME_SYNCABLE_SERVICE_IOS_H_
