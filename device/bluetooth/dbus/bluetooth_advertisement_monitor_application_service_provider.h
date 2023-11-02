// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADVERTISEMENT_MONITOR_APPLICATION_SERVICE_PROVIDER_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADVERTISEMENT_MONITOR_APPLICATION_SERVICE_PROVIDER_H_

#include <memory>

#include "dbus/bus.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_service_provider.h"

namespace bluez {

// BluetoothAdvertisementMonitorApplicationServiceProvider is used to provide a
// D-Bus object that the Bluetooth daemon can communicate with to register
// advertisement monitor service hierarchies.
// This interface exists to manage instances of
// BluetoothAdvertisementMonitorServiceProvider by signalling D-Bus when an
// instance is added/removed and returning all managed instances when queried by
// D-Bus.
class DEVICE_BLUETOOTH_EXPORT
    BluetoothAdvertisementMonitorApplicationServiceProvider {
 public:
  BluetoothAdvertisementMonitorApplicationServiceProvider(
      const BluetoothAdvertisementMonitorApplicationServiceProvider&) = delete;
  BluetoothAdvertisementMonitorApplicationServiceProvider& operator=(
      const BluetoothAdvertisementMonitorApplicationServiceProvider&) = delete;

  virtual ~BluetoothAdvertisementMonitorApplicationServiceProvider();

  // Creates the instance where |bus| is the D-Bus bus connection to export the
  // object onto and |object_path| is the D-Bus object path that it should have.
  static std::unique_ptr<
      BluetoothAdvertisementMonitorApplicationServiceProvider>
  Create(dbus::Bus* bus, const dbus::ObjectPath& object_path);

  virtual void AddMonitor(
      std::unique_ptr<BluetoothAdvertisementMonitorServiceProvider>
          advertisement_monitor_service_provider) = 0;
  virtual void RemoveMonitor(const dbus::ObjectPath& monitor_path) = 0;

 protected:
  BluetoothAdvertisementMonitorApplicationServiceProvider();
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADVERTISEMENT_MONITOR_APPLICATION_SERVICE_PROVIDER_H_
