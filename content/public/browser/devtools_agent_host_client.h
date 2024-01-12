// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_CLIENT_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_CLIENT_H_

#include <optional>

#include "base/containers/span.h"
#include "content/common/content_export.h"
#include "url/origin.h"

class GURL;
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
                                       base::span<const uint8_t> message) = 0;

  // This method is called when attached agent host is closed.
  virtual void AgentHostClosed(DevToolsAgentHost* agent_host) = 0;

  // Returns true if the client is allowed to attach to the given URL.
  // Note: this method may be called before navigation commits.
  virtual bool MayAttachToURL(const GURL& url, bool is_webui);

  // Returns true if the client is allowed to attach to the given
  // RenderFrameHost.
  virtual bool MayAttachToRenderFrameHost(RenderFrameHost* render_frame_host);

  // Returns true if the client is considered to be in the same trust domain
  // from security perspective. It implies that the client is allowed to attach
  // to the browser agent host and perform other privileged operations. Browser
  // client is allowed to discover other DevTools targets and generally
  // manipulate browser altogether.
  virtual bool IsTrusted();

  // Returns true if the client is allowed to read local files over the
  // protocol. Example would be exposing file content to the page under debug.
  virtual bool MayReadLocalFiles();

  // Returns true if the client is allowed to write local files over the
  // protocol. Example would be manipulating a deault downloads path.
  virtual bool MayWriteLocalFiles();

  // Returns true if the client is allowed to perform operations
  // they may potentially be used to gain privileges, e.g. providing
  // JS compilation cache entries. This should only be true for clients
  // that are already privileged, such as local automation clients.
  virtual bool AllowUnsafeOperations();

  // A value to use as NavigationController::LoadURLParams::initiator_origin.
  // If set, navigations would also be treated as renderer-initiated.
  // This is useful e.g. for Chrome Extensions so that their calls to
  // Page.navigate would be treated as renderer-initiated naviation subject to
  // URL spoofing protection.
  virtual std::optional<url::Origin> GetNavigationInitiatorOrigin();

  // Determines protocol message format.
  virtual bool UsesBinaryProtocol();

  // Returns "DevTools" | "Extension" | "RemoteDebugger" | "Other", which is
  // used to emit to the correct UMA histogram.
  virtual std::string GetTypeForMetrics();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_CLIENT_H_
