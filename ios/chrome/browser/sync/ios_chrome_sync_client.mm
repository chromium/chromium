// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/ios_chrome_sync_client.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "components/autofill/core/browser/webdata/autocomplete_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_profile_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_metadata_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/browser_sync/sync_api_component_factory_impl.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/sync/typed_url_sync_bridge.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/metrics/demographics/user_demographics.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/base/sync_util.h"
#include "components/sync/driver/sync_api_component_factory.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_user_events/user_event_service.h"
#include "ios/chrome/browser/bookmarks/bookmark_sync_service_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "ios/chrome/browser/favicon/favicon_service_factory.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/invalidation/ios_chrome_profile_invalidation_provider_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/signin/trusted_vault_client_backend_factory.h"
#include "ios/chrome/browser/sync/consent_auditor_factory.h"
#include "ios/chrome/browser/sync/device_info_sync_service_factory.h"
#include "ios/chrome/browser/sync/ios_trusted_vault_client.h"
#include "ios/chrome/browser/sync/ios_user_event_service_factory.h"
#include "ios/chrome/browser/sync/model_type_store_service_factory.h"
#include "ios/chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "ios/chrome/browser/sync/session_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_invalidations_service_factory.h"
#include "ios/chrome/browser/webdata_services/web_data_service_factory.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSChromeSyncClient::IOSChromeSyncClient(ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {
  profile_web_data_service_ =
      ios::WebDataServiceFactory::GetAutofillWebDataForBrowserState(
          browser_state_, ServiceAccessType::IMPLICIT_ACCESS);
  account_web_data_service_ =
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableAccountWalletStorage)
          ? ios::WebDataServiceFactory::GetAutofillWebDataForAccount(
                browser_state_, ServiceAccessType::IMPLICIT_ACCESS)
          : nullptr;
  db_thread_ = profile_web_data_service_
                   ? profile_web_data_service_->GetDBTaskRunner()
                   : nullptr;
  password_store_ = IOSChromePasswordStoreFactory::GetForBrowserState(
      browser_state_, ServiceAccessType::IMPLICIT_ACCESS);

  component_factory_ =
      std::make_unique<browser_sync::SyncApiComponentFactoryImpl>(
          this, ::GetChannel(), web::GetUIThreadTaskRunner({}), db_thread_,
          profile_web_data_service_, account_web_data_service_, password_store_,
          /*account_password_store=*/nullptr,
          ios::BookmarkSyncServiceFactory::GetForBrowserState(browser_state_));

  trusted_vault_client_ = std::make_unique<IOSTrustedVaultClient>(
      ChromeAccountManagerServiceFactory::GetForBrowserState(browser_state_),
      TrustedVaultClientBackendFactory::GetForBrowserState(browser_state_));
}

IOSChromeSyncClient::~IOSChromeSyncClient() {}

PrefService* IOSChromeSyncClient::GetPrefService() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return browser_state_->GetPrefs();
}

signin::IdentityManager* IOSChromeSyncClient::GetIdentityManager() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return IdentityManagerFactory::GetForBrowserState(browser_state_);
}

base::FilePath IOSChromeSyncClient::GetLocalSyncBackendFolder() {
  return base::FilePath();
}

syncer::ModelTypeStoreService* IOSChromeSyncClient::GetModelTypeStoreService() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return ModelTypeStoreServiceFactory::GetForBrowserState(browser_state_);
}

syncer::DeviceInfoSyncService* IOSChromeSyncClient::GetDeviceInfoSyncService() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return DeviceInfoSyncServiceFactory::GetForBrowserState(browser_state_);
}

send_tab_to_self::SendTabToSelfSyncService*
IOSChromeSyncClient::GetSendTabToSelfSyncService() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return SendTabToSelfSyncServiceFactory::GetForBrowserState(browser_state_);
}

favicon::FaviconService* IOSChromeSyncClient::GetFaviconService() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return ios::FaviconServiceFactory::GetForBrowserState(
      browser_state_, ServiceAccessType::IMPLICIT_ACCESS);
}

history::HistoryService* IOSChromeSyncClient::GetHistoryService() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return ios::HistoryServiceFactory::GetForBrowserState(
      browser_state_, ServiceAccessType::EXPLICIT_ACCESS);
}

sync_preferences::PrefServiceSyncable*
IOSChromeSyncClient::GetPrefServiceSyncable() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return browser_state_->GetSyncablePrefs();
}

sync_sessions::SessionSyncService*
IOSChromeSyncClient::GetSessionSyncService() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return SessionSyncServiceFactory::GetForBrowserState(browser_state_);
}

syncer::DataTypeController::TypeVector
IOSChromeSyncClient::CreateDataTypeControllers(
    syncer::SyncService* sync_service) {
  // The iOS port does not have any platform-specific datatypes.
  return component_factory_->CreateCommonDataTypeControllers(
      /*disabled_types=*/{}, sync_service);
}

invalidation::InvalidationService*
IOSChromeSyncClient::GetInvalidationService() {
  invalidation::ProfileInvalidationProvider* provider =
      IOSChromeProfileInvalidationProviderFactory::GetForBrowserState(
          browser_state_);
  if (provider)
    return provider->GetInvalidationService();
  return nullptr;
}

syncer::SyncInvalidationsService*
IOSChromeSyncClient::GetSyncInvalidationsService() {
  return SyncInvalidationsServiceFactory::GetForBrowserState(browser_state_);
}

syncer::TrustedVaultClient* IOSChromeSyncClient::GetTrustedVaultClient() {
  return trusted_vault_client_.get();
}

scoped_refptr<syncer::ExtensionsActivity>
IOSChromeSyncClient::GetExtensionsActivity() {
  return nullptr;
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
IOSChromeSyncClient::GetControllerDelegateForModelType(syncer::ModelType type) {
  switch (type) {
    case syncer::READING_LIST: {
      ReadingListModel* reading_list_model =
          ReadingListModelFactory::GetForBrowserState(browser_state_);
      return reading_list_model->GetModelTypeSyncBridge()
          ->change_processor()
          ->GetControllerDelegate();
    }
    case syncer::USER_CONSENTS:
      return ConsentAuditorFactory::GetForBrowserState(browser_state_)
          ->GetControllerDelegate();
    case syncer::USER_EVENTS:
      return IOSUserEventServiceFactory::GetForBrowserState(browser_state_)
          ->GetControllerDelegate();

    // We don't exercise this function for certain datatypes, because their
    // controllers get the delegate elsewhere.
    case syncer::AUTOFILL:
    case syncer::AUTOFILL_PROFILE:
    case syncer::AUTOFILL_WALLET_DATA:
    case syncer::AUTOFILL_WALLET_METADATA:
    case syncer::BOOKMARKS:
    case syncer::DEVICE_INFO:
    case syncer::SESSIONS:
    case syncer::TYPED_URLS:
      NOTREACHED();
      return base::WeakPtr<syncer::ModelTypeControllerDelegate>();

    default:
      NOTREACHED();
      return base::WeakPtr<syncer::ModelTypeControllerDelegate>();
  }
}

syncer::SyncApiComponentFactory*
IOSChromeSyncClient::GetSyncApiComponentFactory() {
  return component_factory_.get();
}

syncer::SyncTypePreferenceProvider*
IOSChromeSyncClient::GetPreferenceProvider() {
  return nullptr;
}

void IOSChromeSyncClient::OnLocalSyncTransportDataCleared() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  metrics::ClearDemographicsPrefs(browser_state_->GetPrefs());
}
