// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_proxy_host.h"

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/lazy_instance.h"
#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/scoped_active_url.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_owner_properties.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_message.h"

namespace content {

namespace {

// The (process id, routing id) pair that identifies one RenderFrameProxy.
typedef std::pair<int32_t, int32_t> RenderFrameProxyHostID;
typedef base::hash_map<RenderFrameProxyHostID, RenderFrameProxyHost*>
    RoutingIDFrameProxyMap;
base::LazyInstance<RoutingIDFrameProxyMap>::DestructorAtExit
    g_routing_id_frame_proxy_map = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
RenderFrameProxyHost* RenderFrameProxyHost::FromID(int process_id,
                                                   int routing_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RoutingIDFrameProxyMap* frames = g_routing_id_frame_proxy_map.Pointer();
  auto it = frames->find(RenderFrameProxyHostID(process_id, routing_id));
  return it == frames->end() ? NULL : it->second;
}

RenderFrameProxyHost::RenderFrameProxyHost(SiteInstance* site_instance,
                                           RenderViewHostImpl* render_view_host,
                                           FrameTreeNode* frame_tree_node)
    : routing_id_(site_instance->GetProcess()->GetNextRoutingID()),
      site_instance_(site_instance),
      process_(site_instance->GetProcess()),
      frame_tree_node_(frame_tree_node),
      render_frame_proxy_created_(false),
      render_view_host_(render_view_host) {
  GetProcess()->AddRoute(routing_id_, this);
  CHECK(g_routing_id_frame_proxy_map.Get().insert(
      std::make_pair(
          RenderFrameProxyHostID(GetProcess()->GetID(), routing_id_),
          this)).second);
  CHECK(render_view_host ||
        (frame_tree_node_->render_manager()->ForInnerDelegate() &&
         frame_tree_node_->IsMainFrame()));
  if (render_view_host)
    frame_tree_node_->frame_tree()->AddRenderViewHostRef(render_view_host_);

  bool is_proxy_to_parent = !frame_tree_node_->IsMainFrame() &&
                            frame_tree_node_->parent()
                                    ->render_manager()
                                    ->current_frame_host()
                                    ->GetSiteInstance() == site_instance;
  bool is_proxy_to_outer_delegate =
      frame_tree_node_->IsMainFrame() &&
      frame_tree_node_->render_manager()->ForInnerDelegate();

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
    cross_process_frame_connector_.reset(new CrossProcessFrameConnector(this));
  }
}

RenderFrameProxyHost::~RenderFrameProxyHost() {
  if (!destruction_callback_.is_null())
    std::move(destruction_callback_).Run();

  if (GetProcess()->IsInitializedAndNotDead()) {
    // TODO(nasko): For now, don't send this IPC for top-level frames, as
    // the top-level RenderFrame will delete the RenderFrameProxy.
    // This can be removed once we don't have a swapped out state on
    // RenderFrame. See https://crbug.com/357747
    if (!frame_tree_node_->IsMainFrame())
      Send(new FrameMsg_DeleteProxy(routing_id_));
  }

  if (render_view_host_)
    frame_tree_node_->frame_tree()->ReleaseRenderViewHostRef(render_view_host_);
  GetProcess()->RemoveRoute(routing_id_);
  g_routing_id_frame_proxy_map.Get().erase(
      RenderFrameProxyHostID(GetProcess()->GetID(), routing_id_));
}

void RenderFrameProxyHost::SetChildRWHView(
    RenderWidgetHostView* view,
    const gfx::Size* initial_frame_size) {
  cross_process_frame_connector_->SetView(
      static_cast<RenderWidgetHostViewChildFrame*>(view));
  if (initial_frame_size)
    cross_process_frame_connector_->SetLocalFrameSize(*initial_frame_size);
}

RenderViewHostImpl* RenderFrameProxyHost::GetRenderViewHost() {
  return frame_tree_node_->frame_tree()->GetRenderViewHost(
      site_instance_.get());
}

RenderWidgetHostView* RenderFrameProxyHost::GetRenderWidgetHostView() {
  return frame_tree_node_->parent()->render_manager()
      ->GetRenderWidgetHostView();
}

bool RenderFrameProxyHost::Send(IPC::Message *msg) {
  return GetProcess()->Send(msg);
}

bool RenderFrameProxyHost::OnMessageReceived(const IPC::Message& msg) {
  // Crash reports trigerred by IPC messages for this proxy should be associated
  // with the URL of the current RenderFrameHost that is being proxied.
  ScopedActiveURL scoped_active_url(this);

  if (cross_process_frame_connector_.get() &&
      cross_process_frame_connector_->OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameProxyHost, msg)
    IPC_MESSAGE_HANDLER(FrameHostMsg_Detach, OnDetach)
    IPC_MESSAGE_HANDLER(FrameHostMsg_OpenURL, OnOpenURL)
    IPC_MESSAGE_HANDLER(FrameHostMsg_CheckCompleted, OnCheckCompleted)
    IPC_MESSAGE_HANDLER(FrameHostMsg_RouteMessageEvent, OnRouteMessageEvent)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidChangeOpener, OnDidChangeOpener)
    IPC_MESSAGE_HANDLER(FrameHostMsg_AdvanceFocus, OnAdvanceFocus)
    IPC_MESSAGE_HANDLER(FrameHostMsg_FrameFocused, OnFrameFocused)
    IPC_MESSAGE_HANDLER(FrameHostMsg_PrintCrossProcessSubframe,
                        OnPrintCrossProcessSubframe)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool RenderFrameProxyHost::InitRenderFrameProxy() {
  DCHECK(!render_frame_proxy_created_);

  // It is possible to reach this when the process is dead (in particular, when
  // creating proxies from CreateProxiesForChildFrame).  In that case, don't
  // create the proxy.  The process shouldn't be resurrected just to create
  // RenderFrameProxies; it should be restored only if it needs to host a
  // RenderFrame.  When that happens, the process will be reinitialized, and
  // all necessary proxies, including any of the ones we skipped here, will be
  // created by CreateProxiesForSiteInstance. See https://crbug.com/476846
  if (!GetProcess()->IsInitializedAndNotDead())
    return false;

  int parent_routing_id = MSG_ROUTING_NONE;
  if (frame_tree_node_->parent()) {
    // It is safe to use GetRenderFrameProxyHost to get the parent proxy, since
    // new child frames always start out as local frames, so a new proxy should
    // never have a RenderFrameHost as a parent.
    RenderFrameProxyHost* parent_proxy =
        frame_tree_node_->parent()->render_manager()->GetRenderFrameProxyHost(
            site_instance_.get());
    CHECK(parent_proxy);

    // Proxies that aren't live in the parent node should not be initialized
    // here, since there is no valid parent RenderFrameProxy on the renderer
    // side.  This can happen when adding a new child frame after an opener
    // process crashed and was reloaded.  See https://crbug.com/501152.
    if (!parent_proxy->is_render_frame_proxy_live())
      return false;

    parent_routing_id = parent_proxy->GetRoutingID();
    CHECK_NE(parent_routing_id, MSG_ROUTING_NONE);
  }

  int opener_routing_id = MSG_ROUTING_NONE;
  if (frame_tree_node_->opener()) {
    opener_routing_id = frame_tree_node_->render_manager()->GetOpenerRoutingID(
        site_instance_.get());
  }

  int view_routing_id = frame_tree_node_->frame_tree()
      ->GetRenderViewHost(site_instance_.get())->GetRoutingID();
  GetProcess()->GetRendererInterface()->CreateFrameProxy(
      routing_id_, view_routing_id, opener_routing_id, parent_routing_id,
      frame_tree_node_->current_replication_state(),
      frame_tree_node_->devtools_frame_token());

  set_render_frame_proxy_created(true);

  // For subframes, initialize the proxy's FrameOwnerProperties only if they
  // differ from default values.
  bool should_send_properties =
      frame_tree_node_->frame_owner_properties() != FrameOwnerProperties();
  if (frame_tree_node_->parent() && should_send_properties) {
    Send(new FrameMsg_SetFrameOwnerProperties(
        routing_id_, frame_tree_node_->frame_owner_properties()));
  }

  return true;
}

void RenderFrameProxyHost::UpdateOpener() {
  // Another frame in this proxy's SiteInstance may reach the new opener by
  // first reaching this proxy and then referencing its window.opener.  Ensure
  // the new opener's proxy exists in this case.
  if (frame_tree_node_->opener()) {
    frame_tree_node_->opener()->render_manager()->CreateOpenerProxies(
        GetSiteInstance(), frame_tree_node_);
  }

  int opener_routing_id =
      frame_tree_node_->render_manager()->GetOpenerRoutingID(GetSiteInstance());
  Send(new FrameMsg_UpdateOpener(GetRoutingID(), opener_routing_id));
}

void RenderFrameProxyHost::SetFocusedFrame() {
  Send(new FrameMsg_SetFocusedFrame(routing_id_));
}

void RenderFrameProxyHost::ScrollRectToVisible(
    const gfx::Rect& rect_to_scroll,
    const blink::WebScrollIntoViewParams& params) {
  Send(new FrameMsg_ScrollRectToVisible(routing_id_, rect_to_scroll, params));
}

void RenderFrameProxyHost::BubbleLogicalScroll(
    blink::WebScrollDirection direction,
    blink::WebScrollGranularity granularity) {
  Send(new FrameMsg_BubbleLogicalScroll(routing_id_, direction, granularity));
}

void RenderFrameProxyHost::SetDestructionCallback(
    DestructionCallback destruction_callback) {
  destruction_callback_ = std::move(destruction_callback);
}

void RenderFrameProxyHost::OnDetach() {
  if (frame_tree_node_->render_manager()->ForInnerDelegate()) {
    // Only main frame proxy can detach for inner WebContents.
    DCHECK(frame_tree_node_->IsMainFrame());
    frame_tree_node_->render_manager()->RemoveOuterDelegateFrame();
    return;
  }

  // This message should only be received for subframes.  Note that we can't
  // restrict it to just the current SiteInstances of the ancestors of this
  // frame, because another frame in the tree may be able to detach this frame
  // by navigating its parent.
  if (frame_tree_node_->IsMainFrame()) {
    bad_message::ReceivedBadMessage(GetProcess(), bad_message::RFPH_DETACH);
    return;
  }
  frame_tree_node_->frame_tree()->RemoveFrame(frame_tree_node_);
}

void RenderFrameProxyHost::OnOpenURL(
    const FrameHostMsg_OpenURL_Params& params) {
  GURL validated_url(params.url);
  GetProcess()->FilterURL(false, &validated_url);

  mojo::ScopedMessagePipeHandle blob_url_token_handle(params.blob_url_token);
  blink::mojom::BlobURLTokenPtr blob_url_token(
      blink::mojom::BlobURLTokenPtrInfo(std::move(blob_url_token_handle),
                                        blink::mojom::BlobURLToken::Version_));
  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
  if (blob_url_token) {
    if (!params.url.SchemeIsBlob()) {
      bad_message::ReceivedBadMessage(
          GetProcess(), bad_message::RFPH_BLOB_URL_TOKEN_FOR_NON_BLOB_URL);
      return;
    }
    blob_url_loader_factory =
        ChromeBlobStorageContext::URLLoaderFactoryForToken(
            GetSiteInstance()->GetBrowserContext(), std::move(blob_url_token));
  }

  // Verify that we are in the same BrowsingInstance as the current
  // RenderFrameHost.
  RenderFrameHostImpl* current_rfh = frame_tree_node_->current_frame_host();
  if (!site_instance_->IsRelatedSiteInstance(current_rfh->GetSiteInstance()))
    return;

  // Verify if the request originator (*not* |current_rfh|) has access to the
  // contents of the POST body.
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanReadRequestBody(
          GetSiteInstance(), params.resource_request_body)) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RFPH_ILLEGAL_UPLOAD_PARAMS);
    return;
  }

  // Since this navigation targeted a specific RenderFrameProxy, it should stay
  // in the current tab.
  DCHECK_EQ(WindowOpenDisposition::CURRENT_TAB, params.disposition);

  // TODO(alexmos, creis): Figure out whether |params.user_gesture| needs to be
  // passed in as well.
  // TODO(lfg, lukasza): Remove |extra_headers| parameter from
  // RequestTransferURL method once both RenderFrameProxyHost and
  // RenderFrameHostImpl call RequestOpenURL from their OnOpenURL handlers.
  // See also https://crbug.com/647772.
  // TODO(clamy): The transition should probably be changed for POST navigations
  // to PAGE_TRANSITION_FORM_SUBMIT. See https://crbug.com/829827.
  frame_tree_node_->navigator()->NavigateFromFrameProxy(
      current_rfh, validated_url, site_instance_.get(), params.referrer,
      ui::PAGE_TRANSITION_LINK, params.should_replace_current_entry,
      params.uses_post ? "POST" : "GET", params.resource_request_body,
      params.extra_headers, std::move(blob_url_loader_factory));
}

void RenderFrameProxyHost::OnCheckCompleted() {
  RenderFrameHostImpl* target_rfh = frame_tree_node()->current_frame_host();
  target_rfh->Send(new FrameMsg_CheckCompleted(target_rfh->GetRoutingID()));
}

void RenderFrameProxyHost::OnRouteMessageEvent(
    const FrameMsg_PostMessage_Params& params) {
  RenderFrameHostImpl* target_rfh = frame_tree_node()->current_frame_host();

  // Only deliver the message if the request came from a RenderFrameHost in the
  // same BrowsingInstance or if this WebContents is dedicated to a browser
  // plugin guest.
  //
  // TODO(alexmos, lazyboy):  The check for browser plugin guest currently
  // requires going through the delegate.  It should be refactored and
  // performed here once OOPIF support in <webview> is further along.
  SiteInstance* target_site_instance = target_rfh->GetSiteInstance();
  if (!target_site_instance->IsRelatedSiteInstance(GetSiteInstance()) &&
      !target_rfh->delegate()->ShouldRouteMessageEvent(target_rfh,
                                                       GetSiteInstance()))
    return;

  FrameMsg_PostMessage_Params new_params(params);

  // If there is a source_routing_id, translate it to the routing ID of the
  // equivalent RenderFrameProxyHost in the target process.
  if (new_params.source_routing_id != MSG_ROUTING_NONE) {
    RenderFrameHostImpl* source_rfh = RenderFrameHostImpl::FromID(
        GetProcess()->GetID(), new_params.source_routing_id);
    if (!source_rfh) {
      new_params.source_routing_id = MSG_ROUTING_NONE;
    } else {
      // https://crbug.com/822958: If the postMessage is going to a descendant
      // frame, ensure that any pending visual properties such as size are sent
      // to the target frame before the postMessage, as sites might implicitly
      // be relying on this ordering.
      bool target_is_descendant_of_source = false;
      for (FrameTreeNode* node = target_rfh->frame_tree_node(); node;
           node = node->parent()) {
        if (node == source_rfh->frame_tree_node()) {
          target_is_descendant_of_source = true;
          break;
        }
      }
      if (target_is_descendant_of_source) {
        target_rfh->GetRenderWidgetHost()
            ->SynchronizeVisualPropertiesIgnoringPendingAck();
      }

      // Ensure that we have a swapped-out RVH and proxy for the source frame
      // in the target SiteInstance. If it doesn't exist, create it on demand
      // and also create its opener chain, since that will also be accessible
      // to the target page.
      target_rfh->delegate()->EnsureOpenerProxiesExist(source_rfh);

      // If the message source is a cross-process subframe, its proxy will only
      // be created in --site-per-process mode.  If the proxy wasn't created,
      // set the source routing ID to MSG_ROUTING_NONE (see
      // https://crbug.com/485520 for discussion on why this is ok).
      RenderFrameProxyHost* source_proxy_in_target_site_instance =
          source_rfh->frame_tree_node()
              ->render_manager()
              ->GetRenderFrameProxyHost(target_site_instance);
      if (source_proxy_in_target_site_instance) {
        new_params.source_routing_id =
            source_proxy_in_target_site_instance->GetRoutingID();
      } else {
        new_params.source_routing_id = MSG_ROUTING_NONE;
      }
    }
  }

  target_rfh->Send(
      new FrameMsg_PostMessageEvent(target_rfh->GetRoutingID(), new_params));
}

void RenderFrameProxyHost::OnDidChangeOpener(int32_t opener_routing_id) {
  frame_tree_node_->render_manager()->DidChangeOpener(opener_routing_id,
                                                      GetSiteInstance());
}

void RenderFrameProxyHost::OnAdvanceFocus(blink::WebFocusType type,
                                          int32_t source_routing_id) {
  RenderFrameHostImpl* target_rfh =
      frame_tree_node_->render_manager()->current_frame_host();

  // Translate the source RenderFrameHost in this process to its equivalent
  // RenderFrameProxyHost in the target process.  This is needed for continuing
  // the focus traversal from correct place in a parent frame after one of its
  // child frames finishes its traversal.
  RenderFrameHostImpl* source_rfh =
      RenderFrameHostImpl::FromID(GetProcess()->GetID(), source_routing_id);
  RenderFrameProxyHost* source_proxy =
      source_rfh
          ? source_rfh->frame_tree_node()
                ->render_manager()
                ->GetRenderFrameProxyHost(target_rfh->GetSiteInstance())
          : nullptr;

  target_rfh->AdvanceFocus(type, source_proxy);
  frame_tree_node_->current_frame_host()->delegate()->OnAdvanceFocus(
      source_rfh);
}

void RenderFrameProxyHost::OnFrameFocused() {
  frame_tree_node_->current_frame_host()->delegate()->SetFocusedFrame(
      frame_tree_node_, GetSiteInstance());
}

void RenderFrameProxyHost::OnPrintCrossProcessSubframe(const gfx::Rect& rect,
                                                       int document_cookie) {
  RenderFrameHostImpl* rfh = frame_tree_node_->current_frame_host();
  rfh->delegate()->PrintCrossProcessSubframe(rect, document_cookie, rfh);
}

}  // namespace content
