// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_VPN_SERVICE_PROXY_H_
#define CONTENT_PUBLIC_BROWSER_VPN_SERVICE_PROXY_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "content/common/content_export.h"

namespace content {
class PepperVpnProviderResourceHostProxy;

// Describes interface for communication with an external VpnService.
// All the methods below can only be called on the UI thread.
class CONTENT_EXPORT VpnServiceProxy {
 public:
  using SuccessCallback = base::OnceClosure;
  using FailureCallback =
      base::OnceCallback<void(const std::string& error_name,
                              const std::string& error_message)>;

  virtual ~VpnServiceProxy() {}

  // Binds an existing VPN connection in the VpnService. Registers with the
  // VpnService the Resource host back-end.
  virtual void Bind(const std::string& host_id,
                    const std::string& configuration_id,
                    const std::string& configuration_name,
                    SuccessCallback success,
                    FailureCallback failure,
                    std::unique_ptr<PepperVpnProviderResourceHostProxy>
                        pepper_vpn_provider_proxy) = 0;

  // Sends an IP packet to the VpnService.
  virtual void SendPacket(const std::string& host_id,
                          const std::vector<char>& data,
                          SuccessCallback success,
                          FailureCallback failure) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_VPN_SERVICE_PROXY_H_
