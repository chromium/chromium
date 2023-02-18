// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_simple_task_runner.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_win.h"
#include "device/bluetooth/bluetooth_classic_win.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session_outcome.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"
#include "device/bluetooth/test/test_bluetooth_adapter_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kAdapterAddress[] = "A1:B2:C3:D4:E5:F6";
const char kAdapterName[] = "Bluetooth Adapter Name";

const char kTestAudioSdpName[] = "Audio";
const char kTestAudioSdpName2[] = "Audio2";
const char kTestAudioSdpBytes[] =
    "35510900000a00010001090001350319110a09000435103506190100090019350619001909"
    "010209000535031910020900093508350619110d090102090100250c417564696f20536f75"
    "726365090311090001";
const device::BluetoothUUID kTestAudioSdpUuid("110a");

void MakeDeviceState(const std::string& name,
                     const std::string& address,
                     device::BluetoothTaskManagerWin::DeviceState* state) {
  state->name = name;
  state->address = address;
  state->bluetooth_class = 0;
  state->authenticated = false;
  state->connected = false;
}

}  // namespace

namespace device {

class BluetoothAdapterWinTest : public testing::Test {
 public:
  BluetoothAdapterWinTest()
      : ui_task_runner_(new base::TestSimpleTaskRunner()),
        bluetooth_task_runner_(new base::TestSimpleTaskRunner()),
        adapter_(new BluetoothAdapterWin()),
        adapter_win_(static_cast<BluetoothAdapterWin*>(adapter_.get())),
        observer_(adapter_),
        init_callback_called_(false) {
    adapter_win_->InitForTest(
        base::BindOnce(&BluetoothAdapterWinTest::RunInitCallback,
                       base::Unretained(this)),
        nullptr, ui_task_runner_, bluetooth_task_runner_);
  }

  void SetUp() override {
    num_start_discovery_callbacks_ = 0;
    num_start_discovery_error_callbacks_ = 0;
    num_stop_discovery_callbacks_ = 0;
    num_stop_discovery_error_callbacks_ = 0;
  }

  void RunInitCallback() { init_callback_called_ = true; }

  // This queue is used to store discovery sessions after a successful start so
  // we can test stopping discovery and so the destructor is not called
  base::queue<std::unique_ptr<BluetoothDiscoverySession>>
      active_discovery_sessions_;

  void DiscoverySessionCallbackPassthrough(
      base::OnceClosure callback,
      std::unique_ptr<BluetoothDiscoverySession> new_session) {
    active_discovery_sessions_.push(std::move(new_session));
    std::move(callback).Run();
  }

  void IncrementNumStartDiscoveryCallbacks() {
    num_start_discovery_callbacks_++;
  }

  void IncrementNumStartDiscoveryErrorCallbacks() {
    num_start_discovery_error_callbacks_++;
  }

  void IncrementNumStopDiscoveryCallbacks() { num_stop_discovery_callbacks_++; }

  void IncrementNumStopDiscoveryErrorCallbacks(
      UMABluetoothDiscoverySessionOutcome) {
    num_stop_discovery_error_callbacks_++;
  }

  void IncrementNumStopErrorCallbacks() {
    ++num_stop_discovery_error_callbacks_;
  }

  typedef base::OnceCallback<void(UMABluetoothDiscoverySessionOutcome)>
      DiscoverySessionErrorCallback;

  using ErrorCallback = base::OnceClosure;

  void CallStartDiscoverySession() {
    adapter_win_->StartDiscoverySession(
        /*client_name=*/std::string(),
        base::BindOnce(
            &BluetoothAdapterWinTest::DiscoverySessionCallbackPassthrough,
            base::Unretained(this),
            base::BindOnce(
                &BluetoothAdapterWinTest::IncrementNumStartDiscoveryCallbacks,
                base::Unretained(this))),
        base::BindOnce(
            &BluetoothAdapterWinTest::IncrementNumStartDiscoveryErrorCallbacks,
            base::Unretained(this)));
  }

  void StopTopDiscoverySession(base::OnceClosure callback,
                               ErrorCallback error_callback) {
    active_discovery_sessions_.front()->Stop(std::move(callback),
                                             std::move(error_callback));
    active_discovery_sessions_.pop();
  }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> ui_task_runner_;
  scoped_refptr<base::TestSimpleTaskRunner> bluetooth_task_runner_;
  scoped_refptr<BluetoothAdapter> adapter_;
  raw_ptr<BluetoothAdapterWin> adapter_win_;
  TestBluetoothAdapterObserver observer_;
  bool init_callback_called_;
  int num_start_discovery_callbacks_;
  int num_start_discovery_error_callbacks_;
  int num_stop_discovery_callbacks_;
  int num_stop_discovery_error_callbacks_;
};

TEST_F(BluetoothAdapterWinTest, AdapterNotPresent) {
  BluetoothTaskManagerWin::AdapterState state;
  adapter_win_->AdapterStateChanged(state);
  EXPECT_FALSE(adapter_win_->IsPresent());
}

TEST_F(BluetoothAdapterWinTest, AdapterPresent) {
  BluetoothTaskManagerWin::AdapterState state;
  state.address = kAdapterAddress;
  state.name = kAdapterName;
  adapter_win_->AdapterStateChanged(state);
  EXPECT_TRUE(adapter_win_->IsPresent());
}

TEST_F(BluetoothAdapterWinTest, AdapterPresentChanged) {
  BluetoothTaskManagerWin::AdapterState state;
  state.address = kAdapterAddress;
  state.name = kAdapterName;
  adapter_win_->AdapterStateChanged(state);
  EXPECT_EQ(1, observer_.present_changed_count());
  adapter_win_->AdapterStateChanged(state);
  EXPECT_EQ(1, observer_.present_changed_count());
  BluetoothTaskManagerWin::AdapterState empty_state;
  adapter_win_->AdapterStateChanged(empty_state);
  EXPECT_EQ(2, observer_.present_changed_count());
}

TEST_F(BluetoothAdapterWinTest, AdapterPoweredChanged) {
  BluetoothTaskManagerWin::AdapterState state;
  state.powered = true;
  adapter_win_->AdapterStateChanged(state);
  EXPECT_EQ(1, observer_.powered_changed_count());
  adapter_win_->AdapterStateChanged(state);
  EXPECT_EQ(1, observer_.powered_changed_count());
  state.powered = false;
  adapter_win_->AdapterStateChanged(state);
  EXPECT_EQ(2, observer_.powered_changed_count());
}

TEST_F(BluetoothAdapterWinTest, AdapterInitialized) {
  EXPECT_FALSE(adapter_win_->IsInitialized());
  EXPECT_FALSE(init_callback_called_);
  BluetoothTaskManagerWin::AdapterState state;
  adapter_win_->AdapterStateChanged(state);
  EXPECT_TRUE(adapter_win_->IsInitialized());
  EXPECT_TRUE(init_callback_called_);
}

TEST_F(BluetoothAdapterWinTest, SingleStartDiscovery) {
  bluetooth_task_runner_->ClearPendingTasks();
  CallStartDiscoverySession();
  EXPECT_FALSE(ui_task_runner_->HasPendingTask());
  EXPECT_EQ(1u, bluetooth_task_runner_->NumPendingTasks());
  EXPECT_FALSE(adapter_->IsDiscovering());
  EXPECT_EQ(0, num_start_discovery_callbacks_);
  adapter_win_->DiscoveryStarted(true);
  ui_task_runner_->RunPendingTasks();
  EXPECT_TRUE(adapter_->IsDiscovering());
  EXPECT_EQ(1, num_start_discovery_callbacks_);
  EXPECT_EQ(1, observer_.discovering_changed_count());
}

TEST_F(BluetoothAdapterWinTest, SingleStartDiscoveryFailure) {
  CallStartDiscoverySession();
  adapter_win_->DiscoveryStarted(false);
  ui_task_runner_->RunPendingTasks();
  EXPECT_FALSE(adapter_->IsDiscovering());
  EXPECT_EQ(1, num_start_discovery_error_callbacks_);
  EXPECT_EQ(0, observer_.discovering_changed_count());
}

TEST_F(BluetoothAdapterWinTest, MultipleStartDiscoveries) {
  bluetooth_task_runner_->ClearPendingTasks();
  int num_discoveries = 5;
  for (int i = 0; i < num_discoveries; i++) {
    CallStartDiscoverySession();
    EXPECT_EQ(1u, bluetooth_task_runner_->NumPendingTasks());
  }
  EXPECT_FALSE(ui_task_runner_->HasPendingTask());
  EXPECT_FALSE(adapter_->IsDiscovering());
  EXPECT_EQ(0, num_start_discovery_callbacks_);
  adapter_win_->DiscoveryStarted(true);
  ui_task_runner_->RunPendingTasks();
  EXPECT_TRUE(adapter_->IsDiscovering());
  EXPECT_EQ(num_discoveries, num_start_discovery_callbacks_);
  EXPECT_EQ(1, observer_.discovering_changed_count());
}

TEST_F(BluetoothAdapterWinTest, MultipleStartDiscoveriesFailure) {
  int num_discoveries = 5;
  for (int i = 0; i < num_discoveries; i++) {
    CallStartDiscoverySession();
  }
  // Fake callback from OS for initial start call.
  adapter_win_->DiscoveryStarted(false);
  // Fake callback from OS for second call which includes the 4 remaining starts
  // calls.
  adapter_win_->DiscoveryStarted(false);

  EXPECT_FALSE(adapter_->IsDiscovering());
  EXPECT_EQ(num_discoveries, num_start_discovery_error_callbacks_);
  EXPECT_EQ(0, observer_.discovering_changed_count());
}

TEST_F(BluetoothAdapterWinTest, MultipleStartDiscoveriesAfterDiscovering) {
  CallStartDiscoverySession();
  adapter_win_->DiscoveryStarted(true);
  ui_task_runner_->RunPendingTasks();
  EXPECT_TRUE(adapter_->IsDiscovering());
  EXPECT_EQ(1, num_start_discovery_callbacks_);

  bluetooth_task_runner_->ClearPendingTasks();
  for (int i = 0; i < 5; i++) {
    int num_start_discovery_callbacks = num_start_discovery_callbacks_;
    CallStartDiscoverySession();
    EXPECT_TRUE(adapter_->IsDiscovering());
    EXPECT_FALSE(bluetooth_task_runner_->HasPendingTask());
    EXPECT_FALSE(ui_task_runner_->HasPendingTask());
    EXPECT_EQ(num_start_discovery_callbacks + 1,
              num_start_discovery_callbacks_);
  }
  EXPECT_EQ(1, observer_.discovering_changed_count());
}

TEST_F(BluetoothAdapterWinTest, StartDiscoveryAfterDiscoveringFailure) {
  CallStartDiscoverySession();
  adapter_win_->DiscoveryStarted(false);
  ui_task_runner_->RunPendingTasks();
  EXPECT_FALSE(adapter_->IsDiscovering());
  EXPECT_EQ(1, num_start_discovery_error_callbacks_);

  CallStartDiscoverySession();
  adapter_win_->DiscoveryStarted(true);
  ui_task_runner_->RunPendingTasks();
  EXPECT_TRUE(adapter_->IsDiscovering());
  EXPECT_EQ(1, num_start_discovery_callbacks_);
}

TEST_F(BluetoothAdapterWinTest, SingleStopDiscovery) {
  CallStartDiscoverySession();
  adapter_win_->DiscoveryStarted(true);
  ui_task_runner_->ClearPendingTasks();
  StopTopDiscoverySession(
      base::BindOnce(
          &BluetoothAdapterWinTest::IncrementNumStopDiscoveryCallbacks,
          base::Unretained(this)),
      ErrorCallback());
  EXPECT_TRUE(adapter_->IsDiscovering());
  EXPECT_EQ(0, num_stop_discovery_callbacks_);
  bluetooth_task_runner_->ClearPendingTasks();
  adapter_win_->DiscoveryStopped();
  ui_task_runner_->RunPendingTasks();
  EXPECT_FALSE(adapter_->IsDiscovering());
  EXPECT_EQ(1, num_stop_discovery_callbacks_);
  EXPECT_FALSE(bluetooth_task_runner_->HasPendingTask());
  EXPECT_EQ(2, observer_.discovering_changed_count());
}

TEST_F(BluetoothAdapterWinTest, MultipleStopDiscoveries) {
  int num_discoveries = 5;
  for (int i = 0; i < num_discoveries; i++) {
    CallStartDiscoverySession();
  }
  adapter_win_->DiscoveryStarted(true);
  ui_task_runner_->RunPendingTasks();
  bluetooth_task_runner_->ClearPendingTasks();
  for (int i = 0; i < num_discoveries - 1; i++) {
    StopTopDiscoverySession(
        base::BindOnce(
            &BluetoothAdapterWinTest::IncrementNumStopDiscoveryCallbacks,
            base::Unretained(this)),
        ErrorCallback());
    EXPECT_FALSE(bluetooth_task_runner_->HasPendingTask());
    ui_task_runner_->RunPendingTasks();
    EXPECT_EQ(i + 1, num_stop_discovery_callbacks_);
  }
  StopTopDiscoverySession(
      base::BindOnce(
          &BluetoothAdapterWinTest::IncrementNumStopDiscoveryCallbacks,
          base::Unretained(this)),
      ErrorCallback());
  EXPECT_EQ(1u, bluetooth_task_runner_->NumPendingTasks());
  EXPECT_TRUE(adapter_->IsDiscovering());
  adapter_win_->DiscoveryStopped();
  ui_task_runner_->RunPendingTasks();
  EXPECT_FALSE(adapter_->IsDiscovering());
  EXPECT_EQ(num_discoveries, num_stop_discovery_callbacks_);
  EXPECT_EQ(2, observer_.discovering_changed_count());
}

TEST_F(BluetoothAdapterWinTest,
       StartDiscoveryAndStartDiscoveryAndStopDiscoveries) {
  CallStartDiscoverySession();
  adapter_win_->DiscoveryStarted(true);
  CallStartDiscoverySession();
  ui_task_runner_->RunPendingTasks();
  bluetooth_task_runner_->ClearPendingTasks();
  StopTopDiscoverySession(
      base::BindOnce(
          &BluetoothAdapterWinTest::IncrementNumStopDiscoveryCallbacks,
          base::Unretained(this)),
      ErrorCallback());
  EXPECT_FALSE(bluetooth_task_runner_->HasPendingTask());
  StopTopDiscoverySession(
      base::BindOnce(
          &BluetoothAdapterWinTest::IncrementNumStopDiscoveryCallbacks,
          base::Unretained(this)),
      ErrorCallback());
  EXPECT_EQ(1u, bluetooth_task_runner_->NumPendingTasks());
}

TEST_F(BluetoothAdapterWinTest,
       StartDiscoveryAndStopDiscoveryAndStartDiscovery) {
  CallStartDiscoverySession();
  adapter_win_->DiscoveryStarted(true);
  EXPECT_TRUE(adapter_->IsDiscovering());
  ui_task_runner_->RunPendingTasks();
  active_discovery_sessions_.front()->Stop(
      base::BindOnce(
          &BluetoothAdapterWinTest::IncrementNumStopDiscoveryCallbacks,
          base::Unretained(this)),
      ErrorCallback());
  adapter_win_->DiscoveryStopped();
  ui_task_runner_->RunPendingTasks();
  EXPECT_FALSE(adapter_->IsDiscovering());
  CallStartDiscoverySession();
  adapter_win_->DiscoveryStarted(true);
  EXPECT_TRUE(adapter_->IsDiscovering());
}

TEST_F(BluetoothAdapterWinTest, StartDiscoveryBeforeDiscoveryStopped) {
  CallStartDiscoverySession();
  adapter_win_->DiscoveryStarted(true);
  ui_task_runner_->RunPendingTasks();
  StopTopDiscoverySession(
      base::BindOnce(
          &BluetoothAdapterWinTest::IncrementNumStopDiscoveryCallbacks,
          base::Unretained(this)),
      ErrorCallback());
  CallStartDiscoverySession();
  bluetooth_task_runner_->ClearPendingTasks();
  adapter_win_->DiscoveryStopped();
  EXPECT_EQ(1u, bluetooth_task_runner_->NumPendingTasks());
}

TEST_F(BluetoothAdapterWinTest, DevicesPolled) {
  std::vector<std::unique_ptr<BluetoothTaskManagerWin::DeviceState>> devices;
  devices.push_back(std::make_unique<BluetoothTaskManagerWin::DeviceState>());
  devices.push_back(std::make_unique<BluetoothTaskManagerWin::DeviceState>());
  devices.push_back(std::make_unique<BluetoothTaskManagerWin::DeviceState>());

  BluetoothTaskManagerWin::DeviceState* android_phone_state = devices[0].get();
  BluetoothTaskManagerWin::DeviceState* laptop_state = devices[1].get();
  BluetoothTaskManagerWin::DeviceState* iphone_state = devices[2].get();
  MakeDeviceState("phone", "A1:B2:C3:D4:E5:E0", android_phone_state);
  MakeDeviceState("laptop", "A1:B2:C3:D4:E5:E1", laptop_state);
  MakeDeviceState("phone", "A1:B2:C3:D4:E5:E2", iphone_state);

  // Add 3 devices
  observer_.Reset();
  adapter_win_->DevicesPolled(devices);
  EXPECT_EQ(3, observer_.device_added_count());
  EXPECT_EQ(0, observer_.device_removed_count());
  EXPECT_EQ(0, observer_.device_changed_count());

  // Change a device name
  android_phone_state->name = std::string("phone2");
  observer_.Reset();
  adapter_win_->DevicesPolled(devices);
  EXPECT_EQ(0, observer_.device_added_count());
  EXPECT_EQ(0, observer_.device_removed_count());
  EXPECT_EQ(1, observer_.device_changed_count());

  // Change a device address
  android_phone_state->address = "A1:B2:C3:D4:E5:E6";
  observer_.Reset();
  adapter_win_->DevicesPolled(devices);
  EXPECT_EQ(1, observer_.device_added_count());
  EXPECT_EQ(1, observer_.device_removed_count());
  EXPECT_EQ(0, observer_.device_changed_count());

  // Remove a device
  devices.erase(devices.begin());
  observer_.Reset();
  adapter_win_->DevicesPolled(devices);
  EXPECT_EQ(0, observer_.device_added_count());
  EXPECT_EQ(1, observer_.device_removed_count());
  EXPECT_EQ(0, observer_.device_changed_count());

  // Add a service
  laptop_state->service_record_states.push_back(
      std::make_unique<BluetoothTaskManagerWin::ServiceRecordState>());
  BluetoothTaskManagerWin::ServiceRecordState* audio_state =
      laptop_state->service_record_states.back().get();
  audio_state->name = kTestAudioSdpName;
  base::HexStringToBytes(kTestAudioSdpBytes, &audio_state->sdp_bytes);
  observer_.Reset();
  adapter_win_->DevicesPolled(devices);
  EXPECT_EQ(0, observer_.device_added_count());
  EXPECT_EQ(0, observer_.device_removed_count());
  EXPECT_EQ(1, observer_.device_changed_count());

  // Change a service
  audio_state->name = kTestAudioSdpName2;
  observer_.Reset();
  adapter_win_->DevicesPolled(devices);
  EXPECT_EQ(0, observer_.device_added_count());
  EXPECT_EQ(0, observer_.device_removed_count());
  EXPECT_EQ(1, observer_.device_changed_count());

  // Remove a service
  laptop_state->service_record_states.clear();
  observer_.Reset();
  adapter_win_->DevicesPolled(devices);
  EXPECT_EQ(0, observer_.device_added_count());
  EXPECT_EQ(0, observer_.device_removed_count());
  EXPECT_EQ(1, observer_.device_changed_count());
}

}  // namespace device
