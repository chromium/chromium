// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/mojo/device_impl.h"

#include <stddef.h>

#include <memory>
#include <numeric>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/usb/usb_device.h"
#include "third_party/blink/public/common/features.h"

namespace device {

using mojom::UsbControlTransferParamsPtr;
using mojom::UsbControlTransferRecipient;
using mojom::UsbIsochronousPacketPtr;
using mojom::UsbTransferDirection;
using mojom::UsbTransferStatus;

namespace usb {

namespace {

constexpr size_t kUsbTransferLengthLimit = 32 * 1024 * 1024;  // 32 MiB

void OnTransferIn(mojom::UsbDevice::GenericTransferInCallback callback,
                  UsbTransferStatus status,
                  scoped_refptr<base::RefCountedBytes> buffer,
                  size_t buffer_size) {
  auto data = buffer ? base::span(*buffer).first(buffer_size)
                     : base::span<const uint8_t>();
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
  uint32_t buffer_size = std::accumulate(
      packets.begin(), packets.end(), 0u,
      [](const uint32_t& a, const UsbIsochronousPacketPtr& packet) {
        return a + packet->length;
      });
  auto data = buffer ? base::span(*buffer).first(buffer_size)
                     : base::span<const uint8_t>();
  std::move(callback).Run(data, std::move(packets));
}

void OnIsochronousTransferOut(
    mojom::UsbDevice::IsochronousTransferOutCallback callback,
    scoped_refptr<base::RefCountedBytes> buffer,
    std::vector<UsbIsochronousPacketPtr> packets) {
  std::move(callback).Run(std::move(packets));
}

// IsAndroidSecurityKeyRequest returns true if |params| is attempting to
// configure an Android phone to act as a security key.
bool IsAndroidSecurityKeyRequest(
    const mojom::UsbControlTransferParamsPtr& params,
    base::span<const uint8_t> data) {
  // This matches a request to send an AOA model string:
  // https://source.android.com/devices/accessories/aoa#attempt-to-start-in-accessory-mode
  //
  // The magic model is matched as a prefix because sending trailing NULs etc
  // would be considered equivalent by Android but would not be caught by an
  // exact match here. Android is case-sensitive thus a byte-wise match is
  // suitable.
  const char* magic = mojom::UsbControlTransferParams::kSecurityKeyAOAModel;
  return params->type == mojom::UsbControlTransferType::VENDOR &&
         params->request == 52 && params->index == 1 &&
         data.size() >= strlen(magic) &&
         memcmp(data.data(), magic, strlen(magic)) == 0;
}

// Returns the sum of `packet_lengths`, or nullopt if the sum would overflow.
std::optional<uint32_t> TotalPacketLength(
    base::span<const uint32_t> packet_lengths) {
  uint32_t total_bytes = 0;
  for (const uint32_t packet_length : packet_lengths) {
    // Check for overflow.
    if (std::numeric_limits<uint32_t>::max() - total_bytes < packet_length) {
      return std::nullopt;
    }
    total_bytes += packet_length;
  }
  return total_bytes;
}

}  // namespace

// static
void DeviceImpl::Create(scoped_refptr<device::UsbDevice> device,
                        mojo::PendingReceiver<mojom::UsbDevice> receiver,
                        mojo::PendingRemote<mojom::UsbDeviceClient> client,
                        base::span<const uint8_t> blocked_interface_classes,
                        bool allow_security_key_requests) {
  auto* device_impl =
      new DeviceImpl(std::move(device), std::move(client),
                     blocked_interface_classes, allow_security_key_requests);
  device_impl->receiver_ = mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(device_impl), std::move(receiver));
}

DeviceImpl::~DeviceImpl() {
  CloseHandle();
}

DeviceImpl::DeviceImpl(scoped_refptr<device::UsbDevice> device,
                       mojo::PendingRemote<mojom::UsbDeviceClient> client,
                       base::span<const uint8_t> blocked_interface_classes,
                       bool allow_security_key_requests)
    : device_(std::move(device)),
      blocked_interface_classes_(blocked_interface_classes.begin(),
                                 blocked_interface_classes.end()),
      allow_security_key_requests_(allow_security_key_requests),
      client_(std::move(client)) {
  DCHECK(device_);
  observation_.Observe(device_.get());

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
        base::ranges::find(config->interfaces, index & 0xff,
                           &mojom::UsbInterfaceInfo::interface_number);
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

  self->opening_ = false;
  self->device_handle_ = std::move(handle);
  if (self->device_handle_ && self->client_)
    self->client_->OnDeviceOpened();

  if (self->device_handle_) {
    std::move(callback).Run(mojom::UsbOpenDeviceResult::NewSuccess(
        mojom::UsbOpenDeviceSuccess::OK));
  } else {
    std::move(callback).Run(mojom::UsbOpenDeviceResult::NewError(
        mojom::UsbOpenDeviceError::ACCESS_DENIED));
  }
}

void DeviceImpl::OnPermissionGrantedForOpen(OpenCallback callback,
                                            bool granted) {
  if (granted) {
    device_->Open(base::BindOnce(
        &DeviceImpl::OnOpen, weak_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    opening_ = false;
    std::move(callback).Run(mojom::UsbOpenDeviceResult::NewError(
        mojom::UsbOpenDeviceError::ACCESS_DENIED));
  }
}

void DeviceImpl::Open(OpenCallback callback) {
  if (opening_ || device_handle_) {
    std::move(callback).Run(mojom::UsbOpenDeviceResult::NewError(
        mojom::UsbOpenDeviceError::ALREADY_OPEN));
    return;
  }

  opening_ = true;

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
    std::move(callback).Run(mojom::UsbClaimInterfaceResult::kFailure);
    return;
  }

  const mojom::UsbConfigurationInfo* config = device_->GetActiveConfiguration();
  if (!config) {
    std::move(callback).Run(mojom::UsbClaimInterfaceResult::kFailure);
    return;
  }

  auto interface_it =
      base::ranges::find(config->interfaces, interface_number,
                         &mojom::UsbInterfaceInfo::interface_number);
  if (interface_it == config->interfaces.end()) {
    std::move(callback).Run(mojom::UsbClaimInterfaceResult::kFailure);
    return;
  }

  for (const auto& alternate : (*interface_it)->alternates) {
    if (base::Contains(blocked_interface_classes_, alternate->class_code)) {
      std::move(callback).Run(mojom::UsbClaimInterfaceResult::kProtectedClass);
      return;
    }
  }

  device_handle_->ClaimInterface(
      interface_number,
      base::BindOnce(&DeviceImpl::OnInterfaceClaimed,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
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

void DeviceImpl::ClearHalt(UsbTransferDirection direction,
                           uint8_t endpoint_number,
                           ClearHaltCallback callback) {
  if (!device_handle_) {
    std::move(callback).Run(false);
    return;
  }

  device_handle_->ClearHalt(direction, endpoint_number, std::move(callback));
}

void DeviceImpl::ControlTransferIn(UsbControlTransferParamsPtr params,
                                   uint32_t length,
                                   uint32_t timeout,
                                   ControlTransferInCallback callback) {
  if (!device_handle_) {
    std::move(callback).Run(mojom::UsbTransferStatus::TRANSFER_ERROR, {});
    return;
  }
  if (ShouldRejectUsbTransferLengthAndReportBadMessage(length)) {
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
                                    base::span<const uint8_t> data,
                                    uint32_t timeout,
                                    ControlTransferOutCallback callback) {
  if (!device_handle_) {
    std::move(callback).Run(mojom::UsbTransferStatus::TRANSFER_ERROR);
    return;
  }
  if (ShouldRejectUsbTransferLengthAndReportBadMessage(data.size())) {
    return;
  }

  if (HasControlTransferPermission(params->recipient, params->index) &&
      (allow_security_key_requests_ ||
       !IsAndroidSecurityKeyRequest(params, data))) {
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
  if (ShouldRejectUsbTransferLengthAndReportBadMessage(length)) {
    return;
  }

  uint8_t endpoint_address = endpoint_number | 0x80;
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(length);
  device_handle_->GenericTransfer(
      UsbTransferDirection::INBOUND, endpoint_address, buffer, timeout,
      base::BindOnce(&OnTransferIn, std::move(callback)));
}

void DeviceImpl::GenericTransferOut(uint8_t endpoint_number,
                                    base::span<const uint8_t> data,
                                    uint32_t timeout,
                                    GenericTransferOutCallback callback) {
  if (!device_handle_) {
    std::move(callback).Run(mojom::UsbTransferStatus::TRANSFER_ERROR);
    return;
  }
  if (ShouldRejectUsbTransferLengthAndReportBadMessage(data.size())) {
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

  std::optional<uint32_t> total_bytes = TotalPacketLength(packet_lengths);
  if (!total_bytes.has_value()) {
    mojo::ReportBadMessage("Invalid isochronous packet lengths.");
    std::move(callback).Run(
        {}, BuildIsochronousPacketArray(
                packet_lengths, mojom::UsbTransferStatus::TRANSFER_ERROR));
    return;
  }
  if (ShouldRejectUsbTransferLengthAndReportBadMessage(total_bytes.value())) {
    return;
  }

  uint8_t endpoint_address = endpoint_number | 0x80;
  device_handle_->IsochronousTransferIn(
      endpoint_address, packet_lengths, timeout,
      base::BindOnce(&OnIsochronousTransferIn, std::move(callback)));
}

void DeviceImpl::IsochronousTransferOut(
    uint8_t endpoint_number,
    base::span<const uint8_t> data,
    const std::vector<uint32_t>& packet_lengths,
    uint32_t timeout,
    IsochronousTransferOutCallback callback) {
  if (!device_handle_) {
    std::move(callback).Run(BuildIsochronousPacketArray(
        packet_lengths, mojom::UsbTransferStatus::TRANSFER_ERROR));
    return;
  }

  std::optional<uint32_t> total_bytes = TotalPacketLength(packet_lengths);
  if (!total_bytes.has_value() || total_bytes.value() != data.size()) {
    mojo::ReportBadMessage("Invalid isochronous packet lengths.");
    std::move(callback).Run(BuildIsochronousPacketArray(
        packet_lengths, mojom::UsbTransferStatus::TRANSFER_ERROR));
    return;
  }
  if (ShouldRejectUsbTransferLengthAndReportBadMessage(total_bytes.value())) {
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

void DeviceImpl::OnInterfaceClaimed(ClaimInterfaceCallback callback,
                                    bool success) {
  std::move(callback).Run(success ? mojom::UsbClaimInterfaceResult::kSuccess
                                  : mojom::UsbClaimInterfaceResult::kFailure);
}

void DeviceImpl::OnClientConnectionError() {
  // Close the connection with Blink when WebUsbServiceImpl notifies the
  // permission revocation from settings UI.
  receiver_->Close();
}

bool DeviceImpl::ShouldRejectUsbTransferLengthAndReportBadMessage(
    size_t length) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kWebUSBTransferSizeLimit)) {
    return false;
  }

  if (length <= kUsbTransferLengthLimit) {
    return false;
  }
  receiver_->ReportBadMessage(
      base::StringPrintf("Transfer size %zu is over the limit.", length));
  return true;
}

}  // namespace usb
}  // namespace device
