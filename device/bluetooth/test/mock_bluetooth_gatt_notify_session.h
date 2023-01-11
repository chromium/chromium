// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_NOTIFY_SESSION_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_NOTIFY_SESSION_H_

#include <stdint.h>

#include <string>

#include "base/functional/callback.h"
#include "base/timer/timer.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class MockBluetoothAdapter;
class MockBluetoothGattCharacteristic;

class MockBluetoothGattNotifySession : public BluetoothGattNotifySession {
 public:
  explicit MockBluetoothGattNotifySession(
      base::WeakPtr<BluetoothRemoteGattCharacteristic> characteristic);

  MockBluetoothGattNotifySession(const MockBluetoothGattNotifySession&) =
      delete;
  MockBluetoothGattNotifySession& operator=(
      const MockBluetoothGattNotifySession&) = delete;

  ~MockBluetoothGattNotifySession() override;

  MOCK_METHOD0(IsActive, bool());
  void Stop(base::OnceClosure c) override { Stop_(c); }
  MOCK_METHOD1(Stop_, void(base::OnceClosure&));

  // Starts notifying the adapter's observers that the characteristic's value
  // changed.
  // TODO(ortuno): Remove the following 3 functions. This is a temporary hack to
  // get layout tests working with Notifications.
  // http://crbug.com/543884
  void StartTestNotifications(MockBluetoothAdapter* adapter,
                              MockBluetoothGattCharacteristic* characteristic,
                              const std::vector<uint8_t>& value);
  void StopTestNotifications();

 private:
  void DoNotify(MockBluetoothAdapter* adapter,
                MockBluetoothGattCharacteristic* characteristic,
                const std::vector<uint8_t>& value);

  base::RepeatingTimer test_notifications_timer_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_NOTIFY_SESSION_H_
