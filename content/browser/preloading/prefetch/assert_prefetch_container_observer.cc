// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/assert_prefetch_container_observer.h"

#include "base/no_destructor.h"
#include "base/state_transitions.h"

namespace content {

AssertPrefetchContainerObserver::AssertPrefetchContainerObserver(
    PrefetchContainer& prefetch_container)
    : prefetch_container_(prefetch_container.GetWeakPtr()) {
  CHECK_EQ(prefetch_container_->GetLoadState(),
           PrefetchContainerLoadState::kNotStarted);
  prefetch_container_->AddObserver(this);
}

AssertPrefetchContainerObserver::~AssertPrefetchContainerObserver() {
  // Currently `AssertPrefetchContainerObserver` is owned by `PrefetchContainer`
  // and destroyed after `OnWillBeDestroyed()`.
  DUMP_WILL_BE_CHECK(on_will_be_destroyed_called_);
  prefetch_container_->RemoveObserver(this);
}

void AssertPrefetchContainerObserver::UpdateObservedLoadState() {
  using T = PrefetchContainerLoadState;
  // Currently `PrefetchContainer::Observer` doesn't have corresponding
  // notifications when transitioning to `kStarted` or `kFailedHeldback`.
  // Therefore `observed_load_state_` transitions directly from `kEligible` to
  // e.g. `kDeterminedHead`, skipping `kStarted`.
  static const base::NoDestructor<base::StateTransitions<T>> transitions(
      base::StateTransitions<T>({
          {T::kNotStarted, {T::kEligible, T::kFailedIneligible}},
          {T::kEligible, {T::kDeterminedHead, T::kFailedDeterminedHead}},
          {T::kFailedIneligible, {}},
          {T::kFailedHeldback, {}},
          {T::kStarted, {}},
          {T::kDeterminedHead, {T::kCompleted, T::kFailed}},
          {T::kFailedDeterminedHead, {T::kFailed}},
          {T::kCompleted, {}},
          {T::kFailed, {}},
      }));
  CHECK_STATE_TRANSITION(transitions, observed_load_state_,
                         prefetch_container_->GetLoadState());

  observed_load_state_ = prefetch_container_->GetLoadState();
}

void AssertPrefetchContainerObserver::OnWillBeDestroyed(
    const PrefetchContainer& prefetch_container) {
  DUMP_WILL_BE_CHECK_EQ(&prefetch_container, prefetch_container_.get());
  DUMP_WILL_BE_CHECK(!on_will_be_destroyed_called_);
  on_will_be_destroyed_called_ = true;

  if (observed_load_state_ != prefetch_container_->GetLoadState()) {
    // Transitions allowed without `PrefetchContainer::Observer` notifications,
    // to capture missing observer calls.
    using T = PrefetchContainerLoadState;
    static const base::NoDestructor<base::StateTransitions<T>> transitions(
        base::StateTransitions<T>({
            {T::kEligible, {T::kFailedHeldback, T::kStarted}},
        }));
    CHECK_STATE_TRANSITION(transitions, observed_load_state_,
                           prefetch_container_->GetLoadState());
  }
}

void AssertPrefetchContainerObserver::OnGotInitialEligibility(
    const PrefetchContainer& prefetch_container,
    PreloadingEligibility eligibility) {
  DUMP_WILL_BE_CHECK_EQ(&prefetch_container, prefetch_container_.get());
  DUMP_WILL_BE_CHECK(!on_will_be_destroyed_called_);
  DUMP_WILL_BE_CHECK(!prefetch_container_->is_in_dtor());
  DUMP_WILL_BE_CHECK(prefetch_container_->GetLoadState() ==
                         PrefetchContainerLoadState::kEligible ||
                     prefetch_container_->GetLoadState() ==
                         PrefetchContainerLoadState::kFailedIneligible);
  UpdateObservedLoadState();
}

void AssertPrefetchContainerObserver::OnDeterminedHead(
    const PrefetchContainer& prefetch_container) {
  DUMP_WILL_BE_CHECK_EQ(&prefetch_container, prefetch_container_.get());
  DUMP_WILL_BE_CHECK(!on_will_be_destroyed_called_);
  DUMP_WILL_BE_CHECK(!prefetch_container_->is_in_dtor());
  DUMP_WILL_BE_CHECK(prefetch_container_->GetLoadState() ==
                         PrefetchContainerLoadState::kDeterminedHead ||
                     prefetch_container_->GetLoadState() ==
                         PrefetchContainerLoadState::kFailedDeterminedHead);
  UpdateObservedLoadState();
}

void AssertPrefetchContainerObserver::OnPrefetchCompletedOrFailed(
    const PrefetchContainer& prefetch_container,
    const network::URLLoaderCompletionStatus& completion_status,
    const std::optional<int>& response_code) {
  DUMP_WILL_BE_CHECK_EQ(&prefetch_container, prefetch_container_.get());
  DUMP_WILL_BE_CHECK(!on_will_be_destroyed_called_);
  DUMP_WILL_BE_CHECK(!prefetch_container_->is_in_dtor());
  DUMP_WILL_BE_CHECK(prefetch_container_->GetLoadState() ==
                         PrefetchContainerLoadState::kCompleted ||
                     prefetch_container_->GetLoadState() ==
                         PrefetchContainerLoadState::kFailed);
  UpdateObservedLoadState();
}

}  // namespace content
