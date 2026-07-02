// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_device_watcher_mac.h"

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class BluetoothLowEnergyDeviceWatcherMacTest : public testing::Test {
 public:
  BluetoothLowEnergyDeviceWatcherMacTest()
      : ui_task_runner_(new base::TestSimpleTaskRunner()) {}

  void SetUp() override {}

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::TestSimpleTaskRunner> ui_task_runner_;
};

TEST_F(BluetoothLowEnergyDeviceWatcherMacTest, WatcherShouldBeReleased) {
  base::RunLoop loop;
  {
    // `watcher` should be released when it goes out of scope.
    auto watcher = BluetoothLowEnergyDeviceWatcherMac::CreateAndStartWatching(
        ui_task_runner_, base::DoNothing());
    watcher->set_destroy_callback_for_testing(loop.QuitClosure());
  }
  loop.Run();
}

}  // namespace device
