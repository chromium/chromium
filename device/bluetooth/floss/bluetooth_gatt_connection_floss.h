// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_GATT_CONNECTION_FLOSS_H_
#define DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_GATT_CONNECTION_FLOSS_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/floss/floss_adapter_client.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/floss/floss_gatt_manager_client.h"

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace floss {

// BluetoothGattConnectionFloss is the Floss implementation tracking a Gatt
// connection. It observes the adapter client directly to keep track of
// connection status.
class BluetoothGattConnectionFloss : public device::BluetoothGattConnection,
                                     public floss::FlossAdapterClient::Observer,
                                     public floss::FlossGattClientObserver {
 public:
  explicit BluetoothGattConnectionFloss(
      scoped_refptr<device::BluetoothAdapter> adapter,
      const FlossDeviceId& device_id);

  BluetoothGattConnectionFloss(const BluetoothGattConnectionFloss&) = delete;
  BluetoothGattConnectionFloss& operator=(const BluetoothGattConnectionFloss&) =
      delete;

  ~BluetoothGattConnectionFloss() override;

  // BluetoothGattConnection overrides:
  bool IsConnected() override;
  void Disconnect() override;

 private:
  // floss::FlossAdapterClient::Observer overrides.
  void AdapterDeviceDisconnected(const FlossDeviceId& device) override;

  // floss::FlossGattClientObserver overrides.
  void GattClientConnectionState(GattStatus status,
                                 int32_t client_id,
                                 bool connected,
                                 std::string address) override;

  /// Cached identity of this connection.
  FlossDeviceId id_;

  /// Is this gatt connection active?
  bool connected_;
};
}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_GATT_CONNECTION_FLOSS_H_
