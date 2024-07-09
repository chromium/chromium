// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_PROXY_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_PROXY_HOST_H_

#include <stdint.h>

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/content_export.h"
#include "content/common/frame.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-forward.h"
#include "third_party/blink/public/mojom/messaging/transferable_message.mojom-forward.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-forward.h"

namespace blink {
class AssociatedInterfaceProvider;
}

namespace gfx {
class Rect;
class RectF;
}

namespace perfetto {
namespace protos {
namespace pbzero {
class RenderFrameProxyHost;
}
}  // namespace protos
}  // namespace perfetto

namespace content {

class BatchedProxyIPCSender;
class CrossProcessFrameConnector;
class FrameTreeNode;
class RenderViewHostImpl;
class RenderWidgetHostViewChildFrame;
class SiteInstanceGroup;

// When a page's frames are rendered by multiple processes, each renderer has a
// full copy of the frame tree. It has full RenderFrames for the frames it is
// responsible for rendering and placeholder objects (i.e.,
// `blink::RemoteFrame`) for frames rendered by other processes.
//
// This class is the browser-side host object for the placeholder. Each node in
// the frame tree has a RenderFrameHost for the active SiteInstance and a set
// of RenderFrameProxyHost objects - one for all other SiteInstanceGroups with
// references to this frame. The proxies allow us to keep existing window
// references valid over cross-process navigations and route cross-site
// asynchronous JavaScript calls, such as postMessage.
//
// RenderFrameProxyHost is created whenever a cross-site
// navigation occurs and a reference to the frame navigating needs to be kept
// alive. A RenderFrameProxyHost and a RenderFrameHost in the same
// SiteInstanceGroup can exist at the same time, but only one will be "active"
// at a time. There are two cases where the two objects will coexist:
// * When navigating cross-process and there is already a RenderFrameProxyHost
// for the new SiteInstanceGroup. A pending RenderFrameHost is created, but it
// is not used until it commits. At that point, RenderFrameHostManager
// transitions the pending RenderFrameHost to the active one and deletes the
// proxy.
// * When navigating cross-process and the existing document has an unload
// event handler. When the new navigation commits, RenderFrameHostManager
// creates a RenderFrameProxyHost for the old SiteInstanceGroup and uses it
// going forward. It also instructs the RenderFrameHost to run the unload event
// handler and is kept alive for the duration. Once the event handling is
// complete, the RenderFrameHost is deleted.
class CONTENT_EXPORT RenderFrameProxyHost
    : public IPC::Listener,
      public IPC::Sender,
      public blink::mojom::RemoteFrameHost,
      public blink::mojom::RemoteMainFrameHost {
 public:
  // A test observer to monitor RenderFrameProxyHosts.
  class TestObserver {
   public:
    virtual ~TestObserver() = default;
    // Called when a RenderFrameProxyHost is created.
    virtual void OnCreated(RenderFrameProxyHost* host) {}
    // Called when a RenderFrameProxyHost is deleted.
    virtual void OnDeleted(RenderFrameProxyHost* host) {}
    // Called when Remote/RemoteMainFrame mojo channels are bound to a
    // RenderFrameProxyHost.
    virtual void OnRemoteFrameBound(RenderFrameProxyHost* host) {}
    virtual void OnRemoteMainFrameBound(RenderFrameProxyHost* host) {}
  };

  static void SetObserverForTesting(TestObserver* observer);

  static RenderFrameProxyHost* FromID(int process_id, int routing_id);
  static RenderFrameProxyHost* FromFrameToken(
      int process_id,
      const blink::RemoteFrameToken& frame_token);
  static bool IsFrameTokenInUse(const blink::RemoteFrameToken& frame_token);

  RenderFrameProxyHost(SiteInstanceGroup* site_instance_group,
                       scoped_refptr<RenderViewHostImpl> render_view_host,
                       FrameTreeNode* frame_tree_node,
                       const blink::RemoteFrameToken& frame_token);

  RenderFrameProxyHost(const RenderFrameProxyHost&) = delete;
  RenderFrameProxyHost& operator=(const RenderFrameProxyHost&) = delete;

  ~RenderFrameProxyHost() override;

  RenderProcessHost* GetProcess() const { return process_; }

  // Initializes the object and creates the `blink::RemoteFrame` in the process
  // for the `site_instance_group_`. If `batched_proxy_ipc_sender` is not null,
  // then the proxy will not be created immediately. It will be batch created
  // later.
  bool InitRenderFrameProxy(
      BatchedProxyIPCSender* batched_proxy_ipc_sender = nullptr);

  int GetRoutingID() const { return routing_id_; }
  GlobalRoutingID GetGlobalID() const {
    return GlobalRoutingID(GetProcess()->GetID(), routing_id_);
  }

  // Each RenderFrameProxyHost belongs to a SiteInstanceGroup, where it is a
  // placeholder for a frame in a different SiteInstanceGroup.
  SiteInstanceGroup* site_instance_group() const {
    return site_instance_group_.get();
  }

  // TODO(crbug.com/40169570): FrameTree and FrameTreeNode are not const
  // as with prerenderer activation the page needs to move between
  // FrameTreeNodes and FrameTrees. Note that FrameTreeNode can only change for
  // root nodes. As it's hard to make sure that all places handle this
  // transition correctly, MPArch will remove references from this class to
  // FrameTree/FrameTreeNode.
  FrameTreeNode* frame_tree_node() const { return frame_tree_node_; }

  void set_frame_tree_node(FrameTreeNode& frame_tree_node) {
    frame_tree_node_ = &frame_tree_node;
  }

  // Associates the RenderWidgetHostViewChildFrame |view| with this
  // RenderFrameProxyHost. If |initial_frame_size| isn't specified at this time,
  // the child frame will wait until the CrossProcessFrameConnector
  // receives its size from the parent via FrameHostMsg_UpdateResizeParams
  // before it begins parsing the content.
  void SetChildRWHView(RenderWidgetHostViewChildFrame* view,
                       const gfx::Size* initial_frame_size,
                       bool allow_paint_holding);

  RenderViewHostImpl* GetRenderViewHost();

  // IPC::Sender
  bool Send(IPC::Message* msg) override;

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& msg) override;
  std::string ToDebugString() override;

  CrossProcessFrameConnector* cross_process_frame_connector() {
    return cross_process_frame_connector_.get();
  }

  // Update the frame's opener in the renderer process in response to the
  // opener being modified (e.g., with window.open or being set to null) in
  // another renderer process.
  void UpdateOpener();

  // Set this proxy as the focused frame in the renderer process.  This is
  // called to replicate the focused frame when a frame in a different process
  // becomes focused.
  void SetFocusedFrame();

  // Scroll |rect_to_scroll| into view, starting from this proxy's FrameOwner
  // element in the frame's parent. Calling this continues a scroll started in
  // the frame's current process. |rect_to_scroll| is with respect to the
  // coordinates of the originating frame in OOPIF process.
  void ScrollRectToVisible(const gfx::RectF& rect_to_scroll,
                           blink::mojom::ScrollIntoViewParamsPtr params);

  // Sets render frame proxy created state. If |created| is false, any existing
  // mojo connections to RenderFrameProxyHost will be closed.
  void SetRenderFrameProxyCreated(bool created);

  // Returns if the `blink::RemoteFrame` for this host is alive.
  bool is_render_frame_proxy_live() const {
    return render_frame_proxy_created_;
  }

  // Returns associated remote for the blink::mojom::RemoteFrame Mojo interface.
  const mojo::AssociatedRemote<blink::mojom::RemoteFrame>&
  GetAssociatedRemoteFrame();

  // Returns associated remote for the blink::mojom::RemoteMainFrame Mojo
  // interface.
  const mojo::AssociatedRemote<blink::mojom::RemoteMainFrame>&
  GetAssociatedRemoteMainFrame();

  // blink::mojom::RemoteFrameHost
  void SetInheritedEffectiveTouchAction(cc::TouchAction touch_action) override;
  void UpdateRenderThrottlingStatus(bool is_throttled,
                                    bool subtree_throttled,
                                    bool display_locked) override;
  void VisibilityChanged(blink::mojom::FrameVisibility visibility) override;
  void DidFocusFrame() override;
  void CheckCompleted() override;
  void CapturePaintPreviewOfCrossProcessSubframe(
      const gfx::Rect& clip_rect,
      const base::UnguessableToken& guid) override;
  void SetIsInert(bool inert) override;
  void DidChangeOpener(
      const std::optional<blink::LocalFrameToken>& opener_frame_token) override;
  void AdvanceFocus(blink::mojom::FocusType focus_type,
                    const blink::LocalFrameToken& source_frame_token) override;
  void RouteMessageEvent(
      const std::optional<blink::LocalFrameToken>& source_frame_token,
      const url::Origin& source_origin,
      const std::u16string& target_origin,
      blink::TransferableMessage message) override;
  void PrintCrossProcessSubframe(const gfx::Rect& rect,
                                 int document_cookie) override;
  void Detach() override;
  void UpdateViewportIntersection(
      blink::mojom::ViewportIntersectionStatePtr intersection_state,
      const std::optional<blink::FrameVisualProperties>& visual_properties)
      override;
  void SynchronizeVisualProperties(
      const blink::FrameVisualProperties& frame_visual_properties) override;
  void OpenURL(blink::mojom::OpenURLParamsPtr params) override;

  // blink::mojom::RemoteMainFrameHost overrides:
  void FocusPage() override;
  void TakeFocus(bool reverse) override;
  void UpdateTargetURL(
      const GURL& url,
      blink::mojom::RemoteMainFrameHost::UpdateTargetURLCallback callback)
      override;
  void RouteCloseEvent() override;

  // Requests a viz::LocalSurfaceId to enable auto-resize mode from the parent
  // renderer.
  void EnableAutoResize(const gfx::Size& min_size, const gfx::Size& max_size);
  // Requests a viz::LocalSurfaceId to disable auto-resize mode from the parent
  // renderer.
  void DisableAutoResize();
  void DidUpdateVisualProperties(const cc::RenderFrameMetadata& metadata);
  void ChildProcessGone();

  bool IsInertForTesting();

  mojo::PendingAssociatedReceiver<blink::mojom::RemoteFrame>
  BindRemoteFrameReceiverForTesting();
  mojo::PendingAssociatedReceiver<blink::mojom::RemoteMainFrame>
  BindRemoteMainFrameReceiverForTesting();

  const blink::RemoteFrameToken& GetFrameToken() const { return frame_token_; }

  // Bind mojo endpoints of the Remote/RemoteMainFrame in blink and pass unbound
  // corresponding endpoints. The corresponding endpoints should be transferred
  // and bound in blink.
  blink::mojom::RemoteFrameInterfacesFromBrowserPtr
  CreateAndBindRemoteFrameInterfaces();
  blink::mojom::RemoteMainFrameInterfacesPtr
  CreateAndBindRemoteMainFrameInterfaces();

  // Bind mojo endpoints of the Remote/RemoteMainFrame in blink.
  void BindRemoteFrameInterfaces(
      mojo::PendingAssociatedRemote<blink::mojom::RemoteFrame>,
      mojo::PendingAssociatedReceiver<blink::mojom::RemoteFrameHost>);
  void BindRemoteMainFrameInterfaces(
      mojo::PendingAssociatedRemote<blink::mojom::RemoteMainFrame>
          remote_main_frame,
      mojo::PendingAssociatedReceiver<blink::mojom::RemoteMainFrameHost>
          remote_main_frame_host_receiver);

  // Invalidate the mojo connections between this RenderFrameProxyHost and its
  // associated instances in renderer, allowing the endpoints to be re-bound.
  // This is needed when:
  // - the renderer side object goes away due to the renderer process going away
  //   (i.e. crashing)
  // - undoing a `CommitNavigation()` that has already been sent to a
  //   speculative RenderFrameHost by swapping it back to a
  //   `blink::RemoteFrame`.
  void TearDownMojoConnection();

  using TraceProto = perfetto::protos::pbzero::RenderFrameProxyHost;
  // Write a representation of this object into a trace.
  void WriteIntoTrace(perfetto::TracedProto<TraceProto> proto) const;

  base::SafeRef<RenderFrameProxyHost> GetSafeRef();

 private:
  // These interceptors need access to frame_host_receiver_for_testing().
  friend class InitiatorClosingOpenURLInterceptor;
  friend class RemoteFrameHostInterceptor;
  friend class UpdateViewportIntersectionMessageFilter;
  friend class SynchronizeVisualPropertiesInterceptor;

  // Helper to retrieve the |AgentSchedulingGroup| this proxy host is associated
  // with.
  AgentSchedulingGroupHost& GetAgentSchedulingGroup();

  // Helper to compute the serialized source origin from an actual source origin
  // for postMessage. This will ultimately be exposed to JavaScript via the
  // message's event.origin field.
  std::u16string SerializePostMessageSourceOrigin(
      const url::Origin& source_origin);

  // Needed for tests to be able to swap the implementation and intercept calls.
  mojo::AssociatedReceiver<blink::mojom::RemoteFrameHost>&
  frame_host_receiver_for_testing() {
    return remote_frame_host_receiver_;
  }

  // This RenderFrameProxyHost's routing id.
  int routing_id_;

  // The SiteInstanceGroup this RenderFrameProxyHost belongs to, where it is a
  // placeholder for a frame in a different SiteInstanceGroup.
  scoped_refptr<SiteInstanceGroup> site_instance_group_;

  // The renderer process this RenderFrameProxyHost is associated with. It is
  // equivalent to the result of site_instance_group_->GetProcess(), but that
  // method has the side effect of creating the process if it doesn't exist.
  // Cache a pointer to avoid unnecessary process creation.
  raw_ptr<RenderProcessHost> process_;

  // The node in the frame tree where this proxy is located.
  raw_ptr<FrameTreeNode> frame_tree_node_;

  // True if we have a live `blink::RemoteFrame` for this host.
  bool render_frame_proxy_created_;

  // When a RenderFrameHost is in a different process from its parent in the
  // frame tree, this class connects its associated RenderWidgetHostView
  // to this RenderFrameProxyHost, which corresponds to the same frame in the
  // parent's renderer process.
  std::unique_ptr<CrossProcessFrameConnector> cross_process_frame_connector_;

  // The RenderViewHost that this RenderFrameProxyHost is associated with.
  //
  // It is kept alive as long as any RenderFrameHosts or RenderFrameProxyHosts
  // are using it.
  //
  // TODO(creis): RenderViewHost will eventually go away and be replaced with
  // some form of page context.
  scoped_refptr<RenderViewHostImpl> render_view_host_;

  std::unique_ptr<blink::AssociatedInterfaceProvider>
      remote_associated_interfaces_;

  // Holder of Mojo connection with the Frame service in Blink.
  mojo::AssociatedRemote<blink::mojom::RemoteFrame> remote_frame_;

  // Holder of Mojo connection with the RemoteMainFrame in Blink. This remote
  // will be valid when the frame is the active main frame.
  mojo::AssociatedRemote<blink::mojom::RemoteMainFrame> remote_main_frame_;

  mojo::AssociatedReceiver<blink::mojom::RemoteFrameHost>
      remote_frame_host_receiver_{this};

  mojo::AssociatedReceiver<blink::mojom::RemoteMainFrameHost>
      remote_main_frame_host_receiver_{this};

  blink::RemoteFrameToken frame_token_;

  base::WeakPtrFactory<RenderFrameProxyHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_PROXY_HOST_H_
