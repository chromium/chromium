// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_match_resolver.h"

#include <algorithm>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prerender/prerender_features.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/public/browser/navigation_handle_user_data.h"

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
    PrefetchContainer::Key navigated_key,
    PrefetchServiceWorkerState expected_service_worker_state,
    bool is_nav_prerender,
    base::WeakPtr<PrefetchService> prefetch_service,
    Callback callback)
    : navigated_key_(std::move(navigated_key)),
      expected_service_worker_state_(expected_service_worker_state),
      prefetch_service_(std::move(prefetch_service)),
      callback_(std::move(callback)),
      is_nav_prerender_(is_nav_prerender) {
  switch (expected_service_worker_state_) {
    case PrefetchServiceWorkerState::kAllowed:
      NOTREACHED();
    case PrefetchServiceWorkerState::kControlled:
      CHECK(base::FeatureList::IsEnabled(features::kPrefetchServiceWorker));
      break;
    case PrefetchServiceWorkerState::kDisallowed:
      break;
  }
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
    PrefetchContainer::Key navigated_key,
    PrefetchServiceWorkerState expected_service_worker_state,
    bool is_nav_prerender,
    PrefetchService& prefetch_service,
    base::WeakPtr<PrefetchServingPageMetricsContainer>
        serving_page_metrics_container,
    Callback callback) {
  TRACE_EVENT0("loading", "PrefetchMatchResolver::FindPrefetch");
  // See the comment of `self_`.
  auto prefetch_match_resolver = base::WrapUnique(new PrefetchMatchResolver(
      std::move(navigated_key), expected_service_worker_state, is_nav_prerender,
      prefetch_service.GetWeakPtr(), std::move(callback)));
  PrefetchMatchResolver& ref = *prefetch_match_resolver.get();
  ref.self_ = std::move(prefetch_match_resolver);

  ref.FindPrefetchInternal(prefetch_service,
                           std::move(serving_page_metrics_container));
}

void PrefetchMatchResolver::FindPrefetchInternal(
    PrefetchService& prefetch_service,
    base::WeakPtr<PrefetchServingPageMetricsContainer>
        serving_page_metrics_container) {
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
      case PrefetchContainer::ServableState::kServable:
        if (candidate.second->prefetch_container->CreateReader()
                .HaveDefaultContextCookiesChanged()) {
          UnblockForCookiesChanged(candidate.second->prefetch_container->key());
          return;
        }
        break;
      case PrefetchContainer::ServableState::kNotServable:
        NOTREACHED();
      case PrefetchContainer::ServableState::kShouldBlockUntilHeadReceived:
      case PrefetchContainer::ServableState::kShouldBlockUntilEligibilityGot:
        // nop
        break;
    }
  }

  for (auto& candidate : candidates_) {
    switch (servable_states.at(candidate.first)) {
      case PrefetchContainer::ServableState::kServable:
        // Got matching and servable.
        UnblockForMatch(candidate.first);
        return;
      case PrefetchContainer::ServableState::kNotServable:
        NOTREACHED();
      case PrefetchContainer::ServableState::kShouldBlockUntilHeadReceived:
      case PrefetchContainer::ServableState::kShouldBlockUntilEligibilityGot:
        // nop
        break;
    }
  }

  // There is no matching and servable prefetch at this point. We should wait
  // remaining ones.

  CHECK(!wait_started_at_.has_value());
  wait_started_at_ = base::TimeTicks::Now();

  for (auto& candidate : candidates_) {
    StartWaitFor(candidate.first, servable_states.at(candidate.first));
  }

  if (candidates_.size() == 0) {
    UnblockForNoCandidates();
  }
}

void PrefetchMatchResolver::RegisterCandidate(
    PrefetchContainer& prefetch_container) {
  auto candidate_data = std::make_unique<CandidateData>();
  // #prefetch-key-availability
  //
  // Note that `CHECK(candidates_.contains(prefetch_key))` and
  // `CHECK(candidate_data->prefetch_container)` below always hold because
  // `PrefetchMatchResolver` observes lifecycle events of `PrefetchContainer`.
  candidate_data->prefetch_container = prefetch_container.GetWeakPtr();
  candidate_data->timeout_timer = nullptr;

  candidates_[prefetch_container.key()] = std::move(candidate_data);
}

void PrefetchMatchResolver::StartWaitFor(
    const PrefetchContainer::Key& prefetch_key,
    PrefetchContainer::ServableState servable_state) {
  // By #prefetch-key-availability
  CHECK(candidates_.contains(prefetch_key));
  auto& candidate_data = candidates_[prefetch_key];
  CHECK(candidate_data->prefetch_container);
  PrefetchContainer& prefetch_container = *candidate_data->prefetch_container;

  // `kServable` -> `kNotServable` is the only possible change during
  // `FindPrefetchInternal()` call.
  CHECK_EQ(prefetch_container.GetServableState(PrefetchCacheableDuration()),
           servable_state);
  switch (servable_state) {
    case PrefetchContainer::ServableState::kServable:
    case PrefetchContainer::ServableState::kNotServable:
      NOTREACHED();
    case PrefetchContainer::ServableState::kShouldBlockUntilHeadReceived:
    case PrefetchContainer::ServableState::kShouldBlockUntilEligibilityGot:
      // nop
      break;
  }
  CHECK(!candidate_data->timeout_timer);

  // TODO(crbug.com/356552413): Merge
  // https://chromium-review.googlesource.com/c/chromium/src/+/5668924 and
  // write tests.
  base::TimeDelta timeout = PrefetchBlockUntilHeadTimeout(
      prefetch_container.GetPrefetchType(), is_nav_prerender_);
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
    const PrefetchContainer::Key& prefetch_key,
    bool is_served) {
  // By #prefetch-key-availability
  CHECK(candidates_.contains(prefetch_key));
  auto& candidate_data = candidates_[prefetch_key];
  CHECK(candidate_data->prefetch_container);
  PrefetchContainer& prefetch_container = *candidate_data->prefetch_container;

  prefetch_container.OnUnregisterCandidate(navigated_key_.url(), is_served,
                                           GetBlockedDuration());
  prefetch_container.RemoveObserver(this);
  candidates_.erase(prefetch_key);
}

void PrefetchMatchResolver::OnWillBeDestroyed(
    PrefetchContainer& prefetch_container) {
  MaybeUnblockForUnmatch(prefetch_container.key());
}

void PrefetchMatchResolver::OnGotInitialEligibility(
    PrefetchContainer& prefetch_container,
    PreloadingEligibility eligibility) {
  CHECK(features::UsePrefetchPrerenderIntegration());

  if (eligibility != PreloadingEligibility::kEligible) {
    MaybeUnblockForUnmatch(prefetch_container.key());
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
    MaybeUnblockForUnmatch(prefetch_container.key());
    return;
  }

  switch (prefetch_container.GetServableState(PrefetchCacheableDuration())) {
    case PrefetchContainer::ServableState::kShouldBlockUntilEligibilityGot:
      // All callsites of `PrefetchContainer::OnDeterminedHead()` are
      // `PrefetchStreamingURLLoader`, which implies the prefetch passed
      // eligibility check.
      NOTREACHED();
    // `kShouldBlockUntilHeadReceived` case occurs if a prefetch is redirected
    // and the redirect is not eligible.
    //
    //    PrefetchService::OnGotEligibilityForRedirect()
    // -> PrefetchStreamingURLLoader::HandleRedirect(kFail)
    // -> PrefetchContainer::OnDeterminedHead()
    // -> here
    case PrefetchContainer::ServableState::kShouldBlockUntilHeadReceived:
    case PrefetchContainer::ServableState::kNotServable:
      MaybeUnblockForUnmatch(prefetch_container.key());
      return;
    case PrefetchContainer::ServableState::kServable:
      // Proceed.
      break;
  }

  if (prefetch_container.CreateReader().HaveDefaultContextCookiesChanged()) {
    UnblockForCookiesChanged(prefetch_container.key());
    return;
  }

  // Non-redirect header is received and now the value of
  // `PrefetchContainer::IsNoVarySearchHeaderMatch()` is determined.
  const bool is_match =
      prefetch_container.IsExactMatch(navigated_key_.url()) ||
      prefetch_container.IsNoVarySearchHeaderMatch(navigated_key_.url());
  if (!is_match) {
    MaybeUnblockForUnmatch(prefetch_container.key());
    return;
  }

  if (prefetch_container.HasPrefetchBeenConsideredToServe()) {
    MaybeUnblockForUnmatch(prefetch_container.key());
    return;
  }

  // Got matching and servable.
  UnblockForMatch(prefetch_container.key());
}

void PrefetchMatchResolver::OnTimeout(PrefetchContainer::Key prefetch_key) {
  // `timeout_timer` is alive, which implies `candidate` is alive.
  CHECK(candidates_.contains(prefetch_key));
  auto& candidate_data = candidates_[prefetch_key];
  CHECK(candidate_data->prefetch_container);

  MaybeUnblockForUnmatch(prefetch_key);
}

void PrefetchMatchResolver::UnblockForMatch(
    const PrefetchContainer::Key& prefetch_key) {
  TRACE_EVENT0("loading", "PrefetchMatchResolver::UnblockForMatch");

  // By #prefetch-key-availability
  CHECK(candidates_.contains(prefetch_key));
  auto& candidate_data = candidates_[prefetch_key];
  CHECK(candidate_data->prefetch_container);
  PrefetchContainer& prefetch_container = *candidate_data->prefetch_container;

  CHECK_EQ(prefetch_container.service_worker_state(),
           expected_service_worker_state_);

  UnregisterCandidate(prefetch_key, /*is_served=*/true);

  // Unregister remaining candidates as not served.
  for (auto& key2 : Keys(candidates_)) {
    UnregisterCandidate(key2, /*is_served=*/false);
  }

  // Postprocess for success case.

  PrefetchContainer::Reader reader = prefetch_container.CreateReader();

  // Cookie change is handled in two paths:
  //
  // - Before waiting `PrefetchContainer`, which is `kServable` at the match
  //   start timing. It is handled in `FindPrefetchInternal()`.
  // - After waiting `PrefetchContainer`, which is `kShouldBlockUntilHead` at
  //   the match start timing. It is handled in `OnDeterminedHead()`.
  //
  // So, the below condition is satisfied.
  CHECK(!reader.HaveDefaultContextCookiesChanged());

  if (!reader.HasIsolatedCookieCopyStarted()) {
    // Basically, we can assume `PrefetchService` is available as waiting
    // `PrefetchContainer` is owned by it. But in unit tests, we use invalid
    // frame tree node id and this `prefetch_service` is not available.
    if (prefetch_service_) {
      prefetch_service_->CopyIsolatedCookies(reader);
    }
  }
  CHECK(reader);

  UnblockInternal(std::move(reader));
}

void PrefetchMatchResolver::UnblockForNoCandidates() {
  TRACE_EVENT0("loading", "PrefetchMatchResolver::UnblockForNoCandidates");
  UnblockInternal({});
}

void PrefetchMatchResolver::MaybeUnblockForUnmatch(
    const PrefetchContainer::Key& prefetch_key) {
  UnregisterCandidate(prefetch_key, /*is_served=*/false);

  if (candidates_.size() == 0) {
    UnblockForNoCandidates();
  }

  // It still waits for other `PrefetchContainer`s.
}

void PrefetchMatchResolver::UnblockForCookiesChanged(
    const PrefetchContainer::Key& key) {
  // Unregister remaining candidates as not served, with calling
  // `PrefetchContainer::OnDetectedCookiesChange()`.
  for (auto& prefetch_key : Keys(candidates_)) {
    // By #prefetch-key-availability
    CHECK(candidates_.contains(prefetch_key));
    auto& candidate_data = candidates_[prefetch_key];
    CHECK(candidate_data->prefetch_container);
    PrefetchContainer& prefetch_container = *candidate_data->prefetch_container;

    UnregisterCandidate(prefetch_key, /*is_served=*/false);

    prefetch_container.OnDetectedCookiesChange(
        /*is_unblock_for_cookies_changed_triggered_by_this_prefetch_container*/
        prefetch_key == key);
  }

  UnblockForNoCandidates();
}

void PrefetchMatchResolver::UnblockInternal(PrefetchContainer::Reader reader) {
  // Postcondition: This resolver waits for no `PrefetchContainer`s when it has
  // been unblocking.
  CHECK_EQ(candidates_.size(), 0u);

  auto callback = std::move(callback_);

  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                             std::move(self_));

  std::move(callback).Run(std::move(reader));
}

}  // namespace content
