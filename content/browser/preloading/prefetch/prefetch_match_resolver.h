// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_MATCH_RESOLVER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_MATCH_RESOLVER_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "content/browser/preloading/prefetch/no_vary_search_helper.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle_user_data.h"

namespace content {

class PrefetchContainer;

// Manages matching process of prefetch
// https://wicg.github.io/nav-speculation/prefetch.html#wait-for-a-matching-prefetch-record
//
// This class is created per call of
// `PrefetchURLLoaderInterceptor::MaybeCreateLoader()` except redirects for
// already matched prefetch and still servable ones, i.e. a prefetch was matched
// by prior call of `PrefetchMatchResolver::FindPrefetch()`.
//
// Lifetime of this class is from the call of `FindPrefetch()` to calling
// `callback_`. This is owned by itself. See the comment on `self_`.
class CONTENT_EXPORT PrefetchMatchResolver final
    : public PrefetchContainer::Observer {
 public:
  using Callback = base::OnceCallback<void(PrefetchContainer::Reader reader)>;

  ~PrefetchMatchResolver() override;

  // Not movable nor copyable.
  PrefetchMatchResolver(PrefetchMatchResolver&& other) = delete;
  PrefetchMatchResolver& operator=(PrefetchMatchResolver&& other) = delete;
  PrefetchMatchResolver(const PrefetchMatchResolver&) = delete;
  PrefetchMatchResolver& operator=(const PrefetchMatchResolver&) = delete;

  // PrefetchContainer::Observer implementation
  void OnWillBeDestroyed(PrefetchContainer& prefetch_container) override;
  void OnGotInitialEligibility(PrefetchContainer& prefetch_container,
                               PreloadingEligibility eligibility) override;
  void OnDeterminedHead(PrefetchContainer& prefetch_container) override;

  // Finds prefetch that matches to a navigation and is servable.
  //
  // Corresponds to
  // https://wicg.github.io/nav-speculation/prefetch.html#wait-for-a-matching-prefetch-record
  //
  // This method is async. `callback` will be called when it is done.
  // `bool(reader)` is true iff a matching servable prefetch is found.
  //
  // Matches prefetches only if its final PrefetchServiceWorkerState is
  // `expected_service_worker_state` (either `kControlled` or `kDisallowed`).
  static void FindPrefetch(
      PrefetchContainer::Key navigated_key,
      PrefetchServiceWorkerState expected_service_worker_state,
      bool is_nav_prerender,
      PrefetchService& prefetch_service,
      base::WeakPtr<PrefetchServingPageMetricsContainer>
          serving_page_metrics_container,
      Callback callback);

 private:
  struct CandidateData final {
    CandidateData();
    ~CandidateData();

    base::WeakPtr<PrefetchContainer> prefetch_container;
    std::unique_ptr<base::OneShotTimer> timeout_timer;
  };

  explicit PrefetchMatchResolver(
      PrefetchContainer::Key navigated_key,
      PrefetchServiceWorkerState expected_service_worker_state,
      bool is_nav_prerender,
      base::WeakPtr<PrefetchService> prefetch_service,
      Callback callback);

  // Returns blocked duration. Returns null iff it's not blocked yet.
  std::optional<base::TimeDelta> GetBlockedDuration() const;

  // Helpers of `FindPrefetch()`.
  //
  // Control flow starts with `FindPrefetchInternal()` and ends with
  // `UnblockInternal()`.
  //
  // Actually, it is different from
  // https://wicg.github.io/nav-speculation/prefetch.html#wait-for-a-matching-prefetch-record
  // Major ones:
  //
  // - This implementation has timeout: `CandidateData::timeout_timer`.
  // - This implementation collects candidate prefetches first. So, it doesn't
  //   handle prefetches started after this method started.
  void FindPrefetchInternal(PrefetchService& prefetch_service,
                            base::WeakPtr<PrefetchServingPageMetricsContainer>
                                serving_page_metrics_container);
  // Each candidate `PrefetchContainer` proceeds to
  //
  //    `RegisterCandidate()` (required)
  // -> `StartWaitFor()` (optional, if servable state is
  //    `kShouldBlockUntilHead`)
  // -> `UnregisterCandidate()` (required)
  void RegisterCandidate(PrefetchContainer& prefetch_container);
  void StartWaitFor(const PrefetchContainer::Key& prefetch_key,
                    PrefetchContainer::ServableState servable_state);
  void UnregisterCandidate(const PrefetchContainer::Key& prefetch_key,
                           bool is_served);
  void OnTimeout(PrefetchContainer::Key prefetch_key);
  void UnblockForMatch(const PrefetchContainer::Key& prefetch_key);
  void UnblockForNoCandidates();
  // Unregisters unmatched prefetch and unblocks if there are no other waiting
  // prefetches.
  void MaybeUnblockForUnmatch(const PrefetchContainer::Key& prefetch_key);
  void UnblockForCookiesChanged(const PrefetchContainer::Key& key);
  void UnblockInternal(PrefetchContainer::Reader reader);

  // Lifetime of this class is from the call of `FindPrefetch()` to calling
  // `callback_`. Note that
  //
  // - `FindPrefetchInternal()` consumes this class. We don't want to use this
  //   class twice.
  // - `NavigationLoaderInterceptor::MaybeCreateLoader()` can be called multiple
  //   times, e.g. redirect.
  //
  // So, we don't believe that `NavigationHandleUserData` is an appropriate
  // choice to manage lifetime. Possible choices are:
  //
  // A. This way.
  // B. Have another class that inherits `NavigationHandleUserData` and manages
  //    this class for each `NavigationLoaderInterceptor::MaybeCreateLoader()`
  //    call.
  //
  // Note that `NavigationLoaderInterceptor::MaybeCreateLoader()` requires that
  // `callback_` is eventually called. So, we don't need to care about memory
  // leak.
  //
  // A would be enough.
  std::unique_ptr<PrefetchMatchResolver> self_;

  const PrefetchContainer::Key navigated_key_;
  const PrefetchServiceWorkerState expected_service_worker_state_;
  base::WeakPtr<PrefetchService> prefetch_service_;
  Callback callback_;
  const bool is_nav_prerender_;
  std::map<PrefetchContainer::Key, std::unique_ptr<CandidateData>> candidates_;
  std::optional<base::TimeTicks> wait_started_at_ = std::nullopt;
};

// Abstracts required operations for `PrefetchContainer` that is used to collect
// match candidates in the first phase of
// `PrefetchMatchResolver::FindPrefetch()`. Used for unit testing.
template <class T>
concept MatchCandidate =
    requires(T& t,
             const GURL& url,
             base::TimeDelta cacheable_duration,
             base::WeakPtr<PrefetchServingPageMetricsContainer>
                 serving_page_metrics_container,
             std::ostream& ostream) {
      t.key();
      t.GetURL();
      t.GetServableState(cacheable_duration);
      t.GetNoVarySearchHint();
      t.IsNoVarySearchHeaderMatch(url);
      t.ShouldWaitForNoVarySearchHeader(url);
      t.HasPrefetchStatus();
      t.GetPrefetchStatus();
      t.HasPrefetchBeenConsideredToServe();
      t.IsDecoy();
      t.SetServingPageMetrics(serving_page_metrics_container);
      t.UpdateServingPageMetrics();
      ostream << t;
    };

// Do not use it outside of this header.
//
// Collects "potentially matching" `PrefetchContainer`s.
//
// "potentially matching" is either:
//
// - Exact match
// - No-Vary-Search header match.
// - No-Vary-Search hint match and non redirect header is not still arrived.
template <class T>
  requires MatchCandidate<T>
std::vector<T*> CollectPotentialMatchPrefetchContainers(
    const std::map<PrefetchContainer::Key, std::unique_ptr<T>>& prefetches,
    const PrefetchContainer::Key& navigated_key) {
  std::vector<T*> result;

  // Note that exact match one is at the head if exists by the property of
  // `IterateCandidates()`.
  no_vary_search::IterateCandidates(
      navigated_key, prefetches,
      base::BindRepeating(
          [](const PrefetchContainer::Key& navigated_key,
             std::vector<T*>* result,
             const std::unique_ptr<T>& prefetch_container,
             no_vary_search::MatchType match_type) {
            switch (match_type) {
              case no_vary_search::MatchType::kExact:
              case no_vary_search::MatchType::kNoVarySearchHeader:
              case no_vary_search::MatchType::kNoVarySearchHint:
                result->push_back(prefetch_container.get());
                break;
              case no_vary_search::MatchType::kOther:
                break;
            }
            return no_vary_search::IterateCandidateResult::kContinue;
          },
          navigated_key, base::Unretained(&result)));

  return result;
}

// Do not use it outside of this header.
//
// Returns "availability" of a `PrefetchContainer`.
//
// "Available" here is not a technical term. It means that the
// `PrefetchContainer` is able to be used or has the possibility in the near
// future. See implementation for the detailed conditions.
template <class T>
  requires MatchCandidate<T>
bool IsCandidateAvailable(const T& candidate,
                          PrefetchContainer::ServableState servable_state,
                          bool is_nav_prerender) {
  if (candidate.HasPrefetchBeenConsideredToServe()) {
    DVLOG(1) << "CollectMatchCandidatesGeneric: skipped because already "
                "considered to serve: candidate = "
             << candidate;
    return false;
  }

  switch (servable_state) {
    case PrefetchContainer::ServableState::kNotServable:
      DVLOG(1) << "CollectMatchCandidatesGeneric: skipped because not "
                  "servable: candidate = "
               << candidate;
      return false;
    case PrefetchContainer::ServableState::kShouldBlockUntilEligibilityGot:
    case PrefetchContainer::ServableState::kShouldBlockUntilHeadReceived:
    case PrefetchContainer::ServableState::kServable:
      break;
  }

  switch (servable_state) {
    case PrefetchContainer::ServableState::kShouldBlockUntilEligibilityGot:
      if (!is_nav_prerender) {
        DVLOG(1)
            << "CollectMatchCandidatesGeneric: skipped because it's checking "
               "eligibility and the navigation is not a prerender: candidate = "
            << candidate;
        return false;
      }
      break;
    case PrefetchContainer::ServableState::kServable:
    case PrefetchContainer::ServableState::kNotServable:
    case PrefetchContainer::ServableState::kShouldBlockUntilHeadReceived:
      break;
  }

  if (candidate.IsDecoy()) {
    DVLOG(1) << "CollectMatchCandidatesGeneric: skipped because prefetch is a "
                "decoy: candidate = "
             << candidate;
    return false;
  }

  if (candidate.HasPrefetchStatus() &&
      candidate.GetPrefetchStatus() ==
          PrefetchStatus::kPrefetchNotUsedCookiesChanged) {
    // Note: This codepath is only be reached in practice if we create a
    // second NavigationRequest to this prefetch's URL. The first
    // NavigationRequest would call GetPrefetch, which might set this
    // PrefetchContainer's status to kPrefetchNotUsedCookiesChanged.
    DVLOG(1) << "CollectMatchCandidatesGeneric: skipped because cookies for "
                "url have changed since prefetch completed: candidate = "
             << candidate;
    return false;
  }

  DVLOG(1) << "CollectMatchCandidatesGeneric: matched: candidate = "
           << candidate;
  return true;
}

// Collects `PrefetchContainer`s that are expected to match to `navigated_key`.
//
// This is defined with the template for testing the first phase of
// `PrefetchMatchResolver::FindPrefetch()` with mock `PrefetchContainer`.
template <class T>
  requires MatchCandidate<T>
std::pair<
    std::vector<T*>,
    base::flat_map<PrefetchContainer::Key, PrefetchContainer::ServableState>>
CollectMatchCandidatesGeneric(
    const std::map<PrefetchContainer::Key, std::unique_ptr<T>>& prefetches,
    const PrefetchContainer::Key& navigated_key,
    bool is_nav_prerender,
    base::WeakPtr<PrefetchServingPageMetricsContainer>
        serving_page_metrics_container) {
  std::vector<T*> candidates =
      CollectPotentialMatchPrefetchContainers(prefetches, navigated_key);

  for (T* candidate : candidates) {
    candidate->SetServingPageMetrics(serving_page_metrics_container);
    candidate->UpdateServingPageMetrics();
  }

  std::vector<T*> candidates_available;
  // See the comment of `PrefetchService::CollectMatchCandidates()`.
  base::flat_map<PrefetchContainer::Key, PrefetchContainer::ServableState>
      servable_states;
  for (T* candidate : candidates) {
    PrefetchContainer::ServableState servable_state =
        candidate->GetServableState(PrefetchCacheableDuration());
    if (IsCandidateAvailable(*candidate, servable_state, is_nav_prerender)) {
      candidates_available.push_back(candidate);
      servable_states.emplace(candidate->key(), servable_state);
    }
  }

  return std::make_pair(std::move(candidates_available),
                        std::move(servable_states));
}

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_MATCH_RESOLVER_H_
