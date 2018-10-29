// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/public/cpp/fake_usb_device_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/usb/public/cpp/fake_usb_device.h"
#include "device/usb/public/cpp/usb_utils.h"

namespace device {

FakeUsbDeviceManager::FakeUsbDeviceManager() : weak_factory_(this) {}

FakeUsbDeviceManager::~FakeUsbDeviceManager() {}

// mojom::UsbDeviceManager implementation:
void FakeUsbDeviceManager::GetDevices(mojom::UsbEnumerationOptionsPtr options,
                                      GetDevicesCallback callback) {
  std::vector<mojom::UsbDeviceFilterPtr> filters;
  if (options)
    filters.swap(options->filters);

  std::vector<mojom::UsbDeviceInfoPtr> device_infos;
  for (const auto& it : devices_) {
    mojom::UsbDeviceInfoPtr device_info = it.second->GetDeviceInfo();
    if (UsbDeviceFilterMatchesAny(filters, *device_info)) {
      device_infos.push_back(std::move(device_info));
    }
  }

  std::move(callback).Run(std::move(device_infos));
}

void FakeUsbDeviceManager::GetDevice(const std::string& guid,
                                     mojom::UsbDeviceRequest device_request,
                                     mojom::UsbDeviceClientPtr device_client) {
  auto it = devices_.find(guid);
  if (it == devices_.end())
    return;

  FakeUsbDevice::Create(it->second, std::move(device_request),
                        std::move(device_client));
}

void FakeUsbDeviceManager::SetClient(
    mojom::UsbDeviceManagerClientAssociatedPtrInfo client) {
  DCHECK(client);
  mojom::UsbDeviceManagerClientAssociatedPtr client_ptr;
  client_ptr.Bind(std::move(client));
  clients_.AddPtr(std::move(client_ptr));
}

void FakeUsbDeviceManager::AddBinding(mojom::UsbDeviceManagerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

mojom::UsbDeviceInfoPtr FakeUsbDeviceManager::AddDevice(
    scoped_refptr<FakeUsbDeviceInfo> device) {
  DCHECK(device);
  DCHECK(!base::ContainsKey(devices_, device->guid()));
  devices_[device->guid()] = device;
  auto device_info = device->GetDeviceInfo();

  // Notify all the clients.
  clients_.ForAllPtrs(
      [&device_info](device::mojom::UsbDeviceManagerClient* client) {
        client->OnDeviceAdded(device_info->Clone());
      });
  return device_info;
}

void FakeUsbDeviceManager::RemoveDevice(
    scoped_refptr<FakeUsbDeviceInfo> device) {
  DCHECK(device);
  DCHECK(base::ContainsKey(devices_, device->guid()));
  auto device_info = device->GetDeviceInfo();
  devices_.erase(device->guid());

  // Notify all the clients
  clients_.ForAllPtrs(
      [&device_info](device::mojom::UsbDeviceManagerClient* client) {
        client->OnDeviceRemoved(device_info->Clone());
      });

  device->NotifyDeviceRemoved();
}

void FakeUsbDeviceManager::RemoveDevice(const std::string& guid) {
  DCHECK(ContainsKey(devices_, guid));
  RemoveDevice(devices_[guid]);
}

}  // namespace device
