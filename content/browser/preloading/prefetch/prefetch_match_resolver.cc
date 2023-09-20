// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "content/browser/preloading/prefetch/prefetch_match_resolver.h"

#include "base/memory/weak_ptr.h"
#include "base/ranges/algorithm.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/public/browser/navigation_handle_user_data.h"

namespace content {

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
        PrefetchContainer& prefetch_container) {
  // The prefetch_container has already received its head.
  CHECK(!IsWaitingForPrefetch(prefetch_container));
  prefetch_container.SetPrefetchStatus(
      PrefetchStatus::kPrefetchNotUsedCookiesChanged);
  prefetch_container.UpdateServingPageMetrics();
  prefetch_container.OnReturnPrefetchToServe(/*served=*/false);
  prefetch_container.CancelStreamingURLLoaderIfNotServing();
  DVLOG(1) << *this
           << "::FallbackToRegularNavigationWhenMatchedPrefetchCookiesChanged:"
           << prefetch_container << " not served because Cookies changed.";

  // Do the same for other prefetches in `in_progress_prefetch_matches_`.
  for (auto& [prefetch_url, weak_prefetch_container] :
       in_progress_prefetch_matches_) {
    if (!weak_prefetch_container) {
      continue;
    }
    weak_prefetch_container->SetPrefetchStatus(
        PrefetchStatus::kPrefetchNotUsedCookiesChanged);
    weak_prefetch_container->UpdateServingPageMetrics();
    weak_prefetch_container->OnReturnPrefetchToServe(/*served=*/false);
    weak_prefetch_container->CancelStreamingURLLoaderIfNotServing();
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

}  // namespace content
