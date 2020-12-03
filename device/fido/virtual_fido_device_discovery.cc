// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/virtual_fido_device_discovery.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "device/fido/virtual_u2f_device.h"

namespace device {
namespace test {

VirtualFidoDeviceDiscovery::VirtualFidoDeviceDiscovery(
    FidoTransportProtocol transport,
    scoped_refptr<VirtualFidoDevice::State> state,
    ProtocolVersion supported_protocol,
    const VirtualCtap2Device::Config& ctap2_config)
    : FidoDeviceDiscovery(transport),
      state_(std::move(state)),
      supported_protocol_(supported_protocol),
      ctap2_config_(ctap2_config) {}
VirtualFidoDeviceDiscovery::~VirtualFidoDeviceDiscovery() = default;

void VirtualFidoDeviceDiscovery::StartInternal() {
  std::unique_ptr<FidoDevice> device;
  if (supported_protocol_ == ProtocolVersion::kCtap2) {
    device = std::make_unique<VirtualCtap2Device>(state_, ctap2_config_);
  } else {
    device = std::make_unique<VirtualU2fDevice>(state_);
  }

  AddDevice(std::move(device));
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&VirtualFidoDeviceDiscovery::NotifyDiscoveryStarted,
                     AsWeakPtr(), true /* success */));
}

}  // namespace test
}  // namespace device
