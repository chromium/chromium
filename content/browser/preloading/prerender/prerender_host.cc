// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_host.h"

#include "base/feature_list.h"
#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_conversion_helper.h"
#include "base/trace_event/typed_macros.h"
#include "content/browser/client_hints/client_hints.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/preloading/prerender/prerender_metrics.h"
#include "content/browser/preloading/prerender/prerender_page_holder.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/site_info.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/prerender_trigger_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/referrer.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "url/origin.h"

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

PreloadingFailureReason ToPreloadingFailureReason(
    PrerenderHost::FinalStatus status) {
  return static_cast<PreloadingFailureReason>(
      static_cast<int>(status) +
      static_cast<int>(
          PreloadingFailureReason::kPreloadingFailureReasonCommonEnd));
}

}  // namespace

// static
PrerenderHost* PrerenderHost::GetPrerenderHostFromFrameTreeNode(
    FrameTreeNode& frame_tree_node) {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(WebContentsImpl::FromRenderFrameHost(
          frame_tree_node.current_frame_host()));
  DCHECK(web_contents);
  PrerenderHostRegistry* prerender_registry =
      web_contents->GetPrerenderHostRegistry();
  int prerender_host_id =
      frame_tree_node.frame_tree()->root()->frame_tree_node_id();

  if (PrerenderHost* host =
          prerender_registry->FindNonReservedHostById(prerender_host_id)) {
    return host;
  } else {
    // TODO(https://crbug.com/1355279): This function can be called during
    // prerender activation so we have to call FindReservedHostById here and
    // give it another shot. Consider using delegate after PrerenderHost
    // implements FrameTree::Delegate.
    return prerender_registry->FindReservedHostById(prerender_host_id);
  }
}

PrerenderHost::PrerenderHost(const PrerenderAttributes& attributes,
                             WebContents& web_contents,
                             base::WeakPtr<PreloadingAttempt> attempt)
    : attributes_(attributes), attempt_(std::move(attempt)) {
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
    observer.OnHostDestroyed(final_status_.value_or(FinalStatus::kDestroyed));

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

  // Even when LoadURLWithParams() returns a valid navigation handle, navigation
  // can fail during navigation start, for example, due to prerendering a
  // non-supported URL scheme that is filtered out in
  // PrerenderNavigationThrottle.
  if (final_status_.has_value())
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

  const bool is_inside_prerender_frame_tree =
      navigation_request->frame_tree_node()->frame_tree() ==
      page_holder_->frame_tree();
  // Observe navigation only in the prerendering frame tree.
  if (!is_inside_prerender_frame_tree)
    return;

  const bool is_prerender_main_frame =
      navigation_request->GetFrameTreeNodeId() == frame_tree_node_id_;

  if (is_prerender_main_frame) {
    GetPrerenderedMainFrameHost()
        ->delegate()
        ->GetPrerenderHostRegistry()
        ->OnPrerenderNavigationFinished(
            navigation_request->GetFrameTreeNodeId());
  }

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

    // Prerender is ready to activate. Set the status to kReady.
    SetTriggeringOutcome(PreloadingTriggeringOutcome::kReady);
  }
}

void PrerenderHost::OnVisibilityChanged(Visibility visibility) {
  TRACE_EVENT("navigation", "PrerenderHost::OnVisibilityChanged");
  // Keep prerenderings alive in the background when their visibility state
  // changes to HIDDEN if the feature is enabled.
  if (base::FeatureList::IsEnabled(blink::features::kPrerender2InBackground))
    return;

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

  FrameTree& target_frame_tree = page_holder_->GetPrimaryFrameTree();
  std::unique_ptr<StoredPage> page = page_holder_->Activate(navigation_request);

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

  // TODO(crbug.com/1299330): Replace
  // `navigation_request.GetNextPageUkmSourceId()` with prerendered page's UKM
  // source ID.
  RecordFinalStatus(FinalStatus::kActivated, attributes_.initiator_ukm_id,
                    navigation_request.GetNextPageUkmSourceId());

  // Prerender is activated. Set the status to kSuccess.
  SetTriggeringOutcome(PreloadingTriggeringOutcome::kSuccess);

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
  ActivationNavigationParamsMatch result =
      AreBeginNavigationParamsCompatibleWithNavigation(
          navigation_request.begin_params());
  if (result != ActivationNavigationParamsMatch::kOk) {
    RecordPrerenderActivationNavigationParamsMatch(result, trigger_type(),
                                                   embedder_histogram_suffix());
    return false;
  }

  // Compare CommonNavigationParams.
  result = AreCommonNavigationParamsCompatibleWithNavigation(
      navigation_request.common_params());
  if (result != ActivationNavigationParamsMatch::kOk) {
    RecordPrerenderActivationNavigationParamsMatch(result, trigger_type(),
                                                   embedder_histogram_suffix());
    return false;
  }

  RecordPrerenderActivationNavigationParamsMatch(
      ActivationNavigationParamsMatch::kOk, trigger_type(),
      embedder_histogram_suffix());
  return true;
}

PrerenderHost::ActivationNavigationParamsMatch
PrerenderHost::AreBeginNavigationParamsCompatibleWithNavigation(
    const blink::mojom::BeginNavigationParams& potential_activation) {
  if (potential_activation.initiator_frame_token !=
      begin_params_->initiator_frame_token) {
    return ActivationNavigationParamsMatch::kInitiatorFrameToken;
  }

  if (!AreHttpRequestHeadersCompatible(potential_activation.headers,
                                       begin_params_->headers)) {
    return ActivationNavigationParamsMatch::kHttpRequestHeader;
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
  DCHECK(!begin_params_->is_form_submission);
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
  DCHECK(!begin_params_->trust_token_params);
  if (potential_activation.trust_token_params !=
      begin_params_->trust_token_params) {
    return ActivationNavigationParamsMatch::kTrustTokenParams;
  }

  // Web bundle token cannot be set due because it is only set for child
  // frame navigations.
  DCHECK(!begin_params_->web_bundle_token);
  if (potential_activation.web_bundle_token) {
    return ActivationNavigationParamsMatch::kWebBundleToken;
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
      return ActivationNavigationParamsMatch::kRequestContextType;
  }

  // Since impression should not be set, no need to compare contents.
  DCHECK(!begin_params_->impression);
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
    return ActivationNavigationParamsMatch::kInitiatorOrigin;
  }

  if (potential_activation.transition != common_params_->transition) {
    return ActivationNavigationParamsMatch::kTransition;
  }

  DCHECK_EQ(common_params_->navigation_type,
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

  DCHECK(common_params_->base_url_for_data_url.is_empty());
  if (potential_activation.base_url_for_data_url !=
      common_params_->base_url_for_data_url) {
    return ActivationNavigationParamsMatch::kBaseUrlForDataUrl;
  }

  // The method parameter is compared only by DCHECK_EQ because that change is
  // detected earlier by checking the HTTP request headers changes.
  DCHECK_EQ(potential_activation.method, common_params_->method);

  // Initial prerender navigation can't be a form submission.
  DCHECK(!common_params_->post_data);
  if (potential_activation.post_data != common_params_->post_data) {
    return ActivationNavigationParamsMatch::kPostData;
  }

  // No need to compare source_location, as it's only passed to the DevTools for
  // debugging purposes and does not impact the properties of the document
  // created by this navigation.

  DCHECK(!common_params_->started_from_context_menu);
  if (potential_activation.started_from_context_menu !=
      common_params_->started_from_context_menu) {
    return ActivationNavigationParamsMatch::kStartedFromContextMenu;
  }

  // has_user_gesture doesn't affect any of the security properties of the
  // document created by navigation, so equality of the values is not required.
  // TODO(crbug.com/1232915): ensure that the user activation status is
  // propagated to the activated document.

  // text_fragment_token doesn't affect any of the security properties of the
  // document created by navigation, so equality of the values is not required.
  // TODO(crbug.com/1232919): ensure the activated document consumes
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
  DCHECK(!common_params_->is_history_navigation_in_new_child_frame);
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

  // Set failure reason for this PreloadingAttempt specific to the
  // FinalStatus.
  SetFailureReason(status);
}

void PrerenderHost::CreatePageHolder(WebContentsImpl& web_contents) {
  page_holder_ = std::make_unique<PrerenderPageHolder>(web_contents);
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

  // The prerendered page should be checked by the main world CSP. See also
  // relevant comments in AreCommonNavigationParamsCompatibleWithNavigation().
  DCHECK_EQ(common_params_->should_check_main_world_csp,
            network::mojom::CSPDisposition::CHECK);
}

void PrerenderHost::SetTriggeringOutcome(PreloadingTriggeringOutcome outcome) {
  if (!attempt_)
    return;

  attempt_->SetTriggeringOutcome(outcome);
}

void PrerenderHost::SetEligibility(PreloadingEligibility eligibility) {
  if (!attempt_)
    return;

  attempt_->SetEligibility(eligibility);
}

void PrerenderHost::SetFailureReason(FinalStatus status) {
  if (!attempt_)
    return;

  switch (status) {
    // When adding a new failure reason, consider whether it should be
    // propagated to `attempt_`. Most values should be propagated, but we
    // explicitly do not propagate failure reasons if the prerender was actually
    // successful (kActivated), or if prerender was successfully prepared but
    // then destroyed because it wasn't needed for a subsequent navigation
    // (kTriggerDestroyed).
    case FinalStatus::kActivated:
    case FinalStatus::kTriggerDestroyed:
      return;
    case FinalStatus::kDestroyed:
    case FinalStatus::kLowEndDevice:
    case FinalStatus::kCrossOriginRedirect:
    case FinalStatus::kCrossOriginNavigation:
    case FinalStatus::kInvalidSchemeRedirect:
    case FinalStatus::kInvalidSchemeNavigation:
    case FinalStatus::kInProgressNavigation:
    case FinalStatus::kNavigationRequestBlockedByCsp:
    case FinalStatus::kMainFrameNavigation:
    case FinalStatus::kMojoBinderPolicy:
    case FinalStatus::kRendererProcessCrashed:
    case FinalStatus::kRendererProcessKilled:
    case FinalStatus::kDownload:
    case FinalStatus::kNavigationNotCommitted:
    case FinalStatus::kNavigationBadHttpStatus:
    case FinalStatus::kClientCertRequested:
    case FinalStatus::kNavigationRequestNetworkError:
    case FinalStatus::kMaxNumOfRunningPrerendersExceeded:
    case FinalStatus::kCancelAllHostsForTesting:
    case FinalStatus::kDidFailLoad:
    case FinalStatus::kStop:
    case FinalStatus::kSslCertificateError:
    case FinalStatus::kLoginAuthRequested:
    case FinalStatus::kUaChangeRequiresReload:
    case FinalStatus::kBlockedByClient:
    case FinalStatus::kAudioOutputDeviceRequested:
    case FinalStatus::kMixedContent:
    case FinalStatus::kTriggerBackgrounded:
    case FinalStatus::kEmbedderTriggeredAndSameOriginRedirected:
    case FinalStatus::kEmbedderTriggeredAndCrossOriginRedirected:
    case FinalStatus::kMemoryLimitExceeded:
    case FinalStatus::kFailToGetMemoryUsage:
    case FinalStatus::kDataSaverEnabled:
    case FinalStatus::kHasEffectiveUrl:
    case FinalStatus::kActivatedBeforeStarted:
      attempt_->SetFailureReason(ToPreloadingFailureReason(status));
      // We reset the attempt to ensure we don't update once we have reported it
      // as failure or accidentally use it for any other prerender attempts as
      // PrerenderHost deletion is async.
      attempt_.reset();
      return;
  }
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
