// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_IOS_CHROME_SYNC_CLIENT_H__
#define IOS_CHROME_BROWSER_SYNC_IOS_CHROME_SYNC_CLIENT_H__

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/browser_sync/browser_sync_client.h"

namespace autofill {
class AutofillWebDataService;
}

namespace ios {
class ChromeBrowserState;
}

namespace password_manager {
class PasswordStore;
}

namespace browser_sync {
class ProfileSyncComponentsFactoryImpl;
}

class IOSChromeSyncClient : public browser_sync::BrowserSyncClient {
 public:
  explicit IOSChromeSyncClient(ios::ChromeBrowserState* browser_state);
  ~IOSChromeSyncClient() override;

  // BrowserSyncClient implementation.
  PrefService* GetPrefService() override;
  base::FilePath GetLocalSyncBackendFolder() override;
  syncer::ModelTypeStoreService* GetModelTypeStoreService() override;
  syncer::DeviceInfoSyncService* GetDeviceInfoSyncService() override;
  send_tab_to_self::SendTabToSelfSyncService* GetSendTabToSelfSyncService()
      override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  favicon::FaviconService* GetFaviconService() override;
  history::HistoryService* GetHistoryService() override;
  sync_sessions::SessionSyncService* GetSessionSyncService() override;
  base::Closure GetPasswordStateChangedCallback() override;
  syncer::DataTypeController::TypeVector CreateDataTypeControllers(
      syncer::SyncService* sync_service) override;
  invalidation::InvalidationService* GetInvalidationService() override;
  syncer::TrustedVaultClient* GetTrustedVaultClient() override;
  BookmarkUndoService* GetBookmarkUndoService() override;
  scoped_refptr<syncer::ExtensionsActivity> GetExtensionsActivity() override;
  base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) override;
  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetControllerDelegateForModelType(syncer::ModelType type) override;
  scoped_refptr<syncer::ModelSafeWorker> CreateModelWorkerForGroup(
      syncer::ModelSafeGroup group) override;
  syncer::SyncApiComponentFactory* GetSyncApiComponentFactory() override;
  syncer::SyncTypePreferenceProvider* GetPreferenceProvider() override;

 private:
  ios::ChromeBrowserState* const browser_state_;

  // The sync api component factory in use by this client.
  // TODO(crbug.com/915154): Revert to SyncApiComponentFactory once common
  // controller creation is moved elsewhere.
  std::unique_ptr<browser_sync::ProfileSyncComponentsFactoryImpl>
      component_factory_;

  std::unique_ptr<syncer::TrustedVaultClient> trusted_vault_client_;

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
