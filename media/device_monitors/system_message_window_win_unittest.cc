// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/device_monitors/system_message_window_win.h"

#include <dbt.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/system/system_monitor.h"
#include "base/test/mock_devices_changed_observer.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class SystemMessageWindowWinTest : public testing::Test {
 public:
  ~SystemMessageWindowWinTest() override {}

 protected:
  void SetUp() override {
    system_monitor_.AddDevicesChangedObserver(&observer_);
  }

  // Run single threaded to not require explicit COM initialization
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::SystemMonitor system_monitor_;
  base::MockDevicesChangedObserver observer_;
  SystemMessageWindowWin window_;
};

TEST_F(SystemMessageWindowWinTest, DevicesChanged) {
  EXPECT_CALL(observer_, OnDevicesChanged(testing::_)).Times(1);
  window_.OnDeviceChange(DBT_DEVNODES_CHANGED, NULL);
  base::RunLoop().RunUntilIdle();
}

TEST_F(SystemMessageWindowWinTest, RandomMessage) {
  window_.OnDeviceChange(DBT_DEVICEQUERYREMOVE, NULL);
  base::RunLoop().RunUntilIdle();
}

}  // namespace media
