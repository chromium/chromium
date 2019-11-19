// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/hid/fake_hid_manager.h"

#include <memory>
#include <utility>

#include "base/guid.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace device {

FakeHidConnection::FakeHidConnection(mojom::HidDeviceInfoPtr device)
    : device_(std::move(device)) {}

FakeHidConnection::~FakeHidConnection() = default;

// mojom::HidConnection implementation:
void FakeHidConnection::Read(ReadCallback callback) {
  const char kResult[] = "This is a HID input report.";
  uint8_t report_id = device_->has_report_id ? 1 : 0;

  std::vector<uint8_t> buffer(kResult, kResult + sizeof(kResult) - 1);

  std::move(callback).Run(true, report_id, buffer);
}

void FakeHidConnection::Write(uint8_t report_id,
                              const std::vector<uint8_t>& buffer,
                              WriteCallback callback) {
  const char kExpected[] = "o-report";  // 8 bytes
  if (buffer.size() != sizeof(kExpected) - 1) {
    std::move(callback).Run(false);
    return;
  }

  int expected_report_id = device_->has_report_id ? 1 : 0;
  if (report_id != expected_report_id) {
    std::move(callback).Run(false);
    return;
  }

  if (memcmp(buffer.data(), kExpected, sizeof(kExpected) - 1) != 0) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(true);
}

void FakeHidConnection::GetFeatureReport(uint8_t report_id,
                                         GetFeatureReportCallback callback) {
  uint8_t expected_report_id = device_->has_report_id ? 1 : 0;
  if (report_id != expected_report_id) {
    std::move(callback).Run(false, base::nullopt);
    return;
  }

  const char kResult[] = "This is a HID feature report.";
  std::vector<uint8_t> buffer;
  if (device_->has_report_id)
    buffer.push_back(report_id);
  buffer.insert(buffer.end(), kResult, kResult + sizeof(kResult) - 1);

  std::move(callback).Run(true, buffer);
}

void FakeHidConnection::SendFeatureReport(uint8_t report_id,
                                          const std::vector<uint8_t>& buffer,
                                          SendFeatureReportCallback callback) {
  const char kExpected[] = "The app is setting this HID feature report.";
  if (buffer.size() != sizeof(kExpected) - 1) {
    std::move(callback).Run(false);
    return;
  }

  int expected_report_id = device_->has_report_id ? 1 : 0;
  if (report_id != expected_report_id) {
    std::move(callback).Run(false);
    return;
  }

  if (memcmp(buffer.data(), kExpected, sizeof(kExpected) - 1) != 0) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(true);
}

// Implementation of FakeHidManager.
FakeHidManager::FakeHidManager() {}
FakeHidManager::~FakeHidManager() = default;

void FakeHidManager::Bind(mojo::PendingReceiver<mojom::HidManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

// mojom::HidManager implementation:
void FakeHidManager::GetDevicesAndSetClient(
    mojo::PendingAssociatedRemote<mojom::HidManagerClient> client,
    GetDevicesCallback callback) {
  GetDevices(std::move(callback));

  clients_.Add(std::move(client));
}

void FakeHidManager::GetDevices(GetDevicesCallback callback) {
  std::vector<mojom::HidDeviceInfoPtr> device_list;
  for (auto& map_entry : devices_)
    device_list.push_back(map_entry.second->Clone());

  std::move(callback).Run(std::move(device_list));
}

void FakeHidManager::Connect(
    const std::string& device_guid,
    mojo::PendingRemote<mojom::HidConnectionClient> connection_client,
    mojo::PendingRemote<mojom::HidConnectionWatcher> watcher,
    ConnectCallback callback) {
  if (!base::Contains(devices_, device_guid)) {
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  mojo::PendingRemote<mojom::HidConnection> connection;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<FakeHidConnection>(devices_[device_guid]->Clone()),
      connection.InitWithNewPipeAndPassReceiver());
  std::move(callback).Run(std::move(connection));
}

mojom::HidDeviceInfoPtr FakeHidManager::CreateAndAddDevice(
    uint16_t vendor_id,
    uint16_t product_id,
    const std::string& product_name,
    const std::string& serial_number,
    mojom::HidBusType bus_type) {
  mojom::HidDeviceInfoPtr device = mojom::HidDeviceInfo::New();
  device->guid = base::GenerateGUID();
  device->vendor_id = vendor_id;
  device->product_id = product_id;
  device->product_name = product_name;
  device->serial_number = serial_number;
  device->bus_type = bus_type;
  AddDevice(device.Clone());
  return device;
}

mojom::HidDeviceInfoPtr FakeHidManager::CreateAndAddDeviceWithTopLevelUsage(
    uint16_t vendor_id,
    uint16_t product_id,
    const std::string& product_name,
    const std::string& serial_number,
    mojom::HidBusType bus_type,
    uint16_t usage_page,
    uint16_t usage) {
  mojom::HidDeviceInfoPtr device = mojom::HidDeviceInfo::New();
  device->guid = base::GenerateGUID();
  device->vendor_id = vendor_id;
  device->product_id = product_id;
  device->product_name = product_name;
  device->serial_number = serial_number;
  device->bus_type = bus_type;

  std::vector<mojom::HidReportDescriptionPtr> input_reports;
  std::vector<mojom::HidReportDescriptionPtr> output_reports;
  std::vector<mojom::HidReportDescriptionPtr> feature_reports;
  std::vector<mojom::HidCollectionInfoPtr> children;
  device->collections.push_back(mojom::HidCollectionInfo::New(
      mojom::HidUsageAndPage::New(usage, usage_page), std::vector<uint8_t>(),
      mojom::kHIDCollectionTypeApplication, std::move(input_reports),
      std::move(output_reports), std::move(feature_reports),
      std::move(children)));
  AddDevice(device.Clone());
  return device;
}

void FakeHidManager::AddDevice(mojom::HidDeviceInfoPtr device) {
  std::string guid = device->guid;
  devices_[guid] = std::move(device);

  mojom::HidDeviceInfo* device_info = devices_[guid].get();
  for (auto& client : clients_)
    client->DeviceAdded(device_info->Clone());
}

void FakeHidManager::RemoveDevice(const std::string& guid) {
  if (base::Contains(devices_, guid)) {
    mojom::HidDeviceInfo* device_info = devices_[guid].get();
    for (auto& client : clients_)
      client->DeviceRemoved(device_info->Clone());
    devices_.erase(guid);
  }
}

}  // namespace device
