// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_SHARING_MODEL_MIGRATION_MIGRATABLE_SYNC_SERVICE_COORDINATOR_FACTORY_H_
#define IOS_CHROME_BROWSER_DATA_SHARING_MODEL_MIGRATION_MIGRATABLE_SYNC_SERVICE_COORDINATOR_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace data_sharing {
class MigratableSyncServiceCoordinator;

// A factory to create a MigratableSyncServiceCoordinator.
class MigratableSyncServiceCoordinatorFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  // Gets the MigratableSyncServiceCoordinator for the profile. Returns null
  // for incognito.
  static MigratableSyncServiceCoordinator* GetForProfile(ProfileIOS* profile);

  // Gets the lazy singleton instance of the
  // MigratableSyncServiceCoordinatorFactory.
  static MigratableSyncServiceCoordinatorFactory* GetInstance();

  // Disallow copy/assign.
  MigratableSyncServiceCoordinatorFactory(
      const MigratableSyncServiceCoordinatorFactory&) = delete;
  void operator=(const MigratableSyncServiceCoordinatorFactory&) = delete;

 private:
  friend class base::NoDestructor<MigratableSyncServiceCoordinatorFactory>;

  MigratableSyncServiceCoordinatorFactory();
  ~MigratableSyncServiceCoordinatorFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace data_sharing

#endif  // IOS_CHROME_BROWSER_DATA_SHARING_MODEL_MIGRATION_MIGRATABLE_SYNC_SERVICE_COORDINATOR_FACTORY_H_
