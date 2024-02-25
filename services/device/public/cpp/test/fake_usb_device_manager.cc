// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/fake_usb_device_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/device/public/cpp/test/fake_usb_device.h"
#include "services/device/public/cpp/test/mock_usb_mojo_device.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"

namespace device {

FakeUsbDeviceManager::FakeUsbDeviceManager() {}

FakeUsbDeviceManager::~FakeUsbDeviceManager() {
  RemoveAllDevices();
}

void FakeUsbDeviceManager::EnumerateDevicesAndSetClient(
    mojo::PendingAssociatedRemote<mojom::UsbDeviceManagerClient> client,
    EnumerateDevicesAndSetClientCallback callback) {
  GetDevices(nullptr, std::move(callback));
  SetClient(std::move(client));
}

// mojom::UsbDeviceManager implementation:
void FakeUsbDeviceManager::GetDevices(mojom::UsbEnumerationOptionsPtr options,
                                      GetDevicesCallback callback) {
  std::vector<mojom::UsbDeviceFilterPtr> filters;
  if (options)
    filters.swap(options->filters);

  std::vector<mojom::UsbDeviceInfoPtr> device_infos;
  for (const auto& it : devices_) {
    mojom::UsbDeviceInfoPtr device_info = it.second->GetDeviceInfo().Clone();
    if (UsbDeviceFilterMatchesAny(filters, *device_info)) {
      device_infos.push_back(std::move(device_info));
    }
  }

  std::move(callback).Run(std::move(device_infos));
}

void FakeUsbDeviceManager::GetDevice(
    const std::string& guid,
    const std::vector<uint8_t>& blocked_interface_classes,
    mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver,
    mojo::PendingRemote<mojom::UsbDeviceClient> device_client) {
  auto it = devices_.find(guid);
  if (it == devices_.end())
    return;

  FakeUsbDevice::Create(it->second, blocked_interface_classes,
                        std::move(device_receiver), std::move(device_client));
}

void FakeUsbDeviceManager::GetSecurityKeyDevice(
    const std::string& guid,
    mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver,
    mojo::PendingRemote<mojom::UsbDeviceClient> device_client) {
  return GetDevice(guid, /*blocked_interface_classes=*/{},
                   std::move(device_receiver), std::move(device_client));
}

#if BUILDFLAG(IS_ANDROID)
void FakeUsbDeviceManager::RefreshDeviceInfo(
    const std::string& guid,
    RefreshDeviceInfoCallback callback) {
  auto it = devices_.find(guid);
  if (it == devices_.end()) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(it->second->GetDeviceInfo().Clone());
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
void FakeUsbDeviceManager::CheckAccess(const std::string& guid,
                                       CheckAccessCallback callback) {
  std::move(callback).Run(true);
}

void FakeUsbDeviceManager::OpenFileDescriptor(
    const std::string& guid,
    uint32_t drop_privileges_mask,
    mojo::PlatformHandle lifeline_fd,
    OpenFileDescriptorCallback callback) {
  std::move(callback).Run(base::File(
      base::FilePath(FILE_PATH_LITERAL("/dev/null")),
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void FakeUsbDeviceManager::SetClient(
    mojo::PendingAssociatedRemote<mojom::UsbDeviceManagerClient> client) {
  DCHECK(client);
  clients_.Add(std::move(client));
}

void FakeUsbDeviceManager::AddReceiver(
    mojo::PendingReceiver<mojom::UsbDeviceManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

mojom::UsbDeviceInfoPtr FakeUsbDeviceManager::AddDevice(
    scoped_refptr<FakeUsbDeviceInfo> device) {
  DCHECK(device);
  DCHECK(!base::Contains(devices_, device->guid()));
  devices_[device->guid()] = device;
  auto device_info = device->GetDeviceInfo().Clone();

  // Notify all the clients.
  for (auto& client : clients_)
    client->OnDeviceAdded(device_info->Clone());
  return device_info;
}

void FakeUsbDeviceManager::RemoveDevice(
    scoped_refptr<FakeUsbDeviceInfo> device) {
  DCHECK(device);
  DCHECK(base::Contains(devices_, device->guid()));

  auto device_info = device->GetDeviceInfo().Clone();
  devices_.erase(device->guid());

  // Notify all the clients
  for (auto& client : clients_)
    client->OnDeviceRemoved(device_info->Clone());

  device->NotifyDeviceRemoved();
}

void FakeUsbDeviceManager::RemoveDevice(const std::string& guid) {
  DCHECK(base::Contains(devices_, guid));

  RemoveDevice(devices_[guid]);
}

void FakeUsbDeviceManager::RemoveAllDevices() {
  std::vector<scoped_refptr<FakeUsbDeviceInfo>> device_list;
  for (const auto& pair : devices_) {
    device_list.push_back(pair.second);
  }
  for (const auto& device : device_list) {
    RemoveDevice(device);
  }
}

const device::mojom::UsbDeviceInfo* FakeUsbDeviceManager::GetDeviceInfo(
    const std::string& guid) {
  if (!base::Contains(devices_, guid))
    return nullptr;

  return &devices_[guid]->GetDeviceInfo();
}

bool FakeUsbDeviceManager::SetMockForDevice(const std::string& guid,
                                            MockUsbMojoDevice* mock_device) {
  if (!base::Contains(devices_, guid))
    return false;

  devices_[guid]->SetMockDevice(mock_device);
  return true;
}

}  // namespace device
