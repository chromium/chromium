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

bool PrefetchMatchResolver::HasExactPrefetchMatch() const {
  return !!exact_prefetch_match_;
}

void PrefetchMatchResolver::SetExactPrefetchMatch(PrefetchContainer& prefetch) {
  CHECK(!exact_prefetch_match_ ||
        exact_prefetch_match_->GetURL() == prefetch.GetURL());
  exact_prefetch_match_ = prefetch.GetWeakPtr();
}

PrefetchContainer* PrefetchMatchResolver::GetExactPrefetchMatch() const {
  return exact_prefetch_match_.get();
}

bool PrefetchMatchResolver::HasInexactPrefetchMatch() const {
  return base::ranges::any_of(inexact_prefetch_matches_,
                              [](const auto& weak_prefetch_container) {
                                return !!weak_prefetch_container;
                              });
}
void PrefetchMatchResolver::AddInexactPrefetchMatch(
    PrefetchContainer& prefetch) {
  inexact_prefetch_matches_.push_back(prefetch.GetWeakPtr());
}

std::vector<PrefetchContainer*>
PrefetchMatchResolver::GetInexactPrefetchMatches() const {
  std::vector<PrefetchContainer*> inexact_prefetch_matches;
  for (const auto& weak_prefetch_container : inexact_prefetch_matches_) {
    if (weak_prefetch_container) {
      inexact_prefetch_matches.push_back(weak_prefetch_container.get());
    }
  }
  return inexact_prefetch_matches;
}

base::WeakPtr<PrefetchMatchResolver> PrefetchMatchResolver::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PrefetchMatchResolver::SetOnPrefetchToServeReadyCallback(
    PrefetchMatchResolver::OnPrefetchToServeReady on_prefetch_to_serve_ready) {
  on_prefetch_to_serve_ready_callback_ = std::move(on_prefetch_to_serve_ready);
}
PrefetchMatchResolver::OnPrefetchToServeReady
PrefetchMatchResolver::ReleaseOnPrefetchToServeReadyCallback() {
  return std::move(on_prefetch_to_serve_ready_callback_);
}

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(PrefetchMatchResolver);

}  // namespace content
