// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_navigation_throttle.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/preloading/prerender/prerender_metrics.h"
#include "content/browser/preloading/prerender/prerender_navigation_utils.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/public/browser/prerender_trigger_type.h"
#include "content/public/common/content_features.h"
#include "services/network/public/mojom/parsed_headers.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

namespace {

// For the given two origins, analyze what kind of redirection happened.
void AnalyzeCrossOriginRedirection(
    const url::Origin& current_origin,
    const url::Origin& initial_origin,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix) {
  DCHECK_NE(initial_origin, current_origin);
  DCHECK_EQ(trigger_type, PrerenderTriggerType::kEmbedder);
  DCHECK(current_origin.GetURL().SchemeIsHTTPOrHTTPS());
  DCHECK(initial_origin.GetURL().SchemeIsHTTPOrHTTPS());

  std::bitset<3> bits;
  bits[2] = current_origin.scheme() != initial_origin.scheme();
  bits[1] = current_origin.host() != initial_origin.host();
  bits[0] = current_origin.port() != initial_origin.port();
  DCHECK(bits.any());
  auto mismatch_type =
      static_cast<PrerenderCrossOriginRedirectionMismatch>(bits.to_ulong());

  RecordPrerenderRedirectionMismatchType(mismatch_type, trigger_type,
                                         embedder_histogram_suffix);

  if (mismatch_type ==
      PrerenderCrossOriginRedirectionMismatch::kSchemePortMismatch) {
    RecordPrerenderRedirectionProtocolChange(
        current_origin.scheme() == url::kHttpsScheme
            ? PrerenderCrossOriginRedirectionProtocolChange::
                  kHttpProtocolUpgrade
            : PrerenderCrossOriginRedirectionProtocolChange::
                  kHttpProtocolDowngrade,
        trigger_type, embedder_histogram_suffix);
    return;
  }
  if (mismatch_type == PrerenderCrossOriginRedirectionMismatch::kHostMismatch) {
    if (current_origin.DomainIs(initial_origin.host())) {
      RecordPrerenderRedirectionDomain(
          PrerenderCrossOriginRedirectionDomain::kRedirectToSubDomain,
          trigger_type, embedder_histogram_suffix);
      return;
    }
    RecordPrerenderRedirectionDomain(
        initial_origin.DomainIs(current_origin.host())
            ? PrerenderCrossOriginRedirectionDomain::kRedirectFromSubDomain
            : PrerenderCrossOriginRedirectionDomain::kCrossDomain,
        trigger_type, embedder_histogram_suffix);
  }
}

// Prerender2 Embedders trigger based on rules decided by the browser. Prevent
// the browser from triggering on the hosts listed.
// Blocked hosts are expected to be passed as a comma separated string.
// e.g. example1.test,example2.test
const base::FeatureParam<std::string> kPrerender2EmbedderBlockedHosts{
    &blink::features::kPrerender2, "embedder_blocked_hosts", ""};

bool ShouldSkipHostInBlockList(const GURL& url) {
  if (!base::FeatureList::IsEnabled(blink::features::kPrerender2)) {
    return false;
  }

  // Keep this as static because the blocked origins are served via feature
  // parameters and are never changed until browser restart.
  const static base::NoDestructor<std::vector<std::string>>
      embedder_blocked_hosts(
          base::SplitString(kPrerender2EmbedderBlockedHosts.Get(), ",",
                            base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY));

  return base::Contains(*embedder_blocked_hosts, url.host());
}

}  // namespace

PrerenderNavigationThrottle::~PrerenderNavigationThrottle() = default;

// static
std::unique_ptr<PrerenderNavigationThrottle>
PrerenderNavigationThrottle::MaybeCreateThrottleFor(
    NavigationHandle* navigation_handle) {
  auto* navigation_request = NavigationRequest::From(navigation_handle);
  FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
  if (frame_tree_node->GetFrameType() == FrameType::kPrerenderMainFrame) {
    PrerenderHost* prerender_host =
        static_cast<PrerenderHost*>(frame_tree_node->frame_tree().delegate());
    DCHECK(prerender_host);

    return base::WrapUnique(new PrerenderNavigationThrottle(navigation_handle));
  }
  return nullptr;
}

const char* PrerenderNavigationThrottle::GetNameForLogging() {
  return "PrerenderNavigationThrottle";
}

NavigationThrottle::ThrottleCheckResult
PrerenderNavigationThrottle::WillStartRequest() {
  return WillStartOrRedirectRequest(/*is_redirection=*/false);
}

NavigationThrottle::ThrottleCheckResult
PrerenderNavigationThrottle::WillRedirectRequest() {
  return WillStartOrRedirectRequest(/*is_redirection=*/true);
}

PrerenderNavigationThrottle::PrerenderNavigationThrottle(
    NavigationHandle* navigation_handle)
    : NavigationThrottle(navigation_handle) {
  auto* navigation_request = NavigationRequest::From(navigation_handle);
  PrerenderHost* prerender_host = static_cast<PrerenderHost*>(
      navigation_request->frame_tree_node()->frame_tree().delegate());
  DCHECK(prerender_host);

  // This throttle is responsible for setting the initial navigation id on the
  // PrerenderHost, since the PrerenderHost obtains the NavigationRequest,
  // which has the ID, only after the navigation throttles run.
  if (prerender_host->GetInitialNavigationId().has_value()) {
    // If the host already has an initial navigation id, this throttle
    // will later cancel the navigation in Will*Request(). Just do nothing
    // until then.
  } else {
    prerender_host->SetInitialNavigation(
        static_cast<NavigationRequest*>(navigation_handle));
  }
}

NavigationThrottle::ThrottleCheckResult
PrerenderNavigationThrottle::WillStartOrRedirectRequest(bool is_redirection) {
  // Take the root frame tree node of the prerendering page.
  auto* navigation_request = NavigationRequest::From(navigation_handle());
  FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
  DCHECK_EQ(frame_tree_node->GetFrameType(), FrameType::kPrerenderMainFrame);

  PrerenderHostRegistry* prerender_host_registry =
      frame_tree_node->current_frame_host()
          ->delegate()
          ->GetPrerenderHostRegistry();

  // Get the prerender host of the prerendering page.
  PrerenderHost* prerender_host =
      static_cast<PrerenderHost*>(frame_tree_node->frame_tree().delegate());
  DCHECK(prerender_host);

  // Navigations after the initial prerendering navigation are disallowed.
  if (*prerender_host->GetInitialNavigationId() !=
      navigation_request->GetNavigationId()) {
    prerender_host_registry->CancelHost(
        frame_tree_node->frame_tree_node_id(),
        PrerenderFinalStatus::kMainFrameNavigation);
    return CANCEL;
  }

  GURL prerendering_url = navigation_handle()->GetURL();

  if ((prerender_host->trigger_type() == PrerenderTriggerType::kEmbedder) &&
      ShouldSkipHostInBlockList(prerendering_url)) {
    prerender_host_registry->CancelHost(
        frame_tree_node->frame_tree_node_id(),
        PrerenderFinalStatus::kEmbedderHostDisallowed);
    return CANCEL;
  }

  // Allow only HTTP(S) schemes.
  // https://wicg.github.io/nav-speculation/prerendering.html#no-bad-navs
  if (!prerendering_url.SchemeIsHTTPOrHTTPS()) {
    prerender_host_registry->CancelHost(
        frame_tree_node->frame_tree_node_id(),
        is_redirection ? PrerenderFinalStatus::kInvalidSchemeRedirect
                       : PrerenderFinalStatus::kInvalidSchemeNavigation);
    return CANCEL;
  }

  url::Origin prerendering_origin = url::Origin::Create(prerendering_url);
  if (!prerender_host->IsBrowserInitiated() &&
      prerendering_origin == prerender_host->initiator_origin()) {
    is_same_site_cross_origin_prerender_ =
        same_site_cross_origin_prerender_did_redirect_ = false;
  }

  if (prerender_host->IsBrowserInitiated()) {
    // Cancel an embedder triggered prerendering if it is redirected to a URL
    // cross-site to the initial prerendering URL.
    if (is_redirection) {
      url::Origin initial_origin =
          url::Origin::Create(prerender_host->GetInitialUrl());
      bool is_same_site = prerender_navigation_utils::IsSameSite(
          prerendering_url, initial_origin);
      if (!is_same_site) {
        AnalyzeCrossOriginRedirection(
            prerendering_origin, initial_origin, prerender_host->trigger_type(),
            prerender_host->embedder_histogram_suffix());
        prerender_host_registry->CancelHost(
            frame_tree_node->frame_tree_node_id(),
            PrerenderFinalStatus::kCrossSiteRedirect);
        return CANCEL;
      }
      if (!base::FeatureList::IsEnabled(
              blink::features::
                  kSameSiteRedirectionForEmbedderTriggeredPrerender) &&
          !initial_origin.IsSameOriginWith(prerendering_url)) {
        // This branch is used to enforce disable the feature. So, no need to
        // perform analysis here.
        prerender_host_registry->CancelHost(
            frame_tree_node->frame_tree_node_id(),
            PrerenderFinalStatus::kSameSiteCrossOriginRedirect);
        return CANCEL;
      }
    }

    // Skip the same-origin check for non-redirected cases as the initiator
    // origin is nullopt for browser-initiated prerendering.
    DCHECK(!prerender_host->initiator_origin().has_value());
  } else if (!prerender_navigation_utils::IsSameSite(
                 prerendering_url,
                 prerender_host->initiator_origin().value())) {
    // TODO(crbug.com/1176054): Once cross-site prerendering is implemented,
    // we'll need to enforce strict referrer policies
    // (https://wicg.github.io/nav-speculation/prefetch.html#list-of-sufficiently-strict-speculative-navigation-referrer-policies).
    //
    // Cancel prerendering if this is cross-site prerendering, cross-site
    // redirection during prerendering, or cross-site navigation from a
    // prerendered page.
    prerender_host_registry->CancelHost(
        frame_tree_node->frame_tree_node_id(),
        is_redirection ? PrerenderFinalStatus::kCrossSiteRedirect
                       : PrerenderFinalStatus::kCrossSiteNavigation);
    return CANCEL;
  } else if (prerendering_origin != prerender_host->initiator_origin()) {
    if (blink::features::
            IsSameSiteCrossOriginForSpeculationRulesPrerender2Enabled()) {
      is_same_site_cross_origin_prerender_ = true;
      same_site_cross_origin_prerender_did_redirect_ = is_redirection;
    } else {
      prerender_host_registry->CancelHost(
          frame_tree_node->frame_tree_node_id(),
          is_redirection
              ? PrerenderFinalStatus::kSameSiteCrossOriginRedirect
              : PrerenderFinalStatus::kSameSiteCrossOriginNavigation);
      return CANCEL;
    }
  }

  return PROCEED;
}

NavigationThrottle::ThrottleCheckResult
PrerenderNavigationThrottle::WillProcessResponse() {
  auto* navigation_request = NavigationRequest::From(navigation_handle());

  FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
  DCHECK_EQ(frame_tree_node->GetFrameType(), FrameType::kPrerenderMainFrame);

  PrerenderHostRegistry* prerender_host_registry =
      frame_tree_node->current_frame_host()
          ->delegate()
          ->GetPrerenderHostRegistry();

  // https://wicg.github.io/nav-speculation/prerendering.html#navigate-fetch-patch
  // "1. If browsingContext is a prerendering browsing context and
  // responseOrigin is not same origin with incumbentNavigationOrigin, then:"
  // "1.1. Let loadingModes be the result of getting the supported loading
  // modes for response."
  // "1.2. If loadingModes does not contain `credentialed-prerender`, then
  // set response to a network error."
  bool is_credentialed_prerender =
      navigation_request->response() &&
      navigation_request->response()->parsed_headers &&
      base::Contains(
          navigation_request->response()->parsed_headers->supports_loading_mode,
          network::mojom::LoadingMode::kCredentialedPrerender);
  if (!is_credentialed_prerender && is_same_site_cross_origin_prerender_) {
    prerender_host_registry->CancelHost(
        frame_tree_node->frame_tree_node_id(),
        same_site_cross_origin_prerender_did_redirect_
            ? PrerenderFinalStatus::kSameSiteCrossOriginRedirectNotOptIn
            : PrerenderFinalStatus::kSameSiteCrossOriginNavigationNotOptIn);
    return CANCEL;
  }

  absl::optional<PrerenderFinalStatus> cancel_reason;

  // TODO(crbug.com/1318739): Delay until activation instead of cancellation.
  if (navigation_handle()->IsDownload()) {
    // Disallow downloads during prerendering and cancel the prerender.
    cancel_reason = PrerenderFinalStatus::kDownload;
  } else if (prerender_navigation_utils::IsDisallowedHttpResponseCode(
                 navigation_request->commit_params().http_response_code)) {
    // There's no point in trying to prerender failed navigations.
    cancel_reason = PrerenderFinalStatus::kNavigationBadHttpStatus;
  }

  if (cancel_reason.has_value()) {
    prerender_host_registry->CancelHost(frame_tree_node->frame_tree_node_id(),
                                        cancel_reason.value());
    return CANCEL;
  }
  return PROCEED;
}

}  // namespace content
