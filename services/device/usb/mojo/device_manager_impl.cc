// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/mojo/device_manager_impl.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"
#include "services/device/usb/mojo/device_impl.h"
#include "services/device/usb/usb_device.h"
#include "services/device/usb/usb_service.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/dbus/permission_broker/permission_broker_client.h"  // nogncheck
#include "services/device/usb/usb_device_linux.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace device::usb {

DeviceManagerImpl::DeviceManagerImpl()
    : DeviceManagerImpl(UsbService::Create()) {}

DeviceManagerImpl::DeviceManagerImpl(std::unique_ptr<UsbService> usb_service)
    : usb_service_(std::move(usb_service)) {
  if (usb_service_)
    observation_.Observe(usb_service_.get());
}

DeviceManagerImpl::~DeviceManagerImpl() = default;

void DeviceManagerImpl::AddReceiver(
    mojo::PendingReceiver<mojom::UsbDeviceManager> receiver) {
  if (usb_service_)
    receivers_.Add(this, std::move(receiver));
}

void DeviceManagerImpl::EnumerateDevicesAndSetClient(
    mojo::PendingAssociatedRemote<mojom::UsbDeviceManagerClient> client,
    EnumerateDevicesAndSetClientCallback callback) {
  usb_service_->GetDevices(base::BindOnce(
      &DeviceManagerImpl::OnGetDevices, weak_factory_.GetWeakPtr(),
      /*options=*/nullptr, std::move(client), std::move(callback)));
}

void DeviceManagerImpl::GetDevices(mojom::UsbEnumerationOptionsPtr options,
                                   GetDevicesCallback callback) {
  usb_service_->GetDevices(base::BindOnce(
      &DeviceManagerImpl::OnGetDevices, weak_factory_.GetWeakPtr(),
      std::move(options), mojo::NullAssociatedRemote(), std::move(callback)));
}

void DeviceManagerImpl::GetDevice(
    const std::string& guid,
    const std::vector<uint8_t>& blocked_interface_classes,
    mojo::PendingReceiver<mojom::UsbDevice> device_receiver,
    mojo::PendingRemote<mojom::UsbDeviceClient> device_client) {
  return GetDeviceInternal(guid, std::move(device_receiver),
                           std::move(device_client), blocked_interface_classes,
                           /*allow_security_key_requests=*/false);
}

void DeviceManagerImpl::GetSecurityKeyDevice(
    const std::string& guid,
    mojo::PendingReceiver<mojom::UsbDevice> device_receiver,
    mojo::PendingRemote<mojom::UsbDeviceClient> device_client) {
  return GetDeviceInternal(guid, std::move(device_receiver),
                           std::move(device_client),
                           /*blocked_interface_classes=*/{},
                           /*allow_security_key_requests=*/true);
}

#if BUILDFLAG(IS_ANDROID)
void DeviceManagerImpl::RefreshDeviceInfo(const std::string& guid,
                                          RefreshDeviceInfoCallback callback) {
  scoped_refptr<UsbDevice> device = usb_service_->GetDevice(guid);
  if (!device) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (device->permission_granted()) {
    std::move(callback).Run(device->device_info().Clone());
    return;
  }

  device->RequestPermission(
      base::BindOnce(&DeviceManagerImpl::OnPermissionGrantedToRefresh,
                     weak_factory_.GetWeakPtr(), device, std::move(callback)));
}

void DeviceManagerImpl::OnPermissionGrantedToRefresh(
    scoped_refptr<UsbDevice> device,
    RefreshDeviceInfoCallback callback,
    bool granted) {
  DCHECK_EQ(granted, device->permission_granted());
  if (!device->permission_granted()) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(device->device_info().Clone());
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
void DeviceManagerImpl::CheckAccess(const std::string& guid,
                                    CheckAccessCallback callback) {
  scoped_refptr<UsbDevice> device = usb_service_->GetDevice(guid);
  if (device) {
    device->CheckUsbAccess(std::move(callback));
  } else {
    LOG(ERROR) << "Was asked to check access to non-existent USB device: "
               << guid;
    std::move(callback).Run(false);
  }
}

void DeviceManagerImpl::OpenFileDescriptor(
    const std::string& guid,
    uint32_t drop_privileges_mask,
    mojo::PlatformHandle lifeline_fd,
    OpenFileDescriptorCallback callback) {
  scoped_refptr<UsbDevice> device = usb_service_->GetDevice(guid);
  if (!device) {
    LOG(ERROR) << "Was asked to open non-existent USB device: " << guid;
    std::move(callback).Run(base::File());
  } else {
    auto split_callback = base::SplitOnceCallback(std::move(callback));
    auto devpath =
        static_cast<device::UsbDeviceLinux*>(device.get())->device_path();

    // The |lifeline_fd| passed through D-Bus gets is duped, so we need to close
    // our original.
    chromeos::PermissionBrokerClient::Get()->ClaimDevicePath(
        devpath, drop_privileges_mask, lifeline_fd.GetFD().get(),
        base::BindOnce(&DeviceManagerImpl::OnOpenFileDescriptor,
                       weak_factory_.GetWeakPtr(),
                       std::move(split_callback.first)),
        base::BindOnce(&DeviceManagerImpl::OnOpenFileDescriptorError,
                       weak_factory_.GetWeakPtr(),
                       std::move(split_callback.second)));
  }
}

void DeviceManagerImpl::OnOpenFileDescriptor(
    OpenFileDescriptorCallback callback,
    base::ScopedFD fd) {
  std::move(callback).Run(base::File(std::move(fd)));
}

void DeviceManagerImpl::OnOpenFileDescriptorError(
    OpenFileDescriptorCallback callback,
    const std::string& error_name,
    const std::string& message) {
  LOG(ERROR) << "Failed to open USB device file: " << error_name << " "
             << message;
  std::move(callback).Run(base::File());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void DeviceManagerImpl::SetClient(
    mojo::PendingAssociatedRemote<mojom::UsbDeviceManagerClient> client) {
  DCHECK(client);
  clients_.Add(std::move(client));
}

void DeviceManagerImpl::OnGetDevices(
    mojom::UsbEnumerationOptionsPtr options,
    mojo::PendingAssociatedRemote<mojom::UsbDeviceManagerClient> client,
    GetDevicesCallback callback,
    const std::vector<scoped_refptr<UsbDevice>>& devices) {
  std::vector<mojom::UsbDeviceFilterPtr> filters;
  if (options)
    filters.swap(options->filters);

  std::vector<mojom::UsbDeviceInfoPtr> device_infos;
  for (const auto& device : devices) {
    if (UsbDeviceFilterMatchesAny(filters, device->device_info())) {
      device_infos.push_back(device->device_info().Clone());
    }
  }

  std::move(callback).Run(std::move(device_infos));

  if (client)
    SetClient(std::move(client));
}

void DeviceManagerImpl::OnDeviceAdded(scoped_refptr<UsbDevice> device) {
  for (auto& client : clients_)
    client->OnDeviceAdded(device->device_info().Clone());
}

void DeviceManagerImpl::OnDeviceRemoved(scoped_refptr<UsbDevice> device) {
  for (auto& client : clients_)
    client->OnDeviceRemoved(device->device_info().Clone());
}

void DeviceManagerImpl::WillDestroyUsbService() {
  observation_.Reset();
  usb_service_ = nullptr;

  // Close all the connections.
  receivers_.Clear();
  clients_.Clear();
}

void DeviceManagerImpl::GetDeviceInternal(
    const std::string& guid,
    mojo::PendingReceiver<mojom::UsbDevice> device_receiver,
    mojo::PendingRemote<mojom::UsbDeviceClient> device_client,
    base::span<const uint8_t> blocked_interface_classes,
    bool allow_security_key_requests) {
  scoped_refptr<UsbDevice> device = usb_service_->GetDevice(guid);
  if (!device)
    return;

  DeviceImpl::Create(std::move(device), std::move(device_receiver),
                     std::move(device_client), blocked_interface_classes,
                     allow_security_key_requests);
}

}  // namespace device::usb
