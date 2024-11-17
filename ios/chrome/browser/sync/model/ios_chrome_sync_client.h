// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_IOS_CHROME_SYNC_CLIENT_H__
#define IOS_CHROME_BROWSER_SYNC_MODEL_IOS_CHROME_SYNC_CLIENT_H__

#import <memory>

#import "base/files/file_path.h"
#import "base/memory/raw_ptr.h"
#import "components/browser_sync/sync_engine_factory_impl.h"
#import "components/sync/service/sync_client.h"

namespace supervised_user {
class SupervisedUserSettingsService;
}  // namespace supervised_user

namespace syncer {
class DataTypeStoreService;
class DeviceInfoSyncService;
}  // namespace syncer

namespace trusted_vault {
class TrustedVaultService;
}  // namespace trusted_vault

class IOSChromeSyncClient : public syncer::SyncClient {
 public:
  IOSChromeSyncClient(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      trusted_vault::TrustedVaultService* trusted_vault_service,
      syncer::SyncInvalidationsService* sync_invalidations_service,
      syncer::DeviceInfoSyncService* device_info_sync_service,
      syncer::DataTypeStoreService* data_type_store_service,
      supervised_user::SupervisedUserSettingsService*
          supervised_user_settings_service);

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
  void RegisterTrustedVaultAutoUpgradeSyntheticFieldTrial(
      const syncer::TrustedVaultAutoUpgradeSyntheticFieldTrialGroup& group)
      override;

 private:
  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<trusted_vault::TrustedVaultService> trusted_vault_service_;
  const raw_ptr<syncer::SyncInvalidationsService> sync_invalidations_service_;
  const raw_ptr<supervised_user::SupervisedUserSettingsService>
      supervised_user_settings_service_;
  browser_sync::SyncEngineFactoryImpl engine_factory_;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_IOS_CHROME_SYNC_CLIENT_H__
