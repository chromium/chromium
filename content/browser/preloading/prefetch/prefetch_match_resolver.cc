// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_match_resolver.h"

#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/ranges/algorithm.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
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

PrefetchMatchResolver::PrefetchMatchResolver(
    NavigationHandle& navigation_handle) {}

PrefetchMatchResolver::~PrefetchMatchResolver() = default;

base::WeakPtr<PrefetchMatchResolver> PrefetchMatchResolver::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PrefetchMatchResolver::SetOnPrefetchToServeReadyCallback(
    PrefetchMatchResolver::OnPrefetchToServeReady on_prefetch_to_serve_ready) {
  on_prefetch_to_serve_ready_callback_ = std::move(on_prefetch_to_serve_ready);
  DVLOG(1) << *this
           << "::SetCallback:" << &on_prefetch_to_serve_ready_callback_;
}

PrefetchMatchResolver::OnPrefetchToServeReady
PrefetchMatchResolver::ReleaseOnPrefetchToServeReadyCallback() {
  DVLOG(1) << *this
           << "::ReleaseCallback:" << &on_prefetch_to_serve_ready_callback_;
  CHECK(on_prefetch_to_serve_ready_callback_);
  return std::move(on_prefetch_to_serve_ready_callback_);
}

void PrefetchMatchResolver::PrefetchServed(PrefetchContainer::Reader reader) {
  ReleaseOnPrefetchToServeReadyCallback().Run(std::move(reader));
}

void PrefetchMatchResolver::PrefetchNotAvailable() {
  DVLOG(1) << *this << "::PrefetchNotAvailable";
  ReleaseOnPrefetchToServeReadyCallback().Run({});
}

void PrefetchMatchResolver::PrefetchNotUsable(
    const PrefetchContainer& prefetch_container) {
  DVLOG(1) << *this << "::PrefetchNotUsable:" << prefetch_container.GetURL();
  CHECK(!in_progress_prefetch_matches_.contains(prefetch_container.GetURL()));
  MaybeFallbackToRegularNavigationWhenPrefetchNotUsable();
}

void PrefetchMatchResolver::PrefetchNotUsable(const GURL& prefetch_url) {
  DVLOG(1) << *this << "::PrefetchNotUsable: " << prefetch_url;
  CHECK(!in_progress_prefetch_matches_.contains(prefetch_url));
  MaybeFallbackToRegularNavigationWhenPrefetchNotUsable();
}

void PrefetchMatchResolver::WaitForPrefetch(
    PrefetchContainer& prefetch_container) {
  DVLOG(1) << *this << "::WaitForPrefetch: " << prefetch_container.GetURL();
  in_progress_prefetch_matches_[prefetch_container.GetURL()] =
      prefetch_container.GetWeakPtr();
}

void PrefetchMatchResolver::EndWaitForPrefetch(const GURL& prefetch_url) {
  CHECK(in_progress_prefetch_matches_.count(prefetch_url));
  in_progress_prefetch_matches_.erase(prefetch_url);
}

bool PrefetchMatchResolver::IsWaitingForPrefetch(
    const PrefetchContainer& prefetch_container) const {
  DVLOG(1) << *this
           << "::IsWaitingForPrefetchP: " << prefetch_container.GetURL();
  return IsWaitingForPrefetch(prefetch_container.GetURL());
}

bool PrefetchMatchResolver::IsWaitingForPrefetch(
    const GURL& prefetch_url) const {
  DVLOG(1) << *this << "::IsWaitingForPrefetchU: " << prefetch_url;
  return in_progress_prefetch_matches_.count(prefetch_url);
}

void PrefetchMatchResolver::
    MaybeFallbackToRegularNavigationWhenPrefetchNotUsable() {
  DVLOG(1) << *this
           << "::MaybeFallbackToRegularNavigationWhenPrefetchNotUsable";
  if (IsWaitingOnPrefetchHead()) {
    return;
  }
  // We are not waiting on any more prefetches in progress. Resolve to no
  // prefetch available.
  PrefetchNotAvailable();
}

void PrefetchMatchResolver::
    FallbackToRegularNavigationWhenMatchedPrefetchCookiesChanged(
        PrefetchContainer& prefetch_container,
        const GURL& navigated_url) {
  // The prefetch_container has already received its head.
  CHECK(!IsWaitingForPrefetch(prefetch_container));
  prefetch_container.OnDetectedCookiesChange();
  prefetch_container.OnReturnPrefetchToServe(/*served=*/false, navigated_url);
  DVLOG(1) << *this
           << "::FallbackToRegularNavigationWhenMatchedPrefetchCookiesChanged:"
           << prefetch_container << " not served because Cookies changed.";

  // Do the same for other prefetches in `in_progress_prefetch_matches_`.
  for (auto& [prefetch_url, weak_prefetch_container] :
       in_progress_prefetch_matches_) {
    if (!weak_prefetch_container) {
      continue;
    }
    weak_prefetch_container->OnDetectedCookiesChange();
    weak_prefetch_container->OnReturnPrefetchToServe(/*served=*/false,
                                                     navigated_url);
    DVLOG(1)
        << *this
        << "::FallbackToRegularNavigationWhenMatchedPrefetchCookiesChanged:"
        << *weak_prefetch_container << " not served because Cookies changed.";
  }

  // Remove all of the prefetches from `in_progress_prefetch_matches_` and let
  // the browser know to fallback to regular navigation instead.
  in_progress_prefetch_matches_.clear();
  PrefetchNotAvailable();
}

bool PrefetchMatchResolver::IsWaitingOnPrefetchHead() const {
  DVLOG(1) << *this << "::IsWaitingOnPrefetchHead";
  return !in_progress_prefetch_matches_.empty();
}

CONTENT_EXPORT std::ostream& operator<<(
    std::ostream& ostream,
    const PrefetchMatchResolver& prefetch_match_resolver) {
  return ostream << "PrefetchMatchResolver[" << &prefetch_match_resolver
                 << ", waiting_on = "
                 << prefetch_match_resolver.in_progress_prefetch_matches_.size()
                 << " ]";
}

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(PrefetchMatchResolver);

PrefetchMatchResolver2::CandidateData::CandidateData() = default;
PrefetchMatchResolver2::CandidateData::~CandidateData() = default;

PrefetchMatchResolver2::PrefetchMatchResolver2(
    PrefetchContainer::Key navigated_key,
    base::WeakPtr<PrefetchService> prefetch_service,
    Callback callback)
    : navigated_key_(std::move(navigated_key)),
      prefetch_service_(std::move(prefetch_service)),
      callback_(std::move(callback)) {}

PrefetchMatchResolver2::~PrefetchMatchResolver2() = default;

std::optional<base::TimeDelta> PrefetchMatchResolver2::GetBlockedDuration()
    const {
  if (wait_started_at_.has_value()) {
    return base::TimeTicks::Now() - wait_started_at_.value();
  } else {
    return std::nullopt;
  }
}

// static
void PrefetchMatchResolver2::FindPrefetch(
    PrefetchContainer::Key navigated_key,
    PrefetchService& prefetch_service,
    base::WeakPtr<PrefetchServingPageMetricsContainer>
        serving_page_metrics_container,
    Callback callback) {
  // See the comment of `self_`.
  auto prefetch_match_resolver = base::WrapUnique(new PrefetchMatchResolver2(
      std::move(navigated_key), prefetch_service.GetWeakPtr(),
      std::move(callback)));
  PrefetchMatchResolver2& ref = *prefetch_match_resolver.get();
  ref.self_ = std::move(prefetch_match_resolver);

  ref.FindPrefetchInternal(prefetch_service,
                           std::move(serving_page_metrics_container));
}

void PrefetchMatchResolver2::FindPrefetchInternal(
    PrefetchService& prefetch_service,
    base::WeakPtr<PrefetchServingPageMetricsContainer>
        serving_page_metrics_container) {
  auto [candidates, servable_states] = prefetch_service.CollectMatchCandidates(
      navigated_key_, std::move(serving_page_metrics_container));
  // Consume `candidates`.
  for (auto& prefetch_container : candidates) {
    RegisterCandidate(*prefetch_container);
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
          UnblockForCookiesChanged();
          return;
        }
        break;
      case PrefetchContainer::ServableState::kNotServable:
        NOTREACHED_NORETURN();
      case PrefetchContainer::ServableState::kShouldBlockUntilHeadReceived:
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
        NOTREACHED_NORETURN();
      case PrefetchContainer::ServableState::kShouldBlockUntilHeadReceived:
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

void PrefetchMatchResolver2::RegisterCandidate(
    PrefetchContainer& prefetch_container) {
  auto candidate_data = std::make_unique<CandidateData>();
  // #prefetch-key-availability
  //
  // Note that `CHECK(candidates_.contains(prefetch_key))` and
  // `CHECK(candidate_data->prefetch_container)` below always hold because
  // `PrefetchMatchResolver2` observes lifecycle events of `PrefetchContainer`.
  candidate_data->prefetch_container = prefetch_container.GetWeakPtr();
  candidate_data->timeout_timer = nullptr;

  candidates_[prefetch_container.key()] = std::move(candidate_data);
}

void PrefetchMatchResolver2::StartWaitFor(
    const PrefetchContainer::Key& prefetch_key,
    PrefetchContainer::ServableState servable_state) {
  // By #prefetch-key-availability
  CHECK(candidates_.contains(prefetch_key));
  auto& candidate_data = candidates_[prefetch_key];
  CHECK(candidate_data->prefetch_container);
  PrefetchContainer& prefetch_container = *candidate_data->prefetch_container;

  CHECK_EQ(servable_state,
           PrefetchContainer::ServableState::kShouldBlockUntilHeadReceived);
  // `kServable` -> `kNotServable` is the only possible change during
  // `FindPrefetchInternal()` call. If `servable_state` is
  // `kShouldBlockUntilHeadReceived`, `GetServableState()` is the same.
  CHECK_EQ(prefetch_container.GetServableState(PrefetchCacheableDuration()),
           PrefetchContainer::ServableState::kShouldBlockUntilHeadReceived);
  CHECK(!candidate_data->timeout_timer);

  // TODO(crbug.com/356552413): Merge
  // https://chromium-review.googlesource.com/c/chromium/src/+/5668924 and write
  // tests.
  base::TimeDelta timeout =
      PrefetchBlockUntilHeadTimeout(prefetch_container.GetPrefetchType());
  if (timeout.is_positive()) {
    candidate_data->timeout_timer = std::make_unique<base::OneShotTimer>();
    candidate_data->timeout_timer->Start(
        FROM_HERE, timeout,
        base::BindOnce(&PrefetchMatchResolver2::OnTimeout,
                       // Safety: `timeout_timer` is owned by this.
                       Unretained(this), prefetch_key));
  }

  prefetch_container.AddObserver(this);
}

void PrefetchMatchResolver2::UnregisterCandidate(
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

void PrefetchMatchResolver2::OnWillBeDestroyed(
    PrefetchContainer& prefetch_container) {
  MaybeUnblockForUnmatch(prefetch_container.key());
}

void PrefetchMatchResolver2::OnDeterminedHead(
    PrefetchContainer& prefetch_container) {
  CHECK(candidates_.contains(prefetch_container.key()));
  CHECK(!prefetch_container.is_in_dtor());

  if (prefetch_container.CreateReader().HaveDefaultContextCookiesChanged()) {
    UnblockForCookiesChanged();
    return;
  }

  switch (prefetch_container.GetServableState(PrefetchCacheableDuration())) {
    // `kShouldBlockUntilHeadReceived` case occurs if a prefetch is redirected
    // and the redirect is not eligible.
    //
    //    PrefetchService::OnGotEligibilityForRedirect()
    // -> PrefetchStreamingURLLoader::HandleRedirect(kFail)
    // -> PrefetchContainer::OnDeterminedHead2()
    // -> here
    case PrefetchContainer::ServableState::kShouldBlockUntilHeadReceived:
    case PrefetchContainer::ServableState::kNotServable:
      MaybeUnblockForUnmatch(prefetch_container.key());
      return;
    case PrefetchContainer::ServableState::kServable:
      // Proceed.
      break;
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

void PrefetchMatchResolver2::OnTimeout(PrefetchContainer::Key prefetch_key) {
  // `timeout_timer` is alive, which implies `candidate` is alive.
  CHECK(candidates_.contains(prefetch_key));
  auto& candidate_data = candidates_[prefetch_key];
  CHECK(candidate_data->prefetch_container);

  MaybeUnblockForUnmatch(prefetch_key);
}

void PrefetchMatchResolver2::UnblockForMatch(
    const PrefetchContainer::Key& prefetch_key) {
  // By #prefetch-key-availability
  CHECK(candidates_.contains(prefetch_key));
  auto& candidate_data = candidates_[prefetch_key];
  CHECK(candidate_data->prefetch_container);
  PrefetchContainer& prefetch_container = *candidate_data->prefetch_container;

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

void PrefetchMatchResolver2::UnblockForNoCandidates() {
  UnblockInternal({});
}

void PrefetchMatchResolver2::MaybeUnblockForUnmatch(
    const PrefetchContainer::Key& prefetch_key) {
  UnregisterCandidate(prefetch_key, /*is_served=*/false);

  if (candidates_.size() == 0) {
    UnblockForNoCandidates();
  }

  // It still waits for other `PrefetchContainer`s.
}

void PrefetchMatchResolver2::UnblockForCookiesChanged() {
  // Unregister remaining candidates as not served, with calling
  // `PrefetchContainer::OnDetectedCookiesChange2()`.
  for (auto& prefetch_key : Keys(candidates_)) {
    // By #prefetch-key-availability
    CHECK(candidates_.contains(prefetch_key));
    auto& candidate_data = candidates_[prefetch_key];
    CHECK(candidate_data->prefetch_container);
    PrefetchContainer& prefetch_container = *candidate_data->prefetch_container;

    UnregisterCandidate(prefetch_key, /*is_served=*/false);

    prefetch_container.OnDetectedCookiesChange2();
  }

  UnblockForNoCandidates();
}

void PrefetchMatchResolver2::UnblockInternal(PrefetchContainer::Reader reader) {
  // Postcondition: This resolver waits for no `PrefetchContainer`s when it has
  // been unblocking.
  CHECK_EQ(candidates_.size(), 0u);

  auto callback = std::move(callback_);

  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                             std::move(self_));

  std::move(callback).Run(std::move(reader));
}

}  // namespace content
