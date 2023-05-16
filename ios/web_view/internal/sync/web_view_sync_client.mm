// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/web_view_sync_client.h"

#import <algorithm>

#import "base/check_op.h"
#import "base/command_line.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/notreached.h"
#import "components/autofill/core/browser/webdata/autofill_profile_sync_bridge.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/invalidation/impl/profile_invalidation_provider.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/metrics/demographics/user_demographics.h"
#import "components/sync/base/sync_util.h"
#import "components/sync/service/data_type_controller.h"
#import "components/sync/service/sync_api_component_factory.h"
#import "components/version_info/version_info.h"
#import "components/version_info/version_string.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web_view/internal/passwords/web_view_account_password_store_factory.h"
#import "ios/web_view/internal/passwords/web_view_password_store_factory.h"
#import "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/sync/web_view_device_info_sync_service_factory.h"
#import "ios/web_view/internal/sync/web_view_model_type_store_service_factory.h"
#import "ios/web_view/internal/sync/web_view_profile_invalidation_provider_factory.h"
#import "ios/web_view/internal/sync/web_view_sync_invalidations_service_factory.h"
#import "ios/web_view/internal/sync/web_view_trusted_vault_client.h"
#import "ios/web_view/internal/webdata_services/web_view_web_data_service_wrapper_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

namespace {
syncer::ModelTypeSet GetDisabledTypes() {
  syncer::ModelTypeSet disabled_types = syncer::UserTypes();
  disabled_types.Remove(syncer::AUTOFILL);
  disabled_types.Remove(syncer::AUTOFILL_WALLET_DATA);
  disabled_types.Remove(syncer::AUTOFILL_WALLET_METADATA);
  disabled_types.Remove(syncer::AUTOFILL_PROFILE);
  disabled_types.Remove(syncer::PASSWORDS);
  return disabled_types;
}
}  // namespace

// static
std::unique_ptr<WebViewSyncClient> WebViewSyncClient::Create(
    WebViewBrowserState* browser_state) {
  return std::make_unique<WebViewSyncClient>(
      WebViewWebDataServiceWrapperFactory::GetAutofillWebDataForBrowserState(
          browser_state, ServiceAccessType::IMPLICIT_ACCESS)
          .get(),
      WebViewWebDataServiceWrapperFactory::GetAutofillWebDataForAccount(
          browser_state, ServiceAccessType::IMPLICIT_ACCESS)
          .get(),
      WebViewPasswordStoreFactory::GetForBrowserState(
          browser_state, ServiceAccessType::IMPLICIT_ACCESS)
          .get(),
      WebViewAccountPasswordStoreFactory::GetForBrowserState(
          browser_state, ServiceAccessType::IMPLICIT_ACCESS)
          .get(),
      browser_state->GetPrefs(),
      WebViewIdentityManagerFactory::GetForBrowserState(browser_state),
      WebViewModelTypeStoreServiceFactory::GetForBrowserState(browser_state),
      WebViewDeviceInfoSyncServiceFactory::GetForBrowserState(browser_state),
      WebViewProfileInvalidationProviderFactory::GetForBrowserState(
          browser_state)
          ->GetInvalidationService(),
      WebViewSyncInvalidationsServiceFactory::GetForBrowserState(
          browser_state));
}

WebViewSyncClient::WebViewSyncClient(
    autofill::AutofillWebDataService* profile_web_data_service,
    autofill::AutofillWebDataService* account_web_data_service,
    password_manager::PasswordStoreInterface* profile_password_store,
    password_manager::PasswordStoreInterface* account_password_store,
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    syncer::ModelTypeStoreService* model_type_store_service,
    syncer::DeviceInfoSyncService* device_info_sync_service,
    invalidation::InvalidationService* invalidation_service,
    syncer::SyncInvalidationsService* sync_invalidations_service)
    : profile_web_data_service_(profile_web_data_service),
      account_web_data_service_(account_web_data_service),
      profile_password_store_(profile_password_store),
      account_password_store_(account_password_store),
      pref_service_(pref_service),
      identity_manager_(identity_manager),
      model_type_store_service_(model_type_store_service),
      device_info_sync_service_(device_info_sync_service),
      invalidation_service_(invalidation_service),
      sync_invalidations_service_(sync_invalidations_service) {
  component_factory_ =
      std::make_unique<browser_sync::SyncApiComponentFactoryImpl>(
          this, version_info::Channel::STABLE, web::GetUIThreadTaskRunner({}),
          profile_web_data_service_->GetDBTaskRunner(),
          profile_web_data_service_, account_web_data_service_,
          profile_password_store_, account_password_store_,
          /*local_or_syncable_bookmark_sync_service=*/nullptr,
          /*account_bookmark_sync_service=*/nullptr,
          /*power_bookmark_service=*/nullptr);
  trusted_vault_client_ = std::make_unique<WebViewTrustedVaultClient>();
}

WebViewSyncClient::~WebViewSyncClient() {}

PrefService* WebViewSyncClient::GetPrefService() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return pref_service_;
}

signin::IdentityManager* WebViewSyncClient::GetIdentityManager() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return identity_manager_;
}

base::FilePath WebViewSyncClient::GetLocalSyncBackendFolder() {
  return base::FilePath();
}

syncer::ModelTypeStoreService* WebViewSyncClient::GetModelTypeStoreService() {
  return model_type_store_service_;
}

syncer::DeviceInfoSyncService* WebViewSyncClient::GetDeviceInfoSyncService() {
  return device_info_sync_service_;
}

favicon::FaviconService* WebViewSyncClient::GetFaviconService() {
  return nullptr;
}

history::HistoryService* WebViewSyncClient::GetHistoryService() {
  return nullptr;
}

ReadingListModel* WebViewSyncClient::GetReadingListModel() {
  return nullptr;
}

sync_preferences::PrefServiceSyncable*
WebViewSyncClient::GetPrefServiceSyncable() {
  return nullptr;
}

sync_sessions::SessionSyncService* WebViewSyncClient::GetSessionSyncService() {
  return nullptr;
}

send_tab_to_self::SendTabToSelfSyncService*
WebViewSyncClient::GetSendTabToSelfSyncService() {
  return nullptr;
}

syncer::DataTypeController::TypeVector
WebViewSyncClient::CreateDataTypeControllers(
    syncer::SyncService* sync_service) {
  return component_factory_->CreateCommonDataTypeControllers(GetDisabledTypes(),
                                                             sync_service);
}

invalidation::InvalidationService* WebViewSyncClient::GetInvalidationService() {
  return invalidation_service_;
}

syncer::SyncInvalidationsService*
WebViewSyncClient::GetSyncInvalidationsService() {
  return sync_invalidations_service_;
}

syncer::TrustedVaultClient* WebViewSyncClient::GetTrustedVaultClient() {
  return trusted_vault_client_.get();
}

scoped_refptr<syncer::ExtensionsActivity>
WebViewSyncClient::GetExtensionsActivity() {
  return nullptr;
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
WebViewSyncClient::GetControllerDelegateForModelType(syncer::ModelType type) {
  NOTREACHED();
  return base::WeakPtr<syncer::ModelTypeControllerDelegate>();
}

syncer::SyncApiComponentFactory*
WebViewSyncClient::GetSyncApiComponentFactory() {
  return component_factory_.get();
}

syncer::SyncTypePreferenceProvider* WebViewSyncClient::GetPreferenceProvider() {
  return nullptr;
}

void WebViewSyncClient::OnLocalSyncTransportDataCleared() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  metrics::ClearDemographicsPrefs(pref_service_);
}

}  // namespace ios_web_view
