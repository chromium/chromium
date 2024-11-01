// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_ble_connection.h"

#include <bitset>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_characteristic.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_connection.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_notify_session.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"
#include "device/fido/cable/fido_ble_uuids.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "device/bluetooth/test/bluetooth_test_android.h"
#elif BUILDFLAG(IS_MAC)
#include "device/bluetooth/test/bluetooth_test_mac.h"
#elif BUILDFLAG(IS_WIN)
#include "device/bluetooth/test/bluetooth_test_win.h"
#elif BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#include "device/bluetooth/test/bluetooth_test_bluez.h"
#elif BUILDFLAG(IS_FUCHSIA)
#include "device/bluetooth/test/bluetooth_test_fuchsia.h"
#endif

namespace device {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Return;

using NiceMockBluetoothAdapter = ::testing::NiceMock<MockBluetoothAdapter>;
using NiceMockBluetoothDevice = ::testing::NiceMock<MockBluetoothDevice>;
using NiceMockBluetoothGattService =
    ::testing::NiceMock<MockBluetoothGattService>;
using NiceMockBluetoothGattCharacteristic =
    ::testing::NiceMock<MockBluetoothGattCharacteristic>;
using NiceMockBluetoothGattConnection =
    ::testing::NiceMock<MockBluetoothGattConnection>;
using NiceMockBluetoothGattNotifySession =
    ::testing::NiceMock<MockBluetoothGattNotifySession>;

namespace {

constexpr auto kDefaultServiceRevision =
    static_cast<uint8_t>(FidoBleConnection::ServiceRevision::kFido2);

std::vector<uint8_t> ToByteVector(std::string_view str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

BluetoothDevice* GetMockDevice(MockBluetoothAdapter* adapter,
                               const std::string& address) {
  const std::vector<BluetoothDevice*> devices = adapter->GetMockDevices();
  auto found =
      base::ranges::find(devices, address, &BluetoothDevice::GetAddress);
  return found != devices.end() ? *found : nullptr;
}

class TestReadCallback {
 public:
  void OnRead(std::vector<uint8_t> value) {
    value_ = std::move(value);
    run_loop_->Quit();
  }

  const std::vector<uint8_t> WaitForResult() {
    run_loop_->Run();
    run_loop_.emplace();
    return value_;
  }

  FidoBleConnection::ReadCallback GetCallback() {
    return base::BindRepeating(&TestReadCallback::OnRead,
                               base::Unretained(this));
  }

 private:
  std::vector<uint8_t> value_;
  std::optional<base::RunLoop> run_loop_{std::in_place};
};

using TestConnectionFuture = base::test::TestFuture<bool>;

using TestReadControlPointLengthFuture =
    base::test::TestFuture<std::optional<uint16_t>>;

using TestReadServiceRevisionsFuture =
    base::test::TestFuture<std::set<FidoBleConnection::ServiceRevision>>;

using TestWriteCallback = base::test::TestFuture<bool>;
}  // namespace

class FidoBleConnectionTest : public ::testing::Test {
 public:
  FidoBleConnectionTest() {
    ON_CALL(*adapter_, GetDevice(_))
        .WillByDefault(Invoke([this](const std::string& address) {
          return GetMockDevice(adapter_.get(), address);
        }));

    BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
  }

  BluetoothAdapter* adapter() { return adapter_.get(); }
  MockBluetoothDevice* device() { return fido_device_; }

  void AddFidoDevice(const std::string& device_address) {
    auto fido_device = std::make_unique<NiceMockBluetoothDevice>(
        adapter_.get(), /* bluetooth_class */ 0u,
        BluetoothTest::kTestDeviceNameU2f, device_address, /* paired */ true,
        /* connected */ false);
    fido_device_ = fido_device.get();
    adapter_->AddMockDevice(std::move(fido_device));

    ON_CALL(*fido_device_, GetGattServices())
        .WillByDefault(
            Invoke(fido_device_.get(), &MockBluetoothDevice::GetMockServices));

    ON_CALL(*fido_device_, GetGattService(_))
        .WillByDefault(
            Invoke(fido_device_.get(), &MockBluetoothDevice::GetMockService));
    AddFidoService();
  }

  void SetupConnectingFidoDevice(const std::string& device_address) {
    ON_CALL(*fido_device_, CreateGattConnection)
        .WillByDefault(Invoke([this, &device_address](auto callback,
                                                      auto service_uuid) {
          connection_ =
              new NiceMockBluetoothGattConnection(adapter_, device_address);
          std::move(callback).Run(
              std::move(base::WrapUnique(connection_.get())),
              /*error_code=*/std::nullopt);
        }));

    ON_CALL(*fido_device_, IsGattServicesDiscoveryComplete)
        .WillByDefault(Return(true));

    ON_CALL(*fido_service_revision_bitfield_, ReadRemoteCharacteristic_)
        .WillByDefault(Invoke(
            [=](BluetoothRemoteGattCharacteristic::ValueCallback& callback) {
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, base::BindOnce(std::move(callback),
                                            /*error_code=*/std::nullopt,
                                            std::vector<uint8_t>(
                                                {kDefaultServiceRevision})));
            }));

    ON_CALL(*fido_service_revision_bitfield_, WriteRemoteCharacteristic_)
        .WillByDefault(Invoke(
            [=](auto&, BluetoothRemoteGattCharacteristic::WriteType,
                base::OnceClosure& callback,
                const BluetoothRemoteGattCharacteristic::ErrorCallback&) {
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, std::move(callback));
            }));

    ON_CALL(*fido_status_, StartNotifySession_(_, _))
        .WillByDefault(Invoke(
            [this](BluetoothRemoteGattCharacteristic::NotifySessionCallback&
                       callback,
                   BluetoothRemoteGattCharacteristic::ErrorCallback&) {
              notify_session_ = new NiceMockBluetoothGattNotifySession(
                  fido_status_->GetWeakPtr());
              std::move(callback).Run(base::WrapUnique(notify_session_.get()));
            }));
  }

  void SimulateGattDiscoveryComplete(bool complete) {
    EXPECT_CALL(*fido_device_, IsGattServicesDiscoveryComplete)
        .WillOnce(Return(complete));
  }

  void SimulateGattConnectionError() {
    EXPECT_CALL(*fido_device_, CreateGattConnection)
        .WillOnce(Invoke([](auto callback, auto service_uuid) {
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE, base::BindOnce(std::move(callback),
                                        /*connection=*/nullptr,
                                        BluetoothDevice::ERROR_FAILED));
        }));
  }

  void SimulateGattNotifySessionStartError() {
    EXPECT_CALL(*fido_status_, StartNotifySession_(_, _))
        .WillOnce(Invoke(
            [](auto&&,
               BluetoothGattCharacteristic::ErrorCallback& error_callback) {
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(std::move(error_callback),
                                 BluetoothGattService::GattErrorCode::kFailed));
            }));
  }

  void NotifyStatusChanged(const std::vector<uint8_t>& value) {
    for (auto& observer : adapter_->GetObservers())
      observer.GattCharacteristicValueChanged(adapter_.get(), fido_status_,
                                              value);
  }

  void NotifyGattServicesDiscovered() {
    adapter_->NotifyGattServicesDiscovered(fido_device_);
  }

  void ChangeDeviceAddressAndNotifyObservers(std::string new_address) {
    auto old_address = fido_device_->GetAddress();
    EXPECT_CALL(*fido_device_, GetAddress)
        .WillRepeatedly(::testing::Return(new_address));
    for (auto& observer : adapter_->GetObservers())
      observer.DeviceAddressChanged(adapter_.get(), fido_device_, old_address);
  }

  void SetNextReadControlPointLengthReponse(bool success,
                                            const std::vector<uint8_t>& value) {
    EXPECT_CALL(*fido_control_point_length_, ReadRemoteCharacteristic_(_))
        .WillOnce(Invoke(
            [success, value](
                BluetoothRemoteGattCharacteristic::ValueCallback& callback) {
              std::optional<BluetoothGattService::GattErrorCode> error_code;
              if (!success)
                error_code = BluetoothGattService::GattErrorCode::kFailed;
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(std::move(callback), error_code, value));
            }));
  }

  void SetNextReadServiceRevisionResponse(bool success,
                                          const std::vector<uint8_t>& value) {
    EXPECT_CALL(*fido_service_revision_, ReadRemoteCharacteristic_(_))
        .WillOnce(Invoke(
            [success, value](
                BluetoothRemoteGattCharacteristic::ValueCallback& callback) {
              std::optional<BluetoothGattService::GattErrorCode> error_code;
              if (!success)
                error_code = BluetoothGattService::GattErrorCode::kFailed;
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(std::move(callback), error_code, value));
            }));
  }

  void SetNextReadServiceRevisionBitfieldResponse(
      bool success,
      const std::vector<uint8_t>& value) {
    EXPECT_CALL(*fido_service_revision_bitfield_, ReadRemoteCharacteristic_(_))
        .WillOnce(Invoke(
            [success, value](
                BluetoothRemoteGattCharacteristic::ValueCallback& callback) {
              auto error_code =
                  success ? std::nullopt
                          : std::make_optional(
                                BluetoothGattService::GattErrorCode::kFailed);
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(std::move(callback), error_code, value));
            }));
  }

  void SetNextWriteControlPointResponse(bool success) {
    EXPECT_CALL(
        *fido_control_point_,
        WriteRemoteCharacteristic_(
            _, BluetoothRemoteGattCharacteristic::WriteType::kWithoutResponse,
            _, _))
        .WillOnce(
            Invoke([success](const auto& data,
                             BluetoothRemoteGattCharacteristic::WriteType,
                             base::OnceClosure& callback,
                             BluetoothRemoteGattCharacteristic::ErrorCallback&
                                 error_callback) {
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  success ? std::move(callback)
                          : base::BindOnce(
                                std::move(error_callback),
                                BluetoothGattService::GattErrorCode::kFailed));
            }));
  }

  void SetNextWriteServiceRevisionResponse(std::vector<uint8_t> expected_data,
                                           bool success) {
    EXPECT_CALL(
        *fido_service_revision_bitfield_,
        WriteRemoteCharacteristic_(
            expected_data,
            BluetoothRemoteGattCharacteristic::WriteType::kWithResponse, _, _))
        .WillOnce(
            Invoke([success](const auto& data,
                             BluetoothRemoteGattCharacteristic::WriteType,
                             base::OnceClosure& callback,
                             BluetoothRemoteGattCharacteristic::ErrorCallback&
                                 error_callback) {
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  success ? std::move(callback)
                          : base::BindOnce(
                                std::move(error_callback),
                                BluetoothGattService::GattErrorCode::kFailed));
            }));
  }

  void AddFidoService() {
    auto fido_service = std::make_unique<NiceMockBluetoothGattService>(
        fido_device_, "fido_service", BluetoothUUID(kFidoServiceUUID),
        /*is_primary=*/true);
    fido_service_ = fido_service.get();
    fido_device_->AddMockService(std::move(fido_service));

    {
      auto fido_control_point =
          std::make_unique<NiceMockBluetoothGattCharacteristic>(
              fido_service_, "fido_control_point",
              BluetoothUUID(kFidoControlPointUUID),
              BluetoothGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE,
              BluetoothGattCharacteristic::PERMISSION_NONE);
      fido_control_point_ = fido_control_point.get();
      fido_service_->AddMockCharacteristic(std::move(fido_control_point));
    }

    {
      auto fido_status = std::make_unique<NiceMockBluetoothGattCharacteristic>(
          fido_service_, "fido_status", BluetoothUUID(kFidoStatusUUID),
          BluetoothGattCharacteristic::PROPERTY_NOTIFY,
          BluetoothGattCharacteristic::PERMISSION_NONE);
      fido_status_ = fido_status.get();
      fido_service_->AddMockCharacteristic(std::move(fido_status));
    }

    {
      auto fido_control_point_length =
          std::make_unique<NiceMockBluetoothGattCharacteristic>(
              fido_service_, "fido_control_point_length",
              BluetoothUUID(kFidoControlPointLengthUUID),
              BluetoothGattCharacteristic::PROPERTY_READ,
              BluetoothGattCharacteristic::PERMISSION_NONE);
      fido_control_point_length_ = fido_control_point_length.get();
      fido_service_->AddMockCharacteristic(
          std::move(fido_control_point_length));
    }

    {
      auto fido_service_revision =
          std::make_unique<NiceMockBluetoothGattCharacteristic>(
              fido_service_, "fido_service_revision",
              BluetoothUUID(kFidoServiceRevisionUUID),
              BluetoothGattCharacteristic::PROPERTY_READ,
              BluetoothGattCharacteristic::PERMISSION_NONE);
      fido_service_revision_ = fido_service_revision.get();
      fido_service_->AddMockCharacteristic(std::move(fido_service_revision));
    }

    {
      auto fido_service_revision_bitfield =
          std::make_unique<NiceMockBluetoothGattCharacteristic>(
              fido_service_, "fido_service_revision_bitfield",
              BluetoothUUID(kFidoServiceRevisionBitfieldUUID),
              BluetoothGattCharacteristic::PROPERTY_READ |
                  BluetoothGattCharacteristic::PROPERTY_WRITE,
              BluetoothGattCharacteristic::PERMISSION_NONE);
      fido_service_revision_bitfield_ = fido_service_revision_bitfield.get();
      fido_service_->AddMockCharacteristic(
          std::move(fido_service_revision_bitfield));
    }
  }

 protected:
  static BluetoothUUID uuid() { return BluetoothUUID(kFidoServiceUUID); }

 private:
  base::test::TaskEnvironment task_environment_;

  scoped_refptr<MockBluetoothAdapter> adapter_ =
      base::MakeRefCounted<NiceMockBluetoothAdapter>();

  raw_ptr<MockBluetoothDevice> fido_device_;
  raw_ptr<MockBluetoothGattService> fido_service_;

  raw_ptr<MockBluetoothGattCharacteristic> fido_control_point_;
  raw_ptr<MockBluetoothGattCharacteristic> fido_status_;
  raw_ptr<MockBluetoothGattCharacteristic> fido_control_point_length_;
  raw_ptr<MockBluetoothGattCharacteristic> fido_service_revision_;
  raw_ptr<MockBluetoothGattCharacteristic> fido_service_revision_bitfield_;

  raw_ptr<MockBluetoothGattConnection, AcrossTasksDanglingUntriaged>
      connection_;
  raw_ptr<MockBluetoothGattNotifySession, AcrossTasksDanglingUntriaged>
      notify_session_;
};

TEST_F(FidoBleConnectionTest, Address) {
  const std::string device_address = BluetoothTest::kTestDeviceAddress1;
  FidoBleConnection connection(adapter(), device_address, uuid(),
                               base::DoNothing());
  connection.Connect(base::DoNothing());
  EXPECT_EQ(device_address, connection.address());
}

TEST_F(FidoBleConnectionTest, DeviceNotPresent) {
  const std::string device_address = BluetoothTest::kTestDeviceAddress1;
  FidoBleConnection connection(adapter(), device_address, uuid(),
                               base::DoNothing());

  TestConnectionFuture connection_future;
  connection.Connect(connection_future.GetCallback());
  EXPECT_TRUE(connection_future.Wait());
  EXPECT_FALSE(connection_future.Get());
}

TEST_F(FidoBleConnectionTest, PreConnected) {
  const std::string device_address = BluetoothTest::kTestDeviceAddress1;
  AddFidoDevice(device_address);
  SetupConnectingFidoDevice(device_address);
  FidoBleConnection connection(adapter(), device_address, uuid(),
                               base::DoNothing());

  TestConnectionFuture connection_future;
  connection.Connect(connection_future.GetCallback());
  EXPECT_TRUE(connection_future.Wait());
  EXPECT_TRUE(connection_future.Get());
}

TEST_F(FidoBleConnectionTest, NoConnectionWithoutCompletedGattDiscovery) {
  const std::string device_address = BluetoothTest::kTestDeviceAddress1;
  AddFidoDevice(device_address);
  SetupConnectingFidoDevice(device_address);
  FidoBleConnection connection(adapter(), device_address, uuid(),
                               base::DoNothing());

  SimulateGattDiscoveryComplete(false);
  TestConnectionFuture connection_future;
  connection.Connect(connection_future.GetCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(connection_future.IsReady());

  NotifyGattServicesDiscovered();
  EXPECT_TRUE(connection_future.Wait());
  EXPECT_TRUE(connection_future.Get());
}

TEST_F(FidoBleConnectionTest, GattServicesDiscoveredIgnoredBeforeConnection) {
  const std::string device_address = BluetoothTest::kTestDeviceAddress1;
  AddFidoDevice(device_address);
  SetupConnectingFidoDevice(device_address);
  FidoBleConnection connection(adapter(), device_address, uuid(),
                               base::DoNothing());
  NotifyGattServicesDiscovered();

  SimulateGattDiscoveryComplete(false);
  TestConnectionFuture connection_future;
  connection.Connect(connection_future.GetCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(connection_future.IsReady());

  NotifyGattServicesDiscovered();
  EXPECT_TRUE(connection_future.Wait());
  EXPECT_TRUE(connection_future.Get());
}

TEST_F(FidoBleConnectionTest, GattServicesDiscoveredAgain) {
  const std::string device_address = BluetoothTest::kTestDeviceAddress1;
  AddFidoDevice(device_address);
  SetupConnectingFidoDevice(device_address);
  FidoBleConnection connection(adapter(), device_address, uuid(),
                               base::DoNothing());

  TestConnectionFuture connection_future;
  connection.Connect(connection_future.GetCallback());
  NotifyGattServicesDiscovered();
  EXPECT_TRUE(connection_future.Wait());
  EXPECT_TRUE(connection_future.Get());

  // A second call to the event handler should not trigger another attempt to
  // obtain Gatt Services.
  EXPECT_CALL(*device(), GetGattServices).Times(0);
  EXPECT_CALL(*device(), GetGattService).Times(0);
  NotifyGattServicesDiscovered();
}

TEST_F(FidoBleConnectionTest, SimulateGattConnectionError) {
  const std::string device_address = BluetoothTest::kTestDeviceAddress1;
  AddFidoDevice(device_address);
  SetupConnectingFidoDevice(device_address);
  FidoBleConnection connection(adapter(), device_address, uuid(),
                               base::DoNothing());

  SimulateGattConnectionError();
  TestConnectionFuture connection_future;
  connection.Connect(connection_future.GetCallback());
  EXPECT_TRUE(connection_future.Wait());
  EXPECT_FALSE(connection_future.Get());
}

TEST_F(FidoBleConnectionTest, SimulateGattNotifySessionStartError) {
  const std::string device_address = BluetoothTest::kTestDeviceAddress1;
  AddFidoDevice(device_address);
  SetupConnectingFidoDevice(device_address);
  FidoBleConnection connection(adapter(), device_address, uuid(),
                               base::DoNothing());

  SimulateGattNotifySessionStartError();
  TestConnectionFuture connection_future;
  connection.Connect(connection_future.GetCallback());
  EXPECT_TRUE(connection_future.Wait());
  EXPECT_FALSE(connection_future.Get());
}

TEST_F(FidoBleConnectionTest, MultipleServiceRevisions) {
  const std::string device_address = BluetoothTest::kTestDeviceAddress1;
  AddFidoDevice(device_address);
  SetupConnectingFidoDevice(device_address);

  static constexpr struct {
    std::bitset<8> supported_revisions;
    std::bitset<8> selected_revision;
  } test_cases[] = {
      // Only U2F 1.1 is supported, pick it.
      {0b1000'0000, 0b1000'0000},
      // Only U2F 1.2 is supported, pick it.
      {0b0100'0000, 0b0100'0000},
      // U2F 1.1 and U2F 1.2 are supported, pick U2F 1.2.
      {0b1100'0000, 0b0100'0000},
      // Only FIDO2 is supported, pick it.
      {0b0010'0000, 0b0010'0000},
      // U2F 1.1 and FIDO2 are supported, pick FIDO2.
      {0b1010'0000, 0b0010'0000},
      // U2F 1.2 and FIDO2 are supported, pick FIDO2.
      {0b0110'0000, 0b0010'0000},
      // U2F 1.1, U2F 1.2 and FIDO2 are supported, pick FIDO2.
      {0b1110'0000, 0b0010'0000},
      // U2F 1.1 and a future revision are supported, pick U2F 1.1.
      {0b1000'1000, 0b1000'0000},
      // U2F 1.2 and a future revision are supported, pick U2F 1.2.
      {0b0100'1000, 0b0100'0000},
      // FIDO2 and a future revision are supported, pick FIDO2.
      {0b0010'1000, 0b0010'0000},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(::testing::Message()
                 << "Supported Revisions: " << test_case.supported_revisions
                 << ", Selected Revision: " << test_case.selected_revision);
    SetNextReadServiceRevisionBitfieldResponse(
        true, {static_cast<uint8_t>(test_case.supported_revisions.to_ulong())});

    SetNextWriteServiceRevisionResponse(
        {static_cast<uint8_t>(test_case.selected_revision.to_ulong())}, true);

    FidoBleConnection connection(adapter(), device_address, uuid(),
                                 base::DoNothing());
    TestConnectionFuture connection_future;
    connection.Connect(connection_future.GetCallback());
    EXPECT_TRUE(connection_future.Wait());
    EXPECT_TRUE(connection_future.Get());
  }
}

TEST_F(FidoBleConnectionTest, UnsupportedServiceRevisions) {
  const std::string device_address = BluetoothTest::kTestDeviceAddress1;
  AddFidoDevice(device_address);
  SetupConnectingFidoDevice(device_address);

  // Test failure cases.
  static constexpr struct {
    std::bitset<8> supported_revisions;
  } test_cases[] = {
      {0b0000'0000},  // No Service Revision.
      {0b0001'0000},  // Unsupported Service Revision (4th bit).
      {0b0000'1000},  // Unsupported Service Revision (3th bit).
      {0b0000'0100},  // Unsupported Service Revision (2th bit).
      {0b0000'0010},  // Unsupported Service Revision (1th bit).
      {0b0000'0001},  // Unsupported Service Revision (0th bit).
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(::testing::Message()
                 << "Supported Revisions: " << test_case.supported_revisions);
    SetNextReadServiceRevisionBitfieldResponse(
        true, {static_cast<uint8_t>(test_case.supported_revisions.to_ulong())});

    FidoBleConnection connection(adapter(), device_address, uuid(),
                                 base::DoNothing());
    TestConnectionFuture connection_future;
    connection.Connect(connection_future.GetCallback());
    EXPECT_TRUE(connection_future.Wait());
    EXPECT_FALSE(connection_future.Get());
  }
}

TEST_F(FidoBleConnectionTest, ReadServiceRevisionsFails) {
  const std::string device_address = BluetoothTest::kTestDeviceAddress1;

  AddFidoDevice(device_address);
  SetupConnectingFidoDevice(device_address);
  SetNextReadServiceRevisionBitfieldResponse(false, {});

  FidoBleConnection connection(adapter(), device_address, uuid(),
                               base::DoNothing());
  TestConnectionFuture connection_future;
  connection.Connect(connection_future.GetCallback());
  EXPECT_TRUE(connection_future.Wait());
  EXPECT_FALSE(connection_future.Get());
}

TEST_F(FidoBleConnectionTest, WriteServiceRevisionsFails) {
  const std::string device_address = BluetoothTest::kTestDeviceAddress1;

  AddFidoDevice(device_address);
  SetupConnectingFidoDevice(device_address);
  SetNextReadServiceRevisionBitfieldResponse(true, {kDefaultServiceRevision});
  SetNextWriteServiceRevisionResponse({kDefaultServiceRevision}, false);

  FidoBleConnection connection(adapter(), device_address, uuid(),
                               base::DoNothing());
  TestConnectionFuture connection_future;
  connection.Connect(connection_future.GetCallback());
  EXPECT_TRUE(connection_future.Wait());
  EXPECT_FALSE(connection_future.Get());
}

TEST_F(FidoBleConnectionTest, ReadStatusNotifications) {
  const std::string device_address = BluetoothTest::kTestDeviceAddress1;
  TestReadCallback read_callback;

  AddFidoDevice(device_address);
  SetupConnectingFidoDevice(device_address);
  FidoBleConnection connection(adapter(), device_address, uuid(),
                               read_callback.GetCallback());

  TestConnectionFuture connection_future;
  connection.Connect(connection_future.GetCallback());
  EXPECT_TRUE(connection_future.Wait());
  EXPECT_TRUE(connection_future.Get());

  std::vector<uint8_t> payload = ToByteVector("foo");
  NotifyStatusChanged(payload);
  EXPECT_EQ(payload, read_callback.WaitForResult());

  payload = ToByteVector("bar");
  NotifyStatusChanged(payload);
  EXPECT_EQ(payload, read_callback.WaitForResult());
}

TEST_F(FidoBleConnectionTest, ReadControlPointLength) {
  const std::string device_address = BluetoothTest::kTestDeviceAddress1;
  AddFidoDevice(device_address);
  SetupConnectingFidoDevice(device_address);
  FidoBleConnection connection(adapter(), device_address, uuid(),
                               base::DoNothing());

  TestConnectionFuture connection_future;
  connection.Connect(connection_future.GetCallback());
  EXPECT_TRUE(connection_future.Wait());
  EXPECT_TRUE(connection_future.Get());

  {
    TestReadControlPointLengthFuture length_future;
    SetNextReadControlPointLengthReponse(false, {});
    connection.ReadControlPointLength(length_future.GetCallback());
    EXPECT_TRUE(length_future.Wait());
    EXPECT_EQ(std::nullopt, length_future.Get());
  }

  // The Control Point Length should consist of exactly two bytes, hence we
  // EXPECT_EQ(std::nullopt) for payloads of size 0, 1 and 3.
  {
    TestReadControlPointLengthFuture length_future;
    SetNextReadControlPointLengthReponse(true, {});
    connection.ReadControlPointLength(length_future.GetCallback());
    EXPECT_TRUE(length_future.Wait());
    EXPECT_EQ(std::nullopt, length_future.Get());
  }

  {
    TestReadControlPointLengthFuture length_future;
    SetNextReadControlPointLengthReponse(true, {0xAB});
    connection.ReadControlPointLength(length_future.GetCallback());
    EXPECT_TRUE(length_future.Wait());
    EXPECT_EQ(std::nullopt, length_future.Get());
  }

  {
    TestReadControlPointLengthFuture length_future;
    SetNextReadControlPointLengthReponse(true, {0xAB, 0xCD});
    connection.ReadControlPointLength(length_future.GetCallback());
    EXPECT_TRUE(length_future.Wait());
    EXPECT_EQ(0xABCD, *length_future.Get());
  }

  {
    TestReadControlPointLengthFuture length_future;
    SetNextReadControlPointLengthReponse(true, {0xAB, 0xCD, 0xEF});
    connection.ReadControlPointLength(length_future.GetCallback());
    EXPECT_TRUE(length_future.Wait());
    EXPECT_EQ(std::nullopt, length_future.Get());
  }
}

TEST_F(FidoBleConnectionTest, WriteControlPoint) {
  const std::string device_address = BluetoothTest::kTestDeviceAddress1;
  AddFidoDevice(device_address);
  SetupConnectingFidoDevice(device_address);
  FidoBleConnection connection(adapter(), device_address, uuid(),
                               base::DoNothing());

  TestConnectionFuture connection_future;
  connection.Connect(connection_future.GetCallback());
  EXPECT_TRUE(connection_future.Wait());
  EXPECT_TRUE(connection_future.Get());

  {
    TestWriteCallback write_future;
    SetNextWriteControlPointResponse(false);
    connection.WriteControlPoint({}, write_future.GetCallback());
    EXPECT_TRUE(write_future.Wait());
    EXPECT_FALSE(write_future.Get());
  }

  {
    TestWriteCallback write_future;
    SetNextWriteControlPointResponse(true);
    connection.WriteControlPoint({}, write_future.GetCallback());
    EXPECT_TRUE(write_future.Wait());
    EXPECT_TRUE(write_future.Get());
  }
}

TEST_F(FidoBleConnectionTest, ReadsAndWriteFailWhenDisconnected) {
  const std::string device_address = BluetoothTest::kTestDeviceAddress1;

  AddFidoDevice(device_address);
  SetupConnectingFidoDevice(device_address);
  FidoBleConnection connection(adapter(), device_address, uuid(),
                               base::DoNothing());

  SimulateGattConnectionError();
  TestConnectionFuture connection_future;
  connection.Connect(connection_future.GetCallback());
  EXPECT_TRUE(connection_future.Wait());
  EXPECT_FALSE(connection_future.Get());

  // Reads should always fail on a disconnected device.
  TestReadControlPointLengthFuture length_future;
  connection.ReadControlPointLength(length_future.GetCallback());
  EXPECT_TRUE(length_future.Wait());
  EXPECT_EQ(std::nullopt, length_future.Get());

  // Writes should always fail on a disconnected device.
  TestWriteCallback write_future;
  connection.WriteControlPoint({}, write_future.GetCallback());
  EXPECT_TRUE(write_future.Wait());
  EXPECT_FALSE(write_future.Get());
}

TEST_F(FidoBleConnectionTest, ConnectionAddressChangeWhenDeviceAddressChanges) {
  const std::string device_address = BluetoothTest::kTestDeviceAddress1;
  static constexpr char kTestDeviceAddress2[] = "test_device_address_2";

  AddFidoDevice(device_address);
  SetupConnectingFidoDevice(device_address);
  FidoBleConnection connection(adapter(), device_address, uuid(),
                               base::DoNothing());
  ChangeDeviceAddressAndNotifyObservers(kTestDeviceAddress2);
  EXPECT_EQ(kTestDeviceAddress2, connection.address());
}

}  // namespace device
