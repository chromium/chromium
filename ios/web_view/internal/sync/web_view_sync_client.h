// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_SYNC_CLIENT_H_
#define IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_SYNC_CLIENT_H_

#include <memory>

#include "components/browser_sync/sync_engine_factory_impl.h"
#include "components/sync/service/sync_client.h"

namespace syncer {
class DeviceInfoSyncService;
class DataTypeStoreService;
}  // namespace syncer

namespace ios_web_view {

class WebViewSyncClient : public syncer::SyncClient {
 public:
  explicit WebViewSyncClient(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      syncer::DataTypeStoreService* data_type_store_service,
      syncer::DeviceInfoSyncService* device_info_sync_service,
      syncer::SyncInvalidationsService* sync_invalidations_service);

  WebViewSyncClient(const WebViewSyncClient&) = delete;
  WebViewSyncClient& operator=(const WebViewSyncClient&) = delete;

  ~WebViewSyncClient() override;

  // SyncClient implementation.
  PrefService* GetPrefService() override;
  signin::IdentityManager* GetIdentityManager() override;
  base::FilePath GetLocalSyncBackendFolder() override;
  syncer::SyncInvalidationsService* GetSyncInvalidationsService() override;
  trusted_vault::TrustedVaultClient* GetTrustedVaultClient() override;
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
  const raw_ptr<syncer::SyncInvalidationsService> sync_invalidations_service_;

  std::unique_ptr<browser_sync::SyncEngineFactoryImpl> engine_factory_;
  std::unique_ptr<trusted_vault::TrustedVaultClient> trusted_vault_client_;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_SYNC_CLIENT_H_
