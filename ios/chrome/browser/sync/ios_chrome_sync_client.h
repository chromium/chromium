// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_IOS_CHROME_SYNC_CLIENT_H__
#define IOS_CHROME_BROWSER_SYNC_IOS_CHROME_SYNC_CLIENT_H__

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "components/browser_sync/browser_sync_client.h"

class ChromeBrowserState;

namespace autofill {
class AutofillWebDataService;
}

namespace password_manager {
class PasswordStoreInterface;
}

namespace browser_sync {
class SyncApiComponentFactoryImpl;
}

class IOSChromeSyncClient : public browser_sync::BrowserSyncClient {
 public:
  explicit IOSChromeSyncClient(ChromeBrowserState* browser_state);

  IOSChromeSyncClient(const IOSChromeSyncClient&) = delete;
  IOSChromeSyncClient& operator=(const IOSChromeSyncClient&) = delete;

  ~IOSChromeSyncClient() override;

  // BrowserSyncClient implementation.
  PrefService* GetPrefService() override;
  signin::IdentityManager* GetIdentityManager() override;
  base::FilePath GetLocalSyncBackendFolder() override;
  syncer::ModelTypeStoreService* GetModelTypeStoreService() override;
  syncer::DeviceInfoSyncService* GetDeviceInfoSyncService() override;
  send_tab_to_self::SendTabToSelfSyncService* GetSendTabToSelfSyncService()
      override;
  favicon::FaviconService* GetFaviconService() override;
  history::HistoryService* GetHistoryService() override;
  sync_preferences::PrefServiceSyncable* GetPrefServiceSyncable() override;
  sync_sessions::SessionSyncService* GetSessionSyncService() override;
  syncer::DataTypeController::TypeVector CreateDataTypeControllers(
      syncer::SyncService* sync_service) override;
  invalidation::InvalidationService* GetInvalidationService() override;
  syncer::SyncInvalidationsService* GetSyncInvalidationsService() override;
  syncer::TrustedVaultClient* GetTrustedVaultClient() override;
  scoped_refptr<syncer::ExtensionsActivity> GetExtensionsActivity() override;
  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetControllerDelegateForModelType(syncer::ModelType type) override;
  syncer::SyncApiComponentFactory* GetSyncApiComponentFactory() override;
  syncer::SyncTypePreferenceProvider* GetPreferenceProvider() override;
  void OnLocalSyncTransportDataCleared() override;

 private:
  ChromeBrowserState* const browser_state_;

  // The sync api component factory in use by this client.
  std::unique_ptr<browser_sync::SyncApiComponentFactoryImpl> component_factory_;

  std::unique_ptr<syncer::TrustedVaultClient> trusted_vault_client_;

  // Members that must be fetched on the UI thread but accessed on their
  // respective backend threads.
  scoped_refptr<autofill::AutofillWebDataService> profile_web_data_service_;
  scoped_refptr<autofill::AutofillWebDataService> account_web_data_service_;
  scoped_refptr<password_manager::PasswordStoreInterface>
      profile_password_store_;
  scoped_refptr<password_manager::PasswordStoreInterface>
      account_password_store_;

  // The task runner for the `web_data_service_`, if any.
  scoped_refptr<base::SingleThreadTaskRunner> db_thread_;
};

#endif  // IOS_CHROME_BROWSER_SYNC_IOS_CHROME_SYNC_CLIENT_H__
