// Copyright 2012 The Chromium Authors
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
#include "base/win/scoped_com_initializer.h"
#include "media/audio/win/core_audio_util_win.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class SystemMessageWindowWinTest : public testing::Test {
 public:
  ~SystemMessageWindowWinTest() override {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(com_initializer_.Succeeded());
    system_monitor_.AddDevicesChangedObserver(&observer_);
  }

  // CoreAudioUtil::IsSupported() needs COM and caches its result, so COM must
  // be initialized before `window_` is constructed.
  base::win::ScopedCOMInitializer com_initializer_;
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

// KSCATEGORY_AUDIO arrivals should be reported as DEVTYPE_AUDIO changes only
// when Core Audio is unsupported. With Core Audio, AudioDeviceListenerWin
// reports audio device changes, so audio events that reach the shared HWND
// through other registrations (e.g. device::DeviceMonitorWin) must be ignored.
// Core Audio support varies by machine, so check whichever behavior applies.
TEST_F(SystemMessageWindowWinTest, AudioDeviceChange) {
  DEV_BROADCAST_DEVICEINTERFACE device_interface = {};
  device_interface.dbcc_size = sizeof(device_interface);
  device_interface.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
  device_interface.dbcc_classguid = KSCATEGORY_AUDIO;

  base::RunLoop run_loop;
  const bool core_audio_supported = CoreAudioUtil::IsSupported();
  if (core_audio_supported) {
    EXPECT_CALL(observer_, OnDevicesChanged(testing::_)).Times(0);
  } else {
    EXPECT_CALL(observer_, OnDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO))
        .WillOnce([&](base::SystemMonitor::DeviceType) { run_loop.Quit(); });
  }
  window_.OnDeviceChange(DBT_DEVICEARRIVAL,
                         reinterpret_cast<LPARAM>(&device_interface));
  if (core_audio_supported) {
    // SystemMonitor notifies observers via posted tasks, so quit only after
    // any notification posted by OnDeviceChange() has had a chance to run.
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
  }
  run_loop.Run();
}

}  // namespace media
