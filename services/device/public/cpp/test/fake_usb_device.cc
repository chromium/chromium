// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/fake_usb_device.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "services/device/public/cpp/test/mock_usb_mojo_device.h"
#include "services/device/public/cpp/usb/usb_utils.h"

namespace device {

// static
void FakeUsbDevice::Create(
    scoped_refptr<FakeUsbDeviceInfo> device,
    base::span<const uint8_t> blocked_interface_classes,
    mojo::PendingReceiver<device::mojom::UsbDevice> receiver,
    mojo::PendingRemote<mojom::UsbDeviceClient> client) {
  auto* device_object =
      new FakeUsbDevice(device, blocked_interface_classes, std::move(client));
  device_object->receiver_ = mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(device_object), std::move(receiver));
}

FakeUsbDevice::~FakeUsbDevice() {
  CloseHandle();
  observation_.Reset();
}

FakeUsbDevice::FakeUsbDevice(
    scoped_refptr<FakeUsbDeviceInfo> device,
    base::span<const uint8_t> blocked_interface_classes,
    mojo::PendingRemote<mojom::UsbDeviceClient> client)
    : device_(device),
      blocked_interface_classes_(blocked_interface_classes.begin(),
                                 blocked_interface_classes.end()),
      client_(std::move(client)) {
  DCHECK(device_);
  observation_.Observe(device_.get());

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
    mock_device->Close(base::DoNothing());
  }

  if (client_)
    client_->OnDeviceClosed();

  is_opened_ = false;
}

// Device implementation:
void FakeUsbDevice::Open(OpenCallback callback) {
  if (is_opened_) {
    std::move(callback).Run(mojom::UsbOpenDeviceResult::NewError(
        mojom::UsbOpenDeviceError::ALREADY_OPEN));
    return;
  }

  // Go on with mock device for testing.
  MockUsbMojoDevice* mock_device = device_->mock_device();
  if (mock_device) {
    mock_device->Open(base::BindOnce(&FakeUsbDevice::FinishOpen,
                                     base::Unretained(this),
                                     std::move(callback)));
    return;
  }

  FinishOpen(std::move(callback), mojom::UsbOpenDeviceResult::NewSuccess(
                                      mojom::UsbOpenDeviceSuccess::OK));
}

void FakeUsbDevice::FinishOpen(OpenCallback callback,
                               mojom::UsbOpenDeviceResultPtr result) {
  DCHECK(!is_opened_);
  is_opened_ = true;
  if (client_)
    client_->OnDeviceOpened();

  std::move(callback).Run(std::move(result));
}

void FakeUsbDevice::Close(CloseCallback callback) {
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

  std::move(callback).Run(device_->SetActiveConfig(value));
}

void FakeUsbDevice::ClaimInterface(uint8_t interface_number,
                                   ClaimInterfaceCallback callback) {
  // Go on with mock device for testing.
  MockUsbMojoDevice* mock_device = device_->mock_device();
  if (mock_device) {
    mock_device->ClaimInterface(interface_number, std::move(callback));
    return;
  }

  const mojom::UsbDeviceInfo& device_info = device_->GetDeviceInfo();
  auto config_it = base::ranges::find(
      device_info.configurations, device_info.active_configuration,
      &mojom::UsbConfigurationInfo::configuration_value);
  if (config_it == device_info.configurations.end()) {
    std::move(callback).Run(mojom::UsbClaimInterfaceResult::kFailure);
    LOG(ERROR) << "No such configuration.";
    return;
  }

  auto interface_it =
      base::ranges::find((*config_it)->interfaces, interface_number,
                         &mojom::UsbInterfaceInfo::interface_number);
  if (interface_it == (*config_it)->interfaces.end()) {
    std::move(callback).Run(mojom::UsbClaimInterfaceResult::kFailure);
    LOG(ERROR) << "No such interface in " << (*config_it)->interfaces.size()
               << " interfaces.";
    return;
  }

  for (const auto& alternate : (*interface_it)->alternates) {
    if (base::Contains(blocked_interface_classes_, alternate->class_code)) {
      std::move(callback).Run(mojom::UsbClaimInterfaceResult::kProtectedClass);
      return;
    }
  }

  bool success = claimed_interfaces_.insert(interface_number).second;
  if (!success) {
    LOG(ERROR) << "Interface already claimed.";
  }

  std::move(callback).Run(success ? mojom::UsbClaimInterfaceResult::kSuccess
                                  : mojom::UsbClaimInterfaceResult::kFailure);
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

void FakeUsbDevice::ClearHalt(mojom::UsbTransferDirection direction,
                              uint8_t endpoint_number,
                              ClearHaltCallback callback) {
  // Go on with mock device for testing.
  MockUsbMojoDevice* mock_device = device_->mock_device();
  if (mock_device) {
    mock_device->ClearHalt(direction, endpoint_number, std::move(callback));
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
    base::span<const uint8_t> data,
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
                                       base::span<const uint8_t> data,
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
    base::span<const uint8_t> data,
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
