// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADVERTISEMENT_MONITOR_SERVICE_PROVIDER_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADVERTISEMENT_MONITOR_SERVICE_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"

namespace bluez {

// BluetoothAdvertisementMonitorServiceProvider is used to provide a D-Bus
// object that the Bluetooth daemon can communicate with to register
// Advertisement Monitor service hierarchies.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdvertisementMonitorServiceProvider {
 public:
  // Interface for reacting to BluetoothAdvertisementMonitorServiceProvider
  // events.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when the advertisement monitor is successfully activated and
    // ready to start receiving device found or device lost events.
    virtual void OnActivate() = 0;

    // Called when the advertisement monitor is invalidated. The advertisement
    // monitor cannot recover from this state.
    virtual void OnRelease() = 0;

    virtual void OnDeviceFound(const dbus::ObjectPath& device_path) = 0;

    virtual void OnDeviceLost(const dbus::ObjectPath& device_path) = 0;
  };

  virtual ~BluetoothAdvertisementMonitorServiceProvider();

  // Writes an array of the service's properties into the provided writer.
  virtual void WriteProperties(dbus::MessageWriter* writer) {}

  virtual const dbus::ObjectPath& object_path() const = 0;

  // Create a BluetoothAdvertisementMonitorServiceProvider instance for
  // exporting the object identified by |object_path| onto the D-Bus connection
  // |bus|.
  static std::unique_ptr<BluetoothAdvertisementMonitorServiceProvider> Create(
      dbus::Bus* bus,
      const dbus::ObjectPath& object_path,
      std::unique_ptr<device::BluetoothLowEnergyScanFilter> filter,
      base::WeakPtr<Delegate> delegate);

 protected:
  BluetoothAdvertisementMonitorServiceProvider();
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADVERTISEMENT_MONITOR_SERVICE_PROVIDER_H_
