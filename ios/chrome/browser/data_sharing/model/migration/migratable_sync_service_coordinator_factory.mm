// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_sharing/model/migration/migratable_sync_service_coordinator_factory.h"

#import "components/data_sharing/public/migration/migratable_sync_service_coordinator.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace data_sharing {

// static
MigratableSyncServiceCoordinator*
MigratableSyncServiceCoordinatorFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<MigratableSyncServiceCoordinator>(
          profile, /*create=*/true);
}

// static
MigratableSyncServiceCoordinatorFactory*
MigratableSyncServiceCoordinatorFactory::GetInstance() {
  static base::NoDestructor<MigratableSyncServiceCoordinatorFactory> instance;
  return instance.get();
}

MigratableSyncServiceCoordinatorFactory::
    MigratableSyncServiceCoordinatorFactory()
    : ProfileKeyedServiceFactoryIOS("MigratableSyncServiceCoordinator",
                                    ProfileSelection::kNoInstanceInIncognito) {}

MigratableSyncServiceCoordinatorFactory::
    ~MigratableSyncServiceCoordinatorFactory() = default;

std::unique_ptr<KeyedService>
MigratableSyncServiceCoordinatorFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<MigratableSyncServiceCoordinator>();
}

}  // namespace data_sharing
