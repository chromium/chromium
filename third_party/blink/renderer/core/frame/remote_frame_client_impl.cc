// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/remote_frame_client_impl.h"

#include <memory>
#include <utility>

#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom-blink.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/events/wheel_event.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/frame/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

RemoteFrameClientImpl::RemoteFrameClientImpl(WebRemoteFrameImpl* web_frame)
    : web_frame_(web_frame) {}

void RemoteFrameClientImpl::Trace(Visitor* visitor) const {
  visitor->Trace(web_frame_);
  RemoteFrameClient::Trace(visitor);
}

bool RemoteFrameClientImpl::InShadowTree() const {
  return web_frame_->GetTreeScopeType() == mojom::blink::TreeScopeType::kShadow;
}

void RemoteFrameClientImpl::Detached(FrameDetachType type) {
  // We only notify the browser process when the frame is being detached for
  // removal, not after a swap.
  if (type == FrameDetachType::kRemove &&
      web_frame_->GetFrame()->IsRemoteFrameHostRemoteBound()) {
    web_frame_->GetFrame()->GetRemoteFrameHostRemote().Detach();
  }
  web_frame_->Close();

  if (web_frame_->Parent()) {
    if (type == FrameDetachType::kRemove)
      WebFrame::ToCoreFrame(*web_frame_)->DetachFromParent();
  } else if (auto* view = web_frame_->View()) {
    // This could be a RemoteFrame that doesn't have a parent (fenced frames)
    // but not actually the `view`'s main frame.
    if (view->MainFrame() == web_frame_) {
      // If the RemoteFrame being detached is also the main frame in the
      // renderer process, we need to notify the webview to allow it to clean
      // things up.
      view->DidDetachRemoteMainFrame();
    }
  }

  // Clear our reference to RemoteFrame at the very end, in case the client
  // refers to it.
  web_frame_->SetCoreFrame(nullptr);
}

void RemoteFrameClientImpl::CreateRemoteChild(
    const RemoteFrameToken& token,
    const std::optional<FrameToken>& opener_frame_token,
    mojom::blink::TreeScopeType tree_scope_type,
    mojom::blink::FrameReplicationStatePtr replication_state,
    mojom::blink::FrameOwnerPropertiesPtr owner_properties,
    bool is_loading,
    const base::UnguessableToken& devtools_frame_token,
    mojom::blink::RemoteFrameInterfacesFromBrowserPtr remote_frame_interfaces) {
  CreateRemoteChildImpl(
      token, opener_frame_token, tree_scope_type, std::move(replication_state),
      std::move(owner_properties), is_loading, devtools_frame_token,
      std::move(remote_frame_interfaces));
}

unsigned RemoteFrameClientImpl::BackForwardLength() {
  return To<WebViewImpl>(web_frame_->View())->HistoryListLength();
}

void RemoteFrameClientImpl::CreateRemoteChildren(
    const Vector<mojom::blink::CreateRemoteChildParamsPtr>& params) {
  for (const auto& child_param : params) {
    WebRemoteFrameImpl* new_child = CreateRemoteChildImpl(
        child_param->token, child_param->opener_frame_token,
        child_param->tree_scope_type, std::move(child_param->replication_state),
        std::move(child_param->owner_properties), child_param->is_loading,
        child_param->devtools_frame_token,
        std::move(child_param->frame_interfaces));
    new_child->frame_client_->CreateRemoteChildren(child_param->child_params);
  }
}

WebRemoteFrameImpl* RemoteFrameClientImpl::CreateRemoteChildImpl(
    const RemoteFrameToken& token,
    const std::optional<FrameToken>& opener_frame_token,
    mojom::blink::TreeScopeType tree_scope_type,
    mojom::blink::FrameReplicationStatePtr replication_state,
    mojom::blink::FrameOwnerPropertiesPtr owner_properties,
    bool is_loading,
    const base::UnguessableToken& devtools_frame_token,
    mojom::blink::RemoteFrameInterfacesFromBrowserPtr remote_frame_interfaces) {
  WebFrame* opener = nullptr;
  if (opener_frame_token)
    opener = WebFrame::FromFrameToken(opener_frame_token.value());
  return web_frame_->CreateRemoteChild(
      tree_scope_type, token, is_loading, devtools_frame_token, opener,
      std::move(remote_frame_interfaces->frame_host),
      std::move(remote_frame_interfaces->frame_receiver),
      std::move(replication_state), std::move(owner_properties));
}

}  // namespace blink
