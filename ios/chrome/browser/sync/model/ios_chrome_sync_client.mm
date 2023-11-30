// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/ios_chrome_sync_client.h"

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
#import "components/browser_sync/sync_client_utils.h"
#import "components/consent_auditor/consent_auditor.h"
#import "components/dom_distiller/core/dom_distiller_service.h"
#import "components/history/core/browser/history_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/metrics/demographics/user_demographics.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/password_manager/core/browser/sharing/password_receiver_service.h"
#import "components/password_manager/core/browser/sharing/password_sender_service.h"
#import "components/reading_list/core/dual_reading_list_model.h"
#import "components/reading_list/core/reading_list_model.h"
#import "components/supervised_user/core/common/buildflags.h"
#import "components/sync/base/features.h"
#import "components/sync/base/report_unrecoverable_error.h"
#import "components/sync/base/sync_util.h"
#import "components/sync/service/sync_api_component_factory.h"
#import "components/sync/service/sync_service.h"
#import "components/sync_sessions/session_sync_service.h"
#import "components/sync_user_events/user_event_service.h"
#import "components/trusted_vault/trusted_vault_service.h"
#import "components/variations/service/google_groups_updater_service.h"
#import "ios/chrome/browser/bookmarks/model/account_bookmark_sync_service_factory.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_sync_service_factory.h"
#import "ios/chrome/browser/consent_auditor/model/consent_auditor_factory.h"
#import "ios/chrome/browser/dom_distiller/model/dom_distiller_service_factory.h"
#import "ios/chrome/browser/favicon/favicon_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/metrics/model/google_groups_updater_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_receiver_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_sender_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/power_bookmarks/model/power_bookmark_service_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/ios_user_event_service_factory.h"
#import "ios/chrome/browser/sync/model/model_type_store_service_factory.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_invalidations_service_factory.h"
#import "ios/chrome/browser/trusted_vault/model/ios_trusted_vault_service_factory.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#import "components/supervised_user/core/browser/supervised_user_settings_service.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_settings_service_factory.h"
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

IOSChromeSyncClient::IOSChromeSyncClient(ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {
  profile_web_data_service_ =
      ios::WebDataServiceFactory::GetAutofillWebDataForBrowserState(
          browser_state_, ServiceAccessType::IMPLICIT_ACCESS);
  account_web_data_service_ =
      ios::WebDataServiceFactory::GetAutofillWebDataForAccount(
          browser_state_, ServiceAccessType::IMPLICIT_ACCESS);
  ;
  db_thread_ = profile_web_data_service_
                   ? profile_web_data_service_->GetDBTaskRunner()
                   : nullptr;
  profile_password_store_ =
      IOSChromeProfilePasswordStoreFactory::GetForBrowserState(
          browser_state_, ServiceAccessType::IMPLICIT_ACCESS);
  account_password_store_ =
      IOSChromeAccountPasswordStoreFactory::GetForBrowserState(
          browser_state_, ServiceAccessType::IMPLICIT_ACCESS);

  supervised_user::SupervisedUserSettingsService*
      supervised_user_settings_service = nullptr;
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  supervised_user_settings_service =
      SupervisedUserSettingsServiceFactory::GetForBrowserState(browser_state);
#endif

  sync_bookmarks::BookmarkSyncService* local_or_syncable_bookmark_sync_service =
      ios::LocalOrSyncableBookmarkSyncServiceFactory::GetForBrowserState(
          browser_state_);
  sync_bookmarks::BookmarkSyncService* account_bookmark_sync_service =
      ios::AccountBookmarkSyncServiceFactory::GetForBrowserState(
          browser_state_);

  component_factory_ =
      std::make_unique<browser_sync::SyncApiComponentFactoryImpl>(
          this, ::GetChannel(), web::GetUIThreadTaskRunner({}), db_thread_,
          profile_web_data_service_, account_web_data_service_,
          profile_password_store_, account_password_store_,
          local_or_syncable_bookmark_sync_service,
          account_bookmark_sync_service,
          PowerBookmarkServiceFactory::GetForBrowserState(browser_state_),
          supervised_user_settings_service);

  local_data_query_helper_ =
      std::make_unique<browser_sync::LocalDataQueryHelper>(
          profile_password_store_.get(), account_password_store_.get(),
          local_or_syncable_bookmark_sync_service,
          account_bookmark_sync_service,
          ReadingListModelFactory::GetAsDualReadingListModelForBrowserState(
              browser_state_));
  local_data_migration_helper_ =
      std::make_unique<browser_sync::LocalDataMigrationHelper>(
          profile_password_store_.get(), account_password_store_.get(),
          local_or_syncable_bookmark_sync_service,
          account_bookmark_sync_service,
          ReadingListModelFactory::GetAsDualReadingListModelForBrowserState(
              browser_state_));
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

password_manager::PasswordReceiverService*
IOSChromeSyncClient::GetPasswordReceiverService() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return IOSChromePasswordReceiverServiceFactory::GetForBrowserState(
      browser_state_);
}

password_manager::PasswordSenderService*
IOSChromeSyncClient::GetPasswordSenderService() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return IOSChromePasswordSenderServiceFactory::GetForBrowserState(
      browser_state_);
}

syncer::DataTypeController::TypeVector
IOSChromeSyncClient::CreateDataTypeControllers(
    syncer::SyncService* sync_service) {
  // The iOS port does not have any platform-specific datatypes.
  return component_factory_->CreateCommonDataTypeControllers(
      /*disabled_types=*/{}, sync_service);
}

syncer::SyncInvalidationsService*
IOSChromeSyncClient::GetSyncInvalidationsService() {
  return SyncInvalidationsServiceFactory::GetForBrowserState(browser_state_);
}

trusted_vault::TrustedVaultClient*
IOSChromeSyncClient::GetTrustedVaultClient() {
  return IOSTrustedVaultServiceFactory::GetForBrowserState(browser_state_)
      ->GetTrustedVaultClient(trusted_vault::SecurityDomainId::kChromeSync);
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

bool IOSChromeSyncClient::IsCustomPassphraseAllowed() {
  // TODO(crbug.com/1502574): Reconsider if this should integrate with
  // SupervisedUserSettingsServiceFactory, along with corresponding
  // logic in the UI.
  return true;
}

void IOSChromeSyncClient::OnLocalSyncTransportDataCleared() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  metrics::ClearDemographicsPrefs(browser_state_->GetPrefs());

  GoogleGroupsUpdaterService* google_groups_updater =
      GoogleGroupsUpdaterServiceFactory::GetForBrowserState(browser_state_);
  if (google_groups_updater != nullptr) {
    google_groups_updater->ClearSigninScopedState();
  }
}

void IOSChromeSyncClient::GetLocalDataDescriptions(
    syncer::ModelTypeSet types,
    base::OnceCallback<void(
        std::map<syncer::ModelType, syncer::LocalDataDescription>)> callback) {
  local_data_query_helper_->Run(types, std::move(callback));
}

void IOSChromeSyncClient::TriggerLocalDataMigration(
    syncer::ModelTypeSet types) {
  local_data_migration_helper_->Run(types);
}
