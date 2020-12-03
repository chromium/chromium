// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef DEVICE_FIDO_VIRTUAL_FIDO_DEVICE_DISCOVERY_H_
#define DEVICE_FIDO_VIRTUAL_FIDO_DEVICE_DISCOVERY_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/virtual_ctap2_device.h"

namespace device {
namespace test {

// A FidoDeviceDiscovery that always vends a single |VirtualFidoDevice|.
class VirtualFidoDeviceDiscovery
    : public FidoDeviceDiscovery,
      public base::SupportsWeakPtr<VirtualFidoDeviceDiscovery> {
 public:
  VirtualFidoDeviceDiscovery(FidoTransportProtocol transport,
                             scoped_refptr<VirtualFidoDevice::State> state,
                             ProtocolVersion supported_protocol,
                             const VirtualCtap2Device::Config& ctap2_config);
  ~VirtualFidoDeviceDiscovery() override;
  VirtualFidoDeviceDiscovery(const VirtualFidoDeviceDiscovery& other) = delete;
  VirtualFidoDeviceDiscovery& operator=(
      const VirtualFidoDeviceDiscovery& other) = delete;

 protected:
  void StartInternal() override;

 private:
  scoped_refptr<VirtualFidoDevice::State> state_;
  const ProtocolVersion supported_protocol_;
  const VirtualCtap2Device::Config ctap2_config_;
};

}  // namespace test
}  // namespace device

#endif  // DEVICE_FIDO_VIRTUAL_FIDO_DEVICE_DISCOVERY_H_
