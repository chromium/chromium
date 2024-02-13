// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/floss/bluetooth_adapter_floss.h"
#include "device/bluetooth/floss/bluetooth_advertisement_floss.h"
#include "device/bluetooth/floss/bluetooth_device_floss.h"
#include "device/bluetooth/floss/fake_floss_adapter_client.h"
#include "device/bluetooth/floss/fake_floss_advertiser_client.h"
#include "device/bluetooth/floss/fake_floss_lescan_client.h"
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

const uint8_t kTestScannerId = 10;
#if BUILDFLAG(IS_CHROMEOS)
const uint8_t kTestScannerId2 = 11;
#endif
constexpr char kTestDeviceAddr[] = "11:22:33:44:55:66";
constexpr char kTestDeviceName[] = "FlossDevice";

}  // namespace

namespace floss {

class FakeBluetoothLowEnergyScanSessionDelegate
    : public device::BluetoothLowEnergyScanSession::Delegate {
 public:
  FakeBluetoothLowEnergyScanSessionDelegate() = default;

  void OnSessionStarted(
      device::BluetoothLowEnergyScanSession* scan_session,
      std::optional<device::BluetoothLowEnergyScanSession::ErrorCode>
          error_code) override {
    sessions_started_++;
  }
  void OnDeviceFound(device::BluetoothLowEnergyScanSession* scan_session,
                     device::BluetoothDevice* device) override {
    devices_found_.push_back(device->GetAddress());
  }
  void OnDeviceLost(device::BluetoothLowEnergyScanSession* scan_session,
                    device::BluetoothDevice* device) override {
    devices_lost_.push_back(device->GetAddress());
  }
  void OnSessionInvalidated(
      device::BluetoothLowEnergyScanSession* scan_session) override {
    sessions_invalidated_++;
  }

  base::WeakPtr<FakeBluetoothLowEnergyScanSessionDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  int sessions_started_ = 0;
  std::vector<std::string> devices_found_;
  std::vector<std::string> devices_lost_;
  int sessions_invalidated_ = 0;

 private:
  base::WeakPtrFactory<FakeBluetoothLowEnergyScanSessionDelegate>
      weak_ptr_factory_{this};
};

// Unit tests exercising device/bluetooth/floss, with abstract Floss API
// implemented as a fake Floss*Client.
class BluetoothFlossTest : public testing::Test {
 public:
  void SetUp() override {
    // TODO(b/266989920): GetSetterForTesting method used as a shortcut to
    // initiate fake DBUS instances and fake clients. Replace this call with a
    // more proper init after Floss fake implement is completed.
    floss::FlossDBusManager::GetSetterForTesting();
  }

  FakeFlossManagerClient* GetFakeManagerClient() {
    return static_cast<FakeFlossManagerClient*>(
        FlossDBusManager::Get()->GetManagerClient());
  }

  FakeFlossAdapterClient* GetFakeAdapterClient() {
    return static_cast<FakeFlossAdapterClient*>(
        floss::FlossDBusManager::Get()->GetAdapterClient());
  }

  FakeFlossAdvertiserClient* GetFakeAdvertiserClient() {
    return static_cast<FakeFlossAdvertiserClient*>(
        FlossDBusManager::Get()->GetAdvertiserClient());
  }

  FakeFlossLEScanClient* GetFakeLEScanClient() {
    return static_cast<FakeFlossLEScanClient*>(
        FlossDBusManager::Get()->GetLEScanClient());
  }

  void InitializeAdapter() {
    adapter_ = BluetoothAdapterFloss::CreateAdapter();

    GetFakeManagerClient()->SetDefaultEnabled(false);

    base::RunLoop run_loop;
    adapter_->Initialize(run_loop.QuitClosure());
    run_loop.Run();

    ASSERT_TRUE(adapter_);
    ASSERT_TRUE(adapter_->IsInitialized());
  }

  // Triggers fake/simulated device discovery by FakeFlossAdapterClient.
  void DiscoverDevices() {
    ASSERT_TRUE(adapter_.get() != nullptr);
    base::RunLoop loop;
    adapter_->StartDiscoverySession(
        /*client_name=*/std::string(),
        base::BindOnce(&BluetoothFlossTest::DiscoverySessionCallback,
                       base::Unretained(this), loop.QuitClosure()),
        GetErrorCallback(loop.QuitClosure()));

    loop.Run();
  }

  // Simulate adapter enabled event. After adapter is enabled, there are known
  // devices.
  void EnableAdapter() {
    ASSERT_TRUE(adapter_.get() != nullptr);

    GetFakeManagerClient()->SetDefaultEnabled(true);
    GetFakeManagerClient()->NotifyObservers(
        base::BindLambdaForTesting([](FlossManagerClient::Observer* observer) {
          observer->AdapterEnabledChanged(/*adapter=*/0, /*enabled=*/true);
        }));
    GetFakeAdapterClient()->SetConnected(
        FakeFlossAdapterClient::kBondedAddress1, true);
    GetFakeAdapterClient()->SetConnected(
        FakeFlossAdapterClient::kPairedAddressBrEdr, true);
    GetFakeAdapterClient()->SetConnected(
        FakeFlossAdapterClient::kPairedAddressLE, true);
    base::RunLoop().RunUntilIdle();
  }

  void InitializeAndEnableAdapter() {
    InitializeAdapter();
    EnableAdapter();
  }

  // Simulates getting a ScannerRegistered callback.
  void RegisterScanner(const device::BluetoothUUID& uuid, uint8_t scanner_id) {
    ASSERT_TRUE(adapter_.get() != nullptr);
    BluetoothAdapterFloss* floss_adapter =
        static_cast<BluetoothAdapterFloss*>(adapter_.get());

    floss_adapter->ScannerRegistered(uuid, scanner_id, GattStatus::kSuccess);

    base::RunLoop().RunUntilIdle();
  }

  // Simulates getting OnScanResult.
  void GetScanResult() {
    ASSERT_TRUE(adapter_.get() != nullptr);
    BluetoothAdapterFloss* floss_adapter =
        static_cast<BluetoothAdapterFloss*>(adapter_.get());

    ScanResult scan_result;
    scan_result.address = kTestDeviceAddr;
    scan_result.name = kTestDeviceName;
    floss_adapter->ScanResultReceived(scan_result);
  }

  // Simulates getting OnAdvertisementFound.
  void GetAdvFound() {
    ASSERT_TRUE(adapter_.get() != nullptr);
    BluetoothAdapterFloss* floss_adapter =
        static_cast<BluetoothAdapterFloss*>(adapter_.get());

    ScanResult scan_result;
    scan_result.address = kTestDeviceAddr;
    scan_result.name = kTestDeviceName;
    floss_adapter->AdvertisementFound(kTestScannerId, scan_result);
  }

 protected:
  void ErrorCallback(base::OnceClosure quit_closure) {
    std::move(quit_closure).Run();
  }

  base::OnceClosure GetErrorCallback(base::OnceClosure quit_closure) {
    return base::BindOnce(&BluetoothFlossTest::ErrorCallback,
                          base::Unretained(this), std::move(quit_closure));
  }

  void DiscoverySessionCallback(
      base::OnceClosure quit_closure,
      std::unique_ptr<BluetoothDiscoverySession> discovery_session) {
    discovery_sessions_.push_back(std::move(discovery_session));
    std::move(quit_closure).Run();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<BluetoothAdapter> adapter_;

  std::vector<std::unique_ptr<BluetoothDiscoverySession>> discovery_sessions_;
};

TEST_F(BluetoothFlossTest, BondFailureTriggersCallbacks) {
  InitializeAndEnableAdapter();
  DiscoverDevices();

  GetFakeAdapterClient()->FailNextBonding();
  BluetoothDevice* device =
      adapter_->GetDevice(FakeFlossAdapterClient::kJustWorksAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());
  ASSERT_FALSE(device->IsConnecting());

  StrictMock<MockPairingDelegate> pairing_delegate;
  base::RunLoop run_loop;
  device->Connect(
      &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_TRUE(error.has_value());
            run_loop.Quit();
          }));
  EXPECT_FALSE(device->IsPaired());
  EXPECT_FALSE(device->IsConnected());
  base::RunLoop().RunUntilIdle();
}

TEST_F(BluetoothFlossTest, PairJustWorks) {
  InitializeAndEnableAdapter();
  DiscoverDevices();

  BluetoothDevice* device =
      adapter_->GetDevice(FakeFlossAdapterClient::kJustWorksAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());
  ASSERT_FALSE(device->IsConnecting());

  StrictMock<MockPairingDelegate> pairing_delegate;
  base::RunLoop run_loop;
  device->Connect(
      &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  EXPECT_TRUE(device->IsPaired());
  EXPECT_TRUE(device->IsConnected());
}

TEST_F(BluetoothFlossTest, PairingTwiceRejectsSecondRequest) {
  InitializeAndEnableAdapter();
  DiscoverDevices();

  BluetoothDevice* device =
      adapter_->GetDevice(FakeFlossAdapterClient::kJustWorksAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());
  ASSERT_FALSE(device->IsConnecting());

  StrictMock<MockPairingDelegate> pairing_delegate;
  base::RunLoop run_loop;
  device->Connect(
      &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  device->Connect(
      &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_TRUE(error.has_value());
            run_loop.Quit();
          }));
  EXPECT_TRUE(device->IsPaired());
  EXPECT_TRUE(device->IsConnected());
}

TEST_F(BluetoothFlossTest, PairConfirmPasskey) {
  InitializeAndEnableAdapter();
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
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  static_cast<BluetoothDeviceFloss*>(device)->SetIsConnected(true);
  run_loop.Run();

  EXPECT_TRUE(device->IsPaired());
}

TEST_F(BluetoothFlossTest, PairDisplayPasskeySucceeded) {
  InitializeAndEnableAdapter();
  DiscoverDevices();

  BluetoothDevice* device =
      adapter_->GetDevice(FakeFlossAdapterClient::kPasskeyDisplayAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate,
              DisplayPasskey(_, FakeFlossAdapterClient::kPasskey))
      .WillOnce([this](BluetoothDevice* device, uint32_t passkey) {
        // Pretend that the remote device has completed passkey entry.
        GetFakeAdapterClient()->NotifyObservers(base::BindLambdaForTesting(
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
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  static_cast<BluetoothDeviceFloss*>(device)->SetIsConnected(true);
  run_loop.Run();

  EXPECT_TRUE(device->IsPaired());
}

TEST_F(BluetoothFlossTest, PairDisplayPasskeyFailed) {
  InitializeAndEnableAdapter();
  DiscoverDevices();

  BluetoothDevice* device =
      adapter_->GetDevice(FakeFlossAdapterClient::kPasskeyDisplayAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate,
              DisplayPasskey(_, FakeFlossAdapterClient::kPasskey))
      .WillOnce([this](BluetoothDevice* device, uint32_t passkey) {
        // Pretend that the remote device has entered wrong passkey.
        GetFakeAdapterClient()->NotifyObservers(base::BindLambdaForTesting(
            [device](FlossAdapterClient::Observer* observer) {
              observer->DeviceBondStateChanged(
                  FlossDeviceId({.address = device->GetAddress(), .name = ""}),
                  static_cast<uint32_t>(
                      FlossAdapterClient::BtifStatus::kAuthFailure),
                  FlossAdapterClient::BondState::kNotBonded);
            }));
      });
  base::RunLoop run_loop;
  device->Connect(
      &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_TRUE(error.has_value());
            run_loop.Quit();
          }));
  static_cast<BluetoothDeviceFloss*>(device)->SetIsConnected(true);
  run_loop.Run();

  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothFlossTest, PairPasskeyEntry) {
  InitializeAndEnableAdapter();
  DiscoverDevices();

  BluetoothDevice* device =
      adapter_->GetDevice(FakeFlossAdapterClient::kPasskeyRequestAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate, RequestPinCode(_))
      .WillOnce([](BluetoothDevice* device) { device->SetPinCode("pin123"); });
  base::RunLoop run_loop;
  device->Connect(
      &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  static_cast<BluetoothDeviceFloss*>(device)->SetIsConnected(true);
  run_loop.Run();

  EXPECT_TRUE(device->IsPaired());
}

TEST_F(BluetoothFlossTest, RemoveBonding) {
  InitializeAndEnableAdapter();
  DiscoverDevices();

  // Simulate adapter enabled event.
  EnableAdapter();

  BluetoothDevice* device =
      adapter_->GetDevice(FakeFlossAdapterClient::kJustWorksAddress);
  ASSERT_TRUE(device);
  ASSERT_FALSE(device->IsPaired());

  StrictMock<MockPairingDelegate> pairing_delegate;
  base::RunLoop run_loop;
  device->Connect(
      &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  static_cast<BluetoothDeviceFloss*>(device)->SetIsConnected(true);
  run_loop.Run();

  EXPECT_TRUE(device->IsPaired());
  ASSERT_TRUE(device->IsConnected());

  // Simulate device disconnecting
  static_cast<BluetoothDeviceFloss*>(device)->SetIsConnected(false);

  base::RunLoop run_loop2;
  device->Forget(base::BindLambdaForTesting([&run_loop2]() {
                   SUCCEED();
                   run_loop2.Quit();
                 }),
                 base::BindLambdaForTesting([]() { FAIL(); }));
  run_loop2.Run();

  device = adapter_->GetDevice(FakeFlossAdapterClient::kJustWorksAddress);
  EXPECT_FALSE(device);

  // Now check with bonded and connected device.
  BluetoothDevice* paired_device =
      adapter_->GetDevice(FakeFlossAdapterClient::kBondedAddress1);

  ASSERT_TRUE(paired_device);
  ASSERT_TRUE(paired_device->IsPaired());
  ASSERT_TRUE(paired_device->IsConnected());

  {
    base::RunLoop loop;
    paired_device->Forget(base::BindLambdaForTesting([&loop]() {
                            SUCCEED();
                            loop.Quit();
                          }),
                          base::BindLambdaForTesting([]() { FAIL(); }));
    loop.Run();
  }

  paired_device = adapter_->GetDevice(FakeFlossAdapterClient::kBondedAddress1);
  ASSERT_TRUE(paired_device);
}

TEST_F(BluetoothFlossTest, PairDisplayPinCodeSucceeded) {
  InitializeAndEnableAdapter();
  DiscoverDevices();

  BluetoothDevice* device =
      adapter_->GetDevice(FakeFlossAdapterClient::kPinCodeDisplayAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate,
              DisplayPinCode(_, FakeFlossAdapterClient::kPinCode))
      .WillOnce([this](BluetoothDevice* device, std::string pincode) {
        // Pretend that the remote device has completed pin code entry.
        GetFakeAdapterClient()->NotifyObservers(base::BindLambdaForTesting(
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
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  static_cast<BluetoothDeviceFloss*>(device)->SetIsConnected(true);
  run_loop.Run();

  EXPECT_TRUE(device->IsPaired());
}

TEST_F(BluetoothFlossTest, PairRequestPinCodeSucceeded) {
  InitializeAndEnableAdapter();
  DiscoverDevices();

  BluetoothDevice* device =
      adapter_->GetDevice(FakeFlossAdapterClient::kPinCodeRequestAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate, RequestPinCode(_))
      .WillOnce([this](BluetoothDevice* device) {
        // Pretend that the remote device has completed pin code entry.
        GetFakeAdapterClient()->NotifyObservers(base::BindLambdaForTesting(
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
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  static_cast<BluetoothDeviceFloss*>(device)->SetIsConnected(true);
  run_loop.Run();

  EXPECT_TRUE(device->IsPaired());
}

TEST_F(BluetoothFlossTest, Disconnect) {
  InitializeAndEnableAdapter();
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
          [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  static_cast<BluetoothDeviceFloss*>(device)->SetIsConnected(true);
  run_loop.Run();

  EXPECT_TRUE(device->IsPaired());

  base::RunLoop run_loop2;
  device->Disconnect(base::BindLambdaForTesting([&run_loop2]() {
                       SUCCEED();
                       run_loop2.Quit();
                     }),
                     base::BindLambdaForTesting([]() { FAIL(); }));
  run_loop2.Run();
}

TEST_F(BluetoothFlossTest, UpdatesDeviceConnectionState) {
  InitializeAndEnableAdapter();
  DiscoverDevices();

  BluetoothDevice* device =
      adapter_->GetDevice(FakeFlossAdapterClient::kJustWorksAddress);
  ASSERT_TRUE(device != nullptr);
  EXPECT_FALSE(device->IsConnected());

  GetFakeAdapterClient()->NotifyObservers(
      base::BindRepeating([](FlossAdapterClient::Observer* observer) {
        observer->AdapterDeviceConnected(FlossDeviceId{
            .address = FakeFlossAdapterClient::kJustWorksAddress, .name = ""});
      }));
  EXPECT_TRUE(device->IsConnected());

  GetFakeAdapterClient()->NotifyObservers(
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
  EnableAdapter();

  // After adapter is enabled, there are known devices.
  BluetoothDeviceFloss* device1 = static_cast<BluetoothDeviceFloss*>(
      adapter_->GetDevice(FakeFlossAdapterClient::kBondedAddress1));
  BluetoothDeviceFloss* device2 = static_cast<BluetoothDeviceFloss*>(
      adapter_->GetDevice(FakeFlossAdapterClient::kBondedAddress2));
  ASSERT_TRUE(device1);
  ASSERT_TRUE(device2);
  EXPECT_TRUE(device1->IsPaired());
  EXPECT_TRUE(device1->IsBondedImpl());
  EXPECT_TRUE(device2->IsPaired());
  EXPECT_TRUE(device2->IsBondedImpl());
  EXPECT_TRUE(device1->IsConnected());
  EXPECT_FALSE(device2->IsConnected());
  EXPECT_EQ(device1->GetBluetoothClass(),
            FakeFlossAdapterClient::kDefaultClassOfDevice);
  EXPECT_EQ(device2->GetBluetoothClass(),
            FakeFlossAdapterClient::kDefaultClassOfDevice);
  EXPECT_EQ(device1->GetType(),
            device::BluetoothTransport::BLUETOOTH_TRANSPORT_LE);
  EXPECT_EQ(device2->GetType(),
            device::BluetoothTransport::BLUETOOTH_TRANSPORT_LE);

  // We should also have paired + connected devices that aren't bonded.
  BluetoothDeviceFloss* paired1 = static_cast<BluetoothDeviceFloss*>(
      adapter_->GetDevice(FakeFlossAdapterClient::kPairedAddressBrEdr));
  BluetoothDeviceFloss* paired2 = static_cast<BluetoothDeviceFloss*>(
      adapter_->GetDevice(FakeFlossAdapterClient::kPairedAddressLE));
  ASSERT_TRUE(paired1);
  ASSERT_TRUE(paired2);

  // Should be paired and connected but not bonded.
  EXPECT_TRUE(paired1->IsPaired());
  EXPECT_TRUE(paired1->IsConnected());
  EXPECT_TRUE(paired2->IsPaired());
  EXPECT_TRUE(paired2->IsConnected());

  EXPECT_FALSE(paired1->IsBondedImpl());
  EXPECT_FALSE(paired2->IsBondedImpl());
}

TEST_F(BluetoothFlossTest, TestIsConnectable) {
  InitializeAndEnableAdapter();
  DiscoverDevices();

  BluetoothDevice* device =
      adapter_->GetDevice(FakeFlossAdapterClient::kPhoneAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_TRUE(device->IsConnectable());

  // HID devices shouldn't be connectable
  device = adapter_->GetDevice(FakeFlossAdapterClient::kPasskeyDisplayAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsConnectable());
}

TEST_F(BluetoothFlossTest, DisabledAdapterClearsDevices) {
  InitializeAndEnableAdapter();
  DiscoverDevices();

  EXPECT_TRUE(adapter_->GetDevices().size() > 0);
  // Simulate adapter enabled event.
  GetFakeManagerClient()->NotifyObservers(
      base::BindLambdaForTesting([](FlossManagerClient::Observer* observer) {
        observer->AdapterEnabledChanged(/*adapter=*/0, /*enabled=*/false);
      }));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(adapter_->GetDevices().empty());
}

TEST_F(BluetoothFlossTest, RepeatsDiscoverySession) {
  InitializeAndEnableAdapter();
  DiscoverDevices();

  EXPECT_TRUE(adapter_->IsDiscovering());

  // Simulate discovery state changed to False.
  GetFakeAdapterClient()->NotifyObservers(
      base::BindLambdaForTesting([](FlossAdapterClient::Observer* observer) {
        observer->AdapterDiscoveringChanged(false);
      }));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(adapter_->IsDiscovering());

  // Force discovery to fail after discovering is stopped.
  GetFakeAdapterClient()->FailNextDiscovery();
  GetFakeAdapterClient()->NotifyObservers(
      base::BindLambdaForTesting([](FlossAdapterClient::Observer* observer) {
        observer->AdapterDiscoveringChanged(false);
      }));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothFlossTest, HandlesClearedDevices) {
  InitializeAndEnableAdapter();
  DiscoverDevices();

  BluetoothDevice* device =
      adapter_->GetDevice(FakeFlossAdapterClient::kJustWorksAddress);
  EXPECT_TRUE(device != nullptr);

  // Simulate clearing away a device.
  GetFakeAdapterClient()->NotifyObservers(
      base::BindLambdaForTesting([](FlossAdapterClient::Observer* observer) {
        FlossDeviceId id{.address = FakeFlossAdapterClient::kJustWorksAddress,
                         .name = ""};
        observer->AdapterClearedDevice(id);
      }));

  base::RunLoop().RunUntilIdle();
  BluetoothDevice* same_device =
      adapter_->GetDevice(FakeFlossAdapterClient::kJustWorksAddress);
  EXPECT_TRUE(same_device == nullptr);

  // Simulate clearing away a bonded device.
  BluetoothDevice* bonded_device =
      adapter_->GetDevice(FakeFlossAdapterClient::kBondedAddress1);
  EXPECT_TRUE(bonded_device != nullptr);
  GetFakeAdapterClient()->NotifyObservers(
      base::BindLambdaForTesting([](FlossAdapterClient::Observer* observer) {
        FlossDeviceId id{.address = FakeFlossAdapterClient::kBondedAddress1,
                         .name = ""};
        observer->AdapterClearedDevice(id);
      }));

  // Bonded devices should not be removed.
  base::RunLoop().RunUntilIdle();
  BluetoothDevice* same_bonded_device =
      adapter_->GetDevice(FakeFlossAdapterClient::kBondedAddress1);
  EXPECT_TRUE(same_bonded_device != nullptr);
}

TEST_F(BluetoothFlossTest, UpdatesDeviceName) {
  InitializeAndEnableAdapter();
  DiscoverDevices();

  BluetoothDevice* device =
      adapter_->GetDevice(FakeFlossAdapterClient::kClassicAddress);
  ASSERT_TRUE(device != nullptr);
  EXPECT_EQ(device->GetName(), FakeFlossAdapterClient::kClassicName);
}

TEST_F(BluetoothFlossTest, SetAdvertisingInterval) {
  InitializeAndEnableAdapter();

  base::RunLoop run_loop0;
  EXPECT_EQ(static_cast<uint32_t>(0),
            GetFakeAdvertiserClient()->start_advertising_set_called_);

  auto data = std::make_unique<device::BluetoothAdvertisement::Data>(
      device::BluetoothAdvertisement::AdvertisementType::
          ADVERTISEMENT_TYPE_BROADCAST);

  data->set_scan_response_data(
      device::BluetoothAdvertisement::ScanResponseData());

  adapter_->RegisterAdvertisement(
      std::move(data),
      base::BindLambdaForTesting(
          [&run_loop0](
              scoped_refptr<device::BluetoothAdvertisement> advertisement) {
            EXPECT_TRUE(advertisement);

            auto* advertisementfloss =
                static_cast<BluetoothAdvertisementFloss*>(advertisement.get());
            EXPECT_FALSE(advertisementfloss->params().connectable);
            EXPECT_TRUE(advertisementfloss->params().scannable);
            run_loop0.Quit();
          }),
      base::BindOnce([](device::BluetoothAdvertisement::ErrorCode error_code) {
        FAIL();
      }));
  run_loop0.Run();
  EXPECT_EQ(static_cast<uint32_t>(1),
            GetFakeAdvertiserClient()->start_advertising_set_called_);

  base::RunLoop run_loop1;
  EXPECT_EQ(static_cast<uint32_t>(0),
            GetFakeAdvertiserClient()->set_advertising_parameters_called_);
  adapter_->SetAdvertisingInterval(
      base::Milliseconds(20), base::Milliseconds(10240),
      base::BindLambdaForTesting([&run_loop1]() { run_loop1.Quit(); }),
      base::BindOnce([](device::BluetoothAdvertisement::ErrorCode error_code) {
        FAIL();
      }));
  run_loop1.Run();
  EXPECT_EQ(static_cast<uint32_t>(1),
            GetFakeAdvertiserClient()->set_advertising_parameters_called_);

  base::RunLoop run_loop2;
  EXPECT_EQ(static_cast<uint32_t>(0),
            GetFakeAdvertiserClient()->stop_advertising_set_called_);
  adapter_->ResetAdvertising(
      base::BindLambdaForTesting([&run_loop2]() { run_loop2.Quit(); }),
      base::BindOnce([](device::BluetoothAdvertisement::ErrorCode error_code) {
        FAIL();
      }));
  run_loop2.Run();
  EXPECT_EQ(static_cast<uint32_t>(1),
            GetFakeAdvertiserClient()->stop_advertising_set_called_);
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(BluetoothFlossTest, StartLowEnergyScanSessions) {
  InitializeAndEnableAdapter();

  // Initial conditions
  EXPECT_EQ(0, GetFakeLEScanClient()->scanners_registered_);

  auto background_scan_session = adapter_->StartLowEnergyScanSession(
      /*filter=*/nullptr, /*delegate=*/nullptr);
  base::RunLoop().RunUntilIdle();

  // We should have registered a scanner
  EXPECT_EQ(1, GetFakeLEScanClient()->scanners_registered_);

  // Register another scanner
  auto another_background_scan_session = adapter_->StartLowEnergyScanSession(
      /*filter=*/nullptr, /*delegate=*/nullptr);
  base::RunLoop().RunUntilIdle();

  // Should register another scanner
  EXPECT_EQ(2, GetFakeLEScanClient()->scanners_registered_);

  // Destroy one of the sessions
  background_scan_session.reset();
  EXPECT_EQ(1, GetFakeLEScanClient()->scanners_registered_);
}

TEST_F(BluetoothFlossTest, StartLowEnergyScanSessionWithScanResult) {
  InitializeAndEnableAdapter();

  FakeBluetoothLowEnergyScanSessionDelegate delegate;
  GetFakeLEScanClient()->SetNextScannerUUID(
      device::BluetoothUUID(kTestUuidStr));
  auto background_scan_session = adapter_->StartLowEnergyScanSession(
      /*filter=*/nullptr, delegate.GetWeakPtr());
  base::RunLoop().RunUntilIdle();

  FakeBluetoothLowEnergyScanSessionDelegate delegate2;
  GetFakeLEScanClient()->SetNextScannerUUID(
      device::BluetoothUUID(kTestUuidStr2));
  auto background_scan_session2 = adapter_->StartLowEnergyScanSession(
      /*filter=*/nullptr, delegate2.GetWeakPtr());
  base::RunLoop().RunUntilIdle();

  // Initial conditions
  EXPECT_TRUE(GetFakeLEScanClient()->scanner_ids_.empty());

  EXPECT_EQ(0, delegate.sessions_started_);
  EXPECT_TRUE(delegate.devices_found_.empty());
  EXPECT_EQ(0, delegate.sessions_invalidated_);

  EXPECT_EQ(0, delegate2.sessions_started_);
  EXPECT_TRUE(delegate2.devices_found_.empty());
  EXPECT_EQ(0, delegate2.sessions_invalidated_);

  // Simulate OnScannerRegistered.
  RegisterScanner(device::BluetoothUUID(kTestUuidStr), kTestScannerId);
  EXPECT_TRUE(
      base::Contains(GetFakeLEScanClient()->scanner_ids_, kTestScannerId));
  EXPECT_EQ(1, delegate.sessions_started_);
  RegisterScanner(device::BluetoothUUID(kTestUuidStr2), kTestScannerId2);
  EXPECT_TRUE(
      base::Contains(GetFakeLEScanClient()->scanner_ids_, kTestScannerId2));
  EXPECT_EQ(1, delegate2.sessions_started_);

  // Simulate a scan result event
  GetScanResult();
  EXPECT_FALSE(base::Contains(delegate.devices_found_, kTestDeviceAddr));

  base::RunLoop run_loop;
  // Because of the workaround in BluetoothAdapterFloss::AdvertisementFound
  // we need to wait for a bit before checking if OnDeviceFound is called.
  // TODO(b/271165074): This is not needed when Floss daemon can consolidate
  // the OnAdvertisementFound callback together with the first advertisement
  // data.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(2));
  GetAdvFound();
  run_loop.Run();
  // The device found should only affect the scanner that causes it.
  EXPECT_TRUE(base::Contains(delegate.devices_found_, kTestDeviceAddr));
  EXPECT_FALSE(base::Contains(delegate2.devices_found_, kTestDeviceAddr));

  // Check that the scanned device is in the devices_ map so clients can
  // access the device.
  BluetoothDevice* device = adapter_->GetDevice(kTestDeviceAddr);
  EXPECT_NE(nullptr, device);

  adapter_->Shutdown();
  EXPECT_EQ(1, delegate.sessions_invalidated_);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace floss
