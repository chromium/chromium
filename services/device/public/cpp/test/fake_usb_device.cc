// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/fake_usb_device.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "services/device/public/cpp/test/mock_usb_mojo_device.h"
#include "services/device/public/cpp/usb/usb_utils.h"

namespace device {

// static
void FakeUsbDevice::Create(
    scoped_refptr<FakeUsbDeviceInfo> device,
    mojo::PendingReceiver<device::mojom::UsbDevice> receiver,
    mojo::PendingRemote<mojom::UsbDeviceClient> client) {
  auto* device_object = new FakeUsbDevice(device, std::move(client));
  device_object->receiver_ = mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(device_object), std::move(receiver));
}

FakeUsbDevice::~FakeUsbDevice() {
  CloseHandle();
  observer_.RemoveAll();
}

FakeUsbDevice::FakeUsbDevice(scoped_refptr<FakeUsbDeviceInfo> device,
                             mojo::PendingRemote<mojom::UsbDeviceClient> client)
    : device_(device), observer_(this), client_(std::move(client)) {
  DCHECK(device_);
  observer_.Add(device_.get());

  if (client_) {
    client_.set_disconnect_handler(base::BindOnce(
        &FakeUsbDevice::OnClientConnectionError, base::Unretained(this)));
  }
}

void FakeUsbDevice::CloseHandle() {
  if (!is_opened_) {
    return;
  }

  MockUsbMojoDevice* mock_device = device_->mock_device();
  if (mock_device) {
    mock_device->Close(base::DoNothing::Once());
    return;
  }

  if (client_)
    client_->OnDeviceClosed();

  is_opened_ = false;
}

// Device implementation:
void FakeUsbDevice::Open(OpenCallback callback) {
  // Go on with mock device for testing.
  MockUsbMojoDevice* mock_device = device_->mock_device();
  if (mock_device) {
    mock_device->Open(std::move(callback));
    is_opened_ = true;
    return;
  }

  if (is_opened_) {
    std::move(callback).Run(mojom::UsbOpenDeviceError::ALREADY_OPEN);
    return;
  }

  is_opened_ = true;
  if (client_)
    client_->OnDeviceOpened();

  std::move(callback).Run(mojom::UsbOpenDeviceError::OK);
}

void FakeUsbDevice::Close(CloseCallback callback) {
  // Go on with mock device for testing.
  MockUsbMojoDevice* mock_device = device_->mock_device();
  if (mock_device) {
    mock_device->Close(std::move(callback));
    return;
  }

  CloseHandle();
  std::move(callback).Run();
}

void FakeUsbDevice::SetConfiguration(uint8_t value,
                                     SetConfigurationCallback callback) {
  // Go on with mock device for testing.
  MockUsbMojoDevice* mock_device = device_->mock_device();
  if (mock_device) {
    mock_device->SetConfiguration(value, std::move(callback));
    return;
  }

  std::move(callback).Run(true);
}

void FakeUsbDevice::ClaimInterface(uint8_t interface_number,
                                   ClaimInterfaceCallback callback) {
  // Go on with mock device for testing.
  MockUsbMojoDevice* mock_device = device_->mock_device();
  if (mock_device) {
    mock_device->ClaimInterface(interface_number, std::move(callback));
    return;
  }

  bool success = claimed_interfaces_.insert(interface_number).second;

  std::move(callback).Run(success);
}

void FakeUsbDevice::ReleaseInterface(uint8_t interface_number,
                                     ReleaseInterfaceCallback callback) {
  // Go on with mock device for testing.
  MockUsbMojoDevice* mock_device = device_->mock_device();
  if (mock_device) {
    mock_device->ReleaseInterface(interface_number, std::move(callback));
    return;
  }

  bool success = claimed_interfaces_.erase(interface_number) > 0;

  std::move(callback).Run(success);
}

void FakeUsbDevice::SetInterfaceAlternateSetting(
    uint8_t interface_number,
    uint8_t alternate_setting,
    SetInterfaceAlternateSettingCallback callback) {
  // Go on with mock device for testing.
  MockUsbMojoDevice* mock_device = device_->mock_device();
  if (mock_device) {
    mock_device->SetInterfaceAlternateSetting(
        interface_number, alternate_setting, std::move(callback));
    return;
  }
  std::move(callback).Run(true);
}

void FakeUsbDevice::Reset(ResetCallback callback) {
  // Go on with mock device for testing.
  MockUsbMojoDevice* mock_device = device_->mock_device();
  if (mock_device) {
    mock_device->Reset(std::move(callback));
    return;
  }
  std::move(callback).Run(true);
}

void FakeUsbDevice::ClearHalt(uint8_t endpoint, ClearHaltCallback callback) {
  // Go on with mock device for testing.
  MockUsbMojoDevice* mock_device = device_->mock_device();
  if (mock_device) {
    mock_device->ClearHalt(endpoint, std::move(callback));
    return;
  }
  std::move(callback).Run(true);
}

void FakeUsbDevice::ControlTransferIn(mojom::UsbControlTransferParamsPtr params,
                                      uint32_t length,
                                      uint32_t timeout,
                                      ControlTransferInCallback callback) {
  // Go on with mock device for testing.
  MockUsbMojoDevice* mock_device = device_->mock_device();
  if (mock_device) {
    mock_device->ControlTransferIn(std::move(params), length, timeout,
                                   std::move(callback));
    return;
  }
  std::move(callback).Run(mojom::UsbTransferStatus::COMPLETED, {});
}

void FakeUsbDevice::ControlTransferOut(
    mojom::UsbControlTransferParamsPtr params,
    const std::vector<uint8_t>& data,
    uint32_t timeout,
    ControlTransferOutCallback callback) {
  // Go on with mock device for testing.
  MockUsbMojoDevice* mock_device = device_->mock_device();
  if (mock_device) {
    mock_device->ControlTransferOut(std::move(params), data, timeout,
                                    std::move(callback));
    return;
  }
  std::move(callback).Run(mojom::UsbTransferStatus::COMPLETED);
}

void FakeUsbDevice::GenericTransferIn(uint8_t endpoint_number,
                                      uint32_t length,
                                      uint32_t timeout,
                                      GenericTransferInCallback callback) {
  // Go on with mock device for testing.
  MockUsbMojoDevice* mock_device = device_->mock_device();
  if (mock_device) {
    mock_device->GenericTransferIn(endpoint_number, length, timeout,
                                   std::move(callback));
    return;
  }
  std::move(callback).Run(mojom::UsbTransferStatus::COMPLETED, {});
}

void FakeUsbDevice::GenericTransferOut(uint8_t endpoint_number,
                                       const std::vector<uint8_t>& data,
                                       uint32_t timeout,
                                       GenericTransferOutCallback callback) {
  // Go on with mock device for testing.
  MockUsbMojoDevice* mock_device = device_->mock_device();
  if (mock_device) {
    mock_device->GenericTransferOut(endpoint_number, data, timeout,
                                    std::move(callback));
    return;
  }
  std::move(callback).Run(mojom::UsbTransferStatus::COMPLETED);
}

void FakeUsbDevice::IsochronousTransferIn(
    uint8_t endpoint_number,
    const std::vector<uint32_t>& packet_lengths,
    uint32_t timeout,
    IsochronousTransferInCallback callback) {
  // Go on with mock device for testing.
  MockUsbMojoDevice* mock_device = device_->mock_device();
  if (mock_device) {
    mock_device->IsochronousTransferIn(endpoint_number, packet_lengths, timeout,
                                       std::move(callback));
    return;
  }

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
  // Go on with mock device for testing.
  MockUsbMojoDevice* mock_device = device_->mock_device();
  if (mock_device) {
    mock_device->IsochronousTransferOut(endpoint_number, data, packet_lengths,
                                        timeout, std::move(callback));
    return;
  }

  std::move(callback).Run(BuildIsochronousPacketArray(
      packet_lengths, mojom::UsbTransferStatus::COMPLETED));
}

void FakeUsbDevice::OnDeviceRemoved(scoped_refptr<FakeUsbDeviceInfo> device) {
  DCHECK_EQ(device_, device);
  receiver_->Close();
}

void FakeUsbDevice::OnClientConnectionError() {
  // Close the binding with Blink when WebUsbService finds permission revoked
  // from setting UI.
  receiver_->Close();
}

}  // namespace device
