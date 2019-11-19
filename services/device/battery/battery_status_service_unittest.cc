// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/battery/battery_status_service.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "services/device/battery/battery_status_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class FakeBatteryManager : public BatteryStatusManager {
 public:
  explicit FakeBatteryManager(
      const BatteryStatusService::BatteryUpdateCallback& callback)
      : callback_(callback), start_invoked_count_(0), stop_invoked_count_(0) {}
  ~FakeBatteryManager() override {}

  // Methods from Battery Status Manager
  bool StartListeningBatteryChange() override {
    start_invoked_count_++;
    return true;
  }

  void StopListeningBatteryChange() override { stop_invoked_count_++; }

  void InvokeUpdateCallback(const mojom::BatteryStatus& status) {
    callback_.Run(status);
  }

  int start_invoked_count() const { return start_invoked_count_; }
  int stop_invoked_count() const { return stop_invoked_count_; }

 private:
  BatteryStatusService::BatteryUpdateCallback callback_;
  int start_invoked_count_;
  int stop_invoked_count_;

  DISALLOW_COPY_AND_ASSIGN(FakeBatteryManager);
};

}  // namespace

class BatteryStatusServiceTest : public testing::Test {
 public:
  BatteryStatusServiceTest()
      : battery_manager_(nullptr),
        callback1_invoked_count_(0),
        callback2_invoked_count_(0) {}
  ~BatteryStatusServiceTest() override {}

 protected:
  typedef BatteryStatusService::BatteryUpdateSubscription BatterySubscription;

  void SetUp() override {
    callback1_ = base::Bind(&BatteryStatusServiceTest::Callback1,
                            base::Unretained(this));
    callback2_ = base::Bind(&BatteryStatusServiceTest::Callback2,
                            base::Unretained(this));

    // We keep a raw pointer to the FakeBatteryManager, which we expect to
    // remain valid for the lifetime of the BatteryStatusService.
    auto battery_manager = std::make_unique<FakeBatteryManager>(
        battery_service_.GetUpdateCallbackForTesting());
    battery_manager_ = battery_manager.get();

    battery_service_.SetBatteryManagerForTesting(std::move(battery_manager));
  }

  void TearDown() override { base::RunLoop().RunUntilIdle(); }

  FakeBatteryManager* battery_manager() { return battery_manager_; }

  std::unique_ptr<BatterySubscription> AddCallback(
      const BatteryStatusService::BatteryUpdateCallback& callback) {
    return battery_service_.AddCallback(callback);
  }

  int callback1_invoked_count() const { return callback1_invoked_count_; }

  int callback2_invoked_count() const { return callback2_invoked_count_; }

  const mojom::BatteryStatus& battery_status() const { return battery_status_; }

  const BatteryStatusService::BatteryUpdateCallback& callback1() const {
    return callback1_;
  }

  const BatteryStatusService::BatteryUpdateCallback& callback2() const {
    return callback2_;
  }

 private:
  void Callback1(const mojom::BatteryStatus& status) {
    callback1_invoked_count_++;
    battery_status_ = status;
  }

  void Callback2(const mojom::BatteryStatus& status) {
    callback2_invoked_count_++;
    battery_status_ = status;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  BatteryStatusService battery_service_;
  FakeBatteryManager* battery_manager_;
  BatteryStatusService::BatteryUpdateCallback callback1_;
  BatteryStatusService::BatteryUpdateCallback callback2_;
  int callback1_invoked_count_;
  int callback2_invoked_count_;
  mojom::BatteryStatus battery_status_;

  DISALLOW_COPY_AND_ASSIGN(BatteryStatusServiceTest);
};

TEST_F(BatteryStatusServiceTest, AddFirstCallback) {
  std::unique_ptr<BatterySubscription> subscription1 = AddCallback(callback1());
  EXPECT_EQ(1, battery_manager()->start_invoked_count());
  EXPECT_EQ(0, battery_manager()->stop_invoked_count());
  subscription1.reset();
  EXPECT_EQ(1, battery_manager()->start_invoked_count());
  EXPECT_EQ(1, battery_manager()->stop_invoked_count());
}

TEST_F(BatteryStatusServiceTest, AddCallbackAfterUpdate) {
  std::unique_ptr<BatterySubscription> subscription1 = AddCallback(callback1());
  mojom::BatteryStatus status;
  battery_manager()->InvokeUpdateCallback(status);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback1_invoked_count());
  EXPECT_EQ(0, callback2_invoked_count());

  std::unique_ptr<BatterySubscription> subscription2 = AddCallback(callback2());
  EXPECT_EQ(1, callback1_invoked_count());
  EXPECT_EQ(1, callback2_invoked_count());
}

TEST_F(BatteryStatusServiceTest, TwoCallbacksUpdate) {
  std::unique_ptr<BatterySubscription> subscription1 = AddCallback(callback1());
  std::unique_ptr<BatterySubscription> subscription2 = AddCallback(callback2());

  mojom::BatteryStatus status;
  status.charging = true;
  status.charging_time = 100;
  status.discharging_time = 200;
  status.level = 0.5;
  battery_manager()->InvokeUpdateCallback(status);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, callback1_invoked_count());
  EXPECT_EQ(1, callback2_invoked_count());
  EXPECT_EQ(status.charging, battery_status().charging);
  EXPECT_EQ(status.charging_time, battery_status().charging_time);
  EXPECT_EQ(status.discharging_time, battery_status().discharging_time);
  EXPECT_EQ(status.level, battery_status().level);
}

TEST_F(BatteryStatusServiceTest, RemoveOneCallback) {
  std::unique_ptr<BatterySubscription> subscription1 = AddCallback(callback1());
  std::unique_ptr<BatterySubscription> subscription2 = AddCallback(callback2());

  mojom::BatteryStatus status;
  battery_manager()->InvokeUpdateCallback(status);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback1_invoked_count());
  EXPECT_EQ(1, callback2_invoked_count());

  subscription1.reset();
  battery_manager()->InvokeUpdateCallback(status);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback1_invoked_count());
  EXPECT_EQ(2, callback2_invoked_count());
}

}  // namespace device
