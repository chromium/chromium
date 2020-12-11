// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/power_monitor/power_monitor_broadcast_source.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/power_monitor_test_base.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class PowerMonitorBroadcastSourceTest : public testing::Test {
 protected:
  PowerMonitorBroadcastSourceTest() {}
  ~PowerMonitorBroadcastSourceTest() override {}

  void SetUp() override {
    auto power_monitor_source = std::make_unique<PowerMonitorBroadcastSource>(
        base::SequencedTaskRunnerHandle::Get());
    power_monitor_source_ptr_ = power_monitor_source.get();
    base::PowerMonitor::Initialize(std::move(power_monitor_source));
    power_monitor_source_ptr_->Init(mojo::NullRemote());
  }

  void TearDown() override {
    base::PowerMonitor::ShutdownForTesting();
    base::RunLoop().RunUntilIdle();
  }

  PowerMonitorBroadcastSource::Client* client() {
    return power_monitor_source_ptr_->client_for_testing();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

 private:
  PowerMonitorBroadcastSource* power_monitor_source_ptr_;

  DISALLOW_COPY_AND_ASSIGN(PowerMonitorBroadcastSourceTest);
};

TEST_F(PowerMonitorBroadcastSourceTest, PowerMessageReceiveBroadcast) {
  base::PowerMonitorTestObserver observer;
  base::PowerMonitor::AddObserver(&observer);

  // Sending resume when not suspended should have no effect.
  client()->Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.resumes(), 0);

  // Pretend we suspended.
  client()->Suspend();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.suspends(), 1);

  // Send a second suspend notification.  This should be suppressed.
  client()->Suspend();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.suspends(), 1);

  // Pretend we were awakened.
  client()->Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.resumes(), 1);

  // Send a duplicate resume notification.  This should be suppressed.
  client()->Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.resumes(), 1);

  // Pretend the device has gone on battery power
  client()->PowerStateChange(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.power_state_changes(), 1);
  EXPECT_EQ(observer.last_power_state(), true);

  // Repeated indications the device is on battery power should be suppressed.
  client()->PowerStateChange(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.power_state_changes(), 1);

  // Pretend the device has gone off battery power
  client()->PowerStateChange(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.power_state_changes(), 2);
  EXPECT_EQ(observer.last_power_state(), false);

  // Repeated indications the device is off battery power should be suppressed.
  client()->PowerStateChange(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.power_state_changes(), 2);
  base::PowerMonitor::RemoveObserver(&observer);
}

}  // namespace device
