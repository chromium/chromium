// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_frame_proxy_host.h"

#include <memory>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/hash/hash.h"
#include "base/lazy_instance.h"
#include "base/no_destructor.h"
#include "base/trace_event/typed_macros.h"
#include "base/types/optional_util.h"
#include "content/browser/bad_message.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/renderer_host/batched_proxy_ipc_sender.h"
#include "content/browser/renderer_host/cross_process_frame_connector.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/ipc_utils.h"
#include "content/browser/renderer_host/navigation_metrics_utils.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/site_instance_group.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/referrer_type_converters.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/messaging/transferable_message.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_initiator_activation_and_ad_status.mojom.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom.h"
#include "ui/gfx/geometry/rect_f.h"

namespace content {

namespace {

RenderFrameProxyHost::TestObserver* g_observer_for_testing = nullptr;

// The (process id, routing id) pair that identifies one RenderFrameProxy.
typedef std::pair<int32_t, int32_t> RenderFrameProxyHostID;
typedef std::unordered_map<RenderFrameProxyHostID,
                           RenderFrameProxyHost*,
                           base::IntPairHash<RenderFrameProxyHostID>>
    RoutingIDFrameProxyMap;
base::LazyInstance<RoutingIDFrameProxyMap>::DestructorAtExit
    g_routing_id_frame_proxy_map = LAZY_INSTANCE_INITIALIZER;

using TokenFrameMap = std::unordered_map<blink::RemoteFrameToken,
                                         RenderFrameProxyHost*,
                                         blink::RemoteFrameToken::Hasher>;
base::LazyInstance<TokenFrameMap>::Leaky g_token_frame_proxy_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
void RenderFrameProxyHost::SetObserverForTesting(TestObserver* observer) {
  // Prevent clobbering by previously set TestObserver.
  DCHECK(!observer || (observer && !g_observer_for_testing));
  g_observer_for_testing = observer;
}

// static
RenderFrameProxyHost* RenderFrameProxyHost::FromID(int process_id,
                                                   int routing_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RoutingIDFrameProxyMap* frames = g_routing_id_frame_proxy_map.Pointer();
  auto it = frames->find(RenderFrameProxyHostID(process_id, routing_id));
  return it == frames->end() ? nullptr : it->second;
}

// static
RenderFrameProxyHost* RenderFrameProxyHost::FromFrameToken(
    int process_id,
    const blink::RemoteFrameToken& frame_token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TokenFrameMap* frames = g_token_frame_proxy_map.Pointer();
  auto it = frames->find(frame_token);
  // The check against |process_id| isn't strictly necessary, but represents
  // an extra level of protection against a renderer trying to force a frame
  // token.
  return it != frames->end() && it->second->GetProcess()->GetID() == process_id
             ? it->second
             : nullptr;
}

// static
bool RenderFrameProxyHost::IsFrameTokenInUse(
    const blink::RemoteFrameToken& frame_token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TokenFrameMap* frames = g_token_frame_proxy_map.Pointer();
  return frames->find(frame_token) != frames->end();
}

RenderFrameProxyHost::RenderFrameProxyHost(
    SiteInstanceGroup* site_instance_group,
    scoped_refptr<RenderViewHostImpl> render_view_host,
    FrameTreeNode* frame_tree_node,
    const blink::RemoteFrameToken& frame_token)
    : routing_id_(site_instance_group->process()->GetNextRoutingID()),
      site_instance_group_(site_instance_group),
      process_(site_instance_group->process()),
      frame_tree_node_(frame_tree_node),
      render_frame_proxy_created_(false),
      render_view_host_(std::move(render_view_host)),
      frame_token_(frame_token) {
  TRACE_EVENT_BEGIN("navigation", "RenderFrameProxyHost",
                    perfetto::Track::FromPointer(this),
                    "render_frame_proxy_host_when_created", *this);
  GetAgentSchedulingGroup().AddRoute(routing_id_, this);
  CHECK(
      g_routing_id_frame_proxy_map.Get()
          .insert(std::make_pair(
              RenderFrameProxyHostID(GetProcess()->GetID(), routing_id_), this))
          .second);
  CHECK(g_token_frame_proxy_map.Get()
            .insert(std::make_pair(frame_token_, this))
            .second);
  CHECK(render_view_host_ ||
        frame_tree_node_->render_manager()->IsMainFrameForInnerDelegate());

  bool is_proxy_to_parent =
      !frame_tree_node_->IsMainFrame() &&
      frame_tree_node_->parent()->GetSiteInstance()->group() ==
          site_instance_group;
  bool is_proxy_to_outer_delegate =
      frame_tree_node_->render_manager()->IsMainFrameForInnerDelegate();

  // If this is a proxy to parent frame or this proxy is for the inner
  // WebContents's FrameTreeNode in outer WebContents's SiteInstance, then we
  // need a CrossProcessFrameConnector.
  if (is_proxy_to_parent || is_proxy_to_outer_delegate) {
    // The RenderFrameHost navigating cross-process is destroyed and a proxy for
    // it is created in the parent's process. CrossProcessFrameConnector
    // initialization only needs to happen on an initial cross-process
    // navigation, when the RenderFrameHost leaves the same process as its
    // parent. The same CrossProcessFrameConnector is used for subsequent cross-
    // process navigations, but it will be destroyed if the frame is
    // navigated back to the same SiteInstance as its parent.
    cross_process_frame_connector_ =
        std::make_unique<CrossProcessFrameConnector>(this);
  }

  if (g_observer_for_testing)
    g_observer_for_testing->OnCreated(this);
}

RenderFrameProxyHost::~RenderFrameProxyHost() {
  if (g_observer_for_testing)
    g_observer_for_testing->OnDeleted(this);

  if (GetProcess()->IsInitializedAndNotDead()) {
    // TODO(nasko): For now, don't send this IPC for top-level frames, as
    // the top-level RenderFrame will delete the RenderFrameProxy.
    // This can be removed once we don't have a swapped out state on
    // RenderFrame. See https://crbug.com/357747
    if (!frame_tree_node_->IsMainFrame() && is_render_frame_proxy_live())
      GetAssociatedRemoteFrame()->DetachAndDispose();
  }

  // TODO(arthursonzogni): There are no known reason for removing the
  // RenderViewHostImpl here instead of automatically at the end of the
  // destructor. This line can be removed.
  render_view_host_.reset();

  GetAgentSchedulingGroup().RemoveRoute(routing_id_);
  g_routing_id_frame_proxy_map.Get().erase(
      RenderFrameProxyHostID(GetProcess()->GetID(), routing_id_));
  g_token_frame_proxy_map.Get().erase(frame_token_);
  TRACE_EVENT_END("navigation", perfetto::Track::FromPointer(this));
}

void RenderFrameProxyHost::SetChildRWHView(RenderWidgetHostViewChildFrame* view,
                                           const gfx::Size* initial_frame_size,
                                           bool allow_paint_holding) {
  cross_process_frame_connector_->SetView(view, allow_paint_holding);
  if (initial_frame_size)
    cross_process_frame_connector_->SetLocalFrameSize(*initial_frame_size);
}

RenderViewHostImpl* RenderFrameProxyHost::GetRenderViewHost() {
  return render_view_host_.get();
}

bool RenderFrameProxyHost::Send(IPC::Message* msg) {
  return GetAgentSchedulingGroup().Send(msg);
}

bool RenderFrameProxyHost::OnMessageReceived(const IPC::Message& msg) {
  return false;
}

std::string RenderFrameProxyHost::ToDebugString() {
  return "RFPH:" + frame_tree_node_->current_frame_host()->ToDebugString();
}

bool RenderFrameProxyHost::InitRenderFrameProxy(
    BatchedProxyIPCSender* batched_proxy_ipc_sender) {
  DCHECK(!render_frame_proxy_created_);
  // We shouldn't be creating proxies for subframes of frames in
  // BackForwardCache.
  DCHECK(!frame_tree_node_->current_frame_host()->IsInBackForwardCache());

  // If the current RenderFrameHost is pending deletion, no new proxies should
  // be created for it, since this frame should no longer be visible from other
  // processes. We can get here with postMessage while trying to recreate
  // proxies for the sender.
  if (frame_tree_node_->current_frame_host()->IsPendingDeletion())
    return false;

  // It is possible to reach this when the process is dead (in particular, when
  // creating proxies from CreateProxiesForChildFrame).  In that case, don't
  // create the proxy.  The process shouldn't be resurrected just to create
  // RenderFrameProxies; it should be restored only if it needs to host a
  // RenderFrame.  When that happens, the process will be reinitialized, and
  // all necessary proxies, including any of the ones we skipped here, will be
  // created by CreateProxiesForSiteInstance. See https://crbug.com/476846
  if (!GetProcess()->IsInitializedAndNotDead())
    return false;

  std::optional<blink::FrameToken> opener_frame_token;
  if (frame_tree_node_->opener()) {
    opener_frame_token =
        frame_tree_node_->render_manager()->GetOpenerFrameToken(
            site_instance_group());
  }

  // The current `RenderFrameHost`'s `devtools_frame_token` can be used here
  // because it is not expected to differ when there is a
  // `RenderFrameProxyHost` in a separate window. The token may change on
  // MPArch activations in the main frame (e.g., prerender), but those
  // cannot occur if the `BrowsingInstance` has more than one window.
  const ::base::UnguessableToken& devtools_frame_token =
      frame_tree_node_->current_frame_host()->devtools_frame_token();

  if (frame_tree_node_->parent()) {
    // It is safe to use GetRenderFrameProxyHost to get the parent proxy, since
    // new child frames always start out as local frames, so a new proxy should
    // never have a RenderFrameHost as a parent.
    RenderFrameProxyHost* parent_proxy =
        frame_tree_node_->parent()
            ->browsing_context_state()
            ->GetRenderFrameProxyHost(site_instance_group());
    CHECK(parent_proxy);

    // Proxies that aren't live in the parent node should not be initialized
    // here, since there is no valid parent `blink::RemoteFrame` on the renderer
    // side. This can happen when adding a new child frame after an opener
    // process crashed and was reloaded. See https://crbug.com/501152.
    //
    // Note that with `batched_proxy_ipc_sender`, the parent proxy could be
    // non-live but pending creation. In that case, it is fine to initialize
    // this proxy, as `batched_proxy_ipc_sender` guarantees that its parent
    // will be created first in the renderer.
    GlobalRoutingID parent_global_id = parent_proxy->GetGlobalID();
    bool is_parent_proxy_creation_pending =
        batched_proxy_ipc_sender &&
        batched_proxy_ipc_sender->IsProxyCreationPending(parent_global_id);
    if (!parent_proxy->is_render_frame_proxy_live() &&
        !is_parent_proxy_creation_pending) {
      return false;
    }

    // TODO(crbug.com/40248300): Support main frame proxy batch creation
    // with batched_proxy_ipc_sender.
    if (batched_proxy_ipc_sender) {
      batched_proxy_ipc_sender->AddNewChildProxyCreationTask(
          GetSafeRef(), frame_token_, opener_frame_token,
          frame_tree_node_->tree_scope_type(),
          frame_tree_node_->current_replication_state().Clone(),
          frame_tree_node_->frame_owner_properties().Clone(),
          frame_tree_node_->IsLoading(), devtools_frame_token,
          CreateAndBindRemoteFrameInterfaces(), parent_global_id);

      // Don't call `SetRenderFrameProxyCreated(true)` here, since the proxy
      // wasn't actually created. This will be called for all
      // `RenderFrameProxyHosts` later in
      // `BatchedProxyIPCSender::CreateAllProxies`, after all proxies
      // are created.
    } else {
      parent_proxy->GetAssociatedRemoteFrame()->CreateRemoteChild(
          frame_token_, opener_frame_token, frame_tree_node_->tree_scope_type(),
          frame_tree_node_->current_replication_state().Clone(),
          frame_tree_node_->frame_owner_properties().Clone(),
          frame_tree_node_->IsLoading(), devtools_frame_token,
          CreateAndBindRemoteFrameInterfaces());
      SetRenderFrameProxyCreated(true);
    }
  } else {
    GetRenderViewHost()->GetAssociatedPageBroadcast()->CreateRemoteMainFrame(
        frame_token_, opener_frame_token,
        frame_tree_node_->current_replication_state().Clone(),
        frame_tree_node_->IsLoading(), devtools_frame_token,
        CreateAndBindRemoteFrameInterfaces(),
        CreateAndBindRemoteMainFrameInterfaces());
    SetRenderFrameProxyCreated(true);
  }

  return true;
}

AgentSchedulingGroupHost& RenderFrameProxyHost::GetAgentSchedulingGroup() {
  return site_instance_group_->agent_scheduling_group();
}

void RenderFrameProxyHost::SetRenderFrameProxyCreated(bool created) {
  render_frame_proxy_created_ = created;

  // Reset the mojo channels when the associated renderer is gone. It allows
  // reuse of the mojo channels when this RenderFrameProxyHost is reused.
  if (!render_frame_proxy_created_)
    TearDownMojoConnection();
}

const mojo::AssociatedRemote<blink::mojom::RemoteFrame>&
RenderFrameProxyHost::GetAssociatedRemoteFrame() {
  DCHECK(remote_frame_.is_bound());
  return remote_frame_;
}

const mojo::AssociatedRemote<blink::mojom::RemoteMainFrame>&
RenderFrameProxyHost::GetAssociatedRemoteMainFrame() {
  DCHECK(remote_main_frame_.is_bound());
  return remote_main_frame_;
}

void RenderFrameProxyHost::SetInheritedEffectiveTouchAction(
    cc::TouchAction touch_action) {
  cross_process_frame_connector_->OnSetInheritedEffectiveTouchAction(
      touch_action);
}

void RenderFrameProxyHost::UpdateRenderThrottlingStatus(bool is_throttled,
                                                        bool subtree_throttled,
                                                        bool display_locked) {
  cross_process_frame_connector_->UpdateRenderThrottlingStatus(
      is_throttled, subtree_throttled, display_locked);
}

void RenderFrameProxyHost::VisibilityChanged(
    blink::mojom::FrameVisibility visibility) {
  cross_process_frame_connector_->OnVisibilityChanged(visibility);
}

void RenderFrameProxyHost::UpdateOpener() {
  // Another frame in this proxy's SiteInstanceGroup may reach the new opener by
  // first reaching this proxy and then referencing its window.opener.  Ensure
  // the new opener's proxy exists in this case. If this is already a proxy for
  // a frame in another BrowsingInstance in the same CoopRelatedGroup, we should
  // not add extra proxies, as these are not discoverable via window.opener
  // because property access is restricted.
  SiteInstanceGroup* current_group =
      frame_tree_node_->current_frame_host()->GetSiteInstance()->group();
  bool is_coop_rp_proxy =
      current_group->IsCoopRelatedSiteInstanceGroup(site_instance_group()) &&
      !current_group->IsRelatedSiteInstanceGroup(site_instance_group());
  if (is_coop_rp_proxy) {
    return;
  }

  if (frame_tree_node_->opener()) {
    frame_tree_node_->opener()->render_manager()->CreateOpenerProxies(
        site_instance_group(), frame_tree_node_,
        frame_tree_node_->current_frame_host()->browsing_context_state());
  }

  if (!is_render_frame_proxy_live())
    return;
  auto opener_frame_token =
      frame_tree_node_->render_manager()->GetOpenerFrameToken(
          site_instance_group());
  GetAssociatedRemoteFrame()->UpdateOpener(opener_frame_token);
}

void RenderFrameProxyHost::SetFocusedFrame() {
  if (!is_render_frame_proxy_live())
    return;
  GetAssociatedRemoteFrame()->Focus();
}

void RenderFrameProxyHost::ScrollRectToVisible(
    const gfx::RectF& rect_to_scroll,
    blink::mojom::ScrollIntoViewParamsPtr params) {
  if (!is_render_frame_proxy_live())
    return;
  GetAssociatedRemoteFrame()->ScrollRectToVisible(rect_to_scroll,
                                                  std::move(params));
}

void RenderFrameProxyHost::Detach() {
  if (frame_tree_node_->render_manager()->IsMainFrameForInnerDelegate()) {
    frame_tree_node_->render_manager()->RemoveOuterDelegateFrame();
    return;
  }

  // For a main frame with no outer delegate, no further work is needed. In this
  // case, detach can only be triggered by closing the entire RenderViewHost.
  // Instead, this cleanup relies on the destructors of RenderFrameHost and
  // RenderFrameProxyHost decrementing the refcounts of their associated
  // RenderViewHost. When the refcount hits 0, the corresponding renderer object
  // is cleaned up. Since WebContents destruction will also destroy
  // RenderFrameHost/RenderFrameProxyHost objects in FrameTree, this eventually
  // results in all the associated RenderViewHosts being closed.
  if (frame_tree_node_->IsMainFrame())
    return;

  // Otherwise, a remote child frame has been removed from the frame tree.
  // Make sure that this action is mirrored to all the other renderers, so
  // the frame tree remains consistent.
  frame_tree_node_->current_frame_host()->DetachFromProxy();
}

void RenderFrameProxyHost::CheckCompleted() {
  RenderFrameHostImpl* target_rfh = frame_tree_node()->current_frame_host();
  target_rfh->GetAssociatedLocalFrame()->CheckCompleted();
}

void RenderFrameProxyHost::EnableAutoResize(const gfx::Size& min_size,
                                            const gfx::Size& max_size) {
  if (!is_render_frame_proxy_live())
    return;
  GetAssociatedRemoteFrame()->EnableAutoResize(min_size, max_size);
}

void RenderFrameProxyHost::DisableAutoResize() {
  if (!is_render_frame_proxy_live())
    return;
  GetAssociatedRemoteFrame()->DisableAutoResize();
}

void RenderFrameProxyHost::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  if (!is_render_frame_proxy_live())
    return;
  GetAssociatedRemoteFrame()->DidUpdateVisualProperties(metadata);
}

void RenderFrameProxyHost::ChildProcessGone() {
  if (!is_render_frame_proxy_live())
    return;
  GetAssociatedRemoteFrame()->ChildProcessGone();
}

void RenderFrameProxyHost::DidFocusFrame() {
  TRACE_EVENT("navigation", "RenderFrameProxyHost::DidFocusFrame",
              ChromeTrackEvent::kFrameTreeNodeInfo, *frame_tree_node_,
              ChromeTrackEvent::kSiteInstanceGroup, *site_instance_group());
  // If a fenced frame has requested focus something wrong has gone on. We do
  // not support programmatic focus between the embedder and embeddee because
  // that could be a side channel.
  if (frame_tree_node_->IsInFencedFrameTree() &&
      frame_tree_node_->render_manager()->GetProxyToOuterDelegate() == this) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RFPH_FOCUSED_FENCED_FRAME);
    return;
  }

  RenderFrameHostImpl* render_frame_host =
      frame_tree_node_->current_frame_host();
  // Do not focus inactive RenderFrameHost.
  if (!render_frame_host->IsActive())
    return;
  frame_tree_node_->SetFocusedFrame(site_instance_group());
}

void RenderFrameProxyHost::CapturePaintPreviewOfCrossProcessSubframe(
    const gfx::Rect& clip_rect,
    const base::UnguessableToken& guid) {
  RenderFrameHostImpl* rfh = frame_tree_node_->current_frame_host();
  // Do not capture paint on behalf of inactive RenderFrameHost.
  if (rfh->IsInactiveAndDisallowActivation(
          DisallowActivationReasonId::kCapturePaintPreviewProxy)) {
    return;
  }
  rfh->delegate()->CapturePaintPreviewOfCrossProcessSubframe(clip_rect, guid,
                                                             rfh);
}

void RenderFrameProxyHost::SetIsInert(bool inert) {
  cross_process_frame_connector_->SetIsInert(inert);
}

std::u16string RenderFrameProxyHost::SerializePostMessageSourceOrigin(
    const url::Origin& source_origin) {
  std::u16string source_origin_string =
      base::UTF8ToUTF16(source_origin.Serialize());

  // TODO(crbug.com/40554285, crbug.com/40467682): This serialization used to
  // happen in blink via blink::SecurityOrigin::ToString(), but is now happening
  // here via url::Origin::Serialize(). The two are the same except for one
  // unfortunate difference with file URLs. url::Origin always serializes them
  // as "file://", while blink::SecurityOrigin serializes them to "null" or
  // "file://" depending on the `allow_file_access_from_file_urls` flag in
  // WebPreferences. For now, mimic Blink's file: URL serialization here to
  // minimize compat risks. Eventually, this should be improved to (1) rely on
  // url::Origin's version and fix url::Origin::Serialize() to honor
  // `allow_file_access_from_file_urls` if that is important enough to support,
  // (2) plumb `source_origin` further into blink in the receiving renderer, so
  // that it can be serialized there (this requires refactoring the other uses
  // of RenderFrameHostImpl::PostMessageEvent()), or (3) fix file: URLs to
  // always correspond to opaque origins, so that their serializations are
  // always "null" in both blink::SecurityOrigin and url::Origin.
  if (source_origin.scheme() == url::kFileScheme) {
    auto prefs = frame_tree_node()
                     ->current_frame_host()
                     ->delegate()
                     ->GetOrCreateWebPreferences();
    if (!prefs.allow_file_access_from_file_urls) {
      source_origin_string = u"null";
    }
  }
  return source_origin_string;
}

void RenderFrameProxyHost::RouteMessageEvent(
    const std::optional<blink::LocalFrameToken>& source_frame_token,
    const url::Origin& source_origin,
    const std::u16string& target_origin,
    blink::TransferableMessage message) {
  RenderFrameHostImpl* target_rfh = frame_tree_node()->current_frame_host();
  if (!target_rfh->IsRenderFrameLive()) {
    // Check if there is an inner delegate involved; if so target its main
    // frame or otherwise return since there is no point in forwarding the
    // message.
    FrameTreeNode* target_ftn = FrameTreeNode::GloballyFindByID(
        target_rfh->inner_tree_main_frame_tree_node_id());
    target_rfh = target_ftn ? target_ftn->current_frame_host() : nullptr;

    if (!target_rfh || !target_rfh->IsRenderFrameLive())
      return;
  }

  // |target_origin| argument of postMessage is already checked by
  // blink::LocalDOMWindow::DispatchMessageEventWithOriginCheck (needed for
  // messages sent within the same process - e.g. same-site, cross-origin),
  // but this check needs to be duplicated below in case the recipient renderer
  // process got compromised (i.e. in case the renderer-side check may be
  // bypassed).
  if (!target_origin.empty()) {
    url::Origin target_url_origin =
        url::Origin::Create(GURL(base::UTF16ToUTF8(target_origin)));

    // Renderer should send either an empty string (this is how "*" is expressed
    // in the IPC) or a valid, non-opaque origin.  OTOH, there are no security
    // implications here - the message payload needs to be protected from an
    // unintended recipient, not from the sender.
    DCHECK(!target_url_origin.opaque());

    // While the postMessage was in flight, the target might have navigated away
    // to another origin.  In this case, the postMessage should be silently
    // dropped.
    if (target_url_origin != target_rfh->GetLastCommittedOrigin())
      return;
  }

  std::u16string source_origin_string =
      SerializePostMessageSourceOrigin(source_origin);

  // Verify the source origin. Note that this used to skip cases where the
  // origin serialized to "null", but now that old behavior is behind a kill
  // switch.
  //
  // TODO(crbug.com/40109437): Remove this fallback and always validate opaque
  // origins once rollout is complete.
  bool should_verify_source_origin =
      base::FeatureList::IsEnabled(
          features::kAdditionalOpaqueOriginEnforcements) ||
      source_origin_string != u"null";
  if (should_verify_source_origin) {
    auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
    if (!policy->HostsOrigin(GetProcess()->GetID(), source_origin)) {
      bad_message::ReceivedBadMessage(
          GetProcess(), bad_message::RFPH_POST_MESSAGE_INVALID_SOURCE_ORIGIN);
      return;
    }
  }

  // Only deliver the message if the request came from a RenderFrameHost in the
  // same CoopRelatedGroup or if this WebContents is dedicated to a browser
  // plugin guest.
  //
  // TODO(alexmos, lazyboy):  The check for browser plugin guest currently
  // requires going through the delegate.  It should be refactored and
  // performed here once OOPIF support in <webview> is further along.
  SiteInstanceGroup* target_group = target_rfh->GetSiteInstance()->group();
  if (!target_group->IsCoopRelatedSiteInstanceGroup(site_instance_group()) &&
      !target_rfh->delegate()->ShouldRouteMessageEvent(target_rfh)) {
    return;
  }

  // Don't deliver any messages to PDF content frames.
  if (target_rfh->GetSiteInstance()->GetSiteInfo().is_pdf()) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFPH_POST_MESSAGE_PDF_CONTENT_FRAME);
    return;
  }

  // If there is a |source_frame_token|, translate it to the frame token of the
  // equivalent RenderFrameProxyHost in the target process.
  std::optional<blink::RemoteFrameToken> translated_source_token;
  if (source_frame_token) {
    RenderFrameHostImpl* source_rfh = RenderFrameHostImpl::FromFrameToken(
        GetProcess()->GetID(), source_frame_token.value());
    if (source_rfh) {
      // https://crbug.com/822958: If the postMessage is going to a descendant
      // frame, ensure that any pending visual properties such as size are sent
      // to the target frame before the postMessage, as sites might implicitly
      // be relying on this ordering.
      bool target_is_descendant_of_source = false;
      for (RenderFrameHost* rfh = target_rfh; rfh; rfh = rfh->GetParent()) {
        if (rfh == source_rfh) {
          target_is_descendant_of_source = true;
          break;
        }
      }
      if (target_is_descendant_of_source) {
        target_rfh->GetRenderWidgetHost()
            ->SynchronizeVisualPropertiesIgnoringPendingAck();
      }

      if (!target_group->IsRelatedSiteInstanceGroup(site_instance_group()) &&
          target_group->IsCoopRelatedSiteInstanceGroup(site_instance_group())) {
        // If we're getting messaged by a frame in a different BrowsingInstance
        // in the same CoopRelatedGroup, we should create only the proxies
        // needed for event.source (and its ancestor chain).
        source_rfh->frame_tree_node()
            ->render_manager()
            ->CreateRenderFrameProxyAndAncestorChainIfNeeded(
                target_rfh->GetSiteInstance()->group());
      } else {
        // Ensure that we have a swapped-out RVH and proxy for the source frame
        // in the target SiteInstance. If it doesn't exist, create it on demand
        // and also create its opener chain, since that will also be accessible
        // to the target page.
        // TODO(crbug.com/40261772): Using WebContents here disregards
        // the possibility of postMessaging an iframe. This is broken, and
        // sometimes leads to null event.source.
        target_rfh->delegate()->EnsureOpenerProxiesExist(source_rfh);
      }

      // If the message source is a cross-process subframe, its proxy will only
      // be created in --site-per-process mode, which is the case when we set an
      // actual non-empty value for |translated_source_token|. Otherwise (if the
      // proxy wasn't created), use an empty |translated_source_token| (see
      // https://crbug.com/485520 for discussion on why this is ok).
      // The proxy may be in a different BrowsingContextState in the case of
      // postMessages exchanged across inner and outer delegates.
      RenderFrameProxyHost* source_proxy_in_target_group =
          source_rfh->browsing_context_state()->GetRenderFrameProxyHost(
              target_group,
              BrowsingContextState::ProxyAccessMode::kAllowOuterDelegate);
      if (source_proxy_in_target_group) {
        translated_source_token = source_proxy_in_target_group->GetFrameToken();
      }
    }
  }

  target_rfh->PostMessageEvent(translated_source_token, source_origin_string,
                               target_origin, std::move(message));
}

void RenderFrameProxyHost::PrintCrossProcessSubframe(const gfx::Rect& rect,
                                                     int32_t document_cookie) {
  RenderFrameHostImpl* rfh = frame_tree_node_->current_frame_host();
  rfh->delegate()->PrintCrossProcessSubframe(rect, document_cookie, rfh);
}

void RenderFrameProxyHost::SynchronizeVisualProperties(
    const blink::FrameVisualProperties& frame_visual_properties) {
  cross_process_frame_connector_->OnSynchronizeVisualProperties(
      frame_visual_properties);
}

void RenderFrameProxyHost::FocusPage() {
  frame_tree_node_->current_frame_host()->FocusPage();
}

void RenderFrameProxyHost::TakeFocus(bool reverse) {
  frame_tree_node_->current_frame_host()->TakeFocus(reverse);
}

void RenderFrameProxyHost::UpdateTargetURL(
    const GURL& url,
    blink::mojom::RemoteMainFrameHost::UpdateTargetURLCallback callback) {
  frame_tree_node_->current_frame_host()->UpdateTargetURL(url,
                                                          std::move(callback));
}

void RenderFrameProxyHost::RouteCloseEvent() {
  // The renderer already ensures that this can only be called on an outermost
  // main frame - see DOMWindow::Close().  Terminate the renderer if this is
  // not the case.
  RenderFrameHostImpl* rfh = frame_tree_node_->current_frame_host();
  if (!rfh->IsOutermostMainFrame()) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFPH_WINDOW_CLOSE_ON_NON_OUTERMOST_FRAME);
    return;
  }

  // Tell the active RenderFrameHost to run unload handlers and close, as long
  // as the request came from a RenderFrameHost in the same BrowsingInstance.
  // We receive this from a WebViewImpl when it receives a request to close
  // the window containing the active RenderFrameHost. Note that different
  // BrowsingInstances in the same CoopRelatedGroup should not be able to close
  // each other's windows, therefore checking IsRelatedSiteInstance() is enough.
  if (site_instance_group()->IsRelatedSiteInstanceGroup(
          rfh->GetSiteInstance()->group())) {
    rfh->ClosePage(RenderFrameHostImpl::ClosePageSource::kRenderer);
  }
}

void RenderFrameProxyHost::OpenURL(blink::mojom::OpenURLParamsPtr params) {
  // Verify and unpack IPC payload.
  GURL validated_url;
  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
  RenderFrameHostImpl* current_rfh = frame_tree_node_->current_frame_host();

  if (!VerifyOpenURLParams(current_rfh, GetProcess(), params, &validated_url,
                           &blob_url_loader_factory)) {
    return;
  }

  // Only active documents are allowed to navigate from frame proxy:
  // - If the document is in pending deletion, ignore the navigation, because
  // the frame is going to disappear soon anyway.
  // - If the document is in back-forward cache, it's not allowed to navigate as
  // it should remain frozen. Ignore the request and evict the document from
  // back-forward cache.
  // - If the document is prerendering, we don't expect to get here because
  // prerendering pages are expected to defer cross-origin iframes, so there
  // should not be any OOPIFs. Just cancel prerendering if we get here.
  if (current_rfh->IsInactiveAndDisallowActivation(
          DisallowActivationReasonId::kOpenURL)) {
    return;
  }

  // Verify that we are in the same BrowsingInstance as the current
  // RenderFrameHost. Note that different BrowsingInstances in the same
  // CoopRelatedGroup should not be able to navigate each other's frames,
  // therefore checking IsRelatedSiteInstance() is enough.
  if (!site_instance_group()->IsRelatedSiteInstanceGroup(
          current_rfh->GetSiteInstance()->group())) {
    return;
  }

  if (params->initiator_frame_token) {
    RenderFrameHostImpl* initiator_frame = RenderFrameHostImpl::FromFrameToken(
        GetProcess()->GetID(), params->initiator_frame_token.value());
    if (current_rfh->IsOutermostMainFrame()) {
      MaybeRecordAdClickMainFrameNavigationMetrics(
          initiator_frame, params->initiator_activation_and_ad_status);
    }
  }

  // Since this navigation targeted a specific RenderFrameProxy, it should stay
  // in the current tab.
  DCHECK_EQ(WindowOpenDisposition::CURRENT_TAB, params->disposition);

  // Augment |download_policy| for situations that were not covered on the
  // renderer side, e.g. status not available on remote frame, etc.
  blink::NavigationDownloadPolicy download_policy = params->download_policy;
  GetContentClient()->browser()->AugmentNavigationDownloadPolicy(
      current_rfh, params->user_gesture, &download_policy);

  if ((frame_tree_node_->pending_frame_policy().sandbox_flags &
       network::mojom::WebSandboxFlags::kDownloads) !=
      network::mojom::WebSandboxFlags::kNone) {
    download_policy.SetDisallowed(blink::NavigationDownloadType::kSandbox);
  }

  // This will be used to set the Navigation Timing API navigationStart
  // parameter for renderer navigations in the remote frame. If the navigation
  // must wait on the current RenderFrameHost to execute its BeforeUnload event,
  // the navigation start will be updated when the BeforeUnload ack is received.
  const auto navigation_start_time = base::TimeTicks::Now();

  blink::LocalFrameToken* initiator_frame_token =
      base::OptionalToPtr(params->initiator_frame_token);

  // TODO(lfg, lukasza): Remove |extra_headers| parameter from
  // RequestTransferURL method once both RenderFrameProxyHost and
  // RenderFrameHostImpl call RequestOpenURL from their OnOpenURL handlers.
  // See also https://crbug.com/647772.
  // TODO(clamy): The transition should probably be changed for POST navigations
  // to PAGE_TRANSITION_FORM_SUBMIT. See https://crbug.com/829827.
  frame_tree_node_->navigator().NavigateFromFrameProxy(
      current_rfh, validated_url, initiator_frame_token, GetProcess()->GetID(),
      params->initiator_origin, params->initiator_base_url,
      RenderFrameHostImpl::GetSourceSiteInstanceFromFrameToken(
          initiator_frame_token, GetProcess()->GetID(),
          current_rfh->GetStoragePartition()),
      params->referrer.To<content::Referrer>(), ui::PAGE_TRANSITION_LINK,
      params->should_replace_current_entry, download_policy,
      params->post_body ? "POST" : "GET", params->post_body,
      params->extra_headers, std::move(blob_url_loader_factory),
      std::move(params->source_location), params->user_gesture,
      params->is_form_submission, params->impression,
      params->initiator_activation_and_ad_status, navigation_start_time,
      /*is_embedder_initiated_fenced_frame_navigation=*/false,
      /*is_unfenced_top_navigation=*/false,
      /*force_new_browsing_instance=*/false, params->is_container_initiated,
      params->has_rel_opener, params->storage_access_api_status);
}

void RenderFrameProxyHost::UpdateViewportIntersection(
    blink::mojom::ViewportIntersectionStatePtr intersection_state,
    const std::optional<blink::FrameVisualProperties>& visual_properties) {
  cross_process_frame_connector_->UpdateViewportIntersection(
      *intersection_state, visual_properties);
}

void RenderFrameProxyHost::DidChangeOpener(
    const std::optional<blink::LocalFrameToken>& opener_frame_token) {
  frame_tree_node_->render_manager()->DidChangeOpener(opener_frame_token,
                                                      site_instance_group());
}

void RenderFrameProxyHost::AdvanceFocus(
    blink::mojom::FocusType focus_type,
    const blink::LocalFrameToken& source_frame_token) {
  // Translate the source RenderFrameHost in this process to its equivalent
  // RenderFrameProxyHost in the target SiteInstanceGroup.  This is needed for
  // continuing the focus traversal from correct place in a parent frame after
  // one of its child frames finishes its traversal.
  RenderFrameHostImpl* source_rfh = RenderFrameHostImpl::FromFrameToken(
      GetProcess()->GetID(), source_frame_token);
  RenderFrameHostImpl* target_rfh = frame_tree_node_->current_frame_host();
  RenderFrameProxyHost* source_proxy =
      source_rfh
          ? source_rfh->browsing_context_state()->GetRenderFrameProxyHost(
                target_rfh->GetSiteInstance()->group())
          : nullptr;

  if (source_rfh && (source_rfh->HasTransientUserActivation() ||
                     source_rfh->FocusSourceHasTransientUserActivation())) {
    target_rfh->ActivateFocusSourceUserActivation();
    source_rfh->DeactivateFocusSourceUserActivation();
  }

  target_rfh->AdvanceFocus(focus_type, source_proxy);
  target_rfh->delegate()->OnAdvanceFocus(source_rfh);
}

bool RenderFrameProxyHost::IsInertForTesting() {
  return cross_process_frame_connector_->IsInert();
}

mojo::PendingAssociatedReceiver<blink::mojom::RemoteFrame>
RenderFrameProxyHost::BindRemoteFrameReceiverForTesting() {
  remote_frame_.reset();
  return remote_frame_.BindNewEndpointAndPassDedicatedReceiver();
}

mojo::PendingAssociatedReceiver<blink::mojom::RemoteMainFrame>
RenderFrameProxyHost::BindRemoteMainFrameReceiverForTesting() {
  remote_main_frame_.reset();
  return remote_main_frame_.BindNewEndpointAndPassDedicatedReceiver();
}

blink::mojom::RemoteFrameInterfacesFromBrowserPtr
RenderFrameProxyHost::CreateAndBindRemoteFrameInterfaces() {
  auto params = blink::mojom::RemoteFrameInterfacesFromBrowser::New();
  BindRemoteFrameInterfaces(
      params->frame_receiver.InitWithNewEndpointAndPassRemote(),
      params->frame_host.InitWithNewEndpointAndPassReceiver());
  return params;
}

blink::mojom::RemoteMainFrameInterfacesPtr
RenderFrameProxyHost::CreateAndBindRemoteMainFrameInterfaces() {
  auto params = blink::mojom::RemoteMainFrameInterfaces::New();
  BindRemoteMainFrameInterfaces(
      params->main_frame.InitWithNewEndpointAndPassRemote(),
      params->main_frame_host.InitWithNewEndpointAndPassReceiver());
  return params;
}

void RenderFrameProxyHost::BindRemoteFrameInterfaces(
    mojo::PendingAssociatedRemote<blink::mojom::RemoteFrame> remote_frame,
    mojo::PendingAssociatedReceiver<blink::mojom::RemoteFrameHost>
        remote_frame_host_receiver) {
  DCHECK(!remote_frame_.is_bound());
  DCHECK(!remote_frame_host_receiver_.is_bound());

  remote_frame_.Bind(std::move(remote_frame));
  remote_frame_host_receiver_.Bind(std::move(remote_frame_host_receiver));

  if (g_observer_for_testing)
    g_observer_for_testing->OnRemoteFrameBound(this);
}

void RenderFrameProxyHost::BindRemoteMainFrameInterfaces(
    mojo::PendingAssociatedRemote<blink::mojom::RemoteMainFrame>
        remote_main_frame,
    mojo::PendingAssociatedReceiver<blink::mojom::RemoteMainFrameHost>
        remote_main_frame_host_receiver) {
  DCHECK(!remote_main_frame_.is_bound());
  DCHECK(!remote_main_frame_host_receiver_.is_bound());

  remote_main_frame_.Bind(std::move(remote_main_frame));
  remote_main_frame_host_receiver_.Bind(
      std::move(remote_main_frame_host_receiver));

  if (g_observer_for_testing)
    g_observer_for_testing->OnRemoteMainFrameBound(this);
}

void RenderFrameProxyHost::TearDownMojoConnection() {
  remote_frame_.reset();
  remote_frame_host_receiver_.reset();
  remote_main_frame_.reset();
  remote_main_frame_host_receiver_.reset();
}

void RenderFrameProxyHost::WriteIntoTrace(
    perfetto::TracedProto<TraceProto> proto) const {
  proto->set_routing_id(GetRoutingID());
  proto->set_process_id(GetProcess()->GetID());
  proto->set_is_render_frame_proxy_live(is_render_frame_proxy_live());
  if (site_instance_group()) {
    proto->set_rvh_map_id(frame_tree_node_->frame_tree()
                              .GetRenderViewHostMapId(site_instance_group())
                              .value());
    proto->set_site_instance_id(site_instance_group()->GetId().value());
  }
}

base::SafeRef<RenderFrameProxyHost> RenderFrameProxyHost::GetSafeRef() {
  return weak_factory_.GetSafeRef();
}

}  // namespace content
