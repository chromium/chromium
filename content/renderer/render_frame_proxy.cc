// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_frame_proxy.h"

#include <stdint.h>
#include <map>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "content/common/content_switches_internal.h"
#include "content/common/frame_replication_state.h"
#include "content/common/input_messages.h"
#include "content/common/page_messages.h"
#include "content/common/unfreezable_frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/impression.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/agent_scheduling_group.h"
#include "content/renderer/child_frame_compositing_helper.h"
#include "content/renderer/impression_conversions.h"
#include "content/renderer/loader/web_url_request_util.h"
#include "content/renderer/mojo/blink_interface_registry_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/common/navigation/triggering_event_info.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace content {

namespace {

// Facilitates lookup of RenderFrameProxy by routing_id.
typedef std::map<int, RenderFrameProxy*> RoutingIDProxyMap;
static base::LazyInstance<RoutingIDProxyMap>::DestructorAtExit
    g_routing_id_proxy_map = LAZY_INSTANCE_INITIALIZER;

// Facilitates lookup of RenderFrameProxy by WebRemoteFrame.
typedef std::map<blink::WebRemoteFrame*, RenderFrameProxy*> FrameProxyMap;
base::LazyInstance<FrameProxyMap>::DestructorAtExit g_frame_proxy_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
RenderFrameProxy* RenderFrameProxy::CreateProxyToReplaceFrame(
    AgentSchedulingGroup& agent_scheduling_group,
    RenderFrameImpl* frame_to_replace,
    int routing_id,
    blink::mojom::TreeScopeType scope,
    const base::UnguessableToken& proxy_frame_token) {
  CHECK_NE(routing_id, MSG_ROUTING_NONE);

  std::unique_ptr<RenderFrameProxy> proxy(
      new RenderFrameProxy(agent_scheduling_group, routing_id));
  proxy->devtools_frame_token_ = frame_to_replace->GetDevToolsFrameToken();

  // When a RenderFrame is replaced by a RenderProxy, the WebRemoteFrame should
  // always come from WebRemoteFrame::create and a call to WebFrame::swap must
  // follow later.
  blink::WebRemoteFrame* web_frame = blink::WebRemoteFrame::Create(
      scope, proxy.get(), proxy->blink_interface_registry_.get(),
      proxy->GetRemoteAssociatedInterfaces(), proxy_frame_token);

  RenderWidget* ancestor_widget = nullptr;
  bool parent_is_local = false;

  // A top level frame proxy doesn't have a RenderWidget pointer. The pointer
  // is to an ancestor local frame's RenderWidget and there are no ancestors.
  if (frame_to_replace->GetWebFrame()->Parent()) {
    if (frame_to_replace->GetWebFrame()->Parent()->IsWebLocalFrame()) {
      // If the frame was a local frame, get its local root's RenderWidget.
      ancestor_widget = frame_to_replace->GetLocalRootRenderWidget();
      parent_is_local = true;
    } else {
      // Otherwise, grab the pointer from the parent RenderFrameProxy, as
      // it would already have the correct pointer. A proxy with a proxy child
      // must be created before its child, so the first proxy in a descendant
      // chain is either the root or has a local parent frame.
      RenderFrameProxy* parent = RenderFrameProxy::FromWebFrame(
          frame_to_replace->GetWebFrame()->Parent()->ToWebRemoteFrame());
      ancestor_widget = parent->ancestor_render_widget_;
    }
  }

  proxy->Init(web_frame, frame_to_replace->render_view(), ancestor_widget,
              parent_is_local);
  return proxy.release();
}

// static
RenderFrameProxy* RenderFrameProxy::CreateFrameProxy(
    AgentSchedulingGroup& agent_scheduling_group,
    int routing_id,
    int render_view_routing_id,
    const base::Optional<base::UnguessableToken>& opener_frame_token,
    int parent_routing_id,
    const FrameReplicationState& replicated_state,
    const base::UnguessableToken& frame_token,
    const base::UnguessableToken& devtools_frame_token) {
  RenderFrameProxy* parent = nullptr;
  if (parent_routing_id != MSG_ROUTING_NONE) {
    parent = RenderFrameProxy::FromRoutingID(parent_routing_id);
    // It is possible that the parent proxy has been detached in this renderer
    // process, just as the parent's real frame was creating this child frame.
    // In this case, do not create the proxy. See https://crbug.com/568670.
    if (!parent)
      return nullptr;
  }

  std::unique_ptr<RenderFrameProxy> proxy(
      new RenderFrameProxy(agent_scheduling_group, routing_id));
  proxy->devtools_frame_token_ = devtools_frame_token;
  RenderViewImpl* render_view = nullptr;
  RenderWidget* ancestor_widget = nullptr;
  blink::WebRemoteFrame* web_frame = nullptr;

  blink::WebFrame* opener = nullptr;
  if (opener_frame_token)
    opener = blink::WebFrame::FromFrameToken(*opener_frame_token);
  if (!parent) {
    // Create a top level WebRemoteFrame.
    render_view = RenderViewImpl::FromRoutingID(render_view_routing_id);
    blink::WebView* web_view = render_view->GetWebView();
    web_frame = blink::WebRemoteFrame::CreateMainFrame(
        web_view, proxy.get(), proxy->blink_interface_registry_.get(),
        proxy->GetRemoteAssociatedInterfaces(), frame_token, opener);
    // Root frame proxy has no ancestors to point to their RenderWidget.

    // The WebRemoteFrame created here was already attached to the Page as its
    // main frame, so we can call WebView's DidAttachRemoteMainFrame().
    web_view->DidAttachRemoteMainFrame();
  } else {
    // Create a frame under an existing parent. The parent is always expected
    // to be a RenderFrameProxy, because navigations initiated by local frames
    // should not wind up here.
    web_frame = parent->web_frame()->CreateRemoteChild(
        replicated_state.scope,
        blink::WebString::FromUTF8(replicated_state.name),
        replicated_state.frame_policy,
        replicated_state.frame_owner_element_type, proxy.get(),
        proxy->blink_interface_registry_.get(),
        proxy->GetRemoteAssociatedInterfaces(), frame_token, opener);
    render_view = parent->render_view();
    ancestor_widget = parent->ancestor_render_widget_;
  }

  proxy->Init(web_frame, render_view, ancestor_widget, false);

  // Initialize proxy's WebRemoteFrame with the security origin and other
  // replicated information.
  // TODO(dcheng): Calling this when parent_routing_id != MSG_ROUTING_NONE is
  // mostly redundant, since we already pass the name and sandbox flags in
  // createLocalChild(). We should update the Blink interface so it also takes
  // the origin. Then it will be clear that the replication call is only needed
  // for the case of setting up a main frame proxy.
  proxy->SetReplicatedState(replicated_state);

  return proxy.release();
}

RenderFrameProxy* RenderFrameProxy::CreateProxyForPortal(
    AgentSchedulingGroup& agent_scheduling_group,
    RenderFrameImpl* parent,
    int proxy_routing_id,
    const base::UnguessableToken& frame_token,
    const base::UnguessableToken& devtools_frame_token,
    const blink::WebElement& portal_element) {
  auto proxy = base::WrapUnique(
      new RenderFrameProxy(agent_scheduling_group, proxy_routing_id));
  proxy->devtools_frame_token_ = devtools_frame_token;
  blink::WebRemoteFrame* web_frame = blink::WebRemoteFrame::CreateForPortal(
      blink::mojom::TreeScopeType::kDocument, proxy.get(),
      proxy->blink_interface_registry_.get(),
      proxy->GetRemoteAssociatedInterfaces(), frame_token, portal_element);
  proxy->Init(web_frame, parent->render_view(),
              parent->GetLocalRootRenderWidget(), true);
  return proxy.release();
}

// static
RenderFrameProxy* RenderFrameProxy::FromRoutingID(int32_t routing_id) {
  RoutingIDProxyMap* proxies = g_routing_id_proxy_map.Pointer();
  auto it = proxies->find(routing_id);
  return it == proxies->end() ? NULL : it->second;
}

// static
RenderFrameProxy* RenderFrameProxy::FromWebFrame(
    blink::WebRemoteFrame* web_frame) {
  // TODO(dcheng): Turn this into a DCHECK() if it doesn't crash on canary.
  CHECK(web_frame);
  auto iter = g_frame_proxy_map.Get().find(web_frame);
  if (iter != g_frame_proxy_map.Get().end()) {
    RenderFrameProxy* proxy = iter->second;
    DCHECK_EQ(web_frame, proxy->web_frame());
    return proxy;
  }
  // Reaching this is not expected: this implies that the |web_frame| in
  // question is not managed by the content API, or the associated
  // RenderFrameProxy is already deleted--in which case, it's not safe to touch
  // |web_frame|.
  NOTREACHED();
  return nullptr;
}

RenderFrameProxy::RenderFrameProxy(AgentSchedulingGroup& agent_scheduling_group,
                                   int routing_id)
    : agent_scheduling_group_(agent_scheduling_group),
      routing_id_(routing_id),
      provisional_frame_routing_id_(MSG_ROUTING_NONE),
      // TODO(samans): Investigate if it is safe to delay creation of this
      // object until a FrameSinkId is provided.
      parent_local_surface_id_allocator_(
          std::make_unique<viz::ParentLocalSurfaceIdAllocator>()) {
  std::pair<RoutingIDProxyMap::iterator, bool> result =
      g_routing_id_proxy_map.Get().insert(std::make_pair(routing_id_, this));
  CHECK(result.second) << "Inserting a duplicate item.";
  agent_scheduling_group_.AddRoute(routing_id_, this);
  blink_interface_registry_ = std::make_unique<BlinkInterfaceRegistryImpl>(
      binder_registry_.GetWeakPtr(), associated_interfaces_.GetWeakPtr());
}

RenderFrameProxy::~RenderFrameProxy() {
  CHECK(!web_frame_);
  agent_scheduling_group_.RemoveRoute(routing_id_);
  g_routing_id_proxy_map.Get().erase(routing_id_);
}

void RenderFrameProxy::Init(blink::WebRemoteFrame* web_frame,
                            RenderViewImpl* render_view,
                            RenderWidget* ancestor_widget,
                            bool parent_is_local) {
  CHECK(web_frame);
  CHECK(render_view);

  web_frame_ = web_frame;
  render_view_ = render_view;
  ancestor_render_widget_ = ancestor_widget;

  // |ancestor_render_widget_| can be null if this is a proxy for a remote main
  // frame, or a subframe of that proxy. We don't need to register as an
  // observer [since there is no ancestor RenderWidget]. The observer is used to
  // propagate VisualProperty changes down the frame/process hierarchy. Remote
  // main frame proxies do not participate in this flow.
  if (ancestor_render_widget_) {
    blink::WebFrameWidget* ancestor_frame_widget =
        static_cast<blink::WebFrameWidget*>(
            ancestor_render_widget_->GetWebWidget());
    pending_visual_properties_.zoom_level = render_view->GetZoomLevel();
    pending_visual_properties_.page_scale_factor =
        ancestor_frame_widget->PageScaleInMainFrame();
    pending_visual_properties_.is_pinch_gesture_active =
        ancestor_frame_widget->PinchGestureActiveInMainFrame();
    pending_visual_properties_.screen_info =
        ancestor_render_widget_->GetWebWidget()->GetOriginalScreenInfo();
    pending_visual_properties_.visible_viewport_size =
        ancestor_render_widget_->GetWebWidget()->VisibleViewportSizeInDIPs();
    const blink::WebVector<gfx::Rect>& window_segments =
        static_cast<blink::WebFrameWidget*>(
            ancestor_render_widget_->GetWebWidget())
            ->WindowSegments();
    pending_visual_properties_.root_widget_window_segments.assign(
        window_segments.begin(), window_segments.end());
    SynchronizeVisualProperties();
  }

  std::pair<FrameProxyMap::iterator, bool> result =
      g_frame_proxy_map.Get().insert(std::make_pair(web_frame_, this));
  CHECK(result.second) << "Inserted a duplicate item.";

  if (parent_is_local)
    compositing_helper_ = std::make_unique<ChildFrameCompositingHelper>(this);
}

void RenderFrameProxy::ResendVisualProperties() {
  // Reset |sent_visual_properties_| in order to allocate a new
  // viz::LocalSurfaceId.
  sent_visual_properties_ = base::nullopt;
  SynchronizeVisualProperties();
}

void RenderFrameProxy::DidChangeScreenInfo(
    const blink::ScreenInfo& screen_info) {
  DCHECK(ancestor_render_widget_);

  pending_visual_properties_.screen_info = screen_info;
  if (crashed_) {
    // Update the sad page to match the current ScreenInfo.
    compositing_helper_->ChildFrameGone(local_frame_size(),
                                        screen_info.device_scale_factor);
    return;
  }
  SynchronizeVisualProperties();
}

void RenderFrameProxy::ZoomLevelChanged(double zoom_level) {
  DCHECK(ancestor_render_widget_);

  pending_visual_properties_.zoom_level = zoom_level;
  SynchronizeVisualProperties();
}

void RenderFrameProxy::DidChangeRootWindowSegments(
    const std::vector<gfx::Rect>& root_widget_window_segments) {
  pending_visual_properties_.root_widget_window_segments =
      std::move(root_widget_window_segments);
  SynchronizeVisualProperties();
}

void RenderFrameProxy::PageScaleFactorChanged(float page_scale_factor,
                                              bool is_pinch_gesture_active) {
  DCHECK(ancestor_render_widget_);

  pending_visual_properties_.page_scale_factor = page_scale_factor;
  pending_visual_properties_.is_pinch_gesture_active = is_pinch_gesture_active;
  SynchronizeVisualProperties();
}

viz::FrameSinkId RenderFrameProxy::GetFrameSinkId() {
  return frame_sink_id_;
}

void RenderFrameProxy::DidChangeVisibleViewportSize(
    const gfx::Size& visible_viewport_size) {
  DCHECK(ancestor_render_widget_);

  pending_visual_properties_.visible_viewport_size = visible_viewport_size;
  SynchronizeVisualProperties();
}

void RenderFrameProxy::UpdateCaptureSequenceNumber(
    uint32_t capture_sequence_number) {
  DCHECK(ancestor_render_widget_);

  pending_visual_properties_.capture_sequence_number = capture_sequence_number;
  SynchronizeVisualProperties();
}

void RenderFrameProxy::SetReplicatedState(const FrameReplicationState& state) {
  DCHECK(web_frame_);

  web_frame_->SetReplicatedOrigin(
      state.origin, state.has_potentially_trustworthy_unique_origin);

#if DCHECK_IS_ON()
  blink::WebSecurityOrigin security_origin_before_sandbox_flags =
      web_frame_->GetSecurityOrigin();
#endif

  web_frame_->SetReplicatedSandboxFlags(state.active_sandbox_flags);

#if DCHECK_IS_ON()
  // If |state.has_potentially_trustworthy_unique_origin| is set,
  // - |state.origin| should be unique (this is checked in
  //   blink::SecurityOrigin::SetUniqueOriginIsPotentiallyTrustworthy() in
  //   SetReplicatedOrigin()), and thus
  // - The security origin is not updated by SetReplicatedSandboxFlags() and
  //   thus we don't have to apply |has_potentially_trustworthy_unique_origin|
  //   flag after SetReplicatedSandboxFlags().
  if (state.has_potentially_trustworthy_unique_origin)
    DCHECK(security_origin_before_sandbox_flags ==
           web_frame_->GetSecurityOrigin());
#endif

  web_frame_->SetReplicatedName(blink::WebString::FromUTF8(state.name),
                                blink::WebString::FromUTF8(state.unique_name));
  web_frame_->SetReplicatedInsecureRequestPolicy(state.insecure_request_policy);
  web_frame_->SetReplicatedInsecureNavigationsSet(
      state.insecure_navigations_set);
  web_frame_->SetReplicatedAdFrameType(state.ad_frame_type);
  web_frame_->SetReplicatedFeaturePolicyHeaderAndOpenerPolicies(
      state.feature_policy_header, state.opener_feature_state);
  if (state.has_active_user_gesture) {
    // TODO(crbug.com/1087963): This should be hearing about sticky activations
    // and setting those (as well as the active one?). But the call to
    // UpdateUserActivationState sets the transient activation.
    web_frame_->UpdateUserActivationState(
        blink::mojom::UserActivationUpdateType::kNotifyActivation,
        blink::mojom::UserActivationNotificationType::kMedia);
  }
  web_frame_->SetHadStickyUserActivationBeforeNavigation(
      state.has_received_user_gesture_before_nav);

  web_frame_->ResetReplicatedContentSecurityPolicy();
  for (const auto& header : state.accumulated_csp_headers) {
    web_frame_->AddReplicatedContentSecurityPolicyHeader(
        blink::WebString::FromUTF8(header.header_value), header.type,
        header.source);
  }
}

std::string RenderFrameProxy::unique_name() const {
  DCHECK(web_frame_);
  return web_frame_->UniqueName().Utf8();
}

bool RenderFrameProxy::OnMessageReceived(const IPC::Message& msg) {
  // Page IPCs are routed via the main frame (both local and remote) and then
  // forwarded to the RenderView. See comment in
  // RenderFrameHostManager::SendPageMessage() for more information.
  if ((IPC_MESSAGE_CLASS(msg) == PageMsgStart)) {
    if (render_view())
      return render_view()->OnMessageReceived(msg);

    return false;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameProxy, msg)
    IPC_MESSAGE_HANDLER(UnfreezableFrameMsg_DeleteProxy, OnDeleteProxy)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  // Note: If |handled| is true, |this| may have been deleted.
  return handled;
}

void RenderFrameProxy::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (interface_name == blink::mojom::RemoteFrame::Name_) {
    associated_interfaces_.TryBindInterface(interface_name, &handle);
  } else if (interface_name == blink::mojom::RemoteMainFrame::Name_) {
    associated_interfaces_.TryBindInterface(interface_name, &handle);
  } else if (interface_name == content::mojom::RenderFrameProxy::Name_) {
    render_frame_proxy_receiver_.Bind(
        mojo::PendingAssociatedReceiver<mojom::RenderFrameProxy>(
            std::move(handle)));
  }
}

bool RenderFrameProxy::Send(IPC::Message* message) {
  return agent_scheduling_group_.Send(message);
}

void RenderFrameProxy::OnDeleteProxy() {
  DCHECK(web_frame_);
  web_frame_->Detach();
}

void RenderFrameProxy::ChildProcessGone() {
  crashed_ = true;
  compositing_helper_->ChildFrameGone(local_frame_size(),
                                      screen_info().device_scale_factor);
}

void RenderFrameProxy::DidStartLoading() {
  web_frame_->DidStartLoading();
}

void RenderFrameProxy::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  if (!parent_local_surface_id_allocator_->UpdateFromChild(
          metadata.local_surface_id.value_or(viz::LocalSurfaceId()))) {
    return;
  }

  // The viz::LocalSurfaceId has changed so we call SynchronizeVisualProperties
  // here to embed it.
  SynchronizeVisualProperties();
}

void RenderFrameProxy::EnableAutoResize(const gfx::Size& min_size,
                                        const gfx::Size& max_size) {
  DCHECK(ancestor_render_widget_);

  pending_visual_properties_.auto_resize_enabled = true;
  pending_visual_properties_.min_size_for_auto_resize = min_size;
  pending_visual_properties_.max_size_for_auto_resize = max_size;
  SynchronizeVisualProperties();
}

void RenderFrameProxy::DisableAutoResize() {
  DCHECK(ancestor_render_widget_);

  pending_visual_properties_.auto_resize_enabled = false;
  SynchronizeVisualProperties();
}

void RenderFrameProxy::SetFrameSinkId(const viz::FrameSinkId& frame_sink_id) {
  FrameSinkIdChanged(frame_sink_id);
}

void RenderFrameProxy::SynchronizeVisualProperties() {
  DCHECK(ancestor_render_widget_);

  if (!frame_sink_id_.is_valid() || crashed_)
    return;

  // Note that the following flag is true if the capture sequence number
  // actually changed. That is, it is false if we did not have
  // |sent_visual_properties_|, which is different from
  // |synchronized_props_changed| below.
  bool capture_sequence_number_changed =
      sent_visual_properties_ &&
      sent_visual_properties_->capture_sequence_number !=
          pending_visual_properties_.capture_sequence_number;

  if (web_frame_) {
    pending_visual_properties_.compositor_viewport =
        web_frame_->GetCompositingRect();
  }

  bool synchronized_props_changed =
      !sent_visual_properties_ ||
      sent_visual_properties_->auto_resize_enabled !=
          pending_visual_properties_.auto_resize_enabled ||
      sent_visual_properties_->min_size_for_auto_resize !=
          pending_visual_properties_.min_size_for_auto_resize ||
      sent_visual_properties_->max_size_for_auto_resize !=
          pending_visual_properties_.max_size_for_auto_resize ||
      sent_visual_properties_->local_frame_size !=
          pending_visual_properties_.local_frame_size ||
      sent_visual_properties_->screen_space_rect.size() !=
          pending_visual_properties_.screen_space_rect.size() ||
      sent_visual_properties_->screen_info !=
          pending_visual_properties_.screen_info ||
      sent_visual_properties_->zoom_level !=
          pending_visual_properties_.zoom_level ||
      sent_visual_properties_->page_scale_factor !=
          pending_visual_properties_.page_scale_factor ||
      sent_visual_properties_->is_pinch_gesture_active !=
          pending_visual_properties_.is_pinch_gesture_active ||
      sent_visual_properties_->visible_viewport_size !=
          pending_visual_properties_.visible_viewport_size ||
      sent_visual_properties_->compositor_viewport !=
          pending_visual_properties_.compositor_viewport ||
      sent_visual_properties_->root_widget_window_segments !=
          pending_visual_properties_.root_widget_window_segments ||
      capture_sequence_number_changed;

  if (synchronized_props_changed) {
    parent_local_surface_id_allocator_->GenerateId();
    pending_visual_properties_.local_surface_id =
        parent_local_surface_id_allocator_->GetCurrentLocalSurfaceId();
  }

  // If we're synchronizing surfaces, then use an infinite deadline to ensure
  // everything is synchronized.
  cc::DeadlinePolicy deadline = capture_sequence_number_changed
                                    ? cc::DeadlinePolicy::UseInfiniteDeadline()
                                    : cc::DeadlinePolicy::UseDefaultDeadline();
  viz::SurfaceId surface_id(frame_sink_id_, GetLocalSurfaceId());
  compositing_helper_->SetSurfaceId(
      surface_id, pending_visual_properties_.compositor_viewport.size(),
      deadline);

  bool rect_changed = !sent_visual_properties_ ||
                      sent_visual_properties_->screen_space_rect !=
                          pending_visual_properties_.screen_space_rect;
  bool visual_properties_changed = synchronized_props_changed || rect_changed;

  if (!visual_properties_changed)
    return;

  // Let the browser know about the updated view rect.
  Send(new FrameHostMsg_SynchronizeVisualProperties(
      routing_id_, pending_visual_properties_));
  sent_visual_properties_ = pending_visual_properties_;

  TRACE_EVENT_WITH_FLOW2(
      TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
      "RenderFrameProxy::SynchronizeVisualProperties Send Message",
      TRACE_ID_GLOBAL(
          pending_visual_properties_.local_surface_id.submission_trace_id()),
      TRACE_EVENT_FLAG_FLOW_OUT, "message",
      "FrameHostMsg_SynchronizeVisualProperties", "local_surface_id",
      pending_visual_properties_.local_surface_id.ToString());
}

void RenderFrameProxy::FrameDetached(DetachType type) {
  web_frame_->Close();

  // If this proxy was associated with a provisional RenderFrame, and we're not
  // in the process of swapping with it, clean it up as well.
  if (type == DetachType::kRemove &&
      provisional_frame_routing_id_ != MSG_ROUTING_NONE) {
    RenderFrameImpl* provisional_frame =
        RenderFrameImpl::FromRoutingID(provisional_frame_routing_id_);
    // |provisional_frame| should always exist.  If it was deleted via
    // FrameMsg_Delete right before this proxy was removed,
    // RenderFrameImpl::frameDetached would've cleared this proxy's
    // |provisional_frame_routing_id_| and we wouldn't get here.
    CHECK(provisional_frame);
    provisional_frame->GetWebFrame()->Detach();
  }

  // Remove the entry in the WebFrame->RenderFrameProxy map, as the |web_frame_|
  // is no longer valid.
  auto it = g_frame_proxy_map.Get().find(web_frame_);
  CHECK(it != g_frame_proxy_map.Get().end());
  CHECK_EQ(it->second, this);
  g_frame_proxy_map.Get().erase(it);

  web_frame_ = nullptr;

  delete this;
}

void RenderFrameProxy::Navigate(
    const blink::WebURLRequest& request,
    blink::WebLocalFrame* initiator_frame,
    bool should_replace_current_entry,
    bool is_opener_navigation,
    bool initiator_frame_has_download_sandbox_flag,
    bool blocking_downloads_in_sandbox_enabled,
    bool initiator_frame_is_ad,
    blink::CrossVariantMojoRemote<blink::mojom::BlobURLTokenInterfaceBase>
        blob_url_token,
    const base::Optional<blink::WebImpression>& impression) {
  // The request must always have a valid initiator origin.
  DCHECK(!request.RequestorOrigin().IsNull());

  auto params = mojom::OpenURLParams::New();
  params->url = request.Url();
  params->initiator_origin = request.RequestorOrigin();
  params->post_body = GetRequestBodyForWebURLRequest(request);
  DCHECK_EQ(!!params->post_body, request.HttpMethod().Utf8() == "POST");
  params->extra_headers = GetWebURLRequestHeadersAsString(request);
  params->referrer = blink::mojom::Referrer::New(
      blink::WebStringToGURL(request.ReferrerString()),
      request.GetReferrerPolicy());
  params->disposition = WindowOpenDisposition::CURRENT_TAB;
  params->should_replace_current_entry = should_replace_current_entry;
  params->user_gesture = request.HasUserGesture();
  params->triggering_event_info = blink::TriggeringEventInfo::kUnknown;
  params->blob_url_token =
      mojo::PendingRemote<blink::mojom::BlobURLToken>(std::move(blob_url_token))
          .PassPipe();

  RenderFrameImpl* initiator_render_frame =
      RenderFrameImpl::FromWebFrame(initiator_frame);
  params->initiator_routing_id = initiator_render_frame
                                     ? initiator_render_frame->GetRoutingID()
                                     : MSG_ROUTING_NONE;

  if (impression)
    params->impression = ConvertWebImpressionToImpression(*impression);

  // Note: For the AdFrame/Sandbox download policy here it only covers the case
  // where the navigation initiator frame is ad. The download_policy may be
  // further augmented in RenderFrameProxyHost::OnOpenURL if the navigating
  // frame is ad or sandboxed.
  RenderFrameImpl::MaybeSetDownloadFramePolicy(
      is_opener_navigation, request, web_frame_->GetSecurityOrigin(),
      initiator_frame_has_download_sandbox_flag,
      blocking_downloads_in_sandbox_enabled, initiator_frame_is_ad,
      &params->download_policy);

  GetFrameProxyHost()->OpenURL(std::move(params));
}

void RenderFrameProxy::FrameRectsChanged(
    const blink::WebRect& local_frame_rect,
    const blink::WebRect& screen_space_rect) {
  DCHECK(ancestor_render_widget_);

  pending_visual_properties_.screen_space_rect = gfx::Rect(screen_space_rect);
  pending_visual_properties_.local_frame_size =
      gfx::Size(local_frame_rect.width, local_frame_rect.height);
  pending_visual_properties_.screen_info =
      ancestor_render_widget_->GetWebWidget()->GetOriginalScreenInfo();
  if (crashed_) {
    // Update the sad page to match the current size.
    compositing_helper_->ChildFrameGone(local_frame_size(),
                                        screen_info().device_scale_factor);
    return;
  }
  SynchronizeVisualProperties();
}

void RenderFrameProxy::UpdateRemoteViewportIntersection(
    const blink::ViewportIntersectionState& intersection_state) {
  DCHECK(ancestor_render_widget_);
  // TODO(szager): compositor_viewport is propagated twice, via
  // ViewportIntersectionState and also via FrameVisualProperties. It should
  // only go through FrameVisualProperties.
  if (pending_visual_properties_.compositor_viewport !=
      gfx::Rect(intersection_state.compositor_visible_rect)) {
    SynchronizeVisualProperties();
  }
  Send(new FrameHostMsg_UpdateViewportIntersection(routing_id_,
                                                   intersection_state));
}

base::UnguessableToken RenderFrameProxy::GetDevToolsFrameToken() {
  return devtools_frame_token_;
}

cc::Layer* RenderFrameProxy::GetLayer() {
  return embedded_layer_.get();
}

void RenderFrameProxy::SetLayer(scoped_refptr<cc::Layer> layer,
                                bool prevent_contents_opaque_changes,
                                bool is_surface_layer) {
  // |ancestor_render_widget_| can be null if this is a proxy for a remote main
  // frame, or a subframe of that proxy. However, we should not be setting a
  // layer on such a proxy (the layer is used for embedding a child proxy).
  DCHECK(ancestor_render_widget_);

  if (web_frame()) {
    web_frame()->SetCcLayer(layer.get(), prevent_contents_opaque_changes,
                            is_surface_layer);

    // Schedule an animation so that a new frame is produced with the updated
    // layer, otherwise this local root's visible content may not be up to date.
    ancestor_render_widget_->ScheduleAnimation();
  }
  embedded_layer_ = std::move(layer);
}

SkBitmap* RenderFrameProxy::GetSadPageBitmap() {
  return GetContentClient()->renderer()->GetSadWebViewBitmap();
}

const viz::LocalSurfaceId& RenderFrameProxy::GetLocalSurfaceId() const {
  return parent_local_surface_id_allocator_->GetCurrentLocalSurfaceId();
}

mojom::RenderFrameProxyHost* RenderFrameProxy::GetFrameProxyHost() {
  if (!frame_proxy_host_remote_.is_bound())
    GetRemoteAssociatedInterfaces()->GetInterface(&frame_proxy_host_remote_);
  return frame_proxy_host_remote_.get();
}

blink::AssociatedInterfaceProvider*
RenderFrameProxy::GetRemoteAssociatedInterfaces() {
  if (!remote_associated_interfaces_) {
    mojo::PendingAssociatedRemote<blink::mojom::AssociatedInterfaceProvider>
        remote_interfaces;
    agent_scheduling_group_.GetRemoteRouteProvider()->GetRoute(
        routing_id_, remote_interfaces.InitWithNewEndpointAndPassReceiver());
    remote_associated_interfaces_ =
        std::make_unique<blink::AssociatedInterfaceProvider>(
            std::move(remote_interfaces));
  }
  return remote_associated_interfaces_.get();
}

void RenderFrameProxy::WasEvicted() {
  // On eviction, the last SurfaceId is invalidated. We need to allocate a new
  // id.
  ResendVisualProperties();
}

void RenderFrameProxy::FrameSinkIdChanged(
    const viz::FrameSinkId& frame_sink_id) {
  crashed_ = false;
  // The same ParentLocalSurfaceIdAllocator cannot provide LocalSurfaceIds for
  // two different frame sinks, so recreate it here.
  if (frame_sink_id_ != frame_sink_id) {
    parent_local_surface_id_allocator_ =
        std::make_unique<viz::ParentLocalSurfaceIdAllocator>();
  }
  frame_sink_id_ = frame_sink_id;

  // Resend the FrameRects and allocate a new viz::LocalSurfaceId when the view
  // changes.
  ResendVisualProperties();
}

}  // namespace content
