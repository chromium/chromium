// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_DEVICE_INFO_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_DEVICE_INFO_SYNC_SERVICE_FACTORY_H_

#include <memory>
#include <vector>

#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace syncer {
class DeviceInfoSyncService;
class DeviceInfoTracker;
}  // namespace syncer

// Singleton that owns all DeviceInfoSyncService and associates them with
// ChromeBrowserState.
class DeviceInfoSyncServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static syncer::DeviceInfoSyncService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static DeviceInfoSyncServiceFactory* GetInstance();

  DeviceInfoSyncServiceFactory(const DeviceInfoSyncServiceFactory&) = delete;
  DeviceInfoSyncServiceFactory& operator=(const DeviceInfoSyncServiceFactory&) =
      delete;

  // Iterates over browser states and returns any trackers that can be found.
  static void GetAllDeviceInfoTrackers(
      std::vector<const syncer::DeviceInfoTracker*>* trackers);

 private:
  friend struct base::DefaultSingletonTraits<DeviceInfoSyncServiceFactory>;

  DeviceInfoSyncServiceFactory();
  ~DeviceInfoSyncServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_DEVICE_INFO_SYNC_SERVICE_FACTORY_H_
