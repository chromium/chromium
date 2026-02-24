// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/cable_mock_bluetooth_adapter.h"

#include <algorithm>
#include <ranges>

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/fido/cable/fido_ble_uuids.h"

using ::testing::_;
using ::testing::NiceMock;

namespace device::cablev2 {

namespace {

// Below constants are used to construct MockBluetoothDevice for testing.
constexpr char kTestBleDeviceAddress[] = "11:12:13:14:15:16";
constexpr char kTestBleDeviceName[] = "test_cable_device";

std::unique_ptr<MockBluetoothDevice> CreateTestBluetoothDevice() {
  return std::make_unique<testing::NiceMock<MockBluetoothDevice>>(
      /*adapter=*/nullptr, /*bluetooth_class=*/0, kTestBleDeviceName,
      kTestBleDeviceAddress, /*paired=*/true, /*connected=*/true);
}

}  // namespace

// static
scoped_refptr<CableMockBluetoothAdapter>
CableMockBluetoothAdapter::MakePoweredOn() {
  auto mock_adapter =
      base::MakeRefCounted<::testing::NiceMock<CableMockBluetoothAdapter>>();
  EXPECT_CALL(*mock_adapter, IsPresent())
      .WillRepeatedly(::testing::Return(true));
  EXPECT_CALL(*mock_adapter, IsPowered())
      .WillRepeatedly(::testing::Return(true));
  return mock_adapter;
}

// static
scoped_refptr<CableMockBluetoothAdapter>
CableMockBluetoothAdapter::MakePoweredOff() {
  auto mock_adapter =
      base::MakeRefCounted<::testing::NiceMock<CableMockBluetoothAdapter>>();
  EXPECT_CALL(*mock_adapter, IsPresent())
      .WillRepeatedly(::testing::Return(true));
  EXPECT_CALL(*mock_adapter, IsPowered())
      .WillRepeatedly(::testing::Return(false));
  return mock_adapter;
}

// static
scoped_refptr<CableMockBluetoothAdapter>
CableMockBluetoothAdapter::MakeNotPresent() {
  auto mock_adapter =
      base::MakeRefCounted<::testing::NiceMock<CableMockBluetoothAdapter>>();
  EXPECT_CALL(*mock_adapter, IsPresent())
      .WillRepeatedly(::testing::Return(false));
  return mock_adapter;
}

// static
scoped_refptr<CableMockBluetoothAdapter>
CableMockBluetoothAdapter::MakeWithUndeterminedPermission() {
  auto mock_adapter =
      base::MakeRefCounted<::testing::NiceMock<CableMockBluetoothAdapter>>();
  EXPECT_CALL(*mock_adapter, IsPresent())
      .WillRepeatedly(::testing::Return(true));
  EXPECT_CALL(*mock_adapter, GetOsPermissionStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::kUndetermined));
  return mock_adapter;
}

void CableMockBluetoothAdapter::ExpectDiscoveryWithScanCallback() {
  EXPECT_CALL(*this, StartScanWithFilter_(_, _))
      .WillOnce(::testing::WithArg<1>([](auto& callback) {
        std::move(callback).Run(
            false, device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
      }));
}

void CableMockBluetoothAdapter::ExpectDiscoveryWithScanCallback(
    const std::array<uint8_t, kAdvertSize> v2_advert) {
  EXPECT_CALL(*this, StartScanWithFilter_(_, _))
      .WillOnce(::testing::WithArg<1>([this, v2_advert](auto& callback) {
        std::move(callback).Run(
            false, device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
        AddNewTestBluetoothDevice(v2_advert);
      }));
}

#if BUILDFLAG(IS_CHROMEOS)
void CableMockBluetoothAdapter::ExpectLEScan(
    const std::array<uint8_t, kAdvertSize> v2_advert) {
  EXPECT_CALL(*this, StartLowEnergyScanSession(_, _))
      .WillOnce(
          [this, v2_advert](
              std::unique_ptr<BluetoothLowEnergyScanFilter> filter,
              base::WeakPtr<BluetoothLowEnergyScanSession::Delegate> delegate) {
            EXPECT_TRUE(filter);
            delegate->OnSessionStarted(/*scan_session=*/nullptr,
                                       /*error_code=*/std::nullopt);
            auto* device = CreateNewTestBluetoothDevice(v2_advert);
            delegate->OnDeviceFound(/*scan_session=*/nullptr, device);
            return nullptr;
          });
}
#endif  // BUILDFLAG(IS_CHROMEOS)

BluetoothDevice* CableMockBluetoothAdapter::CreateNewTestBluetoothDevice(
    base::span<const uint8_t, kAdvertSize> v2_advert) {
  auto mock_device = CreateTestBluetoothDevice();

  std::vector<uint8_t> service_data(v2_advert.begin(), v2_advert.end());
  BluetoothDevice::ServiceDataMap service_data_map;
  service_data_map.emplace(kGoogleCableUUID128, std::move(service_data));

  mock_device->UpdateAdvertisementData(
      1 /* rssi */, std::nullopt /* flags */, BluetoothDevice::UUIDList(),
      std::nullopt /* tx_power */, std::move(service_data_map),
      BluetoothDevice::ManufacturerDataMap());

  auto* mock_device_ptr = mock_device.get();
  AddMockDevice(std::move(mock_device));

  return mock_device_ptr;
}

void CableMockBluetoothAdapter::AddNewTestBluetoothDevice(
    base::span<const uint8_t, kAdvertSize> v2_advert) {
  auto* device = CreateNewTestBluetoothDevice(v2_advert);
  for (auto& observer : GetObservers()) {
    observer.DeviceAdded(this, device);
  }
}

CableMockBluetoothAdapter::CableMockBluetoothAdapter() {
  EXPECT_CALL(*this, StopScan(_))
      .WillRepeatedly(::testing::WithArg<0>([](auto callback) {
        std::move(callback).Run(
            false, device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
      }));
}

CableMockBluetoothAdapter::~CableMockBluetoothAdapter() = default;

}  // namespace device::cablev2
