// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/power_monitor/power_monitor_message_broadcaster.h"

#include <memory>

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/power_monitor_test.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/cpp/power_monitor/power_monitor_broadcast_source.h"
#include "services/device/public/mojom/power_monitor.mojom.h"

namespace device {

class MockClient : public PowerMonitorBroadcastSource::Client {
 public:
  MockClient(base::OnceClosure service_connected)
      : service_connected_(std::move(service_connected)) {}
  ~MockClient() override = default;

  // Implement device::mojom::PowerMonitorClient
  void PowerStateChange(base::PowerStateObserver::BatteryPowerStatus
                            battery_power_status) override {
    power_state_changes_++;
    if (service_connected_)
      std::move(service_connected_).Run();
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
  base::OnceClosure service_connected_;
};

class PowerMonitorMessageBroadcasterTest : public DeviceServiceTestBase {
 public:
  PowerMonitorMessageBroadcasterTest(
      const PowerMonitorMessageBroadcasterTest&) = delete;
  PowerMonitorMessageBroadcasterTest& operator=(
      const PowerMonitorMessageBroadcasterTest&) = delete;

 protected:
  PowerMonitorMessageBroadcasterTest() = default;
  ~PowerMonitorMessageBroadcasterTest() override = default;

  void SetUp() override {
    DeviceServiceTestBase::SetUp();
  }

  void SetBatteryPowerStatus(
      base::PowerStateObserver::BatteryPowerStatus battery_power_status) {
    power_monitor_source_.SetBatteryPowerStatus(battery_power_status);
  }

  base::PowerStateObserver::BatteryPowerStatus GetBatteryPowerStatus() const {
    return power_monitor_source_.GetBatteryPowerStatus();
  }

  void TearDown() override {
    DestroyDeviceService();
  }

 protected:
  base::test::ScopedPowerMonitorTestSource power_monitor_source_;
};

TEST_F(PowerMonitorMessageBroadcasterTest, PowerMessageBroadcast) {
  base::RunLoop run_loop;

  std::unique_ptr<PowerMonitorBroadcastSource> broadcast_source(
      new PowerMonitorBroadcastSource(
          std::make_unique<MockClient>(run_loop.QuitClosure()),
          base::SequencedTaskRunner::GetCurrentDefault()));
  mojo::PendingRemote<mojom::PowerMonitor> remote_monitor;
  device_service()->BindPowerMonitor(
      remote_monitor.InitWithNewPipeAndPassReceiver());
  broadcast_source->Init(std::move(remote_monitor));
  run_loop.RunUntilIdle();

  MockClient* client =
      static_cast<MockClient*>(broadcast_source->client_for_testing());

  EXPECT_EQ(GetBatteryPowerStatus(),
            base::PowerStateObserver::BatteryPowerStatus::kUnknown);

  // Above PowerMonitorBroadcastSource::Init() will connect to Device Service to
  // bind device::mojom::PowerMonitor interface, on which AddClient() will be
  // called. This invokes a OnPowerStateChange() message unless the current
  // device is_on_battery state is false. See
  // PowerMonitorMessageBroadcasterTest.PowerClientUpdateWhenOnBattery below.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(client->power_state_changes(), 0);

  // Sending resume when not suspended should have no effect.
  power_monitor_source_.GenerateResumeEvent();
  EXPECT_EQ(client->resumes(), 0);

  // Pretend we suspended.
  power_monitor_source_.GenerateSuspendEvent();
  EXPECT_EQ(client->suspends(), 1);

  // Send a second suspend notification.  This should be suppressed.
  power_monitor_source_.GenerateSuspendEvent();
  EXPECT_EQ(client->suspends(), 1);

  // Pretend we were awakened.
  power_monitor_source_.GenerateResumeEvent();
  EXPECT_EQ(client->resumes(), 1);

  // Send a duplicate resume notification.  This should be suppressed.
  power_monitor_source_.GenerateResumeEvent();
  EXPECT_EQ(client->resumes(), 1);

  // Pretend the device has gone on battery power
  power_monitor_source_.GeneratePowerStateEvent(
      base::PowerStateObserver::BatteryPowerStatus::kBatteryPower);
  EXPECT_EQ(client->power_state_changes(), 1);

  // Repeated indications the device is on battery power should be suppressed.
  power_monitor_source_.GeneratePowerStateEvent(
      base::PowerStateObserver::BatteryPowerStatus::kBatteryPower);
  EXPECT_EQ(client->power_state_changes(), 1);

  // Pretend the device has gone off battery power
  power_monitor_source_.GeneratePowerStateEvent(
      base::PowerStateObserver::BatteryPowerStatus::kExternalPower);
  EXPECT_EQ(client->power_state_changes(), 2);

  // Repeated indications the device is off battery power should be suppressed.
  power_monitor_source_.GeneratePowerStateEvent(
      base::PowerStateObserver::BatteryPowerStatus::kExternalPower);
  EXPECT_EQ(client->power_state_changes(), 2);

  broadcast_source.reset();
  base::RunLoop().RunUntilIdle();
}

// When adding a PowerMonitorClient, the new client needs to be sent the
// device's current is_on_battery state. However, when clients are created
// their is_on_battery ivar == false. Therefore, when the device is not on
// battery, these new clients aren't sent an OnPowerStateChange() message.
// This test sets the device's is_on_battery state to true and confirms
// that a new client receives an OnPowerStateChange() message.
TEST_F(PowerMonitorMessageBroadcasterTest, PowerClientUpdateWhenOnBattery) {
  base::RunLoop run_loop;

  SetBatteryPowerStatus(
      base::PowerStateObserver::BatteryPowerStatus::kBatteryPower);

  std::unique_ptr<PowerMonitorBroadcastSource> broadcast_source(
      new PowerMonitorBroadcastSource(
          std::make_unique<MockClient>(run_loop.QuitClosure()),
          base::SequencedTaskRunner::GetCurrentDefault()));
  mojo::PendingRemote<mojom::PowerMonitor> remote_monitor;
  device_service()->BindPowerMonitor(
      remote_monitor.InitWithNewPipeAndPassReceiver());
  broadcast_source->Init(std::move(remote_monitor));
  run_loop.Run();

  MockClient* client =
      static_cast<MockClient*>(broadcast_source->client_for_testing());

  // Above PowerMonitorBroadcastSource::Init() will connect to Device Service to
  // bind device::mojom::PowerMonitor interface, on which AddClient() will be
  // called. This should immediately generate a power state change back to
  // PowerMonitorBroadcastSource.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(client->power_state_changes(), 1);
}

}  // namespace device
