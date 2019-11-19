// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/bluetooth_test_cast.h"

#include "base/bind_helpers.h"
#include "base/task/post_task.h"
#include "chromecast/device/bluetooth/bluetooth_util.h"
#include "chromecast/device/bluetooth/le/mock_gatt_client_manager.h"
#include "chromecast/device/bluetooth/le/remote_device.h"
#include "device/bluetooth/cast/bluetooth_adapter_cast.h"

using ::testing::ByMove;
using ::testing::Return;

namespace device {

class BluetoothTestCast::GattClientManager
    : public chromecast::bluetooth::MockGattClientManager {
 public:
  GattClientManager() = default;
  ~GattClientManager() override = default;

  // chromecast::bluetooth::GattClientManager implementation:
  void GetDevice(
      const chromecast::bluetooth_v2_shlib::Addr& addr,
      base::OnceCallback<void(
          scoped_refptr<chromecast::bluetooth::RemoteDevice>)> cb) override {
    auto it = addr_to_remote_device_.find(addr);
    if (it != addr_to_remote_device_.end()) {
      std::move(cb).Run(it->second);
      return;
    }

    auto device =
        base::MakeRefCounted<chromecast::bluetooth::MockRemoteDevice>(addr);
    addr_to_remote_device_.emplace(addr, device);
    std::move(cb).Run(device);
  }

 private:
  std::map<chromecast::bluetooth_v2_shlib::Addr,
           scoped_refptr<chromecast::bluetooth::MockRemoteDevice>>
      addr_to_remote_device_;
};

BluetoothTestCast::BluetoothTestCast()
    : gatt_client_manager_(std::make_unique<GattClientManager>()) {
  ON_CALL(le_scan_manager_, RequestScan)
      .WillByDefault(Return(ByMove(
          std::unique_ptr<chromecast::bluetooth::LeScanManager::ScanHandle>(
              std::make_unique<chromecast::bluetooth::MockLeScanManager::
                                   MockScanHandle>()))));
}

BluetoothTestCast::~BluetoothTestCast() {
  // Destroy |discovery_sesions_| before adapter, which may hold references to
  // it.
  discovery_sessions_.clear();

  // Tear down adapter, which relies on members in the subclass.
  adapter_ = nullptr;
}

void BluetoothTestCast::InitWithFakeAdapter() {
  adapter_ =
      new BluetoothAdapterCast(gatt_client_manager_.get(), &le_scan_manager_);
  adapter_->SetPowered(true, base::DoNothing(), base::DoNothing());
}

bool BluetoothTestCast::PlatformSupportsLowEnergy() {
  return true;
}

BluetoothDevice* BluetoothTestCast::SimulateLowEnergyDevice(
    int device_ordinal) {
  if (device_ordinal > 7 || device_ordinal < 1)
    return nullptr;

  base::Optional<std::string> device_name = std::string(kTestDeviceName);
  std::string device_address = kTestDeviceAddress1;
  std::vector<std::string> service_uuids;
  std::map<std::string, std::vector<uint8_t>> service_data;
  std::map<uint16_t, std::vector<uint8_t>> manufacturer_data;

  switch (device_ordinal) {
    case 1:
      service_uuids.push_back(kTestUUIDGenericAccess);
      service_uuids.push_back(kTestUUIDGenericAttribute);
      service_data[kTestUUIDHeartRate] = {0x01};
      manufacturer_data[kTestManufacturerId] = {1, 2, 3, 4};
      break;
    case 2:
      service_uuids.push_back(kTestUUIDImmediateAlert);
      service_uuids.push_back(kTestUUIDLinkLoss);
      service_data[kTestUUIDHeartRate] = {};
      service_data[kTestUUIDImmediateAlert] = {0x00, 0x02};
      manufacturer_data[kTestManufacturerId] = {};
      break;
    case 3:
      device_name = std::string(kTestDeviceNameEmpty);
      break;
    case 4:
      device_name = std::string(kTestDeviceNameEmpty);
      device_address = kTestDeviceAddress2;
      break;
    case 5:
      device_name = base::nullopt;
      break;
    default:
      NOTREACHED();
  }
  UpdateAdapter(device_address, device_name, service_uuids, service_data,
                manufacturer_data);
  return adapter_->GetDevice(device_address);
}

void BluetoothTestCast::UpdateAdapter(
    const std::string& address,
    const base::Optional<std::string>& name,
    const std::vector<std::string>& service_uuids,
    const std::map<std::string, std::vector<uint8_t>>& service_data,
    const std::map<uint16_t, std::vector<uint8_t>>& manufacturer_data) {
  // Create a scan result with the desired values.
  chromecast::bluetooth::LeScanResult result;
  ASSERT_TRUE(chromecast::bluetooth::util::ParseAddr(address, &result.addr));
  if (name) {
    result.type_to_data[chromecast::bluetooth::LeScanResult::kGapCompleteName]
        .push_back(std::vector<uint8_t>(name->begin(), name->end()));
  }

  // Add service_uuids.
  std::vector<uint8_t> data;
  for (const auto& uuid_str : service_uuids) {
    chromecast::bluetooth_v2_shlib::Uuid uuid;
    ASSERT_TRUE(chromecast::bluetooth::util::ParseUuid(uuid_str, &uuid));
    data.insert(data.end(), uuid.rbegin(), uuid.rend());
  }
  result
      .type_to_data
          [chromecast::bluetooth::LeScanResult::kGapComplete128BitServiceUuids]
      .push_back(std::move(data));

  // Add service data.
  for (const auto& it : service_data) {
    chromecast::bluetooth_v2_shlib::Uuid uuid;
    ASSERT_TRUE(chromecast::bluetooth::util::ParseUuid(it.first, &uuid));
    std::vector<uint8_t> data(uuid.rbegin(), uuid.rend());
    data.insert(data.end(), it.second.begin(), it.second.end());
    result
        .type_to_data
            [chromecast::bluetooth::LeScanResult::kGapServicesData128bit]
        .push_back(std::move(data));
  }

  // Add manufacturer data.
  for (const auto& it : manufacturer_data) {
    std::vector<uint8_t> data({(it.first & 0xFF), ((it.first >> 8) & 0xFF)});
    data.insert(data.end(), it.second.begin(), it.second.end());
    result
        .type_to_data[chromecast::bluetooth::LeScanResult::kGapManufacturerData]
        .push_back(std::move(data));
  }

  // Update the adapter with the ScanResult.
  le_scan_manager_.observer_->OnNewScanResult(result);
  task_environment_.RunUntilIdle();
}

}  // namespace device
