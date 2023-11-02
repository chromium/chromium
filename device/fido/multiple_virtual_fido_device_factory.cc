// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/multiple_virtual_fido_device_factory.h"

#include "device/fido/virtual_fido_device_discovery.h"

namespace device {
namespace test {

MultipleVirtualFidoDeviceFactory::DeviceDetails::DeviceDetails() = default;
MultipleVirtualFidoDeviceFactory::DeviceDetails::~DeviceDetails() = default;
MultipleVirtualFidoDeviceFactory::DeviceDetails::DeviceDetails(
    DeviceDetails&& other) = default;
MultipleVirtualFidoDeviceFactory::DeviceDetails&
MultipleVirtualFidoDeviceFactory::DeviceDetails::operator=(
    DeviceDetails&& other) = default;

MultipleVirtualFidoDeviceFactory::MultipleVirtualFidoDeviceFactory() = default;
MultipleVirtualFidoDeviceFactory::~MultipleVirtualFidoDeviceFactory() = default;

void MultipleVirtualFidoDeviceFactory::AddDevice(DeviceDetails device_details) {
  devices_.push_back(std::move(device_details));
}

std::vector<std::unique_ptr<FidoDiscoveryBase>>
MultipleVirtualFidoDeviceFactory::Create(FidoTransportProtocol transport) {
  std::vector<std::unique_ptr<FidoDiscoveryBase>> discoveries;
  for (auto& device : devices_) {
    if (device.transport != transport) {
      continue;
    }
    const size_t trace_index = trace_->discoveries.size();
    trace_->discoveries.emplace_back();
    discoveries.push_back(std::make_unique<VirtualFidoDeviceDiscovery>(
        trace_, trace_index, device.transport, device.state, device.protocol,
        device.config, std::move(device.disconnect_events)));
  }
  return discoveries;
}

bool MultipleVirtualFidoDeviceFactory::IsTestOverride() {
  return true;
}

}  // namespace test
}  // namespace device
