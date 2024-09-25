// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DIRECT_SOCKETS_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DIRECT_SOCKETS_DELEGATE_H_

#include <cstdint>
#include <string>

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"

namespace content {

class RenderFrameHost;

// Allows the embedder to alter the logic of some operations in
// content::DirectSocketsServiceImpl.
class CONTENT_EXPORT DirectSocketsDelegate {
 public:
  enum class ProtocolType { kTcp, kConnectedUdp, kBoundUdp, kTcpServer };

  virtual ~DirectSocketsDelegate() = default;

  // Allows embedders to introduce additional rules for API access.
  virtual bool IsAPIAccessAllowed(content::RenderFrameHost& rfh) = 0;

  // Allows embedders to introduce additional rules for specific
  // addresses/ports.
  virtual bool ValidateAddressAndPort(content::RenderFrameHost& rfh,
                                      const std::string& address,
                                      uint16_t port,
                                      ProtocolType) = 0;

  // Allows embedders to introduce additional rules for private network access.
  virtual void RequestPrivateNetworkAccess(
      content::RenderFrameHost& rfh,
      base::OnceCallback<void(/*access_allowed=*/bool)>) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DIRECT_SOCKETS_DELEGATE_H_
