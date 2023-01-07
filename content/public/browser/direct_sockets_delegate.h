// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DIRECT_SOCKETS_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DIRECT_SOCKETS_DELEGATE_H_

#include <cstdint>
#include <string>

#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom-shared.h"

namespace content {

class RenderFrameHost;

// Allows the embedder to alter the logic of some operations in
// content::DirectSocketsServiceImpl.
class CONTENT_EXPORT DirectSocketsDelegate {
 public:
  virtual ~DirectSocketsDelegate() = default;

  // Allows embedders to introduce additional rules for specific
  // addresses/ports.
  virtual bool ValidateAddressAndPort(
      content::RenderFrameHost*,
      const std::string& address,
      uint16_t port,
      blink::mojom::DirectSocketProtocolType) const = 0;

  // If yes, skips post-resolve checks for Direct TCP/UDP sockets.
  virtual bool ShouldSkipPostResolveChecks(content::RenderFrameHost*) const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DIRECT_SOCKETS_DELEGATE_H_
