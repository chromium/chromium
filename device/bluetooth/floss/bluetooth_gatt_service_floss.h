// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_GATT_SERVICE_FLOSS_H_
#define DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_GATT_SERVICE_FLOSS_H_

#include <map>

#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/floss/floss_gatt_client.h"

namespace floss {

class BluetoothAdapterFloss;

// Subclass of |BluetoothGattService| for platforms that use Floss.
class DEVICE_BLUETOOTH_EXPORT BluetoothGattServiceFloss
    : public device::BluetoothGattService,
      public FlossGattClientObserver {
 public:
  BluetoothGattServiceFloss(const BluetoothGattServiceFloss&) = delete;
  BluetoothGattServiceFloss& operator=(const BluetoothGattServiceFloss&) =
      delete;

  // Returns the adapter associated with this service.
  BluetoothAdapterFloss* GetAdapter() const;

  // Processes a |GattStatus| into a service error code.
  static device::BluetoothGattService::GattErrorCode GattStatusToServiceError(
      const GattStatus status);

  // Adds an observer for a specific handle. This observer will only get
  // callbacks invoked for that specific handle.
  void AddObserverForHandle(int32_t handle, FlossGattClientObserver* observer);

  // Removes the observer for a specific handle.
  void RemoveObserverForHandle(int32_t handle);

  // FlossGattClientObserver overrides.
  void GattCharacteristicRead(std::string address,
                              GattStatus status,
                              int32_t handle,
                              const std::vector<uint8_t>& data) override;
  void GattCharacteristicWrite(std::string address,
                               GattStatus status,
                               int32_t handle) override;
  void GattDescriptorRead(std::string address,
                          GattStatus status,
                          int32_t handle,
                          const std::vector<uint8_t>& data) override;
  void GattDescriptorWrite(std::string address,
                           GattStatus status,
                           int32_t handle) override;
  void GattNotify(std::string address,
                  int32_t handle,
                  const std::vector<uint8_t>& data) override;

 protected:
  explicit BluetoothGattServiceFloss(BluetoothAdapterFloss* adapter);
  ~BluetoothGattServiceFloss() override;

  // Cache of observers tied to a specific handle. When callbacks are observed
  // for a specific handle within this GATT service, it is dispatched here to
  // that specific observer.
  std::map<int32_t, raw_ptr<FlossGattClientObserver>> observer_by_handle_;

 private:
  // The adapter associated with (and which indirectly owns) this service.
  raw_ptr<BluetoothAdapterFloss> adapter_;
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_GATT_SERVICE_FLOSS_H_
