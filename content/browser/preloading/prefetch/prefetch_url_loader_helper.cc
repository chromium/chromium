// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_url_loader_helper.h"

#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_origin_prober.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_probe_result.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_serving_page_metrics_container.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/browser/prefetch_metrics.h"
#include "content/public/browser/web_contents.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_partition_key_collection.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace content {
namespace {

PrefetchServingPageMetricsContainer*
PrefetchServingPageMetricsContainerFromFrameTreeNodeId(
    FrameTreeNodeId frame_tree_node_id) {
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (!frame_tree_node || !frame_tree_node->navigation_request()) {
    return nullptr;
  }

  return PrefetchServingPageMetricsContainer::GetForNavigationHandle(
      *frame_tree_node->navigation_request());
}

void RecordCookieWaitTime(base::TimeDelta wait_time) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", wait_time,
      base::TimeDelta(), base::Seconds(5), 50);
}

// Stores state for the asynchronous work required to prepare a prefetch to
// serve.
struct OnGotPrefetchToServeState {
  // Inputs.
  const FrameTreeNodeId frame_tree_node_id;
  const GURL tentative_url;
  base::OnceCallback<void(PrefetchContainer::Reader)> callback;
  PrefetchContainer::Reader reader;

  // True if we've validated that cookies match (to the extent required).
  // False if they don't. Absent if we don't know yet.
  // Unused if `features::kPrefetchCookieIndices` is disabled.
  std::optional<bool> cookies_matched;

  // The probe result, once it has been determined.
  // If it is empty, then this will be the next thing
  // ContinueOnGotPrefetchToServe does.
  std::optional<PrefetchProbeResult> probe_result;

  // True if copying isolated cookies is either done or has been determined
  // unnecessary.
  bool cookie_copy_complete_if_required = false;
};

// Forward declarations are required for these to call each other while
// appearing in the order they occur.
void ContinueOnGotPrefetchToServe(
    std::unique_ptr<OnGotPrefetchToServeState> state);
void StartCookieValidation(std::unique_ptr<OnGotPrefetchToServeState>& state);
void OnGotCookiesForValidation(
    std::unique_ptr<OnGotPrefetchToServeState> state,
    const std::vector<net::CookieWithAccessResult>& cookies,
    const std::vector<net::CookieWithAccessResult>& excluded_cookies);
void StartProbe(std::unique_ptr<OnGotPrefetchToServeState>& state);
void OnProbeComplete(std::unique_ptr<OnGotPrefetchToServeState> state,
                     base::TimeTicks probe_start_time,
                     PrefetchProbeResult probe_result);
void EnsureCookiesCopied(std::unique_ptr<OnGotPrefetchToServeState>& state);
void OnCookieCopyComplete(std::unique_ptr<OnGotPrefetchToServeState> state,
                          base::TimeTicks cookie_copy_start_time);

// Overall structure of asynchronous execution (in coroutine style).

void ContinueOnGotPrefetchToServe(
    std::unique_ptr<OnGotPrefetchToServeState> state) {
  // If the cookies need to be matched, fetch them and confirm that they're
  // correct.
  if (base::FeatureList::IsEnabled(features::kPrefetchCookieIndices)) {
    if (!state->cookies_matched.has_value()) {
      StartCookieValidation(state);
      if (!state) {
        // Fetching the cookies asynchronously. Continue later.
        return;
      }
    }
    CHECK(state->cookies_matched.has_value());
    if (!state->cookies_matched.value()) {
      // Cookies did not match, but needed to. We're done here.
      std::move(state->callback).Run({});
      return;
    }
  }

  // If probing hasn't happened yet, do it if necessary.
  if (!state->probe_result.has_value()) {
    StartProbe(state);
    if (!state) {
      // The probe is happening asynchronously (it took ownership of |state|),
      // and this algorithm will continue later.
      return;
    }
    if (!state->probe_result.has_value()) {
      // Could not start a probe. We're done here.
      std::move(state->callback).Run({});
      return;
    }
  }
  if (!PrefetchProbeResultIsSuccess(state->probe_result.value())) {
    // Probe failed. We're done here.
    std::move(state->callback).Run({});
    return;
  }

  // Copy isolated cookies, if required.
  if (!state->cookie_copy_complete_if_required) {
    EnsureCookiesCopied(state);
    if (!state) {
      // Cookie copy is happening and this function will continue later.
      return;
    }
  }

  // All prerequisites should now be complete.
  CHECK(!base::FeatureList::IsEnabled(features::kPrefetchCookieIndices) ||
        state->cookies_matched.value_or(false));
  CHECK(PrefetchProbeResultIsSuccess(state->probe_result.value()));
  CHECK(state->cookie_copy_complete_if_required);

  if (!state->reader) {
    std::move(state->callback).Run({});
    return;
  }

  switch (state->reader.GetServableState(PrefetchCacheableDuration())) {
    case PrefetchContainer::ServableState::kNotServable:
    case PrefetchContainer::ServableState::kShouldBlockUntilHeadReceived:
      std::move(state->callback).Run({});
      return;
    case PrefetchContainer::ServableState::kServable:
      break;
  }

  // Delay updating the prefetch with the probe result in case it becomes not
  // servable.
  state->reader.OnPrefetchProbeResult(state->probe_result.value());

  PrefetchServingPageMetricsContainer* serving_page_metrics_container =
      PrefetchServingPageMetricsContainerFromFrameTreeNodeId(
          state->frame_tree_node_id);
  if (serving_page_metrics_container) {
    serving_page_metrics_container->SetPrefetchStatus(
        state->reader.GetPrefetchStatus());
  }

  std::move(state->callback).Run(std::move(state->reader));
}

// COOKIE VALIDATION

void StartCookieValidation(std::unique_ptr<OnGotPrefetchToServeState>& state) {
  WebContents* web_contents =
      WebContents::FromFrameTreeNodeId(state->frame_tree_node_id);
  if (!web_contents || !state->reader) {
    // We can't confirm that the cookies matched. But probably everything is
    // being torn down, anyway.
    state->cookies_matched = false;
    return;
  }
  if (!state->reader.VariesOnCookieIndices()) {
    state->cookies_matched = true;
    return;
  }
  network::mojom::CookieManager* cookie_manager =
      web_contents->GetBrowserContext()
          ->GetDefaultStoragePartition()
          ->GetCookieManagerForBrowserProcess();
  // Note: This currently relies on this being for main frame use only.
  // The partitioning below needs to be adjusted if a subframe use were
  // possible.
  CHECK(FrameTreeNode::GloballyFindByID(state->frame_tree_node_id)
            ->IsMainFrame());
  const GURL& url = state->reader.GetCurrentURLToServe();
  net::SchemefulSite site(url);
  cookie_manager->GetCookieList(
      url, net::CookieOptions::MakeAllInclusive(),
      net::CookiePartitionKeyCollection::FromOptional(
          net::CookiePartitionKey::FromNetworkIsolationKey(
              net::NetworkIsolationKey(site, site), net::SiteForCookies(site),
              site, /*main_frame_navigation=*/true)),
      base::BindOnce(&OnGotCookiesForValidation, std::move(state)));
}

void OnGotCookiesForValidation(
    std::unique_ptr<OnGotPrefetchToServeState> state,
    const std::vector<net::CookieWithAccessResult>& cookies,
    const std::vector<net::CookieWithAccessResult>& excluded_cookies) {
  std::vector<std::pair<std::string, std::string>> cookie_values;
  cookie_values.reserve(cookies.size());
  for (const net::CookieWithAccessResult& cookie : cookies) {
    cookie_values.emplace_back(cookie.cookie.Name(), cookie.cookie.Value());
  }

  state->cookies_matched =
      state->reader && state->reader.MatchesCookieIndices(cookie_values);
  ContinueOnGotPrefetchToServe(std::move(state));
}

// ORIGIN PROBING

void StartProbe(std::unique_ptr<OnGotPrefetchToServeState>& state) {
  // TODO(crbug.com/40274818): Should we check for existence of an
  // `origin_prober` earlier instead of waiting until we have a matching
  // prefetch?
  PrefetchService* prefetch_service =
      PrefetchService::GetFromFrameTreeNodeId(state->frame_tree_node_id);
  if (!prefetch_service || !prefetch_service->GetPrefetchOriginProber()) {
    return;
  }
  PrefetchOriginProber* prober = prefetch_service->GetPrefetchOriginProber();
  if (!state->reader.IsIsolatedNetworkContextRequiredToServe() ||
      !prober->ShouldProbeOrigins()) {
    state->probe_result = PrefetchProbeResult::kNoProbing;
    return;
  }
  GURL probe_url = url::SchemeHostPort(state->tentative_url).GetURL();
  prober->Probe(probe_url,
                base::BindOnce(&OnProbeComplete, std::move(state),
                               /*probe_start_time=*/base::TimeTicks::Now()));
}

// Called when the `PrefetchOriginProber` check is done (if performed).
// `probe_start_time` is used to calculate probe latency which is
// reported to the tab helper.
void OnProbeComplete(std::unique_ptr<OnGotPrefetchToServeState> state,
                     base::TimeTicks probe_start_time,
                     PrefetchProbeResult probe_result) {
  state->probe_result = probe_result;

  PrefetchServingPageMetricsContainer* serving_page_metrics_container =
      PrefetchServingPageMetricsContainerFromFrameTreeNodeId(
          state->frame_tree_node_id);
  if (serving_page_metrics_container) {
    serving_page_metrics_container->SetProbeLatency(base::TimeTicks::Now() -
                                                    probe_start_time);
  }

  if (!PrefetchProbeResultIsSuccess(probe_result) && state->reader) {
    state->reader.OnPrefetchProbeResult(probe_result);
    if (serving_page_metrics_container) {
      serving_page_metrics_container->SetPrefetchStatus(
          state->reader.GetPrefetchStatus());
    }
  }

  ContinueOnGotPrefetchToServe(std::move(state));
}

// ISOLATED COOKIE COPYING

// Ensures that the cookies for prefetch are copied from its isolated
// network context to the default network context.
void EnsureCookiesCopied(std::unique_ptr<OnGotPrefetchToServeState>& state) {
  PrefetchContainer::Reader& reader = state->reader;

  // Start the cookie copy for the next redirect hop of |state->reader|.
  if (reader && !reader.HasIsolatedCookieCopyStarted()) {
    PrefetchService* prefetch_service =
        PrefetchService::GetFromFrameTreeNodeId(state->frame_tree_node_id);
    if (prefetch_service) {
      prefetch_service->CopyIsolatedCookies(reader);
    }
  }

  if (reader) {
    reader.OnInterceptorCheckCookieCopy();
  }

  if (!reader || !reader.IsIsolatedCookieCopyInProgress()) {
    RecordCookieWaitTime(base::TimeDelta());
    state->cookie_copy_complete_if_required = true;
    return;
  }

  reader.SetOnCookieCopyCompleteCallback(
      base::BindOnce(&OnCookieCopyComplete, std::move(state),
                     /*cookie_copy_start_time=*/base::TimeTicks::Now()));
}

void OnCookieCopyComplete(std::unique_ptr<OnGotPrefetchToServeState> state,
                          base::TimeTicks cookie_copy_start_time) {
  base::TimeDelta wait_time = base::TimeTicks::Now() - cookie_copy_start_time;
  CHECK_GE(wait_time, base::TimeDelta());
  RecordCookieWaitTime(wait_time);
  state->cookie_copy_complete_if_required = true;
  ContinueOnGotPrefetchToServe(std::move(state));
}

}  // namespace

void OnGotPrefetchToServe(
    FrameTreeNodeId frame_tree_node_id,
    const network::ResourceRequest& tentative_resource_request,
    base::OnceCallback<void(PrefetchContainer::Reader)> get_prefetch_callback,
    PrefetchContainer::Reader reader) {
  // TODO(crbug.com/40274818): With multiple prefetches matching, we should
  // move some of the checks here in `PrefetchService::ReturnPrefetchToServe`.
  // Why ? Because we might be able to serve a different prefetch if the
  // prefetch in the `reader` cannot be served.

  // The |tentative_resource_request.url| might be different from
  // |GetCurrentURLToServe()| because of No-Vary-Search non-exact url
  // match.
#if DCHECK_IS_ON()
  if (reader) {
    GURL::Replacements replacements;
    replacements.ClearRef();
    replacements.ClearQuery();
    DCHECK_EQ(tentative_resource_request.url.ReplaceComponents(replacements),
              reader.GetCurrentURLToServe().ReplaceComponents(replacements));
  }
#endif

  if (!reader) {
    std::move(get_prefetch_callback).Run({});
    return;
  }

  switch (reader.GetServableState(PrefetchCacheableDuration())) {
    case PrefetchContainer::ServableState::kNotServable:
    case PrefetchContainer::ServableState::kShouldBlockUntilHeadReceived:
      std::move(get_prefetch_callback).Run({});
      return;
    case PrefetchContainer::ServableState::kServable:
      break;
  }

  // We should not reach here if the cookies have changed. This should already
  // have been checked in one of the call sites:
  // 1) PrefetchService::ReturnPrefetchToServe (in which case |reader| should be
  //    empty)
  // 2) PrefetchURLLoaderInterceptor::MaybeCreateLoader (before serving the next
  //    next redirect hop)
  CHECK(!reader.HaveDefaultContextCookiesChanged());

  // Asynchronous activity begins here.
  // We allocate an explicit "coroutine state" for this and manage it manually.
  // While slightly verbose, this avoids duplication of logic later on in
  // control flow. This function will asynchronously call itself until it's
  // done.
  ContinueOnGotPrefetchToServe(base::WrapUnique(new OnGotPrefetchToServeState{
      .frame_tree_node_id = frame_tree_node_id,
      .tentative_url = tentative_resource_request.url,
      .callback = std::move(get_prefetch_callback),
      .reader = std::move(reader)}));
}

}  // namespace content
