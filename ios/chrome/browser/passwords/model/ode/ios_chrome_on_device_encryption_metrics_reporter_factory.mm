// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ode/ios_chrome_on_device_encryption_metrics_reporter_factory.h"

#import <memory>
#import <utility>

#import "base/check_deref.h"
#import "base/feature_list.h"
#import "base/no_destructor.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/core/browser/ode/on_device_encryption_metrics_reporter.h"
#import "components/password_manager/core/browser/ode/password_trusted_vault_on_device_encryption_state_tracker.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

// static
password_manager::OnDeviceEncryptionMetricsReporter*
IOSChromeOnDeviceEncryptionMetricsReporterFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<
          password_manager::OnDeviceEncryptionMetricsReporter>(profile,
                                                               /*create=*/true);
}

// static
IOSChromeOnDeviceEncryptionMetricsReporterFactory*
IOSChromeOnDeviceEncryptionMetricsReporterFactory::GetInstance() {
  static base::NoDestructor<IOSChromeOnDeviceEncryptionMetricsReporterFactory>
      instance;
  return instance.get();
}

IOSChromeOnDeviceEncryptionMetricsReporterFactory::
    IOSChromeOnDeviceEncryptionMetricsReporterFactory()
    : ProfileKeyedServiceFactoryIOS("OnDeviceEncryptionMetricsReporter",
                                    ProfileSelection::kNoInstanceInIncognito,
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(SyncServiceFactory::GetInstance());
}

IOSChromeOnDeviceEncryptionMetricsReporterFactory::
    ~IOSChromeOnDeviceEncryptionMetricsReporterFactory() = default;

void IOSChromeOnDeviceEncryptionMetricsReporterFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  password_manager::OnDeviceEncryptionMetricsReporter::RegisterProfilePrefs(
      registry);
}

std::unique_ptr<KeyedService>
IOSChromeOnDeviceEncryptionMetricsReporterFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::
              kPasswordManagerOnDeviceEncryptionMetricsReporter)) {
    return nullptr;
  }

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  auto password_tracker = std::make_unique<
      password_manager::PasswordTrustedVaultOnDeviceEncryptionStateTracker>(
      sync_service);

  // TODO(crbug.com/540854648): Implement passkey tracker.
  return std::make_unique<password_manager::OnDeviceEncryptionMetricsReporter>(
      /*passkey_tracker=*/nullptr, std::move(password_tracker),
      CHECK_DEREF(profile->GetPrefs()));
}
