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
#include "content/common/content_switches_internal.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/agent_scheduling_group.h"
#include "content/renderer/mojo/blink_interface_registry_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_view_impl.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/common/navigation/navigation_policy.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom.h"
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
    blink::mojom::TreeScopeType tree_scope_type,
    const blink::RemoteFrameToken& proxy_frame_token) {
  CHECK_NE(routing_id, MSG_ROUTING_NONE);

  std::unique_ptr<RenderFrameProxy> proxy(
      new RenderFrameProxy(agent_scheduling_group, routing_id));

  // When a RenderFrame is replaced by a RenderProxy, the WebRemoteFrame should
  // always come from WebRemoteFrame::create and a call to WebFrame::swap must
  // follow later.
  blink::WebRemoteFrame* web_frame = blink::WebRemoteFrame::Create(
      tree_scope_type, proxy.get(), proxy->blink_interface_registry_.get(),
      proxy->GetRemoteAssociatedInterfaces(), proxy_frame_token);

  proxy->Init(web_frame, frame_to_replace->render_view());
  return proxy.release();
}

// static
RenderFrameProxy* RenderFrameProxy::CreateFrameProxy(
    AgentSchedulingGroup& agent_scheduling_group,
    const blink::RemoteFrameToken& frame_token,
    int routing_id,
    const absl::optional<blink::FrameToken>& opener_frame_token,
    int render_view_routing_id,
    int parent_routing_id,
    blink::mojom::TreeScopeType tree_scope_type,
    blink::mojom::FrameReplicationStatePtr replicated_state,
    const base::UnguessableToken& devtools_frame_token,
    mojom::RemoteMainFrameInterfacesPtr remote_main_frame_interfaces) {
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
  RenderViewImpl* render_view = nullptr;
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
        proxy->GetRemoteAssociatedInterfaces(), frame_token,
        devtools_frame_token, opener);
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
    web_frame = parent->web_frame()->CreateRemoteChild(
        tree_scope_type, blink::WebString::FromUTF8(replicated_state->name),
        replicated_state->frame_policy, proxy.get(),
        proxy->blink_interface_registry_.get(),
        proxy->GetRemoteAssociatedInterfaces(), frame_token,
        devtools_frame_token, opener);
    render_view = parent->render_view();
  }

  proxy->Init(web_frame, render_view);

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

RenderFrameProxy* RenderFrameProxy::CreateProxyForPortalOrFencedFrame(
    AgentSchedulingGroup& agent_scheduling_group,
    RenderFrameImpl* parent,
    int proxy_routing_id,
    const blink::RemoteFrameToken& frame_token,
    const base::UnguessableToken& devtools_frame_token,
    const blink::WebElement& frame_owner) {
  auto proxy = base::WrapUnique(
      new RenderFrameProxy(agent_scheduling_group, proxy_routing_id));
  blink::WebRemoteFrame* web_frame =
      blink::WebRemoteFrame::CreateForPortalOrFencedFrame(
          blink::mojom::TreeScopeType::kDocument, proxy.get(),
          proxy->blink_interface_registry_.get(),
          proxy->GetRemoteAssociatedInterfaces(), frame_token,
          devtools_frame_token, frame_owner);
  proxy->Init(web_frame, parent->render_view());
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
                            RenderViewImpl* render_view) {
  CHECK(web_frame);
  CHECK(render_view);

  web_frame_ = web_frame;
  render_view_ = render_view;

  std::pair<FrameProxyMap::iterator, bool> result =
      g_frame_proxy_map.Get().insert(std::make_pair(web_frame_, this));
  CHECK(result.second) << "Inserted a duplicate item.";
}

void RenderFrameProxy::SetReplicatedState(
    blink::mojom::FrameReplicationStatePtr state) {
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
  web_frame_->SetReplicatedIsAdSubframe(state->is_ad_subframe);
  web_frame_->SetReplicatedPermissionsPolicyHeader(
      state->permissions_policy_header);
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
  if (interface_name == blink::mojom::RemoteFrame::Name_)
    associated_interfaces_.TryBindInterface(interface_name, &handle);
}

bool RenderFrameProxy::Send(IPC::Message* message) {
  return agent_scheduling_group_.Send(message);
}

void RenderFrameProxy::DidStartLoading() {
  web_frame_->DidStartLoading();
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
