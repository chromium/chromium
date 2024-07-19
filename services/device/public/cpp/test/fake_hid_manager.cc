// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/public/cpp/test/fake_hid_manager.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/uuid.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/device/public/cpp/hid/hid_blocklist.h"

namespace device {

namespace {

bool IsFidoReport(uint8_t report_id, const mojom::HidDeviceInfo& device_info) {
  for (const auto& collection : device_info.collections) {
    if (collection->usage->usage_page != mojom::kPageFido)
      continue;

    for (const auto& report : collection->input_reports) {
      if (report->report_id == report_id)
        return true;
    }
    for (const auto& report : collection->output_reports) {
      if (report->report_id == report_id)
        return true;
    }
    for (const auto& report : collection->feature_reports) {
      if (report->report_id == report_id)
        return true;
    }
  }
  return false;
}

}  // namespace

FakeHidConnection::FakeHidConnection(
    mojom::HidDeviceInfoPtr device,
    mojo::PendingReceiver<mojom::HidConnection> receiver,
    mojo::PendingRemote<mojom::HidConnectionClient> connection_client,
    mojo::PendingRemote<mojom::HidConnectionWatcher> watcher,
    bool allow_fido_reports)
    : receiver_(this, std::move(receiver)),
      device_(std::move(device)),
      watcher_(std::move(watcher)),
      allow_fido_reports_(allow_fido_reports) {
  receiver_.set_disconnect_handler(base::BindOnce(
      [](FakeHidConnection* self) { delete self; }, base::Unretained(this)));
  if (watcher_) {
    watcher_.set_disconnect_handler(base::BindOnce(
        [](FakeHidConnection* self) { delete self; }, base::Unretained(this)));
  }
  if (connection_client)
    client_.Bind(std::move(connection_client));
}

FakeHidConnection::~FakeHidConnection() = default;

// mojom::HidConnection implementation:
void FakeHidConnection::Read(ReadCallback callback) {
  const char kResult[] = "This is a HID input report.";
  uint8_t report_id = device_->has_report_id ? 1 : 0;

  if (!allow_fido_reports_ && IsFidoReport(report_id, *device_)) {
    std::move(callback).Run(false, 0, std::nullopt);
    return;
  }

  std::vector<uint8_t> buffer(kResult, kResult + sizeof(kResult) - 1);

  std::move(callback).Run(true, report_id, buffer);
}

void FakeHidConnection::Write(uint8_t report_id,
                              const std::vector<uint8_t>& buffer,
                              WriteCallback callback) {
  if (!allow_fido_reports_ && IsFidoReport(report_id, *device_)) {
    std::move(callback).Run(false);
    return;
  }

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
  if (!allow_fido_reports_ && IsFidoReport(report_id, *device_)) {
    std::move(callback).Run(false, std::nullopt);
    return;
  }

  uint8_t expected_report_id = device_->has_report_id ? 1 : 0;
  if (report_id != expected_report_id) {
    std::move(callback).Run(false, std::nullopt);
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
  if (!allow_fido_reports_ && IsFidoReport(report_id, *device_)) {
    std::move(callback).Run(false);
    return;
  }

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
FakeHidManager::FakeHidManager() = default;
FakeHidManager::~FakeHidManager() = default;

void FakeHidManager::Bind(mojo::PendingReceiver<mojom::HidManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

// mojom::HidManager implementation:
void FakeHidManager::AddReceiver(
    mojo::PendingReceiver<mojom::HidManager> receiver) {
  Bind(std::move(receiver));
}

void FakeHidManager::GetDevicesAndSetClient(
    mojo::PendingAssociatedRemote<mojom::HidManagerClient> client,
    GetDevicesCallback callback) {
  GetDevices(std::move(callback));

  if (!client.is_valid())
    return;

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
    bool allow_protected_reports,
    bool allow_fido_reports,
    ConnectCallback callback) {
  if (!base::Contains(devices_, device_guid)) {
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  mojo::PendingRemote<mojom::HidConnection> connection;
  // FakeHidConnection is self-owned.
  new FakeHidConnection(devices_[device_guid]->Clone(),
                        connection.InitWithNewPipeAndPassReceiver(),
                        std::move(connection_client), std::move(watcher),
                        allow_fido_reports);
  std::move(callback).Run(std::move(connection));
}

mojom::HidDeviceInfoPtr FakeHidManager::CreateAndAddDevice(
    const std::string& physical_device_id,
    uint16_t vendor_id,
    uint16_t product_id,
    const std::string& product_name,
    const std::string& serial_number,
    mojom::HidBusType bus_type) {
  return CreateAndAddDeviceWithTopLevelUsage(
      physical_device_id, vendor_id, product_id, product_name, serial_number,
      bus_type, /*usage_page=*/0xff00,
      /*usage=*/0x0001);
}

mojom::HidDeviceInfoPtr FakeHidManager::CreateAndAddDeviceWithTopLevelUsage(
    const std::string& physical_device_id,
    uint16_t vendor_id,
    uint16_t product_id,
    const std::string& product_name,
    const std::string& serial_number,
    mojom::HidBusType bus_type,
    uint16_t usage_page,
    uint16_t usage) {
  auto collection = mojom::HidCollectionInfo::New();
  collection->usage = mojom::HidUsageAndPage::New(usage, usage_page);
  collection->collection_type = mojom::kHIDCollectionTypeApplication;
  collection->input_reports.push_back(mojom::HidReportDescription::New());

  auto device = mojom::HidDeviceInfo::New();
  device->guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  device->physical_device_id = physical_device_id;
  device->vendor_id = vendor_id;
  device->product_id = product_id;
  device->product_name = product_name;
  device->serial_number = serial_number;
  device->bus_type = bus_type;
  device->collections.push_back(std::move(collection));
  device->protected_input_report_ids =
      HidBlocklist::Get().GetProtectedReportIds(HidBlocklist::kReportTypeInput,
                                                vendor_id, product_id,
                                                device->collections);
  device->protected_output_report_ids =
      HidBlocklist::Get().GetProtectedReportIds(HidBlocklist::kReportTypeOutput,
                                                vendor_id, product_id,
                                                device->collections);
  device->protected_feature_report_ids =
      HidBlocklist::Get().GetProtectedReportIds(
          HidBlocklist::kReportTypeFeature, vendor_id, product_id,
          device->collections);
  device->is_excluded_by_blocklist =
      HidBlocklist::Get().IsVendorProductBlocked(vendor_id, product_id);
  AddDevice(device.Clone());
  return device;
}

void FakeHidManager::AddDevice(mojom::HidDeviceInfoPtr device) {
  std::string guid = device->guid;
  DCHECK(!base::Contains(devices_, guid));
  devices_[guid] = std::move(device);

  const mojom::HidDeviceInfoPtr& device_info = devices_[guid];
  for (auto& client : clients_)
    client->DeviceAdded(device_info->Clone());
}

void FakeHidManager::RemoveDevice(const std::string& guid) {
  if (base::Contains(devices_, guid)) {
    const mojom::HidDeviceInfoPtr& device_info = devices_[guid];
    for (auto& client : clients_)
      client->DeviceRemoved(device_info->Clone());
    devices_.erase(guid);
  }
}

void FakeHidManager::ChangeDevice(mojom::HidDeviceInfoPtr device) {
  DCHECK(base::Contains(devices_, device->guid));
  mojom::HidDeviceInfoPtr& device_info = devices_[device->guid];
  device_info = std::move(device);
  for (auto& client : clients_)
    client->DeviceChanged(device_info->Clone());
}

void FakeHidManager::SimulateConnectionError() {
  clients_.Clear();
  receivers_.Clear();
}

}  // namespace device
