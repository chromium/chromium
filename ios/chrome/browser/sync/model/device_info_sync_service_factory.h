// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_DEVICE_INFO_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_DEVICE_INFO_SYNC_SERVICE_FACTORY_H_

#import <memory>
#import <vector>

#import "components/sync_device_info/device_info_sync_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace syncer {
class DeviceInfoTracker;
}  // namespace syncer

// Singleton that owns all DeviceInfoSyncService and associates them with
// ProfileIOS.
class DeviceInfoSyncServiceFactory
    : public TypedProfileKeyedServiceFactoryIOS<DeviceInfoSyncServiceFactory,
                                                syncer::DeviceInfoSyncService> {
 public:
  DeviceInfoSyncServiceFactory(PassKey key);

  // Iterates over profiles and returns any trackers that can be found.
  static void GetAllDeviceInfoTrackers(
      std::vector<const syncer::DeviceInfoTracker*>* trackers);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_DEVICE_INFO_SYNC_SERVICE_FACTORY_H_
