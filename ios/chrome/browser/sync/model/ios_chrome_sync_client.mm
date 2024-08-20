// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/ios_chrome_sync_client.h"

#import <utility>

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/logging.h"
#import "components/autofill/core/browser/webdata/addresses/autofill_profile_sync_bridge.h"
#import "components/autofill/core/browser/webdata/autocomplete/autocomplete_sync_bridge.h"
#import "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#import "components/autofill/core/browser/webdata/payments/autofill_wallet_metadata_sync_bridge.h"
#import "components/autofill/core/browser/webdata/payments/autofill_wallet_sync_bridge.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/browser_sync/browser_sync_switches.h"
#import "components/browser_sync/common_controller_builder.h"
#import "components/browser_sync/sync_client_utils.h"
#import "components/browser_sync/sync_engine_factory_impl.h"
#import "components/consent_auditor/consent_auditor.h"
#import "components/dom_distiller/core/dom_distiller_service.h"
#import "components/history/core/browser/history_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/password_manager/core/browser/sharing/password_receiver_service.h"
#import "components/password_manager/core/browser/sharing/password_sender_service.h"
#import "components/plus_addresses/settings/plus_address_setting_service.h"
#import "components/plus_addresses/webdata/plus_address_webdata_service.h"
#import "components/reading_list/core/dual_reading_list_model.h"
#import "components/reading_list/core/reading_list_model.h"
#import "components/saved_tab_groups/tab_group_sync_service.h"
#import "components/send_tab_to_self/features.h"
#import "components/sharing_message/sharing_message_bridge.h"
#import "components/sharing_message/sharing_message_model_type_controller.h"
#import "components/supervised_user/core/browser/supervised_user_settings_service.h"
#import "components/sync/base/features.h"
#import "components/sync/base/report_unrecoverable_error.h"
#import "components/sync/base/sync_util.h"
#import "components/sync/model/data_type_store_service.h"
#import "components/sync/model/forwarding_data_type_controller_delegate.h"
#import "components/sync/service/sync_engine_factory.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/trusted_vault_synthetic_field_trial.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "components/sync_sessions/session_sync_service.h"
#import "components/sync_user_events/user_event_service.h"
#import "components/trusted_vault/trusted_vault_service.h"
#import "components/webauthn/core/browser/passkey_model.h"
#import "ios/chrome/browser/bookmarks/model/account_bookmark_sync_service_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_sync_service_factory.h"
#import "ios/chrome/browser/consent_auditor/model/consent_auditor_factory.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"
#import "ios/chrome/browser/dom_distiller/model/dom_distiller_service_factory.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/metrics/model/google_groups_manager_factory.h"
#import "ios/chrome/browser/metrics/model/ios_chrome_metrics_service_accessor.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_receiver_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_sender_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/plus_addresses/model/plus_address_setting_service_factory.h"
#import "ios/chrome/browser/power_bookmarks/model/power_bookmark_service_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/sharing_message/model/ios_sharing_message_bridge_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_settings_service_factory.h"
#import "ios/chrome/browser/sync/model/data_type_store_service_factory.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/ios_user_event_service_factory.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_invalidations_service_factory.h"
#import "ios/chrome/browser/trusted_vault/model/ios_trusted_vault_service_factory.h"
#import "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// A global variable is needed to detect "multiprofile" (multi-BrowserState)
// scenarios where more than one profile try to register a synthetic field
// trial.
bool trusted_vault_synthetic_field_trial_registered = false;

}  // namespace

IOSChromeSyncClient::IOSChromeSyncClient(ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {
  profile_password_store_ =
      IOSChromeProfilePasswordStoreFactory::GetForBrowserState(
          browser_state_, ServiceAccessType::IMPLICIT_ACCESS);
  account_password_store_ =
      IOSChromeAccountPasswordStoreFactory::GetForBrowserState(
          browser_state_, ServiceAccessType::IMPLICIT_ACCESS);

  engine_factory_ = std::make_unique<browser_sync::SyncEngineFactoryImpl>(
      this,
      DeviceInfoSyncServiceFactory::GetForBrowserState(browser_state_)
          ->GetDeviceInfoTracker(),
      DataTypeStoreServiceFactory::GetForBrowserState(browser_state_)
          ->GetSyncDataPath());

  local_data_query_helper_ =
      std::make_unique<browser_sync::LocalDataQueryHelper>(
          profile_password_store_.get(), account_password_store_.get(),
          ios::BookmarkModelFactory::GetForBrowserState(browser_state_),
          ReadingListModelFactory::GetAsDualReadingListModelForBrowserState(
              browser_state_));
  local_data_migration_helper_ =
      std::make_unique<browser_sync::LocalDataMigrationHelper>(
          profile_password_store_.get(), account_password_store_.get(),
          ios::BookmarkModelFactory::GetForBrowserState(browser_state_),
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

syncer::DataTypeController::TypeVector
IOSChromeSyncClient::CreateDataTypeControllers(
    syncer::SyncService* sync_service) {
  scoped_refptr<autofill::AutofillWebDataService> profile_web_data_service =
      ios::WebDataServiceFactory::GetAutofillWebDataForBrowserState(
          browser_state_, ServiceAccessType::IMPLICIT_ACCESS);

  browser_sync::CommonControllerBuilder builder;
  builder.SetAutofillWebDataService(
      web::GetUIThreadTaskRunner({}),
      profile_web_data_service ? profile_web_data_service->GetDBTaskRunner()
                               : nullptr,
      profile_web_data_service,
      ios::WebDataServiceFactory::GetAutofillWebDataForAccount(
          browser_state_, ServiceAccessType::IMPLICIT_ACCESS));
  builder.SetBookmarkModel(
      ios::BookmarkModelFactory::GetForBrowserState(browser_state_));
  builder.SetBookmarkSyncService(
      ios::LocalOrSyncableBookmarkSyncServiceFactory::GetForBrowserState(
          browser_state_),
      ios::AccountBookmarkSyncServiceFactory::GetForBrowserState(
          browser_state_));
  builder.SetConsentAuditor(
      ConsentAuditorFactory::GetForBrowserState(browser_state_));
  builder.SetDataSharingService(
      data_sharing::DataSharingServiceFactory::GetForBrowserState(
          browser_state_));
  builder.SetDeviceInfoSyncService(
      DeviceInfoSyncServiceFactory::GetForBrowserState(browser_state_));
  builder.SetDualReadingListModel(
      ReadingListModelFactory::GetAsDualReadingListModelForBrowserState(
          browser_state_));
  builder.SetFaviconService(ios::FaviconServiceFactory::GetForBrowserState(
      browser_state_, ServiceAccessType::IMPLICIT_ACCESS));
  builder.SetGoogleGroupsManager(
      GoogleGroupsManagerFactory::GetForBrowserState(browser_state_));
  builder.SetHistoryService(ios::HistoryServiceFactory::GetForBrowserState(
      browser_state_, ServiceAccessType::EXPLICIT_ACCESS));
  builder.SetIdentityManager(GetIdentityManager());
  builder.SetDataTypeStoreService(
      DataTypeStoreServiceFactory::GetForBrowserState(browser_state_));
#if !BUILDFLAG(IS_ANDROID)
  builder.SetPasskeyModel(
      base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials)
          ? IOSPasskeyModelFactory::GetForBrowserState(browser_state_)
          : nullptr);
#endif  // !BUILDFLAG(IS_ANDROID)
  builder.SetPasswordReceiverService(
      IOSChromePasswordReceiverServiceFactory::GetForBrowserState(
          browser_state_));
  builder.SetPasswordSenderService(
      IOSChromePasswordSenderServiceFactory::GetForBrowserState(
          browser_state_));
  builder.SetPasswordStore(profile_password_store_, account_password_store_);
  builder.SetPlusAddressServices(
      PlusAddressSettingServiceFactory::GetForBrowserState(browser_state_),
      ios::WebDataServiceFactory::GetPlusAddressWebDataForBrowserState(
          browser_state_, ServiceAccessType::IMPLICIT_ACCESS));
  builder.SetPowerBookmarkService(
      PowerBookmarkServiceFactory::GetForBrowserState(browser_state_));
  builder.SetPrefService(browser_state_->GetPrefs());
  builder.SetPrefServiceSyncable(browser_state_->GetSyncablePrefs());
  // TODO(crbug.com/330201909) implement for iOS.
  builder.SetProductSpecificationsService(nullptr);
  builder.SetSendTabToSelfSyncService(
      SendTabToSelfSyncServiceFactory::GetForBrowserState(browser_state_));
  builder.SetSessionSyncService(
      SessionSyncServiceFactory::GetForBrowserState(browser_state_));
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  builder.SetSupervisedUserSettingsService(
      SupervisedUserSettingsServiceFactory::GetForBrowserState(browser_state_));
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
  builder.SetUserEventService(
      IOSUserEventServiceFactory::GetForBrowserState(browser_state_));

  syncer::DataTypeController::TypeVector controllers = builder.Build(
      /*disabled_types=*/{}, sync_service, ::GetChannel());

  if (IsTabGroupSyncEnabled()) {
    syncer::DataTypeControllerDelegate* delegate =
        tab_groups::TabGroupSyncServiceFactory::GetForBrowserState(
            browser_state_)
            ->GetSavedTabGroupControllerDelegate()
            .get();
    // TODO(crbug.com/344893270): Move this controller to
    // CreateCommonDataTypeControllers().
    controllers.push_back(std::make_unique<syncer::DataTypeController>(
        syncer::SAVED_TAB_GROUP, /*delegate_for_full_sync_mode=*/
        std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
            delegate),
        /*delegate_for_transport_mode=*/
        std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
            delegate)));
  }

  if (base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfIOSPushNotifications)) {
    syncer::DataTypeControllerDelegate* sharing_message_delegate =
        IOSSharingMessageBridgeFactory::GetForBrowserState(browser_state_)
            ->GetControllerDelegate()
            .get();
    controllers.push_back(std::make_unique<SharingMessageModelTypeController>(
        /*delegate_for_full_sync_mode=*/
        std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
            sharing_message_delegate),
        /*delegate_for_transport_mode=*/
        std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
            sharing_message_delegate)));
  }

  return controllers;
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

syncer::SyncEngineFactory* IOSChromeSyncClient::GetSyncEngineFactory() {
  return engine_factory_.get();
}

bool IOSChromeSyncClient::IsCustomPassphraseAllowed() {
  supervised_user::SupervisedUserSettingsService*
      supervised_user_settings_service =
          SupervisedUserSettingsServiceFactory::GetForBrowserState(
              browser_state_);
  if (supervised_user_settings_service) {
    return supervised_user_settings_service->IsCustomPassphraseAllowed();
  }
  return true;
}

bool IOSChromeSyncClient::IsPasswordSyncAllowed() {
  return true;
}

void IOSChromeSyncClient::SetPasswordSyncAllowedChangeCb(
    const base::RepeatingClosure& cb) {
  // IsPasswordSyncAllowed() doesn't change on //ios/chrome.
}

void IOSChromeSyncClient::GetLocalDataDescriptions(
    syncer::DataTypeSet types,
    base::OnceCallback<void(
        std::map<syncer::DataType, syncer::LocalDataDescription>)> callback) {
  types.RemoveAll(
      local_data_migration_helper_->GetTypesWithOngoingMigrations());
  local_data_query_helper_->Run(types, std::move(callback));
}

void IOSChromeSyncClient::TriggerLocalDataMigration(syncer::DataTypeSet types) {
  local_data_migration_helper_->Run(types);
}

void IOSChromeSyncClient::RegisterTrustedVaultAutoUpgradeSyntheticFieldTrial(
    const syncer::TrustedVaultAutoUpgradeSyntheticFieldTrialGroup& group) {
  CHECK(group.is_valid());

  if (!base::FeatureList::IsEnabled(
          syncer::kTrustedVaultAutoUpgradeSyntheticFieldTrial)) {
    // Disabled via variations, as additional safeguard.
    return;
  }

  // If `trusted_vault_synthetic_field_trial_registered` is true, and given that
  // each SyncService invokes this function at most once, it means that multiple
  // BrowserState instances are trying to register a synthetic field trial. In
  // that case, register a special "conflict" group.
  const std::string group_name =
      trusted_vault_synthetic_field_trial_registered
          ? syncer::TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::
                GetMultiProfileConflictGroupName()
          : group.name();

  trusted_vault_synthetic_field_trial_registered = true;

  IOSChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      syncer::kTrustedVaultAutoUpgradeSyntheticFieldTrialName, group_name,
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}
