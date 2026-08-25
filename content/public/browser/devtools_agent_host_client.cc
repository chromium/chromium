// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/devtools_agent_host_client.h"

namespace content {

bool DevToolsAgentHostClient::MayAttachToRenderFrameHost(
    RenderFrameHost* render_frame_host) {
  return true;
}

bool DevToolsAgentHostClient::MayAttachToURL(const GURL& url, bool is_webui) {
  return true;
}

// Defaults to true, restricted clients must override this to false.
bool DevToolsAgentHostClient::IsTrusted() {
  return true;
}

// File access is allowed by default, only restricted clients that represent
// not entirely trusted protocol peers override this to false.
bool DevToolsAgentHostClient::MayReadLocalFiles() {
  return true;
}

bool DevToolsAgentHostClient::MayWriteLocalFiles() {
  return true;
}

bool DevToolsAgentHostClient::UsesBinaryProtocol() {
  return false;
}

// Only clients that already have powers of local code execution should override
// this to true.
bool DevToolsAgentHostClient::AllowUnsafeOperations() {
  return false;
}

std::optional<url::Origin>
DevToolsAgentHostClient::GetNavigationInitiatorOrigin() {
  return std::nullopt;
}

std::string DevToolsAgentHostClient::GetTypeForMetrics() {
  return "Other";
}

}  // namespace content
