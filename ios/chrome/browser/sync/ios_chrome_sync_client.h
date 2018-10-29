// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_IOS_CHROME_SYNC_CLIENT_H__
#define IOS_CHROME_BROWSER_SYNC_IOS_CHROME_SYNC_CLIENT_H__

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/sync/driver/sync_client.h"

namespace autofill {
class AutofillWebDataService;
}

namespace ios {
class ChromeBrowserState;
}

namespace password_manager {
class PasswordStore;
}

namespace syncer {
class DeviceInfoTracker;
class SyncApiComponentFactory;
class SyncService;
}

class IOSChromeSyncClient : public syncer::SyncClient {
 public:
  explicit IOSChromeSyncClient(ios::ChromeBrowserState* browser_state);
  ~IOSChromeSyncClient() override;

  // SyncClient implementation.
  syncer::SyncService* GetSyncService() override;
  PrefService* GetPrefService() override;
  base::FilePath GetLocalSyncBackendFolder() override;
  syncer::ModelTypeStoreService* GetModelTypeStoreService() override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  favicon::FaviconService* GetFaviconService() override;
  history::HistoryService* GetHistoryService() override;
  sync_sessions::SessionSyncService* GetSessionSyncService() override;
  bool HasPasswordStore() override;
  base::Closure GetPasswordStateChangedCallback() override;
  syncer::DataTypeController::TypeVector CreateDataTypeControllers(
      syncer::LocalDeviceInfoProvider* local_device_info_provider) override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  invalidation::InvalidationService* GetInvalidationService() override;
  BookmarkUndoService* GetBookmarkUndoServiceIfExists() override;
  scoped_refptr<syncer::ExtensionsActivity> GetExtensionsActivity() override;
  base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) override;
  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetControllerDelegateForModelType(syncer::ModelType type) override;
  scoped_refptr<syncer::ModelSafeWorker> CreateModelWorkerForGroup(
      syncer::ModelSafeGroup group) override;
  syncer::SyncApiComponentFactory* GetSyncApiComponentFactory() override;

  void SetSyncApiComponentFactoryForTesting(
      std::unique_ptr<syncer::SyncApiComponentFactory> component_factory);

  // Iterates over browser states and returns any trackers that can be found.
  static void GetDeviceInfoTrackers(
      std::vector<const syncer::DeviceInfoTracker*>* trackers);

 private:
  ios::ChromeBrowserState* const browser_state_;

  // The sync api component factory in use by this client.
  std::unique_ptr<syncer::SyncApiComponentFactory> component_factory_;

  // Members that must be fetched on the UI thread but accessed on their
  // respective backend threads.
  scoped_refptr<autofill::AutofillWebDataService> profile_web_data_service_;
  scoped_refptr<autofill::AutofillWebDataService> account_web_data_service_;
  scoped_refptr<password_manager::PasswordStore> password_store_;

  // The task runner for the |web_data_service_|, if any.
  scoped_refptr<base::SingleThreadTaskRunner> db_thread_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeSyncClient);
};

#endif  // IOS_CHROME_BROWSER_SYNC_IOS_CHROME_SYNC_CLIENT_H__
