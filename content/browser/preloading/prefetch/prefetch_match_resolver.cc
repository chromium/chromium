// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_match_resolver.h"

#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/browser/preloading/prefetch/prefetch_scheduler.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_serving_handle.h"
#include "content/browser/preloading/preload_serving_metrics_holder.h"
#include "content/browser/preloading/prerender/prerender_features.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/renderer_host/frame_tree.h"

namespace content {

namespace {

// Returns keys of `map` for safe iteration.
//
// `K` must be copyable.
template <class K, class V>
std::vector<K> Keys(const std::map<K, V>& map) {
  std::vector<K> keys;

  for (auto& item : map) {
    keys.push_back(item.first);
  }

  return keys;
}

}  // namespace

PrefetchMatchResolver::CandidateData::CandidateData() = default;
PrefetchMatchResolver::CandidateData::~CandidateData() = default;

PrefetchMatchResolver::PrefetchMatchResolver(
    base::WeakPtr<NavigationRequest> navigation_request,
    base::WeakPtr<PrefetchService> prefetch_service,
    PrefetchKey navigated_key,
    PrefetchServiceWorkerState expected_service_worker_state,
    bool is_nav_prerender,
    base::WeakPtr<PrerenderHost> prerender_host,
    Callback callback,
    perfetto::Flow flow)
    : navigation_request_for_metrics_(std::move(navigation_request)),
      prefetch_service_(std::move(prefetch_service)),
      navigated_key_(std::move(navigated_key)),
      expected_service_worker_state_(expected_service_worker_state),
      callback_(std::move(callback)),
      flow_(std::move(flow)),
      is_nav_prerender_(is_nav_prerender),
      prerender_host_for_metrics_(std::move(prerender_host)),
      prefetch_match_metrics_(std::make_unique<PrefetchMatchMetrics>()) {
  switch (expected_service_worker_state_) {
    case PrefetchServiceWorkerState::kAllowed:
      NOTREACHED();
    case PrefetchServiceWorkerState::kControlled:
      CHECK(base::FeatureList::IsEnabled(features::kPrefetchServiceWorker));
      break;
    case PrefetchServiceWorkerState::kDisallowed:
      break;
  }

  prefetch_match_metrics_->expected_service_worker_state =
      expected_service_worker_state;
  prefetch_match_metrics_->time_match_start = base::TimeTicks::Now();
}

PrefetchMatchResolver::~PrefetchMatchResolver() = default;

std::optional<base::TimeDelta> PrefetchMatchResolver::GetBlockedDuration()
    const {
  if (wait_started_at_.has_value()) {
    return base::TimeTicks::Now() - wait_started_at_.value();
  } else {
    return std::nullopt;
  }
}

// static
void PrefetchMatchResolver::FindPrefetch(
    FrameTreeNodeId frame_tree_node_id,
    PrefetchService& prefetch_service,
    PrefetchKey navigated_key,
    PrefetchServiceWorkerState expected_service_worker_state,
    base::WeakPtr<PrefetchServingPageMetricsContainer>
        serving_page_metrics_container,
    Callback callback,
    perfetto::Flow flow) {
  TRACE_EVENT_BEGIN("loading", "PrefetchMatchResolver::FindPrefetch", flow);

  auto* frame_tree_node = FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (!frame_tree_node) {
    // TODO(crbug.com/360094997): Use `CHECK()` instead once we check the
    // safety.
    DUMP_WILL_BE_NOTREACHED();
    std::move(callback).Run({});
    return;
  }

  auto navigation_request = ([&]() -> base::WeakPtr<NavigationRequest> {
    if (!frame_tree_node->navigation_request()) {
      // We except that a navigation invoking `URLLoaderInterceptor` has
      // `NavigationRequest`, but this path hits in android-x64-rel bots. If it
      // is not satisfied in the prod, give up to record metrics.
      //
      // TODO(crbug.com/360094997): Investigate why.
      return nullptr;
    }

    return frame_tree_node->navigation_request()->GetWeakPtr();
  })();

  auto prerender_host = ([&]() -> base::WeakPtr<PrerenderHost> {
    if (!PreloadServingMetricsCapsule::IsFeatureEnabled()) {
      return nullptr;
    }

    PrerenderHostRegistry* prerender_host_registry =
        frame_tree_node->current_frame_host()
            ->delegate()
            ->GetPrerenderHostRegistry();
    if (!prerender_host_registry) {
      return nullptr;
    }

    PrerenderHost* prerender_host =
        prerender_host_registry->FindNonReservedHostById(frame_tree_node_id);
    if (!prerender_host) {
      return nullptr;
    }

    return prerender_host->GetWeakPtr();
  })();

  TRACE_EVENT_END("loading");

  PrefetchMatchResolver::FindPrefetchInternal1(
      std::move(navigation_request), prefetch_service, std::move(navigated_key),
      expected_service_worker_state,
      frame_tree_node->frame_tree().is_prerendering(),
      std::move(prerender_host), std::move(serving_page_metrics_container),
      std::move(callback), std::move(flow));
}

// static
void PrefetchMatchResolver::FindPrefetchForTesting(
    PrefetchService& prefetch_service,
    PrefetchKey navigated_key,
    PrefetchServiceWorkerState expected_service_worker_state,
    bool is_nav_prerender,
    base::WeakPtr<PrefetchServingPageMetricsContainer>
        serving_page_metrics_container,
    Callback callback) {
  PrefetchMatchResolver::FindPrefetchInternal1(
      /*navigation_request=*/nullptr, prefetch_service,
      std::move(navigated_key), expected_service_worker_state, is_nav_prerender,
      /*prerender_host=*/nullptr, std::move(serving_page_metrics_container),
      std::move(callback), perfetto::Flow::ProcessScoped(0));
}

// static
void PrefetchMatchResolver::FindPrefetchInternal1(
    base::WeakPtr<NavigationRequest> navigation_request,
    PrefetchService& prefetch_service,
    PrefetchKey navigated_key,
    PrefetchServiceWorkerState expected_service_worker_state,
    bool is_nav_prerender,
    base::WeakPtr<PrerenderHost> prerender_host,
    base::WeakPtr<PrefetchServingPageMetricsContainer>
        serving_page_metrics_container,
    Callback callback,
    perfetto::Flow flow) {
  // See the comment of `self_`.
  auto prefetch_match_resolver = base::WrapUnique(new PrefetchMatchResolver(
      std::move(navigation_request), prefetch_service.GetWeakPtr(),
      std::move(navigated_key), expected_service_worker_state, is_nav_prerender,
      std::move(prerender_host), std::move(callback), std::move(flow)));
  PrefetchMatchResolver& ref = *prefetch_match_resolver.get();
  ref.self_ = std::move(prefetch_match_resolver);

  ref.FindPrefetchInternal2(prefetch_service,
                            std::move(serving_page_metrics_container));
}

void PrefetchMatchResolver::FindPrefetchInternal2(
    PrefetchService& prefetch_service,
    base::WeakPtr<PrefetchServingPageMetricsContainer>
        serving_page_metrics_container) {
  TRACE_EVENT_BEGIN("loading", "PrefetchMatchResolver::FindPrefetch", flow_,
                    perfetto::Flow::FromPointer(this));

  auto [candidates, servable_states] = prefetch_service.CollectMatchCandidates(
      navigated_key_, is_nav_prerender_,
      std::move(serving_page_metrics_container));
  // Consume `candidates`.
  for (auto& prefetch_container : candidates) {
    // Register the candidate only if `PrefetchServiceWorkerState` is matching.
    if (prefetch_container->service_worker_state() ==
        expected_service_worker_state_) {
      RegisterCandidate(*prefetch_container);
    } else if (prefetch_container->service_worker_state() ==
               PrefetchServiceWorkerState::kAllowed) {
      // Also register the candidate if `PrefetchServiceWorkerState::kAllowed`,
      // so that we anyway start BlockUntilHead (if eligible) before service
      // worker controller check is done, and remove the candidate later (in
      // `OnDeterminedHead()`) if the final `PrefetchServiceWorkerState` turns
      // not matching.
      CHECK(base::FeatureList::IsEnabled(features::kPrefetchServiceWorker));
      RegisterCandidate(*prefetch_container);
    }
  }
  prefetch_match_metrics_->n_initial_candidates = candidates_.size();
  // `PrefetchMatchMetrics::n_initial_candidates_block_until_head` is `0` when
  // we early-exit before reaching `StartWaitFor()` calls below, as we anyway
  // don't wait for anything.
  prefetch_match_metrics_->n_initial_candidates_block_until_head = 0;

  // Backward compatibility to the behavior of `PrefetchMatchResolver`: If it
  // finds a `PrefetchContainer` in which the cookies of the head of redirect
  // chain changed, propagate it to other potentially matching
  // `PrefetchContainer`s and give up to serve.
  //
  // TODO(kenoss): kenoss@ believes that we can improve it by propagating
  // cookies changed event with `PrefetchContainer` and `PrefetchService`
  // (without `PrefetchMatchResoler`/`PrefetchMatchResoler2`) and marking them
  // `kNotServable`.
  for (auto& candidate : candidates_) {
    switch (servable_states.at(candidate.first)) {
      case PrefetchServableState::kServable:
        if (candidate.second->prefetch_container->CreateServingHandle()
                .HaveDefaultContextCookiesChanged()) {
          UnblockForCookiesChanged(candidate.second->prefetch_container->key());
          return;
        }
        break;
      case PrefetchServableState::kNotServable:
        NOTREACHED();
      case PrefetchServableState::kShouldBlockUntilHeadReceived:
      case PrefetchServableState::kShouldBlockUntilEligibilityGot:
        // nop
        break;
    }
  }

  for (auto& candidate : candidates_) {
    switch (servable_states.at(candidate.first)) {
      case PrefetchServableState::kServable:
        TRACE_EVENT_END("loading");

        // Got matching and servable.
        UnblockForMatch(candidate.first);
        return;
      case PrefetchServableState::kNotServable:
        NOTREACHED();
      case PrefetchServableState::kShouldBlockUntilHeadReceived:
      case PrefetchServableState::kShouldBlockUntilEligibilityGot:
        // nop
        break;
    }
  }

  // There is no matching and servable prefetch at this point. We should wait
  // remaining ones.

  CHECK(!wait_started_at_.has_value());
  wait_started_at_ = base::TimeTicks::Now();
  prefetch_match_metrics_->n_initial_candidates_block_until_head =
      candidates_.size();

  for (auto& candidate : candidates_) {
    StartWaitFor(candidate.first, servable_states.at(candidate.first));
  }

  TRACE_EVENT_END("loading");

  if (candidates_.size() == 0) {
    UnblockForNoCandidates();
  }
}

void PrefetchMatchResolver::RegisterCandidate(
    PrefetchContainer& prefetch_container) {
  auto candidate_data = std::make_unique<CandidateData>();
  TRACE_EVENT("loading", "PrefetchMatchResolver::RegisterCandidate",
              perfetto::Flow::FromPointer(candidate_data.get()));

  // #prefetch-key-availability
  //
  // Note that `CHECK(candidates_.contains(prefetch_key))` and
  // `CHECK(candidate_data->prefetch_container)` below always hold because
  // `PrefetchMatchResolver` observes lifecycle events of `PrefetchContainer`.
  candidate_data->prefetch_container = prefetch_container.GetWeakPtr();
  candidate_data->timeout_timer = nullptr;

  candidates_[prefetch_container.key()] = std::move(candidate_data);

  // If the navigation is prerender initial navigation and a prefetch ahead of
  // prerender is a candidate, capture it for `PreloadServingMetrics`.
  if (PreloadServingMetricsCapsule::IsFeatureEnabled()) {
    if (prerender_host_for_metrics_ &&
        prefetch_container.HasPreloadPipelineInfoForMetrics(
            prerender_host_for_metrics_->preload_pipeline_info())) {
      // We expect at most one potentially matching prefetch-ahead-of-prerender
      // to exist for a given prerender. The pipeline normally won't trigger a
      // new prefetch if one already exists. Furthermore, even if another is
      // triggered, it should be deduplicated against the existing one as they
      // would share the same PrefetchKey. Therefore, this check should always
      // be satisfied.
      CHECK(!prefetch_ahead_of_prerender_for_metrics_);

      prefetch_ahead_of_prerender_for_metrics_ =
          prefetch_container.GetWeakPtr();
    }
  }
}

void PrefetchMatchResolver::StartWaitFor(const PrefetchKey& prefetch_key,
                                         PrefetchServableState servable_state) {
  TRACE_EVENT("loading", "PrefetchMatchResolver::StartWaitFor",
              perfetto::Flow::FromPointer(this));

  // By #prefetch-key-availability
  auto it = candidates_.find(prefetch_key);
  CHECK(it != candidates_.end());
  CandidateData* candidate_data = it->second.get();
  CHECK(candidate_data->prefetch_container);
  PrefetchContainer& prefetch_container = *candidate_data->prefetch_container;

  // `kServable` -> `kNotServable` is the only possible change during
  // `FindPrefetchInternal()` call.
  CHECK_EQ(prefetch_container.GetServableState(PrefetchCacheableDuration()),
           servable_state);
  switch (servable_state) {
    case PrefetchServableState::kServable:
    case PrefetchServableState::kNotServable:
      NOTREACHED();
    case PrefetchServableState::kShouldBlockUntilHeadReceived:
    case PrefetchServableState::kShouldBlockUntilEligibilityGot:
      // nop
      break;
  }
  CHECK(!candidate_data->timeout_timer);

  // TODO(crbug.com/356552413): Merge
  // https://chromium-review.googlesource.com/c/chromium/src/+/5668924 and
  // write tests.
  base::TimeDelta timeout = PrefetchBlockUntilHeadTimeout(
      prefetch_container.request().prefetch_type(),
      prefetch_container.request().should_disable_block_until_head_timeout(),
      is_nav_prerender_);
  if (timeout.is_positive()) {
    candidate_data->timeout_timer = std::make_unique<base::OneShotTimer>();
    candidate_data->timeout_timer->Start(
        FROM_HERE, timeout,
        base::BindOnce(&PrefetchMatchResolver::OnTimeout,
                       // Safety: `timeout_timer` is owned by this.
                       Unretained(this), prefetch_key));
  }

  prefetch_container.AddObserver(this);
}

void PrefetchMatchResolver::UnregisterCandidate(
    const PrefetchKey& prefetch_key,
    bool is_served,
    PrefetchPotentialCandidateServingResult serving_result) {
  // By #prefetch-key-availability
  auto it = candidates_.find(prefetch_key);
  CHECK(it != candidates_.end());
  CandidateData* candidate_data = it->second.get();
  TRACE_EVENT("loading", "PrefetchMatchResolver::UnregisterCandidate",
              perfetto::TerminatingFlow::FromPointer(candidate_data),
              "serving_result", static_cast<int>(serving_result));

  CHECK(candidate_data->prefetch_container);
  PrefetchContainer& prefetch_container = *candidate_data->prefetch_container;

  if (PreloadServingMetricsCapsule::IsFeatureEnabled()) {
    if (&prefetch_container == prefetch_ahead_of_prerender_for_metrics_.get()) {
      prefetch_match_metrics_
          ->prefetch_potential_candidate_serving_result_ahead_of_prerender =
          serving_result;
      prefetch_match_metrics_->prefetch_container_metrics_ahead_of_prerender =
          std::make_unique<PrefetchContainerMetrics>(
              prefetch_container.GetPrefetchContainerMetrics());
    }

    // While `PrefetchMatchResolver` works well with multiple candidates, we
    // have at most one candidate in almost all cases. So, we record the last
    // `PrefetchPotentialCandidateServingResult`.
    //
    // For more details, see
    // https://docs.google.com/document/d/1ITMr_qyysUPIMZpLkmpQABwtVseMBduRqxHGZxIJ1R0/edit?resourcekey=0-ccZ-G6JV4WO-1bP4TiNvjQ&tab=t.x99jls7s2xug
    prefetch_match_metrics_->prefetch_potential_candidate_serving_result_last =
        serving_result;
  }

  prefetch_container.OnUnregisterCandidate(navigated_key_.url(), is_served,
                                           serving_result, is_nav_prerender_,
                                           GetBlockedDuration());
  prefetch_container.RemoveObserver(this);
  candidates_.erase(it);
}

void PrefetchMatchResolver::OnWillBeDestroyed(
    PrefetchContainer& prefetch_container) {
  MaybeUnblockForUnmatch(prefetch_container,
                         PrefetchPotentialCandidateServingResult::
                             kNotServedPrefetchWillBeDestroyed);
}

void PrefetchMatchResolver::OnGotInitialEligibility(
    PrefetchContainer& prefetch_container,
    PreloadingEligibility eligibility) {
  CHECK(features::UsePrefetchPrerenderIntegration());

  if (eligibility != PreloadingEligibility::kEligible) {
    MaybeUnblockForUnmatch(
        prefetch_container,
        PrefetchPotentialCandidateServingResult::kNotServedIneligiblePrefetch);
  }
}

void PrefetchMatchResolver::OnDeterminedHead(
    PrefetchContainer& prefetch_container) {
  CHECK(candidates_.contains(prefetch_container.key()));
  CHECK(!prefetch_container.is_in_dtor());

  // Note that `OnDeterimnedHead()` is called even if `PrefetchContainer` is in
  // failure `PrefetchState`. See, for example, https://crbug.com/375333786.

  // Remove the candidate if the final `PrefetchServiceWorkerState` is not
  // matching, i.e. we started speculatively BlockUntilHead on the candidate
  // when `PrefetchServiceWorkerState::kAllowed`, but the final
  // `PrefetchServiceWorkerState` found not matching here. At the time of
  // `OnDeterminedHead()`, service worker controller check is always done and
  // thus the final `PrefetchServiceWorkerState` is already determined here.
  //
  // TODO(https://crbug.com/40947546): Maybe we could unblock earlier when
  // `PrefetchServiceWorkerState` is transitioned from `kAllowed`.
  CHECK_NE(prefetch_container.service_worker_state(),
           PrefetchServiceWorkerState::kAllowed);
  if (prefetch_container.service_worker_state() !=
      expected_service_worker_state_) {
    CHECK(base::FeatureList::IsEnabled(features::kPrefetchServiceWorker));
    MaybeUnblockForUnmatch(prefetch_container,
                           PrefetchPotentialCandidateServingResult::
                               kNotServedPrefetchServiceWorkerStateMismatch);
    return;
  }

  PrefetchServableState servable_state =
      prefetch_container.GetServableState(PrefetchCacheableDuration());
  PrefetchMatchResolverAction match_resolver_action =
      prefetch_container.GetMatchResolverAction(PrefetchCacheableDuration());
  switch (servable_state) {
    case PrefetchServableState::kShouldBlockUntilEligibilityGot:
      // All callsites of `PrefetchContainer::OnDeterminedHead()` are
      // `PrefetchStreamingURLLoader`, which implies the prefetch passed
      // eligibility check.
      NOTREACHED();
    case PrefetchServableState::kServable:
      // proceed
      break;
    // Otherwise, `MaybeUnblockForUnmatch()`.
    case PrefetchServableState::kShouldBlockUntilHeadReceived:
    case PrefetchServableState::kNotServable:
      auto potential_candidate_serving_result = [&]() {
        switch (servable_state) {
          case PrefetchServableState::kShouldBlockUntilEligibilityGot:
          case PrefetchServableState::kServable:
            NOTREACHED();
            // `kShouldBlockUntilHeadReceived` case occurs if a prefetch is
            // redirected and the redirect is not eligible.
            //
            //    PrefetchService::OnGotEligibilityForRedirect()
            // -> PrefetchStreamingURLLoader::HandleRedirect(kFail)
            // -> PrefetchContainer::OnDeterminedHead()
            // -> here
          case PrefetchServableState::kShouldBlockUntilHeadReceived:
            return PrefetchPotentialCandidateServingResult::
                kNotServedOnDeterminedHeadWithShouldBlockUntilHeadReceived;
          case PrefetchServableState::kNotServable:
            if (match_resolver_action.kind() ==
                    PrefetchMatchResolverAction::ActionKind::kMaybeServe &&
                match_resolver_action.is_expired() == true) {
              return PrefetchPotentialCandidateServingResult::
                  kNotServedOnDeterminedHeadWithServableExpired;
            } else {
              CHECK_EQ(match_resolver_action.kind(),
                       PrefetchMatchResolverAction::ActionKind::kDrop);

              switch (match_resolver_action.prefetch_container_load_state()) {
                case PrefetchContainer::LoadState::kFailedIneligible:
                  return PrefetchPotentialCandidateServingResult::
                      kNotServedIneligibleRedirect;
                case PrefetchContainer::LoadState::kFailedDeterminedHead:
                case PrefetchContainer::LoadState::kFailed:
                  return PrefetchPotentialCandidateServingResult::
                      kNotServedLoadFailed;
                case PrefetchContainer::LoadState::kNotStarted:
                case PrefetchContainer::LoadState::kEligible:
                case PrefetchContainer::LoadState::kStarted:
                case PrefetchContainer::LoadState::kDeterminedHead:
                case PrefetchContainer::LoadState::kCompleted:
                case PrefetchContainer::LoadState::kFailedHeldback:
                  NOTREACHED();
              }
            }
        }
      }();

      // TODO(crbug.com/459617177): We observed
      // `kNotServedOnDeterminedHeadWithNotServableUnknown` for prerender with
      // prefetch ahead of prerender. To understand the behavior, we record an
      // UMA.
      //
      // See also
      // `.*PrefetchMatchMetrics.ExistsPaopThen.PotentialCandidateServingResultAndServableStateAndMatcherAction`.
      //
      // The difference is here we use `PrefetchContainer` of the target of
      // `OnDeterminedHead()` and log per `OnDeterminedHead()` call that results
      // unmatch. The other uses prefetch ahead of prerender and logs at the end
      // of navigation with some conditions.
      //
      // Cardinality: `#PrefetchPotentialCandidateServingResult * 16 = 14 * 16 =
      // 224.
      base::UmaHistogramSparse(
          "Prefetch.PrefetchMatchResolver.OnDeterminedHeadWithUnmatch."
          "PotentialCandidateServingResultAndServableStateAndMatcherAction",
          GetCodeOfPotentialCandidateServingResultAndServableStateAndMatcherAction(
              potential_candidate_serving_result, servable_state,
              match_resolver_action));

      MaybeUnblockForUnmatch(prefetch_container,
                             potential_candidate_serving_result);
      return;
  }

  if (prefetch_container.CreateServingHandle()
          .HaveDefaultContextCookiesChanged()) {
    UnblockForCookiesChanged(prefetch_container.key());
    return;
  }

  // Non-redirect header is received and now the value of
  // `PrefetchContainer::IsNoVarySearchHeaderMatch()` is determined.
  const bool is_match =
      prefetch_container.IsExactMatch(navigated_key_.url()) ||
      prefetch_container.IsNoVarySearchHeaderMatch(navigated_key_.url());
  if (!is_match) {
    MaybeUnblockForUnmatch(prefetch_container,
                           PrefetchPotentialCandidateServingResult::
                               kNotServedDeterminedNVSHeaderMismatch);
    return;
  }

  // Got matching and servable.
  UnblockForMatch(prefetch_container.key());
}

void PrefetchMatchResolver::OnPrefetchCompletedOrFailed(
    PrefetchContainer& prefetch_container,
    const network::URLLoaderCompletionStatus& completion_status,
    const std::optional<int>& response_code) {}

void PrefetchMatchResolver::OnTimeout(PrefetchKey prefetch_key) {
  // `timeout_timer` is alive, which implies `candidate` is alive.
  auto it = candidates_.find(prefetch_key);
  CHECK(it != candidates_.end());
  CandidateData* candidate_data = it->second.get();
  CHECK(candidate_data->prefetch_container);

  MaybeUnblockForUnmatch(
      *candidate_data->prefetch_container,
      PrefetchPotentialCandidateServingResult::kNotServedBlockUntilHeadTimeout);
}

void PrefetchMatchResolver::UnblockForMatch(const PrefetchKey& prefetch_key) {
  TRACE_EVENT_BEGIN("loading", "PrefetchMatchResolver::UnblockForMatch",
                    perfetto::Flow::FromPointer(this));

  // By #prefetch-key-availability
  auto it = candidates_.find(prefetch_key);
  CHECK(it != candidates_.end());
  CandidateData* candidate_data = it->second.get();
  CHECK(candidate_data->prefetch_container);
  PrefetchContainer& prefetch_container = *candidate_data->prefetch_container;

  CHECK_EQ(prefetch_container.service_worker_state(),
           expected_service_worker_state_);

  UnregisterCandidate(prefetch_key, /*is_served=*/true,
                      PrefetchPotentialCandidateServingResult::kServed);

  // Unregister remaining candidates as not served.
  for (auto& key2 : Keys(candidates_)) {
    UnregisterCandidate(key2, /*is_served=*/false,
                        PrefetchPotentialCandidateServingResult::
                            kNotServedOtherCandidatesAreMatched);
  }

  // Postprocess for success case.

  PrefetchServingHandle serving_handle =
      prefetch_container.CreateServingHandle();

  // Cookie change is handled in two paths:
  //
  // - Before waiting `PrefetchContainer`, which is `kServable` at the match
  //   start timing. It is handled in `FindPrefetchInternal()`.
  // - After waiting `PrefetchContainer`, which is `kShouldBlockUntilHead` at
  //   the match start timing. It is handled in `OnDeterminedHead()`.
  //
  // So, the below condition is satisfied.
  CHECK(!serving_handle.HaveDefaultContextCookiesChanged());

  if (!serving_handle.HasIsolatedCookieCopyStarted()) {
    // Basically, we can assume `PrefetchService` is available as waiting
    // `PrefetchContainer` is owned by it. But in unit tests, we use invalid
    // frame tree node id and this `prefetch_service` is not available.
    if (prefetch_service_) {
      prefetch_service_->CopyIsolatedCookies(serving_handle);
    }
  }
  CHECK(serving_handle);

  TRACE_EVENT_END("loading");

  UnblockInternal(std::move(serving_handle));
}

void PrefetchMatchResolver::UnblockForNoCandidates() {
  {
    TRACE_EVENT("loading", "PrefetchMatchResolver::UnblockForNoCandidates",
                perfetto::Flow::FromPointer(this));

    if (prefetch_service_ && expected_service_worker_state_ ==
                                 PrefetchServiceWorkerState::kDisallowed) {
      prefetch_service_->AddRecentUnmatchedNavigatedKeysForMetrics(
          navigated_key_);
    }
  }

  UnblockInternal({});
}

void PrefetchMatchResolver::MaybeUnblockForUnmatch(
    const PrefetchContainer& prefetch_container,
    PrefetchPotentialCandidateServingResult serving_result) {
  {
    TRACE_EVENT("loading", "PrefetchMatchResolver::MaybeUnblockForUnmatch",
                perfetto::Flow::FromPointer(this));

    UnregisterCandidate(prefetch_container.key(), /*is_served=*/false,
                        serving_result);
  }

  if (candidates_.size() == 0) {
    UnblockForNoCandidates();
  }

  // It still waits for other `PrefetchContainer`s.
}

void PrefetchMatchResolver::UnblockForCookiesChanged(const PrefetchKey& key) {
  // Unregister remaining candidates as not served, with calling
  // `PrefetchContainer::OnDetectedCookiesChange()`.
  for (auto& prefetch_key : Keys(candidates_)) {
    // By #prefetch-key-availability
    auto it = candidates_.find(prefetch_key);
    CHECK(it != candidates_.end());
    CandidateData* candidate_data = it->second.get();
    CHECK(candidate_data->prefetch_container);
    PrefetchContainer& prefetch_container = *candidate_data->prefetch_container;

    UnregisterCandidate(
        prefetch_key, /*is_served=*/false,
        PrefetchPotentialCandidateServingResult::kNotServedCookiesChanged);

    prefetch_container.OnDetectedCookiesChange(
        /*is_unblock_for_cookies_changed_triggered_by_this_prefetch_container*/
        prefetch_key == key);
  }

  UnblockForNoCandidates();
}

void PrefetchMatchResolver::UnblockInternal(
    PrefetchServingHandle serving_handle) {
  TRACE_EVENT_BEGIN("loading", "PrefetchMatchResolver::FindPrefetch",
                    perfetto::Flow::FromPointer(this), flow_);

  // Postcondition: This resolver waits for no `PrefetchContainer`s when it has
  // been unblocking.
  CHECK_EQ(candidates_.size(), 0u);

  if (PreloadServingMetricsCapsule::IsFeatureEnabled()) {
    PrefetchContainer* prefetch_container =
        serving_handle.GetPrefetchContainer();
    prefetch_match_metrics_->prefetch_container_metrics =
        prefetch_container
            ? std::make_unique<PrefetchContainerMetrics>(
                  prefetch_container->GetPrefetchContainerMetrics())
            : std::unique_ptr<PrefetchContainerMetrics>(nullptr);

    AttachPrefetchMatchPrerenderDebugMetrics();

    prefetch_match_metrics_->time_match_end = base::TimeTicks::Now();

    if (navigation_request_for_metrics_) {
      auto& preload_serving_metrics_holder =
          *PreloadServingMetricsHolder::GetOrCreateForNavigationHandle(
              *navigation_request_for_metrics_.get());
      preload_serving_metrics_holder.AddPrefetchMatchMetrics(
          std::move(prefetch_match_metrics_));
    }
  }

  auto callback = std::move(callback_);

  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                             std::move(self_));

  TRACE_EVENT_END("loading");

  std::move(callback).Run(std::move(serving_handle));
}

void PrefetchMatchResolver::AttachPrefetchMatchPrerenderDebugMetrics() {
  if (!UsePrefetchScheduler()) {
    return;
  }

  if (!prerender_host_for_metrics_) {
    return;
  }

  if (!prefetch_service_) {
    return;
  }

  // We can't use `prefetch_ahead_of_prerender_for_metrics` as it is set in
  // `RegisterCandidate()` and we'll miss
  // `PrefetchMatchResolverAction::ActionKind::kDrop` case, which is our main
  // motivation for this metrics.
  PrefetchContainer* prefetch_container =
      prefetch_service_->FindPrefetchAheadOfPrerenderForMetrics(
          prerender_host_for_metrics_->preload_pipeline_info());

  auto metrics = std::make_unique<PrefetchMatchPrerenderDebugMetrics>();
  [&]() {
    if (!prefetch_container) {
      return;
    }

    metrics->prefetch_ahead_of_prerender_debug_metrics =
        std::make_unique<PrefetchMatchPrefetchAheadOfPrerenderDebugMetrics>();
    metrics->prefetch_ahead_of_prerender_debug_metrics->prefetch_status =
        prefetch_container->GetPrefetchStatus();
    metrics->prefetch_ahead_of_prerender_debug_metrics->servable_state =
        prefetch_container->GetServableState(PrefetchCacheableDuration());
    metrics->prefetch_ahead_of_prerender_debug_metrics->match_resolver_action =
        prefetch_container->GetMatchResolverAction(PrefetchCacheableDuration());
    metrics->prefetch_ahead_of_prerender_debug_metrics->queue_size =
        prefetch_service_->GetPrefetchSchedulerForMetrics()
            .GetQueueSizeForMetrics();
    metrics->prefetch_ahead_of_prerender_debug_metrics->queue_index =
        prefetch_service_->GetPrefetchSchedulerForMetrics().GetIndexForMetrics(
            *prefetch_container);
  }();

  prefetch_match_metrics_->prerender_debug_metrics = std::move(metrics);
}

}  // namespace content
