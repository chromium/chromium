// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/battery/battery_monitor_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/battery/battery_status_manager.h"
#include "services/device/battery/battery_status_service.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "services/device/public/mojom/constants.mojom.h"

// These tests run against the implementation of the BatteryMonitor interface
// inside Device Service, with a dummy BatteryManager set as a source of the
// battery information. They can be run only on platforms that use the default
// battery service implementation, ie. on the platforms where
// BatteryStatusService is used.

namespace device {

namespace {

void ExpectBatteryStatus(bool* out_called,
                         const mojom::BatteryStatus& expected,
                         const base::Closure& quit_closure,
                         mojom::BatteryStatusPtr status) {
  if (out_called)
    *out_called = true;
  EXPECT_EQ(expected.charging, status->charging);
  EXPECT_EQ(expected.charging_time, status->charging_time);
  EXPECT_EQ(expected.discharging_time, status->discharging_time);
  EXPECT_EQ(expected.level, status->level);
  quit_closure.Run();
}

class FakeBatteryStatusManager : public BatteryStatusManager {
 public:
  explicit FakeBatteryStatusManager(
      const BatteryStatusService::BatteryUpdateCallback& callback)
      : callback_(callback), battery_status_available_(true), started_(false) {}
  ~FakeBatteryStatusManager() override {}

  // Methods from BatteryStatusManager.
  bool StartListeningBatteryChange() override {
    started_ = true;
    if (battery_status_available_)
      InvokeUpdateCallback();
    return battery_status_available_;
  }

  void StopListeningBatteryChange() override {}

  void InvokeUpdateCallback() {
    // Invoke asynchronously to mimic the OS-specific battery managers.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback_, status_));
  }

  void set_battery_status(const mojom::BatteryStatus& status) {
    status_ = status;
  }

  void set_battery_status_available(bool value) {
    battery_status_available_ = value;
  }

  bool started() { return started_; }

 private:
  BatteryStatusService::BatteryUpdateCallback callback_;
  bool battery_status_available_;
  bool started_;
  mojom::BatteryStatus status_;

  DISALLOW_COPY_AND_ASSIGN(FakeBatteryStatusManager);
};

class BatteryMonitorImplTest : public DeviceServiceTestBase {
 public:
  BatteryMonitorImplTest() = default;
  ~BatteryMonitorImplTest() override = default;

 protected:
  void SetUp() override {
    DeviceServiceTestBase::SetUp();

    BatteryStatusService* battery_service = BatteryStatusService::GetInstance();
    auto battery_manager = std::make_unique<FakeBatteryStatusManager>(
        battery_service->GetUpdateCallbackForTesting());
    battery_manager_ = battery_manager.get();
    battery_service->SetBatteryManagerForTesting(std::move(battery_manager));

    connector()->Connect(mojom::kServiceName,
                         battery_monitor_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    battery_monitor_.reset();
    // Enforce BatteryMonitorImpl destruction to run before Device Service
    // destruction shutting down battery status service, thus the
    // BatteryMonitorImpl instance can detach from battery status service
    // completely.
    base::RunLoop().RunUntilIdle();
  }

  FakeBatteryStatusManager* battery_manager() { return battery_manager_; }

  mojo::Remote<mojom::BatteryMonitor> battery_monitor_;

 private:
  FakeBatteryStatusManager* battery_manager_;

  DISALLOW_COPY_AND_ASSIGN(BatteryMonitorImplTest);
};

TEST_F(BatteryMonitorImplTest, BatteryManagerDefaultValues) {
  // Set the fake battery manager to return false on start. Verify that the
  // default battery values will be returned to consumers of Device Service.
  battery_manager()->set_battery_status_available(false);

  mojom::BatteryStatus default_status;
  default_status.charging = true;
  default_status.charging_time = 0;
  default_status.discharging_time = std::numeric_limits<double>::infinity();
  default_status.level = 1.0;
  base::RunLoop run_loop;
  battery_monitor_->QueryNextStatus(base::Bind(
      &ExpectBatteryStatus, nullptr, default_status, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(battery_manager()->started());
}

TEST_F(BatteryMonitorImplTest, BatteryManagerPredefinedValues) {
  // Set the fake battery manager to return predefined battery status values.
  // Verify that the predefined values will be returned to consumers of Device
  // Service.
  mojom::BatteryStatus status;
  status.charging = true;
  status.charging_time = 100;
  status.discharging_time = std::numeric_limits<double>::infinity();
  status.level = 0.5;
  battery_manager()->set_battery_status(status);

  base::RunLoop run_loop;
  battery_monitor_->QueryNextStatus(base::Bind(&ExpectBatteryStatus, nullptr,
                                               status, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(battery_manager()->started());
}

TEST_F(BatteryMonitorImplTest, BatteryManagerInvokeUpdate) {
  // Set the fake battery manager to return predefined battery status values,
  // after queried the battery status first time, query again, the second time
  // query will be pending, and then we change battery level to 0.6 and invoke
  // update. Verify that the second query will get the new level 0.6 correctly.
  mojom::BatteryStatus status;
  status.charging = true;
  status.charging_time = 100;
  status.discharging_time = std::numeric_limits<double>::infinity();
  status.level = 0.5;
  battery_manager()->set_battery_status(status);

  // The first time query should succeed.
  base::RunLoop run_loop1;
  battery_monitor_->QueryNextStatus(base::Bind(
      &ExpectBatteryStatus, nullptr, status, run_loop1.QuitClosure()));
  run_loop1.Run();
  EXPECT_TRUE(battery_manager()->started());

  // The second time query should be pending.
  bool called = false;
  status.level = 0.6;
  base::RunLoop run_loop2;
  battery_monitor_->QueryNextStatus(base::Bind(
      &ExpectBatteryStatus, &called, status, run_loop2.QuitClosure()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(called);

  // InvokeUpdateCallback should fire the pending query correctly.
  battery_manager()->set_battery_status(status);
  battery_manager()->InvokeUpdateCallback();
  run_loop2.Run();
  EXPECT_TRUE(called);
}

}  // namespace

}  // namespace device
