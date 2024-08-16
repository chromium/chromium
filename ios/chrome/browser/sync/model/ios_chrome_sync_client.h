// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_IOS_CHROME_SYNC_CLIENT_H__
#define IOS_CHROME_BROWSER_SYNC_MODEL_IOS_CHROME_SYNC_CLIENT_H__

#include <memory>

#include "base/files/file_path.h"
#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/sync_client.h"
#include "components/trusted_vault/trusted_vault_client.h"

class ChromeBrowserState;

namespace browser_sync {
class SyncEngineFactoryImpl;
}  // namespace browser_sync

namespace password_manager {
class PasswordStoreInterface;
}  // namespace password_manager

namespace syncer {
class SyncService;
}  // namespace syncer

class IOSChromeSyncClient : public syncer::SyncClient {
 public:
  explicit IOSChromeSyncClient(ChromeBrowserState* browser_state);

  IOSChromeSyncClient(const IOSChromeSyncClient&) = delete;
  IOSChromeSyncClient& operator=(const IOSChromeSyncClient&) = delete;

  ~IOSChromeSyncClient() override;

  // TODO(crbug.com/335688372): Inline this in the sync_service_factory.mm.
  syncer::DataTypeController::TypeVector CreateDataTypeControllers(
      syncer::SyncService* sync_service);

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
  const raw_ptr<ChromeBrowserState> browser_state_;

  // The sync engine factory in use by this client.
  std::unique_ptr<browser_sync::SyncEngineFactoryImpl> engine_factory_;

  scoped_refptr<password_manager::PasswordStoreInterface>
      profile_password_store_;
  scoped_refptr<password_manager::PasswordStoreInterface>
      account_password_store_;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_IOS_CHROME_SYNC_CLIENT_H__
