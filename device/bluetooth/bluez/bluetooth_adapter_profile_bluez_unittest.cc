// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_adapter_profile_bluez.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/dbus/bluetooth_profile_service_provider.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_agent_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_profile_manager_client.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "testing/gtest/include/gtest/gtest.h"

using device::BluetoothAdapter;
using device::BluetoothUUID;

namespace bluez {

class BluetoothAdapterProfileBlueZTest : public testing::Test {
 public:
  BluetoothAdapterProfileBlueZTest()
      : success_callback_count_(0),
        error_callback_count_(0),
        fake_delegate_paired_(
            bluez::FakeBluetoothDeviceClient::kPairedDevicePath),
        fake_delegate_autopair_(
            bluez::FakeBluetoothDeviceClient::kLegacyAutopairPath),
        fake_delegate_listen_(""),
        profile_user_ptr_(nullptr) {}

  void SetUp() override {
    std::unique_ptr<bluez::BluezDBusManagerSetter> dbus_setter =
        bluez::BluezDBusManager::GetSetterForTesting();

    dbus_setter->SetBluetoothAdapterClient(
        std::unique_ptr<bluez::BluetoothAdapterClient>(
            new bluez::FakeBluetoothAdapterClient));
    dbus_setter->SetBluetoothAgentManagerClient(
        std::unique_ptr<bluez::BluetoothAgentManagerClient>(
            new bluez::FakeBluetoothAgentManagerClient));
    dbus_setter->SetBluetoothDeviceClient(
        std::unique_ptr<bluez::BluetoothDeviceClient>(
            new bluez::FakeBluetoothDeviceClient));
    dbus_setter->SetBluetoothProfileManagerClient(
        std::unique_ptr<bluez::BluetoothProfileManagerClient>(
            new bluez::FakeBluetoothProfileManagerClient));

    // Grab a pointer to the adapter.
    device::BluetoothAdapterFactory::GetAdapter(
        base::BindOnce(&BluetoothAdapterProfileBlueZTest::AdapterCallback,
                       base::Unretained(this)));
    base::RunLoop().Run();
    ASSERT_TRUE(adapter_.get() != nullptr);
    ASSERT_TRUE(adapter_->IsInitialized());
    ASSERT_TRUE(adapter_->IsPresent());

    // Turn on the adapter.
    adapter_->SetPowered(true, base::DoNothing(), base::DoNothing());
    ASSERT_TRUE(adapter_->IsPowered());
  }

  void TearDown() override {
    profile_.reset();
    adapter_ = nullptr;
    bluez::BluezDBusManager::Shutdown();
  }

  void AdapterCallback(scoped_refptr<BluetoothAdapter> adapter) {
    adapter_ = adapter;
    if (base::RunLoop::IsRunningOnCurrentThread())
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

  class FakeDelegate : public bluez::BluetoothProfileServiceProvider::Delegate {
   public:
    FakeDelegate(const std::string& device_path) : connections_(0) {
      device_path_ = dbus::ObjectPath(device_path);
    }

    // bluez::BluetoothProfileServiceProvider::Delegate:
    void Released() override {
      // noop
    }

    void NewConnection(
        const dbus::ObjectPath& device_path,
        base::ScopedFD fd,
        const bluez::BluetoothProfileServiceProvider::Delegate::Options&
            options,
        ConfirmationCallback callback) override {
      ++connections_;
      fd.reset();
      std::move(callback).Run(SUCCESS);
      if (device_path_.value() != "")
        ASSERT_EQ(device_path_, device_path);
    }

    void RequestDisconnection(const dbus::ObjectPath& device_path,
                              ConfirmationCallback callback) override {
      ++disconnections_;
    }

    void Cancel() override {
      // noop
    }

    unsigned int connections_;
    unsigned int disconnections_;
    dbus::ObjectPath device_path_;
  };

  void ProfileSuccessCallback(
      std::unique_ptr<BluetoothAdapterProfileBlueZ> profile) {
    profile_.swap(profile);
    ++success_callback_count_;
  }

  void ProfileUserSuccessCallback(BluetoothAdapterProfileBlueZ* profile) {
    profile_user_ptr_ = profile;
    ++success_callback_count_;
  }

  void MatchedProfileCallback(BluetoothAdapterProfileBlueZ* profile) {
    ASSERT_EQ(profile_user_ptr_, profile);
    ++success_callback_count_;
  }

  void DBusConnectSuccessCallback() { ++success_callback_count_; }

  void DBusErrorCallback(const std::string& error_name,
                         const std::string& error_message) {
    ++error_callback_count_;
  }

  void BasicErrorCallback(const std::string& error_message) {
    ++error_callback_count_;
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  scoped_refptr<BluetoothAdapter> adapter_;

  unsigned int success_callback_count_;
  unsigned int error_callback_count_;

  FakeDelegate fake_delegate_paired_;
  FakeDelegate fake_delegate_autopair_;
  FakeDelegate fake_delegate_listen_;

  std::unique_ptr<BluetoothAdapterProfileBlueZ> profile_;

  // unowned pointer as expected to be used by clients of
  // BluetoothAdapterBlueZ::UseProfile like BluetoothSocketBlueZ
  BluetoothAdapterProfileBlueZ* profile_user_ptr_;
};

TEST_F(BluetoothAdapterProfileBlueZTest, DelegateCount) {
  BluetoothUUID uuid(bluez::FakeBluetoothProfileManagerClient::kRfcommUuid);
  bluez::BluetoothProfileManagerClient::Options options;

  options.require_authentication.reset(new bool(false));

  BluetoothAdapterProfileBlueZ::Register(
      uuid, options,
      base::Bind(&BluetoothAdapterProfileBlueZTest::ProfileSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterProfileBlueZTest::DBusErrorCallback,
                 base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(profile_);
  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);

  EXPECT_EQ(0U, profile_->DelegateCount());

  profile_->SetDelegate(fake_delegate_paired_.device_path_,
                        &fake_delegate_paired_);

  EXPECT_EQ(1U, profile_->DelegateCount());

  profile_->RemoveDelegate(fake_delegate_autopair_.device_path_,
                           base::DoNothing());

  EXPECT_EQ(1U, profile_->DelegateCount());

  profile_->RemoveDelegate(fake_delegate_paired_.device_path_,
                           base::DoNothing());

  EXPECT_EQ(0U, profile_->DelegateCount());
}

TEST_F(BluetoothAdapterProfileBlueZTest, BlackHole) {
  BluetoothUUID uuid(bluez::FakeBluetoothProfileManagerClient::kRfcommUuid);
  bluez::BluetoothProfileManagerClient::Options options;

  options.require_authentication.reset(new bool(false));

  BluetoothAdapterProfileBlueZ::Register(
      uuid, options,
      base::Bind(&BluetoothAdapterProfileBlueZTest::ProfileSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterProfileBlueZTest::DBusErrorCallback,
                 base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(profile_);
  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);

  bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->ConnectProfile(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kPairedDevicePath),
      bluez::FakeBluetoothProfileManagerClient::kRfcommUuid,
      base::Bind(&BluetoothAdapterProfileBlueZTest::DBusConnectSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterProfileBlueZTest::DBusErrorCallback,
                 base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(1U, error_callback_count_);

  EXPECT_EQ(0U, fake_delegate_paired_.connections_);
}

TEST_F(BluetoothAdapterProfileBlueZTest, Routing) {
  BluetoothUUID uuid(bluez::FakeBluetoothProfileManagerClient::kRfcommUuid);
  bluez::BluetoothProfileManagerClient::Options options;

  options.require_authentication.reset(new bool(false));

  BluetoothAdapterProfileBlueZ::Register(
      uuid, options,
      base::Bind(&BluetoothAdapterProfileBlueZTest::ProfileSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterProfileBlueZTest::DBusErrorCallback,
                 base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(profile_);
  ASSERT_EQ(1U, success_callback_count_);
  ASSERT_EQ(0U, error_callback_count_);

  profile_->SetDelegate(fake_delegate_paired_.device_path_,
                        &fake_delegate_paired_);
  profile_->SetDelegate(fake_delegate_autopair_.device_path_,
                        &fake_delegate_autopair_);
  profile_->SetDelegate(fake_delegate_listen_.device_path_,
                        &fake_delegate_listen_);

  bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->ConnectProfile(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kPairedDevicePath),
      bluez::FakeBluetoothProfileManagerClient::kRfcommUuid,
      base::Bind(&BluetoothAdapterProfileBlueZTest::DBusConnectSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterProfileBlueZTest::DBusErrorCallback,
                 base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);

  EXPECT_EQ(1U, fake_delegate_paired_.connections_);

  bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->ConnectProfile(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLegacyAutopairPath),
      bluez::FakeBluetoothProfileManagerClient::kRfcommUuid,
      base::Bind(&BluetoothAdapterProfileBlueZTest::DBusConnectSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterProfileBlueZTest::DBusErrorCallback,
                 base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(3U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);

  EXPECT_EQ(1U, fake_delegate_autopair_.connections_);

  // Incoming connections look the same from BlueZ.
  bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->ConnectProfile(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kDisplayPinCodePath),
      bluez::FakeBluetoothProfileManagerClient::kRfcommUuid,
      base::Bind(&BluetoothAdapterProfileBlueZTest::DBusConnectSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterProfileBlueZTest::DBusErrorCallback,
                 base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(4U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);

  EXPECT_EQ(1U, fake_delegate_listen_.connections_);
}

TEST_F(BluetoothAdapterProfileBlueZTest, SimultaneousRegister) {
  BluetoothUUID uuid(bluez::FakeBluetoothProfileManagerClient::kRfcommUuid);
  bluez::BluetoothProfileManagerClient::Options options;
  BluetoothAdapterBlueZ* adapter =
      static_cast<BluetoothAdapterBlueZ*>(adapter_.get());

  options.require_authentication.reset(new bool(false));

  success_callback_count_ = 0;
  error_callback_count_ = 0;

  adapter->UseProfile(
      uuid, fake_delegate_paired_.device_path_, options, &fake_delegate_paired_,
      base::Bind(&BluetoothAdapterProfileBlueZTest::ProfileUserSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterProfileBlueZTest::BasicErrorCallback,
                 base::Unretained(this)));

  adapter->UseProfile(
      uuid, fake_delegate_autopair_.device_path_, options,
      &fake_delegate_autopair_,
      base::Bind(&BluetoothAdapterProfileBlueZTest::MatchedProfileCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterProfileBlueZTest::BasicErrorCallback,
                 base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(profile_user_ptr_);
  EXPECT_EQ(2U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);

  adapter->ReleaseProfile(fake_delegate_paired_.device_path_,
                          profile_user_ptr_);
  adapter->ReleaseProfile(fake_delegate_autopair_.device_path_,
                          profile_user_ptr_);

  base::RunLoop().RunUntilIdle();
}

TEST_F(BluetoothAdapterProfileBlueZTest, SimultaneousRegisterFail) {
  BluetoothUUID uuid(
      bluez::FakeBluetoothProfileManagerClient::kUnregisterableUuid);
  bluez::BluetoothProfileManagerClient::Options options;
  BluetoothAdapterBlueZ* adapter =
      static_cast<BluetoothAdapterBlueZ*>(adapter_.get());

  options.require_authentication.reset(new bool(false));

  success_callback_count_ = 0;
  error_callback_count_ = 0;

  adapter->UseProfile(
      uuid, fake_delegate_paired_.device_path_, options, &fake_delegate_paired_,
      base::Bind(&BluetoothAdapterProfileBlueZTest::ProfileUserSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterProfileBlueZTest::BasicErrorCallback,
                 base::Unretained(this)));

  adapter->UseProfile(
      uuid, fake_delegate_autopair_.device_path_, options,
      &fake_delegate_autopair_,
      base::Bind(&BluetoothAdapterProfileBlueZTest::MatchedProfileCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterProfileBlueZTest::BasicErrorCallback,
                 base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(profile_user_ptr_);
  EXPECT_EQ(0U, success_callback_count_);
  EXPECT_EQ(2U, error_callback_count_);
}

}  // namespace bluez
