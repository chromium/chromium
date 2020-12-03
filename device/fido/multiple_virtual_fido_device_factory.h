// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MULTIPLE_VIRTUAL_FIDO_DEVICE_FACTORY_H_
#define DEVICE_FIDO_MULTIPLE_VIRTUAL_FIDO_DEVICE_FACTORY_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device.h"

namespace device {
namespace test {

// Similar to |VirtualFidoDeviceFactory|, but has the ability to return multiple
// |VirtualFidoDevice|s.
class MultipleVirtualFidoDeviceFactory : public device::FidoDiscoveryFactory {
 public:
  struct DeviceDetails {
    DeviceDetails();
    ~DeviceDetails();
    DeviceDetails(const DeviceDetails& other) = delete;
    DeviceDetails& operator=(const DeviceDetails& other) = delete;
    DeviceDetails(DeviceDetails&& other);
    DeviceDetails& operator=(DeviceDetails&& other);

    FidoTransportProtocol transport =
        FidoTransportProtocol::kUsbHumanInterfaceDevice;
    ProtocolVersion protocol = ProtocolVersion::kCtap2;
    VirtualCtap2Device::Config config;
    scoped_refptr<VirtualFidoDevice::State> state =
        base::MakeRefCounted<VirtualFidoDevice::State>();
  };

  MultipleVirtualFidoDeviceFactory();
  ~MultipleVirtualFidoDeviceFactory() override;
  MultipleVirtualFidoDeviceFactory(
      const MultipleVirtualFidoDeviceFactory& other) = delete;
  MultipleVirtualFidoDeviceFactory operator=(
      const MultipleVirtualFidoDeviceFactory& other) = delete;

  void AddDevice(DeviceDetails device_details);

 protected:
  // device::FidoDiscoveryFactory:
  std::vector<std::unique_ptr<FidoDiscoveryBase>> Create(
      FidoTransportProtocol transport) override;
  bool IsTestOverride() override;

 private:
  std::vector<DeviceDetails> devices_;
};

}  // namespace test
}  // namespace device

#endif  // DEVICE_FIDO_MULTIPLE_VIRTUAL_FIDO_DEVICE_FACTORY_H_
