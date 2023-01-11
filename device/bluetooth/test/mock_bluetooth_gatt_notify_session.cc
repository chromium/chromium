// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_gatt_notify_session.h"
#include "base/functional/bind.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_characteristic.h"

using testing::Return;

namespace device {

MockBluetoothGattNotifySession::MockBluetoothGattNotifySession(
    base::WeakPtr<BluetoothRemoteGattCharacteristic> characteristic)
    : BluetoothGattNotifySession(characteristic) {
  ON_CALL(*this, IsActive()).WillByDefault(Return(true));
}

MockBluetoothGattNotifySession::~MockBluetoothGattNotifySession() = default;

void MockBluetoothGattNotifySession::StartTestNotifications(
    MockBluetoothAdapter* adapter,
    MockBluetoothGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  test_notifications_timer_.Start(
      FROM_HERE, base::Milliseconds(10),
      base::BindRepeating(&MockBluetoothGattNotifySession::DoNotify,
                          // base::Timer guarantees it won't call back after its
                          // destructor starts.
                          base::Unretained(this), base::Unretained(adapter),
                          characteristic, value));
}

void MockBluetoothGattNotifySession::StopTestNotifications() {
  test_notifications_timer_.Stop();
}

void MockBluetoothGattNotifySession::DoNotify(
    MockBluetoothAdapter* adapter,
    MockBluetoothGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  for (auto& observer : adapter->GetObservers())
    observer.GattCharacteristicValueChanged(adapter, characteristic, value);
}

}  // namespace device
