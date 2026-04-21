// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DIRECT_SOCKETS_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DIRECT_SOCKETS_DELEGATE_H_

#include <cstdint>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "content/common/content_export.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class BrowserContext;
class RenderFrameHost;

// Allows the embedder to alter the logic of some operations in
// content::DirectSocketsServiceImpl.
class CONTENT_EXPORT DirectSocketsDelegate {
 public:
  enum class ProtocolType { kTcp, kConnectedUdp, kBoundUdp, kTcpServer };

  struct RequestDetails {
    std::string address;
    uint16_t port;
    ProtocolType protocol;
  };

  virtual ~DirectSocketsDelegate() = default;

  // Allows the embedder to expose the API symbols in arbitrary contexts.
  // Note that in this case direct-sockets, direct-sockets-private and
  // direct-sockets-multicast permissions policies will be ignored; providing a
  // Permissions-Policy: direct-sockets=() header won't take effect.
  virtual bool AreDirectSocketsAllowed(BrowserContext* browser_context,
                                       const url::Origin& origin) = 0;

  // Allows embedders to introduce additional rules for specific
  // addresses/ports.
  virtual bool ValidateRequest(RenderFrameHost& rfh, const RequestDetails&) = 0;
  virtual bool ValidateRequestForSharedWorker(BrowserContext* browser_context,
                                              const GURL& shared_worker_url,
                                              const RequestDetails&) = 0;
  virtual bool ValidateRequestForServiceWorker(BrowserContext* browser_context,
                                               const url::Origin& origin,
                                               const RequestDetails&) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DIRECT_SOCKETS_DELEGATE_H_
