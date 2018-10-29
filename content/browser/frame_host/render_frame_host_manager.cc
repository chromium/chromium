// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_host_manager.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/frame_navigation_entry.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_factory.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_owner_properties.h"
#include "content/common/page_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/referrer.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"

#if defined(OS_MACOSX)
#include "ui/gfx/mac/scoped_cocoa_disable_screen_updates.h"
#endif  // defined(OS_MACOSX)

namespace content {

namespace {

bool IsDataOrAbout(const GURL& url) {
  GURL about_srcdoc(content::kAboutSrcDocURL);
  return url == about_srcdoc || url.IsAboutBlank() ||
         url.scheme() == url::kDataScheme;
}

}  // namespace

RenderFrameHostManager::RenderFrameHostManager(FrameTreeNode* frame_tree_node,
                                               Delegate* delegate)
    : frame_tree_node_(frame_tree_node),
      delegate_(delegate),
      weak_factory_(this) {
  DCHECK(frame_tree_node_);
}

RenderFrameHostManager::~RenderFrameHostManager() {
  if (speculative_render_frame_host_)
    UnsetSpeculativeRenderFrameHost();

  // Delete any RenderFrameProxyHosts and swapped out RenderFrameHosts.
  // It is important to delete those prior to deleting the current
  // RenderFrameHost, since the CrossProcessFrameConnector (owned by
  // RenderFrameProxyHost) points to the RenderWidgetHostView associated with
  // the current RenderFrameHost and uses it during its destructor.
  ResetProxyHosts();

  // We should always have a current RenderFrameHost except in some tests.
  SetRenderFrameHost(std::unique_ptr<RenderFrameHostImpl>());
}

void RenderFrameHostManager::Init(SiteInstance* site_instance,
                                  int32_t view_routing_id,
                                  int32_t frame_routing_id,
                                  int32_t widget_routing_id,
                                  bool renderer_initiated_creation) {
  DCHECK(site_instance);
  SetRenderFrameHost(CreateRenderFrameHost(site_instance, view_routing_id,
                                           frame_routing_id, widget_routing_id,
                                           delegate_->IsHidden(),
                                           renderer_initiated_creation));

  // Notify the delegate of the creation of the current RenderFrameHost.
  // Do this only for subframes, as the main frame case is taken care of by
  // WebContentsImpl::Init.
  if (!frame_tree_node_->IsMainFrame()) {
    delegate_->NotifySwappedFromRenderManager(
        nullptr, render_frame_host_.get(), false);
  }
}

RenderViewHostImpl* RenderFrameHostManager::current_host() const {
  if (!render_frame_host_)
    return nullptr;
  return render_frame_host_->render_view_host();
}

WebUIImpl* RenderFrameHostManager::GetNavigatingWebUI() const {
  if (speculative_render_frame_host_)
    return speculative_render_frame_host_->web_ui();
  return render_frame_host_->pending_web_ui();
}

RenderWidgetHostView* RenderFrameHostManager::GetRenderWidgetHostView() const {
  if (delegate_->GetInterstitialForRenderManager())
    return delegate_->GetInterstitialForRenderManager()->GetView();
  if (render_frame_host_)
    return render_frame_host_->GetView();
  return nullptr;
}

bool RenderFrameHostManager::ForInnerDelegate() {
  return delegate_->GetOuterDelegateFrameTreeNodeId() !=
      FrameTreeNode::kFrameTreeNodeInvalidId;
}

RenderWidgetHostImpl*
RenderFrameHostManager::GetOuterRenderWidgetHostForKeyboardInput() {
  if (!ForInnerDelegate() || !frame_tree_node_->IsMainFrame())
    return nullptr;

  FrameTreeNode* outer_contents_frame_tree_node =
      FrameTreeNode::GloballyFindByID(
          delegate_->GetOuterDelegateFrameTreeNodeId());
  return outer_contents_frame_tree_node->parent()
      ->current_frame_host()
      ->render_view_host()
      ->GetWidget();
}

FrameTreeNode* RenderFrameHostManager::GetOuterDelegateNode() {
  int outer_contents_frame_tree_node_id =
      delegate_->GetOuterDelegateFrameTreeNodeId();
  return FrameTreeNode::GloballyFindByID(outer_contents_frame_tree_node_id);
}

RenderFrameProxyHost* RenderFrameHostManager::GetProxyToParent() {
  if (frame_tree_node_->IsMainFrame())
    return nullptr;

  return GetRenderFrameProxyHost(frame_tree_node_->parent()
                                     ->render_manager()
                                     ->current_frame_host()
                                     ->GetSiteInstance());
}

RenderFrameProxyHost* RenderFrameHostManager::GetProxyToOuterDelegate() {
  int outer_contents_frame_tree_node_id =
      delegate_->GetOuterDelegateFrameTreeNodeId();
  FrameTreeNode* outer_contents_frame_tree_node =
      FrameTreeNode::GloballyFindByID(outer_contents_frame_tree_node_id);
  if (!outer_contents_frame_tree_node ||
      !outer_contents_frame_tree_node->parent()) {
    return nullptr;
  }

  return GetRenderFrameProxyHost(outer_contents_frame_tree_node->parent()
                                     ->current_frame_host()
                                     ->GetSiteInstance());
}

void RenderFrameHostManager::RemoveOuterDelegateFrame() {
  FrameTreeNode* outer_delegate_frame_tree_node =
      FrameTreeNode::GloballyFindByID(
          delegate_->GetOuterDelegateFrameTreeNodeId());
  DCHECK(outer_delegate_frame_tree_node->parent());
  outer_delegate_frame_tree_node->frame_tree()->RemoveFrame(
      outer_delegate_frame_tree_node);
}

void RenderFrameHostManager::Stop() {
  render_frame_host_->Stop();

  // A loading speculative RenderFrameHost should also stop.
  if (speculative_render_frame_host_ &&
      speculative_render_frame_host_->is_loading()) {
    speculative_render_frame_host_->Send(
        new FrameMsg_Stop(speculative_render_frame_host_->GetRoutingID()));
  }
}

void RenderFrameHostManager::SetIsLoading(bool is_loading) {
  render_frame_host_->render_view_host()->GetWidget()->SetIsLoading(is_loading);
}

void RenderFrameHostManager::OnBeforeUnloadACK(
    bool proceed,
    const base::TimeTicks& proceed_time) {
  bool proceed_to_fire_unload = false;
  delegate_->BeforeUnloadFiredFromRenderManager(proceed, proceed_time,
                                                &proceed_to_fire_unload);

  if (proceed_to_fire_unload) {
    // If we're about to close the tab and there's a speculative RFH, cancel it.
    // Otherwise, if the navigation in the speculative RFH completes before the
    // close in the current RFH, we'll lose the tab close.
    if (speculative_render_frame_host_)
      CleanUpNavigation();

    render_frame_host_->render_view_host()->ClosePage();
  }
}

void RenderFrameHostManager::DidNavigateFrame(
    RenderFrameHostImpl* render_frame_host,
    bool was_caused_by_user_gesture,
    bool is_same_document_navigation) {
  CommitPendingIfNecessary(render_frame_host, was_caused_by_user_gesture,
                           is_same_document_navigation);

  // Make sure any dynamic changes to this frame's sandbox flags and feature
  // policy that were made prior to navigation take effect.  This should only
  // happen for cross-document navigations.
  if (!is_same_document_navigation)
    CommitPendingFramePolicy();
}

void RenderFrameHostManager::CommitPendingIfNecessary(
    RenderFrameHostImpl* render_frame_host,
    bool was_caused_by_user_gesture,
    bool is_same_document_navigation) {
  if (!speculative_render_frame_host_) {
    // There's no speculative RenderFrameHost so it must be that the current
    // renderer process completed a navigation.

    // We should only hear this from our current renderer.
    DCHECK_EQ(render_frame_host_.get(), render_frame_host);

    // If the current RenderFrameHost has a pending WebUI it must be committed.
    // Note: When one tries to move same-site commit logic into RenderFrameHost
    // itself, mind that the focus setting logic inside CommitPending also needs
    // to be moved there.
    if (render_frame_host_->pending_web_ui())
      CommitPendingWebUI();
    return;
  }

  if (render_frame_host == speculative_render_frame_host_.get()) {
    // A cross-process navigation completed, so show the new renderer. If a
    // same-process navigation is also ongoing, it will be canceled when the
    // speculative RenderFrameHost replaces the current one in the commit call
    // below.
    CommitPending();
    frame_tree_node_->ResetNavigationRequest(false, true);
  } else if (render_frame_host == render_frame_host_.get()) {
    // A same-process navigation committed while a simultaneous cross-process
    // navigation is still ongoing.

    // If the current RenderFrameHost has a pending WebUI it must be committed.
    if (render_frame_host_->pending_web_ui())
      CommitPendingWebUI();

    // A navigation in the original process has taken place.  This should
    // cancel the ongoing cross-process navigation if the commit is
    // cross-document and has a user gesture (since the user might have clicked
    // on a new link while waiting for a slow navigation), but it should not
    // cancel it for same-document navigations (which might happen as
    // bookkeeping) or when there is no user gesture (which might abusively try
    // to prevent the user from leaving).  See https://crbug.com/825677 and
    // https://crbug.com/75195 for examples.
    if (!is_same_document_navigation && was_caused_by_user_gesture) {
      frame_tree_node_->ResetNavigationRequest(false, true);
      CleanUpNavigation();
    }
  } else {
    // No one else should be sending us DidNavigate in this state.
    NOTREACHED();
  }
}

void RenderFrameHostManager::DidChangeOpener(
    int opener_routing_id,
    SiteInstance* source_site_instance) {
  FrameTreeNode* opener = nullptr;
  if (opener_routing_id != MSG_ROUTING_NONE) {
    RenderFrameHostImpl* opener_rfhi = RenderFrameHostImpl::FromID(
        source_site_instance->GetProcess()->GetID(), opener_routing_id);
    // If |opener_rfhi| is null, the opener RFH has already disappeared.  In
    // this case, clear the opener rather than keeping the old opener around.
    if (opener_rfhi)
      opener = opener_rfhi->frame_tree_node();
  }

  if (frame_tree_node_->opener() == opener)
    return;

  frame_tree_node_->SetOpener(opener);

  for (const auto& pair : proxy_hosts_) {
    if (pair.second->GetSiteInstance() == source_site_instance)
      continue;
    pair.second->UpdateOpener();
  }

  if (render_frame_host_->GetSiteInstance() != source_site_instance)
    render_frame_host_->UpdateOpener();

  // Notify the speculative RenderFrameHosts as well.  This is necessary in case
  // a process swap has started while the message was in flight.
  if (speculative_render_frame_host_ &&
      speculative_render_frame_host_->GetSiteInstance() !=
          source_site_instance) {
    speculative_render_frame_host_->UpdateOpener();
  }
}

void RenderFrameHostManager::CommitPendingFramePolicy() {
  // Return early if there were no pending updates to sandbox flags or container
  // policy.
  if (!frame_tree_node_->CommitPendingFramePolicy())
    return;

  // Policy updates can only happen when the frame has a parent.
  CHECK(frame_tree_node_->parent());

  // There should be no children of this frame; any policy changes should only
  // happen on navigation commit.
  DCHECK(!frame_tree_node_->child_count());

  // Notify all of the frame's proxies about updated policies, excluding
  // the parent process since it already knows the latest state.
  SiteInstance* parent_site_instance =
      frame_tree_node_->parent()->current_frame_host()->GetSiteInstance();
  for (const auto& pair : proxy_hosts_) {
    if (pair.second->GetSiteInstance() != parent_site_instance) {
      pair.second->Send(new FrameMsg_DidUpdateFramePolicy(
          pair.second->GetRoutingID(),
          frame_tree_node_->current_replication_state().frame_policy));
    }
  }
}

void RenderFrameHostManager::OnDidSetFramePolicyHeaders() {
  for (const auto& pair : proxy_hosts_) {
    pair.second->Send(new FrameMsg_DidSetFramePolicyHeaders(
        pair.second->GetRoutingID(), frame_tree_node_->active_sandbox_flags(),
        frame_tree_node_->current_replication_state().feature_policy_header));
  }
}

void RenderFrameHostManager::SwapOutOldFrame(
    std::unique_ptr<RenderFrameHostImpl> old_render_frame_host) {
  TRACE_EVENT1("navigation", "RenderFrameHostManager::SwapOutOldFrame",
               "FrameTreeNode id", frame_tree_node_->frame_tree_node_id());

  // Tell the renderer to suppress any further modal dialogs so that we can swap
  // it out.  This must be done before canceling any current dialog, in case
  // there is a loop creating additional dialogs.
  old_render_frame_host->SuppressFurtherDialogs();

  // Now close any modal dialogs that would prevent us from swapping out.  This
  // must be done separately from SwapOut, so that the ScopedPageLoadDeferrer is
  // no longer on the stack when we send the SwapOut message.
  delegate_->CancelModalDialogsForRenderManager();

  // If the old RFH is not live, just return as there is no further work to do.
  // It will be deleted and there will be no proxy created.
  if (!old_render_frame_host->IsRenderFrameLive())
    return;

  // Create a replacement proxy for the old RenderFrameHost. (There should not
  // be one yet.)  This is done even if there are no active frames besides this
  // one to simplify cleanup logic on the renderer side (see
  // https://crbug.com/568836 for motivation).
  RenderFrameProxyHost* proxy =
      CreateRenderFrameProxyHost(old_render_frame_host->GetSiteInstance(),
                                 old_render_frame_host->render_view_host());

  // Reset any NavigationRequest in the RenderFrameHost. A swapped out
  // RenderFrameHost should not be trying to commit a navigation.
  old_render_frame_host->ResetNavigationRequests();

  // Tell the old RenderFrameHost to swap out and be replaced by the proxy.
  old_render_frame_host->SwapOut(proxy, true);

  // |old_render_frame_host| will be deleted when its SwapOut ACK is received,
  // or when the timer times out, or when the RFHM itself is deleted (whichever
  // comes first).
  pending_delete_hosts_.push_back(std::move(old_render_frame_host));
}

void RenderFrameHostManager::DiscardUnusedFrame(
    std::unique_ptr<RenderFrameHostImpl> render_frame_host) {
  // TODO(carlosk): this code is very similar to what can be found in
  // SwapOutOldFrame and we should see that these are unified at some point.

  // If the SiteInstance for the pending RFH is being used by others, ensure
  // that it is replaced by a RenderFrameProxyHost to allow other frames to
  // communicate to this frame.
  SiteInstanceImpl* site_instance = render_frame_host->GetSiteInstance();
  RenderViewHostImpl* rvh = render_frame_host->render_view_host();
  RenderFrameProxyHost* proxy = nullptr;
  if (site_instance->HasSite() && site_instance->active_frame_count() > 1) {
    // If a proxy already exists for the |site_instance|, just reuse it instead
    // of creating a new one. There is no need to call SwapOut on the
    // |render_frame_host|, as this method is only called to discard a pending
    // or speculative RenderFrameHost, i.e. one that has never hosted an actual
    // document.
    proxy = GetRenderFrameProxyHost(site_instance);
    if (!proxy)
      proxy = CreateRenderFrameProxyHost(site_instance, rvh);
  }

  // Doing this is important in the case where the replacement proxy is created
  // above, as the RenderViewHost will continue to exist and should be
  // considered swapped out if it is ever reused.  When there's no replacement
  // proxy, this doesn't really matter, as the RenderViewHost will be destroyed
  // shortly, since |render_frame_host| is its last active frame and will be
  // deleted below.  See https://crbug.com/627400.
  if (frame_tree_node_->IsMainFrame()) {
    rvh->set_main_frame_routing_id(MSG_ROUTING_NONE);
    rvh->SetIsActive(false);
    rvh->set_is_swapped_out(true);
  }

  render_frame_host.reset();

  // If a new RenderFrameProxyHost was created above, or if the old proxy isn't
  // live, create the RenderFrameProxy in the renderer, so that other frames
  // can still communicate with this frame.  See https://crbug.com/653746.
  if (proxy && !proxy->is_render_frame_proxy_live())
    proxy->InitRenderFrameProxy();
}

bool RenderFrameHostManager::DeleteFromPendingList(
    RenderFrameHostImpl* render_frame_host) {
  for (auto iter = pending_delete_hosts_.begin();
       iter != pending_delete_hosts_.end(); iter++) {
    if (iter->get() == render_frame_host) {
      pending_delete_hosts_.erase(iter);
      return true;
    }
  }
  return false;
}

void RenderFrameHostManager::ResetProxyHosts() {
  for (const auto& pair : proxy_hosts_) {
    static_cast<SiteInstanceImpl*>(pair.second->GetSiteInstance())
        ->RemoveObserver(this);
  }
  proxy_hosts_.clear();
}

void RenderFrameHostManager::ClearRFHsPendingShutdown() {
  pending_delete_hosts_.clear();
}

void RenderFrameHostManager::ClearWebUIInstances() {
  current_frame_host()->ClearAllWebUI();
  if (speculative_render_frame_host_)
    speculative_render_frame_host_->ClearAllWebUI();
}

void RenderFrameHostManager::DidCreateNavigationRequest(
    NavigationRequest* request) {
  RenderFrameHostImpl* dest_rfh = GetFrameHostForNavigation(*request);
  DCHECK(dest_rfh);
  request->set_associated_site_instance_type(
      dest_rfh == render_frame_host_.get()
          ? NavigationRequest::AssociatedSiteInstanceType::CURRENT
          : NavigationRequest::AssociatedSiteInstanceType::SPECULATIVE);
}

RenderFrameHostImpl* RenderFrameHostManager::GetFrameHostForNavigation(
    const NavigationRequest& request) {
  DCHECK(!request.common_params().url.SchemeIs(url::kJavaScriptScheme))
      << "Don't call this method for JavaScript URLs as those create a "
         "temporary  NavigationRequest and we don't want to reset an ongoing "
         "navigation's speculative RFH.";

  // The appropriate RenderFrameHost to commit the navigation.
  RenderFrameHostImpl* navigation_rfh = nullptr;

  // First compute the SiteInstance to use for the navigation.
  SiteInstance* current_site_instance = render_frame_host_->GetSiteInstance();
  scoped_refptr<SiteInstance> dest_site_instance =
      GetSiteInstanceForNavigationRequest(request);

  // The SiteInstance determines whether to switch RenderFrameHost or not.
  bool use_current_rfh = current_site_instance == dest_site_instance;

  bool notify_webui_of_rf_creation = false;
  if (use_current_rfh) {
    // GetFrameHostForNavigation will be called more than once during a
    // navigation (currently twice, on request and when it's about to commit in
    // the renderer). In the follow up calls an existing pending WebUI should
    // not be recreated if the URL didn't change. So instead of calling
    // CleanUpNavigation just discard the speculative RenderFrameHost if one
    // exists.
    if (speculative_render_frame_host_) {
      // If the speculative RenderFrameHost is trying to commit a navigation,
      // inform the NavigationController that the load of the corresponding
      // NavigationEntry stopped if needed. This is the case if the new
      // navigation was started from BeginNavigation. If the navigation was
      // started through the NavigationController, the NavigationController has
      // already updated its state properly, and doesn't need to be notified.
      // Note: We query all NavigationEntry IDs fom the RenderFrameHost, however
      // at most one call to DiscardPendingEntryIfNeeded will succeed. Since we
      // don't know which of the entries is the pending one, we have to try them
      // all.
      // TODO(clamy): Clean this up.
      if (request.from_begin_navigation()) {
        std::set<int> ids = speculative_render_frame_host_
                                ->GetNavigationEntryIdsPendingCommit();
        for (int id : ids)
          frame_tree_node_->navigator()->DiscardPendingEntryIfNeeded(id);
      }
      DiscardUnusedFrame(UnsetSpeculativeRenderFrameHost());
    }

    // Short-term solution: avoid creating a WebUI for subframes because
    // non-PlzNavigate code path doesn't do it and some WebUI pages don't
    // support it.
    // TODO(crbug.com/713313): Make WebUI objects always be per-frame instead.
    if (frame_tree_node_->IsMainFrame()) {
      UpdatePendingWebUIOnCurrentFrameHost(request.common_params().url,
                                           request.bindings());
    }

    navigation_rfh = render_frame_host_.get();

    DCHECK(!speculative_render_frame_host_);
  } else {
    // If the current RenderFrameHost cannot be used a speculative one is
    // created with the SiteInstance for the current URL. If a speculative
    // RenderFrameHost already exists we try as much as possible to reuse it and
    // its associated WebUI.

    // Check if an existing speculative RenderFrameHost can be reused.
    if (!speculative_render_frame_host_ ||
        speculative_render_frame_host_->GetSiteInstance() !=
            dest_site_instance.get()) {
      // If there is a speculative RenderFrameHost trying to commit a
      // navigation, inform the NavigationController that the load of the
      // corresponding NavigationEntry stopped if needed. This is the case if
      // the new navigation was started from BeginNavigation. If the navigation
      // was started through the NavigationController, the NavigationController
      // has already updated its state properly, and doesn't need to be
      // notified.
      // Note: We query all NavigationEntry IDs fom the RenderFrameHost, however
      // at most one call to DiscardPendingEntryIfNeeded will succeed. Since we
      // don't know which of the entries is the pending one, we have to try them
      // all.
      // TODO(clamy): Clean this up.
      if (request.from_begin_navigation() && speculative_render_frame_host_) {
        std::set<int> ids = speculative_render_frame_host_
                                ->GetNavigationEntryIdsPendingCommit();
        for (int id : ids)
          frame_tree_node_->navigator()->DiscardPendingEntryIfNeeded(id);
      }

      // If a previous speculative RenderFrameHost didn't exist or if its
      // SiteInstance differs from the one for the current URL, a new one needs
      // to be created.
      CleanUpNavigation();
      bool success = CreateSpeculativeRenderFrameHost(current_site_instance,
                                                      dest_site_instance.get());
      DCHECK(success);
    }
    DCHECK(speculative_render_frame_host_);

    // Short-term solution: avoid creating a WebUI for subframes because
    // non-PlzNavigate code path doesn't do it and some WebUI pages don't
    // support it.
    // TODO(crbug.com/713313): Make WebUI objects always be per-frame instead.
    if (frame_tree_node_->IsMainFrame()) {
      bool changed_web_ui = speculative_render_frame_host_->UpdatePendingWebUI(
          request.common_params().url, request.bindings());
      speculative_render_frame_host_->CommitPendingWebUI();
      DCHECK_EQ(GetNavigatingWebUI(), speculative_render_frame_host_->web_ui());
      notify_webui_of_rf_creation =
          changed_web_ui && speculative_render_frame_host_->web_ui();
    }
    navigation_rfh = speculative_render_frame_host_.get();

    // Check if our current RFH is live.
    if (!render_frame_host_->IsRenderFrameLive()) {
      // The current RFH is not live.  There's no reason to sit around with a
      // sad tab or a newly created RFH while we wait for the navigation to
      // complete. Just switch to the speculative RFH now and go back to normal.
      // (Note that we don't care about on{before}unload handlers if the current
      // RFH isn't live.)
      //
      // If the corresponding RenderFrame is currently associated with a proxy,
      // send a SwapIn message to ensure that the RenderFrame swaps into the
      // frame tree and replaces that proxy on the renderer side.  Normally
      // this happens at navigation commit time, but in this case this must be
      // done earlier to keep browser and renderer state in sync.  This is
      // important to do before CommitPending(), which destroys the
      // corresponding proxy. See https://crbug.com/487872.
      if (GetRenderFrameProxyHost(dest_site_instance.get())) {
        navigation_rfh->Send(
            new FrameMsg_SwapIn(navigation_rfh->GetRoutingID()));
      }
      CommitPending();

      // Notify the WebUI about the new RenderFrame if needed (the newly
      // created WebUI has just been committed by CommitPending, so
      // GetNavigatingWebUI() below will return false).
      if (notify_webui_of_rf_creation && render_frame_host_->web_ui()) {
        render_frame_host_->web_ui()->RenderFrameCreated(
            render_frame_host_.get());
        notify_webui_of_rf_creation = false;
      }
    }
  }
  DCHECK(navigation_rfh &&
         (navigation_rfh == render_frame_host_.get() ||
          navigation_rfh == speculative_render_frame_host_.get()));

  // If the RenderFrame that needs to navigate is not live (its process was just
  // created or has crashed), initialize it.
  if (!navigation_rfh->IsRenderFrameLive()) {
    if (!ReinitializeRenderFrame(navigation_rfh))
      return nullptr;

    notify_webui_of_rf_creation = true;

    if (navigation_rfh == render_frame_host_.get()) {
      EnsureRenderFrameHostVisibilityConsistent();
      EnsureRenderFrameHostPageFocusConsistent();
      // TODO(nasko): This is a very ugly hack. The Chrome extensions process
      // manager still uses NotificationService and expects to see a
      // RenderViewHost changed notification after WebContents and
      // RenderFrameHostManager are completely initialized. This should be
      // removed once the process manager moves away from NotificationService.
      // See https://crbug.com/462682.
      if (frame_tree_node_->IsMainFrame()) {
        delegate_->NotifyMainFrameSwappedFromRenderManager(
            nullptr, render_frame_host_.get());
      }
    }
  }

  // If a WebUI was created in a speculative RenderFrameHost or a new
  // RenderFrame was created then the WebUI never interacted with the
  // RenderFrame or its RenderView. Notify using RenderFrameCreated.
  //
  // Short-term solution: avoid creating a WebUI for subframes because
  // non-PlzNavigate code path doesn't do it and some WebUI pages don't
  // support it.
  // TODO(crbug.com/713313): Make WebUI objects always be per-frame instead.
  if (notify_webui_of_rf_creation && GetNavigatingWebUI() &&
      frame_tree_node_->IsMainFrame()) {
    GetNavigatingWebUI()->RenderFrameCreated(navigation_rfh);
  }

  return navigation_rfh;
}

void RenderFrameHostManager::CleanUpNavigation() {
  if (speculative_render_frame_host_) {
    bool was_loading = speculative_render_frame_host_->is_loading();
    DiscardUnusedFrame(UnsetSpeculativeRenderFrameHost());
    if (was_loading)
      frame_tree_node_->DidStopLoading();
  }
}

std::unique_ptr<RenderFrameHostImpl>
RenderFrameHostManager::UnsetSpeculativeRenderFrameHost() {
  speculative_render_frame_host_->GetProcess()->RemovePendingView();
  return std::move(speculative_render_frame_host_);
}

void RenderFrameHostManager::OnDidStartLoading() {
  for (const auto& pair : proxy_hosts_) {
    pair.second->Send(
        new FrameMsg_DidStartLoading(pair.second->GetRoutingID()));
  }
}

void RenderFrameHostManager::OnDidStopLoading() {
  for (const auto& pair : proxy_hosts_) {
    pair.second->Send(new FrameMsg_DidStopLoading(pair.second->GetRoutingID()));
  }
}

void RenderFrameHostManager::OnDidUpdateName(const std::string& name,
                                             const std::string& unique_name) {
  for (const auto& pair : proxy_hosts_) {
    pair.second->Send(new FrameMsg_DidUpdateName(pair.second->GetRoutingID(),
                                                 name, unique_name));
  }
}

void RenderFrameHostManager::OnDidAddContentSecurityPolicies(
    const std::vector<ContentSecurityPolicyHeader>& headers) {
  for (const auto& pair : proxy_hosts_) {
    pair.second->Send(new FrameMsg_AddContentSecurityPolicies(
        pair.second->GetRoutingID(), headers));
  }
}

void RenderFrameHostManager::OnDidResetContentSecurityPolicy() {
  for (const auto& pair : proxy_hosts_) {
    pair.second->Send(
        new FrameMsg_ResetContentSecurityPolicy(pair.second->GetRoutingID()));
  }
}

void RenderFrameHostManager::OnEnforceInsecureRequestPolicy(
    blink::WebInsecureRequestPolicy policy) {
  for (const auto& pair : proxy_hosts_) {
    pair.second->Send(new FrameMsg_EnforceInsecureRequestPolicy(
        pair.second->GetRoutingID(), policy));
  }
}

void RenderFrameHostManager::OnEnforceInsecureNavigationsSet(
    const std::vector<uint32_t>& insecure_navigations_set) {
  for (const auto& pair : proxy_hosts_) {
    pair.second->Send(new FrameMsg_EnforceInsecureNavigationsSet(
        pair.second->GetRoutingID(), insecure_navigations_set));
  }
}

void RenderFrameHostManager::OnDidChangeCollapsedState(bool collapsed) {
  DCHECK(frame_tree_node_->parent());
  SiteInstance* parent_site_instance =
      frame_tree_node_->parent()->current_frame_host()->GetSiteInstance();

  // There will be no proxy to represent the pending or speculative RFHs in the
  // parent's SiteInstance until the navigation is committed, but the old RFH is
  // not swapped out before that happens either, so we can talk to the
  // FrameOwner in the parent via the child's current RenderFrame at any time.
  DCHECK(current_frame_host());
  if (current_frame_host()->GetSiteInstance() == parent_site_instance) {
    current_frame_host()->Send(
        new FrameMsg_Collapse(current_frame_host()->GetRoutingID(), collapsed));
  } else {
    RenderFrameProxyHost* proxy_to_parent =
        GetRenderFrameProxyHost(parent_site_instance);
    proxy_to_parent->Send(
        new FrameMsg_Collapse(proxy_to_parent->GetRoutingID(), collapsed));
  }
}

void RenderFrameHostManager::OnDidUpdateFrameOwnerProperties(
    const FrameOwnerProperties& properties) {
  // FrameOwnerProperties exist only for frames that have a parent.
  CHECK(frame_tree_node_->parent());
  SiteInstance* parent_instance =
      frame_tree_node_->parent()->current_frame_host()->GetSiteInstance();

  // Notify the RenderFrame if it lives in a different process from its parent.
  if (render_frame_host_->GetSiteInstance() != parent_instance) {
    render_frame_host_->Send(new FrameMsg_SetFrameOwnerProperties(
        render_frame_host_->GetRoutingID(), properties));
  }

  // Notify this frame's proxies if they live in a different process from its
  // parent.  This is only currently needed for the allowFullscreen property,
  // since that can be queried on RemoteFrame ancestors.
  //
  // TODO(alexmos): It would be sufficient to only send this update to proxies
  // in the current FrameTree.
  for (const auto& pair : proxy_hosts_) {
    if (pair.second->GetSiteInstance() != parent_instance) {
      pair.second->Send(new FrameMsg_SetFrameOwnerProperties(
          pair.second->GetRoutingID(), properties));
    }
  }
}

void RenderFrameHostManager::OnDidUpdateOrigin(
    const url::Origin& origin,
    bool is_potentially_trustworthy_unique_origin) {
  for (const auto& pair : proxy_hosts_) {
    pair.second->Send(
        new FrameMsg_DidUpdateOrigin(pair.second->GetRoutingID(), origin,
                                     is_potentially_trustworthy_unique_origin));
  }
}

RenderFrameHostManager::SiteInstanceDescriptor::SiteInstanceDescriptor(
    BrowserContext* browser_context,
    GURL dest_url,
    SiteInstanceRelation relation_to_current)
    : existing_site_instance(nullptr),
      dest_url(dest_url),
      browser_context(browser_context),
      relation(relation_to_current) {}

void RenderFrameHostManager::RenderProcessGone(SiteInstanceImpl* instance) {
  GetRenderFrameProxyHost(instance)->set_render_frame_proxy_created(false);
}

void RenderFrameHostManager::CancelPendingIfNecessary(
    RenderFrameHostImpl* render_frame_host) {
  if (render_frame_host == speculative_render_frame_host_.get()) {
    // TODO(nasko, clamy): This should just clean up the speculative RFH
    // without canceling the request.  See https://crbug.com/636119.
    if (frame_tree_node_->navigation_request() &&
        frame_tree_node_->navigation_request()->navigation_handle()) {
      frame_tree_node_->navigation_request()
          ->navigation_handle()
          ->set_net_error_code(net::ERR_ABORTED);
    }
    frame_tree_node_->ResetNavigationRequest(false, true);
  }
}

void RenderFrameHostManager::UpdateUserActivationState(
    blink::UserActivationUpdateType update_type) {
  for (const auto& pair : proxy_hosts_) {
    pair.second->Send(new FrameMsg_UpdateUserActivationState(
        pair.second->GetRoutingID(), update_type));
  }
}

void RenderFrameHostManager::OnSetHasReceivedUserGestureBeforeNavigation(
    bool value) {
  for (const auto& pair : proxy_hosts_) {
    pair.second->Send(new FrameMsg_SetHasReceivedUserGestureBeforeNavigation(
        pair.second->GetRoutingID(), value));
  }
}

void RenderFrameHostManager::ActiveFrameCountIsZero(
    SiteInstanceImpl* site_instance) {
  // |site_instance| no longer contains any active RenderFrameHosts, so we don't
  // need to maintain a proxy there anymore.
  RenderFrameProxyHost* proxy = GetRenderFrameProxyHost(site_instance);
  CHECK(proxy);

  DeleteRenderFrameProxyHost(site_instance);
}

RenderFrameProxyHost* RenderFrameHostManager::CreateRenderFrameProxyHost(
    SiteInstance* site_instance,
    RenderViewHostImpl* rvh) {
  int site_instance_id = site_instance->GetId();
  CHECK(proxy_hosts_.find(site_instance_id) == proxy_hosts_.end())
      << "A proxy already existed for this SiteInstance.";
  RenderFrameProxyHost* proxy_host =
      new RenderFrameProxyHost(site_instance, rvh, frame_tree_node_);
  proxy_hosts_[site_instance_id] = base::WrapUnique(proxy_host);
  static_cast<SiteInstanceImpl*>(site_instance)->AddObserver(this);
  return proxy_host;
}

void RenderFrameHostManager::DeleteRenderFrameProxyHost(
    SiteInstance* site_instance) {
  static_cast<SiteInstanceImpl*>(site_instance)->RemoveObserver(this);
  proxy_hosts_.erase(site_instance->GetId());
}

bool RenderFrameHostManager::ShouldTransitionCrossSite() {
  // False in single-process mode, which does not support cross-process
  // navigations or OOPIFs.
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSingleProcess);
}

bool RenderFrameHostManager::ShouldSwapBrowsingInstancesForNavigation(
    const GURL& current_effective_url,
    bool current_is_view_source_mode,
    SiteInstance* new_site_instance,
    const GURL& new_effective_url,
    bool new_is_view_source_mode,
    bool is_failure) const {
  // A subframe must stay in the same BrowsingInstance as its parent.
  // TODO(nasko): Ensure that SiteInstance swap is still triggered for subframes
  // in the cases covered by the rest of the checks in this method.
  if (!frame_tree_node_->IsMainFrame())
    return false;

  // If the navigation has resulted in an error page, do not swap
  // BrowsingInstance and keep the error page in a related SiteInstance. If
  // later a reload of this navigation is successful, it  will correctly
  // create a new BrowsingInstance.
  if (is_failure && SiteIsolationPolicy::IsErrorPageIsolationEnabled(
                        frame_tree_node_->IsMainFrame())) {
    return false;
  }

  // If new_entry already has a SiteInstance, assume it is correct.  We only
  // need to force a swap if it is in a different BrowsingInstance.
  if (new_site_instance) {
    return !new_site_instance->IsRelatedSiteInstance(
        render_frame_host_->GetSiteInstance());
  }

  // Check for reasons to swap processes even if we are in a process model that
  // doesn't usually swap (e.g., process-per-tab).  Any time we return true,
  // the new_entry will be rendered in a new SiteInstance AND BrowsingInstance.
  BrowserContext* browser_context =
      delegate_->GetControllerForRenderManager().GetBrowserContext();

  // Don't force a new BrowsingInstance for debug URLs that are handled in the
  // renderer process, like javascript: or chrome://crash.
  if (IsRendererDebugURL(new_effective_url))
    return false;

  // Transitions across BrowserContexts should always require a
  // BrowsingInstance swap. For example, this can happen if an extension in a
  // normal profile opens an incognito window with a web URL using
  // chrome.windows.create().
  //
  // TODO(alexmos): This check should've been enforced earlier in the
  // navigation, in chrome::Navigate().  Verify this, and then convert this to
  // a CHECK and remove the fallback.
  DCHECK_EQ(browser_context,
            render_frame_host_->GetSiteInstance()->GetBrowserContext());
  if (browser_context !=
      render_frame_host_->GetSiteInstance()->GetBrowserContext()) {
    return true;
  }

  // For security, we should transition between processes when one is a Web UI
  // page and one isn't, or if the WebUI types differ.
  if (ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          render_frame_host_->GetProcess()->GetID()) ||
      WebUIControllerFactoryRegistry::GetInstance()->UseWebUIBindingsForURL(
          browser_context, current_effective_url)) {
    // If so, force a swap if destination is not an acceptable URL for Web UI.
    // Here, data URLs are never allowed.
    if (!WebUIControllerFactoryRegistry::GetInstance()->IsURLAcceptableForWebUI(
            browser_context, new_effective_url)) {
      return true;
    }

    // Force swap if the current WebUI type differs from the one for the
    // destination.
    if (WebUIControllerFactoryRegistry::GetInstance()->GetWebUIType(
            browser_context, current_effective_url) !=
        WebUIControllerFactoryRegistry::GetInstance()->GetWebUIType(
            browser_context, new_effective_url)) {
      return true;
    }
  } else {
    // Force a swap if it's a Web UI URL.
    if (WebUIControllerFactoryRegistry::GetInstance()->UseWebUIBindingsForURL(
            browser_context, new_effective_url)) {
      return true;
    }
  }

  // Check with the content client as well.  Important to pass
  // current_effective_url here, which uses the SiteInstance's site if there is
  // no current_entry.
  if (GetContentClient()->browser()->ShouldSwapBrowsingInstancesForNavigation(
          render_frame_host_->GetSiteInstance(),
          current_effective_url, new_effective_url)) {
    return true;
  }

  // We can't switch a RenderView between view source and non-view source mode
  // without screwing up the session history sometimes (when navigating between
  // "view-source:http://foo.com/" and "http://foo.com/", Blink doesn't treat
  // it as a new navigation). So require a BrowsingInstance switch.
  if (current_is_view_source_mode != new_is_view_source_mode)
    return true;

  return false;
}

scoped_refptr<SiteInstance>
RenderFrameHostManager::GetSiteInstanceForNavigation(
    const GURL& dest_url,
    SiteInstance* source_instance,
    SiteInstance* dest_instance,
    SiteInstance* candidate_instance,
    ui::PageTransition transition,
    bool is_failure,
    bool dest_is_restore,
    bool dest_is_view_source_mode,
    bool was_server_redirect) {
  // On renderer-initiated navigations, when the frame initiating the navigation
  // and the frame being navigated differ, |source_instance| is set to the
  // SiteInstance of the initiating frame. |dest_instance| is present on session
  // history navigations. The two cannot be set simultaneously.
  DCHECK(!source_instance || !dest_instance);

  SiteInstance* current_instance = render_frame_host_->GetSiteInstance();

  // We do not currently swap processes for navigations in webview tag guests.
  if (current_instance->GetSiteURL().SchemeIs(kGuestScheme))
    return current_instance;

  // Determine if we need a new BrowsingInstance for this entry.  If true, this
  // implies that it will get a new SiteInstance (and likely process), and that
  // other tabs in the current BrowsingInstance will be unable to script it.
  // This is used for cases that require a process swap even in the
  // process-per-tab model, such as WebUI pages.

  // First determine the effective URL of the current RenderFrameHost. This is
  // the last URL it successfully committed. If it has yet to commit a URL, this
  // falls back to the Site URL of its SiteInstance.
  // Note: the effective URL of the current RenderFrameHost may differ from the
  // URL of the last committed NavigationEntry, which cannot be used to decide
  // whether to use a new SiteInstance. This happens when navigating a subframe,
  // or when a new RenderFrameHost has been swapped in at the beginning of a
  // navigation to replace a crashed RenderFrameHost.
  BrowserContext* browser_context =
      delegate_->GetControllerForRenderManager().GetBrowserContext();
  const GURL& current_effective_url =
      !render_frame_host_->last_successful_url().is_empty()
          ? SiteInstanceImpl::GetEffectiveURL(
                browser_context, render_frame_host_->last_successful_url())
          : render_frame_host_->GetSiteInstance()->GetSiteURL();

  // Determine if the current RenderFrameHost is in view source mode.
  // TODO(clamy): If the current_effective_url doesn't match the last committed
  // NavigationEntry's URL, current_is_view_source_mode should not be computed
  // using the NavigationEntry. This can happen when a tab crashed, and a new
  // RenderFrameHost was swapped in at the beginning of the navigation. See
  // https://crbug.com/766630.
  const NavigationEntry* current_entry =
      delegate_->GetLastCommittedNavigationEntryForRenderManager();
  bool current_is_view_source_mode = current_entry ?
      current_entry->IsViewSourceMode() : dest_is_view_source_mode;

  bool force_swap = ShouldSwapBrowsingInstancesForNavigation(
      current_effective_url, current_is_view_source_mode, dest_instance,
      SiteInstanceImpl::GetEffectiveURL(browser_context, dest_url),
      dest_is_view_source_mode, is_failure);
  SiteInstanceDescriptor new_instance_descriptor =
      SiteInstanceDescriptor(current_instance);
  if (ShouldTransitionCrossSite() || force_swap) {
    new_instance_descriptor = DetermineSiteInstanceForURL(
        dest_url, source_instance, current_instance, dest_instance, transition,
        is_failure, dest_is_restore, dest_is_view_source_mode, force_swap,
        was_server_redirect);
  }

  scoped_refptr<SiteInstance> new_instance =
      ConvertToSiteInstance(new_instance_descriptor, candidate_instance);
  // If |force_swap| is true, we must use a different SiteInstance than the
  // current one. If we didn't, we would have two RenderFrameHosts in the same
  // SiteInstance and the same frame, breaking lookup of RenderFrameHosts by
  // SiteInstance.
  if (force_swap)
    CHECK_NE(new_instance, current_instance);

  if (new_instance == current_instance) {
    // If we're navigating to the same site instance, we won't need to use the
    // current spare RenderProcessHost.
    RenderProcessHostImpl::NotifySpareManagerAboutRecentlyUsedBrowserContext(
        browser_context);
  }

  // Double-check that the new SiteInstance is associated with the right
  // BrowserContext.
  DCHECK_EQ(new_instance->GetBrowserContext(), browser_context);

  // If |new_instance| is a new SiteInstance for a subframe that requires a
  // dedicated process, set its process reuse policy so that such subframes are
  // consolidated into existing processes for that site.
  SiteInstanceImpl* new_instance_impl =
      static_cast<SiteInstanceImpl*>(new_instance.get());
  if (!frame_tree_node_->IsMainFrame() && !new_instance_impl->HasProcess() &&
      new_instance_impl->RequiresDedicatedProcess()) {
    new_instance_impl->set_process_reuse_policy(
        SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  }

  return new_instance;
}

void RenderFrameHostManager::InitializeRenderFrameIfNecessary(
    RenderFrameHostImpl* render_frame_host) {
  // TODO(jam): this copies some logic inside GetFrameHostForNavigation, which
  // also duplicates logic in Navigate. They should all use this method, but
  // that involves slight reordering.
  // http://crbug.com/794229
  if (render_frame_host->IsRenderFrameLive())
    return;

  if (!ReinitializeRenderFrame(render_frame_host))
    return;

  if (render_frame_host != render_frame_host_.get())
    return;

  EnsureRenderFrameHostVisibilityConsistent();

  // TODO(jam): uncomment this when the method is shared. Not adding the call
  // now to make merge to 63 easier.
  // EnsureRenderFrameHostPageFocusConsistent();

  // TODO(nasko): This is a very ugly hack. The Chrome extensions process
  // manager still uses NotificationService and expects to see a
  // RenderViewHost changed notification after WebContents and
  // RenderFrameHostManager are completely initialized. This should be
  // removed once the process manager moves away from NotificationService.
  // See https://crbug.com/462682.
  if (frame_tree_node_->IsMainFrame()) {
    delegate_->NotifyMainFrameSwappedFromRenderManager(
        nullptr, render_frame_host_.get());
  }
}

RenderFrameHostManager::SiteInstanceDescriptor
RenderFrameHostManager::DetermineSiteInstanceForURL(
    const GURL& dest_url,
    SiteInstance* source_instance,
    SiteInstance* current_instance,
    SiteInstance* dest_instance,
    ui::PageTransition transition,
    bool is_failure,
    bool dest_is_restore,
    bool dest_is_view_source_mode,
    bool force_browsing_instance_swap,
    bool was_server_redirect) {
  SiteInstanceImpl* current_instance_impl =
      static_cast<SiteInstanceImpl*>(current_instance);
  NavigationControllerImpl& controller =
      delegate_->GetControllerForRenderManager();
  BrowserContext* browser_context = controller.GetBrowserContext();

  // If the entry has an instance already we should use it, unless it is no
  // longer suitable.
  if (dest_instance) {
    // When error page isolation is enabled, don't reuse |dest_instance| if it's
    // an error page SiteInstance, but the navigation will no longer fail.
    // Similarly, don't reuse |dest_instance| if it's not an error page
    // SiteInstance but the navigation will fail and actually need an error page
    // SiteInstance.
    // Note: The later call to HasWrongProcessForURL does not have context about
    // error page navigaions, so we cannot rely on it to return correct value
    // when error pages are involved.
    if (!SiteIsolationPolicy::IsErrorPageIsolationEnabled(
            frame_tree_node_->IsMainFrame()) ||
        ((dest_instance->GetSiteURL() == GURL(kUnreachableWebDataURL)) ==
         is_failure)) {
      // TODO(nasko,creis): The check whether data: or about: URLs are allowed
      // to commit in the current process should be in HasWrongProcessForURL.
      // However, making this change has further implications and needs more
      // investigation of what behavior changes. For now, use a conservative
      // approach and explicitly check before calling HasWrongProcessForURL.
      SiteInstanceImpl* dest_instance_impl =
          static_cast<SiteInstanceImpl*>(dest_instance);
      if (IsDataOrAbout(dest_url) ||
          !dest_instance_impl->HasWrongProcessForURL(dest_url)) {
        // If we are forcing a swap, this should be in a different
        // BrowsingInstance.
        if (force_browsing_instance_swap) {
          CHECK(!dest_instance->IsRelatedSiteInstance(
              render_frame_host_->GetSiteInstance()));
        }
        return SiteInstanceDescriptor(dest_instance);
      }
    }
  }

  // If a swap is required, we need to force the SiteInstance AND
  // BrowsingInstance to be different ones, using CreateForURL.
  if (force_browsing_instance_swap)
    return SiteInstanceDescriptor(browser_context, dest_url,
                                  SiteInstanceRelation::UNRELATED);

  // If error page navigations should be isolated, ensure a dedicated
  // SiteInstance is used for them.
  if (is_failure && SiteIsolationPolicy::IsErrorPageIsolationEnabled(
                        frame_tree_node_->IsMainFrame())) {
    // Keep the error page in the same BrowsingInstance, such that in the case
    // of transient network errors, a subsequent successful load of the same
    // document will not result in broken scripting relationships between
    // windows.
    return SiteInstanceDescriptor(browser_context, GURL(kUnreachableWebDataURL),
                                  SiteInstanceRelation::RELATED);
  }

  if (!frame_tree_node_->IsMainFrame()) {
    SiteInstance* parent_site_instance =
        frame_tree_node_->parent()->current_frame_host()->GetSiteInstance();
    // TEMPORARY HACK: Don't create OOPIFs on the NTP.  Remove this when the NTP
    // supports OOPIFs or is otherwise omitted from site isolation policy.
    // See https://crbug.com/566091.
    if (GetContentClient()->browser()->ShouldStayInParentProcessForNTP(
            dest_url, parent_site_instance)) {
      return SiteInstanceDescriptor(parent_site_instance);
    }
  }

  // If we haven't used our SiteInstance (and thus RVH) yet, then we can use it
  // for this entry.  We won't commit the SiteInstance to this site until the
  // navigation commits (in DidNavigate), unless the navigation entry was
  // restored or it's a Web UI as described below.
  if (!current_instance_impl->HasSite()) {
    // If we've already created a SiteInstance for our destination, we don't
    // want to use this unused SiteInstance; use the existing one.  (We don't
    // do this check if the current_instance has a site, because for now, we
    // want to compare against the current URL and not the SiteInstance's site.
    // In this case, there is no current URL, so comparing against the site is
    // ok.  See additional comments below.)
    //
    // Also, if the URL should use process-per-site mode and there is an
    // existing process for the site, we should use it.  We can call
    // GetRelatedSiteInstance() for this, which will eagerly set the site and
    // thus use the correct process.
    bool use_process_per_site =
        RenderProcessHost::ShouldUseProcessPerSite(browser_context, dest_url) &&
        RenderProcessHostImpl::GetSoleProcessHostForURL(browser_context,
                                                        dest_url);
    if (current_instance_impl->HasRelatedSiteInstance(dest_url) ||
        use_process_per_site) {
      return SiteInstanceDescriptor(browser_context, dest_url,
                                    SiteInstanceRelation::RELATED);
    }

    // For extensions, Web UI URLs (such as the new tab page), and apps we do
    // not want to use the |current_instance_impl| if it has no site, since it
    // will have a RenderProcessHost of PRIV_NORMAL. Create a new SiteInstance
    // for this URL instead (with the correct process type).
    if (current_instance_impl->HasWrongProcessForURL(dest_url))
      return SiteInstanceDescriptor(browser_context, dest_url,
                                    SiteInstanceRelation::RELATED);

    // View-source URLs must use a new SiteInstance and BrowsingInstance.
    // TODO(nasko): This is the same condition as later in the function. This
    // should be taken into account when refactoring this method as part of
    // http://crbug.com/123007.
    if (dest_is_view_source_mode)
      return SiteInstanceDescriptor(browser_context, dest_url,
                                    SiteInstanceRelation::UNRELATED);

    // If we are navigating from a blank SiteInstance to a WebUI, make sure we
    // create a new SiteInstance.
    if (WebUIControllerFactoryRegistry::GetInstance()->UseWebUIForURL(
            browser_context, dest_url)) {
      return SiteInstanceDescriptor(browser_context, dest_url,
                                    SiteInstanceRelation::UNRELATED);
    }

    // Normally the "site" on the SiteInstance is set lazily when the load
    // actually commits. This is to support better process sharing in case
    // the site redirects to some other site: we want to use the destination
    // site in the site instance.
    //
    // In the case of session restore, as it loads all the pages immediately
    // we need to set the site first, otherwise after a restore none of the
    // pages would share renderers in process-per-site.
    //
    // The embedder can request some urls never to be assigned to SiteInstance
    // through the ShouldAssignSiteForURL() content client method, so that
    // renderers created for particular chrome urls (e.g. the chrome-native://
    // scheme) can be reused for subsequent navigations in the same WebContents.
    // See http://crbug.com/386542.
    if (dest_is_restore && SiteInstanceImpl::ShouldAssignSiteForURL(dest_url))
      current_instance_impl->SetSite(dest_url);

    return SiteInstanceDescriptor(current_instance_impl);
  }

  // Otherwise, only create a new SiteInstance for a cross-process navigation.

  // TODO(creis): Once we intercept links and script-based navigations, we
  // will be able to enforce that all entries in a SiteInstance actually have
  // the same site, and it will be safe to compare the URL against the
  // SiteInstance's site, as follows:
  // const GURL& current_url = current_instance_impl->site();
  // For now, though, we're in a hybrid model where you only switch
  // SiteInstances if you type in a cross-site URL.  This means we have to
  // compare the entry's URL to the last committed entry's URL.
  NavigationEntry* current_entry = controller.GetLastCommittedEntry();
  if (delegate_->GetInterstitialForRenderManager()) {
    // The interstitial is currently the last committed entry, but we want to
    // compare against the last non-interstitial entry.
    current_entry = controller.GetEntryAtOffset(-1);
  }

  // View-source URLs must use a new SiteInstance and BrowsingInstance.
  // We don't need a swap when going from view-source to a debug URL like
  // chrome://crash, however.
  // TODO(creis): Refactor this method so this duplicated code isn't needed.
  // See http://crbug.com/123007.
  if (current_entry &&
      current_entry->IsViewSourceMode() != dest_is_view_source_mode &&
      !IsRendererDebugURL(dest_url)) {
    return SiteInstanceDescriptor(browser_context, dest_url,
                                  SiteInstanceRelation::UNRELATED);
  }

  // Use the source SiteInstance in case of data URLs, about:srcdoc pages and
  // about:blank pages because the content is then controlled and/or scriptable
  // by the source SiteInstance.
  //
  // One exception to this is when these URLs are
  // reached via a server redirect.  Normally, redirects to data: or about:
  // URLs are disallowed as net::ERR_UNSAFE_REDIRECT, but extensions can still
  // redirect arbitary requests to those URLs using webRequest or
  // declarativeWebRequest API.  For these cases, the content isn't controlled
  // by the source SiteInstance, so it need not use it.
  if (source_instance && IsDataOrAbout(dest_url) && !was_server_redirect)
    return SiteInstanceDescriptor(source_instance);

  // Use the current SiteInstance for same site navigations.
  if (IsCurrentlySameSite(render_frame_host_.get(), dest_url))
    return SiteInstanceDescriptor(render_frame_host_->GetSiteInstance());

  // At this point, |dest_url| corresponds to a cross-site navigation.  See if
  // we can swap BrowsingInstances to avoid unneeded process sharing.  This is
  // done for certain main frame browser-initiated navigations. See
  // https://crbug.com/803367.
  if (IsBrowsingInstanceSwapAllowedForPageTransition(transition, dest_url)) {
    return SiteInstanceDescriptor(browser_context, dest_url,
                                  SiteInstanceRelation::UNRELATED);
  }

  // Shortcut some common cases for reusing an existing frame's SiteInstance.
  // There are several reasons for this:
  // - looking at the main frame and openers is required for TDI mode.
  // - with hosted apps, this allows same-site, non-app subframes to be kept
  //   inside the hosted app process.
  // - this avoids putting same-site iframes into different processes after
  //   navigations from isolated origins.  This matters for some OAuth flows;
  //   see https://crbug.com/796912.
  //
  // TODO(alexmos): Ideally, the right SiteInstance for these cases should be
  // found later, as part of creating a new related SiteInstance from
  // BrowsingInstance::GetSiteInstanceForURL().  However, the lookup there (1)
  // does not properly deal with hosted apps (see https://crbug.com/718516),
  // and (2) does not yet deal with cases where a SiteInstance is shared by
  // several sites that don't require a dedicated process (see
  // https://crbug.com/787576).
  if (!frame_tree_node_->IsMainFrame()) {
    RenderFrameHostImpl* main_frame =
        frame_tree_node_->frame_tree()->root()->current_frame_host();
    if (IsCurrentlySameSite(main_frame, dest_url))
      return SiteInstanceDescriptor(main_frame->GetSiteInstance());
    RenderFrameHostImpl* parent =
        frame_tree_node_->parent()->current_frame_host();
    if (IsCurrentlySameSite(parent, dest_url))
      return SiteInstanceDescriptor(parent->GetSiteInstance());
  }
  if (frame_tree_node_->opener()) {
    RenderFrameHostImpl* opener_frame =
        frame_tree_node_->opener()->current_frame_host();
    if (IsCurrentlySameSite(opener_frame, dest_url))
      return SiteInstanceDescriptor(opener_frame->GetSiteInstance());
  }

  // Keep subframes in the parent's SiteInstance unless a dedicated process is
  // required for either the parent or the subframe's destination URL.  This
  // isn't a strict invariant but rather a heuristic to avoid unnecessary
  // OOPIFs; see https://crbug.com/711006.
  //
  // TODO(alexmos): Remove this check after fixing https://crbug.com/787576.
  if (!frame_tree_node_->IsMainFrame()) {
    RenderFrameHostImpl* parent =
        frame_tree_node_->parent()->current_frame_host();
    bool dest_url_requires_dedicated_process =
        SiteInstanceImpl::DoesSiteRequireDedicatedProcess(browser_context,
                                                          dest_url);
    if (!parent->GetSiteInstance()->RequiresDedicatedProcess() &&
        !dest_url_requires_dedicated_process) {
      return SiteInstanceDescriptor(parent->GetSiteInstance());
    }
  }

  // Start the new renderer in a new SiteInstance, but in the current
  // BrowsingInstance.
  return SiteInstanceDescriptor(browser_context, dest_url,
                                SiteInstanceRelation::RELATED);
}

bool RenderFrameHostManager::IsBrowsingInstanceSwapAllowedForPageTransition(
    ui::PageTransition transition,
    const GURL& dest_url) {
  // Disallow BrowsingInstance swaps for subframes.
  if (!frame_tree_node_->IsMainFrame())
    return false;

  // Skip data: and file: URLs, as some tests rely on browser-initiated
  // navigations to those URLs to stay in the same process.  Swapping
  // BrowsingInstances for those URLs may not carry much benefit anyway, since
  // they're likely less common.
  //
  // Note that such URLs are not considered same-site, but since their
  // SiteInstance site URL is based only on scheme (e.g., all data URLs use a
  // site URL of "data:"), a browser-initiated navigation from one such URL to
  // another will still stay in the same SiteInstance, due to the matching site
  // URL.
  if (dest_url.SchemeIsFile() || dest_url.SchemeIs(url::kDataScheme))
    return false;

  // Allow page transitions corresponding to certain browser-initiated
  // navigations: typing in the URL, using a bookmark, or using search.
  switch (ui::PageTransitionStripQualifier(transition)) {
    case ui::PAGE_TRANSITION_TYPED:
    case ui::PAGE_TRANSITION_AUTO_BOOKMARK:
    case ui::PAGE_TRANSITION_GENERATED:
    case ui::PAGE_TRANSITION_KEYWORD:
      return true;
    // TODO(alexmos): PAGE_TRANSITION_AUTO_TOPLEVEL is not included due to a
    // bug that would cause unneeded BrowsingInstance swaps for DevTools,
    // https://crbug.com/733767.  Once that bug is fixed, consider adding this
    // transition here.
    default:
      return false;
  }
}

bool RenderFrameHostManager::IsRendererTransferNeededForNavigation(
    RenderFrameHostImpl* rfh,
    const GURL& dest_url) {
  // Always attempt a process transfer if the SiteInstance has a process that's
  // unsuitable for |dest_url|.  For example, this might happen when reloading
  // a URL for which a hosted app was just installed or uninstalled.
  //
  // This might also happen for a siteless SiteInstance which may have a
  // process that's unsuitable for |dest_url|.  For example, another navigation
  // could share that process when over process limit and lock it to a
  // different site before this SiteInstance sets its site.  See
  // https://crbug.com/773809.
  if (rfh->GetSiteInstance()->HasWrongProcessForURL(dest_url))
    return true;

  // A transfer is not needed if the current SiteInstance doesn't yet have a
  // site.  For example, this happens when tests use NavigateToURL or when
  // navigating a blank window in some cases.
  if (!rfh->GetSiteInstance()->HasSite())
    return false;

  // We do not currently swap processes for navigations in webview tag guests.
  if (rfh->GetSiteInstance()->GetSiteURL().SchemeIs(kGuestScheme))
    return false;

  BrowserContext* context = rfh->GetSiteInstance()->GetBrowserContext();
  // TODO(nasko, nick): These following --site-per-process checks are
  // overly simplistic. Update them to match all the cases
  // considered by DetermineSiteInstanceForURL.
  if (IsCurrentlySameSite(rfh, dest_url)) {
    // The same site, no transition needed for security purposes, and we must
    // keep the same SiteInstance for correctness of synchronous scripting.
    return false;
  }

  // The sites differ. If either one requires a dedicated process,
  // then a transfer is needed.
  if (rfh->GetSiteInstance()->RequiresDedicatedProcess() ||
      SiteInstanceImpl::DoesSiteRequireDedicatedProcess(context,
                                                        dest_url)) {
    return true;
  }

  // If the destination URL is not same-site with current RenderFrameHost and
  // doesn't require a dedicated process (see above), but it is same-site with
  // the opener RenderFrameHost, attempt a transfer so that the destination URL
  // can go back to the opener SiteInstance.  This avoids breaking scripting in
  // some cases when only a subset of sites is isolated
  // (https://crbug.com/807184).
  //
  // TODO(alexmos): This is a temporary workaround and should be removed after
  // fixing https://crbug.com/787576.
  FrameTreeNode* opener = frame_tree_node_->opener();
  if (opener && IsCurrentlySameSite(opener->current_frame_host(), dest_url) &&
      opener->current_frame_host()->GetSiteInstance() != rfh->GetSiteInstance())
    return true;

  return false;
}

scoped_refptr<SiteInstance> RenderFrameHostManager::ConvertToSiteInstance(
    const SiteInstanceDescriptor& descriptor,
    SiteInstance* candidate_instance) {
  SiteInstanceImpl* current_instance = render_frame_host_->GetSiteInstance();

  // Note: If the |candidate_instance| matches the descriptor, it will already
  // be set to |descriptor.existing_site_instance|.
  if (descriptor.existing_site_instance)
    return descriptor.existing_site_instance;

  // Note: If the |candidate_instance| matches the descriptor,
  // GetRelatedSiteInstance will return it.
  if (descriptor.relation == SiteInstanceRelation::RELATED)
    return current_instance->GetRelatedSiteInstance(descriptor.dest_url);

  // At this point we know an unrelated site instance must be returned. First
  // check if the candidate matches.
  if (candidate_instance &&
      !current_instance->IsRelatedSiteInstance(candidate_instance) &&
      candidate_instance->GetSiteURL() ==
          SiteInstance::GetSiteForURL(descriptor.browser_context,
                                      descriptor.dest_url)) {
    return candidate_instance;
  }

  // Otherwise return a newly created one.
  return SiteInstance::CreateForURL(
      delegate_->GetControllerForRenderManager().GetBrowserContext(),
      descriptor.dest_url);
}

bool RenderFrameHostManager::IsCurrentlySameSite(RenderFrameHostImpl* candidate,
                                                 const GURL& dest_url) {
  BrowserContext* browser_context =
      delegate_->GetControllerForRenderManager().GetBrowserContext();

  // Ask embedder whether effective URLs should be used when determining if
  // |dest_url| should end up in |candidate|'s SiteInstance.
  // This is used to keep same-site scripting working for hosted apps.
  bool should_compare_effective_urls =
      GetContentClient()
          ->browser()
          ->ShouldCompareEffectiveURLsForSiteInstanceSelection(
              browser_context, candidate->GetSiteInstance(),
              frame_tree_node_->IsMainFrame(),
              candidate->GetSiteInstance()->original_url(), dest_url);

  bool src_has_effective_url = SiteInstanceImpl::HasEffectiveURL(
      browser_context, candidate->GetSiteInstance()->original_url());
  bool dest_has_effective_url =
      SiteInstanceImpl::HasEffectiveURL(browser_context, dest_url);

  // If the process type is incorrect, reject the candidate even if |dest_url|
  // is same-site.  (The URL may have been installed as an app since
  // the last time we visited it.)
  //
  // This check must be skipped to keep same-site subframe navigations from a
  // hosted app to non-hosted app, and vice versa, in the same process.
  // Otherwise, this would return false due to a process privilege level
  // mismatch.
  bool should_check_for_wrong_process =
      should_compare_effective_urls ||
      (!src_has_effective_url && !dest_has_effective_url);
  if (should_check_for_wrong_process &&
      candidate->GetSiteInstance()->HasWrongProcessForURL(dest_url))
    return false;

  // If we don't have a last successful URL, we can't trust the origin or URL
  // stored on the frame, so we fall back to the SiteInstance URL.  This case
  // matters for newly created frames which haven't committed a navigation yet,
  // as well as for net errors. Note that we use the SiteInstance's
  // original_url() and not the site URL, so that we can do this comparison
  // without the effective URL resolution if needed.
  if (candidate->last_successful_url().is_empty()) {
    return SiteInstanceImpl::IsSameWebSite(
        browser_context, candidate->GetSiteInstance()->original_url(), dest_url,
        should_compare_effective_urls);
  }

  // In the common case, we use the RenderFrameHost's last successful URL. Thus,
  // we compare against the last successful commit when deciding whether to swap
  // this time.
  if (SiteInstanceImpl::IsSameWebSite(
          browser_context, candidate->last_successful_url(), dest_url,
          should_compare_effective_urls)) {
    return true;
  }

  // It is possible that last_successful_url() was a nonstandard scheme (for
  // example, "about:blank"). If so, examine the replicated origin to determine
  // the site.
  if (!candidate->GetLastCommittedOrigin().opaque() &&
      SiteInstanceImpl::IsSameWebSite(
          browser_context,
          GURL(candidate->GetLastCommittedOrigin().Serialize()), dest_url,
          should_compare_effective_urls)) {
    return true;
  }

  // If the last successful URL was "about:blank" with a unique origin (which
  // implies that it was a browser-initiated navigation to "about:blank"), none
  // of the cases above apply, but we should still allow a scenario like
  // foo.com -> about:blank -> foo.com to be treated as same-site, as some
  // tests rely on that behavior.  To accomplish this, compare |dest_url|
  // against the site URL.
  if (candidate->last_successful_url().IsAboutBlank() &&
      candidate->GetLastCommittedOrigin().opaque() &&
      SiteInstanceImpl::IsSameWebSite(
          browser_context, candidate->GetSiteInstance()->original_url(),
          dest_url, should_compare_effective_urls)) {
    return true;
  }

  // Not same-site.
  return false;
}

void RenderFrameHostManager::CreateProxiesForNewRenderFrameHost(
    SiteInstance* old_instance,
    SiteInstance* new_instance) {
  // Only create opener proxies if they are in the same BrowsingInstance.
  if (new_instance->IsRelatedSiteInstance(old_instance)) {
    CreateOpenerProxies(new_instance, frame_tree_node_);
  } else {
    // Ensure that the frame tree has RenderFrameProxyHosts for the
    // new SiteInstance in all necessary nodes.  We do this for all frames in
    // the tree, whether they are in the same BrowsingInstance or not.  If
    // |new_instance| is in the same BrowsingInstance as |old_instance|, this
    // will be done as part of CreateOpenerProxies above; otherwise, we do this
    // here.  We will still check whether two frames are in the same
    // BrowsingInstance before we allow them to interact (e.g., postMessage).
    frame_tree_node_->frame_tree()->CreateProxiesForSiteInstance(
        frame_tree_node_, new_instance);
  }
}

void RenderFrameHostManager::CreateProxiesForNewNamedFrame() {
  DCHECK(!frame_tree_node_->frame_name().empty());

  // If this is a top-level frame, create proxies for this node in the
  // SiteInstances of its opener's ancestors, which are allowed to discover
  // this frame by name (see https://crbug.com/511474 and part 4 of
  // https://html.spec.whatwg.org/#the-rules-for-choosing-a-browsing-context-
  // given-a-browsing-context-name).
  FrameTreeNode* opener = frame_tree_node_->opener();
  if (!opener || !frame_tree_node_->IsMainFrame())
    return;
  SiteInstance* current_instance = render_frame_host_->GetSiteInstance();

  // Start from opener's parent.  There's no need to create a proxy in the
  // opener's SiteInstance, since new windows are always first opened in the
  // same SiteInstance as their opener, and if the new window navigates
  // cross-site, that proxy would be created as part of swapping out.
  for (FrameTreeNode* ancestor = opener->parent(); ancestor;
       ancestor = ancestor->parent()) {
    RenderFrameHostImpl* ancestor_rfh = ancestor->current_frame_host();
    if (ancestor_rfh->GetSiteInstance() != current_instance)
      CreateRenderFrameProxy(ancestor_rfh->GetSiteInstance());
  }
}

std::unique_ptr<RenderFrameHostImpl>
RenderFrameHostManager::CreateRenderFrameHost(
    SiteInstance* site_instance,
    int32_t view_routing_id,
    int32_t frame_routing_id,
    int32_t widget_routing_id,
    bool hidden,
    bool renderer_initiated_creation) {
  if (frame_routing_id == MSG_ROUTING_NONE)
    frame_routing_id = site_instance->GetProcess()->GetNextRoutingID();

  // Create a RVH for main frames, or find the existing one for subframes.
  FrameTree* frame_tree = frame_tree_node_->frame_tree();
  RenderViewHostImpl* render_view_host = nullptr;
  if (frame_tree_node_->IsMainFrame()) {
    render_view_host = frame_tree->CreateRenderViewHost(
        site_instance, view_routing_id, frame_routing_id, widget_routing_id,
        false, hidden);
    // TODO(avi): It's a bit bizarre that this logic lives here instead of in
    // CreateRenderFrame(). It turns out that FrameTree::CreateRenderViewHost
    // doesn't /always/ create a new RenderViewHost. It first tries to find an
    // already existing one to reuse by a SiteInstance lookup. If it finds one,
    // then the supplied routing IDs are completely ignored.
    // CreateRenderFrame() could do this lookup too, but it seems redundant to
    // do this lookup in two places. This is a good yak shave to clean up, or,
    // if just ignored, should be an easy cleanup once RenderViewHostImpl has-a
    // RenderWidgetHostImpl. https://crbug.com/545684
    if (view_routing_id == MSG_ROUTING_NONE) {
      widget_routing_id = render_view_host->GetWidget()->GetRoutingID();
    } else {
      DCHECK_NE(view_routing_id, widget_routing_id);
      DCHECK_EQ(view_routing_id, render_view_host->GetRoutingID());
    }
  } else {
    render_view_host = frame_tree->GetRenderViewHost(site_instance);
    CHECK(render_view_host);
  }

  return RenderFrameHostFactory::Create(
      site_instance, render_view_host, frame_tree->render_frame_delegate(),
      frame_tree, frame_tree_node_, frame_routing_id, widget_routing_id, hidden,
      renderer_initiated_creation);
}

bool RenderFrameHostManager::CreateSpeculativeRenderFrameHost(
    SiteInstance* old_instance,
    SiteInstance* new_instance) {
  CHECK(new_instance);
  CHECK_NE(old_instance, new_instance);

  // The process for the new SiteInstance may (if we're sharing a process with
  // another host that already initialized it) or may not (we have our own
  // process or the existing process crashed) have been initialized. Calling
  // Init multiple times will be ignored, so this is safe.
  if (!new_instance->GetProcess()->Init())
    return false;

  CreateProxiesForNewRenderFrameHost(old_instance, new_instance);

  speculative_render_frame_host_ =
      CreateRenderFrame(new_instance, delegate_->IsHidden(), nullptr);

  // If RenderViewHost was created along with the speculative RenderFrameHost,
  // ensure that RenderViewCreated is fired for it.  It is important to do this
  // after speculative_render_frame_host_ is assigned, so that observers
  // processing RenderViewCreated can find it via
  // RenderViewHostImpl::GetMainFrame().
  if (speculative_render_frame_host_) {
    speculative_render_frame_host_->render_view_host()
        ->DispatchRenderViewCreated();
  }

  return !!speculative_render_frame_host_;
}

std::unique_ptr<RenderFrameHostImpl> RenderFrameHostManager::CreateRenderFrame(
    SiteInstance* instance,
    bool hidden,
    int* view_routing_id_ptr) {
  int32_t widget_routing_id = MSG_ROUTING_NONE;
  RenderFrameProxyHost* proxy = GetRenderFrameProxyHost(instance);

  CHECK(instance);

  std::unique_ptr<RenderFrameHostImpl> new_render_frame_host;
  bool success = true;
  if (view_routing_id_ptr)
    *view_routing_id_ptr = MSG_ROUTING_NONE;

  // We are creating a pending or speculative RFH here. We should never create
  // it in the same SiteInstance as our current RFH.
  CHECK_NE(render_frame_host_->GetSiteInstance(), instance);

  // A RenderFrame in a different process from its parent RenderFrame
  // requires a RenderWidget for input/layout/painting.
  //
  // TODO(ajwong): When RVH no longer owns a RWH, this logic should be
  // simplified as the decision to create a RWH will be centralized here.
  // https://crbug.com/545684
  if (frame_tree_node_->parent() &&
      frame_tree_node_->parent()->current_frame_host()->GetSiteInstance() !=
          instance) {
    widget_routing_id = instance->GetProcess()->GetNextRoutingID();
  }

  new_render_frame_host = CreateRenderFrameHost(
      instance, MSG_ROUTING_NONE, MSG_ROUTING_NONE, widget_routing_id, hidden,
      false);
  RenderViewHostImpl* render_view_host =
      new_render_frame_host->render_view_host();

  // Prevent the process from exiting while we're trying to navigate in it.
  new_render_frame_host->GetProcess()->AddPendingView();

  if (frame_tree_node_->IsMainFrame()) {
    success = InitRenderView(render_view_host, proxy);

    // If we are reusing the RenderViewHost and it doesn't already have a
    // RenderWidgetHostView, we need to create one if this is the main frame.
    if (!render_view_host->GetWidget()->GetView())
      delegate_->CreateRenderWidgetHostViewForRenderManager(render_view_host);
  } else {
    DCHECK(render_view_host->IsRenderViewLive());
  }

  if (success) {
    if (frame_tree_node_->IsMainFrame()) {
      // Don't show the main frame's view until we get a DidNavigate from it.
      // Only the RenderViewHost for the top-level RenderFrameHost has a
      // RenderWidgetHostView; RenderWidgetHosts for out-of-process iframes
      // will be created later and hidden.
      if (render_view_host->GetWidget()->GetView())
        render_view_host->GetWidget()->GetView()->Hide();
    }
    // RenderViewHost for |instance| might exist prior to calling
    // CreateRenderFrame. In such a case, InitRenderView will not create the
    // RenderFrame in the renderer process and it needs to be done
    // explicitly.
    DCHECK(new_render_frame_host);
    success = InitRenderFrame(new_render_frame_host.get());
  }

  if (success) {
    if (view_routing_id_ptr)
      *view_routing_id_ptr = render_view_host->GetRoutingID();
  }

  // Return the new RenderFrameHost on successful creation.
  if (success) {
    DCHECK(new_render_frame_host->GetSiteInstance() == instance);
    return new_render_frame_host;
  }
  return nullptr;
}

int RenderFrameHostManager::CreateRenderFrameProxy(SiteInstance* instance) {
  // A RenderFrameProxyHost should never be created in the same SiteInstance as
  // the current RFH.
  CHECK(instance);
  CHECK_NE(instance, render_frame_host_->GetSiteInstance());

  // Return an already existing RenderFrameProxyHost if one exists and is alive.
  RenderFrameProxyHost* proxy = GetRenderFrameProxyHost(instance);
  if (proxy && proxy->is_render_frame_proxy_live())
    return proxy->GetRoutingID();

  // At this point we know that we either have to 1) create a new
  // RenderFrameProxyHost or 2) revive an existing, but no longer alive
  // RenderFrameProxyHost.
  RenderViewHostImpl* render_view_host =
      frame_tree_node_->frame_tree()->GetRenderViewHost(instance);
  if (!proxy) {
    // Before creating a new RenderFrameProxyHost, ensure a RenderViewHost
    // exists for |instance|, as it creates the page level structure in Blink.
    if (!render_view_host) {
      CHECK(frame_tree_node_->IsMainFrame());
      render_view_host = frame_tree_node_->frame_tree()->CreateRenderViewHost(
          instance, MSG_ROUTING_NONE, MSG_ROUTING_NONE, MSG_ROUTING_NONE, true,
          true);
    }

    proxy = CreateRenderFrameProxyHost(instance, render_view_host);
  }

  // Make sure that the RenderFrameProxy is present in the renderer.
  if (frame_tree_node_->IsMainFrame() && render_view_host) {
    InitRenderView(render_view_host, proxy);
  } else {
    proxy->InitRenderFrameProxy();
  }

  return proxy->GetRoutingID();
}

void RenderFrameHostManager::CreateProxiesForChildFrame(FrameTreeNode* child) {
  RenderFrameProxyHost* outer_delegate_proxy =
      ForInnerDelegate() ? GetProxyToOuterDelegate() : nullptr;
  for (const auto& pair : proxy_hosts_) {
    // Do not create proxies for subframes in the outer delegate's process,
    // since the outer delegate does not need to interact with them.
    if (pair.second.get() == outer_delegate_proxy)
      continue;

    child->render_manager()->CreateRenderFrameProxy(
        pair.second->GetSiteInstance());
  }
}

void RenderFrameHostManager::EnsureRenderViewInitialized(
    RenderViewHostImpl* render_view_host,
    SiteInstance* instance) {
  DCHECK(frame_tree_node_->IsMainFrame());

  if (render_view_host->IsRenderViewLive())
    return;

  // If the proxy in |instance| doesn't exist, this RenderView is not swapped
  // out and shouldn't be reinitialized here.
  RenderFrameProxyHost* proxy = GetRenderFrameProxyHost(instance);
  if (!proxy)
    return;

  InitRenderView(render_view_host, proxy);
}

void RenderFrameHostManager::CreateOuterDelegateProxy(
    SiteInstance* outer_contents_site_instance,
    RenderFrameHostImpl* render_frame_host) {
  // We only get here when Delegate for this manager is an inner delegate and is
  // based on cross process frames.
  RenderFrameProxyHost* proxy =
      CreateRenderFrameProxyHost(outer_contents_site_instance, nullptr);

  // Swap the outer WebContents's frame with the proxy to inner WebContents.
  //
  // We are in the outer WebContents, and its FrameTree would never see
  // a load start for any of its inner WebContents. Eventually, that also makes
  // the FrameTree never see the matching load stop. Therefore, we always pass
  // false to |is_loading| below.
  // TODO(lazyboy): This |is_loading| behavior might not be what we want,
  // investigate and fix.
  render_frame_host->Send(new FrameMsg_SwapOut(
      render_frame_host->GetRoutingID(), proxy->GetRoutingID(),
      false /* is_loading */,
      render_frame_host->frame_tree_node()->current_replication_state()));
  proxy->set_render_frame_proxy_created(true);

  // There is no longer a RenderFrame associated with this RenderFrameHost.
  render_frame_host->SetRenderFrameCreated(false);
}

void RenderFrameHostManager::SetRWHViewForInnerContents(
    RenderWidgetHostView* child_rwhv) {
  DCHECK(ForInnerDelegate() && frame_tree_node_->IsMainFrame());
  GetProxyToOuterDelegate()->SetChildRWHView(child_rwhv, nullptr);
}

bool RenderFrameHostManager::InitRenderView(
    RenderViewHostImpl* render_view_host,
    RenderFrameProxyHost* proxy) {
  // Ensure the renderer process is initialized before creating the
  // RenderView.
  if (!render_view_host->GetProcess()->Init())
    return false;

  // We may have initialized this RenderViewHost for another RenderFrameHost.
  if (render_view_host->IsRenderViewLive())
    return true;

  int opener_frame_routing_id =
      GetOpenerRoutingID(render_view_host->GetSiteInstance());

  bool created = delegate_->CreateRenderViewForRenderManager(
      render_view_host, opener_frame_routing_id,
      proxy ? proxy->GetRoutingID() : MSG_ROUTING_NONE,
      frame_tree_node_->devtools_frame_token(),
      frame_tree_node_->current_replication_state());

  if (created && proxy)
    proxy->set_render_frame_proxy_created(true);

  return created;
}

scoped_refptr<SiteInstance>
RenderFrameHostManager::GetSiteInstanceForNavigationRequest(
    const NavigationRequest& request) {
  // First, check if the navigation can switch SiteInstances. If not, the
  // navigation should use the current SiteInstance.
  SiteInstance* current_site_instance = render_frame_host_->GetSiteInstance();
  bool no_renderer_swap_allowed = false;
  bool should_swap_for_error_isolation = false;
  bool was_server_redirect = request.navigation_handle() &&
                             request.navigation_handle()->WasServerRedirect();

  // When error page isolation is enabled, each navigation that crosses
  // from a success to failure and vice versa needs to do a process swap.
  if (SiteIsolationPolicy::IsErrorPageIsolationEnabled(
          frame_tree_node_->IsMainFrame())) {
    should_swap_for_error_isolation =
        (request.state() == NavigationRequest::FAILED) !=
        (current_site_instance->GetSiteURL() == GURL(kUnreachableWebDataURL));
  }

  if (frame_tree_node_->IsMainFrame()) {
    // Renderer-initiated main frame navigations that may require a
    // SiteInstance swap are sent to the browser via the OpenURL IPC and are
    // afterwards treated as browser-initiated navigations. NavigationRequests
    // marked as renderer-initiated are created by receiving a BeginNavigation
    // IPC, and will then proceed in the same renderer. In site-per-process
    // mode, it is possible for renderer-intiated navigations to be allowed to
    // go cross-process. Check it first.
    bool can_renderer_initiate_transfer =
        render_frame_host_->IsRenderFrameLive() &&
        IsURLHandledByNetworkStack(request.common_params().url) &&
        IsRendererTransferNeededForNavigation(render_frame_host_.get(),
                                              request.common_params().url);
    no_renderer_swap_allowed |=
        request.from_begin_navigation() && !can_renderer_initiate_transfer;
  } else {
    // Subframe navigations will use the current renderer, unless specifically
    // allowed to swap processes.
    no_renderer_swap_allowed |= !CanSubframeSwapProcess(
        request.common_params().url, request.source_site_instance(),
        request.dest_site_instance());
  }

  if (no_renderer_swap_allowed && !should_swap_for_error_isolation)
    return scoped_refptr<SiteInstance>(current_site_instance);

  // If the navigation can swap SiteInstances, compute the SiteInstance it
  // should use.
  // TODO(clamy): We should also consider as a candidate SiteInstance the
  // speculative SiteInstance that was computed on redirects.
  SiteInstance* candidate_site_instance =
      speculative_render_frame_host_
          ? speculative_render_frame_host_->GetSiteInstance()
          : nullptr;

  scoped_refptr<SiteInstance> dest_site_instance = GetSiteInstanceForNavigation(
      request.common_params().url, request.source_site_instance(),
      request.dest_site_instance(), candidate_site_instance,
      request.common_params().transition,
      request.state() == NavigationRequest::FAILED,
      request.restore_type() != RestoreType::NONE, request.is_view_source(),
      was_server_redirect);

  return dest_site_instance;
}

bool RenderFrameHostManager::InitRenderFrame(
    RenderFrameHostImpl* render_frame_host) {
  if (render_frame_host->IsRenderFrameLive())
    return true;

  SiteInstance* site_instance = render_frame_host->GetSiteInstance();

  int opener_routing_id = MSG_ROUTING_NONE;
  if (frame_tree_node_->opener())
    opener_routing_id = GetOpenerRoutingID(site_instance);

  int parent_routing_id = MSG_ROUTING_NONE;
  if (frame_tree_node_->parent()) {
    parent_routing_id = frame_tree_node_->parent()
                            ->render_manager()
                            ->GetRoutingIdForSiteInstance(site_instance);
    CHECK_NE(parent_routing_id, MSG_ROUTING_NONE);
  }

  // At this point, all RenderFrameProxies for sibling frames have already been
  // created, including any proxies that come after this frame.  To preserve
  // correct order for indexed window access (e.g., window.frames[1]), pass the
  // previous sibling frame so that this frame is correctly inserted into the
  // frame tree on the renderer side.
  int previous_sibling_routing_id = MSG_ROUTING_NONE;
  FrameTreeNode* previous_sibling = frame_tree_node_->PreviousSibling();
  if (previous_sibling) {
    previous_sibling_routing_id =
        previous_sibling->render_manager()->GetRoutingIdForSiteInstance(
            site_instance);
    CHECK_NE(previous_sibling_routing_id, MSG_ROUTING_NONE);
  }

  // Check whether there is an existing proxy for this frame in this
  // SiteInstance. If there is, the new RenderFrame needs to be able to find
  // the proxy it is replacing, so that it can fully initialize itself.
  // NOTE: This is the only time that a RenderFrameProxyHost can be in the same
  // SiteInstance as its RenderFrameHost. This is only the case until the
  // RenderFrameHost commits, at which point it will replace and delete the
  // RenderFrameProxyHost.
  int proxy_routing_id = MSG_ROUTING_NONE;
  RenderFrameProxyHost* existing_proxy = GetRenderFrameProxyHost(site_instance);
  if (existing_proxy) {
    proxy_routing_id = existing_proxy->GetRoutingID();
    CHECK_NE(proxy_routing_id, MSG_ROUTING_NONE);
    if (!existing_proxy->is_render_frame_proxy_live())
      existing_proxy->InitRenderFrameProxy();
  }

  return delegate_->CreateRenderFrameForRenderManager(
      render_frame_host, proxy_routing_id, opener_routing_id, parent_routing_id,
      previous_sibling_routing_id);
}

bool RenderFrameHostManager::ReinitializeRenderFrame(
    RenderFrameHostImpl* render_frame_host) {
  // This should be used only when the RenderFrame is not live.
  DCHECK(!render_frame_host->IsRenderFrameLive());

  // Recreate the opener chain.
  CreateOpenerProxies(render_frame_host->GetSiteInstance(), frame_tree_node_);

  // Main frames need both the RenderView and RenderFrame reinitialized, so
  // use InitRenderView.  For cross-process subframes, InitRenderView won't
  // recreate the RenderFrame, so use InitRenderFrame instead.  Note that for
  // subframe RenderFrameHosts, the swapped out RenderView in their
  // SiteInstance will be recreated as part of CreateOpenerProxies above.
  if (!frame_tree_node_->parent()) {
    DCHECK(!GetRenderFrameProxyHost(render_frame_host->GetSiteInstance()));
    if (!InitRenderView(render_frame_host->render_view_host(), nullptr))
      return false;
  } else {
    if (!InitRenderFrame(render_frame_host))
      return false;

    // When a subframe renderer dies, its RenderWidgetHostView is cleared in
    // its CrossProcessFrameConnector, so we need to restore it now that it
    // is re-initialized.
    RenderFrameProxyHost* proxy_to_parent = GetProxyToParent();
    if (proxy_to_parent) {
      const gfx::Size* size = render_frame_host->frame_size()
                                  ? &*render_frame_host->frame_size()
                                  : nullptr;
      GetProxyToParent()->SetChildRWHView(render_frame_host->GetView(), size);
    }
  }

  DCHECK(render_frame_host->IsRenderFrameLive());
  return true;
}

int RenderFrameHostManager::GetRoutingIdForSiteInstance(
    SiteInstance* site_instance) {
  if (render_frame_host_->GetSiteInstance() == site_instance)
    return render_frame_host_->GetRoutingID();

  RenderFrameProxyHost* proxy = GetRenderFrameProxyHost(site_instance);
  if (proxy)
    return proxy->GetRoutingID();

  return MSG_ROUTING_NONE;
}

void RenderFrameHostManager::CommitPendingWebUI() {
  TRACE_EVENT1("navigation", "RenderFrameHostManager::CommitPendingWebUI",
               "FrameTreeNode id", frame_tree_node_->frame_tree_node_id());
  DCHECK(render_frame_host_->pending_web_ui());

  // First check whether we're going to want to focus the location bar after
  // this commit.  We do this now because the navigation hasn't formally
  // committed yet, so if we've already cleared the pending WebUI the call chain
  // this triggers won't be able to figure out what's going on.
  bool will_focus_location_bar = delegate_->FocusLocationBarByDefault();

  render_frame_host_->CommitPendingWebUI();

  if (will_focus_location_bar)
    delegate_->SetFocusToLocationBar(false);
}

void RenderFrameHostManager::CommitPending() {
  TRACE_EVENT1("navigation", "RenderFrameHostManager::CommitPending",
               "FrameTreeNode id", frame_tree_node_->frame_tree_node_id());
  DCHECK(speculative_render_frame_host_);

#if defined(OS_MACOSX)
  // The old RenderWidgetHostView will be hidden before the new
  // RenderWidgetHostView takes its contents. Ensure that Cocoa sees this as
  // a single transaction.
  // https://crbug.com/829523
  // TODO(ccameron): This can be removed when the RenderWidgetHostViewMac uses
  // the same ui::Compositor as MacViews.
  // https://crbug.com/331669
  gfx::ScopedCocoaDisableScreenUpdates disabler;
#endif  // defined(OS_MACOSX)

  bool is_main_frame = frame_tree_node_->IsMainFrame();

  // First check whether we're going to want to focus the location bar after
  // this commit.  We do this now because the navigation hasn't formally
  // committed yet, so if we've already cleared the pending WebUI the call chain
  // this triggers won't be able to figure out what's going on.  Note that
  // subframe commits should not be allowed to steal focus from the main frame
  // by focusing the location bar (see https://crbug.com/700124).
  bool will_focus_location_bar =
      is_main_frame && delegate_->FocusLocationBarByDefault();

  // Remember if the page was focused so we can focus the new renderer in
  // that case.
  bool focus_render_view = !will_focus_location_bar &&
                           render_frame_host_->GetView() &&
                           render_frame_host_->GetView()->HasFocus();

  // Remove the current frame and its descendants from the set of fullscreen
  // frames immediately. They can stay in pending deletion for some time.
  // Removing them when they are deleted is too late.
  // This needs to be done before updating the frame tree structure, else it
  // will have trouble removing the descendants.
  frame_tree_node_->frame_tree()
      ->render_frame_delegate()
      ->FullscreenStateChanged(current_frame_host(), false);

  // TODO(arthursonzogni): Stop doing this. Keep the subframes alive in pending
  // deletion so that they can always properly execute their unload event
  // handlers.
  current_frame_host()->ResetChildren();

  // Swap in the pending or speculative frame and make it active. Also ensure
  // the FrameTree stays in sync.
  std::unique_ptr<RenderFrameHostImpl> old_render_frame_host;
  DCHECK(speculative_render_frame_host_);
  old_render_frame_host =
      SetRenderFrameHost(std::move(speculative_render_frame_host_));

  // For top-level frames, also hide the old RenderViewHost's view.
  // TODO(creis): As long as show/hide are on RVH, we don't want to hide on
  // subframe navigations or we will interfere with the top-level frame.
  if (is_main_frame &&
      old_render_frame_host->render_view_host()->GetWidget()->GetView()) {
    old_render_frame_host->render_view_host()->GetWidget()->GetView()->Hide();
  }

  // Make sure the size is up to date.  (Fix for bug 1079768.)
  delegate_->UpdateRenderViewSizeForRenderManager(is_main_frame);

  if (will_focus_location_bar) {
    delegate_->SetFocusToLocationBar(false);
  } else if (focus_render_view && render_frame_host_->GetView()) {
    if (is_main_frame) {
      render_frame_host_->GetView()->Focus();
    } else {
      // The current tab has page-level focus, so we need to propagate
      // page-level focus to the subframe's renderer. Before doing that, also
      // tell the new renderer what the focused frame is if that frame is not
      // in its process, so that Blink's page-level focus logic won't try to
      // reset frame focus to the main frame.  See https://crbug.com/802156.
      FrameTreeNode* focused_frame =
          frame_tree_node_->frame_tree()->GetFocusedFrame();
      if (focused_frame && !focused_frame->IsMainFrame() &&
          focused_frame->current_frame_host()->GetSiteInstance() !=
              render_frame_host_->GetSiteInstance()) {
        focused_frame->render_manager()
            ->GetRenderFrameProxyHost(render_frame_host_->GetSiteInstance())
            ->SetFocusedFrame();
      }
      frame_tree_node_->frame_tree()->SetPageFocus(
          render_frame_host_->GetSiteInstance(), true);
    }
  }

  // Notify that we've swapped RenderFrameHosts. We do this before shutting down
  // the RFH so that we can clean up RendererResources related to the RFH first.
  delegate_->NotifySwappedFromRenderManager(
      old_render_frame_host.get(), render_frame_host_.get(), is_main_frame);

  // Make the new view show the contents of old view until it has something
  // useful to show.
  if (is_main_frame && old_render_frame_host->GetView() &&
      render_frame_host_->GetView()) {
    render_frame_host_->GetView()->TakeFallbackContentFrom(
        old_render_frame_host->GetView());
  }

  // The RenderViewHost keeps track of the main RenderFrameHost routing id.
  // If this is committing a main frame navigation, update it and set the
  // routing id in the RenderViewHost associated with the old RenderFrameHost
  // to MSG_ROUTING_NONE.
  if (is_main_frame) {
    RenderViewHostImpl* rvh = render_frame_host_->render_view_host();
    rvh->set_main_frame_routing_id(render_frame_host_->routing_id());

    // If the RenderViewHost is transitioning from swapped out to active state,
    // it was reused, so dispatch a RenderViewReady event.  For example, this
    // is necessary to hide the sad tab if one is currently displayed.  See
    // https://crbug.com/591984.
    //
    // TODO(alexmos):  Remove this and move RenderViewReady consumers to use
    // the main frame's RenderFrameCreated instead.
    if (!rvh->is_active())
      rvh->PostRenderViewReady();

    rvh->SetIsActive(true);
    rvh->set_is_swapped_out(false);
    old_render_frame_host->render_view_host()->set_main_frame_routing_id(
        MSG_ROUTING_NONE);
  }

  // Store the old_render_frame_host's current frame size so that it can be used
  // to initialize the child RWHV.
  base::Optional<gfx::Size> old_size = old_render_frame_host->frame_size();

  // Swap out the old frame now that the new one is visible.
  // This will swap it out and schedule it for deletion when the swap out ack
  // arrives (or immediately if the process isn't live).
  SwapOutOldFrame(std::move(old_render_frame_host));

  // Since the new RenderFrameHost is now committed, there must be no proxies
  // for its SiteInstance. Delete any existing ones.
  DeleteRenderFrameProxyHost(render_frame_host_->GetSiteInstance());

  // If this is a subframe, it should have a CrossProcessFrameConnector
  // created already.  Use it to link the new RFH's view to the proxy that
  // belongs to the parent frame's SiteInstance. If this navigation causes
  // an out-of-process frame to return to the same process as its parent, the
  // proxy would have been removed from proxy_hosts_ above.
  // Note: We do this after swapping out the old RFH because that may create
  // the proxy we're looking for.
  RenderFrameProxyHost* proxy_to_parent = GetProxyToParent();
  if (proxy_to_parent) {
    proxy_to_parent->SetChildRWHView(render_frame_host_->GetView(),
                                     old_size ? &*old_size : nullptr);
  }

  // Show the new view (or a sad tab) if necessary.
  bool new_rfh_has_view = !!render_frame_host_->GetView();
  if (!delegate_->IsHidden() && new_rfh_has_view) {
    if (!is_main_frame &&
        !render_frame_host_->render_view_host()->is_active()) {
      // Ensure that page visibility in the subframe's process is set to shown.
      // This is important if the subframe is using a RenderView which
      // started out as active and later became swapped-out, which also updates
      // page visibility to hidden.  Without updating page visibility the
      // subframe would not be able to generate compositor frames.  See
      // https://crbug.com/638375.
      //
      // TODO(alexmos,dcheng,lfg): This workaround should be cleaned up as part
      // of the view/widget split.  We should decouple page visibility from
      // widget visibility.
      RenderFrameProxyHost* proxy =
          frame_tree_node_->frame_tree()
              ->root()
              ->render_manager()
              ->GetRenderFrameProxyHost(render_frame_host_->GetSiteInstance());
      // The proxy should always exist since the RenderViewHost is not active.
      proxy->Send(new PageMsg_WasShown(proxy->GetRoutingID()));
    }

    // In most cases, we need to show the new view.
    render_frame_host_->GetView()->Show();
  }
  // The process will no longer try to exit, so we can decrement the count.
  render_frame_host_->GetProcess()->RemovePendingView();

  if (!new_rfh_has_view) {
    // If the view is gone, then this RenderViewHost died while it was hidden.
    // We ignored the RenderProcessGone call at the time, so we should send it
    // now to make sure the sad tab shows up, etc.
    DCHECK(!render_frame_host_->IsRenderFrameLive());
    DCHECK(!render_frame_host_->render_view_host()->IsRenderViewLive());
    render_frame_host_->ResetLoadingState();
    delegate_->RenderProcessGoneFromRenderManager(
        render_frame_host_->render_view_host());
  }

  // After all is done, there must never be a proxy in the list which has the
  // same SiteInstance as the current RenderFrameHost.
  CHECK(!GetRenderFrameProxyHost(render_frame_host_->GetSiteInstance()));
}

void RenderFrameHostManager::UpdatePendingWebUIOnCurrentFrameHost(
    const GURL& dest_url,
    int entry_bindings) {
  bool pending_webui_changed =
      render_frame_host_->UpdatePendingWebUI(dest_url, entry_bindings);
  DCHECK_EQ(GetNavigatingWebUI(), render_frame_host_->pending_web_ui());

  if (render_frame_host_->pending_web_ui() && pending_webui_changed &&
      render_frame_host_->IsRenderFrameLive()) {
    // If a pending WebUI exists in the current RenderFrameHost and it has been
    // updated and the associated RenderFrame is alive, notify the WebUI about
    // the RenderFrame.
    // Note: If the RenderFrame is not alive at this point the notification
    // will happen later, when the RenderFrame is created.
    if (render_frame_host_->pending_web_ui() == render_frame_host_->web_ui()) {
      // If the active WebUI is being reused it has already interacting with
      // this RenderFrame and its RenderView in the past, so call
      // RenderFrameReused.
      render_frame_host_->pending_web_ui()->RenderFrameReused(
          render_frame_host_.get());
    } else {
      // If this is a new WebUI it has never interacted with the existing
      // RenderFrame so call RenderFrameCreated.
      render_frame_host_->pending_web_ui()->RenderFrameCreated(
          render_frame_host_.get());
    }
  }
}

std::unique_ptr<RenderFrameHostImpl> RenderFrameHostManager::SetRenderFrameHost(
    std::unique_ptr<RenderFrameHostImpl> render_frame_host) {
  // Swap the two.
  std::unique_ptr<RenderFrameHostImpl> old_render_frame_host =
      std::move(render_frame_host_);
  render_frame_host_ = std::move(render_frame_host);

  if (frame_tree_node_->IsMainFrame()) {
    // Update the count of top-level frames using this SiteInstance.  All
    // subframes are in the same BrowsingInstance as the main frame, so we only
    // count top-level ones.  This makes the value easier for consumers to
    // interpret.
    if (render_frame_host_) {
      render_frame_host_->GetSiteInstance()->
          IncrementRelatedActiveContentsCount();
    }
    if (old_render_frame_host) {
      old_render_frame_host->GetSiteInstance()->
          DecrementRelatedActiveContentsCount();
    }
  }

  return old_render_frame_host;
}

RenderViewHostImpl* RenderFrameHostManager::GetSwappedOutRenderViewHost(
    SiteInstance* instance) const {
  RenderFrameProxyHost* proxy = GetRenderFrameProxyHost(instance);
  if (proxy)
    return proxy->GetRenderViewHost();
  return nullptr;
}

RenderFrameProxyHost* RenderFrameHostManager::GetRenderFrameProxyHost(
    SiteInstance* instance) const {
  auto it = proxy_hosts_.find(instance->GetId());
  if (it != proxy_hosts_.end())
    return it->second.get();
  return nullptr;
}

int RenderFrameHostManager::GetProxyCount() {
  return proxy_hosts_.size();
}

void RenderFrameHostManager::CollectOpenerFrameTrees(
    std::vector<FrameTree*>* opener_frame_trees,
    base::hash_set<FrameTreeNode*>* nodes_with_back_links) {
  CHECK(opener_frame_trees);
  opener_frame_trees->push_back(frame_tree_node_->frame_tree());

  // Add the FrameTree of the given node's opener to the list of
  // |opener_frame_trees| if it doesn't exist there already. |visited_index|
  // indicates which FrameTrees in |opener_frame_trees| have already been
  // visited (i.e., those at indices less than |visited_index|).
  // |nodes_with_back_links| collects FrameTreeNodes with openers in FrameTrees
  // that have already been visited (such as those with cycles).
  size_t visited_index = 0;
  while (visited_index < opener_frame_trees->size()) {
    FrameTree* frame_tree = (*opener_frame_trees)[visited_index];
    visited_index++;
    for (FrameTreeNode* node : frame_tree->Nodes()) {
      if (!node->opener())
        continue;

      FrameTree* opener_tree = node->opener()->frame_tree();
      const auto& existing_tree_it = std::find(
          opener_frame_trees->begin(), opener_frame_trees->end(), opener_tree);

      if (existing_tree_it == opener_frame_trees->end()) {
        // This is a new opener tree that we will need to process.
        opener_frame_trees->push_back(opener_tree);
      } else {
        // If this tree is already on our processing list *and* we have visited
        // it,
        // then this node's opener is a back link.  This means the node will
        // need
        // special treatment to process its opener.
        size_t position =
            std::distance(opener_frame_trees->begin(), existing_tree_it);
        if (position < visited_index)
          nodes_with_back_links->insert(node);
      }
    }
  }
}

void RenderFrameHostManager::CreateOpenerProxies(
    SiteInstance* instance,
    FrameTreeNode* skip_this_node) {
  std::vector<FrameTree*> opener_frame_trees;
  base::hash_set<FrameTreeNode*> nodes_with_back_links;

  CollectOpenerFrameTrees(&opener_frame_trees, &nodes_with_back_links);

  // Create opener proxies for frame trees, processing furthest openers from
  // this node first and this node last.  In the common case without cycles,
  // this will ensure that each tree's openers are created before the tree's
  // nodes need to reference them.
  for (int i = opener_frame_trees.size() - 1; i >= 0; i--) {
    opener_frame_trees[i]
        ->root()
        ->render_manager()
        ->CreateOpenerProxiesForFrameTree(instance, skip_this_node);
  }

  // Set openers for nodes in |nodes_with_back_links| in a second pass.
  // The proxies created at these FrameTreeNodes in
  // CreateOpenerProxiesForFrameTree won't have their opener routing ID
  // available when created due to cycles or back links in the opener chain.
  // They must have their openers updated as a separate step after proxy
  // creation.
  for (auto* node : nodes_with_back_links) {
    RenderFrameProxyHost* proxy =
        node->render_manager()->GetRenderFrameProxyHost(instance);
    // If there is no proxy, the cycle may involve nodes in the same process,
    // or, if this is a subframe, --site-per-process may be off.  Either way,
    // there's nothing more to do.
    if (!proxy)
      continue;

    int opener_routing_id =
        node->render_manager()->GetOpenerRoutingID(instance);
    DCHECK_NE(opener_routing_id, MSG_ROUTING_NONE);
    proxy->Send(new FrameMsg_UpdateOpener(proxy->GetRoutingID(),
                                          opener_routing_id));
  }
}

void RenderFrameHostManager::CreateOpenerProxiesForFrameTree(
    SiteInstance* instance,
    FrameTreeNode* skip_this_node) {
  // Currently, this function is only called on main frames.  It should
  // actually work correctly for subframes as well, so if that need ever
  // arises, it should be sufficient to remove this DCHECK.
  DCHECK(frame_tree_node_->IsMainFrame());

  FrameTree* frame_tree = frame_tree_node_->frame_tree();

  // Ensure that all the nodes in the opener's FrameTree have
  // RenderFrameProxyHosts for the new SiteInstance.  Only pass the node to
  // be skipped if it's in the same FrameTree.
  if (skip_this_node && skip_this_node->frame_tree() != frame_tree)
    skip_this_node = nullptr;
  frame_tree->CreateProxiesForSiteInstance(skip_this_node, instance);
}

int RenderFrameHostManager::GetOpenerRoutingID(SiteInstance* instance) {
  if (!frame_tree_node_->opener())
    return MSG_ROUTING_NONE;

  return frame_tree_node_->opener()
      ->render_manager()
      ->GetRoutingIdForSiteInstance(instance);
}

void RenderFrameHostManager::SendPageMessage(IPC::Message* msg,
                                             SiteInstance* instance_to_skip) {
  DCHECK(IPC_MESSAGE_CLASS(*msg) == PageMsgStart);

  // We should always deliver page messages through the main frame.
  DCHECK(!frame_tree_node_->parent());

  if ((IPC_MESSAGE_CLASS(*msg) != PageMsgStart) || frame_tree_node_->parent()) {
    delete msg;
    return;
  }

  auto send_msg = [instance_to_skip](IPC::Sender* sender,
                                     int routing_id,
                                     IPC::Message* msg,
                                     SiteInstance* sender_instance) {
    if (sender_instance == instance_to_skip)
      return;

    IPC::Message* copy = new IPC::Message(*msg);
    copy->set_routing_id(routing_id);
    sender->Send(copy);
  };

  // When sending a PageMessage for an inner WebContents, we don't want to also
  // send it to the outer WebContent's frame as well.
  RenderFrameProxyHost* outer_delegate_proxy =
      ForInnerDelegate() ? GetProxyToOuterDelegate() : nullptr;
  for (const auto& pair : proxy_hosts_) {
    if (outer_delegate_proxy != pair.second.get()) {
      send_msg(pair.second.get(), pair.second->GetRoutingID(), msg,
               pair.second->GetSiteInstance());
    }
  }

  if (speculative_render_frame_host_) {
    send_msg(speculative_render_frame_host_.get(),
             speculative_render_frame_host_->GetRoutingID(), msg,
             speculative_render_frame_host_->GetSiteInstance());
  }

  if (render_frame_host_->GetSiteInstance() != instance_to_skip) {
    // Send directly instead of using send_msg() so that |msg| doesn't leak.
    msg->set_routing_id(render_frame_host_->GetRoutingID());
    render_frame_host_->Send(msg);
  } else {
    delete msg;
  }
}

bool RenderFrameHostManager::CanSubframeSwapProcess(
    const GURL& dest_url,
    SiteInstance* source_instance,
    SiteInstance* dest_instance) {
  // On renderer-initiated navigations, when the frame initiating the navigation
  // and the frame being navigated differ, |source_instance| is set to the
  // SiteInstance of the initiating frame. |dest_instance| is present on session
  // history navigations. The two cannot be set simultaneously.
  DCHECK(!source_instance || !dest_instance);

  // If dest_url is a unique origin like about:blank, then the need for a swap
  // is determined by the source_instance or dest_instance.
  GURL resolved_url = dest_url;
  if (url::Origin::Create(resolved_url).opaque()) {
    if (source_instance) {
      resolved_url = source_instance->GetSiteURL();
    } else if (dest_instance) {
      resolved_url = dest_instance->GetSiteURL();
    } else {
      // If there is no SiteInstance this unique origin can be associated with,
      // then check whether it is safe to put into the parent frame's process.
      // This is the case for about:blank URLs (with or without fragments),
      // since they contain no active data.  This is also the case for
      // about:srcdoc, since such URLs only get active content from their parent
      // frame.  Using the parent frame's process avoids putting blank frames
      // into OOPIFs and preserves scripting for about:srcdoc.
      //
      // Allow a process swap for other unique origin URLs, such as data: URLs.
      // These have active content and may have come from an untrusted source,
      // such as a restored frame from a different site or a redirect.
      // (Normally, redirects to data: or about: URLs are disallowed as
      // net::ERR_UNSAFE_REDIRECT. However, extensions can still redirect
      // arbitary requests to those URLs using the chrome.webRequest or
      // chrome.declarativeWebRequest API, which will end up here (for an
      // example, see ExtensionWebRequestApiTest.WebRequestDeclarative1).)
      if (resolved_url.IsAboutBlank() ||
          resolved_url == GURL(content::kAboutSrcDocURL)) {
        return false;
      }
    }
  }

  // If we are in an OOPIF mode that only applies to some sites, only swap if
  // the policy determines that a transfer would have been needed.  We can get
  // here for session restore.
  if (!IsRendererTransferNeededForNavigation(render_frame_host_.get(),
                                             resolved_url)) {
    DCHECK(!dest_instance ||
           dest_instance == render_frame_host_->GetSiteInstance());
    return false;
  }

  return true;
}

void RenderFrameHostManager::EnsureRenderFrameHostVisibilityConsistent() {
  RenderWidgetHostView* view = GetRenderWidgetHostView();
  if (view && static_cast<RenderWidgetHostImpl*>(view->GetRenderWidgetHost())
                      ->is_hidden() != delegate_->IsHidden()) {
    if (delegate_->IsHidden()) {
      view->Hide();
    } else {
      view->Show();
    }
  }
}

void RenderFrameHostManager::EnsureRenderFrameHostPageFocusConsistent() {
  frame_tree_node_->frame_tree()->SetPageFocus(
      render_frame_host_->GetSiteInstance(), frame_tree_node_->frame_tree()
                                                 ->root()
                                                 ->current_frame_host()
                                                 ->GetRenderWidgetHost()
                                                 ->is_focused());
}

}  // namespace content
