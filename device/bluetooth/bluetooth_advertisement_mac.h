// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADVERTISEMENT_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADVERTISEMENT_MAC_H_

#import <CoreBluetooth/CoreBluetooth.h>

#include <memory>

#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/bluetooth_export.h"

namespace device {

class BluetoothLowEnergyAdvertisementManagerMac;

// Simple implementation of BluetoothAdvertisement for Mac. The primary logic is
// currently handled in BluetoothLowEnergyAdvertisementManagerMac.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdvertisementMac
    : public BluetoothAdvertisement {
 public:
  enum Status {
    WAITING_FOR_ADAPTER,
    ADVERTISEMENT_PENDING,
    ADVERTISING,
    ERROR_ADVERTISING,
    UNREGISTERED,
  };

  BluetoothAdvertisementMac(
      std::unique_ptr<BluetoothAdvertisement::UUIDList> service_uuids,
      BluetoothAdapter::CreateAdvertisementCallback callback,
      BluetoothAdapter::AdvertisementErrorCallback error_callback,
      BluetoothLowEnergyAdvertisementManagerMac* advertisement_manager);

  BluetoothAdvertisementMac(const BluetoothAdvertisementMac&) = delete;
  BluetoothAdvertisementMac& operator=(const BluetoothAdvertisementMac&) =
      delete;

  // BluetoothAdvertisement overrides:
  void Unregister(SuccessCallback success_callback,
                  ErrorCallback error_callback) override;

  Status status() const { return status_; }

  bool is_waiting_for_adapter() { return status_ == WAITING_FOR_ADAPTER; }

  bool is_advertising() { return status_ == ADVERTISING; }

  bool is_advertisement_pending() { return status_ == ADVERTISEMENT_PENDING; }

  BluetoothAdvertisement::UUIDList service_uuids() { return *service_uuids_; }

 private:
  friend class BluetoothLowEnergyAdvertisementManagerMac;

  ~BluetoothAdvertisementMac() override;

  // Called by BluetoothLowEnergyAdvertisementManagerMac.
  void OnAdvertisementPending();
  void OnAdvertisementError(base::SingleThreadTaskRunner* task_runner,
                            BluetoothAdvertisement::ErrorCode error_code);
  void OnAdvertisementSuccess(base::SingleThreadTaskRunner* task_runner);
  void OnAdapterReset();
  void OnAdvertisementRestarted();

  void InvokeSuccessCallback();

  std::unique_ptr<BluetoothAdvertisement::UUIDList> service_uuids_;
  BluetoothAdapter::CreateAdvertisementCallback success_callback_;
  BluetoothAdapter::AdvertisementErrorCallback error_callback_;
  BluetoothLowEnergyAdvertisementManagerMac* advertisement_manager_;
  Status status_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADVERTISEMENT_MAC_H_
