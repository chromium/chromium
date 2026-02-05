// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "content/browser/preloading/prefetch/prefetch_origin_prober.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_probe_result.h"
#include "content/browser/preloading/prefetch/prefetch_servable_state.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_serving_handle.h"
#include "content/browser/preloading/prefetch/prefetch_serving_page_metrics_container.h"
#include "content/browser/preloading/prefetch/prefetch_single_redirect_hop.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_partition_key_collection.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

// TODO(https://crbug.com/437416134): Move this file's contents to
// `prefetch_serving_handle.cc`.

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

// Helper for `base::BindOnce()` + rvalue ref-qualified member method.
// TODO(crbug.com/40254119): Remove this helper when the issue is fixed.
template <typename... UnboundArgs,
          typename Method,
          typename Receiver,
          typename... BoundArgs>
auto BindOnceForRvalueMemberMethod(Method method,
                                   Receiver receiver,
                                   BoundArgs&&... bound_args) {
  return base::BindOnce(
      [](Method method, std::decay_t<Receiver> receiver,
         std::decay_t<BoundArgs>... bound_args, UnboundArgs... unbound_args) {
        (std::move(receiver).*method)(
            std::forward<BoundArgs>(bound_args)...,
            std::forward<UnboundArgs>(unbound_args)...);
      },
      method, std::move(receiver), std::forward<BoundArgs>(bound_args)...);
}

}  // namespace

// Stores state for the asynchronous work required to prepare a prefetch to
// serve.
struct PrefetchServingHandle::OnGotPrefetchToServeState final {
  // Inputs.
  const FrameTreeNodeId frame_tree_node_id;
  const GURL tentative_url;
  base::OnceCallback<void(PrefetchServingHandle)> callback;

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

// Overall structure of asynchronous execution (in coroutine style).

void PrefetchServingHandle::ContinueOnGotPrefetchToServe(
    std::unique_ptr<OnGotPrefetchToServeState> state) && {
  // If the cookies need to be matched, fetch them and confirm that they're
  // correct.
  if (base::FeatureList::IsEnabled(features::kPrefetchCookieIndices)) {
    if (!state->cookies_matched.has_value()) {
      WebContents* web_contents =
          WebContents::FromFrameTreeNodeId(state->frame_tree_node_id);
      if (!web_contents || !IsValid()) {
        // We can't confirm that the cookies matched. But probably everything is
        // being torn down, anyway.
        state->cookies_matched = false;
      } else if (!VariesOnCookieIndices()) {
        state->cookies_matched = true;
      } else {
        std::move(*this).StartCookieValidation(std::move(state));
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
    // TODO(crbug.com/40274818): Should we check for existence of an
    // `origin_prober` earlier instead of waiting until we have a matching
    // prefetch?
    PrefetchService* prefetch_service =
        PrefetchService::GetFromFrameTreeNodeId(state->frame_tree_node_id);
    if (!prefetch_service || !prefetch_service->GetPrefetchOriginProber()) {
      // Could not start a probe. We're done here.
      std::move(state->callback).Run({});
      return;
    }

    PrefetchOriginProber* prober = prefetch_service->GetPrefetchOriginProber();
    if (IsIsolatedNetworkContextRequiredToServe() &&
        prober->ShouldProbeOrigins()) {
      GURL probe_url = url::SchemeHostPort(state->tentative_url).GetURL();
      prober->Probe(probe_url,
                    BindOnceForRvalueMemberMethod<PrefetchProbeResult>(
                        &PrefetchServingHandle::OnProbeComplete,
                        std::move(*this), std::move(state),
                        /*probe_start_time=*/base::TimeTicks::Now()));
      // The probe is happening asynchronously (it took ownership of |state|),
      // and this algorithm will continue later.
      return;
    }

    state->probe_result = PrefetchProbeResult::kNoProbing;
  }

  if (!PrefetchProbeResultIsSuccess(state->probe_result.value())) {
    // Probe failed. We're done here.
    std::move(state->callback).Run({});
    return;
  }

  // Copy isolated cookies, if required.
  // Ensures that the cookies for prefetch are copied from its isolated
  // network context to the default network context.
  if (!state->cookie_copy_complete_if_required) {
    if (IsValid()) {
      if (!HasIsolatedCookieCopyStarted()) {
        // Start the cookie copy for the next redirect hop.
        if (PrefetchService* prefetch_service =
                PrefetchService::GetFromFrameTreeNodeId(
                    state->frame_tree_node_id)) {
          prefetch_service->CopyIsolatedCookies(*this);
        }
      }

      OnInterceptorCheckCookieCopy();

      if (IsIsolatedCookieCopyInProgress()) {
        // Cookie copy is happening and this function will continue later.

        // We first get a `current_redirect_hop` reference and then move out
        // `*this` etc., to avoid use-after-move.
        // TODO(https://crbug.com/437416134): Revamp this for better interfacing
        // and fix potential bugs.
        auto& current_redirect_hop = GetCurrentSingleRedirectHopToServe();
        DUMP_WILL_BE_CHECK(
            !current_redirect_hop.on_cookie_copy_complete_callback_);
        current_redirect_hop.on_cookie_copy_complete_callback_ =
            BindOnceForRvalueMemberMethod<>(
                &PrefetchServingHandle::OnCookieCopyComplete, std::move(*this),
                std::move(state),
                /*cookie_copy_start_time=*/base::TimeTicks::Now());
        return;
      }
    }

    RecordCookieWaitTime(base::TimeDelta());
    state->cookie_copy_complete_if_required = true;
  }

  // All prerequisites should now be complete.
  CHECK(!base::FeatureList::IsEnabled(features::kPrefetchCookieIndices) ||
        state->cookies_matched.value_or(false));
  CHECK(PrefetchProbeResultIsSuccess(state->probe_result.value()));
  CHECK(state->cookie_copy_complete_if_required);

  if (!IsValid()) {
    std::move(state->callback).Run({});
    return;
  }

  switch (GetServableState()) {
    case PrefetchServableState::kNotServable:
    case PrefetchServableState::kShouldBlockUntilEligibilityGot:
    case PrefetchServableState::kShouldBlockUntilHeadReceived:
      std::move(state->callback).Run({});
      return;
    case PrefetchServableState::kServable:
      break;
  }

  // Delay updating the prefetch with the probe result in case it becomes not
  // servable.
  OnPrefetchProbeResult(state->probe_result.value());

  PrefetchServingPageMetricsContainer* serving_page_metrics_container =
      PrefetchServingPageMetricsContainerFromFrameTreeNodeId(
          state->frame_tree_node_id);
  if (serving_page_metrics_container) {
    serving_page_metrics_container->SetPrefetchStatus(GetPrefetchStatus());
  }

  std::move(state->callback).Run(std::move(*this));
}

// COOKIE VALIDATION

void PrefetchServingHandle::StartCookieValidation(
    std::unique_ptr<OnGotPrefetchToServeState> state) && {
  WebContents* web_contents =
      WebContents::FromFrameTreeNodeId(state->frame_tree_node_id);
  network::mojom::CookieManager* cookie_manager =
      web_contents->GetBrowserContext()
          ->GetDefaultStoragePartition()
          ->GetCookieManagerForBrowserProcess();
  // Note: This currently relies on this being for main frame use only.
  // The partitioning below needs to be adjusted if a subframe use were
  // possible.
  CHECK(FrameTreeNode::GloballyFindByID(state->frame_tree_node_id)
            ->IsMainFrame());
  const GURL& url = GetCurrentURLToServe();
  net::SchemefulSite site(url);
  cookie_manager->GetCookieList(
      url, net::CookieOptions::MakeAllInclusive(),
      net::CookiePartitionKeyCollection(
          net::CookiePartitionKey::FromNetworkIsolationKey(
              net::NetworkIsolationKey(site, site), net::SiteForCookies(site),
              site, /*main_frame_navigation=*/true)),
      BindOnceForRvalueMemberMethod<
          const std::vector<net::CookieWithAccessResult>&,
          const std::vector<net::CookieWithAccessResult>&>(
          &PrefetchServingHandle::OnGotCookiesForValidation, std::move(*this),
          std::move(state)));
}

void PrefetchServingHandle::OnGotCookiesForValidation(
    std::unique_ptr<OnGotPrefetchToServeState> state,
    const std::vector<net::CookieWithAccessResult>& cookies,
    const std::vector<net::CookieWithAccessResult>& excluded_cookies) && {
  std::vector<std::pair<std::string, std::string>> cookie_values;
  cookie_values.reserve(cookies.size());
  for (const net::CookieWithAccessResult& cookie : cookies) {
    cookie_values.emplace_back(cookie.cookie.Name(), cookie.cookie.Value());
  }

  state->cookies_matched = IsValid() && MatchesCookieIndices(cookie_values);
  std::move(*this).ContinueOnGotPrefetchToServe(std::move(state));
}

// ORIGIN PROBING

// Called when the `PrefetchOriginProber` check is done (if performed).
// `probe_start_time` is used to calculate probe latency which is
// reported to the tab helper.
void PrefetchServingHandle::OnProbeComplete(
    std::unique_ptr<OnGotPrefetchToServeState> state,
    base::TimeTicks probe_start_time,
    PrefetchProbeResult probe_result) && {
  state->probe_result = probe_result;

  PrefetchServingPageMetricsContainer* serving_page_metrics_container =
      PrefetchServingPageMetricsContainerFromFrameTreeNodeId(
          state->frame_tree_node_id);
  if (serving_page_metrics_container) {
    serving_page_metrics_container->SetProbeLatency(base::TimeTicks::Now() -
                                                    probe_start_time);
  }

  if (!PrefetchProbeResultIsSuccess(probe_result) && IsValid()) {
    OnPrefetchProbeResult(probe_result);
    if (serving_page_metrics_container) {
      serving_page_metrics_container->SetPrefetchStatus(GetPrefetchStatus());
    }
  }

  std::move(*this).ContinueOnGotPrefetchToServe(std::move(state));
}

// ISOLATED COOKIE COPYING

void PrefetchServingHandle::OnCookieCopyComplete(
    std::unique_ptr<OnGotPrefetchToServeState> state,
    base::TimeTicks cookie_copy_start_time) && {
  base::TimeDelta wait_time = base::TimeTicks::Now() - cookie_copy_start_time;
  CHECK_GE(wait_time, base::TimeDelta());
  RecordCookieWaitTime(wait_time);
  state->cookie_copy_complete_if_required = true;
  std::move(*this).ContinueOnGotPrefetchToServe(std::move(state));
}

void PrefetchServingHandle::OnGotPrefetchToServe(
    FrameTreeNodeId frame_tree_node_id,
    const GURL& tentative_resource_request_url,
    base::OnceCallback<void(PrefetchServingHandle)> get_prefetch_callback) && {
  // TODO(crbug.com/40274818): With multiple prefetches matching, we should
  // move some of the checks here in `PrefetchService::ReturnPrefetchToServe`.
  // Why ? Because we might be able to serve a different prefetch if the
  // prefetch in the `*this` cannot be served.

  // The `tentative_resource_request_url` might be different from
  // `GetCurrentURLToServe()` because of No-Vary-Search non-exact url match.
#if DCHECK_IS_ON()
  if (IsValid()) {
    GURL::Replacements replacements;
    replacements.ClearRef();
    replacements.ClearQuery();
    DCHECK_EQ(tentative_resource_request_url.ReplaceComponents(replacements),
              GetCurrentURLToServe().ReplaceComponents(replacements));
  }
#endif

  if (!IsValid()) {
    std::move(get_prefetch_callback).Run({});
    return;
  }

  switch (GetServableState()) {
    case PrefetchServableState::kNotServable:
    case PrefetchServableState::kShouldBlockUntilEligibilityGot:
    case PrefetchServableState::kShouldBlockUntilHeadReceived:
      std::move(get_prefetch_callback).Run({});
      return;
    case PrefetchServableState::kServable:
      break;
  }

  // We should not reach here if the cookies have changed. This should already
  // have been checked in one of the call sites:
  // 1) PrefetchService::ReturnPrefetchToServe (in which case |this| should be
  //    empty)
  // 2) PrefetchURLLoaderInterceptor::MaybeCreateLoader (before serving the next
  //    next redirect hop)
  CHECK(!HaveDefaultContextCookiesChanged());

  // Asynchronous activity begins here.
  // We allocate an explicit "coroutine state" for this and manage it manually.
  // While slightly verbose, this avoids duplication of logic later on in
  // control flow. This function will asynchronously call itself until it's
  // done.
  std::move(*this).ContinueOnGotPrefetchToServe(
      base::WrapUnique(new OnGotPrefetchToServeState{
          .frame_tree_node_id = frame_tree_node_id,
          .tentative_url = tentative_resource_request_url,
          .callback = std::move(get_prefetch_callback)}));
}

}  // namespace content
