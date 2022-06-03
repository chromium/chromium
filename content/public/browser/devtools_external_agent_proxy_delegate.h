// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_EXTERNAL_AGENT_PROXY_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_EXTERNAL_AGENT_PROXY_DELEGATE_H_

#include <string>

#include "base/containers/span.h"
#include "content/common/content_export.h"

namespace content {

class DevToolsExternalAgentProxy;

// Describes the interface for sending messages to an external DevTools agent.
class DevToolsExternalAgentProxyDelegate {
 public:
  virtual ~DevToolsExternalAgentProxyDelegate() {}

  // Informs the agent that a client has attached.
  virtual void Attach(DevToolsExternalAgentProxy* proxy) = 0;

  // Informs the agent that a client has detached.
  virtual void Detach(DevToolsExternalAgentProxy* proxy) = 0;

  // Sends a message to the agent from a client.
  virtual void SendMessageToBackend(DevToolsExternalAgentProxy* proxy,
                                    base::span<const uint8_t> message) = 0;

  // Returns agent host type.
  virtual std::string GetType() = 0;

  // Returns agent host title.
  virtual std::string GetTitle() = 0;

  // Returns the agent host description.
  virtual std::string GetDescription() = 0;

  // Returns url associated with agent host.
  virtual GURL GetURL() = 0;

  // Returns the favicon url for this agent host.
  virtual GURL GetFaviconURL() = 0;

  // Returns the front-end url for this agent host.
  virtual std::string GetFrontendURL() = 0;

  // Activates agent host.
  virtual bool Activate() = 0;

  // Reloads agent host.
  virtual void Reload() = 0;

  // Reloads agent host.
  virtual bool Close() = 0;

  // Returns the time when the host was last active.
  virtual base::TimeTicks GetLastActivityTime() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_EXTERNAL_AGENT_PROXY_DELEGATE_H_
