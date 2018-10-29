// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_CLIENT_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_CLIENT_H_

#include <string>

#include "content/common/content_export.h"

namespace content {

class DevToolsAgentHost;
class RenderFrameHost;

// DevToolsAgentHostClient can attach to a DevToolsAgentHost and start
// debugging it.
class CONTENT_EXPORT DevToolsAgentHostClient {
 public:
  virtual ~DevToolsAgentHostClient() {}

  // Dispatches given protocol message on the client.
  virtual void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                                       const std::string& message) = 0;

  // This method is called when attached agent host is closed.
  virtual void AgentHostClosed(DevToolsAgentHost* agent_host) = 0;

  // Returns true if the client is allowed to attach to the given renderer.
  // Note: this method may be called before navigation commits.
  virtual bool MayAttachToRenderer(content::RenderFrameHost* render_frame_host,
                                   bool is_webui);

  // Returns true if the client is allowed to attach to the browser agent host.
  virtual bool MayAttachToBrowser();

  // Returns true if the client is allowed to discover other DevTools targets.
  // If not, it will be restricted to auto-attaching to related targets.
  virtual bool MayDiscoverTargets();

  // Returns true if the client is allowed to affect local files over the
  // protocol. Example would be manipulating a deault downloads path.
  virtual bool MayAffectLocalFiles();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_CLIENT_H_
