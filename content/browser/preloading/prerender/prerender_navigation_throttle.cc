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
    return base::WrapUnique(
        new PrerenderNavigationThrottle(navigation_request));
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
    NavigationRequest* navigation_request)
    : NavigationThrottle(navigation_request),
      prerender_host_(static_cast<PrerenderHost*>(
          navigation_request->frame_tree_node()->frame_tree().delegate())) {
  CHECK(prerender_host_);

  // This throttle is responsible for setting the initial navigation id on the
  // PrerenderHost, since the PrerenderHost obtains the NavigationRequest,
  // which has the ID, only after the navigation throttles run.
  if (prerender_host_->GetInitialNavigationId().has_value()) {
    // If the host already has an initial navigation id, this throttle
    // will later cancel the navigation in Will*Request(). Just do nothing
    // until then.
  } else {
    prerender_host_->SetInitialNavigation(navigation_request);
  }
}

NavigationThrottle::ThrottleCheckResult
PrerenderNavigationThrottle::WillStartOrRedirectRequest(bool is_redirection) {
  GURL navigation_url = navigation_handle()->GetURL();
  url::Origin navigation_origin = url::Origin::Create(navigation_url);
  url::Origin initial_prerendering_origin =
      url::Origin::Create(prerender_host_->GetInitialUrl());

  // Reset the flags that should be calculated every time redirction happens.
  is_same_site_cross_origin_prerender_ = false;
  same_site_cross_origin_prerender_did_redirect_ = false;

  // Origin checks for the main frame navigation (redirection) happens after the
  // initial prerendering navigation in a prerendered page. Compare the origin
  // of the initial prerendering URL to the origin of navigation (redirection)
  // URL.
  if (!IsInitialNavigation()) {
    if (!base::FeatureList::IsEnabled(
            blink::features::kPrerender2MainFrameNavigation)) {
      // Navigations after the initial prerendering navigation are disallowed
      // when the kPrerender2MainFrameNavigation feature is disabled.
      CancelPrerendering(PrerenderFinalStatus::kMainFrameNavigation);
      return CANCEL;
    }

    // Cross-site navigations after the initial prerendering navigation are
    // disallowed.
    if (prerender_navigation_utils::IsCrossSite(navigation_url,
                                                initial_prerendering_origin)) {
      CancelPrerendering(
          is_redirection
              ? PrerenderFinalStatus::kCrossSiteRedirectInMainFrameNavigation
              : PrerenderFinalStatus::
                    kCrossSiteNavigationInMainFrameNavigation);
      return CANCEL;
    }

    // Same-site cross-origin prerendering is allowed only when the opt-in
    // header is specified on response. This will be checked on
    // WillProcessResponse().
    if (prerender_navigation_utils::IsSameSiteCrossOrigin(
            navigation_url, initial_prerendering_origin)) {
      is_same_site_cross_origin_prerender_ = true;
      same_site_cross_origin_prerender_did_redirect_ = is_redirection;
    }
  }

  if (prerender_host_->IsBrowserInitiated() &&
      ShouldSkipHostInBlockList(navigation_url)) {
    CancelPrerendering(PrerenderFinalStatus::kEmbedderHostDisallowed);
    return CANCEL;
  }

  // Allow only HTTP(S) schemes.
  // https://wicg.github.io/nav-speculation/prerendering.html#no-bad-navs
  if (!navigation_url.SchemeIsHTTPOrHTTPS()) {
    CancelPrerendering(is_redirection
                           ? PrerenderFinalStatus::kInvalidSchemeRedirect
                           : PrerenderFinalStatus::kInvalidSchemeNavigation);
    return CANCEL;
  }

  // Origin checks for initial prerendering navigation (redirection).
  //
  // For non-embedder triggered prerendering, compare the origin of the
  // initiator URL to the origin of navigation (redirection) URL.
  //
  // For embedder triggered prerendering, there is no initiator page, so initial
  // prerendering navigation doesn't check origins and instead initial
  // prerendering redirection compare the origin of initial prerendering URL to
  // the origin of redirection URL.
  //
  // TODO(https://crbug.com/1422248): Merge this into the origin checks for
  // non-initial prerendering navigation above.
  if (IsInitialNavigation()) {
    if (prerender_host_->IsBrowserInitiated()) {
      // Cancel an embedder triggered prerendering if it is redirected to a URL
      // cross-site to the initial prerendering URL.
      if (prerender_navigation_utils::IsCrossSite(
              navigation_url, initial_prerendering_origin)) {
        CHECK(is_redirection);
        AnalyzeCrossOriginRedirection(
            navigation_origin, initial_prerendering_origin,
            prerender_host_->trigger_type(),
            prerender_host_->embedder_histogram_suffix());
        CancelPrerendering(
            PrerenderFinalStatus::kCrossSiteRedirectInInitialNavigation);
        return CANCEL;
      }

      // Skip the same-site check for non-redirected cases as the initiator
      // origin is nullopt for browser-initiated prerendering.
      DCHECK(!prerender_host_->initiator_origin().has_value());
    } else if (prerender_navigation_utils::IsCrossSite(
                   navigation_url,
                   prerender_host_->initiator_origin().value())) {
      // TODO(crbug.com/1176054): Once cross-site prerendering is implemented,
      // we'll need to enforce strict referrer policies
      // (https://wicg.github.io/nav-speculation/prefetch.html#list-of-sufficiently-strict-speculative-navigation-referrer-policies).
      //
      // Cancel prerendering if this is cross-site prerendering, cross-site
      // redirection during prerendering, or cross-site navigation from a
      // prerendered page.
      CancelPrerendering(
          is_redirection
              ? PrerenderFinalStatus::kCrossSiteRedirectInInitialNavigation
              : PrerenderFinalStatus::kCrossSiteNavigationInInitialNavigation);
      return CANCEL;
    } else if (prerender_navigation_utils::IsSameSiteCrossOrigin(
                   navigation_url,
                   prerender_host_->initiator_origin().value())) {
      // Same-site cross-origin prerendering is allowed only when the opt-in
      // header is specified on response. This will be checked on
      // WillProcessResponse().
      is_same_site_cross_origin_prerender_ = true;
      same_site_cross_origin_prerender_did_redirect_ = is_redirection;
    }
  }

  return PROCEED;
}

NavigationThrottle::ThrottleCheckResult
PrerenderNavigationThrottle::WillProcessResponse() {
  auto* navigation_request = NavigationRequest::From(navigation_handle());

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
  // Cancel prerendering when this is same-site cross-origin navigation but the
  // opt-in header is not specified.
  if (is_same_site_cross_origin_prerender_ && !is_credentialed_prerender) {
    // Calculate the final status for cancellation.
    PrerenderFinalStatus final_status = PrerenderFinalStatus::kDestroyed;
    if (IsInitialNavigation()) {
      final_status =
          same_site_cross_origin_prerender_did_redirect_
              ? PrerenderFinalStatus::
                    kSameSiteCrossOriginRedirectNotOptInInInitialNavigation
              : PrerenderFinalStatus::
                    kSameSiteCrossOriginNavigationNotOptInInInitialNavigation;
    } else {
      final_status =
          same_site_cross_origin_prerender_did_redirect_
              ? PrerenderFinalStatus::
                    kSameSiteCrossOriginRedirectNotOptInInMainFrameNavigation
              : PrerenderFinalStatus::
                    kSameSiteCrossOriginNavigationNotOptInInMainFrameNavigation;
    }
    CancelPrerendering(final_status);
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
    CancelPrerendering(cancel_reason.value());
    return CANCEL;
  }
  return PROCEED;
}

bool PrerenderNavigationThrottle::IsInitialNavigation() const {
  return prerender_host_->GetInitialNavigationId() ==
         navigation_handle()->GetNavigationId();
}

void PrerenderNavigationThrottle::CancelPrerendering(
    PrerenderFinalStatus final_status) {
  auto* navigation_request = NavigationRequest::From(navigation_handle());
  FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
  CHECK_EQ(frame_tree_node->GetFrameType(), FrameType::kPrerenderMainFrame);
  PrerenderHostRegistry* prerender_host_registry =
      frame_tree_node->current_frame_host()
          ->delegate()
          ->GetPrerenderHostRegistry();
  prerender_host_registry->CancelHost(prerender_host_->frame_tree_node_id(),
                                      final_status);
}

}  // namespace content
