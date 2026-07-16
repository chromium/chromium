// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/fake_usb_device_handle.h"

#include <algorithm>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/notimplemented.h"
#include "base/numerics/byte_conversions.h"
#include "services/device/usb/usb_device.h"

namespace device {

using mojom::UsbControlTransferRecipient;
using mojom::UsbControlTransferType;
using mojom::UsbTransferDirection;
using mojom::UsbTransferStatus;

FakeUsbDeviceHandle::FakeUsbDeviceHandle(base::span<const uint8_t> data)
    : data_(data) {}

scoped_refptr<UsbDevice> FakeUsbDeviceHandle::GetDevice() const {
  NOTIMPLEMENTED();
  return nullptr;
}

void FakeUsbDeviceHandle::Close() {
  NOTIMPLEMENTED();
}

void FakeUsbDeviceHandle::SetConfiguration(int configuration_value,
                                           ResultCallback callback) {
  NOTIMPLEMENTED();
}

void FakeUsbDeviceHandle::ClaimInterface(int interface_number,
                                         ResultCallback callback) {
  NOTIMPLEMENTED();
}

void FakeUsbDeviceHandle::ReleaseInterface(int interface_number,
                                           ResultCallback callback) {
  NOTIMPLEMENTED();
}

void FakeUsbDeviceHandle::SetInterfaceAlternateSetting(
    int interface_number,
    int alternate_setting,
    ResultCallback callback) {
  NOTIMPLEMENTED();
}

void FakeUsbDeviceHandle::ResetDevice(ResultCallback callback) {
  NOTIMPLEMENTED();
}

void FakeUsbDeviceHandle::ClearHalt(mojom::UsbTransferDirection direction,
                                    uint8_t endpoint_number,
                                    ResultCallback callback) {
  NOTIMPLEMENTED();
}

void FakeUsbDeviceHandle::ControlTransfer(
    UsbTransferDirection direction,
    UsbControlTransferType request_type,
    UsbControlTransferRecipient recipient,
    uint8_t request,
    uint16_t value,
    uint16_t index,
    scoped_refptr<base::RefCountedBytes> buffer,
    unsigned int timeout,
    UsbDeviceHandle::TransferCallback callback) {
  if (data_.empty()) {
    std::move(callback).Run(UsbTransferStatus::DISCONNECT, buffer, 0);
    return;
  }

  uint8_t success = data_[0];
  data_ = data_.subspan(1u);

  if (success % 2) {
    size_t bytes_transferred = 0;
    if (data_.size() >= 2u) {
      bytes_transferred = base::U16FromLittleEndian(data_.first<2u>());
      data_ = data_.subspan(2u);
      bytes_transferred = std::min(bytes_transferred, buffer->size());
      bytes_transferred = std::min(bytes_transferred, data_.size());
    }

    if (direction == UsbTransferDirection::INBOUND) {
      base::span(buffer->as_vector())
          .copy_prefix_from(data_.first(bytes_transferred));
      data_ = data_.subspan(bytes_transferred);
    }

    std::move(callback).Run(UsbTransferStatus::COMPLETED, buffer,
                            bytes_transferred);
  } else {
    std::move(callback).Run(UsbTransferStatus::TRANSFER_ERROR, buffer, 0);
  }
}

void FakeUsbDeviceHandle::IsochronousTransferIn(
    uint8_t endpoint_number,
    const std::vector<uint32_t>& packet_lengths,
    unsigned int timeout,
    IsochronousTransferCallback callback) {
  NOTIMPLEMENTED();
}

void FakeUsbDeviceHandle::IsochronousTransferOut(
    uint8_t endpoint_number,
    scoped_refptr<base::RefCountedBytes> buffer,
    const std::vector<uint32_t>& packet_lengths,
    unsigned int timeout,
    IsochronousTransferCallback callback) {
  NOTIMPLEMENTED();
}

void FakeUsbDeviceHandle::GenericTransfer(
    UsbTransferDirection direction,
    uint8_t endpoint_number,
    scoped_refptr<base::RefCountedBytes> buffer,
    unsigned int timeout,
    TransferCallback callback) {
  NOTIMPLEMENTED();
}

const mojom::UsbInterfaceInfo* FakeUsbDeviceHandle::FindInterfaceByEndpoint(
    uint8_t endpoint_address) {
  NOTIMPLEMENTED();
  return nullptr;
}

FakeUsbDeviceHandle::~FakeUsbDeviceHandle() = default;

}  // namespace device
