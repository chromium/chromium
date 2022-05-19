// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_host.h"

#include "base/feature_list.h"
#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_conversion_helper.h"
#include "base/trace_event/typed_macros.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/prerender/prerender_host_registry.h"
#include "content/browser/prerender/prerender_metrics.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_restore_context_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_info.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/referrer.h"
#include "net/base/load_flags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"

namespace content {

namespace {

bool AreHttpRequestHeadersCompatible(
    const std::string& potential_activation_headers_str,
    const std::string& prerender_headers_str) {
  net::HttpRequestHeaders prerender_headers;
  prerender_headers.AddHeadersFromString(prerender_headers_str);

  net::HttpRequestHeaders potential_activation_headers;
  potential_activation_headers.AddHeadersFromString(
      potential_activation_headers_str);

  // `prerender_headers` contains the "Purpose: prefetch" and "Sec-Purpose:
  // prefetch;prerender" to notify servers of prerender requests, while
  // `potential_activation_headers` doesn't contain it. Remove "Purpose" and
  // "Sec-Purpose" matching from consideration so that activation works with the
  // header.
  prerender_headers.RemoveHeader("Purpose");
  potential_activation_headers.RemoveHeader("Purpose");
  prerender_headers.RemoveHeader("Sec-Purpose");
  potential_activation_headers.RemoveHeader("Sec-Purpose");

  return prerender_headers.ToString() ==
         potential_activation_headers.ToString();
}

}  // namespace

class PrerenderHost::PageHolder : public FrameTree::Delegate,
                                  public NavigationControllerDelegate {
 public:
  explicit PageHolder(WebContentsImpl& web_contents)
      : web_contents_(web_contents),
        frame_tree_(
            std::make_unique<FrameTree>(web_contents.GetBrowserContext(),
                                        this,
                                        this,
                                        &web_contents,
                                        &web_contents,
                                        &web_contents,
                                        &web_contents,
                                        &web_contents,
                                        &web_contents,
                                        FrameTree::Type::kPrerender)) {
    scoped_refptr<SiteInstance> site_instance =
        SiteInstance::Create(web_contents.GetBrowserContext());
    frame_tree_->Init(site_instance.get(),
                      /*renderer_initiated_creation=*/false,
                      /*main_frame_name=*/"", /*opener=*/nullptr,
                      /*frame_policy=*/blink::FramePolicy());

    // Use the same SessionStorageNamespace as the primary page for the
    // prerendering page.
    frame_tree_->controller().SetSessionStorageNamespace(
        site_instance->GetStoragePartitionConfig(),
        web_contents_.GetPrimaryFrameTree()
            .controller()
            .GetSessionStorageNamespace(
                site_instance->GetStoragePartitionConfig()));

    // TODO(https://crbug.com/1199679): This should be moved to FrameTree::Init
    web_contents_.NotifySwappedFromRenderManager(
        /*old_frame=*/nullptr,
        frame_tree_->root()->render_manager()->current_frame_host());
  }

  ~PageHolder() override {
    // If we are still waiting on test loop, we can assume the page loading step
    // has been cancelled and the PageHolder is being discarded without
    // completing loading the page.
    if (on_wait_loading_finished_)
      std::move(on_wait_loading_finished_)
          .Run(PrerenderHost::LoadingOutcome::kPrerenderingCancelled);

    if (frame_tree_)
      frame_tree_->Shutdown();
  }

  // FrameTree::Delegate

  // TODO(https://crbug.com/1199682): Correctly handle load events. Ignored for
  // now as it confuses WebContentsObserver instances because they can not
  // distinguish between the different FrameTrees.

  void DidStartLoading(FrameTreeNode* frame_tree_node,
                       bool should_show_loading_ui) override {}

  void DidStopLoading() override {
    if (on_wait_loading_finished_) {
      std::move(on_wait_loading_finished_)
          .Run(PrerenderHost::LoadingOutcome::kLoadingCompleted);
    }
  }

  void DidChangeLoadProgress() override {}
  bool IsHidden() override { return true; }
  FrameTree* LoadingTree() override {
    // For prerendering loading tree is the same as its frame tree as loading is
    // done at a frame tree level in the background, unlike the loading visible
    // to the user where we account for nested frame tree loading state.
    return frame_tree_.get();
  }
  void NotifyPageChanged(PageImpl& page) override {}
  int GetOuterDelegateFrameTreeNodeId() override {
    // A prerendered FrameTree is not "inner to" or "nested inside" another
    // FrameTree; it exists in parallel to the primary FrameTree of the current
    // WebContents. Therefore, it must not attempt to access the primary
    // FrameTree in the sense of an "outer delegate" relationship, so we return
    // the invalid ID here.
    return FrameTreeNode::kFrameTreeNodeInvalidId;
  }
  bool IsPortal() override { return false; }

  // NavigationControllerDelegate
  void NotifyNavigationStateChanged(InvalidateTypes changed_flags) override {}
  void NotifyBeforeFormRepostWarningShow() override {}
  void NotifyNavigationEntryCommitted(
      const LoadCommittedDetails& load_details) override {}
  void NotifyNavigationEntryChanged(
      const EntryChangedDetails& change_details) override {}
  void NotifyNavigationListPruned(
      const PrunedDetails& pruned_details) override {}
  void NotifyNavigationEntriesDeleted() override {}
  void ActivateAndShowRepostFormWarningDialog() override {
    // Not supported, cancel pending reload.
    GetNavigationController().CancelPendingReload();
  }
  bool ShouldPreserveAbortedURLs() override { return false; }
  WebContents* DeprecatedGetWebContents() override { return GetWebContents(); }
  void UpdateOverridingUserAgent() override {}

  NavigationControllerImpl& GetNavigationController() {
    return frame_tree_->controller();
  }

  WebContents* GetWebContents() { return &web_contents_; }

  FrameTree& GetPrimaryFrameTree() {
    return web_contents_.GetPrimaryFrameTree();
  }

  std::unique_ptr<StoredPage> Activate(NavigationRequest& navigation_request) {
    // There should be no ongoing main-frame navigation during activation.
    // TODO(https://crbug.com/1190644): Make sure sub-frame navigations are
    // fine.
    DCHECK(!frame_tree_->root()->HasNavigation());

    // Before the root's current_frame_host is cleared, collect the subframes of
    // `frame_tree_` whose FrameTree will need to be updated.
    FrameTree::NodeRange node_range = frame_tree_->Nodes();
    std::vector<FrameTreeNode*> subframe_nodes(std::next(node_range.begin()),
                                               node_range.end());

    // Before the root's current_frame_host is cleared, collect the replication
    // state so that it can be used for post-activation validation.
    blink::mojom::FrameReplicationState prior_replication_state =
        frame_tree_->root()->current_replication_state();

    // Update FrameReplicationState::has_received_user_gesture_before_nav of the
    // prerendered page.
    //
    // On regular navigation, it is updated via a renderer => browser IPC
    // (RenderFrameHostImpl::HadStickyUserActivationBeforeNavigationChanged),
    // which is sent from blink::DocumentLoader::CommitNavigation. However,
    // this doesn't happen on prerender page activation, so the value is not
    // correctly updated without this treatment.
    //
    // The updated value will be sent to the renderer on
    // blink::mojom::Page::ActivatePrerenderedPage.
    prior_replication_state.has_received_user_gesture_before_nav =
        navigation_request.frame_tree_node()
            ->has_received_user_gesture_before_nav();

    // frame_tree_->root(). Do not add any code between here and
    // frame_tree_.reset() that calls into observer functions to minimize the
    // duration of current_frame_host being null.
    //
    // TODO(https://crbug.com/1176148): Investigate how to combine taking the
    // prerendered page and frame_tree_ destruction.
    std::unique_ptr<StoredPage> page =
        frame_tree_->root()->render_manager()->TakePrerenderedPage();

    std::unique_ptr<NavigationEntryRestoreContextImpl> context =
        std::make_unique<NavigationEntryRestoreContextImpl>();
    std::unique_ptr<NavigationEntryImpl> nav_entry =
        GetNavigationController()
            .GetEntryWithUniqueID(page->render_frame_host->nav_entry_id())
            ->CloneWithoutSharing(context.get());

    navigation_request.SetPrerenderActivationNavigationState(
        std::move(nav_entry), prior_replication_state);

    FrameTree& target_frame_tree = GetPrimaryFrameTree();
    DCHECK_EQ(&target_frame_tree,
              navigation_request.frame_tree_node()->frame_tree());

    // We support activating the prerenderd page only to the topmost
    // RenderFrameHost.
    CHECK(!page->render_frame_host->GetParentOrOuterDocumentOrEmbedder());

    page->render_frame_host->SetFrameTreeNode(*(target_frame_tree.root()));
    // Copy frame name into the replication state of the primary main frame to
    // ensure that the replication state of the primary main frame after
    // activation matches the replication state stored in the renderer.
    // TODO(https://crbug.com/1237091): Copying frame name here is suboptimal
    // and ideally we'd do this at the same time when transferring the proxies
    // from the StoredPage into RenderFrameHostManager. However, this is a
    // temporary solution until we move this into BrowsingContextState,
    // along with RenderFrameProxyHost.
    page->render_frame_host->frame_tree_node()->set_frame_name_for_activation(
        prior_replication_state.unique_name, prior_replication_state.name);
    for (auto& it : page->proxy_hosts) {
      it.second->set_frame_tree_node(*(target_frame_tree.root()));
    }

    // Iterate over the root RenderFrameHost's subframes and update the
    // associated frame tree. Note that subframe proxies don't need their
    // FrameTrees independently updated, since their FrameTreeNodes don't
    // change, and FrameTree references in those FrameTreeNodes will be updated
    // through RenderFrameHosts.
    //
    // TODO(https://crbug.com/1199693): Need to investigate if and how
    // pending delete RenderFrameHost objects should be handled if prerendering
    // runs all of the unload handlers; they are not currently handled here.
    // This is because pending delete RenderFrameHosts can still receive and
    // process some messages while the RenderFrameHost FrameTree and
    // FrameTreeNode are stale.
    for (FrameTreeNode* subframe_node : subframe_nodes) {
      subframe_node->SetFrameTree(target_frame_tree);
    }

    page->render_frame_host->ForEachRenderFrameHostIncludingSpeculative(
        base::BindRepeating(
            [](const WebContentsImpl& web_contents, RenderFrameHostImpl* rfh) {
              // The visibility state of the prerendering page has not been
              // updated by
              // WebContentsImpl::UpdateVisibilityAndNotifyPageAndView(). So
              // updates the visibility state using the PageVisibilityState of
              // |web_contents|.
              rfh->render_view_host()->SetFrameTreeVisibility(
                  web_contents.GetPageVisibilityState());
            },
            std::cref(web_contents_)));

    frame_tree_->Shutdown();
    frame_tree_.reset();

    return page;
  }

  PrerenderHost::LoadingOutcome WaitForLoadCompletionForTesting() {
    PrerenderHost::LoadingOutcome status =
        PrerenderHost::LoadingOutcome::kLoadingCompleted;
    if (!frame_tree_->IsLoading())
      return status;

    base::RunLoop loop;
    on_wait_loading_finished_ =
        base::BindOnce(&PrerenderHost::PageHolder::FinishWaitingForTesting,
                       loop.QuitClosure(), &status);
    loop.Run();
    return status;
  }

  FrameTree* frame_tree() { return frame_tree_.get(); }

 private:
  static void FinishWaitingForTesting(base::OnceClosure on_close,  // IN-TEST
                                      PrerenderHost::LoadingOutcome* result,
                                      PrerenderHost::LoadingOutcome status) {
    *result = status;
    std::move(on_close).Run();
  }

  // WebContents where this prerenderer is embedded.
  WebContentsImpl& web_contents_;

  // Used for testing, this closure is only set when waiting a page to be
  // either loaded for pre-rendering. |frame_tree_| provides us with a trigger
  // for when the page is loaded.
  base::OnceCallback<void(PrerenderHost::LoadingOutcome)>
      on_wait_loading_finished_;

  // Frame tree created for the prerenderer to load the page and prepare it for
  // a future activation. During activation, the prerendered page will be taken
  // out from |frame_tree_| and moved over to |web_contents_|'s primary frame
  // tree, while |frame_tree_| will be deleted.
  std::unique_ptr<FrameTree> frame_tree_;
};

PrerenderHost::PrerenderHost(const PrerenderAttributes& attributes,
                             WebContents& web_contents)
    : attributes_(attributes) {
  DCHECK(blink::features::IsPrerender2Enabled());
  // If the prerendering is browser-initiated, it is expected to have no
  // initiator. All initiator related information should be null or invalid. On
  // the other hand, renderer-initiated prerendering should have valid initiator
  // information.
  if (attributes.IsBrowserInitiated()) {
    DCHECK(!attributes.initiator_origin.has_value());
    DCHECK(!attributes.initiator_frame_token.has_value());
    DCHECK_EQ(attributes.initiator_process_id,
              ChildProcessHost::kInvalidUniqueID);
    DCHECK_EQ(attributes.initiator_ukm_id, ukm::kInvalidSourceId);
    DCHECK_EQ(attributes.initiator_frame_tree_node_id,
              RenderFrameHost::kNoFrameTreeNodeId);
  } else {
    DCHECK(attributes.initiator_origin.has_value());
    DCHECK(attributes.initiator_frame_token.has_value());
    // TODO(https://crbug.com/1325211): Add back the following DCHECKs after
    // fixing prerendering activation for embedder-triggered prerendering in
    // unittests.
    // DCHECK_NE(attributes.initiator_process_id,
    // ChildProcessHost::kInvalidUniqueID);
    // DCHECK_NE(attributes.initiator_ukm_id, ukm::kInvalidSourceId);
    // DCHECK_NE(attributes.initiator_frame_tree_node_id,
    //           RenderFrameHost::kNoFrameTreeNodeId);
  }
  CreatePageHolder(*static_cast<WebContentsImpl*>(&web_contents));
}

PrerenderHost::~PrerenderHost() {
  // Stop observing here. Otherwise, destructing members may lead
  // DidFinishNavigation call after almost everything being destructed.
  Observe(nullptr);

  for (auto& observer : observers_)
    observer.OnHostDestroyed();

  if (!final_status_)
    RecordFinalStatus(FinalStatus::kDestroyed, attributes_.initiator_ukm_id,
                      ukm::kInvalidSourceId);
}

// TODO(https://crbug.com/1132746): Inspect diffs from the current
// no-state-prefetch implementation. See PrerenderContents::StartPrerendering()
// for example.
bool PrerenderHost::StartPrerendering() {
  TRACE_EVENT0("navigation", "PrerenderHost::StartPrerendering");

  // Observe events about the prerendering contents.
  Observe(page_holder_->GetWebContents());

  // Start prerendering navigation.
  NavigationController::LoadURLParams load_url_params(
      attributes_.prerendering_url);
  load_url_params.initiator_origin = attributes_.initiator_origin;
  load_url_params.initiator_process_id = attributes_.initiator_process_id;
  load_url_params.initiator_frame_token = attributes_.initiator_frame_token;
  load_url_params.is_renderer_initiated = !attributes_.IsBrowserInitiated();
  load_url_params.transition_type =
      ui::PageTransitionFromInt(attributes_.transition_type);

  // Just use the referrer from attributes, as NoStatePrefetch does.
  // TODO(crbug.com/1176054): For cross-origin prerender, follow the spec steps
  // for "sufficiently-strict speculative navigation referrer policies".
  load_url_params.referrer = attributes_.referrer;

  // TODO(https://crbug.com/1189034): Should we set `override_user_agent` here?
  // Things seem to work without it.

  // TODO(https://crbug.com/1132746): Set up other fields of `load_url_params`
  // as well, and add tests for them.
  base::WeakPtr<NavigationHandle> created_navigation_handle =
      page_holder_->GetNavigationController().LoadURLWithParams(
          load_url_params);

  if (!created_navigation_handle)
    return false;

  if (initial_navigation_id_.has_value()) {
    // In usual code path, `initial_navigation_id_` should be set by
    // PrerenderNavigationThrottle during `LoadURLWithParams` above.
    DCHECK_EQ(*initial_navigation_id_,
              created_navigation_handle->GetNavigationId());
    DCHECK(begin_params_);
    DCHECK(common_params_);
  } else {
    // In some exceptional code path, such as the navigation failed due to CSP
    // violations, PrerenderNavigationThrottle didn't run at this point. So,
    // set the ID here.
    initial_navigation_id_ = created_navigation_handle->GetNavigationId();
    // |begin_params_| and |common_params_| is null here, but it doesn't matter
    // as this branch is reached only when the initial navigation fails,
    // so this PrerenderHost can't be activated.
  }

  NavigationRequest* navigation_request =
      NavigationRequest::From(created_navigation_handle.get());
  // The initial navigation in the prerender frame tree should not wait for
  // `beforeunload` in the old page, so BeginNavigation stage should be reached
  // synchronously.
  DCHECK_GE(navigation_request->state(),
            NavigationRequest::WAITING_FOR_RENDERER_RESPONSE);
  return true;
}

void PrerenderHost::DidFinishNavigation(NavigationHandle* navigation_handle) {
  auto* navigation_request = NavigationRequest::From(navigation_handle);

  if (navigation_request->IsSameDocument())
    return;

  // Observe navigation only in the prerendering frame tree.
  if (navigation_request->frame_tree_node()->frame_tree() !=
      page_holder_->frame_tree()) {
    return;
  }

  const bool is_prerender_main_frame =
      navigation_request->GetFrameTreeNodeId() == frame_tree_node_id_;

  // Cancel prerendering on navigation request failure.
  //
  // Check net::Error here rather than PrerenderNavigationThrottle as CSP
  // blocking occurs before NavigationThrottles so cannot be observed in
  // NavigationThrottle::WillFailRequest().
  net::Error net_error = navigation_request->GetNetErrorCode();
  absl::optional<FinalStatus> status;
  if (net_error == net::Error::ERR_BLOCKED_BY_CSP) {
    status = FinalStatus::kNavigationRequestBlockedByCsp;
  } else if (net_error == net::Error::ERR_BLOCKED_BY_CLIENT) {
    status = FinalStatus::kBlockedByClient;
  } else if (is_prerender_main_frame && net_error != net::Error::OK) {
    status = FinalStatus::kNavigationRequestNetworkError;
  } else if (is_prerender_main_frame && !navigation_request->HasCommitted()) {
    status = FinalStatus::kNavigationNotCommitted;
  }
  if (status.has_value()) {
    Cancel(*status);
    return;
  }

  // The prerendered contents are considered ready for activation when the
  // main frame navigation reaches DidFinishNavigation.
  if (is_prerender_main_frame) {
    DCHECK(!is_ready_for_activation_);
    is_ready_for_activation_ = true;
  }
}

void PrerenderHost::OnVisibilityChanged(Visibility visibility) {
  TRACE_EVENT("navigation", "PrerenderHost::OnVisibilityChanged");
  if (visibility == Visibility::HIDDEN) {
    Cancel(FinalStatus::kTriggerBackgrounded);
  }
}

void PrerenderHost::ResourceLoadComplete(
    RenderFrameHost* render_frame_host,
    const GlobalRequestID& request_id,
    const blink::mojom::ResourceLoadInfo& resource_load_info) {
  // Observe resource loads only in the prerendering frame tree.
  if (&render_frame_host->GetPage() !=
      &GetPrerenderedMainFrameHost()->GetPage()) {
    return;
  }

  if (resource_load_info.net_error == net::Error::ERR_BLOCKED_BY_CLIENT) {
    Cancel(FinalStatus::kBlockedByClient);
  }
}

std::unique_ptr<StoredPage> PrerenderHost::Activate(
    NavigationRequest& navigation_request) {
  TRACE_EVENT1("navigation", "PrerenderHost::Activate", "navigation_request",
               &navigation_request);

  DCHECK(is_ready_for_activation_);
  is_ready_for_activation_ = false;

  std::unique_ptr<StoredPage> page = page_holder_->Activate(navigation_request);

  for (auto& observer : observers_)
    observer.OnActivated();

  // TODO(crbug.com/1299330): Replace
  // `navigation_request.GetNextPageUkmSourceId()` with prerendered page's UKM
  // source ID.
  RecordFinalStatus(FinalStatus::kActivated, attributes_.initiator_ukm_id,
                    navigation_request.GetNextPageUkmSourceId());
  devtools_instrumentation::DidActivatePrerender(navigation_request);
  return page;
}

// Ensure that the frame policies are compatible between primary main frame and
// prerendering main frame:
// a) primary main frame's pending_frame_policy would normally apply to the new
// document during its creation. However, for prerendering we can't apply it as
// the document is already created.
// b) prerender main frame's pending_frame_policy can't be transferred to the
// primary main frame, we should not activate if it's non-zero.
// c) Existing  document can't change the frame_policy it is affected by, so we
// can't transfer RenderFrameHosts between FrameTreeNodes with different frame
// policies.
//
// Usually frame policy for the main frame is empty as in the most common case a
// parent document sets a policy on the child iframe.
bool PrerenderHost::IsFramePolicyCompatibleWithPrimaryFrameTree() {
  FrameTreeNode* prerender_root_ftn = page_holder_->frame_tree()->root();
  FrameTreeNode* primary_root_ftn = page_holder_->GetPrimaryFrameTree().root();

  // Ensure that the pending frame policy is not set on the main frames, as it
  // is usually set on frames by their parent frames.
  if (prerender_root_ftn->pending_frame_policy() != blink::FramePolicy()) {
    return false;
  }

  if (primary_root_ftn->pending_frame_policy() != blink::FramePolicy()) {
    return false;
  }

  if (prerender_root_ftn->current_replication_state().frame_policy !=
      primary_root_ftn->current_replication_state().frame_policy) {
    return false;
  }

  return true;
}

bool PrerenderHost::AreInitialPrerenderNavigationParamsCompatibleWithNavigation(
    NavigationRequest& navigation_request) {
  // TODO(crbug.com/1181763): compare the rest of the navigation parameters. We
  // should introduce compile-time parameter checks as well, to ensure how new
  // fields should be compared for compatibility.

  // As the initial prerender navigation is a) limited to HTTP(s) URLs and b)
  // initiated by the PrerenderHost, we do not expect some navigation parameters
  // connected to certain navigation types to be set and the DCHECKS below
  // enforce that.
  // The parameters of the potential activation, however, are coming from the
  // renderer and we mostly don't have any guarantees what they are, so we
  // should not DCHECK them. Instead, by default we compare them with initial
  // prerender activation parameters and fail to activate when they differ.
  // Note: some of those parameters should be never set (or should be ignored)
  // for main-frame / HTTP(s) navigations, but we still compare them here as a
  // defence-in-depth measure.
  DCHECK(navigation_request.IsInPrimaryMainFrame());

  // Compare BeginNavigationParams.
  if (!AreBeginNavigationParamsCompatibleWithNavigation(
          navigation_request.begin_params())) {
    return false;
  }

  // Compare CommonNavigationParams.
  if (!AreCommonNavigationParamsCompatibleWithNavigation(
          navigation_request.common_params())) {
    return false;
  }

  return true;
}

bool PrerenderHost::AreBeginNavigationParamsCompatibleWithNavigation(
    const blink::mojom::BeginNavigationParams& potential_activation) {
  if (potential_activation.initiator_frame_token !=
      begin_params_->initiator_frame_token) {
    return false;
  }

  if (!AreHttpRequestHeadersCompatible(potential_activation.headers,
                                       begin_params_->headers)) {
    return false;
  }

  // Don't activate a prerendered page if the potential activation request
  // requires validation or bypass of the browser cache, as the prerendered page
  // is a kind of caches.
  // TODO(https://crbug.com/1213299): Instead of checking the load flags on
  // activation, we should cancel prerendering when the prerender initial
  // navigation has the flags.
  int cache_load_flags = net::LOAD_VALIDATE_CACHE | net::LOAD_BYPASS_CACHE |
                         net::LOAD_DISABLE_CACHE;
  if (potential_activation.load_flags & cache_load_flags) {
    return false;
  }
  if (potential_activation.load_flags != begin_params_->load_flags) {
    return false;
  }

  if (potential_activation.skip_service_worker !=
      begin_params_->skip_service_worker) {
    return false;
  }

  if (potential_activation.mixed_content_context_type !=
      begin_params_->mixed_content_context_type) {
    return false;
  }

  // Initial prerender navigation cannot be a form submission.
  DCHECK(!begin_params_->is_form_submission);
  if (potential_activation.is_form_submission !=
      begin_params_->is_form_submission) {
    return false;
  }

  if (potential_activation.searchable_form_url !=
      begin_params_->searchable_form_url) {
    return false;
  }

  if (potential_activation.searchable_form_encoding !=
      begin_params_->searchable_form_encoding) {
    return false;
  }

  // Trust token params can be set only on subframe navigations, so both values
  // should be null here.
  DCHECK(!begin_params_->trust_token_params);
  if (potential_activation.trust_token_params !=
      begin_params_->trust_token_params) {
    return false;
  }

  // Web bundle token cannot be set due because it is only set for child
  // frame navigations.
  DCHECK(!begin_params_->web_bundle_token);
  if (potential_activation.web_bundle_token) {
    return false;
  }

  // Don't require equality for request_context_type because link clicks
  // (HYPERLINK) should be allowed for activation, whereas prerender always has
  // type LOCATION.
  DCHECK_EQ(begin_params_->request_context_type,
            blink::mojom::RequestContextType::LOCATION);
  switch (potential_activation.request_context_type) {
    case blink::mojom::RequestContextType::HYPERLINK:
    case blink::mojom::RequestContextType::LOCATION:
      break;
    default:
      return false;
  }

  // Since impression should not be set, no need to compare contents.
  DCHECK(!begin_params_->impression);
  if (potential_activation.impression.has_value()) {
    return false;
  }

  // No need to test for devtools_initiator because this field is used for
  // tracking what triggered a network request, and prerender activation will
  // not use network requests.

  return true;
}

bool PrerenderHost::AreCommonNavigationParamsCompatibleWithNavigation(
    const blink::mojom::CommonNavigationParams& potential_activation) {
  // The CommonNavigationParams::url field is expected to be the same for both
  // initial and activation prerender navigations, as the PrerenderHost
  // selection would have already checked for matching values. Adding a DCHECK
  // here to be safe.
  if (attributes_.url_match_predicate) {
    DCHECK(
        attributes_.url_match_predicate.value().Run(potential_activation.url));
  } else {
    DCHECK_EQ(potential_activation.url, common_params_->url);
  }
  if (potential_activation.initiator_origin !=
      common_params_->initiator_origin) {
    return false;
  }

  if (potential_activation.transition != common_params_->transition) {
    return false;
  }

  DCHECK_EQ(common_params_->navigation_type,
            blink::mojom::NavigationType::DIFFERENT_DOCUMENT);
  if (potential_activation.navigation_type != common_params_->navigation_type) {
    return false;
  }

  // We don't check download_policy as it affects whether the download triggered
  // by the NavigationRequest is allowed to proceed (or logs metrics) and
  // doesn't affect the behaviour of the document created by a non-download
  // navigation after commit (e.g. it doesn't affect future downloads in child
  // frames). PrerenderNavigationThrottle has already ensured that the initial
  // prerendering navigation isn't a download and as prerendering activation
  // won't reach out to the network, it won't turn into a navigation as well.

  DCHECK(common_params_->base_url_for_data_url.is_empty());
  if (potential_activation.base_url_for_data_url !=
      common_params_->base_url_for_data_url) {
    return false;
  }

  // The previews_state is always set to NO_PREVIEWS in BeginNavigation and the
  // previews code was removed, so no need to compare it here as it's not used.
  // TODO(crbug.com/1232909): remove this previews_state.

  if (potential_activation.method != common_params_->method) {
    return false;
  }

  // Initial prerender navigation can't be a form submission.
  DCHECK(!common_params_->post_data);
  if (potential_activation.post_data != common_params_->post_data) {
    return false;
  }

  // No need to compare source_location, as it's only passed to the DevTools for
  // debugging purposes and does not impact the properties of the document
  // created by this navigation.

  DCHECK(!common_params_->started_from_context_menu);
  if (potential_activation.started_from_context_menu !=
      common_params_->started_from_context_menu) {
    return false;
  }

  // has_user_gesture doesn't affect any of the security properties of the
  // document created by navigation, so equality of the values is not required.
  // TODO(crbug.com/1232915): ensure that the user activation status is
  // propagated to the activated document.

  // text_fragment_token doesn't affect any of the security properties of the
  // document created by navigation, so equality of the values is not required.
  // TODO(crbug.com/1232919): ensure the activated document consumes
  // text_fragment_token and scrolls to the corresponding viewport.

  if (potential_activation.should_check_main_world_csp !=
      common_params_->should_check_main_world_csp) {
    return false;
  }

  if (potential_activation.initiator_origin_trial_features !=
      common_params_->initiator_origin_trial_features) {
    return false;
  }

  if (potential_activation.href_translate != common_params_->href_translate) {
    return false;
  }

  // Initial prerender navigation can't be a history navigation.
  DCHECK(!common_params_->is_history_navigation_in_new_child_frame);
  if (potential_activation.is_history_navigation_in_new_child_frame !=
      common_params_->is_history_navigation_in_new_child_frame) {
    return false;
  }

  // The spec mandates matching the referrer policy, and not the referrer URL
  // itself, so we only compare the referrer policy here. Referrer policy is a
  // more predictable value to match than referrer URL.
  // https://wicg.github.io/nav-speculation/prerendering.html#navigate-activation
  if (potential_activation.referrer->policy !=
      common_params_->referrer->policy) {
    return false;
  }

  if (potential_activation.request_destination !=
      common_params_->request_destination) {
    return false;
  }

  return true;
}

RenderFrameHostImpl* PrerenderHost::GetPrerenderedMainFrameHost() {
  DCHECK(page_holder_->frame_tree());
  DCHECK(page_holder_->frame_tree()->root()->current_frame_host());
  return page_holder_->frame_tree()->root()->current_frame_host();
}

FrameTree& PrerenderHost::GetPrerenderFrameTree() {
  DCHECK(page_holder_->frame_tree());
  return *page_holder_->frame_tree();
}

void PrerenderHost::RecordFinalStatus(base::PassKey<PrerenderHostRegistry>,
                                      FinalStatus status) {
  RecordFinalStatus(status, attributes_.initiator_ukm_id,
                    ukm::kInvalidSourceId);
}

void PrerenderHost::CreatePageHolder(WebContentsImpl& web_contents) {
  page_holder_ = std::make_unique<PageHolder>(web_contents);
  frame_tree_node_id_ =
      page_holder_->frame_tree()->root()->frame_tree_node_id();
}

PrerenderHost::LoadingOutcome PrerenderHost::WaitForLoadStopForTesting() {
  return page_holder_->WaitForLoadCompletionForTesting();  // IN-TEST
}

void PrerenderHost::RecordFinalStatus(FinalStatus status,
                                      ukm::SourceId initiator_ukm_id,
                                      ukm::SourceId prerendered_ukm_id) {
  DCHECK(!final_status_);
  final_status_ = status;
  RecordPrerenderHostFinalStatus(status, attributes_, prerendered_ukm_id);
}

const GURL& PrerenderHost::GetInitialUrl() const {
  return attributes_.prerendering_url;
}

void PrerenderHost::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PrerenderHost::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

absl::optional<int64_t> PrerenderHost::GetInitialNavigationId() const {
  return initial_navigation_id_;
}

void PrerenderHost::SetInitialNavigation(NavigationRequest* navigation) {
  DCHECK(!initial_navigation_id_.has_value());
  initial_navigation_id_ = navigation->GetNavigationId();
  begin_params_ = navigation->begin_params().Clone();
  common_params_ = navigation->common_params().Clone();
}

bool PrerenderHost::IsUrlMatch(const GURL& url) const {
  // If the trigger defines its predicate, respect it.
  if (attributes_.url_match_predicate) {
    // Triggers are not allowed to treat a cross-origin url as a matched url. It
    // would cause security risks.
    if (!url::IsSameOriginWith(attributes_.prerendering_url, url))
      return false;
    return attributes_.url_match_predicate.value().Run(url);
  }
  return GetInitialUrl() == url;
}

void PrerenderHost::Cancel(FinalStatus status) {
  TRACE_EVENT("navigation", "PrerenderHost::Cancel", "final_status", status);
  // Already cancelled.
  if (final_status_)
    return;

  RenderFrameHostImpl* host = PrerenderHost::GetPrerenderedMainFrameHost();
  DCHECK(host);
  PrerenderHostRegistry* registry =
      host->delegate()->GetPrerenderHostRegistry();
  DCHECK(registry);
  registry->CancelHost(frame_tree_node_id_, status);
}

}  // namespace content
