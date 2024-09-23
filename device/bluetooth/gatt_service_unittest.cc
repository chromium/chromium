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
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kServiceId[] = "12345678-1234-5678-9abc-def123456789";
const char kCharacteristicUuid[] = "00001101-0000-1000-8000-00805f9b34fb";
const char kTestDeviceName[] = "TestDeviceName";
const char kTestDeviceAddress[] = "TestDeviceAddress";
const std::vector<uint8_t> kReadCharacteristicValue = {0x01, 0x02, 0x03};
const int kReadCharacteristicOffset = 0;

std::unique_ptr<bluetooth::FakeLocalGattCharacteristic>
CreateFakeCharacteristic(device::BluetoothLocalGattService* service) {
  return std::make_unique<bluetooth::FakeLocalGattCharacteristic>(
      /*characteristic_id=*/kCharacteristicUuid,
      /*characteristic_uuid=*/device::BluetoothUUID(kCharacteristicUuid),
      /*service=*/service,
      /*properties=*/
      device::BluetoothGattCharacteristic::Permission::PERMISSION_READ,
      /*property=*/
      device::BluetoothGattCharacteristic::Property::PROPERTY_READ);
}

std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>
CreateMockBluetoothDevice() {
  return std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
      /*adapter=*/nullptr,
      /*class=*/0, kTestDeviceName, kTestDeviceAddress,
      /*paired=*/false,
      /*connected=*/false);
}

}  // namespace

namespace bluetooth {

class GattServiceTest : public testing::Test,
                        public mojom::GattServiceObserver {
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
    mojo::PendingRemote<mojom::GattServiceObserver> pending_observer_remote =
        observer_.BindNewPipeAndPassRemote();
    gatt_service_ = std::make_unique<GattService>(
        std::move(pending_gatt_service_receiver),
        std::move(pending_observer_remote), device::BluetoothUUID(kServiceId),
        mock_bluetooth_adapter_.get(),
        base::BindOnce(&GattServiceTest::OnGattServiceInvalidated,
                       base::Unretained(this)));
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
    delegate_ = delegate;
    return fake_local_gatt_service_->GetWeakPtr();
  }

  // mojo::GattServiceObserver:
  void OnLocalCharacteristicRead(
      bluetooth::mojom::DeviceInfoPtr remote_device,
      const device::BluetoothUUID& characteristic_uuid,
      const device::BluetoothUUID& service_uuid,
      uint32_t offset,
      OnLocalCharacteristicReadCallback callback) override {
    mojom::LocalCharacteristicReadResultPtr read_result;
    if (local_characteristic_read_error_code_.has_value()) {
      read_result = mojom::LocalCharacteristicReadResult::NewErrorCode(
          local_characteristic_read_error_code_.value());
    } else {
      CHECK(local_characteristic_read_value_.has_value());
      read_result = mojom::LocalCharacteristicReadResult::NewData(
          local_characteristic_read_value_.value());
    }

    std::move(callback).Run(std::move(read_result));
    std::move(on_local_characteristic_read_callback_).Run();
  }

  void OnGattServiceInvalidated(device::BluetoothUUID service_id) {
    EXPECT_EQ(device::BluetoothUUID(kServiceId), service_id);
    gatt_service_invalidated_ = true;
    std::move(on_gatt_service_invalidated_callback_).Run();
  }

  void SetGattServiceInvalidatedCallback(base::OnceClosure callback) {
    on_gatt_service_invalidated_callback_ = std::move(callback);
  }

  void CallCreateCharacteristic(
      bool gatt_service_exists,
      bool expected_success,
      device::BluetoothGattCharacteristic::Permissions permissions =
          device::BluetoothGattCharacteristic::Permission::PERMISSION_READ,
      device::BluetoothGattCharacteristic::Properties properties =
          device::BluetoothGattCharacteristic::Property::PROPERTY_READ) {
    if (gatt_service_exists) {
      // Simulate that the GATT service is created successfully, and is never
      // destroyed during the lifetime of this test.
      ON_CALL(*mock_bluetooth_adapter_, GetGattService)
          .WillByDefault(testing::Return(fake_local_gatt_service_.get()));
    } else {
      // Simulate the underlying platform GattService destruction.
      EXPECT_CALL(*mock_bluetooth_adapter_, GetGattService)
          .WillOnce(testing::Return(nullptr));
    }

    base::test::TestFuture<bool> future;
    remote_->CreateCharacteristic(
        /*characteristic_uuid=*/device::BluetoothUUID(kCharacteristicUuid),
        /*permissions=*/permissions,
        /*properties=*/properties, future.GetCallback());
    EXPECT_EQ(expected_success, future.Take());
    EXPECT_EQ(expected_success,
              (nullptr != fake_local_gatt_service_->GetCharacteristic(
                              kCharacteristicUuid)));
  }

  void SetOnLocalCharacteristicReadCallback(base::OnceClosure callback) {
    on_local_characteristic_read_callback_ = std::move(callback);
  }

  void SetOnLocalCharacteristicReadResult(
      std::optional<device::BluetoothGattService::GattErrorCode> error_code,
      std::optional<std::vector<uint8_t>> value) {
    local_characteristic_read_error_code_ = error_code;
    local_characteristic_read_value_ = value;
  }

 protected:
  bool gatt_service_invalidated_ = false;
  std::optional<device::BluetoothGattService::GattErrorCode>
      local_characteristic_read_error_code_;
  std::optional<std::vector<uint8_t>> local_characteristic_read_value_;
  base::OnceClosure on_local_characteristic_read_callback_;
  base::OnceClosure on_gatt_service_invalidated_callback_;
  int create_local_gatt_service_calls_ = 0;
  base::test::SingleThreadTaskEnvironment task_environment_;
  mojo::Remote<mojom::GattService> remote_;
  std::unique_ptr<FakeLocalGattService> fake_local_gatt_service_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
      mock_bluetooth_adapter_;
  std::unique_ptr<GattService> gatt_service_;
  raw_ptr<device::BluetoothLocalGattService::Delegate> delegate_;
  mojo::Receiver<mojom::GattServiceObserver> observer_{this};
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
  CallCreateCharacteristic(/*gatt_service_exists=*/false,
                           /*expected_success=*/false);
}

TEST_F(GattServiceTest, CreateCharacteristic_SuccessIfCreated) {
  CallCreateCharacteristic(/*gatt_service_exists=*/true,
                           /*expected_success=*/true);
}

TEST_F(GattServiceTest,
       CreateCharacteristic_Success_MultiplePermissionsAndProperties) {
  device::BluetoothGattCharacteristic::Permissions permissions =
      device::BluetoothGattCharacteristic::Permission::PERMISSION_READ |
      device::BluetoothGattCharacteristic::Permission::PERMISSION_WRITE;
  device::BluetoothGattCharacteristic::Properties properties =
      device::BluetoothGattCharacteristic::Property::PROPERTY_READ |
      device::BluetoothGattCharacteristic::Property::PROPERTY_WRITE;

  CallCreateCharacteristic(/*gatt_service_exists=*/true,
                           /*expected_success=*/true, permissions, properties);

  auto* fake_characteristic =
      fake_local_gatt_service_->GetCharacteristic(kCharacteristicUuid);
  EXPECT_TRUE(fake_characteristic);
  EXPECT_EQ(properties, fake_characteristic->GetProperties());
  EXPECT_EQ(permissions, fake_characteristic->GetPermissions());
}

TEST_F(GattServiceTest, OnReadCharacteristic_Success) {
  // Simulate that the GATT service is created successfully, and is never
  // destroyed during the lifetime of this test. Simulate a successful
  // characteristic being added to the GATT service.
  CallCreateCharacteristic(/*gatt_service_exists=*/true,
                           /*expected_success=*/true);

  base::MockCallback<base::OnceClosure> mock_observer_callback;
  EXPECT_CALL(mock_observer_callback, Run).Times(1);
  SetOnLocalCharacteristicReadCallback(mock_observer_callback.Get());
  SetOnLocalCharacteristicReadResult(
      /*error_code=*/std::nullopt,
      /*value=*/kReadCharacteristicValue);

  base::RunLoop run_loop;
  auto mock_device = CreateMockBluetoothDevice();
  auto fake_characteristic =
      CreateFakeCharacteristic(fake_local_gatt_service_.get());

  delegate_->OnCharacteristicReadRequest(
      /*device=*/mock_device.get(),
      /*characteristic=*/fake_characteristic.get(),
      /*offset=*/kReadCharacteristicOffset,
      /*callback=*/
      base::BindLambdaForTesting(
          [&](std::optional<::device::BluetoothGattService::GattErrorCode>
                  error_code,
              const std::vector<uint8_t>& value) {
            EXPECT_FALSE(error_code.has_value());
            EXPECT_FALSE(value.empty());
            run_loop.Quit();
          }));

  run_loop.Run();
}

TEST_F(GattServiceTest, OnReadCharacteristic_Failure) {
  // Simulate that the GATT service is created successfully, and is never
  // destroyed during the lifetime of this test. Simulate a successful
  // characteristic being added to the GATT service.
  CallCreateCharacteristic(/*gatt_service_exists=*/true,
                           /*expected_success=*/true);

  base::MockCallback<base::OnceClosure> mock_observer_callback;
  EXPECT_CALL(mock_observer_callback, Run).Times(1);
  SetOnLocalCharacteristicReadCallback(mock_observer_callback.Get());
  SetOnLocalCharacteristicReadResult(
      /*error_code=*/device::BluetoothGattService::GattErrorCode::kFailed,
      /*value=*/std::nullopt);

  base::RunLoop run_loop;
  auto mock_device = CreateMockBluetoothDevice();
  auto fake_characteristic =
      CreateFakeCharacteristic(fake_local_gatt_service_.get());
  delegate_->OnCharacteristicReadRequest(
      /*device=*/mock_device.get(),
      /*characteristic=*/fake_characteristic.get(),
      /*offset=*/kReadCharacteristicOffset,
      /*callback=*/
      base::BindLambdaForTesting(
          [&](std::optional<::device::BluetoothGattService::GattErrorCode>
                  error_code,
              const std::vector<uint8_t>& value) {
            EXPECT_TRUE(error_code.has_value());
            EXPECT_TRUE(value.empty());
            run_loop.Quit();
          }));

  run_loop.Run();
}

TEST_F(GattServiceTest, MojoDisconnect_GattServiceRemote) {
  // Simulate that the GATT service is created successfully, and thus is always
  // accessible via `GetGattService()` calls.
  ON_CALL(*mock_bluetooth_adapter_, GetGattService)
      .WillByDefault(testing::Return(fake_local_gatt_service_.get()));

  base::RunLoop run_loop;
  SetGattServiceInvalidatedCallback(run_loop.QuitClosure());
  remote_.reset();
  run_loop.Run();

  EXPECT_TRUE(gatt_service_invalidated_);
  EXPECT_TRUE(fake_local_gatt_service_->WasDeleted());
}

TEST_F(GattServiceTest, MojoDisconnect_GattServiceObserverRemote) {
  // Simulate that the GATT service is created successfully, and thus is always
  // accessible via `GetGattService()` calls.
  ON_CALL(*mock_bluetooth_adapter_, GetGattService)
      .WillByDefault(testing::Return(fake_local_gatt_service_.get()));

  base::RunLoop run_loop;
  SetGattServiceInvalidatedCallback(run_loop.QuitClosure());
  observer_.reset();
  run_loop.Run();

  EXPECT_TRUE(gatt_service_invalidated_);
  EXPECT_TRUE(fake_local_gatt_service_->WasDeleted());
}

TEST_F(GattServiceTest, Register_Success) {
  // Simulate that the GATT service is created successfully, and is never
  // destroyed during the lifetime of this test.
  ON_CALL(*mock_bluetooth_adapter_, GetGattService)
      .WillByDefault(testing::Return(fake_local_gatt_service_.get()));
  fake_local_gatt_service_->set_should_registration_succeed(true);

  base::test::TestFuture<
      std::optional<device::BluetoothGattService::GattErrorCode>>
      future;
  remote_->Register(future.GetCallback());
  EXPECT_FALSE(future.Take());
}

TEST_F(GattServiceTest, Register_Failure) {
  // Simulate that the GATT service is created successfully, and is never
  // destroyed during the lifetime of this test.
  ON_CALL(*mock_bluetooth_adapter_, GetGattService)
      .WillByDefault(testing::Return(fake_local_gatt_service_.get()));
  fake_local_gatt_service_->set_should_registration_succeed(false);

  base::test::TestFuture<
      std::optional<device::BluetoothGattService::GattErrorCode>>
      future;
  remote_->Register(future.GetCallback());
  EXPECT_TRUE(future.Take());
}

}  // namespace bluetooth
