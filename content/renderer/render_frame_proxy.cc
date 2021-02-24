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
#include "base/stl_util.h"
#include "content/common/content_switches_internal.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/agent_scheduling_group.h"
#include "content/renderer/child_frame_compositing_helper.h"
#include "content/renderer/mojo/blink_interface_registry_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom.h"
#include "third_party/blink/public/platform/impression_conversions.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_request_util.h"
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
    const blink::RemoteFrameToken& proxy_frame_token) {
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

  blink::WebFrameWidget* ancestor_widget = nullptr;
  bool parent_is_local = false;

  // A top level frame proxy doesn't have a RenderWidget pointer. The pointer
  // is to an ancestor local frame's RenderWidget and there are no ancestors.
  if (frame_to_replace->GetWebFrame()->Parent()) {
    if (frame_to_replace->GetWebFrame()->Parent()->IsWebLocalFrame()) {
      // If the frame was a local frame, get its local root's RenderWidget.
      ancestor_widget = frame_to_replace->GetLocalRootWebFrameWidget();
      parent_is_local = true;
    } else {
      // Otherwise, grab the pointer from the parent RenderFrameProxy, as
      // it would already have the correct pointer. A proxy with a proxy child
      // must be created before its child, so the first proxy in a descendant
      // chain is either the root or has a local parent frame.
      RenderFrameProxy* parent = RenderFrameProxy::FromWebFrame(
          frame_to_replace->GetWebFrame()->Parent()->ToWebRemoteFrame());
      ancestor_widget = parent->ancestor_web_frame_widget_;
    }
  }

  proxy->Init(web_frame, frame_to_replace->render_view(), ancestor_widget,
              parent_is_local);
  return proxy.release();
}

// static
RenderFrameProxy* RenderFrameProxy::CreateFrameProxy(
    AgentSchedulingGroup& agent_scheduling_group,
    const blink::RemoteFrameToken& frame_token,
    int routing_id,
    const base::Optional<blink::FrameToken>& opener_frame_token,
    int render_view_routing_id,
    int parent_routing_id,
    mojom::FrameReplicationStatePtr replicated_state,
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
  blink::WebFrameWidget* ancestor_widget = nullptr;
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
        replicated_state->scope,
        blink::WebString::FromUTF8(replicated_state->name),
        replicated_state->frame_policy,
        replicated_state->frame_owner_element_type, proxy.get(),
        proxy->blink_interface_registry_.get(),
        proxy->GetRemoteAssociatedInterfaces(), frame_token, opener);
    render_view = parent->render_view();
    ancestor_widget = parent->ancestor_web_frame_widget_;
  }

  proxy->Init(web_frame, render_view, ancestor_widget, false);

  // Initialize proxy's WebRemoteFrame with the security origin and other
  // replicated information.
  // TODO(dcheng): Calling this when parent_routing_id != MSG_ROUTING_NONE is
  // mostly redundant, since we already pass the name and sandbox flags in
  // createLocalChild(). We should update the Blink interface so it also takes
  // the origin. Then it will be clear that the replication call is only needed
  // for the case of setting up a main frame proxy.
  proxy->SetReplicatedState(std::move(replicated_state));

  return proxy.release();
}

RenderFrameProxy* RenderFrameProxy::CreateProxyForPortal(
    AgentSchedulingGroup& agent_scheduling_group,
    RenderFrameImpl* parent,
    int proxy_routing_id,
    const blink::RemoteFrameToken& frame_token,
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
              parent->GetLocalRootWebFrameWidget(), true);
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
      routing_id_(routing_id) {
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
                            blink::WebFrameWidget* ancestor_widget,
                            bool parent_is_local) {
  CHECK(web_frame);
  CHECK(render_view);

  web_frame_ = web_frame;
  render_view_ = render_view;
  ancestor_web_frame_widget_ = ancestor_widget;

  std::pair<FrameProxyMap::iterator, bool> result =
      g_frame_proxy_map.Get().insert(std::make_pair(web_frame_, this));
  CHECK(result.second) << "Inserted a duplicate item.";

  if (parent_is_local)
    compositing_helper_ = std::make_unique<ChildFrameCompositingHelper>(this);
}

void RenderFrameProxy::SetReplicatedState(
    mojom::FrameReplicationStatePtr state) {
  DCHECK(web_frame_);

  web_frame_->SetReplicatedOrigin(
      state->origin, state->has_potentially_trustworthy_unique_origin);

#if DCHECK_IS_ON()
  blink::WebSecurityOrigin security_origin_before_sandbox_flags =
      web_frame_->GetSecurityOrigin();
#endif

  web_frame_->SetReplicatedSandboxFlags(state->active_sandbox_flags);

#if DCHECK_IS_ON()
  // If |state->has_potentially_trustworthy_unique_origin| is set,
  // - |state->origin| should be unique (this is checked in
  //   blink::SecurityOrigin::SetUniqueOriginIsPotentiallyTrustworthy() in
  //   SetReplicatedOrigin()), and thus
  // - The security origin is not updated by SetReplicatedSandboxFlags() and
  //   thus we don't have to apply |has_potentially_trustworthy_unique_origin|
  //   flag after SetReplicatedSandboxFlags().
  if (state->has_potentially_trustworthy_unique_origin)
    DCHECK(security_origin_before_sandbox_flags ==
           web_frame_->GetSecurityOrigin());
#endif

  web_frame_->SetReplicatedName(blink::WebString::FromUTF8(state->name),
                                blink::WebString::FromUTF8(state->unique_name));
  web_frame_->SetReplicatedInsecureRequestPolicy(
      state->insecure_request_policy);
  web_frame_->SetReplicatedInsecureNavigationsSet(
      state->insecure_navigations_set);
  web_frame_->SetReplicatedAdFrameType(state->ad_frame_type);
  web_frame_->SetReplicatedFeaturePolicyHeader(state->feature_policy_header);
  if (state->has_active_user_gesture) {
    // TODO(crbug.com/1087963): This should be hearing about sticky activations
    // and setting those (as well as the active one?). But the call to
    // UpdateUserActivationState sets the transient activation.
    web_frame_->UpdateUserActivationState(
        blink::mojom::UserActivationUpdateType::kNotifyActivation,
        blink::mojom::UserActivationNotificationType::kMedia);
  }
  web_frame_->SetHadStickyUserActivationBeforeNavigation(
      state->has_received_user_gesture_before_nav);

  web_frame_->ResetReplicatedContentSecurityPolicy();
  web_frame_->AddReplicatedContentSecurityPolicies(
      ToWebContentSecurityPolicies(std::move(state->accumulated_csps)));
}

std::string RenderFrameProxy::unique_name() const {
  DCHECK(web_frame_);
  return web_frame_->UniqueName().Utf8();
}

bool RenderFrameProxy::OnMessageReceived(const IPC::Message& msg) {
  return false;
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

void RenderFrameProxy::ChildProcessGone() {
  remote_process_gone_ = true;
  compositing_helper_->ChildFrameGone(
      ancestor_web_frame_widget_->GetOriginalScreenInfo().device_scale_factor);
}

void RenderFrameProxy::DidStartLoading() {
  web_frame_->DidStartLoading();
}

void RenderFrameProxy::WillSynchronizeVisualProperties(
    bool capture_sequence_number_changed,
    const viz::SurfaceId& surface_id,
    const gfx::Size& compositor_viewport_size) {
  DCHECK(ancestor_web_frame_widget_);
  DCHECK(surface_id.is_valid());
  DCHECK(!remote_process_gone_);

  // If we're synchronizing surfaces, then use an infinite deadline to ensure
  // everything is synchronized.
  cc::DeadlinePolicy deadline = capture_sequence_number_changed
                                    ? cc::DeadlinePolicy::UseInfiniteDeadline()
                                    : cc::DeadlinePolicy::UseDefaultDeadline();
  compositing_helper_->SetSurfaceId(surface_id, compositor_viewport_size,
                                    deadline);
}

void RenderFrameProxy::FrameDetached(DetachType type) {
  web_frame_->Close();

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
    bool should_replace_current_entry,
    bool is_opener_navigation,
    bool initiator_frame_has_download_sandbox_flag,
    bool blocking_downloads_in_sandbox_enabled,
    bool initiator_frame_is_ad,
    blink::CrossVariantMojoRemote<blink::mojom::BlobURLTokenInterfaceBase>
        blob_url_token,
    const base::Optional<blink::WebImpression>& impression,
    const blink::LocalFrameToken* initiator_frame_token,
    blink::CrossVariantMojoRemote<
        blink::mojom::PolicyContainerHostKeepAliveHandleInterfaceBase>
        initiator_policy_container_keep_alive_handle) {
  // The request must always have a valid initiator origin.
  DCHECK(!request.RequestorOrigin().IsNull());

  auto params = mojom::OpenURLParams::New();
  params->url = request.Url();
  params->initiator_origin = request.RequestorOrigin();
  params->post_body = blink::GetRequestBodyForWebURLRequest(request);
  DCHECK_EQ(!!params->post_body, request.HttpMethod().Utf8() == "POST");
  params->extra_headers =
      blink::GetWebURLRequestHeadersAsString(request).Latin1();
  params->referrer = blink::mojom::Referrer::New(
      blink::WebStringToGURL(request.ReferrerString()),
      request.GetReferrerPolicy());
  params->disposition = WindowOpenDisposition::CURRENT_TAB;
  params->should_replace_current_entry = should_replace_current_entry;
  params->user_gesture = request.HasUserGesture();
  params->triggering_event_info = blink::mojom::TriggeringEventInfo::kUnknown;
  params->blob_url_token = std::move(blob_url_token);
  params->initiator_policy_container_keep_alive_handle =
      std::move(initiator_policy_container_keep_alive_handle);
  params->initiator_frame_token = base::OptionalFromPtr(initiator_frame_token);

  if (impression)
    params->impression = blink::ConvertWebImpressionToImpression(*impression);

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

base::UnguessableToken RenderFrameProxy::GetDevToolsFrameToken() {
  return devtools_frame_token_;
}

cc::Layer* RenderFrameProxy::GetLayer() {
  return embedded_layer_.get();
}

void RenderFrameProxy::SetLayer(scoped_refptr<cc::Layer> layer,
                                bool is_surface_layer) {
  // |ancestor_web_frame_widget_| can be null if this is a proxy for a remote
  // main frame, or a subframe of that proxy. However, we should not be setting
  // a layer on such a proxy (the layer is used for embedding a child proxy).
  DCHECK(ancestor_web_frame_widget_);

  if (web_frame())
    web_frame()->SetCcLayer(layer.get(), is_surface_layer);
  embedded_layer_ = std::move(layer);
}

SkBitmap* RenderFrameProxy::GetSadPageBitmap() {
  return GetContentClient()->renderer()->GetSadWebViewBitmap();
}

bool RenderFrameProxy::RemoteProcessGone() const {
  return remote_process_gone_;
}

void RenderFrameProxy::DidSetFrameSinkId() {
  remote_process_gone_ = false;
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
            std::move(remote_interfaces),
            agent_scheduling_group_.agent_group_scheduler()
                .DefaultTaskRunner());
  }
  return remote_associated_interfaces_.get();
}

}  // namespace content
