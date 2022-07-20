// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_FRAME_PROXY_H_
#define CONTENT_RENDERER_RENDER_FRAME_PROXY_H_

#include "base/memory/ref_counted.h"
#include "cc/paint/paint_canvas.h"
#include "content/common/content_export.h"
#include "content/common/frame.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-forward.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-forward.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "third_party/blink/public/web/web_remote_frame_client.h"
#include "url/origin.h"

namespace content {

class AgentSchedulingGroup;
class RenderFrameImpl;

// When a page's frames are rendered by multiple processes, each renderer has a
// full copy of the frame tree. It has full RenderFrames for the frames it is
// responsible for rendering and placeholder objects for frames rendered by
// other processes. This class is the renderer-side object for the placeholder.
// RenderFrameProxy allows us to keep existing window references valid over
// cross-process navigations and route cross-site asynchronous JavaScript calls,
// such as postMessage.
//
// For now, RenderFrameProxy is created when RenderFrame is swapped out. It
// acts as a wrapper and is used for sending and receiving IPC messages. It is
// deleted when the RenderFrame is swapped back in or the node of the frame
// tree is deleted.
//
// Long term, RenderFrameProxy will be created to replace the RenderFrame in the
// frame tree and the RenderFrame will be deleted after its unload handler has
// finished executing. It will still be responsible for routing IPC messages
// which are valid for cross-site interactions between frames.
// RenderFrameProxy will be deleted when the node in the frame tree is deleted
// or when navigating the frame causes it to return to this process and a new
// RenderFrame is created for it.
class CONTENT_EXPORT RenderFrameProxy : public blink::WebRemoteFrameClient {
 public:
  // This method should be used to create a RenderFrameProxy, which will replace
  // an existing RenderFrame during its cross-process navigation from the
  // current process to a different one. `frame_to_replace` is the frame that
  // the new proxy will eventually swap places with.
  static RenderFrameProxy* CreateProxyToReplaceFrame(
      AgentSchedulingGroup& agent_scheduling_group,
      RenderFrameImpl* frame_to_replace,
      blink::mojom::TreeScopeType tree_scope_type,
      const blink::RemoteFrameToken& proxy_frame_token);

  // This method should be used to create a RenderFrameProxy, when there isn't
  // an existing RenderFrame. It should be called to construct a local
  // representation of a RenderFrame that has been created in another process --
  // for example, after a cross-process navigation or after the addition of a
  // new frame local to some other process. `render_view_routing_id` identifies
  // the RenderView to be associated with this frame.  `opener_frame_token`, if
  // supplied, is the new frame's opener.  `parent_frame_token`, if supplied,
  // is the frame token of the RenderFrameProxy to which the new frame is
  // parented.
  //
  // `parent_frame_token` always identifies a remote frame (never a
  // local frame) because a new child of a local frame should always start out
  // as a frame, not a proxy.
  static RenderFrameProxy* CreateFrameProxy(
      AgentSchedulingGroup& agent_scheduling_group,
      const blink::RemoteFrameToken& frame_token,
      const absl::optional<blink::FrameToken>& opener_frame_token,
      int render_view_routing_id,
      const absl::optional<blink::RemoteFrameToken>& parent_frame_token,
      blink::mojom::TreeScopeType tree_scope_type,
      blink::mojom::FrameReplicationStatePtr replicated_state,
      const base::UnguessableToken& devtools_frame_token,
      mojom::RemoteFrameInterfacesFromBrowserPtr remote_frame_interfaces,
      mojom::RemoteMainFrameInterfacesPtr remote_main_frame_interfaces);

  // Creates a RenderFrameProxy to be used with a portal or fenced frame owned
  // by `parent`.
  static RenderFrameProxy* CreateProxyForPortalOrFencedFrame(
      AgentSchedulingGroup& agent_scheduling_group,
      RenderFrameImpl* parent,
      const blink::RemoteFrameToken& frame_token,
      blink::mojom::FrameReplicationStatePtr replicated_state,
      const base::UnguessableToken& devtools_frame_token,
      const blink::WebElement& frame_owner_element,
      mojo::PendingAssociatedRemote<blink::mojom::RemoteFrameHost> frame_host,
      mojo::PendingAssociatedReceiver<blink::mojom::RemoteFrame> frame);

  RenderFrameProxy(const RenderFrameProxy&) = delete;
  RenderFrameProxy& operator=(const RenderFrameProxy&) = delete;

  ~RenderFrameProxy() override;

  blink::WebRemoteFrame* web_frame() { return web_frame_; }

  // blink::WebRemoteFrameClient implementation:
  void FrameDetached(DetachType type) override;

  void DidStartLoading();

 private:
  explicit RenderFrameProxy();

  void Init(blink::WebRemoteFrame* frame);

  // Stores the WebRemoteFrame we are associated with.
  blink::WebRemoteFrame* web_frame_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_FRAME_PROXY_H_
