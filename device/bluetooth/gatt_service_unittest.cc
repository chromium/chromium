// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/gatt_service.h"

#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/fake_local_gatt_characteristic.h"
#include "device/bluetooth/test/fake_local_gatt_service.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kServiceId[] = "TestServiceId";
const char kCharacteristicUuid[] = "1234";

}  // namespace

namespace bluetooth {

class GattServiceTest : public testing::Test {
 public:
  GattServiceTest() = default;
  ~GattServiceTest() override = default;
  GattServiceTest(const GattServiceTest&) = delete;
  GattServiceTest& operator=(const GattServiceTest&) = delete;

  void SetUp() override {
    fake_local_gatt_service_ = std::make_unique<FakeLocalGattService>(
        /*service_id=*/kServiceId,
        /*service_uuid=*/device::BluetoothUUID(kServiceId),
        /*is_primary=*/false);
    mock_bluetooth_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_bluetooth_adapter_, GetGattService)
        .WillByDefault(testing::Return(nullptr));
    ON_CALL(*mock_bluetooth_adapter_, CreateLocalGattService)
        .WillByDefault(Invoke(this, &GattServiceTest::CreateLocalGattService));

    mojo::PendingReceiver<mojom::GattService> pending_gatt_service_receiver =
        remote_.BindNewPipeAndPassReceiver();
    mojo::PendingRemote<mojom::GattServiceObserver> pending_observer_remote;
    gatt_service_ = std::make_unique<GattService>(
        std::move(pending_gatt_service_receiver),
        std::move(pending_observer_remote), device::BluetoothUUID(kServiceId),
        mock_bluetooth_adapter_.get());
  }

  base::WeakPtr<device::BluetoothLocalGattService> CreateLocalGattService(
      const device::BluetoothUUID& uuid,
      bool is_primary,
      device::BluetoothLocalGattService::Delegate* delegate) {
    // Although this method mocks the BluetoothAdapter's creation of
    // `BluetoothLocalGattService`s, and therefore there is no limit on
    // the number created, `GattService` is expected to only create and own
    // a single `BluetoothLocalGattService` instance, which we enforce in the
    // test with the CHECK below.
    CHECK_EQ(0, create_local_gatt_service_calls_);
    create_local_gatt_service_calls_++;
    return fake_local_gatt_service_->GetWeakPtr();
  }

 protected:
  int create_local_gatt_service_calls_ = 0;
  base::test::SingleThreadTaskEnvironment task_environment_;
  mojo::Remote<mojom::GattService> remote_;
  std::unique_ptr<FakeLocalGattService> fake_local_gatt_service_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
      mock_bluetooth_adapter_;
  std::unique_ptr<GattService> gatt_service_;
};

TEST_F(GattServiceTest, CreateGattServiceOnConstruction) {
  EXPECT_TRUE(gatt_service_);
  EXPECT_EQ(1, create_local_gatt_service_calls_);
}

TEST_F(GattServiceTest, CreateCharacteristic_FailureIfGattServiceIsDestroyed) {
  // Simulate the underlying platform GattService destruction.
  EXPECT_CALL(*mock_bluetooth_adapter_, GetGattService)
      .WillOnce(testing::Return(nullptr));

  base::test::TestFuture<bool> future;
  remote_->CreateCharacteristic(
      /*characteristic_uuid=*/device::BluetoothUUID(kCharacteristicUuid),
      /*permissions=*/
      device::BluetoothGattCharacteristic::Permission::PERMISSION_READ,
      /*properties=*/
      device::BluetoothGattCharacteristic::Property::PROPERTY_READ,
      future.GetCallback());
  EXPECT_FALSE(future.Take());
}

TEST_F(GattServiceTest,
       CreateCharacteristic_FailureIfCreateGattCharacteristicFails) {
  EXPECT_CALL(*mock_bluetooth_adapter_, GetGattService)
      .WillOnce(testing::Return(fake_local_gatt_service_.get()));
  fake_local_gatt_service_->set_should_create_local_gatt_characteristic_succeed(
      false);
  base::test::TestFuture<bool> future;
  remote_->CreateCharacteristic(
      /*characteristic_uuid=*/device::BluetoothUUID(kCharacteristicUuid),
      /*permissions=*/
      device::BluetoothGattCharacteristic::Permission::PERMISSION_READ,
      /*properties=*/
      device::BluetoothGattCharacteristic::Property::PROPERTY_READ,
      future.GetCallback());
  EXPECT_FALSE(future.Take());
  EXPECT_FALSE(
      fake_local_gatt_service_->GetCharacteristic(kCharacteristicUuid));
}

TEST_F(GattServiceTest, CreateCharacteristic_SuccessIfCreated) {
  // Simulate that the GATT service is created successfully, and is never
  // destroyed during the lifetime of this test.
  ON_CALL(*mock_bluetooth_adapter_, GetGattService)
      .WillByDefault(testing::Return(fake_local_gatt_service_.get()));
  base::test::TestFuture<bool> future;
  remote_->CreateCharacteristic(
      /*characteristic_uuid=*/device::BluetoothUUID(kCharacteristicUuid),
      /*permissions=*/
      device::BluetoothGattCharacteristic::Permission::PERMISSION_READ,
      /*properties=*/
      device::BluetoothGattCharacteristic::Property::PROPERTY_READ,
      future.GetCallback());
  EXPECT_TRUE(future.Take());
  EXPECT_TRUE(fake_local_gatt_service_->GetCharacteristic(kCharacteristicUuid));
}

TEST_F(GattServiceTest, CreateCharacteristic_FailureIfAlreadyExists) {
  // Simulate that the GATT service is created successfully, and is never
  // destroyed during the lifetime of this test.
  ON_CALL(*mock_bluetooth_adapter_, GetGattService)
      .WillByDefault(testing::Return(fake_local_gatt_service_.get()));

  // Create the characteristic the first time.
  {
    base::test::TestFuture<bool> future;
    remote_->CreateCharacteristic(
        /*characteristic_uuid=*/device::BluetoothUUID(kCharacteristicUuid),
        /*permissions=*/
        device::BluetoothGattCharacteristic::Permission::PERMISSION_READ,
        /*properties=*/
        device::BluetoothGattCharacteristic::Property::PROPERTY_READ,
        future.GetCallback());
    EXPECT_TRUE(future.Take());
    EXPECT_TRUE(
        fake_local_gatt_service_->GetCharacteristic(kCharacteristicUuid));
  }

  // Expect success on another call because it already exists.
  {
    base::test::TestFuture<bool> future;
    remote_->CreateCharacteristic(
        /*characteristic_uuid=*/device::BluetoothUUID(kCharacteristicUuid),
        /*permissions=*/
        device::BluetoothGattCharacteristic::Permission::PERMISSION_READ,
        /*properties=*/
        device::BluetoothGattCharacteristic::Property::PROPERTY_READ,
        future.GetCallback());
    EXPECT_FALSE(future.Take());
  }
}

TEST_F(GattServiceTest,
       CreateCharacteristic_Success_MultiplePermissionsAndProperties) {
  // Simulate that the GATT service is created successfully, and is never
  // destroyed during the lifetime of this test.
  ON_CALL(*mock_bluetooth_adapter_, GetGattService)
      .WillByDefault(testing::Return(fake_local_gatt_service_.get()));
  base::test::TestFuture<bool> future;

  device::BluetoothGattCharacteristic::Permissions permissions =
      device::BluetoothGattCharacteristic::Permission::PERMISSION_READ |
      device::BluetoothGattCharacteristic::Permission::PERMISSION_WRITE;
  device::BluetoothGattCharacteristic::Properties properties =
      device::BluetoothGattCharacteristic::Property::PROPERTY_READ |
      device::BluetoothGattCharacteristic::Property::PROPERTY_WRITE;

  remote_->CreateCharacteristic(
      /*characteristic_uuid=*/device::BluetoothUUID(kCharacteristicUuid),
      /*permissions=*/permissions,
      /*properties=*/properties, future.GetCallback());
  EXPECT_TRUE(future.Take());

  auto* fake_characteristic =
      fake_local_gatt_service_->GetCharacteristic(kCharacteristicUuid);
  EXPECT_TRUE(fake_characteristic);
  EXPECT_EQ(properties, fake_characteristic->GetProperties());
  EXPECT_EQ(permissions, fake_characteristic->GetPermissions());
}

}  // namespace bluetooth
