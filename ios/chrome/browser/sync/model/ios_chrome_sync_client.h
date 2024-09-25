// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_IOS_CHROME_SYNC_CLIENT_H__
#define IOS_CHROME_BROWSER_SYNC_MODEL_IOS_CHROME_SYNC_CLIENT_H__

#import <memory>

#import "base/files/file_path.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/sync/service/sync_client.h"
#import "components/trusted_vault/trusted_vault_client.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace browser_sync {
class LocalDataQueryHelper;
class LocalDataMigrationHelper;
class SyncEngineFactoryImpl;
}  // namespace browser_sync

namespace password_manager {
class PasswordStoreInterface;
}  // namespace password_manager

class IOSChromeSyncClient : public syncer::SyncClient {
 public:
  explicit IOSChromeSyncClient(ProfileIOS* profile);

  IOSChromeSyncClient(const IOSChromeSyncClient&) = delete;
  IOSChromeSyncClient& operator=(const IOSChromeSyncClient&) = delete;

  ~IOSChromeSyncClient() override;

  // SyncClient implementation.
  PrefService* GetPrefService() override;
  signin::IdentityManager* GetIdentityManager() override;
  base::FilePath GetLocalSyncBackendFolder() override;
  trusted_vault::TrustedVaultClient* GetTrustedVaultClient() override;
  syncer::SyncInvalidationsService* GetSyncInvalidationsService() override;
  scoped_refptr<syncer::ExtensionsActivity> GetExtensionsActivity() override;
  syncer::SyncEngineFactory* GetSyncEngineFactory() override;
  bool IsCustomPassphraseAllowed() override;
  bool IsPasswordSyncAllowed() override;
  void SetPasswordSyncAllowedChangeCb(
      const base::RepeatingClosure& cb) override;
  void GetLocalDataDescriptions(
      syncer::DataTypeSet types,
      base::OnceCallback<void(
          std::map<syncer::DataType, syncer::LocalDataDescription>)> callback)
      override;
  void TriggerLocalDataMigration(syncer::DataTypeSet types) override;
  void RegisterTrustedVaultAutoUpgradeSyntheticFieldTrial(
      const syncer::TrustedVaultAutoUpgradeSyntheticFieldTrialGroup& group)
      override;

 private:
  const raw_ptr<ProfileIOS> profile_;

  // The sync engine factory in use by this client.
  std::unique_ptr<browser_sync::SyncEngineFactoryImpl> engine_factory_;

  scoped_refptr<password_manager::PasswordStoreInterface>
      profile_password_store_;
  scoped_refptr<password_manager::PasswordStoreInterface>
      account_password_store_;

  std::unique_ptr<browser_sync::LocalDataQueryHelper> local_data_query_helper_;
  std::unique_ptr<browser_sync::LocalDataMigrationHelper>
      local_data_migration_helper_;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_IOS_CHROME_SYNC_CLIENT_H__
