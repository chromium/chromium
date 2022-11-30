// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_ADVERTISEMENT_MONITOR_APPLICATION_SERVICE_PROVIDER_H_
#define DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_ADVERTISEMENT_MONITOR_APPLICATION_SERVICE_PROVIDER_H_

#include <string>

#include "base/containers/flat_set.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_application_service_provider.h"
#include "device/bluetooth/dbus/fake_bluetooth_advertisement_monitor_service_provider.h"

namespace bluez {

// FakeBluetoothAdvertisementMonitorApplicationServiceProvider simulates
// behavior of an Advertisement Monitor Application Service Provider object and
// is used in test cases.
class DEVICE_BLUETOOTH_EXPORT
    FakeBluetoothAdvertisementMonitorApplicationServiceProvider
    : public BluetoothAdvertisementMonitorApplicationServiceProvider {
 public:
  explicit FakeBluetoothAdvertisementMonitorApplicationServiceProvider(
      const dbus::ObjectPath& object_path);
  FakeBluetoothAdvertisementMonitorApplicationServiceProvider(
      const FakeBluetoothAdvertisementMonitorApplicationServiceProvider&) =
      delete;
  FakeBluetoothAdvertisementMonitorApplicationServiceProvider& operator=(
      const FakeBluetoothAdvertisementMonitorApplicationServiceProvider&) =
      delete;

  ~FakeBluetoothAdvertisementMonitorApplicationServiceProvider() override;

  // BluetoothAdvertisementMonitorApplicationServiceProvider overrides:
  void AddMonitor(std::unique_ptr<BluetoothAdvertisementMonitorServiceProvider>
                      advertisement_monitor_service_provider) override;
  void RemoveMonitor(const dbus::ObjectPath& monitor_path) override;

  size_t AdvertisementMonitorsCount() const;

  FakeBluetoothAdvertisementMonitorServiceProvider*
  GetLastAddedAdvertisementMonitorServiceProvider();

 private:
  // Key is the object path of the AdvertisementMonitorServiceProvider
  std::map<std::string,
           std::unique_ptr<BluetoothAdvertisementMonitorServiceProvider>>
      advertisement_monitor_providers_;

  std::string last_added_advertisement_monitor_provider_path_;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_ADVERTISEMENT_MONITOR_APPLICATION_SERVICE_PROVIDER_H_
