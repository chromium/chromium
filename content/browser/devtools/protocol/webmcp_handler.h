// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_WEBMCP_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_WEBMCP_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/web_mcp.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "url/origin.h"

namespace content {

class RenderFrameHostImpl;

namespace protocol {

class CONTENT_EXPORT WebMCPHandler : public DevToolsDomainHandler,
                                     public WebMCP::Backend,
                                     public WebContentsObserver {
 public:
  WebMCPHandler();
  ~WebMCPHandler() override;

  WebMCPHandler(const WebMCPHandler&) = delete;
  WebMCPHandler& operator=(const WebMCPHandler&) = delete;

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;

  Response Enable() override;
  Response Disable() override;

  Response CancelInvocation(const std::string& invocation_id) override;

  void InvokeTool(const std::string& frame_id,
                  const std::string& tool_name,
                  std::unique_ptr<protocol::DictionaryValue> input,
                  std::unique_ptr<InvokeToolCallback> callback) override;

  // WebContentsObserver overrides:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

 private:
  raw_ptr<RenderFrameHostImpl> host_;
  bool enabled_ = false;
  absl::flat_hash_map<base::UnguessableToken, url::Origin>
      initiated_invocations_;
  std::unique_ptr<WebMCP::Frontend> frontend_;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_WEBMCP_HANDLER_H_
