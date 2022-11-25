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
#include "device/bluetooth/floss/fake_floss_battery_manager_client.h"
#include "device/bluetooth/floss/fake_floss_gatt_client.h"
#include "device/bluetooth/floss/fake_floss_lescan_client.h"
#include "device/bluetooth/floss/fake_floss_manager_client.h"
#include "device/bluetooth/floss/fake_floss_socket_manager.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/test/mock_pairing_delegate.h"
#include "device/bluetooth/test/test_bluetooth_adapter_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "device/bluetooth/floss/fake_floss_admin_client.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

using ::device::BluetoothAdapter;
using ::device::BluetoothDevice;
using ::device::BluetoothDiscoverySession;
using ::device::MockPairingDelegate;
using ::device::TestBluetoothAdapterObserver;
using ::testing::_;
using ::testing::StrictMock;

const uint8_t kTestScannerId = 10;
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
      absl::optional<device::BluetoothLowEnergyScanSession::ErrorCode>
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
    std::unique_ptr<floss::FlossDBusManagerSetter> dbus_setter =
        floss::FlossDBusManager::GetSetterForTesting();

    auto fake_floss_manager_client = std::make_unique<FakeFlossManagerClient>();
    auto fake_floss_adapter_client = std::make_unique<FakeFlossAdapterClient>();
    auto fake_floss_lescan_client = std::make_unique<FakeFlossLEScanClient>();
    auto fake_floss_advertiser_client =
        std::make_unique<FakeFlossAdvertiserClient>();
    auto fake_floss_battery_manager_client =
        std::make_unique<FakeFlossBatteryManagerClient>();
#if BUILDFLAG(IS_CHROMEOS)
    auto fake_floss_admin_client = std::make_unique<FakeFlossAdminClient>();
#endif  // BUILDFLAG(IS_CHROMEOS)

    fake_floss_manager_client_ = fake_floss_manager_client.get();
    fake_floss_adapter_client_ = fake_floss_adapter_client.get();
    fake_floss_lescan_client_ = fake_floss_lescan_client.get();
    fake_floss_advertiser_client_ = fake_floss_advertiser_client.get();
    fake_floss_battery_manager_client_ =
        fake_floss_battery_manager_client.get();

#if BUILDFLAG(IS_CHROMEOS)
    fake_floss_admin_client_ = fake_floss_admin_client.get();
#endif  // BUILDFLAG(IS_CHROMEOS)

    dbus_setter->SetFlossManagerClient(std::move(fake_floss_manager_client));
    dbus_setter->SetFlossAdapterClient(std::move(fake_floss_adapter_client));
    dbus_setter->SetFlossGattClient(std::make_unique<FakeFlossGattClient>());
    dbus_setter->SetFlossSocketManager(
        std::make_unique<FakeFlossSocketManager>());
    dbus_setter->SetFlossLEScanClient(std::move(fake_floss_lescan_client));
    dbus_setter->SetFlossAdvertiserClient(
        std::move(fake_floss_advertiser_client));
    dbus_setter->SetFlossBatteryManagerClient(
        std::move(fake_floss_battery_manager_client));
#if BUILDFLAG(IS_CHROMEOS)
    dbus_setter->SetFlossAdminClient(std::move(fake_floss_admin_client));
#endif  // BUILDFLAG(IS_CHROMEOS)
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

  // Simulate adapter enabled event. After adapter is enabled, there are known
  // devices.
  void EnableAdapter() {
    ASSERT_TRUE(adapter_.get() != nullptr);

    fake_floss_manager_client_->NotifyObservers(
        base::BindLambdaForTesting([](FlossManagerClient::Observer* observer) {
          observer->AdapterEnabledChanged(/*adapter=*/0, /*enabled=*/true);
        }));
    base::RunLoop().RunUntilIdle();
  }

  // Simulates getting a ScannerRegistered callback and then a
  // ScanResultReceived
  void RegisterScannerAndGetScanResult() {
    ASSERT_TRUE(adapter_.get() != nullptr);
    BluetoothAdapterFloss* floss_adapter =
        static_cast<BluetoothAdapterFloss*>(adapter_.get());

    floss_adapter->ScannerRegistered(device::BluetoothUUID(kTestUuidStr),
                                     kTestScannerId, GattStatus::kSuccess);

    base::RunLoop().RunUntilIdle();

    ScanResult scan_result;
    scan_result.address = kTestDeviceAddr;
    scan_result.name = kTestDeviceName;
    floss_adapter->ScanResultReceived(scan_result);
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
  raw_ptr<FakeFlossManagerClient> fake_floss_manager_client_;
  raw_ptr<FakeFlossAdapterClient> fake_floss_adapter_client_;
  raw_ptr<FakeFlossLEScanClient> fake_floss_lescan_client_;
  raw_ptr<FakeFlossAdvertiserClient> fake_floss_advertiser_client_;
  raw_ptr<FakeFlossBatteryManagerClient> fake_floss_battery_manager_client_;
#if BUILDFLAG(IS_CHROMEOS)
  raw_ptr<FakeFlossAdminClient> fake_floss_admin_client_;
#endif  // BUILDFLAG(IS_CHROMEOS)

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
  ASSERT_FALSE(device->IsConnecting());

  StrictMock<MockPairingDelegate> pairing_delegate;
  base::RunLoop run_loop;
  device->Connect(
      &pairing_delegate,
      base::BindLambdaForTesting(
          [&run_loop](absl::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            run_loop.Quit();
          }));
  ASSERT_TRUE(device->IsConnecting());
  run_loop.Run();

  ASSERT_FALSE(device->IsConnecting());
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

TEST_F(BluetoothFlossTest, PairDisplayPasskeySucceeded) {
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

TEST_F(BluetoothFlossTest, PairDisplayPasskeyFailed) {
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
        // Pretend that the remote device has entered wrong passkey.
        fake_floss_adapter_client_->NotifyObservers(base::BindLambdaForTesting(
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
          [&run_loop](absl::optional<BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_TRUE(error.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_FALSE(device->IsPaired());
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

TEST_F(BluetoothFlossTest, Disconnect) {
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
  device->Disconnect(base::BindLambdaForTesting([&run_loop2]() {
                       SUCCEED();
                       run_loop2.Quit();
                     }),
                     base::BindLambdaForTesting([]() { FAIL(); }));
  run_loop2.Run();
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
            FakeFlossAdapterClient::kHeadsetClassOfDevice);
  EXPECT_EQ(device2->GetBluetoothClass(),
            FakeFlossAdapterClient::kHeadsetClassOfDevice);
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

TEST_F(BluetoothFlossTest, DisabledAdapterClearsDevices) {
  InitializeAdapter();
  DiscoverDevices();

  EXPECT_TRUE(adapter_->GetDevices().size() > 0);
  // Simulate adapter enabled event.
  fake_floss_manager_client_->NotifyObservers(
      base::BindLambdaForTesting([](FlossManagerClient::Observer* observer) {
        observer->AdapterEnabledChanged(/*adapter=*/0, /*enabled=*/false);
      }));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(adapter_->GetDevices().empty());
}

TEST_F(BluetoothFlossTest, RepeatsDiscoverySession) {
  InitializeAdapter();
  DiscoverDevices();

  EXPECT_TRUE(adapter_->IsDiscovering());

  // Simulate discovery state changed to False.
  fake_floss_adapter_client_->NotifyObservers(
      base::BindLambdaForTesting([](FlossAdapterClient::Observer* observer) {
        observer->AdapterDiscoveringChanged(false);
      }));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(adapter_->IsDiscovering());

  // Force discovery to fail after discovering is stopped.
  fake_floss_adapter_client_->FailNextDiscovery();
  fake_floss_adapter_client_->NotifyObservers(
      base::BindLambdaForTesting([](FlossAdapterClient::Observer* observer) {
        observer->AdapterDiscoveringChanged(false);
      }));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothFlossTest, HandlesClearedDevices) {
  InitializeAdapter();
  EnableAdapter();
  DiscoverDevices();

  BluetoothDevice* device =
      adapter_->GetDevice(FakeFlossAdapterClient::kJustWorksAddress);
  EXPECT_TRUE(device != nullptr);

  // Simulate clearing away a device.
  fake_floss_adapter_client_->NotifyObservers(
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

  fake_floss_adapter_client_->NotifyObservers(
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
  InitializeAdapter();
  DiscoverDevices();

  BluetoothDevice* device =
      adapter_->GetDevice(FakeFlossAdapterClient::kClassicAddress);
  ASSERT_TRUE(device != nullptr);
  EXPECT_EQ(device->GetName(), FakeFlossAdapterClient::kClassicName);
}

TEST_F(BluetoothFlossTest, SetAdvertisingInterval) {
  InitializeAdapter();

  base::RunLoop run_loop0;
  EXPECT_EQ(static_cast<uint32_t>(0),
            fake_floss_advertiser_client_->start_advertising_set_called_);

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
            fake_floss_advertiser_client_->start_advertising_set_called_);

  base::RunLoop run_loop1;
  EXPECT_EQ(static_cast<uint32_t>(0),
            fake_floss_advertiser_client_->set_advertising_parameters_called_);
  adapter_->SetAdvertisingInterval(
      base::TimeDelta(), base::TimeDelta(),
      base::BindLambdaForTesting([&run_loop1]() { run_loop1.Quit(); }),
      base::BindOnce([](device::BluetoothAdvertisement::ErrorCode error_code) {
        FAIL();
      }));
  run_loop1.Run();
  EXPECT_EQ(static_cast<uint32_t>(1),
            fake_floss_advertiser_client_->set_advertising_parameters_called_);

  base::RunLoop run_loop2;
  EXPECT_EQ(static_cast<uint32_t>(0),
            fake_floss_advertiser_client_->stop_advertising_set_called_);
  adapter_->ResetAdvertising(
      base::BindLambdaForTesting([&run_loop2]() { run_loop2.Quit(); }),
      base::BindOnce([](device::BluetoothAdvertisement::ErrorCode error_code) {
        FAIL();
      }));
  run_loop2.Run();
  EXPECT_EQ(static_cast<uint32_t>(1),
            fake_floss_advertiser_client_->stop_advertising_set_called_);
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(BluetoothFlossTest, StartLowEnergyScanSessions) {
  InitializeAdapter();
  EnableAdapter();

  // Initial conditions
  EXPECT_EQ(0, fake_floss_lescan_client_->scanners_registered_);

  // TODO (b/217274013): Filter is currently being ignored
  auto background_scan_session = adapter_->StartLowEnergyScanSession(
      /*filter=*/nullptr, /*delegate=*/nullptr);
  base::RunLoop().RunUntilIdle();

  // We should have registered a scanner
  EXPECT_EQ(1, fake_floss_lescan_client_->scanners_registered_);

  // Register another scanner
  auto another_background_scan_session = adapter_->StartLowEnergyScanSession(
      /*filter=*/nullptr, /*delegate=*/nullptr);
  base::RunLoop().RunUntilIdle();

  // Should register another scanner
  EXPECT_EQ(2, fake_floss_lescan_client_->scanners_registered_);

  // Destroy one of the sessions
  background_scan_session.reset();
  EXPECT_EQ(1, fake_floss_lescan_client_->scanners_registered_);
}

TEST_F(BluetoothFlossTest, StartLowEnergyScanSessionWithScanResult) {
  InitializeAdapter();
  EnableAdapter();

  FakeBluetoothLowEnergyScanSessionDelegate delegate;
  // TODO (b/217274013): Filter is currently being ignored
  auto background_scan_session = adapter_->StartLowEnergyScanSession(
      /*filter=*/nullptr, delegate.GetWeakPtr());
  base::RunLoop().RunUntilIdle();

  // Initial conditions
  EXPECT_TRUE(fake_floss_lescan_client_->scanner_ids_.empty());
  EXPECT_EQ(0, delegate.sessions_started_);
  EXPECT_TRUE(delegate.devices_found_.empty());
  EXPECT_EQ(0, delegate.sessions_invalidated_);

  // Simulate a scan result event
  RegisterScannerAndGetScanResult();
  EXPECT_TRUE(
      base::Contains(fake_floss_lescan_client_->scanner_ids_, kTestScannerId));
  EXPECT_EQ(1, delegate.sessions_started_);
  EXPECT_TRUE(base::Contains(delegate.devices_found_, kTestDeviceAddr));

  // Check that the scanned device is in the devices_ map so clients can
  // access the device.
  BluetoothDevice* device = adapter_->GetDevice(kTestDeviceAddr);
  EXPECT_NE(nullptr, device);

  adapter_->Shutdown();
  EXPECT_EQ(1, delegate.sessions_invalidated_);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace floss
