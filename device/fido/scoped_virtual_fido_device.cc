// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/scoped_virtual_fido_device.h"

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
      scoped_refptr<VirtualFidoDevice::State> state,
      ProtocolVersion supported_protocol)
      : FidoDeviceDiscovery(FidoTransportProtocol::kUsbHumanInterfaceDevice),
        state_(std::move(state)),
        supported_protocol_(supported_protocol) {}
  ~VirtualFidoDeviceDiscovery() override = default;

 protected:
  void StartInternal() override {
    std::unique_ptr<FidoDevice> device;
    if (supported_protocol_ == ProtocolVersion::kCtap)
      device = std::make_unique<VirtualCtap2Device>(state_);
    else
      device = std::make_unique<VirtualU2fDevice>(state_);

    AddDevice(std::move(device));
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&VirtualFidoDeviceDiscovery::NotifyDiscoveryStarted,
                       AsWeakPtr(), true /* success */));
  }

 private:
  scoped_refptr<VirtualFidoDevice::State> state_;
  ProtocolVersion supported_protocol_;
  DISALLOW_COPY_AND_ASSIGN(VirtualFidoDeviceDiscovery);
};

ScopedVirtualFidoDevice::ScopedVirtualFidoDevice()
    : state_(new VirtualFidoDevice::State) {}
ScopedVirtualFidoDevice::~ScopedVirtualFidoDevice() = default;

void ScopedVirtualFidoDevice::SetSupportedProtocol(
    ProtocolVersion supported_protocol) {
  supported_protocol_ = supported_protocol;
}

VirtualFidoDevice::State* ScopedVirtualFidoDevice::mutable_state() {
  return state_.get();
}

std::unique_ptr<FidoDeviceDiscovery>
ScopedVirtualFidoDevice::CreateFidoDiscovery(
    FidoTransportProtocol transport,
    ::service_manager::Connector* connector) {
  if (transport != FidoTransportProtocol::kUsbHumanInterfaceDevice) {
    return nullptr;
  }
  return std::make_unique<VirtualFidoDeviceDiscovery>(state_,
                                                      supported_protocol_);
}

}  // namespace test
}  // namespace device
