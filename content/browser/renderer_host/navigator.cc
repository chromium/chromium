// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigator.h"

#include <utility>

#include "base/check_op.h"
#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/debug_urls.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigation_request_info.h"
#include "content/browser/renderer_host/navigator_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/browser/web_package/web_bundle_handle_tracker.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/inter_process_time_ticks_converter.h"
#include "content/common/navigation_params.h"
#include "content/common/navigation_params_utils.h"
#include "content/common/page_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/restore_type.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/navigation_policy.h"
#include "content/public/common/url_utils.h"
#include "net/base/net_errors.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace content {

struct Navigator::NavigationMetricsData {
  NavigationMetricsData(base::TimeTicks start_time,
                        GURL url,
                        ukm::SourceId ukm_source_id,
                        bool is_browser_initiated_before_unload,
                        RestoreType restore_type)
      : start_time_(start_time),
        url_(url),
        ukm_source_id_(ukm_source_id),
        is_browser_initiated_before_unload_(
            is_browser_initiated_before_unload) {
    is_restoring_from_last_session_ =
        (restore_type == RestoreType::LAST_SESSION_EXITED_CLEANLY ||
         restore_type == RestoreType::LAST_SESSION_CRASHED);
  }

  base::TimeTicks start_time_;
  GURL url_;
  ukm::SourceId ukm_source_id_;
  bool is_browser_initiated_before_unload_;
  bool is_restoring_from_last_session_;
  base::TimeTicks url_job_start_time_;
  base::TimeDelta before_unload_delay_;

  // Timestamps before_unload_(start|end)_ give the time it took to run
  // beforeunloads dispatched from the browser process. For browser-initated
  // navigations this includes all frames (all beforeunload handlers on a page).
  // For renderer-initated navigations this just includes OOPIFs since local
  // beforeunloads will have been run in the renderer before dispatching the
  // navigation IPC.
  base::Optional<base::TimeTicks> before_unload_start_;
  base::Optional<base::TimeTicks> before_unload_end_;

  // Time at which the browser process received a navigation request and
  // dispatched beforeunloads to the renderer.
  base::Optional<base::TimeTicks> before_unload_sent_;

  // Timestamps renderer_before_unload_(start|end)_ give the time it took to run
  // beforeunloads for local frames in a renderer-initiated navigation, prior to
  // notifying the browser process about the navigation.
  base::Optional<base::TimeTicks> renderer_before_unload_start_;
  base::Optional<base::TimeTicks> renderer_before_unload_end_;
};

Navigator::Navigator(NavigationControllerImpl* navigation_controller,
                     NavigatorDelegate* delegate)
    : controller_(navigation_controller), delegate_(delegate) {}

Navigator::~Navigator() = default;

// static
bool Navigator::CheckWebUIRendererDoesNotDisplayNormalURL(
    RenderFrameHostImpl* render_frame_host,
    const UrlInfo& url_info,
    bool is_renderer_initiated_check) {
  const GURL& url = url_info.url;
  // In single process mode, everything runs in the same process, so the checks
  // below are irrelevant.
  if (RenderProcessHost::run_renderer_in_process())
    return true;

  ChildProcessSecurityPolicyImpl* security_policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  ProcessLock process_lock =
      security_policy->GetProcessLock(render_frame_host->GetProcess()->GetID());

  // In the case of error page process, any URL is allowed to commit.
  if (process_lock == ProcessLock::CreateForErrorPage())
    return true;

  bool frame_has_bindings = ((render_frame_host->GetEnabledBindings() &
                              kWebUIBindingsPolicyMask) != 0);
  bool is_allowed_in_web_ui_renderer =
      WebUIControllerFactoryRegistry::GetInstance()->IsURLAcceptableForWebUI(
          render_frame_host->GetProcess()->GetBrowserContext(), url);

  // Embedders might disable locking for WebUI URLs, which is bad idea, however
  // this method should take this into account.
  SiteInstanceImpl* site_instance = render_frame_host->GetSiteInstance();
  SiteInfo site_info = SiteInstanceImpl::ComputeSiteInfo(
      site_instance->GetIsolationContext(), url_info,
      site_instance->IsCoopCoepCrossOriginIsolated(),
      site_instance->CoopCoepCrossOriginIsolatedOrigin());
  bool should_lock_process =
      SiteInstanceImpl::ShouldLockProcess(site_instance->GetIsolationContext(),
                                          site_info, site_instance->IsGuest());

  // If the |render_frame_host| has any WebUI bindings, disallow URLs that are
  // not allowed in a WebUI renderer process.
  if (frame_has_bindings) {
    // The process itself must have WebUI bit in the security policy.
    // Otherwise it indicates that there is a bug in browser process logic and
    // the browser process must be terminated.
    // TODO(nasko): Convert to CHECK() once it is confirmed this is not
    // violated in reality.
    if (!security_policy->HasWebUIBindings(
            render_frame_host->GetProcess()->GetID())) {
      base::debug::DumpWithoutCrashing();
    }

    // Check whether the process must be locked and if so that the process lock
    // is indeed in place.
    if (should_lock_process && !process_lock.is_locked_to_site())
      return false;

    // There must be a WebUI on the frame.
    if (!render_frame_host->web_ui())
      return false;

    // The |url| must be allowed in a WebUI process if the frame has WebUI.
    if (!is_allowed_in_web_ui_renderer) {
      // If this method is called in response to IPC message from the renderer
      // process, it should be terminated, otherwise it is a bug in the
      // navigation logic and the browser process should be terminated to avoid
      // exposing users to security issues.
      if (is_renderer_initiated_check)
        return false;

      CHECK(false);
    }
  }

  // If |url| is one that is allowed in WebUI renderer process, ensure that its
  // origin is either opaque or matches the origin of the process lock.
  if (is_allowed_in_web_ui_renderer) {
    url::Origin url_origin = url::Origin::Create(url.GetOrigin());

    // Verify |url| matches the origin of the process lock, if one is in place.
    if (should_lock_process) {
      if (!url_origin.opaque() && !process_lock.MatchesOrigin(url_origin))
        return false;
    }
  }

  return true;
}

// A renderer-initiated navigation should be ignored iff a) there is an ongoing
// request b) which is browser initiated and c) the renderer request is not
// user-initiated.
// static
bool Navigator::ShouldIgnoreIncomingRendererRequest(
    const NavigationRequest* ongoing_navigation_request,
    bool has_user_gesture) {
  return ongoing_navigation_request &&
         ongoing_navigation_request->browser_initiated() && !has_user_gesture;
}

NavigatorDelegate* Navigator::GetDelegate() {
  return delegate_;
}

NavigationController* Navigator::GetController() {
  return controller_;
}

void Navigator::DidFailLoadWithError(RenderFrameHostImpl* render_frame_host,
                                     const GURL& url,
                                     int error_code) {
  if (delegate_) {
    delegate_->DidFailLoadWithError(render_frame_host, url, error_code);
  }
}

bool Navigator::StartHistoryNavigationInNewSubframe(
    RenderFrameHostImpl* render_frame_host,
    mojo::PendingAssociatedRemote<mojom::NavigationClient>* navigation_client) {
  return controller_->StartHistoryNavigationInNewSubframe(render_frame_host,
                                                          navigation_client);
}

void Navigator::DidNavigate(
    RenderFrameHostImpl* render_frame_host,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    std::unique_ptr<NavigationRequest> navigation_request,
    bool was_within_same_document) {
  DCHECK(navigation_request);
  FrameTreeNode* frame_tree_node = render_frame_host->frame_tree_node();
  FrameTree* frame_tree = frame_tree_node->frame_tree();
  base::WeakPtr<RenderFrameHostImpl> old_frame_host =
      frame_tree_node->render_manager()->current_frame_host()->GetWeakPtr();

  bool is_same_document_navigation = controller_->IsURLSameDocumentNavigation(
      params.url, params.origin, was_within_same_document, render_frame_host);
  // If a frame claims the navigation was same-document, it must be the current
  // frame, not a pending one.
  if (is_same_document_navigation &&
      render_frame_host != old_frame_host.get()) {
    bad_message::ReceivedBadMessage(render_frame_host->GetProcess(),
                                    bad_message::NI_IN_PAGE_NAVIGATION);
    is_same_document_navigation = false;
  }
  // At this point we have already chosen a SiteInstance for this navigation, so
  // set |origin_requests_isolation| = false in the conversion to UrlInfo below.
  const UrlInfo url_info(params.url, false /* origin_requests_isolation */);
  bool is_cross_document_same_site_navigation =
      !is_same_document_navigation &&
      old_frame_host->IsNavigationSameSite(
          url_info,
          render_frame_host->GetSiteInstance()->IsCoopCoepCrossOriginIsolated(),
          render_frame_host->GetSiteInstance()
              ->CoopCoepCrossOriginIsolatedOrigin());
  if (is_cross_document_same_site_navigation) {
    UMA_HISTOGRAM_BOOLEAN(
        "BackForwardCache.ProactiveSameSiteBISwap.SameSiteNavigationDidSwap",
        navigation_request->did_same_site_proactive_browsing_instance_swap());
  }

  if (navigation_request->did_same_site_proactive_browsing_instance_swap()) {
    // If we did a same-site cross-BrowsingInstance main frame navigation, we
    // might be introducing a web-observable behavior change if we need to
    // unload the old frame (if we can't store the page in the back-forward
    // cache), because on normal same-site navigations the unloading of the old
    // RenderFrameHost happens before commit. We're measuring how often this
    // case happens to determine the risk of this change.
    DCHECK(old_frame_host.get());
    DCHECK_NE(old_frame_host.get(), render_frame_host);
    DCHECK(frame_tree_node->IsMainFrame());
    DCHECK(!old_frame_host->GetSiteInstance()->IsRelatedSiteInstance(
        render_frame_host->GetSiteInstance()));
    DCHECK(is_cross_document_same_site_navigation);
    bool can_store_in_back_forward_cache =
        controller_->GetBackForwardCache().CanStorePageNow(
            old_frame_host.get());
    UMA_HISTOGRAM_BOOLEAN(
        "BackForwardCache.ProactiveSameSiteBISwap.EligibilityDuringCommit",
        can_store_in_back_forward_cache);
    UMA_HISTOGRAM_BOOLEAN(
        "BackForwardCache.ProactiveSameSiteBISwap.UnloadRunsAfterCommit",
        !can_store_in_back_forward_cache &&
            old_frame_host->UnloadHandlerExistsInSameSiteInstanceSubtree());
  }

  if (auto& old_page_info = navigation_request->commit_params().old_page_info) {
    // This is a same-site main-frame navigation where we did a proactive
    // BrowsingInstance swap but we're reusing the old page's process, and we
    // have dispatched the pagehide and visibilitychange handlers of the old
    // page when we committed the new page.
    auto* page_lifecycle_state_manager =
        old_frame_host->render_view_host()->GetPageLifecycleStateManager();
    page_lifecycle_state_manager->DidSetPagehideDispatchDuringNewPageCommit(
        std::move(old_page_info->new_lifecycle_state_for_old_page));
  }

  if (ui::PageTransitionIsMainFrame(params.transition)) {
    if (delegate_) {
      // Run tasks that must execute just before the commit.
      delegate_->DidNavigateMainFramePreCommit(is_same_document_navigation);
    }
  }

  // For browser initiated navigation and same document navigation, frame policy
  // in commit_params is nullopt and should use fallback value instead.
  const blink::FramePolicy pending_frame_policy =
      navigation_request->commit_params().frame_policy.value_or(
          frame_tree_node->pending_frame_policy());

  // DidNavigateFrame() must be called before replicating the new origin and
  // other properties to proxies.  This is because it destroys the subframes of
  // the frame we're navigating from, which might trigger those subframes to
  // run unload handlers.  Those unload handlers should still see the old
  // frame's origin.  See https://crbug.com/825283.
  frame_tree_node->render_manager()->DidNavigateFrame(
      render_frame_host, params.gesture == NavigationGestureUser,
      is_same_document_navigation,
      navigation_request->coop_status()
          .require_browsing_instance_swap() /* clear_proxies_on_commit */,
      pending_frame_policy);

  // Save the new page's origin and other properties, and replicate them to
  // proxies, including the proxy created in DidNavigateFrame() to replace the
  // old frame in cross-process navigation cases.
  frame_tree_node->SetCurrentOrigin(
      params.origin, params.has_potentially_trustworthy_unique_origin);
  frame_tree_node->SetInsecureRequestPolicy(params.insecure_request_policy);
  frame_tree_node->SetInsecureNavigationsSet(params.insecure_navigations_set);

  // Save the activation status of the previous page here before it gets reset
  // in FrameTreeNode::ResetForNavigation.
  bool previous_document_was_activated =
      frame_tree->root()->HasStickyUserActivation();

  if (!is_same_document_navigation) {
    // Navigating to a new location means a new, fresh set of http headers
    // and/or <meta> elements - we need to reset CSP and Feature Policy.
    // However, if the navigation is restoring the given |render_frame_host|
    // from back-forward cache, it does not change the document in the given
    // RenderFrameHost and the existing Content Security Policy should be kept.
    if (!navigation_request->IsServedFromBackForwardCache())
      render_frame_host->ResetContentSecurityPolicies();

    auto reset_result = frame_tree_node->ResetForNavigation(
        navigation_request->IsServedFromBackForwardCache());

    // |old_frame_host| might get immediately deleted after the DidNavigateFrame
    // call above, so use weak pointer here.
    if (old_frame_host && old_frame_host->IsInBackForwardCache()) {
      if (reset_result.changed_frame_policy) {
        old_frame_host->EvictFromBackForwardCacheWithReason(
            BackForwardCacheMetrics::NotRestoredReason::
                kFrameTreeNodeStateReset);
      }
    }
  }

  // Update the site of the SiteInstance if it doesn't have one yet, unless
  // assigning a site is not necessary for this URL. In that case, the
  // SiteInstance can still be considered unused until a navigation to a real
  // page.
  SiteInstanceImpl* site_instance = render_frame_host->GetSiteInstance();
  if (!site_instance->HasSite() &&
      SiteInstanceImpl::ShouldAssignSiteForURL(url_info.url)) {
    site_instance->ConvertToDefaultOrSetSite(url_info);
  }

  // Need to update MIME type here because it's referred to in
  // UpdateNavigationCommands() called by RendererDidNavigate() to
  // determine whether or not to enable the encoding menu.
  // It's updated only for the main frame. For a subframe,
  // RenderView::UpdateURL does not set params.contents_mime_type.
  // (see http://code.google.com/p/chromium/issues/detail?id=2929 )
  // TODO(jungshik): Add a test for the encoding menu to avoid
  // regressing it again.
  // TODO(nasko): Verify the correctness of the above comment, since some of the
  // code doesn't exist anymore. Also, move this code in the
  // PageTransitionIsMainFrame code block above.
  if (ui::PageTransitionIsMainFrame(params.transition) && delegate_) {
    RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
        render_frame_host->GetRenderViewHost());
    rvh->SetContentsMimeType(params.contents_mime_type);
  }

  int old_entry_count = controller_->GetEntryCount();
  LoadCommittedDetails details;
  bool did_navigate = controller_->RendererDidNavigate(
      render_frame_host, params, &details, is_same_document_navigation,
      previous_document_was_activated, navigation_request.get());

  // If the history length and/or offset changed, update other renderers in the
  // FrameTree.
  if (old_entry_count != controller_->GetEntryCount() ||
      details.previous_entry_index !=
          controller_->GetLastCommittedEntryIndex()) {
    frame_tree->root()->render_manager()->SendPageMessage(
        new PageMsg_SetHistoryOffsetAndLength(
            MSG_ROUTING_NONE, controller_->GetLastCommittedEntryIndex(),
            controller_->GetEntryCount()),
        site_instance);
  }

  // Back-forward cache navigations do not create a new document.
  //
  // |was_within_same_document| (controlled by the renderer) also needs to be
  // considered: in some cases, the browser and renderer can disagree. While
  // this is usually a bad message kill, there are some situations where this
  // can legitimately happen. When a new frame is created (e.g. with
  // <iframe src="...">), the initial about:blank document doesn't have a
  // corresponding entry in the browser process. As a result, the browser
  // process incorrectly determines that the navigation is cross-document when
  // in reality it's same-document.
  //
  // TODO(crbug/1099264): Remove |was_within_same_document| from this logic
  // once all same-document navigations have a NavigationEntry. Once this
  // happens there should be no cases where the browser and renderer
  // legitimately disagree as described above.
  bool did_create_new_document =
      !navigation_request->IsServedFromBackForwardCache() &&
      !is_same_document_navigation && !was_within_same_document;

  render_frame_host->DidNavigate(params, navigation_request.get(),
                                 did_create_new_document);

  // Send notification about committed provisional loads. This notification is
  // different from the NAV_ENTRY_COMMITTED notification which doesn't include
  // the actual URL navigated to and isn't sent for AUTO_SUBFRAME navigations.
  if (details.type != NAVIGATION_TYPE_NAV_IGNORE && delegate_) {
    DCHECK_EQ(!render_frame_host->GetParent(),
              did_navigate ? details.is_main_frame : false);
    navigation_request->DidCommitNavigation(params, did_navigate,
                                            details.did_replace_entry,
                                            details.previous_url, details.type);
    navigation_request.reset();
  }

  if (!did_navigate)
    return;  // No navigation happened.

  // DO NOT ADD MORE STUFF TO THIS FUNCTION! Your component should either listen
  // for the appropriate notification (best) or you can add it to
  // DidNavigateMainFramePostCommit / DidNavigateAnyFramePostCommit (only if
  // necessary, please).

  // TODO(carlosk): Move this out.
  RecordNavigationMetrics(details, params, site_instance);

  // Run post-commit tasks.
  if (delegate_) {
    if (details.is_main_frame) {
      delegate_->DidNavigateMainFramePostCommit(render_frame_host, details,
                                                params);
    }

    delegate_->DidNavigateAnyFramePostCommit(render_frame_host, details,
                                             params);
  }
}

void Navigator::Navigate(std::unique_ptr<NavigationRequest> request,
                         ReloadType reload_type,
                         RestoreType restore_type) {
  TRACE_EVENT0("browser,navigation", "Navigator::Navigate");
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP0(
      "navigation,rail", "NavigationTiming navigationStart",
      TRACE_EVENT_SCOPE_GLOBAL, request->common_params().navigation_start);

  // Save destination url, as it is needed for
  // DidStartNavigationToPendingEntry and request could be destroyed after
  // BeginNavigation below.
  GURL dest_url = request->common_params().url;
  FrameTreeNode* frame_tree_node = request->frame_tree_node();

  navigation_data_ = std::make_unique<NavigationMetricsData>(
      request->common_params().navigation_start, request->common_params().url,
      frame_tree_node->current_frame_host()->GetPageUkmSourceId(),
      true /* is_browser_initiated_before_unload */, restore_type);

  // Check if the BeforeUnload event needs to execute before assigning the
  // NavigationRequest to the FrameTreeNode. Assigning it to the FrameTreeNode
  // has the side effect of initializing the current RenderFrameHost, which will
  // return that it should execute the BeforeUnload event (even though we don't
  // need to wait for it in the case of a brand new RenderFrameHost).
  //
  // We don't want to dispatch a beforeunload handler if
  // is_history_navigation_in_new_child is true. This indicates a newly created
  // child frame which does not have a beforeunload handler.
  bool should_dispatch_beforeunload =
      !NavigationTypeUtils::IsSameDocument(
          request->common_params().navigation_type) &&
      !request->common_params().is_history_navigation_in_new_child_frame &&
      frame_tree_node->current_frame_host()->ShouldDispatchBeforeUnload(
          false /* check_subframes_only */);

  int nav_entry_id = request->nav_entry_id();
  bool is_pending_entry =
      controller_->GetPendingEntry() &&
      (nav_entry_id == controller_->GetPendingEntry()->GetUniqueID());
  frame_tree_node->CreatedNavigationRequest(std::move(request));
  DCHECK(frame_tree_node->navigation_request());

  // Have the current renderer execute its beforeunload event if needed. If it
  // is not needed then NavigationRequest::BeginNavigation should be directly
  // called instead.
  if (should_dispatch_beforeunload) {
    frame_tree_node->navigation_request()->SetWaitingForRendererResponse();
    frame_tree_node->current_frame_host()->DispatchBeforeUnload(
        RenderFrameHostImpl::BeforeUnloadType::BROWSER_INITIATED_NAVIGATION,
        reload_type != ReloadType::NONE);
  } else {
    frame_tree_node->navigation_request()->BeginNavigation();
    // WARNING: The NavigationRequest might have been destroyed in
    // BeginNavigation(). Do not use |frame_tree_node->navigation_request()|
    // after this point without null checking it first.
  }

  // Make sure no code called via RFH::Navigate clears the pending entry.
  if (is_pending_entry)
    CHECK_EQ(nav_entry_id, controller_->GetPendingEntry()->GetUniqueID());
}

void Navigator::RequestOpenURL(
    RenderFrameHostImpl* render_frame_host,
    const GURL& url,
    const GlobalFrameRoutingId& initiator_routing_id,
    const base::Optional<url::Origin>& initiator_origin,
    const scoped_refptr<network::ResourceRequestBody>& post_body,
    const std::string& extra_headers,
    const Referrer& referrer,
    WindowOpenDisposition disposition,
    bool should_replace_current_entry,
    bool user_gesture,
    blink::TriggeringEventInfo triggering_event_info,
    const std::string& href_translate,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    const base::Optional<Impression>& impression) {
  // Note: This can be called for subframes (even when OOPIFs are not possible)
  // if the disposition calls for a different window.

  // Only the current RenderFrameHost should be sending an OpenURL request.
  // Pending RenderFrameHost should know where it is navigating and pending
  // deletion RenderFrameHost shouldn't be trying to navigate.
  if (render_frame_host !=
      render_frame_host->frame_tree_node()->current_frame_host()) {
    return;
  }

  SiteInstance* current_site_instance = render_frame_host->GetSiteInstance();

  // TODO(creis): Pass the redirect_chain into this method to support client
  // redirects.  http://crbug.com/311721.
  std::vector<GURL> redirect_chain;

  int frame_tree_node_id = -1;

  // Send the navigation to the current FrameTreeNode if it's destined for a
  // subframe in the current tab.  We'll assume it's for the main frame
  // (possibly of a new or different WebContents) otherwise.
  if (disposition == WindowOpenDisposition::CURRENT_TAB &&
      render_frame_host->GetParent()) {
    frame_tree_node_id =
        render_frame_host->frame_tree_node()->frame_tree_node_id();
  }

  OpenURLParams params(url, referrer, frame_tree_node_id, disposition,
                       ui::PAGE_TRANSITION_LINK,
                       true /* is_renderer_initiated */);
  params.post_data = post_body;
  params.extra_headers = extra_headers;
  if (redirect_chain.size() > 0)
    params.redirect_chain = redirect_chain;
  params.should_replace_current_entry = should_replace_current_entry;
  params.user_gesture = user_gesture;
  params.triggering_event_info = triggering_event_info;
  params.initiator_origin = initiator_origin;
  params.initiator_routing_id = initiator_routing_id;

  // RequestOpenURL is used only for local frames, so we can get here only if
  // the navigation is initiated by a frame in the same SiteInstance as this
  // frame.  Note that navigations on RenderFrameProxies do not use
  // RequestOpenURL and go through NavigateFromFrameProxy instead.
  params.source_site_instance = current_site_instance;

  params.source_render_frame_id = render_frame_host->GetRoutingID();
  params.source_render_process_id = render_frame_host->GetProcess()->GetID();

  if (render_frame_host->web_ui()) {
    // Note that we hide the referrer for Web UI pages. We don't really want
    // web sites to see a referrer of "chrome://blah" (and some chrome: URLs
    // might have search terms or other stuff we don't want to send to the
    // site), so we send no referrer.
    params.referrer = Referrer();

    // Navigations in Web UI pages count as browser-initiated navigations.
    params.is_renderer_initiated = false;
  }

  params.blob_url_loader_factory = std::move(blob_url_loader_factory);
  params.href_translate = href_translate;
  params.impression = impression;

  if (delegate_)
    delegate_->OpenURL(params);
}

void Navigator::NavigateFromFrameProxy(
    RenderFrameHostImpl* render_frame_host,
    const GURL& url,
    const GlobalFrameRoutingId& initiator_routing_id,
    const url::Origin& initiator_origin,
    SiteInstance* source_site_instance,
    const Referrer& referrer,
    ui::PageTransition page_transition,
    bool should_replace_current_entry,
    NavigationDownloadPolicy download_policy,
    const std::string& method,
    scoped_refptr<network::ResourceRequestBody> post_body,
    const std::string& extra_headers,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    bool has_user_gesture,
    const base::Optional<Impression>& impression) {
  // |method != "POST"| should imply absence of |post_body|.
  if (method != "POST" && post_body) {
    NOTREACHED();
    post_body = nullptr;
  }

  // Allow the delegate to cancel the transfer.
  if (!delegate_->ShouldTransferNavigation(
          render_frame_host->frame_tree_node()->IsMainFrame()))
    return;

  // TODO(creis): Determine if this transfer started as a browser-initiated
  // navigation.  See https://crbug.com/495161.
  bool is_renderer_initiated = true;
  Referrer referrer_to_use(referrer);
  if (render_frame_host->web_ui()) {
    // Note that we hide the referrer for Web UI pages. We don't really want
    // web sites to see a referrer of "chrome://blah" (and some chrome: URLs
    // might have search terms or other stuff we don't want to send to the
    // site), so we send no referrer.
    referrer_to_use = Referrer();

    // Navigations in Web UI pages count as browser-initiated navigations.
    is_renderer_initiated = false;
  }

  if (is_renderer_initiated &&
      ShouldIgnoreIncomingRendererRequest(
          render_frame_host->frame_tree_node()->navigation_request(),
          has_user_gesture)) {
    return;
  }

  controller_->NavigateFromFrameProxy(
      render_frame_host, url, initiator_routing_id, initiator_origin,
      is_renderer_initiated, source_site_instance, referrer_to_use,
      page_transition, should_replace_current_entry, download_policy, method,
      post_body, extra_headers, std::move(blob_url_loader_factory), impression);
}

void Navigator::BeforeUnloadCompleted(FrameTreeNode* frame_tree_node,
                                      bool proceed,
                                      const base::TimeTicks& proceed_time) {
  DCHECK(frame_tree_node);

  NavigationRequest* navigation_request = frame_tree_node->navigation_request();

  // The NavigationRequest may have been canceled while the renderer was
  // executing the BeforeUnload event.
  if (!navigation_request)
    return;

  // If the user chose not to proceed, cancel the ongoing navigation.
  // Note: it might be a new navigation, and not the one that triggered the
  // sending of the BeforeUnload IPC in the first place. However, the
  // BeforeUnload where the user asked not to proceed will have taken place
  // after the navigation started. The last user input should be respected, and
  // the navigation cancelled anyway.
  if (!proceed) {
    CancelNavigation(frame_tree_node);
    return;
  }

  // The browser-initiated NavigationRequest that triggered the sending of the
  // BeforeUnload IPC might have been replaced by a renderer-initiated one while
  // the BeforeUnload event executed in the renderer. In that case, the request
  // will already have begun, so there is no need to start it again.
  if (navigation_request->state() >
      NavigationRequest::WAITING_FOR_RENDERER_RESPONSE) {
    DCHECK(navigation_request->from_begin_navigation());
    return;
  }

  // Update the navigation start: it should be when it was determined that the
  // navigation will proceed.
  navigation_request->set_navigation_start_time(proceed_time);

  DCHECK_EQ(NavigationRequest::WAITING_FOR_RENDERER_RESPONSE,
            navigation_request->state());

  // Send the request to the IO thread.
  navigation_request->BeginNavigation();
  // DO NOT USE |navigation_request| BEYOND THIS POINT. It might have been
  // destroyed in BeginNavigation().
  // See https://crbug.com/770157.
}

void Navigator::OnBeginNavigation(
    FrameTreeNode* frame_tree_node,
    mojom::CommonNavigationParamsPtr common_params,
    mojom::BeginNavigationParamsPtr begin_params,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
    mojo::PendingRemote<blink::mojom::NavigationInitiator> navigation_initiator,
    scoped_refptr<PrefetchedSignedExchangeCache>
        prefetched_signed_exchange_cache,
    std::unique_ptr<WebBundleHandleTracker> web_bundle_handle_tracker) {
  // TODO(clamy): the url sent by the renderer should be validated with
  // FilterURL.
  // This is a renderer-initiated navigation.
  DCHECK(frame_tree_node);

  if (common_params->is_history_navigation_in_new_child_frame) {
    // Try to find a FrameNavigationEntry that matches this frame instead, based
    // on the frame's unique name.  If this can't be found, fall back to the
    // default path below.
    if (frame_tree_node->navigator().StartHistoryNavigationInNewSubframe(
            frame_tree_node->current_frame_host(), &navigation_client)) {
      return;
    }
  }

  NavigationRequest* ongoing_navigation_request =
      frame_tree_node->navigation_request();

  // Client redirects during the initial history navigation of a child frame
  // should take precedence over the history navigation (despite being renderer-
  // initiated).  See https://crbug.com/348447 and https://crbug.com/691168.
  if (ongoing_navigation_request &&
      ongoing_navigation_request->common_params()
          .is_history_navigation_in_new_child_frame) {
    // Preemptively clear this local pointer before deleting the request.
    ongoing_navigation_request = nullptr;
    frame_tree_node->ResetNavigationRequest(false);
  }

  // Verify this navigation has precedence.
  if (ShouldIgnoreIncomingRendererRequest(ongoing_navigation_request,
                                          common_params->has_user_gesture)) {
    return;
  }

  NavigationEntryImpl* navigation_entry =
      GetNavigationEntryForRendererInitiatedNavigation(*common_params,
                                                       frame_tree_node);
  const bool override_user_agent =
      delegate_ &&
      delegate_->ShouldOverrideUserAgentForRendererInitiatedNavigation();
  frame_tree_node->CreatedNavigationRequest(
      NavigationRequest::CreateRendererInitiated(
          frame_tree_node, navigation_entry, std::move(common_params),
          std::move(begin_params), controller_->GetLastCommittedEntryIndex(),
          controller_->GetEntryCount(), override_user_agent,
          std::move(blob_url_loader_factory), std::move(navigation_client),
          std::move(navigation_initiator),
          std::move(prefetched_signed_exchange_cache),
          std::move(web_bundle_handle_tracker)));
  NavigationRequest* navigation_request = frame_tree_node->navigation_request();

  navigation_data_ = std::make_unique<NavigationMetricsData>(
      navigation_request->common_params().navigation_start,
      navigation_request->common_params().url,
      frame_tree_node->current_frame_host()->GetPageUkmSourceId(),
      false /* is_browser_initiated_before_unload */, RestoreType::NONE);

  LogRendererInitiatedBeforeUnloadTime(
      navigation_request->begin_params()->before_unload_start,
      navigation_request->begin_params()->before_unload_end);

  // This frame has already run beforeunload before it sent this IPC.  See if
  // any of its cross-process subframes also need to run beforeunload.  If so,
  // delay the navigation until beforeunload completion callbacks are invoked on
  // those frames.
  DCHECK(!NavigationTypeUtils::IsSameDocument(
      navigation_request->common_params().navigation_type));
  bool should_dispatch_beforeunload =
      frame_tree_node->current_frame_host()->ShouldDispatchBeforeUnload(
          true /* check_subframes_only */);
  if (should_dispatch_beforeunload) {
    frame_tree_node->navigation_request()->SetWaitingForRendererResponse();
    frame_tree_node->current_frame_host()->DispatchBeforeUnload(
        RenderFrameHostImpl::BeforeUnloadType::RENDERER_INITIATED_NAVIGATION,
        NavigationTypeUtils::IsReload(
            navigation_request->common_params().navigation_type));
    return;
  }

  // For main frames, NavigationHandle will be created after the call to
  // |DidStartMainFrameNavigation|, so it receives the most up to date pending
  // entry from the NavigationController.
  navigation_request->BeginNavigation();
  // DO NOT USE |navigation_request| BEYOND THIS POINT. It might have been
  // destroyed in BeginNavigation().
  // See https://crbug.com/770157.
}

void Navigator::RestartNavigationAsCrossDocument(
    std::unique_ptr<NavigationRequest> navigation_request) {
  FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
  // Don't restart the navigation if there is already another ongoing navigation
  // in the FrameTreeNode.
  if (frame_tree_node->navigation_request())
    return;

  navigation_request->ResetForCrossDocumentRestart();
  frame_tree_node->CreatedNavigationRequest(std::move(navigation_request));
  frame_tree_node->navigation_request()->BeginNavigation();
  // DO NOT USE THE NAVIGATION REQUEST BEYOND THIS POINT. It might have been
  // destroyed in BeginNavigation().
  // See https://crbug.com/770157.
}

void Navigator::CancelNavigation(FrameTreeNode* frame_tree_node) {
  if (frame_tree_node->navigation_request())
    frame_tree_node->navigation_request()->set_net_error(net::ERR_ABORTED);
  frame_tree_node->ResetNavigationRequest(false);
  if (frame_tree_node->IsMainFrame())
    navigation_data_.reset();
}

void Navigator::LogResourceRequestTime(base::TimeTicks timestamp,
                                       const GURL& url) {
  if (navigation_data_ && navigation_data_->url_ == url) {
    navigation_data_->url_job_start_time_ = timestamp;
    UMA_HISTOGRAM_TIMES(
        "Navigation.TimeToURLJobStart",
        navigation_data_->url_job_start_time_ - navigation_data_->start_time_);
  }
}

void Navigator::LogBeforeUnloadTime(
    base::TimeTicks renderer_before_unload_start_time,
    base::TimeTicks renderer_before_unload_end_time,
    base::TimeTicks before_unload_sent_time) {
  if (!navigation_data_)
    return;

  // Only stores the beforeunload delay if we're tracking a browser initiated
  // navigation and it happened later than the navigation request.
  if (navigation_data_->is_browser_initiated_before_unload_ &&
      renderer_before_unload_start_time > navigation_data_->start_time_) {
    navigation_data_->before_unload_delay_ =
        renderer_before_unload_end_time - renderer_before_unload_start_time;
  }
  // LogBeforeUnloadTime is called once for each cross-process frame. Once all
  // beforeunloads complete, the timestamps in navigation_data will be the
  // timestamps of the beforeunload that blocked the navigation the longest.
  if (!base::TimeTicks::IsConsistentAcrossProcesses()) {
    // These timestamps come directly from the renderer so they might need to be
    // converted to local time stamps.
    InterProcessTimeTicksConverter converter(
        LocalTimeTicks::FromTimeTicks(before_unload_sent_time),
        LocalTimeTicks::FromTimeTicks(base::TimeTicks::Now()),
        RemoteTimeTicks::FromTimeTicks(renderer_before_unload_start_time),
        RemoteTimeTicks::FromTimeTicks(renderer_before_unload_end_time));
    LocalTimeTicks converted_renderer_before_unload_start =
        converter.ToLocalTimeTicks(
            RemoteTimeTicks::FromTimeTicks(renderer_before_unload_start_time));
    LocalTimeTicks converted_renderer_before_unload_end =
        converter.ToLocalTimeTicks(
            RemoteTimeTicks::FromTimeTicks(renderer_before_unload_end_time));
    navigation_data_->before_unload_start_ =
        converted_renderer_before_unload_start.ToTimeTicks();
    navigation_data_->before_unload_end_ =
        converted_renderer_before_unload_end.ToTimeTicks();
  } else {
    navigation_data_->before_unload_start_ = renderer_before_unload_start_time;
    navigation_data_->before_unload_end_ = renderer_before_unload_end_time;
  }
  navigation_data_->before_unload_sent_ = before_unload_sent_time;
}

void Navigator::LogRendererInitiatedBeforeUnloadTime(
    base::TimeTicks renderer_before_unload_start_time,
    base::TimeTicks renderer_before_unload_end_time) {
  DCHECK(navigation_data_);

  if (renderer_before_unload_start_time == base::TimeTicks() ||
      renderer_before_unload_end_time == base::TimeTicks())
    return;

  if (!base::TimeTicks::IsConsistentAcrossProcesses()) {
    // These timestamps come directly from the renderer so they might need to be
    // converted to local time stamps.
    InterProcessTimeTicksConverter converter(
        LocalTimeTicks::FromTimeTicks(base::TimeTicks()),
        LocalTimeTicks::FromTimeTicks(base::TimeTicks::Now()),
        RemoteTimeTicks::FromTimeTicks(renderer_before_unload_start_time),
        RemoteTimeTicks::FromTimeTicks(renderer_before_unload_end_time));
    LocalTimeTicks converted_renderer_before_unload_start =
        converter.ToLocalTimeTicks(
            RemoteTimeTicks::FromTimeTicks(renderer_before_unload_start_time));
    LocalTimeTicks converted_renderer_before_unload_end =
        converter.ToLocalTimeTicks(
            RemoteTimeTicks::FromTimeTicks(renderer_before_unload_end_time));
    navigation_data_->renderer_before_unload_start_ =
        converted_renderer_before_unload_start.ToTimeTicks();
    navigation_data_->renderer_before_unload_end_ =
        converted_renderer_before_unload_end.ToTimeTicks();
  } else {
    navigation_data_->renderer_before_unload_start_ =
        renderer_before_unload_start_time;
    navigation_data_->renderer_before_unload_end_ =
        renderer_before_unload_end_time;
  }
}

void Navigator::RecordNavigationMetrics(
    const LoadCommittedDetails& details,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    SiteInstance* site_instance) {
  DCHECK(site_instance->HasProcess());

  if (!details.is_main_frame || !navigation_data_ ||
      navigation_data_->url_job_start_time_.is_null() ||
      navigation_data_->url_ != params.original_request_url) {
    return;
  }

  ukm::builders::Unload builder(navigation_data_->ukm_source_id_);

  if (navigation_data_->is_browser_initiated_before_unload_) {
    base::TimeDelta time_to_commit =
        base::TimeTicks::Now() - navigation_data_->start_time_;
    UMA_HISTOGRAM_TIMES("Navigation.TimeToCommit", time_to_commit);

    time_to_commit -= navigation_data_->before_unload_delay_;
    base::TimeDelta time_to_network = navigation_data_->url_job_start_time_ -
                                      navigation_data_->start_time_ -
                                      navigation_data_->before_unload_delay_;
    if (navigation_data_->is_restoring_from_last_session_) {
      UMA_HISTOGRAM_TIMES(
          "Navigation.TimeToCommit_SessionRestored_BeforeUnloadDiscounted",
          time_to_commit);
      UMA_HISTOGRAM_TIMES(
          "Navigation.TimeToURLJobStart_SessionRestored_BeforeUnloadDiscounted",
          time_to_network);
      navigation_data_.reset();
      return;
    }
    bool navigation_created_new_renderer_process =
        site_instance->GetProcess()->GetInitTimeForNavigationMetrics() >
        navigation_data_->start_time_;
    if (navigation_created_new_renderer_process) {
      UMA_HISTOGRAM_TIMES(
          "Navigation.TimeToCommit_NewRenderer_BeforeUnloadDiscounted",
          time_to_commit);
      UMA_HISTOGRAM_TIMES(
          "Navigation.TimeToURLJobStart_NewRenderer_BeforeUnloadDiscounted",
          time_to_network);
    } else {
      UMA_HISTOGRAM_TIMES(
          "Navigation.TimeToCommit_ExistingRenderer_BeforeUnloadDiscounted",
          time_to_commit);
      UMA_HISTOGRAM_TIMES(
          "Navigation.TimeToURLJobStart_ExistingRenderer_"
          "BeforeUnloadDiscounted",
          time_to_network);
    }
    if (navigation_data_->before_unload_start_ &&
        navigation_data_->before_unload_end_) {
      builder.SetBeforeUnloadDuration(
          (navigation_data_->before_unload_end_.value() -
           navigation_data_->before_unload_start_.value())
              .InMilliseconds());
    }
  } else {
    if (navigation_data_->renderer_before_unload_start_ &&
        navigation_data_->renderer_before_unload_end_) {
      base::TimeDelta before_unload_duration =
          navigation_data_->renderer_before_unload_end_.value() -
          navigation_data_->renderer_before_unload_start_.value();

      // If we had to dispatch beforeunload handlers for OOPIFs from the
      // browser, add those into the beforeunload duration as they contributed
      // to the total beforeunload latency.
      if (navigation_data_->before_unload_sent_) {
        before_unload_duration +=
            navigation_data_->before_unload_end_.value() -
            navigation_data_->before_unload_start_.value();
      }
      builder.SetBeforeUnloadDuration(before_unload_duration.InMilliseconds());
    }
  }

  // Records the queuing duration of the beforeunload sent from the browser to
  // the frame that blocked the navigation the longest. This can happen in a
  // renderer or browser initiated navigation and could mean a long queuing time
  // blocked the navigation or a long beforeunload. Records nothing if none were
  // sent.
  if (navigation_data_->before_unload_sent_) {
    builder.SetBeforeUnloadQueueingDuration(
        (navigation_data_->before_unload_start_.value() -
         navigation_data_->before_unload_sent_.value())
            .InMilliseconds());
  }

  builder.Record(ukm::UkmRecorder::Get());
  navigation_data_.reset();
}

NavigationEntryImpl*
Navigator::GetNavigationEntryForRendererInitiatedNavigation(
    const mojom::CommonNavigationParams& common_params,
    FrameTreeNode* frame_tree_node) {
  if (!frame_tree_node->IsMainFrame())
    return nullptr;

  // If there is no browser-initiated pending entry for this navigation and it
  // is not for the error URL, create a pending entry and ensure the address bar
  // updates accordingly.  We don't know the referrer or extra headers at this
  // point, but the referrer will be set properly upon commit.  This does not
  // set the SiteInstance for the pending entry, because it may change
  // before the URL commits.
  NavigationEntryImpl* pending_entry = controller_->GetPendingEntry();
  bool has_browser_initiated_pending_entry =
      pending_entry && !pending_entry->is_renderer_initiated();
  if (has_browser_initiated_pending_entry)
    return nullptr;

  // A pending navigation entry is created in OnBeginNavigation(). The renderer
  // sends a provisional load notification after that. We don't want to create
  // a duplicate navigation entry here.
  bool renderer_provisional_load_to_pending_url =
      pending_entry && pending_entry->is_renderer_initiated() &&
      (pending_entry->GetURL() == common_params.url);
  if (renderer_provisional_load_to_pending_url)
    return nullptr;

  // Since GetNavigationEntryForRendererInitiatedNavigation is called from
  // OnBeginNavigation, we can assume that no frame proxies are involved and
  // therefore that |current_site_instance| is also the |source_site_instance|.
  SiteInstance* current_site_instance =
      frame_tree_node->current_frame_host()->GetSiteInstance();
  SiteInstance* source_site_instance = current_site_instance;

  std::unique_ptr<NavigationEntryImpl> entry =
      NavigationEntryImpl::FromNavigationEntry(
          NavigationControllerImpl::CreateNavigationEntry(
              common_params.url, content::Referrer(),
              common_params.initiator_origin, source_site_instance,
              ui::PAGE_TRANSITION_LINK, true /* is_renderer_initiated */,
              std::string() /* extra_headers */,
              controller_->GetBrowserContext(),
              nullptr /* blob_url_loader_factory */,
              common_params.should_replace_current_entry,
              controller_->GetWebContents()));
  entry->set_reload_type(NavigationRequest::NavigationTypeToReloadType(
      common_params.navigation_type));

  controller_->SetPendingEntry(std::move(entry));
  if (delegate_)
    delegate_->NotifyChangedNavigationState(content::INVALIDATE_TYPE_URL);

  return controller_->GetPendingEntry();
}

}  // namespace content
