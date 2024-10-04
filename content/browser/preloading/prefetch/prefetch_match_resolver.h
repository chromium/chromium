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

// TODO(crbug.com/40274818): Problem: how do we inform this class of prefetches
// being started while we are waiting for existing in-progress prefetches ?
// PrefetchService should probably do it.
class CONTENT_EXPORT PrefetchMatchResolver
    : public NavigationHandleUserData<PrefetchMatchResolver> {
 public:
  PrefetchMatchResolver(const PrefetchMatchResolver&) = delete;
  PrefetchMatchResolver& operator=(const PrefetchMatchResolver&) = delete;
  ~PrefetchMatchResolver() override;

  base::WeakPtr<PrefetchMatchResolver> GetWeakPtr();

  using OnPrefetchToServeReady =
      base::OnceCallback<void(PrefetchContainer::Reader prefetch_to_serve)>;
  void SetOnPrefetchToServeReadyCallback(
      OnPrefetchToServeReady on_prefetch_to_serve_ready);

  // A prefetch can be served, so let the browser know that it can use the
  // prefetch for the navigation.
  void PrefetchServed(PrefetchContainer::Reader reader);
  // The prefetch container / prefetch_url cannot be used. If there are no
  // more potential prefetches to wait for, let the browser know to fallback
  // to normal navigation.
  void PrefetchNotUsable(const PrefetchContainer& prefetch_container);
  void PrefetchNotUsable(const GURL& prefetch_url);
  // A prefetch is not available so let the browser know to fallback to regular
  // navigation instead.
  void PrefetchNotAvailable();
  // If Cookies have changed, then none of the matched prefetches can be served.
  // Remove all of the prefetches from `in_progress_prefetch_matches_` and let
  // the browser know to fallback to regular navigation instead.
  void FallbackToRegularNavigationWhenMatchedPrefetchCookiesChanged(
      PrefetchContainer& prefetch_container,
      const GURL& navigated_url);
  void WaitForPrefetch(PrefetchContainer& prefetch_container);
  void EndWaitForPrefetch(const GURL& prefetch_url);
  // Check if we are waiting already for the head of this `prefetch_container`.
  bool IsWaitingForPrefetch(const PrefetchContainer& prefetch_container) const;
  bool IsWaitingForPrefetch(const GURL& prefetch_url) const;

 private:
  friend NavigationHandleUserData<PrefetchMatchResolver>;
  explicit PrefetchMatchResolver(NavigationHandle& navigation_handle);

  void MaybeFallbackToRegularNavigationWhenPrefetchNotUsable();
  bool IsWaitingOnPrefetchHead() const;

  OnPrefetchToServeReady ReleaseOnPrefetchToServeReadyCallback();

  // Once the prefetch (if any) that can be used to serve a navigation to
  // |url| is identified, this callback is called with that
  // prefetch.
  OnPrefetchToServeReady on_prefetch_to_serve_ready_callback_;

  // Keep track of all prefetches that we are waiting for head on.
  std::map<GURL, base::WeakPtr<PrefetchContainer>>
      in_progress_prefetch_matches_;

  base::WeakPtrFactory<PrefetchMatchResolver> weak_ptr_factory_{this};

  // For debug logs.
  CONTENT_EXPORT friend std::ostream& operator<<(
      std::ostream& ostream,
      const PrefetchMatchResolver& prefetch_match_resolver);
  friend NavigationHandleUserData;
  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

// Manages matching process of prefetch
// https://wicg.github.io/nav-speculation/prefetch.html#wait-for-a-matching-prefetch-record
//
// This class is created per call of
// `PrefetchURLLoaderInterceptor::MaybeCreateLoader()` except redirects for
// already matched prefetch and still servable ones, i.e. a prefetch was matched
// by prior call of `PrefetchMatchResolver2::FindPrefetch()`.
//
// Lifetime of this class is from the call of `FindPrefetch()` to calling
// `callback_`. This is owned by itself. See the comment on `self_`.
//
// Note about "2": This is the new implementation of the matching process
// of prefetch that is used when `UseNewWaitLoop()` returns true. The old
// implementation is `PrefetchMatchResolver`, so this is named "2".
// Differences are, for example:
//
// - `PrefetchMatchResolver2` has strict precondition/postcondition
//   e.g. `CHECK_EQ(candidates_.size(), 0u);` when the matching process
//   starts/ends.
// - `PrefetchMatchResolver` is `NavigationHandleUserData`
//   and can be used multiple times for redirects, while
//   `PrefetchMatchResolver2` forbids it in architecture level.
//
// That's the reason why we decided to implement the separate class.
//
// TODO(crbug.com/353490734): Remove the above `Note about "2"`.
class CONTENT_EXPORT PrefetchMatchResolver2 final
    : public PrefetchContainer::Observer {
 public:
  using Callback = base::OnceCallback<void(PrefetchContainer::Reader reader)>;

  ~PrefetchMatchResolver2() override;

  // Not movable nor copyable.
  PrefetchMatchResolver2(PrefetchMatchResolver2&& other) = delete;
  PrefetchMatchResolver2& operator=(PrefetchMatchResolver2&& other) = delete;
  PrefetchMatchResolver2(const PrefetchMatchResolver2&) = delete;
  PrefetchMatchResolver2& operator=(const PrefetchMatchResolver2&) = delete;

  // PrefetchContainer::Observer implementation
  void OnWillBeDestroyed(PrefetchContainer& prefetch_container) override;
  void OnDeterminedHead(PrefetchContainer& prefetch_container) override;

  // Finds prefetch that matches to a navigation and is servable.
  //
  // Corresponds to
  // https://wicg.github.io/nav-speculation/prefetch.html#wait-for-a-matching-prefetch-record
  //
  // This method is async. `callback` will be called when it is done.
  // `bool(reader)` is true iff a matching servable prefetch is found.
  static void FindPrefetch(PrefetchContainer::Key navigated_key,
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

  explicit PrefetchMatchResolver2(PrefetchContainer::Key navigated_key,
                                  base::WeakPtr<PrefetchService>,
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
  void UnblockForCookiesChanged();
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
  std::unique_ptr<PrefetchMatchResolver2> self_;

  const PrefetchContainer::Key navigated_key_;
  base::WeakPtr<PrefetchService> prefetch_service_;
  Callback callback_;
  std::map<PrefetchContainer::Key, std::unique_ptr<CandidateData>> candidates_;
  std::optional<base::TimeTicks> wait_started_at_ = std::nullopt;
};

// Abstracts required operations for `PrefetchContainer` that is used to collect
// match candidates in the first phase of
// `PrefetchMatchResolver2::FindPrefetch()`. Used for unit testing.
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
                          PrefetchContainer::ServableState servable_state) {
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
    case PrefetchContainer::ServableState::kServable:
    case PrefetchContainer::ServableState::kShouldBlockUntilHeadReceived:
      break;
  }

  if (candidate.IsDecoy()) {
    DVLOG(1) << "CollectMatchCandidatesGeneric: skipped because prefetch is a "
                "decoy: candidate = "
             << candidate;
    return false;
  }

  // Note: This codepath is only be reached in practice if we create a
  // second NavigationRequest to this prefetch's URL. The first
  // NavigationRequest would call GetPrefetch, which might set this
  // PrefetchContainer's status to kPrefetchNotUsedCookiesChanged.
  CHECK(candidate.HasPrefetchStatus());
  if (candidate.GetPrefetchStatus() ==
      PrefetchStatus::kPrefetchNotUsedCookiesChanged) {
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
// `PrefetchMatchResolver2::FindPrefetch()` with mock `PrefetchContainer`.
template <class T>
  requires MatchCandidate<T>
std::pair<
    std::vector<T*>,
    base::flat_map<PrefetchContainer::Key, PrefetchContainer::ServableState>>
CollectMatchCandidatesGeneric(
    const std::map<PrefetchContainer::Key, std::unique_ptr<T>>& prefetches,
    const PrefetchContainer::Key& navigated_key,
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
    if (IsCandidateAvailable(*candidate, servable_state)) {
      candidates_available.push_back(candidate);
      servable_states.emplace(candidate->key(), servable_state);
    }
  }

  return std::make_pair(std::move(candidates_available),
                        std::move(servable_states));
}

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_MATCH_RESOLVER_H_
