// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/dbus/bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_agent_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_profile_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bluez {

class BluetoothAdapterBlueZTest : public testing::Test {
 public:
  BluetoothAdapterBlueZTest() = default;
  ~BluetoothAdapterBlueZTest() override = default;

  // testing::Test:
  void SetUp() override {
    std::unique_ptr<bluez::BluezDBusManagerSetter> dbus_setter =
        bluez::BluezDBusManager::GetSetterForTesting();
    dbus_setter->SetBluetoothAdapterClient(
        std::make_unique<bluez::FakeBluetoothAdapterClient>());
    dbus_setter->SetBluetoothAgentManagerClient(
        std::make_unique<bluez::FakeBluetoothAgentManagerClient>());
    dbus_setter->SetBluetoothDeviceClient(
        std::make_unique<bluez::FakeBluetoothDeviceClient>());
    dbus_setter->SetBluetoothProfileManagerClient(
        std::make_unique<bluez::FakeBluetoothProfileManagerClient>());

    adapter_ = BluetoothAdapterBlueZ::CreateAdapter();
    base::RunLoop run_loop;
    adapter_->Initialize(run_loop.QuitClosure());
    run_loop.Run();
    ASSERT_TRUE(adapter_);
    ASSERT_TRUE(adapter_->IsInitialized());
    ASSERT_TRUE(adapter_->IsPresent());
  }

  void SetAdapterDiscoverable(bool is_discoverable) {
    base::MockCallback<base::OnceClosure> error_callback;
    EXPECT_CALL(error_callback, Run()).Times(0);
    base::RunLoop run_loop;
    adapter_->SetDiscoverable(is_discoverable, run_loop.QuitClosure(),
                              error_callback.Get());
    run_loop.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  scoped_refptr<BluetoothAdapterBlueZ> adapter_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(BluetoothAdapterBlueZTest, UpdateName) {
  std::string default_name = adapter_->GetName();
  std::string test_name = "Test Name";

  base::MockCallback<base::OnceClosure> error_callback;
  EXPECT_CALL(error_callback, Run()).Times(0);
  base::RunLoop run_loop;
  adapter_->SetName(test_name, run_loop.QuitClosure(), error_callback.Get());
  run_loop.Run();
  EXPECT_EQ(test_name, adapter_->GetName());

  adapter_->SetStandardChromeOSAdapterName();
  EXPECT_EQ(default_name, adapter_->GetName());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(BluetoothAdapterBlueZTest, SetDiscoverable) {
  EXPECT_FALSE(adapter_->IsDiscoverable());

  SetAdapterDiscoverable(true);
  EXPECT_TRUE(adapter_->IsDiscoverable());

  SetAdapterDiscoverable(false);
  EXPECT_FALSE(adapter_->IsDiscoverable());
}

}  // namespace bluez
