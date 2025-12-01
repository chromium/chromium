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
#include "content/browser/preloading/prefetch/prefetch_servable_state.h"
#include "content/browser/preloading/prefetch/prefetch_serving_handle.h"
#include "content/browser/preloading/preload_serving_metrics.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace content {

class PrefetchService;

// Represents the serving result with the detailed reason per potentially
// matching candidate. Only used for metrics purpose.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PrefetchPotentialCandidateServingResult)
enum class PrefetchPotentialCandidateServingResult {
  // The candidate is matched and served.
  kServed = 0,

  // The candidate is not served because the other potential candidate is
  // already determined to be served.
  kNotServedOtherCandidatesAreMatched = 1,

  // The candidate is not served because the cookie change is detected during
  // waiting the non-redirect header.
  kNotServedCookiesChanged = 2,

  // The candidate is not served because the corresponding prefetch container is
  // going to be destroyed during waiting the non-redirect header.
  kNotServedPrefetchWillBeDestroyed = 3,

  // The candidate is not served because it turned out to be ineligible.
  // This can be recorded only when
  // `features::UsePrefetchPrerenderIntegration()` is
  // true, where the prefetch matching starts before the initial eligibility is
  // determined.
  kNotServedIneligiblePrefetch = 4,

  // Deprecated
  //
  // The candidate is not served because the candidate received
  // `OnDeterminedHead()` but its associated `PrefetchServableState` is
  // not `kServable`.
  // kNotServedUnsatisfiedPrefetchServeableState = 5,

  // The candidate is not served because the candidate's
  // `PrefetchServiceWorkerState` was matched with the expected one when
  // starting matching but turned out to be mismatched after receiving the
  // non-redirect header.
  // This can be record only when `kPrefetchServiceWorker` is enabled.
  kNotServedPrefetchServiceWorkerStateMismatch = 6,

  // The candidate is not served because the candidate's url was matched with
  // the navigation's url using NVS hint but turned out to be mismatched using
  // actual NVS header when receiving the non-redirect header.
  kNotServedDeterminedNVSHeaderMismatch = 7,

  // The candidate is not served because of the timeout provided by
  // `PrefetchBlockUntilHeadTimeout()`.
  kNotServedBlockUntilHeadTimeout = 8,

  // The candidate is not served because
  // `PrefetchContainer::Observer::OnDeterminedHead()` is called with
  // `PrefetchServableState::kShouldBlockUntilHeadReceived`. Basically, we don't
  // expect to enter this path, but there is a buggy corner case.
  kNotServedOnDeterminedHeadWithShouldBlockUntilHeadReceived = 9,
  // The candidate is not served because
  // `PrefetchContainer::Observer::OnDeterminedHead()` is called but the
  // prefetch has been expired.
  kNotServedOnDeterminedHeadWithServableExpired = 10,
  // The candidate is not served due to ineligible redirect.
  kNotServedIneligibleRedirect = 11,
  // The candidate is not served because the loading is failed.
  kNotServedLoadFailed = 12,
  // Deprecated
  //
  // The candidate is not served because
  // `PrefetchContainer::Observer::OnDeterminedHead()` is called with
  // `PrefetchServableState::kNotServable` except for expired nor failure. We
  // don't expect to enter this path.
  // kNotServedOnDeterminedHeadWithNotServableUnknown = 13,

  // A special value for `PrefetchMatchResolver::UnblockForNoCandidates()`.
  kNotServedNoCandidates = 14,

  kMaxValue = kNotServedNoCandidates,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/prefetch/enums.xml)

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
  using Callback =
      base::OnceCallback<void(PrefetchServingHandle serving_handle)>;

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
  void OnPrefetchCompletedOrFailed(
      PrefetchContainer& prefetch_container,
      const network::URLLoaderCompletionStatus& completion_status,
      const std::optional<int>& response_code) override;

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
      FrameTreeNodeId frame_tree_node_id,
      PrefetchService& prefetch_service,
      PrefetchKey navigated_key,
      PrefetchServiceWorkerState expected_service_worker_state,
      base::WeakPtr<PrefetchServingPageMetricsContainer>
          serving_page_metrics_container,
      Callback callback,
      perfetto::Flow flow);
  static void FindPrefetchForTesting(
      PrefetchService& prefetch_service,
      PrefetchKey navigated_key,
      PrefetchServiceWorkerState expected_service_worker_state,
      bool is_nav_prerender,
      base::WeakPtr<PrefetchServingPageMetricsContainer>
          serving_page_metrics_container,
      Callback callback);

  void AttachPrefetchMatchPrerenderDebugMetrics();

 private:
  struct CandidateData final {
    CandidateData();
    ~CandidateData();

    base::WeakPtr<PrefetchContainer> prefetch_container;
    std::unique_ptr<base::OneShotTimer> timeout_timer;
  };

  static void FindPrefetchInternal1(
      base::WeakPtr<NavigationRequest> navigation_request,
      PrefetchService& prefetch_service,
      PrefetchKey navigated_key,
      PrefetchServiceWorkerState expected_service_worker_state,
      bool is_nav_prerender,
      base::WeakPtr<PrerenderHost> prerender_host,
      base::WeakPtr<PrefetchServingPageMetricsContainer>
          serving_page_metrics_container,
      Callback callback,
      perfetto::Flow flow);

  explicit PrefetchMatchResolver(
      base::WeakPtr<NavigationRequest> navigation_request,
      base::WeakPtr<PrefetchService> prefetch_service,
      PrefetchKey navigated_key,
      PrefetchServiceWorkerState expected_service_worker_state,
      bool is_nav_prerender,
      base::WeakPtr<PrerenderHost> prerender_host,
      Callback callback,
      perfetto::Flow flow);

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
  void FindPrefetchInternal2(PrefetchService& prefetch_service,
                             base::WeakPtr<PrefetchServingPageMetricsContainer>
                                 serving_page_metrics_container);
  // Each candidate `PrefetchContainer` proceeds to
  //
  //    `RegisterCandidate()` (required)
  // -> `StartWaitFor()` (optional, if servable state is
  //    `kShouldBlockUntilHead`)
  // -> `UnregisterCandidate()` (required)
  void RegisterCandidate(PrefetchContainer& prefetch_container);
  void StartWaitFor(const PrefetchKey& prefetch_key,
                    PrefetchServableState servable_state);
  void UnregisterCandidate(
      const PrefetchKey& prefetch_key,
      bool is_served,
      PrefetchPotentialCandidateServingResult serving_result);
  void OnTimeout(PrefetchKey prefetch_key);
  void UnblockForMatch(const PrefetchKey& prefetch_key);
  void UnblockForNoCandidates();
  // Unregisters unmatched prefetch and unblocks if there are no other waiting
  // prefetches.
  void MaybeUnblockForUnmatch(
      const PrefetchContainer& prefetch_container,
      PrefetchPotentialCandidateServingResult serving_result);
  void UnblockForCookiesChanged(const PrefetchKey& key);
  void UnblockInternal(PrefetchServingHandle serving_handle);

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

  // `NavigationRequest` associated to `this`.
  //
  // It is used only for metrics purpose.
  base::WeakPtr<NavigationRequest> navigation_request_for_metrics_;
  base::WeakPtr<PrefetchService> prefetch_service_;

  // The key representing a navigation to try match.
  const PrefetchKey navigated_key_;
  const PrefetchServiceWorkerState expected_service_worker_state_;
  // Callback that is called at the match end.
  Callback callback_;
  perfetto::Flow flow_;
  // Is the `NavigationHandle` for initial navigation of prerender or not.
  const bool is_nav_prerender_;
  // And its `PrerenderHost`.
  //
  // When `this` is for a prerender initial navigation, then
  // `prerender_host_for_metrics_` is the `PrerenderHost` of the prerender
  // initial navigation. Otherwise, `nullptr`. Also this is nullptr if
  // `PreloadServingMetricsCapsule::IsFeatureEnabled()` is false.
  base::WeakPtr<PrerenderHost> prerender_host_for_metrics_;
  std::unique_ptr<PrefetchMatchMetrics> prefetch_match_metrics_;

  // Potentially matching candidates.
  //
  // Removed if it is determined actually matching or not.
  //
  // The count is non-increasing, greater than or equal to zero.
  std::map<PrefetchKey, std::unique_ptr<CandidateData>> candidates_;

  std::optional<base::TimeTicks> wait_started_at_ = std::nullopt;

  // Optional. It is set if the navigation is prerender initial navigation and
  // the initial candidates contain a prefetch ahead of prerender that shares
  // `PreloadPipelineInfo`.
  base::WeakPtr<PrefetchContainer> prefetch_ahead_of_prerender_for_metrics_ =
      nullptr;
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
    const std::map<PrefetchKey, std::unique_ptr<T>>& prefetches,
    const PrefetchKey& navigated_key) {
  std::vector<T*> result;

  // Note that exact match one is at the head if exists by the property of
  // `IterateCandidates()`.
  no_vary_search::IterateCandidates(
      navigated_key, prefetches,
      base::BindRepeating(
          [](const PrefetchKey& navigated_key, std::vector<T*>* result,
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
                          PrefetchServableState servable_state,
                          bool is_nav_prerender) {
  switch (servable_state) {
    case PrefetchServableState::kNotServable:
      DVLOG(1) << "CollectMatchCandidatesGeneric: skipped because not "
                  "servable: candidate = "
               << candidate;
      return false;
    case PrefetchServableState::kShouldBlockUntilEligibilityGot:
    case PrefetchServableState::kShouldBlockUntilHeadReceived:
    case PrefetchServableState::kServable:
      break;
  }

  switch (servable_state) {
    case PrefetchServableState::kShouldBlockUntilEligibilityGot:
      if (!is_nav_prerender) {
        DVLOG(1)
            << "CollectMatchCandidatesGeneric: skipped because it's checking "
               "eligibility and the navigation is not a prerender: candidate = "
            << candidate;
        return false;
      }
      break;
    case PrefetchServableState::kServable:
    case PrefetchServableState::kNotServable:
    case PrefetchServableState::kShouldBlockUntilHeadReceived:
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
std::pair<std::vector<T*>, base::flat_map<PrefetchKey, PrefetchServableState>>
CollectMatchCandidatesGeneric(
    const std::map<PrefetchKey, std::unique_ptr<T>>& prefetches,
    const PrefetchKey& navigated_key,
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
  base::flat_map<PrefetchKey, PrefetchServableState> servable_states;
  for (T* candidate : candidates) {
    PrefetchServableState servable_state =
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
