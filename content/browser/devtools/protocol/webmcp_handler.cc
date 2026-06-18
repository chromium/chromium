// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/webmcp_handler.h"

#include "base/functional/bind.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/protocol/handler_helpers.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/inspector_protocol/crdtp/json.h"

namespace content {
namespace protocol {

WebMCPHandler::WebMCPHandler()
    : DevToolsDomainHandler(WebMCP::Metainfo::domainName), host_(nullptr) {}

WebMCPHandler::~WebMCPHandler() = default;

void WebMCPHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<WebMCP::Frontend>(dispatcher->channel());
  WebMCP::Dispatcher::wire(dispatcher, this);
}

void WebMCPHandler::SetRenderer(int process_host_id,
                                RenderFrameHostImpl* frame_host) {
  host_ = frame_host;
  if (enabled_) {
    WebContentsObserver::Observe(host_ ? WebContents::FromRenderFrameHost(host_)
                                       : nullptr);
  }
}

Response WebMCPHandler::Enable() {
  enabled_ = true;
  WebContentsObserver::Observe(host_ ? WebContents::FromRenderFrameHost(host_)
                                     : nullptr);
  return Response::FallThrough();
}

Response WebMCPHandler::Disable() {
  enabled_ = false;
  WebContentsObserver::Observe(nullptr);
  return Response::FallThrough();
}

Response WebMCPHandler::CancelInvocation(const std::string& invocation_id) {
  return Response::FallThrough();
}

void WebMCPHandler::InvokeTool(const std::string& frame_id,
                               const std::string& tool_name,
                               std::unique_ptr<protocol::DictionaryValue> input,
                               std::unique_ptr<InvokeToolCallback> callback) {
  if (!enabled_) {
    callback->sendFailure(
        Response::ServerError("WebMCP domain is not enabled"));
    return;
  }
  if (!host_) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  FrameTreeNode* frame_tree_node =
      FrameTreeNodeFromDevToolsFrameToken(host_->frame_tree_node(), frame_id);

  if (!frame_tree_node) {
    callback->sendFailure(
        Response::InvalidParams("No frame for given id found"));
    return;
  }

  if (RenderFrameDevToolsAgentHost::GetFor(frame_tree_node) !=
      RenderFrameDevToolsAgentHost::GetFor(host_)) {
    callback->sendFailure(
        Response::InvalidParams("FrameId does not belong to current target"));
    return;
  }

  RenderFrameHostImpl* rfh = frame_tree_node->current_frame_host();

  std::string input_arguments;
  if (input) {
    std::vector<uint8_t> cbor;
    crdtp::ProtocolTypeTraits<protocol::DictionaryValue>::Serialize(*input,
                                                                    &cbor);
    crdtp::json::ConvertCBORToJSON(
        crdtp::span<uint8_t>(cbor.data(), cbor.size()), &input_arguments);
  } else {
    input_arguments = "{}";
  }

  base::UnguessableToken invocation_token = base::UnguessableToken::Create();
  initiated_invocations_[invocation_token] = rfh->GetLastCommittedOrigin();

  rfh->GetAssociatedLocalFrame()->InvokeScriptToolForInspector(
      invocation_token, tool_name, input_arguments,
      base::BindOnce(
          [](std::unique_ptr<InvokeToolCallback> callback,
             base::UnguessableToken invocation_token, bool success) {
            if (success) {
              callback->sendSuccess(invocation_token.ToString());
            } else {
              callback->sendFailure(Response::InvalidParams("Tool not found"));
            }
          },
          std::move(callback), invocation_token));
}

void WebMCPHandler::DidFinishNavigation(NavigationHandle* navigation_handle) {
  if (!enabled_ || !navigation_handle->HasCommitted() ||
      navigation_handle->IsErrorPage()) {
    return;
  }

  std::optional<base::UnguessableToken> invocation_id =
      navigation_handle->GetScriptToolInvocationId();
  if (!invocation_id) {
    return;
  }

  auto it = initiated_invocations_.find(*invocation_id);
  if (it == initiated_invocations_.end()) {
    return;
  }

  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      navigation_handle->GetRenderFrameHost());

  // Note: This differs from how the normal Web Platform API (Blink and
  // //content) handles this case. The normal API only tells the invoker that an
  // execution has been canceled/failed when the document hosting the tool is
  // destroyed. Here (and in the //chrome Actor implementation), we cancel tool
  // calls whenever a navigation has happened, regardless of the bf-cache status
  // of the previous document. This means if a tool is invoked in DevTools and
  // navigates cross-origin, DevTools receives an error immediately, rather than
  // hanging until the document is destroyed.
  if (!rfh->GetLastCommittedOrigin().IsSameOriginWith(it->second)) {
    frontend_->ToolResponded(
        invocation_id->ToString(), WebMCP::InvocationStatusEnum::Error, nullptr,
        "Cannot return tool results after a cross-origin navigation");
    initiated_invocations_.erase(it);
    return;
  }

  rfh->GetAssociatedLocalFrame()
      ->NotifyInspectorOfCrossDocumentScriptToolResult(*invocation_id);
  initiated_invocations_.erase(it);
}

}  // namespace protocol
}  // namespace content
