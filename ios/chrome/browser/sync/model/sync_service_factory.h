// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace syncer {
class SyncServiceImpl;
class SyncService;
}  // namespace syncer

// Singleton that owns all SyncServices and associates them with
// ProfileIOS.
class SyncServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static syncer::SyncService* GetForProfile(ProfileIOS* profile);
  static syncer::SyncService* GetForProfileIfExists(ProfileIOS* profile);
  static syncer::SyncServiceImpl* GetForProfileAsSyncServiceImplForTesting(
      ProfileIOS* profile);

  static SyncServiceFactory* GetInstance();

  // Iterates over all profiles that have been loaded so far and extract their
  // SyncService if present. Returned pointers are guaranteed to be not null.
  static std::vector<const syncer::SyncService*> GetAllSyncServices();

 private:
  friend class base::NoDestructor<SyncServiceFactory>;

  SyncServiceFactory();
  ~SyncServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_SERVICE_FACTORY_H_
