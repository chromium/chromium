// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_host.h"

#include <memory>
#include <optional>

#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "content/browser/client_hints/client_hints.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/preloading/prefetch/no_vary_search_helper.h"
#include "content/browser/preloading/preloading_attempt_impl.h"
#include "content/browser/preloading/preloading_trigger_type_impl.h"
#include "content/browser/preloading/prerender/devtools_prerender_attempt.h"
#include "content/browser/preloading/prerender/prerender_features.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/preloading/prerender/prerender_metrics.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_restore_context_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/referrer.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace content {

namespace {

base::OnceCallback<void(FrameTreeNodeId)>& GetHostCreationCallbackForTesting() {
  static base::NoDestructor<base::OnceCallback<void(FrameTreeNodeId)>>
      host_creation_callback_for_testing;
  return *host_creation_callback_for_testing;
}

}  // namespace

// static
PrerenderHost* PrerenderHost::GetFromFrameTreeNodeIfPrerendering(
    FrameTreeNode& frame_tree_node) {
  if (!frame_tree_node.frame_tree().is_prerendering()) {
    return nullptr;
  }
  return &GetFromFrameTreeNode(frame_tree_node);
}

// static
PrerenderHost& PrerenderHost::GetFromFrameTreeNode(
    FrameTreeNode& frame_tree_node) {
  CHECK(frame_tree_node.frame_tree().is_prerendering());
  return *static_cast<PrerenderHost*>(frame_tree_node.frame_tree().delegate());
}

// static
bool PrerenderHost::AreHttpRequestHeadersCompatible(
    const std::string& potential_activation_headers_str,
    const std::string& prerender_headers_str,
    PreloadingTriggerType trigger_type,
    const std::string& histogram_suffix,
    PrerenderCancellationReason& reason) {
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

  prerender_headers.RemoveHeader("RTT");
  potential_activation_headers.RemoveHeader("RTT");
  prerender_headers.RemoveHeader("Downlink");
  potential_activation_headers.RemoveHeader("Downlink");

  // TODO(crbug.com/40244149): Instead of handling headers added by
  // embedders specifically, prerender should expose an interface to embedders
  // to set url parameters.
#if BUILDFLAG(IS_ANDROID)
  // Used by Android devices only.
  if (trigger_type == PreloadingTriggerType::kEmbedder) {
    prerender_headers.RemoveHeader("X-Geo");
    potential_activation_headers.RemoveHeader("X-Geo");
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // Remove the viewport headers as the viewport size of the initiator page can
  // be changed during prerendering. See also https://crbug.com/1401244.
  prerender_headers.RemoveHeader("viewport-width");
  potential_activation_headers.RemoveHeader("viewport-width");
  prerender_headers.RemoveHeader("sec-ch-viewport-width");
  potential_activation_headers.RemoveHeader("sec-ch-viewport-width");
  // Don't need to handle "viewport-height" as it is not defined in the specs.
  prerender_headers.RemoveHeader("sec-ch-viewport-height");
  potential_activation_headers.RemoveHeader("sec-ch-viewport-height");

  return PrerenderHost::IsActivationHeaderMatch(potential_activation_headers,
                                                prerender_headers, reason);
}

// static
void PrerenderHost::SetHostCreationCallbackForTesting(
    base::OnceCallback<void(FrameTreeNodeId host_id)> callback) {
  GetHostCreationCallbackForTesting() = std::move(callback);  // IN-TEST
}

PrerenderHost::PrerenderHost(
    const PrerenderAttributes& attributes,
    WebContentsImpl& web_contents,
    base::WeakPtr<PreloadingAttempt> attempt,
    std::unique_ptr<DevToolsPrerenderAttempt> devtools_attempt)
    : attributes_(attributes),
      metric_suffix_(
          GeneratePrerenderHistogramSuffix(trigger_type(),
                                           embedder_histogram_suffix())),
      attempt_(std::move(attempt)),
      devtools_attempt_(std::move(devtools_attempt)),
      web_contents_(web_contents),
      frame_tree_(std::make_unique<FrameTree>(web_contents.GetBrowserContext(),
                                              this,
                                              this,
                                              &web_contents,
                                              &web_contents,
                                              &web_contents,
                                              &web_contents,
                                              &web_contents,
                                              &web_contents,
                                              FrameTree::Type::kPrerender)) {
  // If the prerendering is browser-initiated, it is expected to have no
  // initiator. All initiator related information should be null or invalid. On
  // the other hand, renderer-initiated prerendering should have valid initiator
  // information.
  if (attributes.IsBrowserInitiated()) {
    CHECK(!attributes.initiator_origin.has_value());
    CHECK(!attributes.initiator_frame_token.has_value());
    CHECK_EQ(attributes.initiator_process_id,
             ChildProcessHost::kInvalidUniqueID);
    CHECK_EQ(attributes.initiator_ukm_id, ukm::kInvalidSourceId);
    CHECK(attributes.initiator_frame_tree_node_id.is_null());
  } else {
    CHECK(attributes.initiator_origin.has_value());
    CHECK(attributes.initiator_frame_token.has_value());
    CHECK_NE(attributes.initiator_process_id,
             ChildProcessHost::kInvalidUniqueID);
    CHECK_NE(attributes.initiator_ukm_id, ukm::kInvalidSourceId);
    CHECK(attributes.initiator_frame_tree_node_id);
  }

  SetTriggeringOutcome(PreloadingTriggeringOutcome::kTriggeredButPending);

  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::Create(web_contents.GetBrowserContext());
  frame_tree_->Init(site_instance.get(),
                    /*renderer_initiated_creation=*/false,
                    /*main_frame_name=*/"", /*opener_for_origin=*/nullptr,
                    /*frame_policy=*/blink::FramePolicy(),
                    base::UnguessableToken::Create());

  // Use the same SessionStorageNamespace as the primary page for the
  // prerendering page.
  frame_tree_->controller().SetSessionStorageNamespace(
      site_instance->GetStoragePartitionConfig(),
      web_contents_->GetPrimaryFrameTree()
          .controller()
          .GetSessionStorageNamespace(
              site_instance->GetStoragePartitionConfig()));

  // TODO(crbug.com/40177940): This should be moved to FrameTree::Init
  web_contents_->NotifySwappedFromRenderManager(
      /*old_frame=*/nullptr,
      frame_tree_->root()->render_manager()->current_frame_host());

  frame_tree_node_id_ = frame_tree_->root()->frame_tree_node_id();

  if (GetHostCreationCallbackForTesting()) {
    std::move(GetHostCreationCallbackForTesting())  // IN-TEST
        .Run(frame_tree_node_id_);
  }
}

// static
bool PrerenderHost::IsActivationHeaderMatch(
    const net::HttpRequestHeaders& potential_activation_headers,
    const net::HttpRequestHeaders& prerender_headers,
    PrerenderCancellationReason& reason) {
  // Normalize the headers.
  using HeaderPair = net::HttpRequestHeaders::HeaderKeyValuePair;
  auto cmp = [](const HeaderPair& a, const HeaderPair& b) {
    return a.key < b.key;
  };
  auto lower_case = [](HeaderPair& x) { x.key = base::ToLowerASCII(x.key); };
  auto same_predicate = [](const HeaderPair& a, const HeaderPair& b) {
    return a.key == b.key && base::EqualsCaseInsensitiveASCII(a.value, b.value);
  };

  std::vector<HeaderPair> potential_header_list(
      potential_activation_headers.GetHeaderVector());
  std::vector<HeaderPair> prerender_header_list(
      prerender_headers.GetHeaderVector());
  std::for_each(potential_header_list.begin(), potential_header_list.end(),
                lower_case);
  std::for_each(prerender_header_list.begin(), prerender_header_list.end(),
                lower_case);
  std::sort(potential_header_list.begin(), potential_header_list.end(), cmp);
  std::sort(prerender_header_list.begin(), prerender_header_list.end(), cmp);

  std::unique_ptr<std::vector<PrerenderMismatchedHeaders>> mismatched_headers =
      std::make_unique<std::vector<PrerenderMismatchedHeaders>>();

  auto prerender_header_list_it = prerender_header_list.begin();
  auto potential_header_list_it = potential_header_list.begin();

  while (prerender_header_list_it != prerender_header_list.end() &&
         potential_header_list_it != potential_header_list.end()) {
    if (same_predicate(*prerender_header_list_it, *potential_header_list_it)) {
      prerender_header_list_it++;
      potential_header_list_it++;
    } else if (prerender_header_list_it->key == potential_header_list_it->key) {
      mismatched_headers->emplace_back(prerender_header_list_it->key,
                                       prerender_header_list_it->value,
                                       potential_header_list_it->value);
      prerender_header_list_it++;
      potential_header_list_it++;
    } else if (prerender_header_list_it->key < potential_header_list_it->key) {
      mismatched_headers->emplace_back(prerender_header_list_it->key,
                                       prerender_header_list_it->value,
                                       std::nullopt);
      prerender_header_list_it++;
    } else {
      mismatched_headers->emplace_back(potential_header_list_it->key,
                                       std::nullopt,
                                       potential_header_list_it->value);
      potential_header_list_it++;
    }
  }

  while (prerender_header_list_it != prerender_header_list.end()) {
    mismatched_headers->emplace_back(prerender_header_list_it->key,
                                     prerender_header_list_it->value,
                                     std::nullopt);
    prerender_header_list_it++;
  }

  while (potential_header_list_it != potential_header_list.end()) {
    mismatched_headers->emplace_back(potential_header_list_it->key,
                                     std::nullopt,
                                     potential_header_list_it->value);
    potential_header_list_it++;
  }
  if (mismatched_headers->empty()) {
    return true;
  }
  reason.SetPrerenderMismatchedHeaders(std::move(mismatched_headers));
  return false;
}

PrerenderHost::~PrerenderHost() {
  for (auto& observer : observers_) {
    observer.OnHostDestroyed(
        final_status_.value_or(PrerenderFinalStatus::kDestroyed));
  }

  if (!final_status_) {
    RecordFailedFinalStatusImpl(
        PrerenderCancellationReason(PrerenderFinalStatus::kDestroyed));
  }

  // If we are still waiting on test loop, we can assume the page loading step
  // has been cancelled and the PrerenderHost is being discarded without
  // completing loading the page.
  if (on_wait_loading_finished_) {
    std::move(on_wait_loading_finished_)
        .Run(PrerenderHost::LoadingOutcome::kPrerenderingCancelled);
  }

  if (frame_tree_) {
    frame_tree_->Shutdown();
  }
}

void PrerenderHost::DidStopLoading() {
  if (on_wait_loading_finished_) {
    std::move(on_wait_loading_finished_).Run(LoadingOutcome::kLoadingCompleted);
  }
}

bool PrerenderHost::IsHidden() {
  return true;
}

FrameTree* PrerenderHost::LoadingTree() {
  // For prerendering loading tree is the same as its frame tree as loading is
  // done at a frame tree level in the background, unlike the loading visible
  // to the user where we account for nested frame tree loading state.
  return frame_tree_.get();
}

void PrerenderHost::SetFocusedFrame(FrameTreeNode* node,
                                    SiteInstanceGroup* source) {
  // `node` can only become focused when `node`'s current RenderFrameHost is
  // active.
  NOTREACHED();
}

FrameTree* PrerenderHost::GetOwnedPictureInPictureFrameTree() {
  return nullptr;
}

FrameTree* PrerenderHost::GetPictureInPictureOpenerFrameTree() {
  return nullptr;
}

FrameTreeNodeId PrerenderHost::GetOuterDelegateFrameTreeNodeId() {
  // A prerendered FrameTree is not "inner to" or "nested inside" another
  // FrameTree; it exists in parallel to the primary FrameTree of the current
  // WebContents. Therefore, it must not attempt to access the primary
  // FrameTree in the sense of an "outer delegate" relationship, so we return
  // the invalid ID here.
  return FrameTreeNodeId();
}

RenderFrameHostImpl* PrerenderHost::GetProspectiveOuterDocument() {
  // A prerendered FrameTree never has an outer document.
  return nullptr;
}

void PrerenderHost::ActivateAndShowRepostFormWarningDialog() {
  // Not supported, cancel pending reload.
  GetNavigationController().CancelPendingReload();
}

bool PrerenderHost::ShouldPreserveAbortedURLs() {
  return false;
}

// TODO(crbug.com/40150744): Inspect diffs from the current
// no-state-prefetch implementation. See PrerenderContents::StartPrerendering()
// for example.
bool PrerenderHost::StartPrerendering() {
  TRACE_EVENT0("navigation", "PrerenderHost::StartPrerendering");

  // Since prerender started we mark it as eligible and set it to running.
  SetTriggeringOutcome(PreloadingTriggeringOutcome::kRunning);

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
  load_url_params.referrer = attributes_.referrer;

  load_url_params.override_user_agent =
      web_contents_->GetDelegate()->ShouldOverrideUserAgentForPrerender2();

  // TODO(https://crbug.com/1406149, https://crbug.com/1378921): Set
  // `override_user_agent` for Android. This field is determined on the Java
  // side based on the URL and we should mimic Java code and set it to the
  // correct value. After fixing this, we can remove the check for UA headers
  // upon activation.

  // TODO(crbug.com/40150744): Set up other fields of `load_url_params`
  // as well, and add tests for them.
  base::WeakPtr<NavigationHandle> created_navigation_handle =
      GetNavigationController().LoadURLWithParams(load_url_params);

  if (!created_navigation_handle)
    return false;

  if (attributes_.prerender_navigation_handle_callback) {
    attributes_.prerender_navigation_handle_callback.Run(
        *created_navigation_handle);
  }

  // Even when LoadURLWithParams() returns a valid navigation handle, navigation
  // can fail during navigation start, for example, due to prerendering a
  // non-supported URL scheme that is filtered out in
  // PrerenderNavigationThrottle.
  if (final_status_.has_value())
    return false;

  if (initial_navigation_id_.has_value()) {
    // In usual code path, `initial_navigation_id_` should be set by
    // PrerenderNavigationThrottle during `LoadURLWithParams` above.
    CHECK_EQ(*initial_navigation_id_,
             created_navigation_handle->GetNavigationId());
    CHECK(begin_params_);
    CHECK(common_params_);
  } else {
    // In some exceptional code path, such as the navigation failed due to CSP
    // violations, PrerenderNavigationThrottle didn't run at this point. So,
    // set the ID here.
    initial_navigation_id_ = created_navigation_handle->GetNavigationId();
    // `begin_params_` and `common_params_` is null here, but it doesn't matter
    // as this branch is reached only when the initial navigation fails,
    // so this PrerenderHost can't be activated.

    // Original code assumes the case CSP prefetch-src blocks prerendering, but
    // prefetch-src was already deprecated, but this code path seems still
    // reachable. To be clarify the actual scenario, let's have the dump code.
    // We may eventually return false for this code path to make things simple.
    // TODO(crbug.com/40248615): Monitor reports and decide if we
    // continue to have the `is_ready_for_activation_` check in
    // CheckInitialPrerenderNavigationParamsCompatibleWithNavigation().
    net::Error net_error = created_navigation_handle->GetNetErrorCode();
    base::debug::Alias(&net_error);
    base::debug::DumpWithoutCrashing();
  }

  NavigationRequest* navigation_request =
      NavigationRequest::From(created_navigation_handle.get());
  // The initial navigation in the prerender frame tree should not wait for
  // `beforeunload` in the old page, so BeginNavigation stage should be reached
  // synchronously.
  CHECK_GE(navigation_request->state(),
           NavigationRequest::WAITING_FOR_RENDERER_RESPONSE);
  return true;
}

void PrerenderHost::DidStartNavigation(NavigationHandle* navigation_handle) {
  CHECK(base::FeatureList::IsEnabled(
      blink::features::kPrerender2MainFrameNavigation));

  auto* navigation_request = NavigationRequest::From(navigation_handle);
  CHECK(navigation_request->IsInPrerenderedMainFrame());

  // Do nothing for the initial navigation.
  if (IsInitialNavigation(*navigation_request)) {
    return;
  }

  // Reset `is_ready_for_activation_` since it can be set to true more than once
  // and CHECK will fail when the main frame navigation happens in a
  // prerendered page and PrerenderHost::DidFinishNavigation is called multiple
  // times.
  is_ready_for_activation_ = false;
}

void PrerenderHost::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  CHECK(navigation_handle);
  // For the initial navigation, set No-Vary-Search if there is a
  // No-Vary-Search header.
  auto* navigation_request = NavigationRequest::From(navigation_handle);
  CHECK(navigation_request->IsInPrerenderedMainFrame());
  // Prerender frame tree node is alive, see:
  // `PrerenderHostRegistry::ReadyToCommitNavigation`.
  CHECK(frame_tree_);
  CHECK_EQ(frame_tree_.get(),
           &navigation_request->frame_tree_node()->frame_tree());

  if (!IsInitialNavigation(*navigation_request)) {
    return;
  }

  if (base::FeatureList::IsEnabled(blink::features::kPrerender2NoVarySearch) &&
      navigation_request->response() &&
      navigation_request->response()->parsed_headers &&
      navigation_request->response()
          ->parsed_headers->no_vary_search_with_parse_error) {
    MaybeSetNoVarySearch(
        *navigation_request->response()
             ->parsed_headers->no_vary_search_with_parse_error);
  } else {
    CHECK(!no_vary_search_.has_value());
    CHECK(!no_vary_search_parse_error_.has_value());
  }

  // ReadyToCommitNavigation is called when the headers are received.
  were_headers_received_ = true;
  for (auto& observer : observers_) {
    observer.OnHeadersReceived();
  }
}

void PrerenderHost::DidFinishNavigation(NavigationHandle* navigation_handle) {
  auto* navigation_request = NavigationRequest::From(navigation_handle);

  // Observe navigation only in the prerendering frame tree.
  CHECK_EQ(&(navigation_request->frame_tree_node()->frame_tree()),
           frame_tree_.get());

  const bool is_prerender_main_frame =
      navigation_request->GetFrameTreeNodeId() == frame_tree_node_id_;

  // Cancel prerendering on navigation request failure.
  //
  // Check net::Error here rather than PrerenderNavigationThrottle as CSP
  // blocking occurs before NavigationThrottles so cannot be observed in
  // NavigationThrottle::WillFailRequest().
  net::Error net_error = navigation_request->GetNetErrorCode();

  std::optional<PrerenderFinalStatus> status;
  if (net_error == net::Error::ERR_BLOCKED_BY_CSP) {
    status = PrerenderFinalStatus::kNavigationRequestBlockedByCsp;
  } else if (net_error == net::Error::ERR_BLOCKED_BY_CLIENT) {
    status = PrerenderFinalStatus::kBlockedByClient;
  } else if (is_prerender_main_frame && net_error != net::Error::OK) {
    status = PrerenderFinalStatus::kNavigationRequestNetworkError;
  } else if (is_prerender_main_frame && !navigation_request->HasCommitted()) {
    status = PrerenderFinalStatus::kNavigationNotCommitted;
  }
  if (status.has_value()) {
    Cancel(*status);
    return;
  }

  // The prerendered contents are considered ready for activation when the
  // main frame navigation reaches DidFinishNavigation and the prerender host
  // has not been canceled yet.
  if (is_prerender_main_frame && !final_status_) {
    CHECK(!is_ready_for_activation_);
    is_ready_for_activation_ = true;

    // Prerender is ready to activate. Set the status to kReady.
    SetTriggeringOutcome(PreloadingTriggeringOutcome::kReady);
  }
}

std::unique_ptr<StoredPage> PrerenderHost::Activate(
    NavigationRequest& navigation_request) {
  TRACE_EVENT1("navigation", "PrerenderHost::Activate", "navigation_request",
               &navigation_request);

  CHECK(is_ready_for_activation_);
  is_ready_for_activation_ = false;

  FrameTree& target_frame_tree = web_contents_->GetPrimaryFrameTree();

  // There should be no ongoing main-frame navigation during activation.
  // TODO(crbug.com/40174232): Make sure sub-frame navigations are
  // fine.
  CHECK(!frame_tree_->root()->HasNavigation());

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
  std::unique_ptr<StoredPage> page =
      frame_tree_->root()->render_manager()->TakePrerenderedPage();

  NavigationEntryRestoreContextImpl context;
  std::unique_ptr<NavigationEntryImpl> nav_entry =
      GetNavigationController()
          .GetEntryWithUniqueID(page->render_frame_host()->nav_entry_id())
          ->CloneWithoutSharing(&context);

  navigation_request.SetPrerenderActivationNavigationState(
      std::move(nav_entry), prior_replication_state);

  CHECK_EQ(&target_frame_tree,
           &navigation_request.frame_tree_node()->frame_tree());

  // We support activating the prerendered page only to the topmost
  // RenderFrameHost.
  CHECK(!page->render_frame_host()->GetParentOrOuterDocumentOrEmbedder());

  page->render_frame_host()->SetFrameTreeNode(*(target_frame_tree.root()));
  page->render_frame_host()->SetRenderFrameHostOwner(target_frame_tree.root());

  // Copy frame name into the replication state of the primary main frame to
  // ensure that the replication state of the primary main frame after
  // activation matches the replication state stored in the renderer.
  // TODO(crbug.com/40192974): Copying frame name here is suboptimal
  // and ideally we'd do this at the same time when transferring the proxies
  // from the StoredPage into RenderFrameHostManager. However, this is a
  // temporary solution until we move this into BrowsingContextState,
  // along with RenderFrameProxyHost.
  page->render_frame_host()->frame_tree_node()->set_frame_name_for_activation(
      prior_replication_state.unique_name, prior_replication_state.name);
  for (auto& it : page->proxy_hosts()) {
    it.second->set_frame_tree_node(*(target_frame_tree.root()));
  }

  // Iterate over the root RenderFrameHost's subframes and update the
  // associated frame tree. Note that subframe proxies don't need their
  // FrameTrees independently updated, since their FrameTreeNodes don't
  // change, and FrameTree references in those FrameTreeNodes will be updated
  // through RenderFrameHosts.
  //
  // TODO(crbug.com/40177949): Need to investigate if and how
  // pending delete RenderFrameHost objects should be handled if prerendering
  // runs all of the unload handlers; they are not currently handled here.
  // This is because pending delete RenderFrameHosts can still receive and
  // process some messages while the RenderFrameHost FrameTree and
  // FrameTreeNode are stale.
  for (FrameTreeNode* subframe_node : subframe_nodes) {
    subframe_node->SetFrameTree(target_frame_tree);
  }

  frame_tree_->Shutdown();
  frame_tree_.reset();

  page->render_frame_host()->ForEachRenderFrameHostIncludingSpeculative(
      [this](RenderFrameHostImpl* rfh) {
        // The visibility state of the prerendering page has not been
        // updated by
        // WebContentsImpl::UpdateVisibilityAndNotifyPageAndView(). So
        // updates the visibility state using the PageVisibilityState of
        // `web_contents`.
        rfh->render_view_host()->SetFrameTreeVisibility(
            web_contents_->GetPageVisibilityState());
      });

  for (auto& observer : observers_)
    observer.OnActivated();

  // The activated page is on the primary tree now. It can propagate the client
  // hints to the global settings.
  BrowserContext* browser_context =
      target_frame_tree.controller().GetBrowserContext();
  ClientHintsControllerDelegate* client_hints_delegate =
      browser_context->GetClientHintsControllerDelegate();
  if (client_hints_delegate) {
    for (auto& [origin, client_hint] : client_hints_type_) {
      PersistAcceptCH(origin, *(target_frame_tree.root()),
                      client_hints_delegate, client_hint);
    }
  }

  RecordActivation(navigation_request);

  // Prerender is activated. Set the status to kSuccess.
  SetTriggeringOutcome(PreloadingTriggeringOutcome::kSuccess);
  devtools_instrumentation::DidActivatePrerender(
      navigation_request, initiator_devtools_navigation_token());
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
  FrameTreeNode* prerender_root_ftn = frame_tree_->root();
  FrameTreeNode* primary_root_ftn = web_contents_->GetPrimaryFrameTree().root();

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
    NavigationRequest& navigation_request,
    PrerenderCancellationReason& reason) {
  // TODO(crbug.com/40170513): compare the rest of the navigation parameters. We
  // should introduce compile-time parameter checks as well, to ensure how new
  // fields should be compared for compatibility.

  // As the initial prerender navigation is a) limited to HTTP(s) URLs and b)
  // initiated by the PrerenderHost, we do not expect some navigation parameters
  // connected to certain navigation types to be set and the CHECKS below
  // enforce that.
  // The parameters of the potential activation, however, are coming from the
  // renderer and we mostly don't have any guarantees what they are, so we
  // should not CHECK them. Instead, by default we compare them with initial
  // prerender activation parameters and fail to activate when they differ.
  // Note: some of those parameters should be never set (or should be ignored)
  // for main-frame / HTTP(s) navigations, but we still compare them here as a
  // defence-in-depth measure.
  CHECK(navigation_request.IsInPrimaryMainFrame());

  // Check `common_params_` and `begin_params_` here as these can be nullptr
  // if LoadURLWithParams failed without running PrerenderNavigationThrottle.
  if (!common_params_ || !begin_params_) {
    return false;
  }

  // Relaxes checks for initiator and transition type. This logic is intended to
  // be used for WebView, as WebView is intended to host embedder-trusted
  // contests.
  bool allow_initiator_and_transition_mismatch =
      web_contents_->GetDelegate()->ShouldAllowPartialParamMismatchOfPrerender2(
          navigation_request);
  // Compare BeginNavigationParams.
  ActivationNavigationParamsMatch result =
      AreBeginNavigationParamsCompatibleWithNavigation(
          navigation_request.begin_params(),
          allow_initiator_and_transition_mismatch, reason);
  if (result != ActivationNavigationParamsMatch::kOk) {
    RecordPrerenderActivationNavigationParamsMatch(result,
                                                   GetHistogramSuffix());
    return false;
  }

  // Compare CommonNavigationParams.
  result = AreCommonNavigationParamsCompatibleWithNavigation(
      navigation_request.common_params(),
      allow_initiator_and_transition_mismatch);
  if (result != ActivationNavigationParamsMatch::kOk) {
    RecordPrerenderActivationNavigationParamsMatch(result,
                                                   GetHistogramSuffix());
    return false;
  }

  RecordPrerenderActivationNavigationParamsMatch(
      ActivationNavigationParamsMatch::kOk, GetHistogramSuffix());
  return true;
}

PrerenderHost::ActivationNavigationParamsMatch
PrerenderHost::AreBeginNavigationParamsCompatibleWithNavigation(
    const blink::mojom::BeginNavigationParams& potential_activation,
    bool allow_initiator_and_transition_mismatch,
    PrerenderCancellationReason& reason) {
  CHECK(begin_params_);

  // TODO(https://crbug.com/340416082): Check details of security properties,
  // update the check to appropriate form and remove differences among all
  // platforms.
  if (!allow_initiator_and_transition_mismatch &&
      (potential_activation.initiator_frame_token !=
       begin_params_->initiator_frame_token)) {
    return ActivationNavigationParamsMatch::kInitiatorFrameToken;
  }

  if (!AreHttpRequestHeadersCompatible(potential_activation.headers,
                                       begin_params_->headers, trigger_type(),
                                       GetHistogramSuffix(), reason)) {
    return ActivationNavigationParamsMatch::kHttpRequestHeader;
  }

  // Don't activate a prerendered page if the potential activation request
  // requires validation or bypass of the browser cache, as the prerendered page
  // is a kind of caches.
  // TODO(crbug.com/40183588): Instead of checking the load flags on
  // activation, we should cancel prerendering when the prerender initial
  // navigation has the flags.
  int cache_load_flags = net::LOAD_VALIDATE_CACHE | net::LOAD_BYPASS_CACHE |
                         net::LOAD_DISABLE_CACHE;
  if (potential_activation.load_flags & cache_load_flags) {
    return ActivationNavigationParamsMatch::kCacheLoadFlags;
  }
  if (potential_activation.load_flags != begin_params_->load_flags) {
    return ActivationNavigationParamsMatch::kLoadFlags;
  }

  if (potential_activation.skip_service_worker !=
      begin_params_->skip_service_worker) {
    return ActivationNavigationParamsMatch::kSkipServiceWorker;
  }

  if (potential_activation.mixed_content_context_type !=
      begin_params_->mixed_content_context_type) {
    return ActivationNavigationParamsMatch::kMixedContentContextType;
  }

  // Initial prerender navigation cannot be a form submission.
  CHECK(!begin_params_->is_form_submission);
  if (potential_activation.is_form_submission !=
      begin_params_->is_form_submission) {
    return ActivationNavigationParamsMatch::kIsFormSubmission;
  }

  if (potential_activation.searchable_form_url !=
      begin_params_->searchable_form_url) {
    return ActivationNavigationParamsMatch::kSearchableFormUrl;
  }

  if (potential_activation.searchable_form_encoding !=
      begin_params_->searchable_form_encoding) {
    return ActivationNavigationParamsMatch::kSearchableFormEncoding;
  }

  // Trust token params can be set only on subframe navigations, so both values
  // should be null here.
  CHECK(!begin_params_->trust_token_params);
  if (potential_activation.trust_token_params !=
      begin_params_->trust_token_params) {
    return ActivationNavigationParamsMatch::kTrustTokenParams;
  }

  // Don't require equality for request_context_type because link clicks
  // (HYPERLINK) should be allowed for activation, whereas prerender always has
  // type LOCATION.
  CHECK_EQ(begin_params_->request_context_type,
           blink::mojom::RequestContextType::LOCATION);
  switch (potential_activation.request_context_type) {
    case blink::mojom::RequestContextType::HYPERLINK:
    case blink::mojom::RequestContextType::LOCATION:
      break;
    default:
      return ActivationNavigationParamsMatch::kRequestContextType;
  }

  // Since impression should not be set, no need to compare contents.
  CHECK(!begin_params_->impression);
  if (potential_activation.impression.has_value()) {
    return ActivationNavigationParamsMatch::kImpressionHasValue;
  }

  // No need to test for devtools_initiator because this field is used for
  // tracking what triggered a network request, and prerender activation will
  // not use network requests.

  return ActivationNavigationParamsMatch::kOk;
}

PrerenderHost::ActivationNavigationParamsMatch
PrerenderHost::AreCommonNavigationParamsCompatibleWithNavigation(
    const blink::mojom::CommonNavigationParams& potential_activation,
    bool allow_initiator_and_transition_mismatch) {
  // The CommonNavigationParams::url field is expected to match both initial and
  // activation prerender navigations, as the PrerenderHost selection would have
  // already checked for matching values. Adding a CHECK here to be safe.
  CHECK(common_params_);
  if (attributes_.url_match_predicate) {
    // TODO(crbug.com/41494389): Figure out what we need to pass here as a
    // web_url_match result instead of std::nullopt.
    CHECK(attributes_.url_match_predicate.Run(potential_activation.url,
                                              std::nullopt));
  } else if (no_vary_search_.has_value()) {
    CHECK(no_vary_search_->AreEquivalent(potential_activation.url,
                                         common_params_->url));
  } else if (no_vary_search_expected().has_value()) {
    CHECK(no_vary_search_expected()->AreEquivalent(potential_activation.url,
                                                   common_params_->url));
  } else {
    CHECK_EQ(potential_activation.url, common_params_->url);
  }

  // TODO(https://crbug.com/340416082): Check details of security properties,
  // update the check to appropriate form and remove differences among all
  // platforms.
  if (!allow_initiator_and_transition_mismatch &&
      (potential_activation.initiator_origin !=
       common_params_->initiator_origin)) {
    return ActivationNavigationParamsMatch::kInitiatorOrigin;
  }

  // The transition must match with the exception of the client redirect flag.
  // The renderer may add the client redirect flag when it has enough
  // information to be certain that this navigation would replace the current
  // history entry (e.g., a renderer-initiated navigation to the current URL).
  int32_t potential_activation_transition =
      potential_activation.transition & ~ui::PAGE_TRANSITION_CLIENT_REDIRECT;
  if (!allow_initiator_and_transition_mismatch &&
      (potential_activation_transition != common_params_->transition)) {
    RecordPrerenderActivationTransition(potential_activation_transition,
                                        GetHistogramSuffix());
    return ActivationNavigationParamsMatch::kTransition;
  }

  CHECK_EQ(common_params_->navigation_type,
           blink::mojom::NavigationType::DIFFERENT_DOCUMENT);
  if (potential_activation.navigation_type != common_params_->navigation_type) {
    return ActivationNavigationParamsMatch::kNavigationType;
  }

  // We don't check download_policy as it affects whether the download triggered
  // by the NavigationRequest is allowed to proceed (or logs metrics) and
  // doesn't affect the behaviour of the document created by a non-download
  // navigation after commit (e.g. it doesn't affect future downloads in child
  // frames). PrerenderNavigationThrottle has already ensured that the initial
  // prerendering navigation isn't a download and as prerendering activation
  // won't reach out to the network, it won't turn into a navigation as well.

  CHECK(common_params_->base_url_for_data_url.is_empty());
  if (potential_activation.base_url_for_data_url !=
      common_params_->base_url_for_data_url) {
    return ActivationNavigationParamsMatch::kBaseUrlForDataUrl;
  }

  // The method parameter is compared only by CHECK_EQ because that change is
  // detected earlier by checking the HTTP request headers changes.
  CHECK_EQ(potential_activation.method, common_params_->method);

  // Initial prerender navigation can't be a form submission.
  CHECK(!common_params_->post_data);
  if (potential_activation.post_data != common_params_->post_data) {
    return ActivationNavigationParamsMatch::kPostData;
  }

  // No need to compare source_location, as it's only passed to the DevTools for
  // debugging purposes and does not impact the properties of the document
  // created by this navigation.

  CHECK(!common_params_->started_from_context_menu);
  if (potential_activation.started_from_context_menu !=
      common_params_->started_from_context_menu) {
    return ActivationNavigationParamsMatch::kStartedFromContextMenu;
  }

  // has_user_gesture doesn't affect any of the security properties of the
  // document created by navigation, so equality of the values is not required.
  // TODO(crbug.com/40191309): ensure that the user activation status is
  // propagated to the activated document.

  // text_fragment_token doesn't affect any of the security properties of the
  // document created by navigation, so equality of the values is not required.
  // TODO(crbug.com/40191311): ensure the activated document consumes
  // text_fragment_token and scrolls to the corresponding viewport.

  // No need to compare should_check_main_world_csp, as if the CSP blocks the
  // initial navigation, it cancels prerendering, and we don't reach here for
  // matching. So regardless of the activation's capability to bypass the main
  // world CSP, the prerendered page is eligible for the activation. This also
  // permits content scripts to activate the page.

  if (potential_activation.initiator_origin_trial_features !=
      common_params_->initiator_origin_trial_features) {
    return ActivationNavigationParamsMatch::kInitiatorOriginTrialFeature;
  }

  if (potential_activation.href_translate != common_params_->href_translate) {
    return ActivationNavigationParamsMatch::kHrefTranslate;
  }

  // Initial prerender navigation can't be a history navigation.
  CHECK(!common_params_->is_history_navigation_in_new_child_frame);
  if (potential_activation.is_history_navigation_in_new_child_frame !=
      common_params_->is_history_navigation_in_new_child_frame) {
    return ActivationNavigationParamsMatch::kIsHistoryNavigationInNewChildFrame;
  }

  // We intentionally don't check referrer or referrer->policy. See spec
  // discussion at https://github.com/WICG/nav-speculation/issues/18.

  if (potential_activation.request_destination !=
      common_params_->request_destination) {
    return ActivationNavigationParamsMatch::kRequestDestination;
  }

  return ActivationNavigationParamsMatch::kOk;
}

RenderFrameHostImpl* PrerenderHost::GetPrerenderedMainFrameHost() {
  CHECK(frame_tree_);
  CHECK(frame_tree_->root()->current_frame_host());
  return frame_tree_->root()->current_frame_host();
}

FrameTree& PrerenderHost::GetPrerenderFrameTree() {
  CHECK(frame_tree_);
  return *frame_tree_;
}

void PrerenderHost::RecordFailedFinalStatus(
    base::PassKey<PrerenderHostRegistry>,
    const PrerenderCancellationReason& reason) {
  RecordFailedFinalStatusImpl(reason);
}

void PrerenderHost::RecordFailedFinalStatusImpl(
    const PrerenderCancellationReason& reason) {
  CHECK(!final_status_);
  CHECK_NE(reason.final_status(), PrerenderFinalStatus::kActivated);
  final_status_ = reason.final_status();
  RecordFailedPrerenderFinalStatus(reason, attributes_);

  // Set failure reason for this PreloadingAttempt specific to the
  // FinalStatus.
  SetFailureReason(reason);
}

void PrerenderHost::RecordActivation(NavigationRequest& navigation_request) {
  CHECK(!final_status_);
  final_status_ = PrerenderFinalStatus::kActivated;

  // TODO(crbug.com/40215894): Replace
  // `navigation_request.GetNextPageUkmSourceId()` with prerendered page's UKM
  // source ID.
  ReportSuccessActivation(attributes_,
                          navigation_request.GetNextPageUkmSourceId());
}

PrerenderHost::LoadingOutcome PrerenderHost::WaitForLoadStopForTesting() {
  LoadingOutcome status = LoadingOutcome::kLoadingCompleted;

  if (!frame_tree_->IsLoadingIncludingInnerFrameTrees() &&
      GetInitialNavigationId().has_value())
    return status;

  base::RunLoop loop;
  on_wait_loading_finished_ = base::BindOnce(
      [](base::OnceClosure on_close, LoadingOutcome* result,
         LoadingOutcome status) {
        *result = status;
        std::move(on_close).Run();
      },
      loop.QuitClosure(), &status);
  loop.Run();
  // Reset callback to null in case if loop is quit by timeout.
  on_wait_loading_finished_.Reset();
  return status;
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

std::optional<int64_t> PrerenderHost::GetInitialNavigationId() const {
  return initial_navigation_id_;
}

void PrerenderHost::SetInitialNavigation(NavigationRequest* navigation) {
  CHECK(!initial_navigation_id_.has_value());
  initial_navigation_id_ = navigation->GetNavigationId();
  begin_params_ = navigation->begin_params().Clone();
  common_params_ = navigation->common_params().Clone();

  // The prerendered page should be checked by the main world CSP. See also
  // relevant comments in AreCommonNavigationParamsCompatibleWithNavigation().
  CHECK_EQ(common_params_->should_check_main_world_csp,
           network::mojom::CSPDisposition::CHECK);
}

void PrerenderHost::SetTriggeringOutcome(PreloadingTriggeringOutcome outcome) {
  if (attempt_) {
    attempt_->SetTriggeringOutcome(outcome);
  }

  if (devtools_attempt_) {
    devtools_attempt_->SetTriggeringOutcome(attributes_, outcome);
  }
}

void PrerenderHost::SetFailureReason(
    const PrerenderCancellationReason& reason) {
  switch (reason.final_status()) {
    // When adding a new failure reason, consider whether it should be
    // propagated to `attempt_`. Most values should be propagated, but we
    // explicitly do not propagate failure reasons if:
    // 1. prerender was successfully prepared but then destroyed because it
    //    wasn't needed for a subsequent navigation (kTriggerDestroyed).
    // 2. the prerender was still pending for its initial navigation when it was
    //    activated (kActivatedBeforeStarted).
    case PrerenderFinalStatus::kTriggerDestroyed:
    case PrerenderFinalStatus::kActivatedBeforeStarted:
    case PrerenderFinalStatus::kTabClosedByUserGesture:
    case PrerenderFinalStatus::kTabClosedWithoutUserGesture:
    case PrerenderFinalStatus::kSpeculationRuleRemoved:
    case PrerenderFinalStatus::kOtherPrerenderedPageActivated:
      return;
    case PrerenderFinalStatus::kDestroyed:
    case PrerenderFinalStatus::kLowEndDevice:
    case PrerenderFinalStatus::kInvalidSchemeRedirect:
    case PrerenderFinalStatus::kInvalidSchemeNavigation:
    case PrerenderFinalStatus::kNavigationRequestBlockedByCsp:
    case PrerenderFinalStatus::kMainFrameNavigation:
    case PrerenderFinalStatus::kMojoBinderPolicy:
    case PrerenderFinalStatus::kRendererProcessCrashed:
    case PrerenderFinalStatus::kRendererProcessKilled:
    case PrerenderFinalStatus::kDownload:
    case PrerenderFinalStatus::kNavigationNotCommitted:
    case PrerenderFinalStatus::kNavigationBadHttpStatus:
    case PrerenderFinalStatus::kClientCertRequested:
    case PrerenderFinalStatus::kNavigationRequestNetworkError:
    case PrerenderFinalStatus::kCancelAllHostsForTesting:
    case PrerenderFinalStatus::kDidFailLoad:
    case PrerenderFinalStatus::kStop:
    case PrerenderFinalStatus::kSslCertificateError:
    case PrerenderFinalStatus::kLoginAuthRequested:
    case PrerenderFinalStatus::kUaChangeRequiresReload:
    case PrerenderFinalStatus::kBlockedByClient:
    case PrerenderFinalStatus::kMixedContent:
    case PrerenderFinalStatus::kTriggerBackgrounded:
    case PrerenderFinalStatus::kMemoryLimitExceeded:
    case PrerenderFinalStatus::kDataSaverEnabled:
    case PrerenderFinalStatus::kTriggerUrlHasEffectiveUrl:
    case PrerenderFinalStatus::kInactivePageRestriction:
    case PrerenderFinalStatus::kStartFailed:
    case PrerenderFinalStatus::kTimeoutBackgrounded:
    case PrerenderFinalStatus::kCrossSiteNavigationInInitialNavigation:
    case PrerenderFinalStatus::kCrossSiteRedirectInInitialNavigation:
    case PrerenderFinalStatus::
        kSameSiteCrossOriginRedirectNotOptInInInitialNavigation:
    case PrerenderFinalStatus::
        kSameSiteCrossOriginNavigationNotOptInInInitialNavigation:
    case PrerenderFinalStatus::kActivationNavigationParameterMismatch:
    case PrerenderFinalStatus::kActivatedInBackground:
    case PrerenderFinalStatus::kEmbedderHostDisallowed:
    case PrerenderFinalStatus::kActivationNavigationDestroyedBeforeSuccess:
    case PrerenderFinalStatus::kPrimaryMainFrameRendererProcessCrashed:
    case PrerenderFinalStatus::kPrimaryMainFrameRendererProcessKilled:
    case PrerenderFinalStatus::kActivationFramePolicyNotCompatible:
    case PrerenderFinalStatus::kPreloadingDisabled:
    case PrerenderFinalStatus::kBatterySaverEnabled:
    case PrerenderFinalStatus::kActivatedDuringMainFrameNavigation:
    case PrerenderFinalStatus::kPreloadingUnsupportedByWebContents:
    case PrerenderFinalStatus::
        kSameSiteCrossOriginNavigationNotOptInInMainFrameNavigation:
    case PrerenderFinalStatus::
        kSameSiteCrossOriginRedirectNotOptInInMainFrameNavigation:
    case PrerenderFinalStatus::kCrossSiteNavigationInMainFrameNavigation:
    case PrerenderFinalStatus::kCrossSiteRedirectInMainFrameNavigation:
    case PrerenderFinalStatus::kMemoryPressureOnTrigger:
    case PrerenderFinalStatus::kMemoryPressureAfterTriggered:
    case PrerenderFinalStatus::kPrerenderingDisabledByDevTools:
    case PrerenderFinalStatus::kActivatedWithAuxiliaryBrowsingContexts:
    case PrerenderFinalStatus::kMaxNumOfRunningEagerPrerendersExceeded:
    case PrerenderFinalStatus::kMaxNumOfRunningNonEagerPrerendersExceeded:
    case PrerenderFinalStatus::kMaxNumOfRunningEmbedderPrerendersExceeded:
    case PrerenderFinalStatus::kPrerenderingUrlHasEffectiveUrl:
    case PrerenderFinalStatus::kRedirectedPrerenderingUrlHasEffectiveUrl:
    case PrerenderFinalStatus::kActivationUrlHasEffectiveUrl:
    case PrerenderFinalStatus::kJavaScriptInterfaceAdded:
    case PrerenderFinalStatus::kJavaScriptInterfaceRemoved:
    case PrerenderFinalStatus::kAllPrerenderingCanceled:
    case PrerenderFinalStatus::kWindowClosed:
    case PrerenderFinalStatus::kSlowNetwork:
    case PrerenderFinalStatus::kV8OptimizerDisabled:
      if (attempt_) {
        attempt_->SetFailureReason(
            ToPreloadingFailureReason(reason.final_status()));
        // We reset the attempt to ensure we don't update once we have reported
        // it as failure or accidentally use it for any other prerender attempts
        // as PrerenderHost deletion is async.
        attempt_.reset();
      }

      if (devtools_attempt_) {
        devtools_attempt_->SetFailureReason(attributes_, reason);
        devtools_attempt_.reset();
      }

      return;
    case PrerenderFinalStatus::kActivated:
      // The activation path does not call this method, so it should never reach
      // this case.
      NOTREACHED();
  }
}

std::optional<UrlMatchType> PrerenderHost::IsUrlMatch(const GURL& url) const {
  // Triggers are not allowed to treat a cross-origin url as a matched url. It
  // would cause security risks.
  if (!url::IsSameOriginWith(attributes_.prerendering_url, url)) {
    return std::nullopt;
  }

  std::optional<UrlMatchType> result;

  if (GetInitialUrl() == url) {
    result = UrlMatchType::kExact;
  }

  // Check No-Vary-Search header and try and match.
  if (!result && no_vary_search_.has_value() &&
      no_vary_search_->AreEquivalent(GetInitialUrl(), url)) {
    result = UrlMatchType::kNoVarySearch;
  }

  if (!attributes_.url_match_predicate) {
    return result;
  }

  // Override the result of default url match logic with the result
  // from the custom url matching predicate call.
  if (attributes_.url_match_predicate.Run(url, result)) {
    return UrlMatchType::kURLPredicateMatch;
  }

  return std::nullopt;
}

bool PrerenderHost::IsNoVarySearchHintUrlMatch(const GURL& url) const {
  // Triggers are not allowed to treat a cross-origin url as a matched url. It
  // would cause security risks.
  if (!url::IsSameOriginWith(attributes_.prerendering_url, url)) {
    return false;
  }

  // We don't care about url_match_predicate here because it is applied only
  // if we know for sure url is a match. This is a "potential"
  // match depending on the No-Vary-Search header that will be received.
  if (attributes_.url_match_predicate) {
    return false;
  }
  // The same as above. We also don't care about the exact match.
  if (GetInitialUrl() == url) {
    return false;
  }

  // Let's check if this PrerenderHost would match by
  // No-Vary-Search hint. We need to check if the headers were already received.
  if (!were_headers_received()) {
    if (no_vary_search_expected().has_value() &&
        no_vary_search_expected()->AreEquivalent(GetInitialUrl(), url)) {
      return true;
    }
  }

  return false;
}

void PrerenderHost::OnAcceptClientHintChanged(
    const url::Origin& origin,
    const std::vector<network::mojom::WebClientHintsType>& client_hints_type) {
  client_hints_type_[origin] = client_hints_type;
}

void PrerenderHost::GetAllowedClientHintsOnPage(
    const url::Origin& origin,
    blink::EnabledClientHints* client_hints) const {
  if (!client_hints_type_.contains(origin))
    return;
  for (const auto& hint : client_hints_type_.at(origin)) {
    client_hints->SetIsEnabled(hint, true);
  }
}

std::string PrerenderHost::GetHistogramSuffix() const {
  return metric_suffix_;
}

void PrerenderHost::Cancel(PrerenderFinalStatus status) {
  TRACE_EVENT("navigation", "PrerenderHost::Cancel", "final_status", status);
  // Already cancelled.
  if (final_status_)
    return;

  RenderFrameHostImpl* host = PrerenderHost::GetPrerenderedMainFrameHost();
  CHECK(host);
  PrerenderHostRegistry* registry =
      host->delegate()->GetPrerenderHostRegistry();
  CHECK(registry);
  registry->CancelHost(frame_tree_node_id_, status);
}

void PrerenderHost::MaybeSetNoVarySearch(
    network::mojom::NoVarySearchWithParseError&
        no_vary_search_with_parse_error) {
  CHECK(!no_vary_search_);
  CHECK(!no_vary_search_parse_error_);
  if (no_vary_search_with_parse_error.is_parse_error()) {
    no_vary_search_parse_error_ =
        no_vary_search_with_parse_error.get_parse_error();
    return;
  }
  CHECK(no_vary_search_with_parse_error.is_no_vary_search());
  net::HttpNoVarySearchData no_vary_search =
      no_vary_search::ParseHttpNoVarySearchDataFromMojom(
          no_vary_search_with_parse_error.get_no_vary_search());
  if (attempt_) {
    static_cast<PreloadingAttemptImpl*>(attempt_.get())
        ->SetNoVarySearchMatchPredicate(base::BindRepeating(
            [](net::HttpNoVarySearchData no_vary_search, const GURL& a,
               const GURL& b) { return no_vary_search.AreEquivalent(a, b); },
            no_vary_search, GetInitialUrl()));
  }
  no_vary_search_ = std::move(no_vary_search);
}

bool PrerenderHost::IsInitialNavigation(
    const NavigationRequest& navigation_request) const {
  return GetInitialNavigationId() == navigation_request.GetNavigationId();
}

base::TimeDelta PrerenderHost::WaitUntilHeadTimeout() {
  int timeout_in_milliseconds = 0;
  if (IsSpeculationRuleType(attributes_.trigger_type)) {
    CHECK(attributes_.eagerness.has_value());
    switch (attributes_.eagerness.value()) {
      case blink::mojom::SpeculationEagerness::kEager:
        timeout_in_milliseconds =
            features::kPrerender2NoVarySearchWaitForHeadersTimeoutEagerPrerender
                .Get();
        break;
      case blink::mojom::SpeculationEagerness::kModerate:
        timeout_in_milliseconds =
            features::
                kPrerender2NoVarySearchWaitForHeadersTimeoutModeratePrerender
                    .Get();
        break;
      case blink::mojom::SpeculationEagerness::kConservative:
        timeout_in_milliseconds =
            features::
                kPrerender2NoVarySearchWaitForHeadersTimeoutConservativePrerender
                    .Get();
        break;
    }
  } else {
    timeout_in_milliseconds =
        features::kPrerender2NoVarySearchWaitForHeadersTimeoutForEmbedders
            .Get();
  }
  return base::Milliseconds(timeout_in_milliseconds);
}

void PrerenderHost::OnWaitingForHeadersStarted(
    NavigationHandle& navigation_handle,
    WaitingForHeadersStartedReason reason) {
  // Prerender frame tree is alive. This check is also done by the caller.
  CHECK(frame_tree_);
  for (auto& observer : observers_) {
    observer.OnWaitingForHeadersStarted(navigation_handle, reason);
  }
}

void PrerenderHost::OnWaitingForHeadersFinished(
    NavigationHandle& navigation_handle,
    WaitingForHeadersFinishedReason reason) {
  // Prerender frame tree is alive. This check is also done by the caller.
  CHECK(frame_tree_);

  base::UmaHistogramEnumeration(
      "Prerender.Experimental.WaitingForHeadersFinishedReason" +
          GetHistogramSuffix(),
      reason);

  for (auto& observer : observers_) {
    observer.OnWaitingForHeadersFinished(navigation_handle, reason);
  }
}

}  // namespace content
