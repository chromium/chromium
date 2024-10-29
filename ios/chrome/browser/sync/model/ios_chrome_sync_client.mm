// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/ios_chrome_sync_client.h"

#import <utility>

#import "base/functional/bind.h"
#import "base/logging.h"
#import "components/browser_sync/browser_sync_switches.h"
#import "components/browser_sync/sync_engine_factory_impl.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/supervised_user/core/browser/supervised_user_settings_service.h"
#import "components/sync/base/sync_util.h"
#import "components/sync/model/data_type_store_service.h"
#import "components/sync/service/trusted_vault_synthetic_field_trial.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "components/trusted_vault/trusted_vault_service.h"
#import "ios/chrome/browser/metrics/model/ios_chrome_metrics_service_accessor.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

namespace {

// A global variable is needed to detect multiprofile scenarios where more than one profile
// tries to register a synthetic field trial.
bool trusted_vault_synthetic_field_trial_registered = false;

}  // namespace

IOSChromeSyncClient::IOSChromeSyncClient(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    trusted_vault::TrustedVaultService* trusted_vault_service,
    syncer::SyncInvalidationsService* sync_invalidations_service,
    syncer::DeviceInfoSyncService* device_info_sync_service,
    syncer::DataTypeStoreService* data_type_store_service,
    supervised_user::SupervisedUserSettingsService*
        supervised_user_settings_service)
    : pref_service_(pref_service),
      identity_manager_(identity_manager),
      trusted_vault_service_(trusted_vault_service),
      sync_invalidations_service_(sync_invalidations_service),
      supervised_user_settings_service_(supervised_user_settings_service),
      engine_factory_(this,
                      device_info_sync_service->GetDeviceInfoTracker(),
                      data_type_store_service->GetSyncDataPath()) {}

IOSChromeSyncClient::~IOSChromeSyncClient() {}

PrefService* IOSChromeSyncClient::GetPrefService() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return pref_service_;
}

signin::IdentityManager* IOSChromeSyncClient::GetIdentityManager() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return identity_manager_;
}

base::FilePath IOSChromeSyncClient::GetLocalSyncBackendFolder() {
  return base::FilePath();
}

syncer::SyncInvalidationsService*
IOSChromeSyncClient::GetSyncInvalidationsService() {
  return sync_invalidations_service_;
}

trusted_vault::TrustedVaultClient*
IOSChromeSyncClient::GetTrustedVaultClient() {
  return trusted_vault_service_->GetTrustedVaultClient(
      trusted_vault::SecurityDomainId::kChromeSync);
}

scoped_refptr<syncer::ExtensionsActivity>
IOSChromeSyncClient::GetExtensionsActivity() {
  return nullptr;
}

syncer::SyncEngineFactory* IOSChromeSyncClient::GetSyncEngineFactory() {
  return &engine_factory_;
}

bool IOSChromeSyncClient::IsCustomPassphraseAllowed() {
  if (supervised_user_settings_service_) {
    return supervised_user_settings_service_->IsCustomPassphraseAllowed();
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

void IOSChromeSyncClient::RegisterTrustedVaultAutoUpgradeSyntheticFieldTrial(
    const syncer::TrustedVaultAutoUpgradeSyntheticFieldTrialGroup& group) {
  CHECK(group.is_valid());

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
