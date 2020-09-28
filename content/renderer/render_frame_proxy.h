// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_FRAME_PROXY_H_
#define CONTENT_RENDERER_RENDER_FRAME_PROXY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/paint/paint_canvas.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/common/content_export.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_proxy.mojom.h"
#include "content/renderer/child_frame_compositor.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/common/widget/screen_info.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-forward.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-forward.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "third_party/blink/public/web/web_remote_frame_client.h"
#include "url/origin.h"

namespace blink {
struct WebRect;
}

namespace content {

class AgentSchedulingGroup;
class BlinkInterfaceRegistryImpl;
class ChildFrameCompositingHelper;
class RenderFrameImpl;
class RenderViewImpl;
class RenderWidget;
struct FrameReplicationState;

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
class CONTENT_EXPORT RenderFrameProxy : public IPC::Listener,
                                        public IPC::Sender,
                                        public ChildFrameCompositor,
                                        public blink::WebRemoteFrameClient,
                                        public mojom::RenderFrameProxy {
 public:
  // This method should be used to create a RenderFrameProxy, which will replace
  // an existing RenderFrame during its cross-process navigation from the
  // current process to a different one. |routing_id| will be ID of the newly
  // created RenderFrameProxy. |frame_to_replace| is the frame that the new
  // proxy will eventually swap places with.
  static RenderFrameProxy* CreateProxyToReplaceFrame(
      AgentSchedulingGroup& agent_scheduling_group,
      RenderFrameImpl* frame_to_replace,
      int routing_id,
      blink::mojom::TreeScopeType scope,
      const base::UnguessableToken& proxy_frame_token);

  // This method should be used to create a RenderFrameProxy, when there isn't
  // an existing RenderFrame. It should be called to construct a local
  // representation of a RenderFrame that has been created in another process --
  // for example, after a cross-process navigation or after the addition of a
  // new frame local to some other process. |routing_id| will be the ID of the
  // newly created RenderFrameProxy. |render_view_routing_id| identifies the
  // RenderView to be associated with this frame.  |opener|, if supplied, is the
  // new frame's opener.  |parent_routing_id| is the routing ID of the
  // RenderFrameProxy to which the new frame is parented.
  //
  // |parent_routing_id| always identifies a RenderFrameProxy (never a
  // RenderFrame) because a new child of a local frame should always start out
  // as a frame, not a proxy.
  static RenderFrameProxy* CreateFrameProxy(
      AgentSchedulingGroup& agent_scheduling_group,
      int routing_id,
      int render_view_routing_id,
      const base::Optional<base::UnguessableToken>& opener_frame_token,
      int parent_routing_id,
      const FrameReplicationState& replicated_state,
      const base::UnguessableToken& frame_token,
      const base::UnguessableToken& devtools_frame_token);

  // Creates a RenderFrameProxy to be used with a portal owned by |parent|.
  // |routing_id| is the routing id of this new RenderFrameProxy.
  static RenderFrameProxy* CreateProxyForPortal(
      AgentSchedulingGroup& agent_scheduling_group,
      RenderFrameImpl* parent,
      int proxy_routing_id,
      const base::UnguessableToken& frame_token,
      const base::UnguessableToken& devtools_frame_token,
      const blink::WebElement& portal_element);

  // Returns the RenderFrameProxy for the given routing ID.
  static RenderFrameProxy* FromRoutingID(int routing_id);

  // Returns the RenderFrameProxy given a WebRemoteFrame. |web_frame| must not
  // be null, nor will this method return null.
  static RenderFrameProxy* FromWebFrame(blink::WebRemoteFrame* web_frame);

  ~RenderFrameProxy() override;

  // IPC::Sender
  bool Send(IPC::Message* msg) override;

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  // Propagate VisualProperties updates from a local root RenderWidget to the
  // child RenderWidget represented by this proxy, which is hosted in another
  // renderer frame tree.
  // TODO(danakj): These should all be grouped into a single method, then we
  // would only get one update per UpdateVisualProperties IPC received in the
  // RenderWidget, and we would only need to send one update to the browser as
  // a result.
  void DidChangeScreenInfo(const blink::ScreenInfo& screen_info) override;
  void DidChangeRootWindowSegments(
      const std::vector<gfx::Rect>& root_widget_window_segments) override;
  void DidChangeVisibleViewportSize(
      const gfx::Size& visible_viewport_size) override;

  // Pass replicated information, such as security origin, to this
  // RenderFrameProxy's WebRemoteFrame.
  void SetReplicatedState(const FrameReplicationState& state);

  int routing_id() { return routing_id_; }
  RenderViewImpl* render_view() { return render_view_; }
  blink::WebRemoteFrame* web_frame() { return web_frame_; }
  std::string unique_name() const;

  void set_provisional_frame_routing_id(int routing_id) {
    provisional_frame_routing_id_ = routing_id;
  }

  int provisional_frame_routing_id() { return provisional_frame_routing_id_; }

  void SynchronizeVisualProperties();

  const gfx::Rect& screen_space_rect() const {
    return pending_visual_properties_.screen_space_rect;
  }

  const gfx::Size& local_frame_size() const {
    return pending_visual_properties_.local_frame_size;
  }

  const blink::ScreenInfo& screen_info() const {
    return pending_visual_properties_.screen_info;
  }

  // blink::WebRemoteFrameClient implementation:
  void FrameDetached(DetachType type) override;
  blink::AssociatedInterfaceProvider* GetRemoteAssociatedInterfaces() override;

  void Navigate(
      const blink::WebURLRequest& request,
      blink::WebLocalFrame* initiator_frame,
      bool should_replace_current_entry,
      bool is_opener_navigation,
      bool initiator_frame_has_download_sandbox_flag,
      bool blocking_downloads_in_sandbox_enabled,
      bool initiator_frame_is_ad,
      blink::CrossVariantMojoRemote<blink::mojom::BlobURLTokenInterfaceBase>
          blob_url_token,
      const base::Optional<blink::WebImpression>& impression) override;
  void FrameRectsChanged(const blink::WebRect& local_frame_rect,
                         const blink::WebRect& screen_space_rect) override;
  void UpdateRemoteViewportIntersection(
      const blink::ViewportIntersectionState& intersection_state) override;
  base::UnguessableToken GetDevToolsFrameToken() override;
  void ZoomLevelChanged(double zoom_level) override;
  void UpdateCaptureSequenceNumber(uint32_t capture_sequence_number) override;
  void PageScaleFactorChanged(float page_scale_factor,
                              bool is_pinch_gesture_active) override;
  viz::FrameSinkId GetFrameSinkId() override;
  void WasEvicted() override;

  void DidStartLoading();

  bool is_pinch_gesture_active_for_testing() {
    return pending_visual_properties_.is_pinch_gesture_active;
  }

  // Called when the associated FrameSinkId has changed.
  void FrameSinkIdChanged(const viz::FrameSinkId& frame_sink_id);

 private:
  RenderFrameProxy(AgentSchedulingGroup& agent_scheduling_group,
                   int routing_id);

  void Init(blink::WebRemoteFrame* frame,
            RenderViewImpl* render_view,
            RenderWidget* render_widget,
            bool parent_is_local);

  void ResendVisualProperties();

  mojom::RenderFrameProxyHost* GetFrameProxyHost();

  // IPC handlers
  void OnDeleteProxy();
  void OnCompositorFrameSwapped(const IPC::Message& message);
  void OnEnforceInsecureRequestPolicy(
      blink::mojom::InsecureRequestPolicy policy);

  // mojom::RenderFrameProxy implementation:
  void EnableAutoResize(const gfx::Size& min_size,
                        const gfx::Size& max_size) override;
  void DisableAutoResize() override;
  void DidUpdateVisualProperties(
      const cc::RenderFrameMetadata& metadata) override;
  void ChildProcessGone() override;
  void SetFrameSinkId(const viz::FrameSinkId& frame_sink_id) override;

  // ChildFrameCompositor:
  cc::Layer* GetLayer() override;
  void SetLayer(scoped_refptr<cc::Layer> layer,
                bool prevent_contents_opaque_changes,
                bool is_surface_layer) override;
  SkBitmap* GetSadPageBitmap() override;

  const viz::LocalSurfaceId& GetLocalSurfaceId() const;

  // The |AgentSchedulingGroup| this proxy is associated with. NOTE: This is
  // different than the |AgentSchedulingGroup| associated with the frame being
  // proxied.
  AgentSchedulingGroup& agent_scheduling_group_;

  // The routing ID by which this RenderFrameProxy is known.
  const int routing_id_;

  // The routing ID of the provisional RenderFrame (if any) that is meant to
  // replace this RenderFrameProxy in the frame tree.
  int provisional_frame_routing_id_;

  // Stores the WebRemoteFrame we are associated with.
  blink::WebRemoteFrame* web_frame_ = nullptr;

  // Provides the mojo interface to this RenderFrameProxy's
  // RenderFrameProxyHost.
  mojo::AssociatedRemote<mojom::RenderFrameProxyHost> frame_proxy_host_remote_;
  std::unique_ptr<blink::AssociatedInterfaceProvider>
      remote_associated_interfaces_;

  // Mojo receiver to this content::mojom::RenderFrameProxy.
  mojo::AssociatedReceiver<mojom::RenderFrameProxy>
      render_frame_proxy_receiver_{this};

  // Can be nullptr when this RenderFrameProxy's parent is not a RenderFrame.
  std::unique_ptr<ChildFrameCompositingHelper> compositing_helper_;

  RenderViewImpl* render_view_ = nullptr;

  // The RenderWidget of the nearest ancestor local root. If the proxy has no
  // local root ancestor (eg it is a proxy of the root frame) then the pointer
  // is null.
  RenderWidget* ancestor_render_widget_ = nullptr;

  // Contains token to be used as a frame id in the devtools protocol.
  // It is derived from the content's devtools_frame_token, is
  // defined by the browser and passed into Blink upon frame creation.
  base::UnguessableToken devtools_frame_token_;

  // TODO(fsamuel): Most RenderFrameProxys don't host viz::Surfaces and
  // therefore don't care to synchronize ResizeParams with viz::LocalSurfaceIds.
  // Perhaps this can be moved to ChildFrameCompositingHelper?
  // The last ResizeParams sent to the browser process, if any.
  base::Optional<blink::FrameVisualProperties> sent_visual_properties_;

  // The current set of ResizeParams. This may or may not match
  // |sent_visual_properties_|.
  blink::FrameVisualProperties pending_visual_properties_;

  bool crashed_ = false;

  viz::FrameSinkId frame_sink_id_;
  std::unique_ptr<viz::ParentLocalSurfaceIdAllocator>
      parent_local_surface_id_allocator_;

  // The layer used to embed the out-of-process content.
  scoped_refptr<cc::Layer> embedded_layer_;

  service_manager::BinderRegistry binder_registry_;
  blink::AssociatedInterfaceRegistry associated_interfaces_;
  std::unique_ptr<BlinkInterfaceRegistryImpl> blink_interface_registry_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameProxy);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_FRAME_PROXY_H_
