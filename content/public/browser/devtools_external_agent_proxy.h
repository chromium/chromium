// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_EXTERNAL_AGENT_PROXY_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_EXTERNAL_AGENT_PROXY_H_

#include "base/containers/span.h"

namespace content {

// Describes interface for communication with an external DevTools agent.
class DevToolsExternalAgentProxy {
 public:
  // Sends the message to the client host.
  virtual void DispatchOnClientHost(base::span<const uint8_t> message) = 0;

  // Informs the client that the connection has closed.
  virtual void ConnectionClosed() = 0;

  virtual ~DevToolsExternalAgentProxy() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_EXTERNAL_AGENT_PROXY_H_
