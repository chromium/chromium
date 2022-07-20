// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_frame_proxy.h"

#include <stdint.h>
#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "content/renderer/render_view_impl.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

namespace content {

// static
RenderFrameProxy* RenderFrameProxy::CreateProxyToReplaceFrame(
    AgentSchedulingGroup& agent_scheduling_group,
    RenderFrameImpl* frame_to_replace,
    blink::mojom::TreeScopeType tree_scope_type,
    const blink::RemoteFrameToken& proxy_frame_token) {
  std::unique_ptr<RenderFrameProxy> proxy(new RenderFrameProxy());

  // When a RenderFrame is replaced by a RenderProxy, the WebRemoteFrame should
  // always come from WebRemoteFrame::create and a call to WebFrame::swap must
  // follow later.
  blink::WebRemoteFrame* web_frame = blink::WebRemoteFrame::Create(
      tree_scope_type, proxy.get(), proxy_frame_token);

  proxy->Init(web_frame);
  return proxy.release();
}

// static
RenderFrameProxy* RenderFrameProxy::CreateFrameProxy(
    AgentSchedulingGroup& agent_scheduling_group,
    const blink::RemoteFrameToken& frame_token,
    const absl::optional<blink::FrameToken>& opener_frame_token,
    int render_view_routing_id,
    const absl::optional<blink::RemoteFrameToken>& parent_frame_token,
    blink::mojom::TreeScopeType tree_scope_type,
    blink::mojom::FrameReplicationStatePtr replicated_state,
    const base::UnguessableToken& devtools_frame_token,
    mojom::RemoteFrameInterfacesFromBrowserPtr remote_frame_interfaces,
    mojom::RemoteMainFrameInterfacesPtr remote_main_frame_interfaces) {
  blink::WebRemoteFrame* parent = nullptr;
  if (parent_frame_token) {
    parent = blink::WebRemoteFrame::FromFrameToken(parent_frame_token.value());
    // It is possible that the parent proxy has been detached in this renderer
    // process, just as the parent's real frame was creating this child frame.
    // In this case, do not create the proxy. See https://crbug.com/568670.
    if (!parent)
      return nullptr;
  }

  std::unique_ptr<RenderFrameProxy> proxy(new RenderFrameProxy());
  blink::WebRemoteFrame* web_frame = nullptr;

  blink::WebFrame* opener = nullptr;
  if (opener_frame_token)
    opener = blink::WebFrame::FromFrameToken(*opener_frame_token);
  if (!parent) {
    // Create a top level WebRemoteFrame.
    RenderViewImpl* render_view =
        RenderViewImpl::FromRoutingID(render_view_routing_id);
    blink::WebView* web_view = render_view->GetWebView();
    web_frame = blink::WebRemoteFrame::CreateMainFrame(
        web_view, proxy.get(), frame_token, devtools_frame_token, opener,
        std::move(remote_frame_interfaces->frame_host),
        std::move(remote_frame_interfaces->frame_receiver),
        std::move(replicated_state));
    // Root frame proxy has no ancestors to point to their RenderWidget.

    // The WebRemoteFrame created here was already attached to the Page as its
    // main frame, so we can call WebView's DidAttachRemoteMainFrame().
    web_view->DidAttachRemoteMainFrame(
        std::move(remote_main_frame_interfaces->main_frame_host),
        std::move(remote_main_frame_interfaces->main_frame));
  } else {
    // Create a frame under an existing parent. The parent is always expected
    // to be a RenderFrameProxy, because navigations initiated by local frames
    // should not wind up here.
    web_frame = parent->CreateRemoteChild(
        tree_scope_type, proxy.get(), frame_token, devtools_frame_token, opener,
        std::move(remote_frame_interfaces->frame_host),
        std::move(remote_frame_interfaces->frame_receiver),
        std::move(replicated_state));
  }

  proxy->Init(web_frame);
  return proxy.release();
}

RenderFrameProxy* RenderFrameProxy::CreateProxyForPortalOrFencedFrame(
    AgentSchedulingGroup& agent_scheduling_group,
    RenderFrameImpl* parent,
    const blink::RemoteFrameToken& frame_token,
    blink::mojom::FrameReplicationStatePtr replicated_state,
    const base::UnguessableToken& devtools_frame_token,
    const blink::WebElement& frame_owner,
    mojo::PendingAssociatedRemote<blink::mojom::RemoteFrameHost> frame_host,
    mojo::PendingAssociatedReceiver<blink::mojom::RemoteFrame> frame) {
  auto proxy = base::WrapUnique(new RenderFrameProxy());
  blink::WebRemoteFrame* web_frame =
      blink::WebRemoteFrame::CreateForPortalOrFencedFrame(
          blink::mojom::TreeScopeType::kDocument, proxy.get(), frame_token,
          devtools_frame_token, frame_owner, std::move(frame_host),
          std::move(frame), std::move(replicated_state));
  proxy->Init(web_frame);
  return proxy.release();
}

RenderFrameProxy::RenderFrameProxy() = default;

RenderFrameProxy::~RenderFrameProxy() = default;

void RenderFrameProxy::Init(blink::WebRemoteFrame* web_frame) {
  CHECK(web_frame);
  web_frame_ = web_frame;
}

void RenderFrameProxy::DidStartLoading() {
  web_frame_->DidStartLoading();
}

void RenderFrameProxy::FrameDetached(DetachType type) {
  web_frame_->Close();
  web_frame_ = nullptr;

  delete this;
}

}  // namespace content
