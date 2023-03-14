// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DIRECT_SOCKETS_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DIRECT_SOCKETS_DELEGATE_H_

#include <cstdint>
#include <string>

#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

class BrowserContext;

// Allows the embedder to alter the logic of some operations in
// content::DirectSocketsServiceImpl.
class CONTENT_EXPORT DirectSocketsDelegate {
 public:
  enum class ProtocolType { kTcp, kConnectedUdp, kBoundUdp, kTcpServer };

  virtual ~DirectSocketsDelegate() = default;

  // Allows embedders to introduce additional rules for specific
  // addresses/ports. |lock_url| is the URL to which the renderer
  // process is locked.
  virtual bool ValidateAddressAndPort(content::BrowserContext* browser_context,
                                      const GURL& lock_url,
                                      const std::string& address,
                                      uint16_t port,
                                      ProtocolType) const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DIRECT_SOCKETS_DELEGATE_H_
