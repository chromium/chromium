// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/hid/fake_input_service_linux.h"

namespace device {

FakeInputServiceLinux::FakeInputServiceLinux() {}

FakeInputServiceLinux::~FakeInputServiceLinux() {}

// mojom::InputDeviceManager implementation:
void FakeInputServiceLinux::GetDevicesAndSetClient(
    mojo::PendingAssociatedRemote<mojom::InputDeviceManagerClient> client,
    GetDevicesCallback callback) {
  GetDevices(std::move(callback));

  if (!client.is_valid())
    return;

  clients_.Add(std::move(client));
}

void FakeInputServiceLinux::GetDevices(GetDevicesCallback callback) {
  std::vector<mojom::InputDeviceInfoPtr> devices;
  for (auto& device : devices_)
    devices.push_back(device.second->Clone());

  std::move(callback).Run(std::move(devices));
}

void FakeInputServiceLinux::Bind(
    mojo::PendingReceiver<mojom::InputDeviceManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void FakeInputServiceLinux::AddDevice(mojom::InputDeviceInfoPtr info) {
  auto* device_info = info.get();
  for (auto& client : clients_)
    client->InputDeviceAdded(device_info->Clone());

  devices_[info->id] = std::move(info);
}

void FakeInputServiceLinux::RemoveDevice(const std::string& id) {
  devices_.erase(id);

  for (auto& client : clients_)
    client->InputDeviceRemoved(id);
}

}  // namespace device
