// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/public/cpp/fake_usb_device.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "device/usb/public/cpp/usb_utils.h"

namespace device {

// static
void FakeUsbDevice::Create(scoped_refptr<FakeUsbDeviceInfo> device,
                           mojom::UsbDeviceRequest request,
                           mojom::UsbDeviceClientPtr client) {
  auto* device_object = new FakeUsbDevice(device, std::move(client));
  device_object->binding_ = mojo::MakeStrongBinding(
      base::WrapUnique(device_object), std::move(request));
}

FakeUsbDevice::~FakeUsbDevice() {
  CloseHandle();
}

FakeUsbDevice::FakeUsbDevice(scoped_refptr<FakeUsbDeviceInfo> device,
                             mojom::UsbDeviceClientPtr client)
    : device_(device), observer_(this), client_(std::move(client)) {
  DCHECK(device_);
  observer_.Add(device_.get());
}

void FakeUsbDevice::CloseHandle() {
  if (is_opened_ && client_)
    client_->OnDeviceClosed();

  is_opened_ = false;
}

// Device implementation:
void FakeUsbDevice::Open(OpenCallback callback) {
  is_opened_ = true;
  if (client_)
    client_->OnDeviceOpened();

  std::move(callback).Run(mojom::UsbOpenDeviceError::OK);
}

void FakeUsbDevice::Close(CloseCallback callback) {
  CloseHandle();
  std::move(callback).Run();
}
void FakeUsbDevice::SetConfiguration(uint8_t value,
                                     SetConfigurationCallback callback) {
  std::move(callback).Run(true);
}
void FakeUsbDevice::ClaimInterface(uint8_t interface_number,
                                   ClaimInterfaceCallback callback) {
  std::move(callback).Run(true);
}
void FakeUsbDevice::ReleaseInterface(uint8_t interface_number,
                                     ReleaseInterfaceCallback callback) {
  std::move(callback).Run(true);
}
void FakeUsbDevice::SetInterfaceAlternateSetting(
    uint8_t interface_number,
    uint8_t alternate_setting,
    SetInterfaceAlternateSettingCallback callback) {
  std::move(callback).Run(true);
}
void FakeUsbDevice::Reset(ResetCallback callback) {
  std::move(callback).Run(true);
}
void FakeUsbDevice::ClearHalt(uint8_t endpoint, ClearHaltCallback callback) {
  std::move(callback).Run(true);
}
void FakeUsbDevice::ControlTransferIn(mojom::UsbControlTransferParamsPtr params,
                                      uint32_t length,
                                      uint32_t timeout,
                                      ControlTransferInCallback callback) {
  std::move(callback).Run(mojom::UsbTransferStatus::COMPLETED, {});
}
void FakeUsbDevice::ControlTransferOut(
    mojom::UsbControlTransferParamsPtr params,
    const std::vector<uint8_t>& data,
    uint32_t timeout,
    ControlTransferOutCallback callback) {
  std::move(callback).Run(mojom::UsbTransferStatus::COMPLETED);
}
void FakeUsbDevice::GenericTransferIn(uint8_t endpoint_number,
                                      uint32_t length,
                                      uint32_t timeout,
                                      GenericTransferInCallback callback) {
  std::move(callback).Run(mojom::UsbTransferStatus::COMPLETED, {});
}
void FakeUsbDevice::GenericTransferOut(uint8_t endpoint_number,
                                       const std::vector<uint8_t>& data,
                                       uint32_t timeout,
                                       GenericTransferOutCallback callback) {
  std::move(callback).Run(mojom::UsbTransferStatus::COMPLETED);
}
void FakeUsbDevice::IsochronousTransferIn(
    uint8_t endpoint_number,
    const std::vector<uint32_t>& packet_lengths,
    uint32_t timeout,
    IsochronousTransferInCallback callback) {
  std::move(callback).Run(
      {}, BuildIsochronousPacketArray(packet_lengths,
                                      mojom::UsbTransferStatus::COMPLETED));
}
void FakeUsbDevice::IsochronousTransferOut(
    uint8_t endpoint_number,
    const std::vector<uint8_t>& data,
    const std::vector<uint32_t>& packet_lengths,
    uint32_t timeout,
    IsochronousTransferOutCallback callback) {
  std::move(callback).Run(BuildIsochronousPacketArray(
      packet_lengths, mojom::UsbTransferStatus::COMPLETED));
}

void FakeUsbDevice::OnDeviceRemoved(scoped_refptr<FakeUsbDeviceInfo> device) {
  DCHECK_EQ(device_, device);
  binding_->Close();
}

}  // namespace device
