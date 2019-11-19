// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/hid/fake_hid_impl_for_testing.h"

#include <utility>

#include "device/fido/fido_parsing_utils.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/mojom/connector.mojom.h"

namespace device {

namespace {

MATCHER_P(IsCtapHidCommand, expected_command, "") {
  return arg.size() >= 5 &&
         arg[4] == (0x80 | static_cast<uint8_t>(expected_command));
}

}  // namespace

MockFidoHidConnection::MockFidoHidConnection(
    device::mojom::HidDeviceInfoPtr device,
    mojo::PendingReceiver<device::mojom::HidConnection> receiver,
    std::array<uint8_t, 4> connection_channel_id)
    : receiver_(this, std::move(receiver)),
      device_(std::move(device)),
      connection_channel_id_(connection_channel_id) {}

MockFidoHidConnection::~MockFidoHidConnection() {}

void MockFidoHidConnection::Read(ReadCallback callback) {
  return ReadPtr(&callback);
}

void MockFidoHidConnection::Write(uint8_t report_id,
                                  const std::vector<uint8_t>& buffer,
                                  WriteCallback callback) {
  return WritePtr(report_id, buffer, &callback);
}

void MockFidoHidConnection::GetFeatureReport(
    uint8_t report_id,
    GetFeatureReportCallback callback) {
  NOTREACHED();
}

void MockFidoHidConnection::SendFeatureReport(
    uint8_t report_id,
    const std::vector<uint8_t>& buffer,
    SendFeatureReportCallback callback) {
  NOTREACHED();
}

void MockFidoHidConnection::SetNonce(base::span<uint8_t const> nonce) {
  nonce_ = std::vector<uint8_t>(nonce.begin(), nonce.end());
}

void MockFidoHidConnection::ExpectWriteHidInit() {
  EXPECT_CALL(*this, WritePtr(::testing::_,
                              IsCtapHidCommand(FidoHidDeviceCommand::kInit),
                              ::testing::_))
      .WillOnce(::testing::Invoke(
          [&](auto&&, const std::vector<uint8_t>& buffer,
              device::mojom::HidConnection::WriteCallback* cb) {
            ASSERT_EQ(64u, buffer.size());
            // First 7 bytes are 4 bytes of channel id, one byte representing
            // HID command, 2 bytes for payload length.
            SetNonce(base::make_span(buffer).subspan(7, 8));
            std::move(*cb).Run(true);
          }));
}

void MockFidoHidConnection::ExpectHidWriteWithCommand(
    FidoHidDeviceCommand cmd) {
  EXPECT_CALL(*this,
              WritePtr(::testing::_, IsCtapHidCommand(cmd), ::testing::_))
      .WillOnce(::testing::Invoke(
          [&](auto&&, const std::vector<uint8_t>& buffer,
              device::mojom::HidConnection::WriteCallback* cb) {
            std::move(*cb).Run(true);
          }));
}

bool FakeFidoHidConnection::mock_connection_error_ = false;

FakeFidoHidConnection::FakeFidoHidConnection(
    device::mojom::HidDeviceInfoPtr device)
    : device_(std::move(device)) {}

FakeFidoHidConnection::~FakeFidoHidConnection() = default;

void FakeFidoHidConnection::Read(ReadCallback callback) {
  std::vector<uint8_t> buffer = {'F', 'a', 'k', 'e', ' ', 'H', 'i', 'd'};
  std::move(callback).Run(true, 0, buffer);
}

void FakeFidoHidConnection::Write(uint8_t report_id,
                                  const std::vector<uint8_t>& buffer,
                                  WriteCallback callback) {
  if (mock_connection_error_) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(true);
}

void FakeFidoHidConnection::GetFeatureReport(
    uint8_t report_id,
    GetFeatureReportCallback callback) {
  NOTREACHED();
}

void FakeFidoHidConnection::SendFeatureReport(
    uint8_t report_id,
    const std::vector<uint8_t>& buffer,
    SendFeatureReportCallback callback) {
  NOTREACHED();
}

FakeFidoHidManager::FakeFidoHidManager() = default;

FakeFidoHidManager::~FakeFidoHidManager() = default;

void FakeFidoHidManager::AddReceiver(mojo::ScopedMessagePipeHandle handle) {
  receivers_.Add(this, mojo::PendingReceiver<device::mojom::HidManager>(
                           std::move(handle)));
}

void FakeFidoHidManager::AddReceiver2(
    mojo::PendingReceiver<device::mojom::HidManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void FakeFidoHidManager::AddFidoHidDevice(std::string guid) {
  auto c_info = device::mojom::HidCollectionInfo::New();
  c_info->usage = device::mojom::HidUsageAndPage::New(1, 0xf1d0);
  auto device = device::mojom::HidDeviceInfo::New();
  device->guid = std::move(guid);
  device->product_name = "Test Fido Device";
  device->serial_number = "123FIDO";
  device->bus_type = device::mojom::HidBusType::kHIDBusTypeUSB;
  device->collections.push_back(std::move(c_info));
  device->max_input_report_size = 64;
  device->max_output_report_size = 64;
  AddDevice(std::move(device));
}

void FakeFidoHidManager::GetDevicesAndSetClient(
    mojo::PendingAssociatedRemote<device::mojom::HidManagerClient> client,
    GetDevicesCallback callback) {
  GetDevices(std::move(callback));

  clients_.Add(std::move(client));
}

void FakeFidoHidManager::GetDevices(GetDevicesCallback callback) {
  std::vector<device::mojom::HidDeviceInfoPtr> device_list;
  for (auto& map_entry : devices_)
    device_list.push_back(map_entry.second->Clone());

  std::move(callback).Run(std::move(device_list));
}

void FakeFidoHidManager::Connect(
    const std::string& device_guid,
    mojo::PendingRemote<mojom::HidConnectionClient> connection_client,
    mojo::PendingRemote<mojom::HidConnectionWatcher> watcher,
    ConnectCallback callback) {
  auto device_it = devices_.find(device_guid);
  auto connection_it = connections_.find(device_guid);
  if (device_it == devices_.end() || connection_it == connections_.end()) {
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  std::move(callback).Run(std::move(connection_it->second));
}

void FakeFidoHidManager::AddDevice(device::mojom::HidDeviceInfoPtr device) {
  device::mojom::HidDeviceInfo* device_info = device.get();
  for (auto& client : clients_)
    client->DeviceAdded(device_info->Clone());

  devices_[device->guid] = std::move(device);
}

void FakeFidoHidManager::AddDeviceAndSetConnection(
    device::mojom::HidDeviceInfoPtr device,
    mojo::PendingRemote<device::mojom::HidConnection> connection) {
  connections_[device->guid] = std::move(connection);
  AddDevice(std::move(device));
}

void FakeFidoHidManager::RemoveDevice(const std::string device_guid) {
  auto it = devices_.find(device_guid);
  if (it == devices_.end())
    return;

  device::mojom::HidDeviceInfo* device_info = it->second.get();
  for (auto& client : clients_)
    client->DeviceRemoved(device_info->Clone());
  devices_.erase(it);
}

ScopedFakeFidoHidManager::ScopedFakeFidoHidManager() {
  mojo::PendingReceiver<service_manager::mojom::Connector> receiver;
  connector_ = service_manager::Connector::Create(&receiver);
  connector_->OverrideBinderForTesting(
      service_manager::ServiceFilter::ByName(device::mojom::kServiceName),
      device::mojom::HidManager::Name_,
      base::BindRepeating(&FakeFidoHidManager::AddReceiver,
                          base::Unretained(this)));
}

ScopedFakeFidoHidManager::~ScopedFakeFidoHidManager() = default;

}  // namespace device
