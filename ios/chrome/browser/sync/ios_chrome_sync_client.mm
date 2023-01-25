// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/ios_chrome_sync_client.h"

#import <utility>

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/logging.h"
#import "components/autofill/core/browser/webdata/autocomplete_sync_bridge.h"
#import "components/autofill/core/browser/webdata/autofill_profile_sync_bridge.h"
#import "components/autofill/core/browser/webdata/autofill_wallet_metadata_sync_bridge.h"
#import "components/autofill/core/browser/webdata/autofill_wallet_sync_bridge.h"
#import "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/browser_sync/browser_sync_switches.h"
#import "components/browser_sync/sync_api_component_factory_impl.h"
#import "components/consent_auditor/consent_auditor.h"
#import "components/dom_distiller/core/dom_distiller_service.h"
#import "components/history/core/browser/history_service.h"
#import "components/history/core/browser/sync/typed_url_sync_bridge.h"
#import "components/invalidation/impl/invalidation_switches.h"
#import "components/invalidation/impl/profile_invalidation_provider.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/metrics/demographics/user_demographics.h"
#import "components/password_manager/core/browser/password_store_interface.h"
#import "components/reading_list/core/reading_list_model.h"
#import "components/sync/base/report_unrecoverable_error.h"
#import "components/sync/base/sync_util.h"
#import "components/sync/driver/sync_api_component_factory.h"
#import "components/sync/driver/sync_service.h"
#import "components/sync_sessions/session_sync_service.h"
#import "components/sync_user_events/user_event_service.h"
#import "ios/chrome/browser/bookmarks/bookmark_sync_service_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/consent_auditor/consent_auditor_factory.h"
#import "ios/chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#import "ios/chrome/browser/favicon/favicon_service_factory.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/invalidation/ios_chrome_profile_invalidation_provider_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/power_bookmarks/power_bookmark_service_factory.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/signin/trusted_vault_client_backend_factory.h"
#import "ios/chrome/browser/sync/device_info_sync_service_factory.h"
#import "ios/chrome/browser/sync/ios_trusted_vault_client.h"
#import "ios/chrome/browser/sync/ios_user_event_service_factory.h"
#import "ios/chrome/browser/sync/model_type_store_service_factory.h"
#import "ios/chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/sync/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_invalidations_service_factory.h"
#import "ios/chrome/browser/webdata_services/web_data_service_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

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
  profile_password_store_ = IOSChromePasswordStoreFactory::GetForBrowserState(
      browser_state_, ServiceAccessType::IMPLICIT_ACCESS);
  account_password_store_ =
      IOSChromeAccountPasswordStoreFactory::GetForBrowserState(
          browser_state_, ServiceAccessType::IMPLICIT_ACCESS);

  component_factory_ =
      std::make_unique<browser_sync::SyncApiComponentFactoryImpl>(
          this, ::GetChannel(), web::GetUIThreadTaskRunner({}), db_thread_,
          profile_web_data_service_, account_web_data_service_,
          profile_password_store_, account_password_store_,
          ios::BookmarkSyncServiceFactory::GetForBrowserState(browser_state_),
          PowerBookmarkServiceFactory::GetForBrowserState(browser_state_));

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

ReadingListModel* IOSChromeSyncClient::GetReadingListModel() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return ReadingListModelFactory::GetForBrowserState(browser_state_);
}

send_tab_to_self::SendTabToSelfSyncService*
IOSChromeSyncClient::GetSendTabToSelfSyncService() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return SendTabToSelfSyncServiceFactory::GetForBrowserState(browser_state_);
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
    case syncer::READING_LIST:
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
