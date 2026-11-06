// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ode/ios_chrome_on_device_encryption_metrics_reporter_factory.h"

#import <memory>
#import <utility>

#import "base/check_deref.h"
#import "base/feature_list.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/core/browser/ode/on_device_encryption_metrics_reporter.h"
#import "components/password_manager/core/browser/ode/on_device_encryption_state_tracker.h"
#import "components/password_manager/core/browser/ode/passkey_trusted_vault_on_device_encryption_state_tracker.h"
#import "components/password_manager/core/browser/ode/password_trusted_vault_on_device_encryption_state_tracker.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/sync/service/sync_service.h"
#import "components/trusted_vault/trusted_vault_server_constants.h"
#import "components/trusted_vault/trusted_vault_service.h"
#import "ios/chrome/browser/passwords/model/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/trusted_vault/model/ios_trusted_vault_service_factory.h"

IOSChromeOnDeviceEncryptionMetricsReporterFactory::
    IOSChromeOnDeviceEncryptionMetricsReporterFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "OnDeviceEncryptionMetricsReporter",
          ProfileSelection::kNoInstanceInIncognito,
          ServiceCreation::kCreateWithProfile,
          TestingCreation::kNoServiceForTests) {
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(IOSTrustedVaultServiceFactory::GetInstance());
}

void IOSChromeOnDeviceEncryptionMetricsReporterFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  password_manager::OnDeviceEncryptionMetricsReporter::RegisterProfilePrefs(
      registry);
}

std::unique_ptr<KeyedService>
IOSChromeOnDeviceEncryptionMetricsReporterFactory::BuildServiceInstanceFor(
    ProfileIOS& profile) const {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::
              kPasswordManagerOnDeviceEncryptionMetricsReporter)) {
    return nullptr;
  }

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  std::unique_ptr<password_manager::OnDeviceEncryptionStateTracker>
      passkey_tracker;
  if (base::FeatureList::IsEnabled(kIOSPasskeyOnDeviceEncryptionStateTracker)) {
    trusted_vault::TrustedVaultService* trusted_vault_service =
        IOSTrustedVaultServiceFactory::GetForProfile(profile);
    trusted_vault::TrustedVaultClient* passkey_trusted_vault_client =
        trusted_vault_service ? trusted_vault_service->GetTrustedVaultClient(
                                    trusted_vault::SecurityDomainId::kPasskeys)
                              : nullptr;
    passkey_tracker = std::make_unique<
        password_manager::PasskeyTrustedVaultOnDeviceEncryptionStateTracker>(
        sync_service, passkey_trusted_vault_client);
  } else {
    // Use a no-op tracker when the kill switch is disabled.
    passkey_tracker =
        std::make_unique<password_manager::OnDeviceEncryptionStateTracker>();
  }
  auto password_tracker = std::make_unique<
      password_manager::PasswordTrustedVaultOnDeviceEncryptionStateTracker>(
      sync_service);

  return std::make_unique<password_manager::OnDeviceEncryptionMetricsReporter>(
      std::move(passkey_tracker), std::move(password_tracker),
      CHECK_DEREF(profile.GetPrefs()));
}
