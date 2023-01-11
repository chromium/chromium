// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_LOW_ENERGY_SCAN_SESSION_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_LOW_ENERGY_SCAN_SESSION_BLUEZ_H_

#include <stddef.h>
#include <stdint.h>

#include "base/functional/callback.h"
#include "device/bluetooth/bluetooth_low_energy_scan_session.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_application_service_provider.h"

namespace bluez {

class DEVICE_BLUETOOTH_EXPORT BluetoothLowEnergyScanSessionBlueZ
    : public device::BluetoothLowEnergyScanSession,
      public bluez::BluetoothAdvertisementMonitorServiceProvider::Delegate {
 public:
  BluetoothLowEnergyScanSessionBlueZ(
      const BluetoothLowEnergyScanSessionBlueZ&) = delete;
  BluetoothLowEnergyScanSessionBlueZ& operator=(
      const BluetoothLowEnergyScanSessionBlueZ&) = delete;

  // Destructor callback is called with |session_id| as a parameter.
  BluetoothLowEnergyScanSessionBlueZ(
      const std::string& session_id,
      base::WeakPtr<BluetoothAdapterBlueZ> adapter,
      base::WeakPtr<device::BluetoothLowEnergyScanSession::Delegate> delegate,
      base::OnceCallback<void(const std::string&)> destructor_callback);

  ~BluetoothLowEnergyScanSessionBlueZ() override;

  // bluez::BluetoothAdvertisementMonitorServiceProvider::Delegate override:
  void OnActivate() override;
  void OnRelease() override;
  void OnDeviceFound(const dbus::ObjectPath& device_path) override;
  void OnDeviceLost(const dbus::ObjectPath& device_path) override;

  base::WeakPtr<BluetoothLowEnergyScanSessionBlueZ> GetWeakPtr();

 private:
  const std::string session_id_;
  base::WeakPtr<BluetoothAdapterBlueZ> adapter_;
  base::WeakPtr<device::BluetoothLowEnergyScanSession::Delegate> delegate_;
  base::OnceCallback<void(const std::string&)> destructor_callback_;
  bool has_activated_ = false;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothLowEnergyScanSessionBlueZ> weak_ptr_factory_{
      this};
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_LOW_ENERGY_SCAN_SESSION_BLUEZ_H_
