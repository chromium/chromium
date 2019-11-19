// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/hid/fido_hid_discovery.h"

#include <utility>

#include "base/bind.h"
#include "device/fido/hid/fido_hid_device.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

FidoHidDiscovery::FidoHidDiscovery(::service_manager::Connector* connector)
    : FidoDeviceDiscovery(FidoTransportProtocol::kUsbHumanInterfaceDevice),
      connector_(connector) {
  // TODO(piperc@): Give this constant a name.
  filter_.SetUsagePage(0xf1d0);
}

FidoHidDiscovery::~FidoHidDiscovery() = default;

void FidoHidDiscovery::StartInternal() {
  DCHECK(connector_);
  connector_->Connect(device::mojom::kServiceName,
                      hid_manager_.BindNewPipeAndPassReceiver());

  hid_manager_->GetDevicesAndSetClient(
      receiver_.BindNewEndpointAndPassRemote(),
      base::BindOnce(&FidoHidDiscovery::OnGetDevices,
                     weak_factory_.GetWeakPtr()));
}

void FidoHidDiscovery::DeviceAdded(
    device::mojom::HidDeviceInfoPtr device_info) {
  // The init packet header is the larger of the headers so we only compare
  // against it below.
  static_assert(
      kHidInitPacketHeaderSize >= kHidContinuationPacketHeaderSize,
      "init header is expected to be larger than continuation header");

  // Ignore non-U2F devices.
  if (filter_.Matches(*device_info) &&
      // Check that the supported report sizes are sufficient for at least one
      // byte of non-header data per report and not larger than our maximum
      // size.
      device_info->max_input_report_size > kHidInitPacketHeaderSize &&
      device_info->max_input_report_size <= kHidMaxPacketSize &&
      device_info->max_output_report_size > kHidInitPacketHeaderSize &&
      device_info->max_output_report_size <= kHidMaxPacketSize) {
    AddDevice(std::make_unique<FidoHidDevice>(std::move(device_info),
                                              hid_manager_.get()));
  }
}

void FidoHidDiscovery::DeviceRemoved(
    device::mojom::HidDeviceInfoPtr device_info) {
  // Ignore non-U2F devices.
  if (filter_.Matches(*device_info)) {
    RemoveDevice(FidoHidDevice::GetIdForDevice(*device_info));
  }
}

void FidoHidDiscovery::OnGetDevices(
    std::vector<device::mojom::HidDeviceInfoPtr> device_infos) {
  for (auto& device_info : device_infos)
    DeviceAdded(std::move(device_info));

  NotifyDiscoveryStarted(true);
}

}  // namespace device
