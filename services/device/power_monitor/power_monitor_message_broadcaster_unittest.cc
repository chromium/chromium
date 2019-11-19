// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/power_monitor/power_monitor_message_broadcaster.h"

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/power_monitor_test_base.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/cpp/power_monitor/power_monitor_broadcast_source.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/power_monitor.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

class MockClient : public PowerMonitorBroadcastSource::Client {
 public:
  MockClient(base::Closure service_connected)
      : service_connected_(service_connected) {}
  ~MockClient() override = default;

  // Implement device::mojom::PowerMonitorClient
  void PowerStateChange(bool on_battery_power) override {
    power_state_changes_++;
    service_connected_.Run();
  }
  void Suspend() override { suspends_++; }
  void Resume() override { resumes_++; }

  // Test status counts.
  int power_state_changes() { return power_state_changes_; }
  int suspends() { return suspends_; }
  int resumes() { return resumes_; }

 private:
  int power_state_changes_ = 0;  // Count of OnPowerStateChange notifications.
  int suspends_ = 0;             // Count of OnSuspend notifications.
  int resumes_ = 0;              // Count of OnResume notifications.
  base::Closure service_connected_;
};

class PowerMonitorMessageBroadcasterTest : public DeviceServiceTestBase {
 protected:
  PowerMonitorMessageBroadcasterTest() = default;
  ~PowerMonitorMessageBroadcasterTest() override = default;

  void SetUp() override {
    DeviceServiceTestBase::SetUp();

    power_monitor_source_ = new base::PowerMonitorTestSource();
    base::PowerMonitor::Initialize(
        std::unique_ptr<base::PowerMonitorSource>(power_monitor_source_));
  }

  void TearDown() override {
    // The DeviceService must be destroyed before shutting down the
    // PowerMonitor, which the DeviceService is observing.
    DestroyDeviceService();
    base::PowerMonitor::ShutdownForTesting();
  }

  base::PowerMonitorTestSource* source() { return power_monitor_source_; }

 private:
  base::PowerMonitorTestSource* power_monitor_source_;

  DISALLOW_COPY_AND_ASSIGN(PowerMonitorMessageBroadcasterTest);
};

TEST_F(PowerMonitorMessageBroadcasterTest, PowerMessageBroadcast) {
  base::RunLoop run_loop;

  std::unique_ptr<PowerMonitorBroadcastSource> broadcast_source(
      new PowerMonitorBroadcastSource(
          std::make_unique<MockClient>(run_loop.QuitClosure()),
          base::SequencedTaskRunnerHandle::Get()));
  mojo::PendingRemote<mojom::PowerMonitor> remote_monitor;
  connector()->Connect(mojom::kServiceName,
                       remote_monitor.InitWithNewPipeAndPassReceiver());
  broadcast_source->Init(std::move(remote_monitor));
  run_loop.Run();

  MockClient* client =
      static_cast<MockClient*>(broadcast_source->client_for_testing());

  // Above PowerMonitorBroadcastSource::Init() will connect to Device Service to
  // bind device::mojom::PowerMonitor interface, on which AddClient() will be
  // called then, this should invoke immediatelly a power state change back to
  // PowerMonitorBroadcastSource.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(client->power_state_changes(), 1);

  // Sending resume when not suspended should have no effect.
  source()->GenerateResumeEvent();
  EXPECT_EQ(client->resumes(), 0);

  // Pretend we suspended.
  source()->GenerateSuspendEvent();
  EXPECT_EQ(client->suspends(), 1);

  // Send a second suspend notification.  This should be suppressed.
  source()->GenerateSuspendEvent();
  EXPECT_EQ(client->suspends(), 1);

  // Pretend we were awakened.
  source()->GenerateResumeEvent();
  EXPECT_EQ(client->resumes(), 1);

  // Send a duplicate resume notification.  This should be suppressed.
  source()->GenerateResumeEvent();
  EXPECT_EQ(client->resumes(), 1);

  // Pretend the device has gone on battery power
  source()->GeneratePowerStateEvent(true);
  EXPECT_EQ(client->power_state_changes(), 2);

  // Repeated indications the device is on battery power should be suppressed.
  source()->GeneratePowerStateEvent(true);
  EXPECT_EQ(client->power_state_changes(), 2);

  // Pretend the device has gone off battery power
  source()->GeneratePowerStateEvent(false);
  EXPECT_EQ(client->power_state_changes(), 3);

  // Repeated indications the device is off battery power should be suppressed.
  source()->GeneratePowerStateEvent(false);
  EXPECT_EQ(client->power_state_changes(), 3);

  broadcast_source.reset();
  base::RunLoop().RunUntilIdle();
}

}  // namespace device
