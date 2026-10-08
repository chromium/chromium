// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_SERVICE_FACTORY_H_

#import <memory>

#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace syncer {
class SyncServiceImpl;
}  // namespace syncer

// Singleton that owns all SyncServices and associates them with
// ProfileIOS.
class SyncServiceFactory
    : public TypedProfileKeyedServiceFactoryIOS<SyncServiceFactory,
                                                syncer::SyncService> {
 public:
  SyncServiceFactory(PassKey key);

  // Returns the service instance as syncer::SyncServiceImpl. Behavior
  // is undefined if a test factory has been installed and the service
  // is not a real instance.
  static syncer::SyncServiceImpl* GetForProfileAsSyncServiceImplForTesting(
      ProfileIOS* profile);

  // Iterates over all profiles that have been loaded so far and extract their
  // SyncService if present. Returned pointers are guaranteed to be not null.
  static std::vector<const syncer::SyncService*> GetAllSyncServices();

  // Returns the default factory, mainly for tests that want to use a real
  // SyncServiceImpl (as opposed to, say, a TestSyncService).
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_SERVICE_FACTORY_H_
