// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/virtual_fido_device_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_u2f_device.h"

namespace device {
namespace test {

// A FidoDeviceDiscovery that always vends a single |VirtualFidoDevice|.
class VirtualFidoDeviceDiscovery
    : public FidoDeviceDiscovery,
      public base::SupportsWeakPtr<VirtualFidoDeviceDiscovery> {
 public:
  explicit VirtualFidoDeviceDiscovery(
      FidoTransportProtocol transport,
      scoped_refptr<VirtualFidoDevice::State> state,
      ProtocolVersion supported_protocol,
      const VirtualCtap2Device::Config& ctap2_config)
      : FidoDeviceDiscovery(transport),
        state_(std::move(state)),
        supported_protocol_(supported_protocol),
        ctap2_config_(ctap2_config) {}
  ~VirtualFidoDeviceDiscovery() override = default;

 protected:
  void StartInternal() override {
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

 private:
  scoped_refptr<VirtualFidoDevice::State> state_;
  const ProtocolVersion supported_protocol_;
  const VirtualCtap2Device::Config ctap2_config_;
  DISALLOW_COPY_AND_ASSIGN(VirtualFidoDeviceDiscovery);
};

VirtualFidoDeviceFactory::VirtualFidoDeviceFactory() = default;
VirtualFidoDeviceFactory::~VirtualFidoDeviceFactory() = default;

void VirtualFidoDeviceFactory::SetSupportedProtocol(
    ProtocolVersion supported_protocol) {
  supported_protocol_ = supported_protocol;
}

void VirtualFidoDeviceFactory::SetTransport(FidoTransportProtocol transport) {
  transport_ = transport;
  state_->transport = transport;
}

void VirtualFidoDeviceFactory::SetCtap2Config(
    const VirtualCtap2Device::Config& config) {
  supported_protocol_ = ProtocolVersion::kCtap2;
  ctap2_config_ = config;
}

VirtualFidoDevice::State* VirtualFidoDeviceFactory::mutable_state() {
  return state_.get();
}

std::unique_ptr<FidoDiscoveryBase> VirtualFidoDeviceFactory::Create(
    FidoTransportProtocol transport,
    ::service_manager::Connector* connector) {
  if (transport != transport_) {
    return nullptr;
  }
  return std::make_unique<VirtualFidoDeviceDiscovery>(
      transport_, state_, supported_protocol_, ctap2_config_);
}

}  // namespace test
}  // namespace device
