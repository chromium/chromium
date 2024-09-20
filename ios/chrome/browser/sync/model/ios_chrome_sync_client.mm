// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/ios_chrome_sync_client.h"

#import <utility>

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/logging.h"
#import "components/browser_sync/browser_sync_switches.h"
#import "components/browser_sync/sync_client_utils.h"
#import "components/browser_sync/sync_engine_factory_impl.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/supervised_user/core/browser/supervised_user_settings_service.h"
#import "components/sync/base/features.h"
#import "components/sync/base/sync_util.h"
#import "components/sync/model/data_type_store_service.h"
#import "components/sync/service/sync_engine_factory.h"
#import "components/sync/service/trusted_vault_synthetic_field_trial.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "components/trusted_vault/trusted_vault_service.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/metrics/model/ios_chrome_metrics_service_accessor.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_settings_service_factory.h"
#import "ios/chrome/browser/sync/model/data_type_store_service_factory.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_invalidations_service_factory.h"
#import "ios/chrome/browser/trusted_vault/model/ios_trusted_vault_service_factory.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

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
          ReadingListModelFactory::GetAsDualReadingListModelForProfile(
              browser_state_));
  local_data_migration_helper_ =
      std::make_unique<browser_sync::LocalDataMigrationHelper>(
          profile_password_store_.get(), account_password_store_.get(),
          ios::BookmarkModelFactory::GetForBrowserState(browser_state_),
          ReadingListModelFactory::GetAsDualReadingListModelForProfile(
              browser_state_));
}

IOSChromeSyncClient::~IOSChromeSyncClient() {}

PrefService* IOSChromeSyncClient::GetPrefService() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return browser_state_->GetPrefs();
}

signin::IdentityManager* IOSChromeSyncClient::GetIdentityManager() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return IdentityManagerFactory::GetForProfile(browser_state_);
}

base::FilePath IOSChromeSyncClient::GetLocalSyncBackendFolder() {
  return base::FilePath();
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
