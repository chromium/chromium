// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_VIRTUAL_DISCOVERY_H_
#define CONTENT_BROWSER_WEBAUTH_VIRTUAL_DISCOVERY_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "device/fido/fido_device_discovery.h"

namespace device {
class FidoDevice;
}

namespace content {

// A fully automated FidoDeviceDiscovery implementation, which is disconnected
// from the real world, and discovers VirtualFidoDevice instances.
class CONTENT_EXPORT VirtualFidoDiscovery
    : public ::device::FidoDeviceDiscovery,
      public base::SupportsWeakPtr<VirtualFidoDiscovery> {
 public:
  explicit VirtualFidoDiscovery(::device::FidoTransportProtocol transport);

  // Notifies the AuthenticatorEnvironment of this instance being destroyed.
  ~VirtualFidoDiscovery() override;

  void AddVirtualDevice(std::unique_ptr<::device::FidoDevice> device);
  bool RemoveVirtualDevice(base::StringPiece device_id);

 protected:
  // FidoDeviceDiscovery:
  void StartInternal() override;

 private:
  std::vector<std::unique_ptr<::device::FidoDevice>>
      devices_pending_discovery_start_;

  DISALLOW_COPY_AND_ASSIGN(VirtualFidoDiscovery);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_VIRTUAL_DISCOVERY_H_
