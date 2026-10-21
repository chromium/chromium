// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_SHARING_MODEL_MIGRATION_MIGRATABLE_SYNC_SERVICE_COORDINATOR_FACTORY_H_
#define IOS_CHROME_BROWSER_DATA_SHARING_MODEL_MIGRATION_MIGRATABLE_SYNC_SERVICE_COORDINATOR_FACTORY_H_

#import "components/data_sharing/public/migration/migratable_sync_service_coordinator.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace data_sharing {

// A factory to create a MigratableSyncServiceCoordinator.
class MigratableSyncServiceCoordinatorFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          MigratableSyncServiceCoordinatorFactory,
          MigratableSyncServiceCoordinator> {
 public:
  explicit MigratableSyncServiceCoordinatorFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace data_sharing

#endif  // IOS_CHROME_BROWSER_DATA_SHARING_MODEL_MIGRATION_MIGRATABLE_SYNC_SERVICE_COORDINATOR_FACTORY_H_
