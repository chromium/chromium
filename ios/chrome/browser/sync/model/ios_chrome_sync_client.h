// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_IOS_CHROME_SYNC_CLIENT_H__
#define IOS_CHROME_BROWSER_SYNC_MODEL_IOS_CHROME_SYNC_CLIENT_H__

#include <memory>

#include "base/files/file_path.h"
#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/service/sync_client.h"
#include "components/trusted_vault/trusted_vault_client.h"

class ChromeBrowserState;

namespace browser_sync {
class LocalDataQueryHelper;
class LocalDataMigrationHelper;
class SyncApiComponentFactoryImpl;
}  // namespace browser_sync

namespace password_manager {
class PasswordStoreInterface;
}  // namespace password_manager

class IOSChromeSyncClient : public syncer::SyncClient {
 public:
  explicit IOSChromeSyncClient(ChromeBrowserState* browser_state);

  IOSChromeSyncClient(const IOSChromeSyncClient&) = delete;
  IOSChromeSyncClient& operator=(const IOSChromeSyncClient&) = delete;

  ~IOSChromeSyncClient() override;

  // SyncClient implementation.
  PrefService* GetPrefService() override;
  signin::IdentityManager* GetIdentityManager() override;
  base::FilePath GetLocalSyncBackendFolder() override;
  syncer::ModelTypeController::TypeVector CreateModelTypeControllers(
      syncer::SyncService* sync_service) override;
  trusted_vault::TrustedVaultClient* GetTrustedVaultClient() override;
  syncer::SyncInvalidationsService* GetSyncInvalidationsService() override;
  scoped_refptr<syncer::ExtensionsActivity> GetExtensionsActivity() override;
  syncer::SyncApiComponentFactory* GetSyncApiComponentFactory() override;
  bool IsCustomPassphraseAllowed() override;
  bool IsPasswordSyncAllowed() override;
  void SetPasswordSyncAllowedChangeCb(
      const base::RepeatingClosure& cb) override;
  void GetLocalDataDescriptions(
      syncer::ModelTypeSet types,
      base::OnceCallback<void(
          std::map<syncer::ModelType, syncer::LocalDataDescription>)> callback)
      override;
  void TriggerLocalDataMigration(syncer::ModelTypeSet types) override;
  void RegisterTrustedVaultAutoUpgradeSyntheticFieldTrial(
      const syncer::TrustedVaultAutoUpgradeSyntheticFieldTrialGroup& group)
      override;

 private:
  const raw_ptr<ChromeBrowserState> browser_state_;

  // The sync api component factory in use by this client.
  std::unique_ptr<browser_sync::SyncApiComponentFactoryImpl> component_factory_;

  scoped_refptr<password_manager::PasswordStoreInterface>
      profile_password_store_;
  scoped_refptr<password_manager::PasswordStoreInterface>
      account_password_store_;

  std::unique_ptr<browser_sync::LocalDataQueryHelper> local_data_query_helper_;
  std::unique_ptr<browser_sync::LocalDataMigrationHelper>
      local_data_migration_helper_;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_IOS_CHROME_SYNC_CLIENT_H__
