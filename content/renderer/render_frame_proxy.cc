// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_frame_proxy.h"

#include <stdint.h>
#include <map>
#include <utility>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "components/viz/common/features.h"
#include "content/common/content_switches_internal.h"
#include "content/common/frame_message_structs.h"
#include "content/common/frame_owner_properties.h"
#include "content/common/frame_replication_state.h"
#include "content/common/input_messages.h"
#include "content/common/page_messages.h"
#include "content/common/swapped_out_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/screen_info.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/child_frame_compositing_helper.h"
#include "content/renderer/frame_owner_properties.h"
#include "content/renderer/loader/web_url_request_util.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget.h"
#include "content/renderer/resource_timing_info_conversions.h"
#include "ipc/ipc_message_macros.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_resource_timing_info.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_triggering_event_info.h"
#include "third_party/blink/public/web/web_user_gesture_indicator.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/size_conversions.h"

#if defined(USE_AURA)
#include "content/renderer/mus/mus_embedded_frame.h"
#include "content/renderer/mus/renderer_window_tree_client.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
// nogncheck because dependency on //printing is conditional upon
// enable_basic_printing flags.
#include "printing/metafile_skia.h"          // nogncheck
#include "printing/metafile_skia_wrapper.h"  // nogncheck
#endif

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
    RenderFrameImpl* frame_to_replace,
    int routing_id,
    blink::WebTreeScopeType scope) {
  CHECK_NE(routing_id, MSG_ROUTING_NONE);

  std::unique_ptr<RenderFrameProxy> proxy(new RenderFrameProxy(routing_id));
  proxy->unique_name_ = frame_to_replace->unique_name();
  proxy->devtools_frame_token_ = frame_to_replace->GetDevToolsFrameToken();

  // When a RenderFrame is replaced by a RenderProxy, the WebRemoteFrame should
  // always come from WebRemoteFrame::create and a call to WebFrame::swap must
  // follow later.
  blink::WebRemoteFrame* web_frame =
      blink::WebRemoteFrame::Create(scope, proxy.get());

  bool parent_is_local =
      !frame_to_replace->GetWebFrame()->Parent() ||
      frame_to_replace->GetWebFrame()->Parent()->IsWebLocalFrame();

  // If frame_to_replace has a RenderFrameProxy parent, then its
  // RenderWidget will be destroyed along with it, so the new
  // RenderFrameProxy uses its parent's RenderWidget.
  RenderWidget* widget =
      parent_is_local
          ? frame_to_replace->GetRenderWidget()
          : RenderFrameProxy::FromWebFrame(
                frame_to_replace->GetWebFrame()->Parent()->ToWebRemoteFrame())
                ->render_widget();
  proxy->Init(web_frame, frame_to_replace->render_view(), widget,
              parent_is_local);
  return proxy.release();
}

RenderFrameProxy* RenderFrameProxy::CreateFrameProxy(
    int routing_id,
    int render_view_routing_id,
    blink::WebFrame* opener,
    int parent_routing_id,
    const FrameReplicationState& replicated_state,
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

  std::unique_ptr<RenderFrameProxy> proxy(new RenderFrameProxy(routing_id));
  proxy->devtools_frame_token_ = devtools_frame_token;
  RenderViewImpl* render_view = nullptr;
  RenderWidget* render_widget = nullptr;
  blink::WebRemoteFrame* web_frame = nullptr;

  if (!parent) {
    // Create a top level WebRemoteFrame.
    render_view = RenderViewImpl::FromRoutingID(render_view_routing_id);
    web_frame = blink::WebRemoteFrame::CreateMainFrame(
        render_view->GetWebView(), proxy.get(), opener);
    render_widget = render_view->GetWidget();

    // If the RenderView is reused by this proxy after having been used for a
    // pending RenderFrame that was discarded, its swapped out state needs to
    // be updated, as the OnSwapOut flow which normally does this won't happen
    // in that case.  See https://crbug.com/653746 and
    // https://crbug.com/651980.
    if (!render_widget->is_swapped_out())
      render_widget->SetSwappedOut(true);
  } else {
    // Create a frame under an existing parent. The parent is always expected
    // to be a RenderFrameProxy, because navigations initiated by local frames
    // should not wind up here.
    web_frame = parent->web_frame()->CreateRemoteChild(
        replicated_state.scope,
        blink::WebString::FromUTF8(replicated_state.name),
        replicated_state.frame_policy.sandbox_flags,
        replicated_state.frame_policy.container_policy,
        replicated_state.frame_owner_element_type, proxy.get(), opener);
    proxy->unique_name_ = replicated_state.unique_name;
    render_view = parent->render_view();
    render_widget = parent->render_widget();
  }

  proxy->Init(web_frame, render_view, render_widget, false);

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

RenderFrameProxy::RenderFrameProxy(int routing_id)
    : routing_id_(routing_id),
      provisional_frame_routing_id_(MSG_ROUTING_NONE),
      web_frame_(nullptr),
      render_view_(nullptr),
      render_widget_(nullptr) {
  std::pair<RoutingIDProxyMap::iterator, bool> result =
      g_routing_id_proxy_map.Get().insert(std::make_pair(routing_id_, this));
  CHECK(result.second) << "Inserting a duplicate item.";
  RenderThread::Get()->AddRoute(routing_id_, this);
}

RenderFrameProxy::~RenderFrameProxy() {
  render_widget_->UnregisterRenderFrameProxy(this);

  CHECK(!web_frame_);
  RenderThread::Get()->RemoveRoute(routing_id_);
  g_routing_id_proxy_map.Get().erase(routing_id_);
}

void RenderFrameProxy::Init(blink::WebRemoteFrame* web_frame,
                            RenderViewImpl* render_view,
                            RenderWidget* render_widget,
                            bool parent_is_local) {
  CHECK(web_frame);
  CHECK(render_view);
  CHECK(render_widget);

  web_frame_ = web_frame;
  render_view_ = render_view;
  render_widget_ = render_widget;

  render_widget_->RegisterRenderFrameProxy(this);

  std::pair<FrameProxyMap::iterator, bool> result =
      g_frame_proxy_map.Get().insert(std::make_pair(web_frame_, this));
  CHECK(result.second) << "Inserted a duplicate item.";

  enable_surface_synchronization_ = features::IsSurfaceSynchronizationEnabled();

  if (parent_is_local)
    compositing_helper_ = std::make_unique<ChildFrameCompositingHelper>(this);

  pending_visual_properties_.screen_info =
      render_widget_->GetOriginalScreenInfo();

#if defined(USE_AURA)
  if (features::IsMultiProcessMash()) {
    RendererWindowTreeClient* renderer_window_tree_client =
        RendererWindowTreeClient::Get(render_widget_->routing_id());
    // It's possible a MusEmbeddedFrame has already been scheduled for creation
    // (that is, RendererWindowTreeClient::Embed() was called). Call to
    // OnRenderFrameProxyCreated() to potentially get the MusEmbeddedFrame.
    // OnRenderFrameProxyCreated() returns null if Embed() was not called.
    mus_embedded_frame_ =
        renderer_window_tree_client->OnRenderFrameProxyCreated(this);
  }
#endif
}

void RenderFrameProxy::ResendVisualProperties() {
  // Reset |sent_visual_properties_| in order to allocate a new
  // viz::LocalSurfaceId.
  sent_visual_properties_ = base::nullopt;
  SynchronizeVisualProperties();
}

void RenderFrameProxy::WillBeginCompositorFrame() {
  if (compositing_helper_ && compositing_helper_->surface_id().is_valid()) {
    FrameHostMsg_HittestData_Params params;
    params.surface_id = compositing_helper_->surface_id();
    params.ignored_for_hittest = web_frame_->IsIgnoredForHitTest();
    render_widget_->QueueMessage(
        new FrameHostMsg_HittestData(render_widget_->routing_id(), params));
  }
}

void RenderFrameProxy::OnScreenInfoChanged(const ScreenInfo& screen_info) {
  pending_visual_properties_.screen_info = screen_info;
  if (crashed_) {
    // Update the sad page to match the current ScreenInfo.
    compositing_helper_->ChildFrameGone(local_frame_size(),
                                        screen_info.device_scale_factor);
    return;
  }
  SynchronizeVisualProperties();
}

void RenderFrameProxy::OnZoomLevelChanged(double zoom_level) {
  pending_visual_properties_.zoom_level = zoom_level;
  SynchronizeVisualProperties();
}

void RenderFrameProxy::UpdateCaptureSequenceNumber(
    uint32_t capture_sequence_number) {
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

  web_frame_->SetReplicatedName(blink::WebString::FromUTF8(state.name));
  web_frame_->SetReplicatedInsecureRequestPolicy(state.insecure_request_policy);
  web_frame_->SetReplicatedInsecureNavigationsSet(
      state.insecure_navigations_set);
  web_frame_->SetReplicatedFeaturePolicyHeader(state.feature_policy_header);
  if (state.has_received_user_gesture) {
    web_frame_->UpdateUserActivationState(
        blink::UserActivationUpdateType::kNotifyActivation);
  }
  web_frame_->SetHasReceivedUserGestureBeforeNavigation(
      state.has_received_user_gesture_before_nav);

  web_frame_->ResetReplicatedContentSecurityPolicy();
  OnAddContentSecurityPolicies(state.accumulated_csp_headers);
}

// Update the proxy's FrameOwner with new sandbox flags and container policy
// that were set by its parent in another process.
//
// Normally, when a frame's sandbox attribute is changed dynamically, the
// frame's FrameOwner is updated with the new sandbox flags right away, while
// the frame's SecurityContext is updated when the frame is navigated and the
// new sandbox flags take effect.
//
// Currently, there is no use case for a proxy's pending FrameOwner sandbox
// flags, so there's no message sent to proxies when the sandbox attribute is
// first updated.  Instead, the active flags are updated when they take effect,
// by OnDidSetActiveSandboxFlags. The proxy's FrameOwner flags are updated here
// with the caveat that the FrameOwner won't learn about updates to its flags
// until they take effect.
void RenderFrameProxy::OnDidUpdateFramePolicy(
    const blink::FramePolicy& frame_policy) {
  DCHECK(web_frame()->Parent());
  web_frame_->SetFrameOwnerPolicy(frame_policy.sandbox_flags,
                                  frame_policy.container_policy);
}

// Update the proxy's SecurityContext with new sandbox flags or feature policy
// that were set during navigation. Unlike changes to the FrameOwner, which are
// handled by OnDidUpdateFramePolicy, these changes should be considered
// effective immediately.
//
// These flags / policy are needed on the remote frame's SecurityContext to
// ensure that sandbox flags and feature policy are inherited properly if this
// proxy ever parents a local frame.
void RenderFrameProxy::OnDidSetFramePolicyHeaders(
    blink::WebSandboxFlags active_sandbox_flags,
    blink::ParsedFeaturePolicy feature_policy_header) {
  web_frame_->SetReplicatedSandboxFlags(active_sandbox_flags);
  web_frame_->SetReplicatedFeaturePolicyHeader(feature_policy_header);
}

bool RenderFrameProxy::OnMessageReceived(const IPC::Message& msg) {
  // Forward Page IPCs to the RenderView.
  if ((IPC_MESSAGE_CLASS(msg) == PageMsgStart)) {
    if (render_view())
      return render_view()->OnMessageReceived(msg);

    return false;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameProxy, msg)
    IPC_MESSAGE_HANDLER(FrameMsg_DeleteProxy, OnDeleteProxy)
    IPC_MESSAGE_HANDLER(FrameMsg_ChildFrameProcessGone, OnChildFrameProcessGone)
    IPC_MESSAGE_HANDLER(FrameMsg_FirstSurfaceActivation,
                        OnFirstSurfaceActivation)
    IPC_MESSAGE_HANDLER(FrameMsg_IntrinsicSizingInfoOfChildChanged,
                        OnIntrinsicSizingInfoOfChildChanged)
    IPC_MESSAGE_HANDLER(FrameMsg_UpdateOpener, OnUpdateOpener)
    IPC_MESSAGE_HANDLER(FrameMsg_ViewChanged, OnViewChanged)
    IPC_MESSAGE_HANDLER(FrameMsg_DidStartLoading, OnDidStartLoading)
    IPC_MESSAGE_HANDLER(FrameMsg_DidStopLoading, OnDidStopLoading)
    IPC_MESSAGE_HANDLER(FrameMsg_DidUpdateFramePolicy, OnDidUpdateFramePolicy)
    IPC_MESSAGE_HANDLER(FrameMsg_DidSetFramePolicyHeaders,
                        OnDidSetFramePolicyHeaders)
    IPC_MESSAGE_HANDLER(FrameMsg_ForwardResourceTimingToParent,
                        OnForwardResourceTimingToParent)
    IPC_MESSAGE_HANDLER(FrameMsg_DispatchLoad, OnDispatchLoad)
    IPC_MESSAGE_HANDLER(FrameMsg_Collapse, OnCollapse)
    IPC_MESSAGE_HANDLER(FrameMsg_DidUpdateName, OnDidUpdateName)
    IPC_MESSAGE_HANDLER(FrameMsg_AddContentSecurityPolicies,
                        OnAddContentSecurityPolicies)
    IPC_MESSAGE_HANDLER(FrameMsg_ResetContentSecurityPolicy,
                        OnResetContentSecurityPolicy)
    IPC_MESSAGE_HANDLER(FrameMsg_EnforceInsecureRequestPolicy,
                        OnEnforceInsecureRequestPolicy)
    IPC_MESSAGE_HANDLER(FrameMsg_EnforceInsecureNavigationsSet,
                        OnEnforceInsecureNavigationsSet)
    IPC_MESSAGE_HANDLER(FrameMsg_SetFrameOwnerProperties,
                        OnSetFrameOwnerProperties)
    IPC_MESSAGE_HANDLER(FrameMsg_DidUpdateOrigin, OnDidUpdateOrigin)
    IPC_MESSAGE_HANDLER(InputMsg_SetFocus, OnSetPageFocus)
    IPC_MESSAGE_HANDLER(FrameMsg_DidUpdateVisualProperties,
                        OnDidUpdateVisualProperties)
    IPC_MESSAGE_HANDLER(FrameMsg_EnableAutoResize, OnEnableAutoResize)
    IPC_MESSAGE_HANDLER(FrameMsg_DisableAutoResize, OnDisableAutoResize)
    IPC_MESSAGE_HANDLER(FrameMsg_SetFocusedFrame, OnSetFocusedFrame)
    IPC_MESSAGE_HANDLER(FrameMsg_WillEnterFullscreen, OnWillEnterFullscreen)
    IPC_MESSAGE_HANDLER(FrameMsg_UpdateUserActivationState,
                        OnUpdateUserActivationState)
    IPC_MESSAGE_HANDLER(FrameMsg_ScrollRectToVisible, OnScrollRectToVisible)
    IPC_MESSAGE_HANDLER(FrameMsg_BubbleLogicalScroll, OnBubbleLogicalScroll)
    IPC_MESSAGE_HANDLER(FrameMsg_SetHasReceivedUserGestureBeforeNavigation,
                        OnSetHasReceivedUserGestureBeforeNavigation)
    IPC_MESSAGE_HANDLER(FrameMsg_RenderFallbackContent, OnRenderFallbackContent)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  // Note: If |handled| is true, |this| may have been deleted.
  return handled;
}

bool RenderFrameProxy::Send(IPC::Message* message) {
  return RenderThread::Get()->Send(message);
}

void RenderFrameProxy::OnDeleteProxy() {
  DCHECK(web_frame_);
  web_frame_->Detach();
}

void RenderFrameProxy::OnChildFrameProcessGone() {
  crashed_ = true;
  compositing_helper_->ChildFrameGone(local_frame_size(),
                                      screen_info().device_scale_factor);
}

void RenderFrameProxy::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  // If this WebFrame has already been detached, its parent will be null. This
  // can happen when swapping a WebRemoteFrame with a WebLocalFrame, where this
  // message may arrive after the frame was removed from the frame tree, but
  // before the frame has been destroyed. http://crbug.com/446575.
  if (!web_frame()->Parent())
    return;

  if (!enable_surface_synchronization_) {
    compositing_helper_->SetSurfaceId(surface_info.id(), local_frame_size(),
                                      cc::DeadlinePolicy::UseDefaultDeadline());
  }
  compositing_helper_->SetOldestAcceptableFallback(surface_info.id(),
                                                   local_frame_size());
}

void RenderFrameProxy::OnIntrinsicSizingInfoOfChildChanged(
    blink::WebIntrinsicSizingInfo sizing_info) {
  web_frame()->IntrinsicSizingInfoChanged(sizing_info);
}

void RenderFrameProxy::OnUpdateOpener(int opener_routing_id) {
  blink::WebFrame* opener = RenderFrameImpl::ResolveOpener(opener_routing_id);
  web_frame_->SetOpener(opener);
}

void RenderFrameProxy::OnDidStartLoading() {
  web_frame_->DidStartLoading();
}

void RenderFrameProxy::OnViewChanged(
    const FrameMsg_ViewChanged_Params& params) {
  crashed_ = false;
  // In mash the FrameSinkId comes from RendererWindowTreeClient.
  if (!features::IsMultiProcessMash())
    frame_sink_id_ = *params.frame_sink_id;

  // Resend the FrameRects and allocate a new viz::LocalSurfaceId when the view
  // changes.
  ResendVisualProperties();
}

void RenderFrameProxy::OnDidStopLoading() {
  web_frame_->DidStopLoading();
}

void RenderFrameProxy::OnForwardResourceTimingToParent(
    const ResourceTimingInfo& info) {
  web_frame_->ForwardResourceTimingToParent(
      ResourceTimingInfoToWebResourceTimingInfo(info));
}

void RenderFrameProxy::OnDispatchLoad() {
  web_frame_->DispatchLoadEventForFrameOwner();
}

void RenderFrameProxy::OnCollapse(bool collapsed) {
  web_frame_->Collapse(collapsed);
}

void RenderFrameProxy::OnDidUpdateName(const std::string& name,
                                       const std::string& unique_name) {
  web_frame_->SetReplicatedName(blink::WebString::FromUTF8(name));
  unique_name_ = unique_name;
}

void RenderFrameProxy::OnAddContentSecurityPolicies(
    const std::vector<ContentSecurityPolicyHeader>& headers) {
  for (const auto& header : headers) {
    web_frame_->AddReplicatedContentSecurityPolicyHeader(
        blink::WebString::FromUTF8(header.header_value), header.type,
        header.source);
  }
}

void RenderFrameProxy::OnResetContentSecurityPolicy() {
  web_frame_->ResetReplicatedContentSecurityPolicy();
}

void RenderFrameProxy::OnEnforceInsecureRequestPolicy(
    blink::WebInsecureRequestPolicy policy) {
  web_frame_->SetReplicatedInsecureRequestPolicy(policy);
}

void RenderFrameProxy::OnEnforceInsecureNavigationsSet(
    const std::vector<uint32_t>& set) {
  web_frame_->SetReplicatedInsecureNavigationsSet(set);
}

void RenderFrameProxy::OnSetFrameOwnerProperties(
    const FrameOwnerProperties& properties) {
  web_frame_->SetFrameOwnerProperties(
      ConvertFrameOwnerPropertiesToWebFrameOwnerProperties(properties));
}

void RenderFrameProxy::OnDidUpdateOrigin(
    const url::Origin& origin,
    bool is_potentially_trustworthy_unique_origin) {
  web_frame_->SetReplicatedOrigin(origin,
                                  is_potentially_trustworthy_unique_origin);
}

void RenderFrameProxy::OnSetPageFocus(bool is_focused) {
  render_view_->SetFocus(is_focused);
}

void RenderFrameProxy::OnSetFocusedFrame() {
  // This uses focusDocumentView rather than setFocusedFrame so that blur
  // events are properly dispatched on any currently focused elements.
  render_view_->webview()->FocusDocumentView(web_frame_);
}

void RenderFrameProxy::OnWillEnterFullscreen() {
  web_frame_->WillEnterFullscreen();
}

void RenderFrameProxy::OnUpdateUserActivationState(
    blink::UserActivationUpdateType update_type) {
  web_frame_->UpdateUserActivationState(update_type);
}

void RenderFrameProxy::OnScrollRectToVisible(
    const gfx::Rect& rect_to_scroll,
    const blink::WebScrollIntoViewParams& params) {
  web_frame_->ScrollRectToVisible(rect_to_scroll, params);
}

void RenderFrameProxy::OnBubbleLogicalScroll(
    blink::WebScrollDirection direction,
    blink::WebScrollGranularity granularity) {
  web_frame_->BubbleLogicalScroll(direction, granularity);
}

void RenderFrameProxy::OnDidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  if (!parent_local_surface_id_allocator_.UpdateFromChild(
          metadata.local_surface_id.value_or(viz::LocalSurfaceId()),
          metadata.local_surface_id_allocation_time_from_child.value_or(
              base::TimeTicks()))) {
    return;
  }

  // The viz::LocalSurfaceId has changed so we call SynchronizeVisualProperties
  // here to embed it.
  SynchronizeVisualProperties();
}

void RenderFrameProxy::OnEnableAutoResize(const gfx::Size& min_size,
                                          const gfx::Size& max_size) {
  pending_visual_properties_.auto_resize_enabled = true;
  pending_visual_properties_.min_size_for_auto_resize = min_size;
  pending_visual_properties_.max_size_for_auto_resize = max_size;
  SynchronizeVisualProperties();
}

void RenderFrameProxy::OnDisableAutoResize() {
  pending_visual_properties_.auto_resize_enabled = false;
  SynchronizeVisualProperties();
}

#if defined(USE_AURA)
void RenderFrameProxy::SetMusEmbeddedFrame(
    std::unique_ptr<MusEmbeddedFrame> mus_embedded_frame) {
  mus_embedded_frame_ = std::move(mus_embedded_frame);
}
#endif

void RenderFrameProxy::SynchronizeVisualProperties() {
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
      capture_sequence_number_changed;

  if (synchronized_props_changed) {
    parent_local_surface_id_allocator_.GenerateId();
    pending_visual_properties_.local_surface_id_allocation_time =
        parent_local_surface_id_allocator_.allocation_time();
  }

  viz::SurfaceId surface_id(frame_sink_id_, GetLocalSurfaceId());
  if (enable_surface_synchronization_) {
    // If we're synchronizing surfaces, then use an infinite deadline to ensure
    // everything is synchronized.
    cc::DeadlinePolicy deadline =
        capture_sequence_number_changed
            ? cc::DeadlinePolicy::UseInfiniteDeadline()
            : cc::DeadlinePolicy::UseDefaultDeadline();
    compositing_helper_->SetSurfaceId(surface_id, local_frame_size(), deadline);
  }

  bool rect_changed = !sent_visual_properties_ ||
                      sent_visual_properties_->screen_space_rect !=
                          pending_visual_properties_.screen_space_rect;
  bool visual_properties_changed = synchronized_props_changed || rect_changed;

#if defined(USE_AURA)
  if (rect_changed && mus_embedded_frame_) {
    mus_embedded_frame_->SetWindowBounds(GetLocalSurfaceId(),
                                         gfx::Rect(local_frame_size()));
  }
#endif

  if (!visual_properties_changed)
    return;

  // Let the browser know about the updated view rect.
  Send(new FrameHostMsg_SynchronizeVisualProperties(
      routing_id_, surface_id, pending_visual_properties_));
  sent_visual_properties_ = pending_visual_properties_;

  // The visible rect that the OOPIF needs to raster depends partially on
  // parameters that might have changed. If they affect the raster area, resend
  // the intersection rects.
  gfx::Rect new_compositor_visible_rect = web_frame_->GetCompositingRect();
  if (new_compositor_visible_rect != last_compositor_visible_rect_)
    UpdateRemoteViewportIntersection(last_intersection_rect_,
                                     last_occluded_or_obscured_);
}

void RenderFrameProxy::OnSetHasReceivedUserGestureBeforeNavigation(bool value) {
  web_frame_->SetHasReceivedUserGestureBeforeNavigation(value);
}

void RenderFrameProxy::OnRenderFallbackContent() const {
  web_frame_->RenderFallbackContent();
}

void RenderFrameProxy::FrameDetached(DetachType type) {
#if defined(USE_AURA)
  mus_embedded_frame_.reset();
#endif

  if (type == DetachType::kRemove && web_frame_->Parent()) {
    // Let the browser process know this subframe is removed, so that it is
    // destroyed in its current process.
    Send(new FrameHostMsg_Detach(routing_id_));
  }

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

void RenderFrameProxy::CheckCompleted() {
  Send(new FrameHostMsg_CheckCompleted(routing_id_));
}

void RenderFrameProxy::ForwardPostMessage(
    blink::WebLocalFrame* source_frame,
    blink::WebRemoteFrame* target_frame,
    blink::WebSecurityOrigin target_origin,
    blink::WebDOMMessageEvent event,
    bool has_user_gesture) {
  DCHECK(!web_frame_ || web_frame_ == target_frame);

  FrameMsg_PostMessage_Params params;
  params.message =
      new base::RefCountedData<blink::TransferableMessage>(event.AsMessage());
  params.message->data.has_user_gesture = has_user_gesture;
  params.source_origin = event.Origin().Utf16();
  if (!target_origin.IsNull())
    params.target_origin = target_origin.ToString().Utf16();

  // Include the routing ID for the source frame (if one exists), which the
  // browser process will translate into the routing ID for the equivalent
  // frame in the target process.
  params.source_routing_id = MSG_ROUTING_NONE;
  if (source_frame) {
    RenderFrameImpl* source_render_frame =
        RenderFrameImpl::FromWebFrame(source_frame);
    if (source_render_frame)
      params.source_routing_id = source_render_frame->GetRoutingID();
  }

  Send(new FrameHostMsg_RouteMessageEvent(routing_id_, params));
}

void RenderFrameProxy::Navigate(const blink::WebURLRequest& request,
                                bool should_replace_current_entry,
                                mojo::ScopedMessagePipeHandle blob_url_token) {
  FrameHostMsg_OpenURL_Params params;
  params.url = request.Url();
  params.uses_post = request.HttpMethod().Utf8() == "POST";
  params.resource_request_body = GetRequestBodyForWebURLRequest(request);
  params.extra_headers = GetWebURLRequestHeadersAsString(request);
  // TODO(domfarolino): Retrieve the referrer in the form of a referrer member
  // instead of the header field. See https://crbug.com/850813.
  params.referrer = Referrer(blink::WebStringToGURL(request.HttpHeaderField(
                                 blink::WebString::FromUTF8("Referer"))),
                             request.GetReferrerPolicy());
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  params.should_replace_current_entry = should_replace_current_entry;
  params.user_gesture = request.HasUserGesture();
  params.triggering_event_info = blink::WebTriggeringEventInfo::kUnknown;
  params.blob_url_token = blob_url_token.release();

  Send(new FrameHostMsg_OpenURL(routing_id_, params));
}

void RenderFrameProxy::FrameRectsChanged(
    const blink::WebRect& local_frame_rect,
    const blink::WebRect& screen_space_rect) {
  pending_visual_properties_.screen_space_rect = gfx::Rect(screen_space_rect);
  pending_visual_properties_.local_frame_size =
      gfx::Size(local_frame_rect.width, local_frame_rect.height);
  pending_visual_properties_.screen_info =
      render_widget_->GetOriginalScreenInfo();
  if (crashed_) {
    // Update the sad page to match the current size.
    compositing_helper_->ChildFrameGone(local_frame_size(),
                                        screen_info().device_scale_factor);
    return;
  }
  SynchronizeVisualProperties();
}

void RenderFrameProxy::UpdateRemoteViewportIntersection(
    const blink::WebRect& viewport_intersection,
    bool occluded_or_obscured) {
  last_intersection_rect_ = viewport_intersection;
  last_compositor_visible_rect_ = web_frame_->GetCompositingRect();
  last_occluded_or_obscured_ = occluded_or_obscured;
  Send(new FrameHostMsg_UpdateViewportIntersection(
      routing_id_, gfx::Rect(viewport_intersection),
      last_compositor_visible_rect_, last_occluded_or_obscured_));
}

void RenderFrameProxy::VisibilityChanged(bool visible) {
  Send(new FrameHostMsg_VisibilityChanged(routing_id_, visible));
}

void RenderFrameProxy::SetIsInert(bool inert) {
  Send(new FrameHostMsg_SetIsInert(routing_id_, inert));
}

void RenderFrameProxy::SetInheritedEffectiveTouchAction(
    cc::TouchAction touch_action) {
  Send(new FrameHostMsg_SetInheritedEffectiveTouchAction(routing_id_,
                                                         touch_action));
}

void RenderFrameProxy::UpdateRenderThrottlingStatus(bool is_throttled,
                                                    bool subtree_throttled) {
  Send(new FrameHostMsg_UpdateRenderThrottlingStatus(routing_id_, is_throttled,
                                                     subtree_throttled));
}

void RenderFrameProxy::DidChangeOpener(blink::WebFrame* opener) {
  // A proxy shouldn't normally be disowning its opener.  It is possible to get
  // here when a proxy that is being detached clears its opener, in which case
  // there is no need to notify the browser process.
  if (!opener)
    return;

  // Only a LocalFrame (i.e., the caller of window.open) should be able to
  // update another frame's opener.
  DCHECK(opener->IsWebLocalFrame());

  int opener_routing_id =
      RenderFrameImpl::FromWebFrame(opener->ToWebLocalFrame())->GetRoutingID();
  Send(new FrameHostMsg_DidChangeOpener(routing_id_, opener_routing_id));
}

void RenderFrameProxy::AdvanceFocus(blink::WebFocusType type,
                                    blink::WebLocalFrame* source) {
  int source_routing_id = RenderFrameImpl::FromWebFrame(source)->GetRoutingID();
  Send(new FrameHostMsg_AdvanceFocus(routing_id_, type, source_routing_id));
}

void RenderFrameProxy::FrameFocused() {
  Send(new FrameHostMsg_FrameFocused(routing_id_));
}

base::UnguessableToken RenderFrameProxy::GetDevToolsFrameToken() {
  return devtools_frame_token_;
}

#if defined(USE_AURA)
void RenderFrameProxy::OnMusEmbeddedFrameSinkIdAllocated(
    const viz::FrameSinkId& frame_sink_id) {
  // RendererWindowTreeClient should only call this when mus is hosting viz.
  DCHECK(features::IsMultiProcessMash());
  frame_sink_id_ = frame_sink_id;
  // Resend the FrameRects and allocate a new viz::LocalSurfaceId when the view
  // changes.
  ResendVisualProperties();
}
#endif

cc::Layer* RenderFrameProxy::GetLayer() {
  return embedded_layer_.get();
}

void RenderFrameProxy::SetLayer(scoped_refptr<cc::Layer> layer,
                                bool prevent_contents_opaque_changes,
                                bool is_surface_layer) {
  if (web_frame()) {
    web_frame()->SetCcLayer(layer.get(), prevent_contents_opaque_changes,
                            is_surface_layer);
  }
  embedded_layer_ = std::move(layer);
}

SkBitmap* RenderFrameProxy::GetSadPageBitmap() {
  return GetContentClient()->renderer()->GetSadWebViewBitmap();
}

uint32_t RenderFrameProxy::Print(const blink::WebRect& rect,
                                 cc::PaintCanvas* canvas) {
#if BUILDFLAG(ENABLE_PRINTING)
  printing::MetafileSkia* metafile =
      printing::MetafileSkiaWrapper::GetMetafileFromCanvas(canvas);
  DCHECK(metafile);

  // Create a place holder content for the remote frame so it can be replaced
  // with actual content later.
  uint32_t content_id =
      metafile->CreateContentForRemoteFrame(rect, routing_id_);

  // Inform browser to print the remote subframe.
  Send(new FrameHostMsg_PrintCrossProcessSubframe(
      routing_id_, rect, metafile->GetDocumentCookie()));
  return content_id;
#else
  return 0;
#endif
}

const viz::LocalSurfaceId& RenderFrameProxy::GetLocalSurfaceId() const {
  return parent_local_surface_id_allocator_.GetCurrentLocalSurfaceId();
}

}  // namespace content
