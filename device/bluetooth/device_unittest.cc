// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/device.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_characteristic.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_connection.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace bluetooth {

using NiceMockBluetoothAdapter =
    testing::NiceMock<device::MockBluetoothAdapter>;
using NiceMockBluetoothDevice = testing::NiceMock<device::MockBluetoothDevice>;
using NiceMockBluetoothGattService =
    testing::NiceMock<device::MockBluetoothGattService>;
using NiceMockBluetoothGattCharacteristic =
    testing::NiceMock<device::MockBluetoothGattCharacteristic>;
using NiceMockBluetoothGattConnection =
    testing::NiceMock<device::MockBluetoothGattConnection>;

using Properties = device::BluetoothGattCharacteristic::Properties;
using Property = device::BluetoothGattCharacteristic::Property;
using Permissions = device::BluetoothGattCharacteristic::Permissions;
using Permission = device::BluetoothGattCharacteristic::Permission;

namespace {
const char kTestLeDeviceAddress0[] = "11:22:33:44:55:66";
const char kTestLeDeviceName0[] = "Test LE Device 0";

const char kTestServiceId0[] = "service_id0";
const char kTestServiceUuid0[] = "1234";

const char kTestServiceId1[] = "service_id1";
const char kTestServiceUuid1[] = "5678";

const char kTestCharacteristicId0[] = "characteristic_id0";
const char kTestCharacteristicUuid0[] = "1234";

const char kTestCharacteristicId1[] = "characteristic_id1";
const char kTestCharacteristicUuid1[] = "5678";

const char kTestCharacteristicId2[] = "characteristic_id2";
const char kTestCharacteristicUuid2[] = "9012";

const Properties kReadWriteProperties =
    Property::PROPERTY_READ | Property::PROPERTY_WRITE;
const Properties kAllProperties = Property::NUM_PROPERTY - 1;
const Permissions kReadWritePermissions =
    Permission::PERMISSION_READ | Permission::PERMISSION_WRITE;
const Permissions kAllPermissions = Permission::NUM_PERMISSION - 1;

class BluetoothInterfaceDeviceTest : public testing::Test {
 public:
  enum class Call { EXPECTED, NOT_EXPECTED };

  BluetoothInterfaceDeviceTest()
      : adapter_(new NiceMockBluetoothAdapter),
        device_(adapter_.get(),
                0,
                kTestLeDeviceName0,
                kTestLeDeviceAddress0,
                false,
                true) {
    ON_CALL(*adapter_, GetDevice(kTestLeDeviceAddress0))
        .WillByDefault(Return(&device_));

    auto service1 = std::make_unique<NiceMockBluetoothGattService>(
        &device_, kTestServiceId0, device::BluetoothUUID(kTestServiceUuid0),
        /*is_primary=*/true);

    auto characteristic1 =
        std::make_unique<NiceMockBluetoothGattCharacteristic>(
            service1.get(), kTestCharacteristicId0,
            device::BluetoothUUID(kTestCharacteristicUuid0),
            kReadWriteProperties, kReadWritePermissions);

    auto characteristic2 =
        std::make_unique<NiceMockBluetoothGattCharacteristic>(
            service1.get(), kTestCharacteristicId1,
            device::BluetoothUUID(kTestCharacteristicUuid1),
            kReadWriteProperties, kReadWritePermissions);

    service1->AddMockCharacteristic(std::move(characteristic1));
    service1->AddMockCharacteristic(std::move(characteristic2));

    auto service2 = std::make_unique<NiceMockBluetoothGattService>(
        &device_, kTestServiceId1, device::BluetoothUUID(kTestServiceUuid1),
        /*is_primary=*/true);

    auto characteristic3 =
        std::make_unique<NiceMockBluetoothGattCharacteristic>(
            service2.get(), kTestCharacteristicId2,
            device::BluetoothUUID(kTestCharacteristicUuid2), kAllProperties,
            kAllPermissions);

    service2->AddMockCharacteristic(std::move(characteristic3));

    device_.AddMockService(std::move(service1));
    device_.AddMockService(std::move(service2));

    EXPECT_CALL(device_, GetGattServices())
        .WillRepeatedly(
            Invoke(&device_, &device::MockBluetoothDevice::GetMockServices));

    EXPECT_CALL(device_, GetGattService(testing::_))
        .WillRepeatedly(
            Invoke(&device_, &device::MockBluetoothDevice::GetMockService));

    auto connection = std::make_unique<NiceMockBluetoothGattConnection>(
        adapter_, device_.GetAddress());

    Device::Create(adapter_, std::move(connection),
                   proxy_.BindNewPipeAndPassReceiver());

    proxy_.set_disconnect_handler(
        base::BindOnce(&BluetoothInterfaceDeviceTest::OnConnectionError,
                       weak_factory_.GetWeakPtr()));
  }

  void TearDown() override {
    EXPECT_EQ(expected_success_callback_calls_, actual_success_callback_calls_);
    EXPECT_EQ(message_pipe_closed_, expect_device_service_deleted_);
    proxy_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void OnConnectionError() { message_pipe_closed_ = true; }

  void SimulateGattServicesDiscovered() {
    for (auto& observer : adapter_->GetObservers())
      observer.GattServicesDiscovered(adapter_.get(), &device_);
  }

  void SimulateDeviceChanged() {
    for (auto& observer : adapter_->GetObservers())
      observer.DeviceChanged(adapter_.get(), &device_);
  }

  void CheckGetServicesCountImpl(Call expected,
                                 size_t expected_service_count,
                                 int num_of_preceding_calls,
                                 std::vector<mojom::ServiceInfoPtr> services) {
    EXPECT_EQ(num_of_preceding_calls, actual_callback_count_);
    ++actual_callback_count_;

    if (expected == Call::EXPECTED)
      ++actual_success_callback_calls_;

    EXPECT_EQ(expected_service_count, services.size());
  }

  Device::GetServicesCallback CheckGetServicesCount(Call expected) {
    if (expected == Call::EXPECTED)
      ++expected_success_callback_calls_;

    return base::BindOnce(
        &BluetoothInterfaceDeviceTest::CheckGetServicesCountImpl,
        weak_factory_.GetWeakPtr(), expected, 2 /* expected_service_count */,
        expected_callback_count_++);
  }

  scoped_refptr<NiceMockBluetoothAdapter> adapter_;
  NiceMockBluetoothDevice device_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  mojo::Remote<mojom::Device> proxy_;

  bool message_pipe_closed_ = false;
  bool expect_device_service_deleted_ = false;
  int expected_success_callback_calls_ = 0;
  int actual_success_callback_calls_ = 0;
  int actual_callback_count_ = 0;
  int expected_callback_count_ = 0;

  base::WeakPtrFactory<BluetoothInterfaceDeviceTest> weak_factory_{this};
};
}  // namespace

TEST_F(BluetoothInterfaceDeviceTest, GetServices) {
  EXPECT_CALL(device_, IsGattServicesDiscoveryComplete())
      .WillRepeatedly(Return(true));

  proxy_->GetServices(CheckGetServicesCount(Call::EXPECTED));

  base::RunLoop().RunUntilIdle();
}

TEST_F(BluetoothInterfaceDeviceTest, GetCharacteristics) {
  EXPECT_CALL(device_, IsGattServicesDiscoveryComplete())
      .WillRepeatedly(Return(true));

  base::test::TestFuture<
      std::optional<std::vector<bluetooth::mojom::CharacteristicInfoPtr>>>
      future;
  proxy_->GetCharacteristics(kTestServiceId0, future.GetCallback());

  const auto& characteristics = future.Get();
  EXPECT_TRUE(characteristics.has_value());
  EXPECT_EQ(2u, characteristics.value().size());

  const auto& characteristic = characteristics.value().at(0);
  EXPECT_EQ(kTestCharacteristicId0, characteristic->id);
  EXPECT_EQ(device::BluetoothUUID(kTestCharacteristicUuid0),
            characteristic->uuid);
  EXPECT_EQ(kReadWriteProperties, characteristic->properties);
  EXPECT_EQ(kReadWritePermissions, characteristic->permissions);
}

TEST_F(BluetoothInterfaceDeviceTest, GetServicesNotDiscovered) {
  EXPECT_CALL(device_, IsGattServicesDiscoveryComplete())
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));

  // Client: Sends multiple requests for services.
  proxy_->GetServices(CheckGetServicesCount(Call::EXPECTED));
  proxy_->GetServices(CheckGetServicesCount(Call::EXPECTED));

  base::RunLoop().RunUntilIdle();

  SimulateGattServicesDiscovered();

  // No more GetServices calls will complete.
  SimulateGattServicesDiscovered();

  base::RunLoop().RunUntilIdle();

  // Client: Sends more requests which run immediately.
  proxy_->GetServices(CheckGetServicesCount(Call::EXPECTED));
  proxy_->GetServices(CheckGetServicesCount(Call::EXPECTED));

  base::RunLoop().RunUntilIdle();

  // No more GetServices calls will complete.
  SimulateGattServicesDiscovered();

  // Wait for message pipe to process error.
  base::RunLoop().RunUntilIdle();
}

TEST_F(BluetoothInterfaceDeviceTest,
       GetServicesLostConnectionWithPendingRequests) {
  EXPECT_CALL(device_, IsGattServicesDiscoveryComplete())
      .WillRepeatedly(Return(false));
  // Client: Sends multiple requests for services.
  proxy_->GetServices(CheckGetServicesCount(Call::NOT_EXPECTED));
  proxy_->GetServices(CheckGetServicesCount(Call::NOT_EXPECTED));
  EXPECT_EQ(0, actual_callback_count_);

  // Simulate connection loss.
  device_.SetConnected(false);
  SimulateDeviceChanged();
  expect_device_service_deleted_ = true;

  // Wait for message pipe to process error.
  base::RunLoop().RunUntilIdle();
}

TEST_F(BluetoothInterfaceDeviceTest,
       GetServicesForcedDisconnectionWithPendingRequests) {
  EXPECT_CALL(device_, IsGattServicesDiscoveryComplete())
      .WillRepeatedly(Return(false));

  // Client: Sends multiple requests for services.
  proxy_->GetServices(CheckGetServicesCount(Call::NOT_EXPECTED));
  proxy_->GetServices(CheckGetServicesCount(Call::NOT_EXPECTED));
  EXPECT_EQ(0, actual_callback_count_);

  // Simulate connection loss.
  proxy_->Disconnect();
  expect_device_service_deleted_ = true;

  // Wait for message pipe to process error.
  base::RunLoop().RunUntilIdle();
}
}  // namespace bluetooth
