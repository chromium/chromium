// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/floss/bluetooth_adapter_floss.h"
#include "device/bluetooth/floss/fake_floss_adapter_client.h"
#include "device/bluetooth/floss/fake_floss_manager_client.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/test/mock_pairing_delegate.h"
#include "device/bluetooth/test/test_bluetooth_adapter_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::device::BluetoothAdapter;
using ::device::BluetoothDevice;
using ::device::BluetoothDiscoverySession;
using ::device::MockPairingDelegate;
using ::device::TestBluetoothAdapterObserver;
using ::testing::_;
using ::testing::StrictMock;

}  // namespace

namespace floss {

// Unit tests exercising device/bluetooth/floss, with abstract Floss API
// implemented as a fake Floss*Client.
class BluetoothFlossTest : public testing::Test {
 public:
  void SetUp() override {
    std::unique_ptr<floss::FlossDBusManagerSetter> dbus_setter =
        floss::FlossDBusManager::GetSetterForTesting();

    auto fake_floss_manager_client = std::make_unique<FakeFlossManagerClient>();
    auto fake_floss_adapter_client = std::make_unique<FakeFlossAdapterClient>();

    fake_floss_manager_client_ = fake_floss_manager_client.get();
    fake_floss_adapter_client_ = fake_floss_adapter_client.get();

    dbus_setter->SetFlossManagerClient(std::move(fake_floss_manager_client));
    dbus_setter->SetFlossAdapterClient(std::move(fake_floss_adapter_client));
  }

  void InitializeAdapter() {
    adapter_ = BluetoothAdapterFloss::CreateAdapter();

    fake_floss_manager_client_->SetAdapterPowered(/*adapter=*/0,
                                                  /*powered=*/true);

    base::RunLoop run_loop;
    adapter_->Initialize(run_loop.QuitClosure());
    run_loop.Run();

    ASSERT_TRUE(adapter_);
    ASSERT_TRUE(adapter_->IsInitialized());
  }

  // Triggers fake/simulated device discovery by FakeFlossAdapterClient.
  void DiscoverDevices() {
    ASSERT_TRUE(adapter_.get() != nullptr);

    adapter_->StartDiscoverySession(
        /*client_name=*/std::string(),
        base::BindOnce(&BluetoothFlossTest::DiscoverySessionCallback,
                       base::Unretained(this)),
        GetErrorCallback());

    base::RunLoop().Run();
  }

 protected:
  void ErrorCallback() { QuitMessageLoop(); }

  base::OnceClosure GetErrorCallback() {
    return base::BindOnce(&BluetoothFlossTest::ErrorCallback,
                          base::Unretained(this));
  }

  void DiscoverySessionCallback(
      std::unique_ptr<BluetoothDiscoverySession> discovery_session) {
    discovery_sessions_.push_back(std::move(discovery_session));
    QuitMessageLoop();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<BluetoothAdapter> adapter_;

  // Holds pointer to FakeFloss*Client's so that we can manipulate the fake
  // within tests.
  FakeFlossManagerClient* fake_floss_manager_client_;
  FakeFlossAdapterClient* fake_floss_adapter_client_;

  std::vector<std::unique_ptr<BluetoothDiscoverySession>> discovery_sessions_;

 private:
  // Some tests use a message loop since background processing is simulated;
  // break out of those loops.
  void QuitMessageLoop() {
    if (base::RunLoop::IsRunningOnCurrentThread())
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }
};

TEST_F(BluetoothFlossTest, PairJustWorks) {
  InitializeAdapter();
  DiscoverDevices();

  BluetoothDevice* device =
      adapter_->GetDevice(FakeFlossAdapterClient::kJustWorksAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  StrictMock<MockPairingDelegate> pairing_delegate;
  base::RunLoop run_loop;
  device->Connect(
      &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](absl::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(device->IsPaired());
}

TEST_F(BluetoothFlossTest, PairConfirmPasskey) {
  InitializeAdapter();
  DiscoverDevices();

  BluetoothDevice* device =
      adapter_->GetDevice(FakeFlossAdapterClient::kPhoneAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate,
              ConfirmPasskey(_, FakeFlossAdapterClient::kPasskey))
      .WillOnce([](BluetoothDevice* device, uint32_t passkey) {
        device->ConfirmPairing();
      });
  base::RunLoop run_loop;
  device->Connect(
      &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](absl::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(device->IsPaired());
}

TEST_F(BluetoothFlossTest, PairDisplayPasskey) {
  InitializeAdapter();
  DiscoverDevices();

  BluetoothDevice* device =
      adapter_->GetDevice(FakeFlossAdapterClient::kKeyboardAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate,
              DisplayPasskey(_, FakeFlossAdapterClient::kPasskey))
      .WillOnce([this](BluetoothDevice* device, uint32_t passkey) {
        // Pretend that the remote device has completed passkey entry.
        fake_floss_adapter_client_->NotifyObservers(base::BindLambdaForTesting(
            [device](FlossAdapterClient::Observer* observer) {
              observer->DeviceBondStateChanged(
                  FlossDeviceId({.address = device->GetAddress(), .name = ""}),
                  /*status=*/0, FlossAdapterClient::BondState::kBonded);
            }));
      });
  base::RunLoop run_loop;
  device->Connect(
      &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](absl::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(device->IsPaired());
}

TEST_F(BluetoothFlossTest, PairPasskeyEntry) {
  InitializeAdapter();
  DiscoverDevices();

  BluetoothDevice* device =
      adapter_->GetDevice(FakeFlossAdapterClient::kOldDeviceAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate, RequestPinCode(_))
      .WillOnce([](BluetoothDevice* device) { device->SetPinCode("pin123"); });
  base::RunLoop run_loop;
  device->Connect(
      &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](absl::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(device->IsPaired());
}

TEST_F(BluetoothFlossTest, RemoveBonding) {
  InitializeAdapter();
  DiscoverDevices();

  BluetoothDevice* device =
      adapter_->GetDevice(FakeFlossAdapterClient::kJustWorksAddress);
  ASSERT_TRUE(device);
  ASSERT_FALSE(device->IsPaired());

  StrictMock<MockPairingDelegate> pairing_delegate;
  base::RunLoop run_loop;
  device->Connect(
      &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](absl::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(device->IsPaired());

  base::RunLoop run_loop2;
  device->Forget(base::BindLambdaForTesting([&run_loop2]() {
                   SUCCEED();
                   run_loop2.Quit();
                 }),
                 base::BindLambdaForTesting([]() { FAIL(); }));
  run_loop2.Run();

  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothFlossTest, UpdatesDeviceConnectionState) {
  InitializeAdapter();
  DiscoverDevices();

  BluetoothDevice* device =
      adapter_->GetDevice(FakeFlossAdapterClient::kJustWorksAddress);
  ASSERT_TRUE(device != nullptr);
  EXPECT_FALSE(device->IsConnected());

  fake_floss_adapter_client_->NotifyObservers(
      base::BindRepeating([](FlossAdapterClient::Observer* observer) {
        observer->AdapterDeviceConnected(FlossDeviceId{
            .address = FakeFlossAdapterClient::kJustWorksAddress, .name = ""});
      }));
  EXPECT_TRUE(device->IsConnected());

  fake_floss_adapter_client_->NotifyObservers(
      base::BindRepeating([](FlossAdapterClient::Observer* observer) {
        observer->AdapterDeviceDisconnected(FlossDeviceId{
            .address = FakeFlossAdapterClient::kJustWorksAddress, .name = ""});
      }));
  EXPECT_FALSE(device->IsConnected());
}

TEST_F(BluetoothFlossTest, AdapterInitialDevices) {
  InitializeAdapter();

  // Before adapter is enabled, there are no known devices.
  EXPECT_FALSE(adapter_->GetDevice(FakeFlossAdapterClient::kBondedAddress1));
  EXPECT_FALSE(adapter_->GetDevice(FakeFlossAdapterClient::kBondedAddress2));

  // Simulate adapter enabled event.
  fake_floss_manager_client_->NotifyObservers(
      base::BindLambdaForTesting([](FlossManagerClient::Observer* observer) {
        observer->AdapterEnabledChanged(/*hci=*/0, /*enabled=*/true);
      }));
  base::RunLoop().RunUntilIdle();

  // After adapter is enabled, there are known devices.
  BluetoothDevice* device1 =
      adapter_->GetDevice(FakeFlossAdapterClient::kBondedAddress1);
  BluetoothDevice* device2 =
      adapter_->GetDevice(FakeFlossAdapterClient::kBondedAddress2);
  ASSERT_TRUE(device1);
  ASSERT_TRUE(device2);
  EXPECT_TRUE(device1->IsPaired());
  EXPECT_TRUE(device2->IsPaired());
  EXPECT_TRUE(device1->IsConnected());
  EXPECT_FALSE(device2->IsConnected());
}

}  // namespace floss
