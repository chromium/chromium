// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_SYNC_CLIENT_H_
#define IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_SYNC_CLIENT_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/browser_sync/common_controller_builder.h"
#include "components/browser_sync/sync_api_component_factory_impl.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/sync/service/sync_client.h"
#include "ios/web_view/internal/web_view_browser_state.h"

namespace syncer {
class ModelTypeStoreService;
}  // namespace syncer

namespace ios_web_view {

class WebViewSyncClient : public syncer::SyncClient {
 public:
  static std::unique_ptr<WebViewSyncClient> Create(
      WebViewBrowserState* browser_state);

  explicit WebViewSyncClient(
      autofill::AutofillWebDataService* profile_web_data_service,
      autofill::AutofillWebDataService* account_web_data_service,
      password_manager::PasswordStoreInterface* profile_password_store,
      password_manager::PasswordStoreInterface* account_password_store,
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      syncer::ModelTypeStoreService* model_type_store_service,
      syncer::DeviceInfoSyncService* device_info_sync_service,
      syncer::SyncInvalidationsService* sync_invalidations_service);

  WebViewSyncClient(const WebViewSyncClient&) = delete;
  WebViewSyncClient& operator=(const WebViewSyncClient&) = delete;

  ~WebViewSyncClient() override;

  // SyncClient implementation.
  PrefService* GetPrefService() override;
  signin::IdentityManager* GetIdentityManager() override;
  base::FilePath GetLocalSyncBackendFolder() override;
  syncer::ModelTypeController::TypeVector CreateModelTypeControllers(
      syncer::SyncService* sync_service) override;
  syncer::SyncInvalidationsService* GetSyncInvalidationsService() override;
  trusted_vault::TrustedVaultClient* GetTrustedVaultClient() override;
  scoped_refptr<syncer::ExtensionsActivity> GetExtensionsActivity() override;
  syncer::SyncApiComponentFactory* GetSyncApiComponentFactory() override;
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

  std::unique_ptr<browser_sync::SyncApiComponentFactoryImpl> component_factory_;
  std::unique_ptr<trusted_vault::TrustedVaultClient> trusted_vault_client_;
  browser_sync::CommonControllerBuilder controller_builder_;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_SYNC_CLIENT_H_
