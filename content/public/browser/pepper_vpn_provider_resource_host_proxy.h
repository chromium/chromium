// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PEPPER_VPN_PROVIDER_RESOURCE_HOST_PROXY_H_
#define CONTENT_PUBLIC_BROWSER_PEPPER_VPN_PROVIDER_RESOURCE_HOST_PROXY_H_

#include <vector>

#include "content/common/content_export.h"

namespace content {

// Describes interface for communication with the VpnProviderResouceHost.
class CONTENT_EXPORT PepperVpnProviderResourceHostProxy {
 public:
  virtual ~PepperVpnProviderResourceHostProxy() {}

  // Passes an Unbind event to the VpnProviderResouceHost.
  virtual void SendOnUnbind() = 0;

  // Sends an IP packet to the VpnProviderResouceHost.
  virtual void SendOnPacketReceived(const std::vector<char>& data) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PEPPER_VPN_PROVIDER_RESOURCE_HOST_PROXY_H_
