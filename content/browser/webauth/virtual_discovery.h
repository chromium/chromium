// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_VIRTUAL_DISCOVERY_H_
#define CONTENT_BROWSER_WEBAUTH_VIRTUAL_DISCOVERY_H_

#include <memory>
#include <string_view>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "device/fido/fido_device_discovery.h"
#include "device/fido/fido_transport_protocol.h"

namespace device {
class VirtualFidoDevice;
}

namespace content {

// A fully automated FidoDeviceDiscovery implementation, which is disconnected
// from the real world, and discovers VirtualFidoDevice instances.
class VirtualFidoDiscovery final : public ::device::FidoDeviceDiscovery {
 public:
  explicit VirtualFidoDiscovery(::device::FidoTransportProtocol transport);

  VirtualFidoDiscovery(const VirtualFidoDiscovery&) = delete;
  VirtualFidoDiscovery& operator=(const VirtualFidoDiscovery&) = delete;

  // Notifies the AuthenticatorEnvironment of this instance being destroyed.
  ~VirtualFidoDiscovery() override;

  void AddVirtualDevice(std::unique_ptr<device::VirtualFidoDevice> device);
  bool RemoveVirtualDevice(std::string_view device_id);

 protected:
  // FidoDeviceDiscovery:
  void StartInternal() override;

 private:
  std::vector<std::unique_ptr<device::VirtualFidoDevice>>
      devices_pending_discovery_start_;

  base::WeakPtrFactory<VirtualFidoDiscovery> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_VIRTUAL_DISCOVERY_H_
