// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_LOW_ENERGY_SCAN_SESSION_FLOSS_H_
#define DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_LOW_ENERGY_SCAN_SESSION_FLOSS_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"
#include "device/bluetooth/bluetooth_low_energy_scan_session.h"
#include "device/bluetooth/floss/floss_lescan_client.h"

namespace floss {

class DEVICE_BLUETOOTH_EXPORT BluetoothLowEnergyScanSessionFloss
    : public device::BluetoothLowEnergyScanSession {
 public:
  BluetoothLowEnergyScanSessionFloss(
      std::unique_ptr<device::BluetoothLowEnergyScanFilter> filter,
      base::WeakPtr<device::BluetoothLowEnergyScanSession::Delegate> delegate,
      base::OnceCallback<void(const std::string&)> destructor_callback);
  ~BluetoothLowEnergyScanSessionFloss() override;

  void OnActivate(uint8_t scanner_id, bool success);
  void OnRelease();
  void OnDeviceFound(device::BluetoothDevice* device);
  void OnDeviceLost(device::BluetoothDevice* device);
  void OnRegistered(device::BluetoothUUID uuid);
  uint8_t GetScannerId() { return scanner_id_; }
  std::optional<ScanFilter> GetFlossScanFilter();

  base::WeakPtr<BluetoothLowEnergyScanSessionFloss> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
 private:
  std::unique_ptr<device::BluetoothLowEnergyScanFilter> filter_;
  base::WeakPtr<device::BluetoothLowEnergyScanSession::Delegate> delegate_;
  base::OnceCallback<void(const std::string&)> destructor_callback_;
  device::BluetoothUUID uuid_;
  uint8_t scanner_id_ = 0;
  bool has_activated_ = false;

  base::WeakPtrFactory<BluetoothLowEnergyScanSessionFloss> weak_ptr_factory_{
      this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_LOW_ENERGY_SCAN_SESSION_FLOSS_H_
