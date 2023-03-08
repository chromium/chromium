// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_BLUETOOTH_GATT_SERVER_TEST_H_
#define DEVICE_BLUETOOTH_TEST_BLUETOOTH_GATT_SERVER_TEST_H_

#include <cstdint>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"
#include "device/bluetooth/test/test_bluetooth_local_gatt_service_delegate.h"

#if BUILDFLAG(IS_ANDROID)
#include "device/bluetooth/test/bluetooth_test_android.h"
#elif BUILDFLAG(IS_APPLE)
#include "device/bluetooth/test/bluetooth_test_mac.h"
#elif BUILDFLAG(IS_WIN)
#include "device/bluetooth/test/bluetooth_test_win.h"
#elif defined(USE_CAST_BLUETOOTH_ADAPTER)
#include "device/bluetooth/test/bluetooth_test_cast.h"
#elif BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#include "device/bluetooth/test/bluetooth_test_bluez.h"
#elif BUILDFLAG(IS_FUCHSIA)
#include "device/bluetooth/test/bluetooth_test_fuchsia.h"
#endif

namespace device {

// Base class for BlueZ GATT server unit tests.
class BluetoothGattServerTest : public BluetoothTest {
 public:
  BluetoothGattServerTest();
  ~BluetoothGattServerTest() override;

  // Start and complete boilerplate setup steps.
  void StartGattSetup();
  void CompleteGattSetup();

  // BluetoothTest overrides:
  void SetUp() override;
  void TearDown() override;

  // Utility methods to deal with values.
  static uint64_t GetInteger(const std::vector<uint8_t>& value);
  static std::vector<uint8_t> GetValue(uint64_t int_value);

 protected:
  base::WeakPtr<BluetoothLocalGattService> service_;

  std::unique_ptr<TestBluetoothLocalGattServiceDelegate> delegate_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_BLUETOOTH_GATT_SERVER_TEST_H_
