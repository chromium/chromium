// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_DEVICE_INFO_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_DEVICE_INFO_SYNC_SERVICE_FACTORY_H_

#import <memory>
#import <vector>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace syncer {
class DeviceInfoSyncService;
class DeviceInfoTracker;
}  // namespace syncer

// Singleton that owns all DeviceInfoSyncService and associates them with
// ProfileIOS.
class DeviceInfoSyncServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static syncer::DeviceInfoSyncService* GetForProfile(ProfileIOS* profile);
  static syncer::DeviceInfoSyncService* GetForProfileIfExists(
      ProfileIOS* profile);
  static DeviceInfoSyncServiceFactory* GetInstance();

  // Iterates over profiles and returns any trackers that can be found.
  static void GetAllDeviceInfoTrackers(
      std::vector<const syncer::DeviceInfoTracker*>* trackers);

 private:
  friend class base::NoDestructor<DeviceInfoSyncServiceFactory>;

  DeviceInfoSyncServiceFactory();
  ~DeviceInfoSyncServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_DEVICE_INFO_SYNC_SERVICE_FACTORY_H_
