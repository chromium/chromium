// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/bluetooth_local_gatt_service_floss.h"

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/floss/bluetooth_local_gatt_characteristic_floss.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace floss {

namespace {

constexpr char kTestUUIDGenericService[] =
    "00001801-0000-1000-8000-00805f9b34fb";
constexpr char kTestUUIDGenericCharacteristic[] =
    "00001801-0001-1000-8000-00805f9b34fb";
constexpr char kTestUUIDGenericDescriptor[] =
    "00001801-0002-1000-8000-00805f9b34fb";

}  // namespace

class BluetoothLocalGattServiceFlossTest : public testing::Test {
 public:
  BluetoothLocalGattServiceFlossTest() = default;

  void SetUp() override {
    FlossDBusManager::GetSetterForTesting();

    expected_client_instance_id = INIT_CLIENT_INSTANCE_ID;
    expected_floss_instance_id = 0;
  }

  void TearDown() override {}

  // Mock a registration of this service. Replaces
  // `BluetoothLocalGattServiceFloss::Register`.
  void RegisterService(base::WeakPtr<BluetoothLocalGattServiceFloss> service,
                       base::RunLoop& run_loop) {
    service->register_callbacks_ = std::make_pair(
        base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }),
        base::BindLambdaForTesting(
            [&run_loop](BluetoothGattServiceFloss::GattErrorCode error_code) {
              LOG(ERROR) << " Test failed with error_code "
                         << static_cast<int>(error_code);
              run_loop.Quit();
            }));
  }

  int32_t ClientInstanceId(
      base::WeakPtr<BluetoothLocalGattServiceFloss> service) {
    return service->client_instance_id_;
  }
  int32_t ClientInstanceId(
      base::WeakPtr<BluetoothLocalGattCharacteristicFloss> characteristic) {
    return characteristic->client_instance_id_;
  }
  int32_t ClientInstanceId(
      base::WeakPtr<BluetoothLocalGattDescriptorFloss> descriptor) {
    return descriptor->client_instance_id_;
  }

  uint32_t INIT_CLIENT_INSTANCE_ID =
      BluetoothLocalGattServiceFloss::instance_id_tracker_;

  int expected_client_instance_id;
  int expected_floss_instance_id;

  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(BluetoothLocalGattServiceFlossTest, InstanceIdsResolved) {
  // Create custom UUIDs for each GATT attribute.
  auto service_uuid = device::BluetoothUUID(kTestUUIDGenericService);
  auto characteristic_uuid =
      device::BluetoothUUID(kTestUUIDGenericCharacteristic);
  auto descriptor_uuid = device::BluetoothUUID(kTestUUIDGenericDescriptor);

  // Create and add GATT attributes.
  auto service = BluetoothLocalGattServiceFloss::Create(
      nullptr, service_uuid, /*is_primary=*/true, nullptr);
  auto characteristic = BluetoothLocalGattCharacteristicFloss::Create(
      characteristic_uuid,
      device::BluetoothLocalGattCharacteristic::Properties(),
      device::BluetoothLocalGattCharacteristic::Permissions(), service.get());
  auto descriptor = BluetoothLocalGattDescriptorFloss::Create(
      descriptor_uuid, device::BluetoothLocalGattCharacteristic::Permissions(),
      characteristic.get());

  // Create corresponding D-Bus-friendly GATT attribute objects.
  GattDescriptor dbus_descriptor;
  dbus_descriptor.uuid = descriptor_uuid;
  dbus_descriptor.instance_id = ++expected_floss_instance_id;
  GattCharacteristic dbus_characteristic;
  dbus_characteristic.uuid = characteristic_uuid;
  dbus_characteristic.instance_id = ++expected_floss_instance_id;
  dbus_characteristic.descriptors = {dbus_descriptor};
  GattService dbus_service;
  dbus_service.uuid = service_uuid;
  dbus_service.instance_id = ++expected_floss_instance_id;
  dbus_service.characteristics = {dbus_characteristic};

  base::RunLoop run_loop;
  RegisterService(service, run_loop);
  service->GattServerServiceAdded(GattStatus::kSuccess, dbus_service);
  base::RunLoop().RunUntilIdle();

  // Ensure uuids match.
  EXPECT_EQ(service->GetUUID(), service_uuid);
  EXPECT_EQ(characteristic->GetUUID(), characteristic_uuid);
  EXPECT_EQ(descriptor->GetUUID(), descriptor_uuid);

  // Ensure instance ids are preserved.
  EXPECT_EQ(ClientInstanceId(service), expected_client_instance_id++);
  EXPECT_EQ(ClientInstanceId(characteristic), expected_client_instance_id++);
  EXPECT_EQ(ClientInstanceId(descriptor), expected_client_instance_id++);

  // Ensure Floss-assigned instance ids match.
  EXPECT_EQ(service->InstanceId(), expected_floss_instance_id--);
  EXPECT_EQ(characteristic->InstanceId(), expected_floss_instance_id--);
  EXPECT_EQ(descriptor->InstanceId(), expected_floss_instance_id--);

  base::RunLoop().RunUntilIdle();
}

}  // namespace floss
