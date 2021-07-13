// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_host.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/run_loop.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_conversion_helper.h"
#include "content/browser/prerender/prerender_host_registry.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_restore_context_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/load_flags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"

namespace content {

namespace {
void CollectAllChildren(RenderFrameHostImpl& rfh,
                        std::vector<RenderFrameHostImpl*>* result) {
  result->push_back(&rfh);
  for (size_t i = 0; i < rfh.child_count(); ++i) {
    if (RenderFrameHostImpl* speculative_frame_host =
            rfh.child_at(i)->render_manager()->speculative_frame_host()) {
      result->push_back(speculative_frame_host);
    }
    CollectAllChildren(*(rfh.child_at(i)->current_frame_host()), result);
  }
}

// Iterate over RenderFrameHostImpl::children_ rather than FrameTree::Nodes()
// because the |rfh| root node will not have a current_frame_host value. The
// root node is set to null in MPArch prerender activation when generating a
// BackForwardCacheImpl::Entry.
// TODO(mcnee): Implement in terms of RenderFrameHost::ForEachRenderFrameHost.
std::vector<RenderFrameHostImpl*> AllDescendantActiveRenderFrameHosts(
    RenderFrameHostImpl& rfh) {
  std::vector<RenderFrameHostImpl*> result;
  CollectAllChildren(rfh, &result);
  return result;
}

struct ActivateResult {
  ActivateResult(PrerenderHost::FinalStatus status,
                 std::unique_ptr<BackForwardCacheImpl::Entry> entry)
      : status(status), entry(std::move(entry)) {}

  PrerenderHost::FinalStatus status = PrerenderHost::FinalStatus::kActivated;
  std::unique_ptr<BackForwardCacheImpl::Entry> entry;
};

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
                      /*main_frame_name=*/"");

    const auto& site_info =
        static_cast<SiteInstanceImpl*>(site_instance.get())->GetSiteInfo();
    // Use the same SessionStorageNamespace as the primary page for the
    // prerendering page.
    frame_tree_->controller().SetSessionStorageNamespace(
        site_info.GetStoragePartitionId(site_instance->GetBrowserContext()),
        web_contents_.GetFrameTree()->controller().GetSessionStorageNamespace(
            site_info));

    // TODO(https://crbug.com/1199679): This should be moved to FrameTree::Init
    web_contents_.NotifySwappedFromRenderManager(
        /*old_frame=*/nullptr,
        frame_tree_->root()->render_manager()->current_frame_host(),
        /*is_main_frame=*/true);
  }

  ~PageHolder() override {
    if (frame_tree_)
      frame_tree_->Shutdown();
  }

  // FrameTree::Delegate

  // TODO(https://crbug.com/1199682): Correctly handle load events. Ignored for
  // now as it confuses WebContentsObserver instances because they can not
  // distinguish between the different FrameTrees.

  void DidStartLoading(FrameTreeNode* frame_tree_node,
                       bool to_different_document) override {}

  void DidStopLoading() override {
    if (on_stopped_loading_for_tests_) {
      std::move(on_stopped_loading_for_tests_).Run();
    }
  }

  void DidChangeLoadProgress() override {}
  bool IsHidden() override { return true; }
  void NotifyPageChanged(PageImpl& page) override {}

  // NavigationControllerDelegate
  void NotifyNavigationStateChanged(InvalidateTypes changed_flags) override {}
  bool IsBeingDestroyed() override { return false; }
  void NotifyBeforeFormRepostWarningShow() override {}
  void NotifyNavigationEntryCommitted(
      const LoadCommittedDetails& load_details) override {}
  void NotifyNavigationEntryChanged(
      const EntryChangedDetails& change_details) override {}
  void NotifyNavigationListPruned(
      const PrunedDetails& pruned_details) override {}
  void NotifyNavigationEntriesDeleted() override {}
  void ActivateAndShowRepostFormWarningDialog() override {}
  bool ShouldPreserveAbortedURLs() override { return false; }
  WebContents* DeprecatedGetWebContents() override { return GetWebContents(); }
  void UpdateOverridingUserAgent() override {}

  NavigationControllerImpl& GetNavigationController() {
    return frame_tree_->controller();
  }

  WebContents* GetWebContents() { return &web_contents_; }

  ActivateResult Activate(NavigationRequest& navigation_request) {
    // There should be no ongoing main-frame navigation during activation.
    // TODO(https://crbug.com/1190644): Make sure sub-frame navigations are
    // fine.
    DCHECK(!frame_tree_->root()->HasNavigation());

    // NOTE: TakePrerenderedPage() clears the current_frame_host value of
    // frame_tree_->root(). Do not add any code between here and
    // frame_tree_.reset() that calls into observer functions to minimize the
    // duration of current_frame_host being null.
    //
    // TODO(https://crbug.com/1176148): Investigate how to combine taking the
    // prerendered page and frame_tree_ destruction.
    std::unique_ptr<BackForwardCacheImpl::Entry> page =
        frame_tree_->root()->render_manager()->TakePrerenderedPage();

    std::unique_ptr<NavigationEntryRestoreContextImpl> context =
        std::make_unique<NavigationEntryRestoreContextImpl>();
    std::unique_ptr<NavigationEntryImpl> nav_entry =
        GetNavigationController()
            .GetEntryWithUniqueID(page->render_frame_host->nav_entry_id())
            ->CloneWithoutSharing(context.get());

    navigation_request.SetPrerenderNavigationEntry(std::move(nav_entry));

    FrameTree* target_frame_tree = web_contents_.GetFrameTree();
    DCHECK_EQ(target_frame_tree,
              navigation_request.frame_tree_node()->frame_tree());

    page->render_frame_host->SetFrameTreeNode(*(target_frame_tree->root()));
    for (auto& it : page->proxy_hosts) {
      it.second->set_frame_tree_node(*(target_frame_tree->root()));
    }

    // Iterate over root RenderFrameHost and all of its descendant frames and
    // updates the associated frame tree. Note that subframe proxies don't need
    // their FrameTrees independently updated, since their FrameTreeNodes don't
    // change, and FrameTree references in those FrameTreeNodes will be updated
    // through RenderFrameHosts.
    //
    // TODO(https://crbug.com/1199693): Need to investigate if and how
    // pending delete RenderFrameHost objects should be handled if prerendering
    // runs all of the unload handlers; they are not currently handled here.
    // This is because pending delete RenderFrameHosts can still receive and
    // process some messages while the RenderFrameHost FrameTree and
    // FrameTreeNode are stale.
    for (auto* rfh : AllDescendantActiveRenderFrameHosts(
             *(page->render_frame_host.get()))) {
      rfh->frame_tree_node()->SetFrameTree(*target_frame_tree);
      rfh->SetFrameTree(*target_frame_tree);
      rfh->render_view_host()->SetFrameTree(*target_frame_tree);
      // The visibility state of the prerendering page has not been updated by
      // WebContentsImpl::UpdateVisibilityAndNotifyPageAndView(). So updates
      // the visibility state using the PageVisibilityState of |web_contents_|.
      rfh->render_view_host()->SetFrameTreeVisibility(
          web_contents_.GetPageVisibilityState());
      if (rfh->GetRenderWidgetHost()) {
        rfh->GetRenderWidgetHost()->SetFrameTree(*target_frame_tree);
      }
    }

    frame_tree_->Shutdown();
    frame_tree_.reset();

    return ActivateResult(FinalStatus::kActivated, std::move(page));
  }

  void WaitForLoadCompletionForTesting() {
    if (!frame_tree_->IsLoading())
      return;

    base::RunLoop loop;
    on_stopped_loading_for_tests_ = loop.QuitClosure();
    loop.Run();
  }

  FrameTree* frame_tree() { return frame_tree_.get(); }

 private:
  // WebContents where this prerenderer is embedded.
  WebContentsImpl& web_contents_;

  // This can be called when |frame_tree_| is destroyed so it must be
  // destructed after |frame_tree_|.
  base::OnceClosure on_stopped_loading_for_tests_;

  // Frame tree created for the prerenderer to load the page and prepare it for
  // a future activation. During activation, the prerendered page will be taken
  // out from |frame_tree_| and moved over to |web_contents_|'s primary frame
  // tree, while |frame_tree_| will be deleted.
  std::unique_ptr<FrameTree> frame_tree_;
};

PrerenderHost::PrerenderHost(blink::mojom::PrerenderAttributesPtr attributes,
                             RenderFrameHostImpl& initiator_render_frame_host)
    : attributes_(std::move(attributes)),
      initiator_origin_(initiator_render_frame_host.GetLastCommittedOrigin()),
      initiator_process_id_(initiator_render_frame_host.GetProcess()->GetID()),
      initiator_frame_token_(initiator_render_frame_host.GetFrameToken()) {
  DCHECK(blink::features::IsPrerender2Enabled());
  auto* web_contents =
      WebContents::FromRenderFrameHost(&initiator_render_frame_host);
  DCHECK(web_contents);
  CreatePageHolder(*static_cast<WebContentsImpl*>(web_contents));
}

PrerenderHost::~PrerenderHost() {
  // Stop observing here. Otherwise, destructing members may lead
  // DidFinishNavigation call after almost everything being destructed.
  Observe(nullptr);

  for (auto& observer : observers_)
    observer.OnHostDestroyed();

  if (!final_status_)
    RecordFinalStatus(FinalStatus::kDestroyed);
}

// TODO(https://crbug.com/1132746): Inspect diffs from the current
// no-state-prefetch implementation. See PrerenderContents::StartPrerendering()
// for example.
bool PrerenderHost::StartPrerendering() {
  TRACE_EVENT0("navigation", "PrerenderHost::StartPrerendering");

  // Observe events about the prerendering contents.
  Observe(page_holder_->GetWebContents());

  // Start prerendering navigation.
  NavigationController::LoadURLParams load_url_params(attributes_->url);
  load_url_params.initiator_origin = initiator_origin_;
  load_url_params.initiator_process_id = initiator_process_id_;
  load_url_params.initiator_frame_token = initiator_frame_token_;

  // Just use the referrer from attributes, as NoStatePrefetch does.
  // TODO(crbug.com/1176054): For cross-origin prerender, follow the spec steps
  // for "sufficiently-strict speculative navigation referrer policies".
  if (attributes_->referrer)
    load_url_params.referrer = Referrer(*attributes_->referrer);

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
  } else {
    // In some exceptional code path, such as the navigation failed due to CSP
    // violations, PrerenderNavigationThrottle didn't run at this point. So,
    // set the ID here.
    initial_navigation_id_ = created_navigation_handle->GetNavigationId();
  }

  NavigationRequest* navigation_request =
      NavigationRequest::From(created_navigation_handle.get());
  // The initial navigation in the prerender frame tree should not wait for
  // `beforeunload` in the old page, so BeginNavigation stage should be reached
  // synchronously.
  DCHECK_GE(navigation_request->state(),
            NavigationRequest::WAITING_FOR_RENDERER_RESPONSE);
  begin_params_ = navigation_request->begin_params().Clone();
  common_params_ = navigation_request->common_params().Clone();
  return true;
}

void PrerenderHost::DidFinishNavigation(NavigationHandle* navigation_handle) {
  // Observe navigation only in the prerendering main frame.
  if (navigation_handle->GetFrameTreeNodeId() != frame_tree_node_id_)
    return;

  // Stop observing the events about the prerendered contents.
  Observe(nullptr);

  // Cancel prerendering on navigation request failure.
  //
  // Check net::Error here rather than PrerenderNavigationThrottle as CSP
  // blocking occurs before NavigationThrottles so cannot be observed in
  // NavigationThrottle::WillFailRequest().
  net::Error net_error = navigation_handle->GetNetErrorCode();
  absl::optional<FinalStatus> status;
  if (net_error == net::Error::ERR_BLOCKED_BY_CSP) {
    status = FinalStatus::kNavigationRequestBlockedByCsp;
  } else if (net_error != net::Error::OK) {
    status = FinalStatus::kNavigationRequestNetworkError;
  } else if (!navigation_handle->HasCommitted()) {
    status = FinalStatus::kNavigationNotCommitted;
  }
  if (status.has_value()) {
    Cancel(*status);
    return;
  }

  // The prerendered contents are considered ready for activation when the
  // main frame navigation reaches DidFinishNavigation.
  DCHECK(!is_ready_for_activation_);
  is_ready_for_activation_ = true;
}

std::unique_ptr<BackForwardCacheImpl::Entry> PrerenderHost::Activate(
    NavigationRequest& navigation_request) {
  TRACE_EVENT1("navigation", "PrerenderHost::Activate", "navigation_request",
               &navigation_request);

  DCHECK(is_ready_for_activation_);
  is_ready_for_activation_ = false;

  ActivateResult result = page_holder_->Activate(navigation_request);
  if (result.status != FinalStatus::kActivated) {
    RecordFinalStatus(result.status);
    return nullptr;
  }

  for (auto& observer : observers_)
    observer.OnActivated();

  RecordFinalStatus(FinalStatus::kActivated);
  return std::move(result.entry);
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

  if (potential_activation.headers != begin_params_->headers) {
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

  // TODO(https://crbug.com/1181763): Determine if we should compare
  // `request_context_type`. Just checking for equality is bad because
  // the prerender has type LOCATION and a link click would have type
  // HYPERLINK.

  if (potential_activation.request_destination !=
      begin_params_->request_destination) {
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
  if (potential_activation.initiator_origin !=
      common_params_->initiator_origin) {
    return false;
  }

  DCHECK_EQ(common_params_->navigation_type,
            blink::mojom::NavigationType::DIFFERENT_DOCUMENT);
  if (potential_activation.navigation_type != common_params_->navigation_type) {
    return false;
  }

  DCHECK(common_params_->base_url_for_data_url.is_empty());
  if (potential_activation.base_url_for_data_url !=
      common_params_->base_url_for_data_url) {
    return false;
  }

  DCHECK(common_params_->history_url_for_data_url.is_empty());
  if (potential_activation.history_url_for_data_url.is_empty() !=
      common_params_->history_url_for_data_url.is_empty()) {
    return false;
  }

  if (potential_activation.method != common_params_->method) {
    return false;
  }

  // Initial prerender navigation can't be a form submission.
  DCHECK(!common_params_->post_data);
  if (potential_activation.post_data != common_params_->post_data) {
    return false;
  }

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

  // The CommonNavigationParams::url field is expected to be the same for both
  // initial and activation prerender navigations, as the PrerenderHost
  // selection would have already checked for matching values. Adding a DCHECK
  // here to be safe.
  DCHECK_EQ(potential_activation.url, common_params_->url);

  if (potential_activation.started_from_context_menu !=
      common_params_->started_from_context_menu) {
    return false;
  }

  return true;
}

RenderFrameHostImpl* PrerenderHost::GetPrerenderedMainFrameHost() {
  DCHECK(page_holder_->frame_tree());
  DCHECK(page_holder_->frame_tree()->root()->current_frame_host());
  return page_holder_->frame_tree()->root()->current_frame_host();
}

void PrerenderHost::RecordFinalStatus(base::PassKey<PrerenderHostRegistry>,
                                      FinalStatus status) {
  RecordFinalStatus(status);
}

void PrerenderHost::CreatePageHolder(WebContentsImpl& web_contents) {
  page_holder_ = std::make_unique<PageHolder>(web_contents);
  frame_tree_node_id_ =
      page_holder_->frame_tree()->root()->frame_tree_node_id();
}

void PrerenderHost::WaitForLoadStopForTesting() {
  page_holder_->WaitForLoadCompletionForTesting();  // IN-TEST
}

void PrerenderHost::RecordFinalStatus(FinalStatus status) {
  DCHECK(!final_status_);
  final_status_ = status;
  base::UmaHistogramEnumeration(
      "Prerender.Experimental.PrerenderHostFinalStatus", status);
}

const GURL& PrerenderHost::GetInitialUrl() const {
  return attributes_->url;
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

void PrerenderHost::SetInitialNavigationId(int64_t navigation_id) {
  DCHECK(!initial_navigation_id_.has_value());
  initial_navigation_id_ = navigation_id;
}

void PrerenderHost::Cancel(FinalStatus status) {
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
