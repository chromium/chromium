// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_classic_device_mac.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "device/bluetooth/bluetooth_adapter_mac.h"
#include "device/bluetooth/test/test_bluetooth_adapter_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class BluetoothClassicDeviceMacTest : public testing::Test {
 public:
  BluetoothClassicDeviceMacTest()
      : ui_task_runner_(new base::TestSimpleTaskRunner()),
        adapter_(BluetoothAdapterMac::CreateAdapterForTest(/*name=*/"",
                                                           /*address=*/"",
                                                           ui_task_runner_)),
        observer_(adapter_) {}

  void TearDown() override { task_environment_.RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::TestSimpleTaskRunner> ui_task_runner_;
  scoped_refptr<BluetoothAdapterMac> adapter_;
  TestBluetoothAdapterObserver observer_;
};

TEST_F(BluetoothClassicDeviceMacTest, DeviceDisconnected) {
  auto device = std::make_unique<BluetoothClassicDeviceMac>(adapter_.get(),
                                                            /*device=*/nil);
  device->OnDeviceDisconnected();
  EXPECT_EQ(0, observer_.device_added_count());
  EXPECT_EQ(1, observer_.device_changed_count());
  // Reset is needed here to clear stored pointer to `device` inside
  // `observer_`, otherwise that will be treated as dangling pointer when
  // `observer_` is destroyed out of the test case.
  observer_.Reset();
}

}  // namespace device
