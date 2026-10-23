// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/base/device_monitor_win.h"

#include "base/at_exit.h"
#include "base/scoped_observation.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class TestObserver : public DeviceMonitorWin::Observer {
 public:
  void Observe(DeviceMonitorWin* monitor) { observation_.Observe(monitor); }
  void Reset() { observation_.Reset(); }

 private:
  base::ScopedObservation<DeviceMonitorWin, DeviceMonitorWin::Observer>
      observation_{this};
};

}  // namespace

// Observers can outlive the AtExitManager (e.g. the serial enumerator's UI
// thread helper, which is leaked at browser shutdown), so the monitors must
// not be destroyed by it. If they were, `observer` would be left with a
// dangling pointer to the monitor and would call RemoveObserver() on it after
// it was freed.
TEST(DeviceMonitorWinTest, MonitorOutlivesAtExitManager) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);

  TestObserver observer;
  DeviceMonitorWin* monitor = nullptr;
  {
    base::ShadowingAtExitManager at_exit_manager;
    monitor = DeviceMonitorWin::GetForAllInterfaces();
    ASSERT_TRUE(monitor);
    observer.Observe(monitor);
  }

  EXPECT_EQ(monitor, DeviceMonitorWin::GetForAllInterfaces());
  observer.Reset();
}

}  // namespace device
