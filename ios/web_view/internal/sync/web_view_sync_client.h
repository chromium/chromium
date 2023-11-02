// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_SYNC_CLIENT_H_
#define IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_SYNC_CLIENT_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/browser_sync/browser_sync_client.h"
#include "components/browser_sync/sync_api_component_factory_impl.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "ios/web_view/internal/web_view_browser_state.h"

namespace ios_web_view {

class WebViewSyncClient : public browser_sync::BrowserSyncClient {
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
      invalidation::InvalidationService* invalidation_service,
      syncer::SyncInvalidationsService* sync_invalidations_service);

  WebViewSyncClient(const WebViewSyncClient&) = delete;
  WebViewSyncClient& operator=(const WebViewSyncClient&) = delete;

  ~WebViewSyncClient() override;

  // BrowserSyncClient implementation.
  PrefService* GetPrefService() override;
  signin::IdentityManager* GetIdentityManager() override;
  base::FilePath GetLocalSyncBackendFolder() override;
  syncer::ModelTypeStoreService* GetModelTypeStoreService() override;
  syncer::DeviceInfoSyncService* GetDeviceInfoSyncService() override;
  favicon::FaviconService* GetFaviconService() override;
  history::HistoryService* GetHistoryService() override;
  send_tab_to_self::SendTabToSelfSyncService* GetSendTabToSelfSyncService()
      override;
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
  autofill::AutofillWebDataService* profile_web_data_service_;
  autofill::AutofillWebDataService* account_web_data_service_;
  password_manager::PasswordStoreInterface* profile_password_store_;
  password_manager::PasswordStoreInterface* account_password_store_;
  PrefService* pref_service_;
  signin::IdentityManager* identity_manager_;
  syncer::ModelTypeStoreService* model_type_store_service_;
  syncer::DeviceInfoSyncService* device_info_sync_service_;
  invalidation::InvalidationService* invalidation_service_;
  syncer::SyncInvalidationsService* sync_invalidations_service_;

  std::unique_ptr<browser_sync::SyncApiComponentFactoryImpl> component_factory_;
  std::unique_ptr<syncer::TrustedVaultClient> trusted_vault_client_;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_SYNC_CLIENT_H_
