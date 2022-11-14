// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/adapter.h"

#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_advertisement.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_socket.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceCallback;

using testing::_;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::NiceMock;
using testing::Return;

namespace {

const char kKnownDeviceAddress[] = "00:00:00:00:01";
const char kUnknownDeviceAddress[] = "00:00:00:00:02";
const char kServiceName[] = "ServiceName";
const char kServiceId[] = "0000abcd-0000-0000-0000-000000000001";
const char kDeviceServiceDataStr[] = "ServiceData";

std::vector<uint8_t> GetByteVector(const std::string& str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

class MockBluetoothAdapterWithAdvertisements
    : public device::MockBluetoothAdapter {
 public:
  void RegisterAdvertisement(
      std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
      device::BluetoothAdapter::CreateAdvertisementCallback callback,
      device::BluetoothAdapter::AdvertisementErrorCallback error_callback)
      override {
    last_advertisement_data_ = std::move(advertisement_data);

    if (should_advertisement_registration_succeed_) {
      std::move(callback).Run(
          base::MakeRefCounted<device::MockBluetoothAdvertisement>());
    } else {
      std::move(error_callback)
          .Run(device::BluetoothAdvertisement::ErrorCode::
                   INVALID_ADVERTISEMENT_ERROR_CODE);
    }
  }

  bool should_advertisement_registration_succeed_ = true;
  std::unique_ptr<device::BluetoothAdvertisement::Data>
      last_advertisement_data_;

 protected:
  ~MockBluetoothAdapterWithAdvertisements() override = default;
};

}  // namespace

namespace bluetooth {

class AdapterTest : public testing::Test {
 public:
  AdapterTest() = default;
  ~AdapterTest() override = default;
  AdapterTest(const AdapterTest&) = delete;
  AdapterTest& operator=(const AdapterTest&) = delete;

  void SetUp() override {
    mock_bluetooth_adapter_ = base::MakeRefCounted<
        NiceMock<MockBluetoothAdapterWithAdvertisements>>();
    ON_CALL(*mock_bluetooth_adapter_, IsPresent()).WillByDefault(Return(true));
    ON_CALL(*mock_bluetooth_adapter_, IsPowered()).WillByDefault(Return(true));

    // |mock_known_bluetooth_device_| is a device found via discovery.
    mock_known_bluetooth_device_ =
        std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
            mock_bluetooth_adapter_.get(),
            /*class=*/0, "Known Device", kKnownDeviceAddress,
            /*paired=*/false,
            /*connected=*/false);
    // |mock_unknown_bluetooth_device_| is |connected| because it is created
    // as a result of calling ConnectDevice().
    mock_unknown_bluetooth_device_ =
        std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
            mock_bluetooth_adapter_.get(),
            /*class=*/0, "Unknown Device", kUnknownDeviceAddress,
            /*paired=*/false,
            /*connected=*/true);

    // |mock_bluetooth_adapter_| can only find |mock_known_bluetooth_device_|
    // via GetDevice(), not |mock_unknown_bluetooth_device_|.
    ON_CALL(*mock_bluetooth_adapter_, GetDevice(kKnownDeviceAddress))
        .WillByDefault(Return(mock_known_bluetooth_device_.get()));

    mock_bluetooth_socket_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothSocket>>();

    adapter_ = std::make_unique<Adapter>(mock_bluetooth_adapter_);
  }

 protected:
  void RegisterAdvertisement(bool should_succeed, bool use_scan_data) {
    mock_bluetooth_adapter_->should_advertisement_registration_succeed_ =
        should_succeed;

    auto service_data = GetByteVector(kDeviceServiceDataStr);
    mojo::Remote<mojom::Advertisement> advertisement;

    base::RunLoop run_loop;
    adapter_->RegisterAdvertisement(
        device::BluetoothUUID(kServiceId), service_data,
        /*use_scan_data=*/use_scan_data,
        base::BindLambdaForTesting([&](mojo::PendingRemote<mojom::Advertisement>
                                           pending_advertisement) {
          EXPECT_EQ(should_succeed, pending_advertisement.is_valid());
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  void VerifyAdvertisement() {
    auto service_data = GetByteVector(kDeviceServiceDataStr);
    auto uuid_list =
        mock_bluetooth_adapter_->last_advertisement_data_->service_uuids();
    EXPECT_EQ(1u, uuid_list->size());
    EXPECT_EQ(kServiceId, (*uuid_list)[0]);
    auto last_service_data =
        mock_bluetooth_adapter_->last_advertisement_data_->service_data();
    EXPECT_EQ(service_data, last_service_data->at(kServiceId));
    EXPECT_FALSE(mock_bluetooth_adapter_->last_advertisement_data_
                     ->scan_response_data());
  }

  void VerifyAdvertisementWithScanData() {
    auto service_data = GetByteVector(kDeviceServiceDataStr);
    auto uuid_list =
        mock_bluetooth_adapter_->last_advertisement_data_->service_uuids();
    EXPECT_EQ(1u, uuid_list->size());
    EXPECT_EQ(kServiceId, (*uuid_list)[0]);
    EXPECT_FALSE(
        mock_bluetooth_adapter_->last_advertisement_data_->service_data());
    auto last_scan_response_data =
        mock_bluetooth_adapter_->last_advertisement_data_->scan_response_data();
    ASSERT_TRUE(base::Contains(*last_scan_response_data, 0x16));
    const auto& raw_data = (*last_scan_response_data)[0x16];
    // First two bytes should be the identifying bits of the kServiceId UUID.
    // They should be in litten endian order (reversed).
    EXPECT_EQ(0xCD, raw_data[0]);
    EXPECT_EQ(0xAB, raw_data[1]);
    EXPECT_EQ(service_data,
              std::vector<uint8_t>(raw_data.begin() + 2, raw_data.end()));
  }

  scoped_refptr<NiceMock<MockBluetoothAdapterWithAdvertisements>>
      mock_bluetooth_adapter_;
  std::unique_ptr<NiceMock<device::MockBluetoothDevice>>
      mock_known_bluetooth_device_;
  std::unique_ptr<NiceMock<device::MockBluetoothDevice>>
      mock_unknown_bluetooth_device_;
  scoped_refptr<NiceMock<device::MockBluetoothSocket>> mock_bluetooth_socket_;
  std::unique_ptr<Adapter> adapter_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(AdapterTest, TestRegisterAdvertisement_Success) {
  RegisterAdvertisement(/*should_succeed=*/true, /*use_scan_data=*/false);
  VerifyAdvertisement();
}

TEST_F(AdapterTest, TestRegisterAdvertisement_Error) {
  RegisterAdvertisement(/*should_succeed=*/false, /*use_scan_data=*/false);
  VerifyAdvertisement();
}

TEST_F(AdapterTest, TestRegisterAdvertisement_ScanResponseData) {
  RegisterAdvertisement(/*should_succeed=*/true, /*use_scan_data=*/true);
  VerifyAdvertisementWithScanData();
}

TEST_F(AdapterTest, TestConnectToServiceInsecurely_DisallowedUuid) {
  // Do not call Adapter::AllowConnectionsForUuid();

  base::RunLoop run_loop;
  adapter_->ConnectToServiceInsecurely(
      kKnownDeviceAddress, device::BluetoothUUID(kServiceId),
      /*should_unbond_on_error=*/false,
      base::BindLambdaForTesting(
          [&](mojom::ConnectToServiceResultPtr connect_to_service_result) {
            EXPECT_FALSE(connect_to_service_result);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(AdapterTest, TestConnectToServiceInsecurely_KnownDevice_Success) {
  EXPECT_CALL(
      *mock_known_bluetooth_device_,
      ConnectToServiceInsecurely(device::BluetoothUUID(kServiceId), _, _))
      .WillOnce(RunOnceCallback<1>(mock_bluetooth_socket_));

  adapter_->AllowConnectionsForUuid(device::BluetoothUUID(kServiceId));

  base::RunLoop run_loop;
  adapter_->ConnectToServiceInsecurely(
      kKnownDeviceAddress, device::BluetoothUUID(kServiceId),
      /*should_unbond_on_error=*/false,
      base::BindLambdaForTesting(
          [&](mojom::ConnectToServiceResultPtr connect_to_service_result) {
            EXPECT_TRUE(connect_to_service_result);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(AdapterTest, TestConnectToServiceInsecurely_KnownDevice_Error) {
  EXPECT_CALL(
      *mock_known_bluetooth_device_,
      ConnectToServiceInsecurely(device::BluetoothUUID(kServiceId), _, _))
      .WillOnce(RunOnceCallback<2>("Error"));

  adapter_->AllowConnectionsForUuid(device::BluetoothUUID(kServiceId));

  base::RunLoop run_loop;
  adapter_->ConnectToServiceInsecurely(
      kKnownDeviceAddress, device::BluetoothUUID(kServiceId),
      /*should_unbond_on_error=*/false,
      base::BindLambdaForTesting(
          [&](mojom::ConnectToServiceResultPtr connect_to_service_result) {
            EXPECT_FALSE(connect_to_service_result);
            run_loop.Quit();
          }));
  run_loop.Run();
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
TEST_F(
    AdapterTest,
    TestConnectToServiceInsecurely_UnknownDevice_Success_ServicesAlreadyResolved) {
  EXPECT_CALL(*mock_bluetooth_adapter_,
              ConnectDevice(kUnknownDeviceAddress, _, _, _))
      .WillOnce(RunOnceCallback<2>(mock_unknown_bluetooth_device_.get()));
  EXPECT_CALL(
      *mock_unknown_bluetooth_device_,
      ConnectToServiceInsecurely(device::BluetoothUUID(kServiceId), _, _))
      .WillOnce(RunOnceCallback<1>(mock_bluetooth_socket_));
  EXPECT_CALL(*mock_unknown_bluetooth_device_,
              IsGattServicesDiscoveryComplete())
      .WillOnce(Return(true));

  adapter_->AllowConnectionsForUuid(device::BluetoothUUID(kServiceId));

  base::RunLoop run_loop;
  adapter_->ConnectToServiceInsecurely(
      kUnknownDeviceAddress, device::BluetoothUUID(kServiceId),
      /*should_unbond_on_error=*/false,
      base::BindLambdaForTesting(
          [&](mojom::ConnectToServiceResultPtr connect_to_service_result) {
            EXPECT_TRUE(connect_to_service_result);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(
    AdapterTest,
    TestConnectToServiceInsecurely_UnknownDevice_Success_WaitForServicesToResolve) {
  EXPECT_CALL(*mock_bluetooth_adapter_,
              ConnectDevice(kUnknownDeviceAddress, _, _, _))
      .WillOnce(RunOnceCallback<2>(mock_unknown_bluetooth_device_.get()));
  EXPECT_CALL(
      *mock_unknown_bluetooth_device_,
      ConnectToServiceInsecurely(device::BluetoothUUID(kServiceId), _, _))
      .WillOnce(RunOnceCallback<1>(mock_bluetooth_socket_));

  // At first, return false to force |adapter_| to wait for the value to change,
  // but subsequently return true. On that first call, post a task to trigger
  // a notification that services are now resolved.
  EXPECT_CALL(*mock_unknown_bluetooth_device_,
              IsGattServicesDiscoveryComplete())
      .WillOnce(
          DoAll(InvokeWithoutArgs([this]() {
                  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                      FROM_HERE, base::BindLambdaForTesting([&]() {
                        adapter_->GattServicesDiscovered(
                            mock_bluetooth_adapter_.get(),
                            mock_unknown_bluetooth_device_.get());
                      }));
                }),
                Return(false)))
      .WillRepeatedly(Return(true));

  adapter_->AllowConnectionsForUuid(device::BluetoothUUID(kServiceId));

  base::RunLoop run_loop;
  adapter_->ConnectToServiceInsecurely(
      kUnknownDeviceAddress, device::BluetoothUUID(kServiceId),
      /*should_unbond_on_error=*/false,
      base::BindLambdaForTesting(
          [&](mojom::ConnectToServiceResultPtr connect_to_service_result) {
            EXPECT_TRUE(connect_to_service_result);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(
    AdapterTest,
    TestConnectToServiceInsecurely_UnknownDevice_Failure_WaitForServicesToResolve_DeviceRemoved) {
  EXPECT_CALL(*mock_bluetooth_adapter_,
              ConnectDevice(kUnknownDeviceAddress, _, _, _))
      .WillOnce(RunOnceCallback<2>(mock_unknown_bluetooth_device_.get()));

  // Return false to force |adapter_| to wait for the value to change.
  EXPECT_CALL(*mock_unknown_bluetooth_device_,
              IsGattServicesDiscoveryComplete())
      .WillOnce(Return(false));

  adapter_->AllowConnectionsForUuid(device::BluetoothUUID(kServiceId));

  base::RunLoop run_loop;
  adapter_->ConnectToServiceInsecurely(
      kUnknownDeviceAddress, device::BluetoothUUID(kServiceId),
      /*should_unbond_on_error=*/false,
      base::BindLambdaForTesting(
          [&](mojom::ConnectToServiceResultPtr connect_to_service_result) {
            EXPECT_FALSE(connect_to_service_result);
            run_loop.Quit();
          }));
  // Device is removed before GATT service discovery is complete, resulting in a
  // failed connect-to-service result
  adapter_->DeviceRemoved(mock_bluetooth_adapter_.get(),
                          mock_unknown_bluetooth_device_.get());
  run_loop.Run();
}

TEST_F(
    AdapterTest,
    TestConnectToServiceInsecurely_UnknownDevice_Failure_WaitForServicesToResolve_DeviceChangedWithNoRssi) {
  EXPECT_CALL(*mock_bluetooth_adapter_,
              ConnectDevice(kUnknownDeviceAddress, _, _, _))
      .WillOnce(RunOnceCallback<2>(mock_unknown_bluetooth_device_.get()));

  // Return false to force |adapter_| to wait for the value to change.
  EXPECT_CALL(*mock_unknown_bluetooth_device_,
              IsGattServicesDiscoveryComplete())
      .WillOnce(Return(false));

  adapter_->AllowConnectionsForUuid(device::BluetoothUUID(kServiceId));

  base::RunLoop run_loop;
  adapter_->ConnectToServiceInsecurely(
      kUnknownDeviceAddress, device::BluetoothUUID(kServiceId),
      /*should_unbond_on_error=*/false,
      base::BindLambdaForTesting(
          [&](mojom::ConnectToServiceResultPtr connect_to_service_result) {
            EXPECT_FALSE(connect_to_service_result);
            run_loop.Quit();
          }));
  // Before GATT service discovery is complete, we are notified of a device
  // change where the device has no RSSI. This will result in a failed
  // connect-to-service result.
  EXPECT_CALL(*mock_unknown_bluetooth_device_, GetInquiryRSSI())
      .WillRepeatedly(Return(absl::nullopt));
  adapter_->DeviceChanged(mock_bluetooth_adapter_.get(),
                          mock_unknown_bluetooth_device_.get());
  run_loop.Run();
}

TEST_F(AdapterTest, TestConnectToServiceInsecurely_UnknownDevice_Error) {
  EXPECT_CALL(*mock_bluetooth_adapter_,
              ConnectDevice(kUnknownDeviceAddress, _, _, _))
      .WillOnce(RunOnceCallback<3>(""));

  adapter_->AllowConnectionsForUuid(device::BluetoothUUID(kServiceId));

  base::RunLoop run_loop;
  adapter_->ConnectToServiceInsecurely(
      kUnknownDeviceAddress, device::BluetoothUUID(kServiceId),
      /*should_unbond_on_error=*/false,
      base::BindLambdaForTesting(
          [&](mojom::ConnectToServiceResultPtr connect_to_service_result) {
            EXPECT_FALSE(connect_to_service_result);
            run_loop.Quit();
          }));
  run_loop.Run();
}
#else
TEST_F(AdapterTest, TestConnectToServiceInsecurely_UnknownDevice) {
  adapter_->AllowConnectionsForUuid(device::BluetoothUUID(kServiceId));

  base::RunLoop run_loop;
  adapter_->ConnectToServiceInsecurely(
      kUnknownDeviceAddress, device::BluetoothUUID(kServiceId),
      /*should_unbond_on_error=*/false,
      base::BindLambdaForTesting(
          [&](mojom::ConnectToServiceResultPtr connect_to_service_result) {
            EXPECT_FALSE(connect_to_service_result);
            run_loop.Quit();
          }));
  run_loop.Run();
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(AdapterTest, TestConnectToServiceInsecurely_HalfPaired) {
  EXPECT_CALL(*mock_known_bluetooth_device_, IsBonded).WillOnce(Return(true));

  EXPECT_CALL(*mock_known_bluetooth_device_,
              ConnectToServiceInsecurely(_, _, _))
      .WillOnce(RunOnceCallback<2>("br-connection-canceled"));

  EXPECT_CALL(*mock_known_bluetooth_device_, Forget).Times(1);

  adapter_->AllowConnectionsForUuid(device::BluetoothUUID(kServiceId));

  adapter_->ConnectToServiceInsecurely(
      kKnownDeviceAddress, device::BluetoothUUID(kServiceId),
      /*should_unbond_on_error=*/true,
      base::BindLambdaForTesting(
          [&](mojom::ConnectToServiceResultPtr connect_to_service_result) {}));
}
#endif

TEST_F(AdapterTest, TestCreateRfcommServiceInsecurely_DisallowedUuid) {
  // Do not call Adapter::AllowConnectionsForUuid();

  base::RunLoop run_loop;
  adapter_->CreateRfcommServiceInsecurely(
      kServiceName, device::BluetoothUUID(kServiceId),
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<mojom::ServerSocket> pending_server_socket) {
            EXPECT_FALSE(pending_server_socket);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(AdapterTest, TestCreateRfcommServiceInsecurely_Error) {
  EXPECT_CALL(*mock_bluetooth_adapter_,
              CreateRfcommService(device::BluetoothUUID(kServiceId), _, _, _))
      .WillOnce(RunOnceCallback<3>("Error"));

  adapter_->AllowConnectionsForUuid(device::BluetoothUUID(kServiceId));

  base::RunLoop run_loop;
  adapter_->CreateRfcommServiceInsecurely(
      kServiceName, device::BluetoothUUID(kServiceId),
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<mojom::ServerSocket> pending_server_socket) {
            EXPECT_FALSE(pending_server_socket);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(AdapterTest, TestCreateRfcommServiceInsecurely_Success) {
  EXPECT_CALL(*mock_bluetooth_adapter_,
              CreateRfcommService(device::BluetoothUUID(kServiceId), _, _, _))
      .WillOnce(RunOnceCallback<2>(mock_bluetooth_socket_));

  adapter_->AllowConnectionsForUuid(device::BluetoothUUID(kServiceId));

  base::RunLoop run_loop;
  adapter_->CreateRfcommServiceInsecurely(
      kServiceName, device::BluetoothUUID(kServiceId),
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<mojom::ServerSocket> pending_server_socket) {
            EXPECT_TRUE(pending_server_socket);
            run_loop.Quit();
          }));
  run_loop.Run();
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(AdapterTest, TestMetricsOnShutdown_NoPendingConnects) {
  base::HistogramTester histogram_tester;
  adapter_.reset();

  EXPECT_EQ(0u,
            histogram_tester
                .GetAllSamples(
                    "Bluetooth.Mojo.PendingConnectAtShutdown.DurationWaiting")
                .size());
  histogram_tester.ExpectUniqueSample(
      "Bluetooth.Mojo.PendingConnectAtShutdown."
      "NumberOfServiceDiscoveriesInProgress",
      /*sample=*/0, /*expected_bucket_count=*/1);
}

TEST_F(AdapterTest, TestMetricsOnShutdown_PendingConnects) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_bluetooth_adapter_,
              ConnectDevice(kUnknownDeviceAddress, _, _, _))
      .WillOnce(RunOnceCallback<2>(mock_unknown_bluetooth_device_.get()));

  EXPECT_CALL(*mock_unknown_bluetooth_device_,
              IsGattServicesDiscoveryComplete())
      .WillRepeatedly(Return(false));

  adapter_->AllowConnectionsForUuid(device::BluetoothUUID(kServiceId));
  adapter_->ConnectToServiceInsecurely(
      kUnknownDeviceAddress, device::BluetoothUUID(kServiceId),
      /*should_unbond_on_error=*/false, base::DoNothing());
  base::RunLoop().RunUntilIdle();

  adapter_.reset();

  histogram_tester.ExpectUniqueSample(
      "Bluetooth.Mojo.PendingConnectAtShutdown."
      "NumberOfServiceDiscoveriesInProgress",
      /*sample=*/1, /*expected_bucket_count=*/1);
  EXPECT_EQ(1u, histogram_tester
                    .GetAllSamples("Bluetooth.Mojo.PendingConnectAtShutdown."
                                   "NumberOfServiceDiscoveriesInProgress")
                    .size());
}
#endif
}  // namespace bluetooth
