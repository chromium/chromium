// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/mojo/device_impl.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <numeric>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/stl_util.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/usb/usb_descriptors.h"
#include "services/device/usb/usb_device.h"

namespace device {

using mojom::UsbControlTransferParamsPtr;
using mojom::UsbControlTransferRecipient;
using mojom::UsbIsochronousPacketPtr;
using mojom::UsbTransferDirection;
using mojom::UsbTransferStatus;

namespace usb {

namespace {

void OnTransferIn(mojom::UsbDevice::GenericTransferInCallback callback,
                  UsbTransferStatus status,
                  scoped_refptr<base::RefCountedBytes> buffer,
                  size_t buffer_size) {
  std::vector<uint8_t> data;
  if (buffer) {
    // TODO(rockot/reillyg): Take advantage of the ability to access the
    // std::vector<uint8_t> within a base::RefCountedBytes to move instead of
    // copy.
    data.resize(buffer_size);
    std::copy(buffer->front(), buffer->front() + buffer_size, data.begin());
  }

  std::move(callback).Run(mojo::ConvertTo<mojom::UsbTransferStatus>(status),
                          data);
}

void OnTransferOut(mojom::UsbDevice::GenericTransferOutCallback callback,
                   UsbTransferStatus status,
                   scoped_refptr<base::RefCountedBytes> buffer,
                   size_t buffer_size) {
  std::move(callback).Run(mojo::ConvertTo<mojom::UsbTransferStatus>(status));
}

void OnIsochronousTransferIn(
    mojom::UsbDevice::IsochronousTransferInCallback callback,
    scoped_refptr<base::RefCountedBytes> buffer,
    std::vector<UsbIsochronousPacketPtr> packets) {
  std::vector<uint8_t> data;
  if (buffer) {
    // TODO(rockot/reillyg): Take advantage of the ability to access the
    // std::vector<uint8_t> within a base::RefCountedBytes to move instead of
    // copy.
    uint32_t buffer_size = std::accumulate(
        packets.begin(), packets.end(), 0u,
        [](const uint32_t& a, const UsbIsochronousPacketPtr& packet) {
          return a + packet->length;
        });
    data.resize(buffer_size);
    std::copy(buffer->front(), buffer->front() + buffer_size, data.begin());
  }
  std::move(callback).Run(data, std::move(packets));
}

void OnIsochronousTransferOut(
    mojom::UsbDevice::IsochronousTransferOutCallback callback,
    scoped_refptr<base::RefCountedBytes> buffer,
    std::vector<UsbIsochronousPacketPtr> packets) {
  std::move(callback).Run(std::move(packets));
}

}  // namespace

// static
void DeviceImpl::Create(scoped_refptr<device::UsbDevice> device,
                        mojo::PendingReceiver<mojom::UsbDevice> receiver,
                        mojo::PendingRemote<mojom::UsbDeviceClient> client) {
  auto* device_impl = new DeviceImpl(std::move(device), std::move(client));
  device_impl->receiver_ = mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(device_impl), std::move(receiver));
}

DeviceImpl::~DeviceImpl() {
  CloseHandle();
}

DeviceImpl::DeviceImpl(scoped_refptr<device::UsbDevice> device,
                       mojo::PendingRemote<mojom::UsbDeviceClient> client)
    : device_(std::move(device)), observer_(this), client_(std::move(client)) {
  DCHECK(device_);
  observer_.Add(device_.get());

  if (client_) {
    client_.set_disconnect_handler(base::BindOnce(
        &DeviceImpl::OnClientConnectionError, weak_factory_.GetWeakPtr()));
  }
}

void DeviceImpl::CloseHandle() {
  if (device_handle_) {
    device_handle_->Close();
    if (client_)
      client_->OnDeviceClosed();
  }
  device_handle_ = nullptr;
}

bool DeviceImpl::HasControlTransferPermission(
    UsbControlTransferRecipient recipient,
    uint16_t index) {
  DCHECK(device_handle_);

  if (recipient != UsbControlTransferRecipient::INTERFACE &&
      recipient != UsbControlTransferRecipient::ENDPOINT) {
    return true;
  }

  const mojom::UsbConfigurationInfo* config = device_->GetActiveConfiguration();
  if (!config)
    return false;

  const mojom::UsbInterfaceInfo* interface = nullptr;
  if (recipient == UsbControlTransferRecipient::ENDPOINT) {
    interface = device_handle_->FindInterfaceByEndpoint(index & 0xff);
  } else {
    auto interface_it =
        std::find_if(config->interfaces.begin(), config->interfaces.end(),
                     [index](const mojom::UsbInterfaceInfoPtr& this_iface) {
                       return this_iface->interface_number == (index & 0xff);
                     });
    if (interface_it != config->interfaces.end())
      interface = interface_it->get();
  }

  return interface != nullptr;
}

// static
void DeviceImpl::OnOpen(base::WeakPtr<DeviceImpl> self,
                        OpenCallback callback,
                        scoped_refptr<UsbDeviceHandle> handle) {
  if (!self) {
    if (handle)
      handle->Close();
    return;
  }

  self->device_handle_ = std::move(handle);
  if (self->device_handle_ && self->client_)
    self->client_->OnDeviceOpened();

  std::move(callback).Run(self->device_handle_
                              ? mojom::UsbOpenDeviceError::OK
                              : mojom::UsbOpenDeviceError::ACCESS_DENIED);
}

void DeviceImpl::OnPermissionGrantedForOpen(OpenCallback callback,
                                            bool granted) {
  if (granted) {
    device_->Open(base::BindOnce(
        &DeviceImpl::OnOpen, weak_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    std::move(callback).Run(mojom::UsbOpenDeviceError::ACCESS_DENIED);
  }
}

void DeviceImpl::Open(OpenCallback callback) {
  if (device_handle_) {
    std::move(callback).Run(mojom::UsbOpenDeviceError::ALREADY_OPEN);
    return;
  }

  if (!device_->permission_granted()) {
    device_->RequestPermission(
        base::BindOnce(&DeviceImpl::OnPermissionGrantedForOpen,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  device_->Open(base::BindOnce(&DeviceImpl::OnOpen, weak_factory_.GetWeakPtr(),
                               std::move(callback)));
}

void DeviceImpl::Close(CloseCallback callback) {
  CloseHandle();
  std::move(callback).Run();
}

void DeviceImpl::SetConfiguration(uint8_t value,
                                  SetConfigurationCallback callback) {
  if (!device_handle_) {
    std::move(callback).Run(false);
    return;
  }

  device_handle_->SetConfiguration(value, std::move(callback));
}

void DeviceImpl::ClaimInterface(uint8_t interface_number,
                                ClaimInterfaceCallback callback) {
  if (!device_handle_) {
    std::move(callback).Run(false);
    return;
  }

  const mojom::UsbConfigurationInfo* config = device_->GetActiveConfiguration();
  if (!config) {
    std::move(callback).Run(false);
    return;
  }

  auto interface_it = std::find_if(
      config->interfaces.begin(), config->interfaces.end(),
      [interface_number](const mojom::UsbInterfaceInfoPtr& interface) {
        return interface->interface_number == interface_number;
      });
  if (interface_it == config->interfaces.end()) {
    std::move(callback).Run(false);
    return;
  }

  device_handle_->ClaimInterface(interface_number, std::move(callback));
}

void DeviceImpl::ReleaseInterface(uint8_t interface_number,
                                  ReleaseInterfaceCallback callback) {
  if (!device_handle_) {
    std::move(callback).Run(false);
    return;
  }

  device_handle_->ReleaseInterface(interface_number, std::move(callback));
}

void DeviceImpl::SetInterfaceAlternateSetting(
    uint8_t interface_number,
    uint8_t alternate_setting,
    SetInterfaceAlternateSettingCallback callback) {
  if (!device_handle_) {
    std::move(callback).Run(false);
    return;
  }

  device_handle_->SetInterfaceAlternateSetting(
      interface_number, alternate_setting, std::move(callback));
}

void DeviceImpl::Reset(ResetCallback callback) {
  if (!device_handle_) {
    std::move(callback).Run(false);
    return;
  }

  device_handle_->ResetDevice(std::move(callback));
}

void DeviceImpl::ClearHalt(uint8_t endpoint, ClearHaltCallback callback) {
  if (!device_handle_) {
    std::move(callback).Run(false);
    return;
  }

  device_handle_->ClearHalt(endpoint, std::move(callback));
}

void DeviceImpl::ControlTransferIn(UsbControlTransferParamsPtr params,
                                   uint32_t length,
                                   uint32_t timeout,
                                   ControlTransferInCallback callback) {
  if (!device_handle_) {
    std::move(callback).Run(mojom::UsbTransferStatus::TRANSFER_ERROR, {});
    return;
  }

  if (HasControlTransferPermission(params->recipient, params->index)) {
    auto buffer = base::MakeRefCounted<base::RefCountedBytes>(length);
    device_handle_->ControlTransfer(
        UsbTransferDirection::INBOUND, params->type, params->recipient,
        params->request, params->value, params->index, buffer, timeout,
        base::BindOnce(&OnTransferIn, std::move(callback)));
  } else {
    std::move(callback).Run(mojom::UsbTransferStatus::PERMISSION_DENIED, {});
  }
}

void DeviceImpl::ControlTransferOut(UsbControlTransferParamsPtr params,
                                    const std::vector<uint8_t>& data,
                                    uint32_t timeout,
                                    ControlTransferOutCallback callback) {
  if (!device_handle_) {
    std::move(callback).Run(mojom::UsbTransferStatus::TRANSFER_ERROR);
    return;
  }

  if (HasControlTransferPermission(params->recipient, params->index)) {
    auto buffer = base::MakeRefCounted<base::RefCountedBytes>(data);
    device_handle_->ControlTransfer(
        UsbTransferDirection::OUTBOUND, params->type, params->recipient,
        params->request, params->value, params->index, buffer, timeout,
        base::BindOnce(&OnTransferOut, std::move(callback)));
  } else {
    std::move(callback).Run(mojom::UsbTransferStatus::PERMISSION_DENIED);
  }
}

void DeviceImpl::GenericTransferIn(uint8_t endpoint_number,
                                   uint32_t length,
                                   uint32_t timeout,
                                   GenericTransferInCallback callback) {
  if (!device_handle_) {
    std::move(callback).Run(mojom::UsbTransferStatus::TRANSFER_ERROR, {});
    return;
  }

  uint8_t endpoint_address = endpoint_number | 0x80;
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(length);
  device_handle_->GenericTransfer(
      UsbTransferDirection::INBOUND, endpoint_address, buffer, timeout,
      base::BindOnce(&OnTransferIn, std::move(callback)));
}

void DeviceImpl::GenericTransferOut(uint8_t endpoint_number,
                                    const std::vector<uint8_t>& data,
                                    uint32_t timeout,
                                    GenericTransferOutCallback callback) {
  if (!device_handle_) {
    std::move(callback).Run(mojom::UsbTransferStatus::TRANSFER_ERROR);
    return;
  }

  uint8_t endpoint_address = endpoint_number;
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(data);
  device_handle_->GenericTransfer(
      UsbTransferDirection::OUTBOUND, endpoint_address, buffer, timeout,
      base::BindOnce(&OnTransferOut, std::move(callback)));
}

void DeviceImpl::IsochronousTransferIn(
    uint8_t endpoint_number,
    const std::vector<uint32_t>& packet_lengths,
    uint32_t timeout,
    IsochronousTransferInCallback callback) {
  if (!device_handle_) {
    std::move(callback).Run(
        {}, BuildIsochronousPacketArray(
                packet_lengths, mojom::UsbTransferStatus::TRANSFER_ERROR));
    return;
  }

  uint8_t endpoint_address = endpoint_number | 0x80;
  device_handle_->IsochronousTransferIn(
      endpoint_address, packet_lengths, timeout,
      base::BindOnce(&OnIsochronousTransferIn, std::move(callback)));
}

void DeviceImpl::IsochronousTransferOut(
    uint8_t endpoint_number,
    const std::vector<uint8_t>& data,
    const std::vector<uint32_t>& packet_lengths,
    uint32_t timeout,
    IsochronousTransferOutCallback callback) {
  if (!device_handle_) {
    std::move(callback).Run(BuildIsochronousPacketArray(
        packet_lengths, mojom::UsbTransferStatus::TRANSFER_ERROR));
    return;
  }

  uint8_t endpoint_address = endpoint_number;
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(data);
  device_handle_->IsochronousTransferOut(
      endpoint_address, buffer, packet_lengths, timeout,
      base::BindOnce(&OnIsochronousTransferOut, std::move(callback)));
}

void DeviceImpl::OnDeviceRemoved(scoped_refptr<device::UsbDevice> device) {
  DCHECK_EQ(device_, device);
  receiver_->Close();
}

void DeviceImpl::OnClientConnectionError() {
  // Close the connection with Blink when WebUsbServiceImpl notifies the
  // permission revocation from settings UI.
  receiver_->Close();
}

}  // namespace usb
}  // namespace device
