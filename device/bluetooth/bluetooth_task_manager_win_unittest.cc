// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/test/test_pending_task.h"
#include "base/test/test_simple_task_runner.h"
#include "device/bluetooth/bluetooth_classic_win.h"
#include "device/bluetooth/bluetooth_init_win.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BluetoothTaskObserver : public device::BluetoothTaskManagerWin::Observer {
 public:
  BluetoothTaskObserver()
      : num_adapter_state_changed_(0),
        num_discovery_started_(0),
        num_discovery_stopped_(0) {
  }

  ~BluetoothTaskObserver() override {}

  void AdapterStateChanged(
      const device::BluetoothTaskManagerWin::AdapterState& state) override {
    num_adapter_state_changed_++;
  }

  void DiscoveryStarted(bool success) override { num_discovery_started_++; }

  void DiscoveryStopped() override { num_discovery_stopped_++; }

  int num_adapter_state_changed() const {
    return num_adapter_state_changed_;
  }

  int num_discovery_started() const {
    return num_discovery_started_;
  }

  int num_discovery_stopped() const {
    return num_discovery_stopped_;
  }

 private:
   int num_adapter_state_changed_;
   int num_discovery_started_;
   int num_discovery_stopped_;
};

}  // namespace

namespace device {

class BluetoothTaskManagerWinTest : public testing::Test {
 public:
  BluetoothTaskManagerWinTest()
      : ui_task_runner_(new base::TestSimpleTaskRunner()),
        bluetooth_task_runner_(new base::TestSimpleTaskRunner()),
        task_manager_(new BluetoothTaskManagerWin(ui_task_runner_)),
        has_bluetooth_stack_(device::bluetooth_init_win::HasBluetoothStack()) {
    task_manager_->InitializeWithBluetoothTaskRunner(bluetooth_task_runner_);
  }

  void SetUp() override { task_manager_->AddObserver(&observer_); }

  void TearDown() override { task_manager_->RemoveObserver(&observer_); }

  int GetPollingIntervalMs() const {
    return BluetoothTaskManagerWin::kPollIntervalMs;
  }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> ui_task_runner_;
  scoped_refptr<base::TestSimpleTaskRunner> bluetooth_task_runner_;
  scoped_refptr<BluetoothTaskManagerWin> task_manager_;
  BluetoothTaskObserver observer_;
  const bool has_bluetooth_stack_;
};

TEST_F(BluetoothTaskManagerWinTest, StartPolling) {
  EXPECT_EQ(1u, bluetooth_task_runner_->NumPendingTasks());
}

TEST_F(BluetoothTaskManagerWinTest, PollAdapterIfBluetoothStackIsAvailable) {
  bluetooth_task_runner_->RunPendingTasks();
  size_t num_expected_pending_tasks = has_bluetooth_stack_ ? 1 : 0;
  EXPECT_EQ(num_expected_pending_tasks,
            bluetooth_task_runner_->NumPendingTasks());
}

TEST_F(BluetoothTaskManagerWinTest, Polling) {
  if (!has_bluetooth_stack_)
    return;

  int num_polls = 5;

  for (int i = 0; i < num_polls; i++) {
    bluetooth_task_runner_->RunPendingTasks();
  }

  ui_task_runner_->RunPendingTasks();
  EXPECT_EQ(num_polls, observer_.num_adapter_state_changed());
}

TEST_F(BluetoothTaskManagerWinTest, SetPowered) {
  if (!has_bluetooth_stack_)
    return;

  bluetooth_task_runner_->ClearPendingTasks();
  task_manager_->PostSetPoweredBluetoothTask(true, base::OnceClosure(),
                                             base::OnceClosure());

  EXPECT_EQ(1u, bluetooth_task_runner_->NumPendingTasks());
  bluetooth_task_runner_->RunPendingTasks();
  EXPECT_TRUE(ui_task_runner_->NumPendingTasks() >= 1);
}

TEST_F(BluetoothTaskManagerWinTest, Discovery) {
  if (!has_bluetooth_stack_)
    return;

  bluetooth_task_runner_->RunPendingTasks();
  bluetooth_task_runner_->ClearPendingTasks();
  task_manager_->PostStartDiscoveryTask();
  bluetooth_task_runner_->RunPendingTasks();
  ui_task_runner_->RunPendingTasks();
  EXPECT_EQ(1, observer_.num_discovery_started());
  task_manager_->PostStopDiscoveryTask();
  bluetooth_task_runner_->RunPendingTasks();
  ui_task_runner_->RunPendingTasks();
  EXPECT_EQ(1, observer_.num_discovery_stopped());
}

}  // namespace device
