// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_servable_state.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_match_resolver.h"

namespace content {

CONTENT_EXPORT std::ostream& operator<<(std::ostream& ostream,
                                        PrefetchServableState servable_state) {
  switch (servable_state) {
    case PrefetchServableState::kNotServable:
      return ostream << "NotServable";
    case PrefetchServableState::kServable:
      return ostream << "Servable";
    case PrefetchServableState::kShouldBlockUntilHeadReceived:
      return ostream << "ShouldBlockUntilHeadReceived";
    case PrefetchServableState::kShouldBlockUntilEligibilityGot:
      return ostream << "ShouldBlockUntilEligibilityGot";
  }
}

PrefetchMatchResolverAction::PrefetchMatchResolverAction(
    ActionKind kind,
    PrefetchContainer::LoadState prefetch_container_load_state,
    std::optional<bool> is_expired)
    : kind_(kind),
      prefetch_container_load_state_(prefetch_container_load_state),
      is_expired_(is_expired) {
  CHECK_EQ(is_expired.has_value(), kind == ActionKind::kMaybeServe);
}

PrefetchMatchResolverAction::~PrefetchMatchResolverAction() = default;

PrefetchServableState PrefetchMatchResolverAction::ToServableState() const {
  switch (kind_) {
    case ActionKind::kDrop:
      return PrefetchServableState::kNotServable;
    case ActionKind::kWait:
      switch (prefetch_container_load_state_) {
        case PrefetchContainer::LoadState::kNotStarted:
        case PrefetchContainer::LoadState::kEligible:
          return PrefetchServableState::kShouldBlockUntilEligibilityGot;
        case PrefetchContainer::LoadState::kStarted:
          return PrefetchServableState::kShouldBlockUntilHeadReceived;
        case PrefetchContainer::LoadState::kDeterminedHead:
        case PrefetchContainer::LoadState::kCompleted:
        case PrefetchContainer::LoadState::kFailedHeldback:
        case PrefetchContainer::LoadState::kFailedIneligible:
        case PrefetchContainer::LoadState::kFailedDeterminedHead:
        case PrefetchContainer::LoadState::kFailed:
          NOTREACHED();
      }
    case ActionKind::kMaybeServe:
      if (is_expired_.value()) {
        return PrefetchServableState::kNotServable;
      } else {
        return PrefetchServableState::kServable;
      }
  }
}

int GetCodeOfPrefetchServableStateAndPrefetchMatchResolverActionForDebug(
    PrefetchServableState servable_state,
    const PrefetchMatchResolverAction& match_resolver_action) {
  // The integers below are only for tentative metrics, and are introduced to
  // decouple the metrics values from the underlying enum values.

  int servable_state_int;
  switch (servable_state) {
    case PrefetchServableState::kShouldBlockUntilEligibilityGot:
      servable_state_int = 1;
      break;
    case PrefetchServableState::kShouldBlockUntilHeadReceived:
      servable_state_int = 2;
      break;
    case PrefetchServableState::kServable:
      servable_state_int = 3;
      break;
    case PrefetchServableState::kNotServable:
      servable_state_int = 4;
      break;
  }

  int action_kind_int;
  switch (match_resolver_action.kind()) {
    case PrefetchMatchResolverAction::ActionKind::kDrop:
      action_kind_int = 1;
      break;
    case PrefetchMatchResolverAction::ActionKind::kWait:
      action_kind_int = 2;
      break;
    case PrefetchMatchResolverAction::ActionKind::kMaybeServe:
      action_kind_int = 3;
      break;
  }

  int action_prefetch_container_load_state_int;
  switch (match_resolver_action.prefetch_container_load_state()) {
    case PrefetchContainer::LoadState::kNotStarted:
      action_prefetch_container_load_state_int = 1;
      break;
    case PrefetchContainer::LoadState::kEligible:
      action_prefetch_container_load_state_int = 2;
      break;
    case PrefetchContainer::LoadState::kStarted:
      action_prefetch_container_load_state_int = 3;
      break;
    case PrefetchContainer::LoadState::kDeterminedHead:
      action_prefetch_container_load_state_int = 4;
      break;
    case PrefetchContainer::LoadState::kCompleted:
      action_prefetch_container_load_state_int = 5;
      break;
    case PrefetchContainer::LoadState::kFailedHeldback:
      action_prefetch_container_load_state_int = 6;
      break;
    case PrefetchContainer::LoadState::kFailedIneligible:
      action_prefetch_container_load_state_int = 7;
      break;
    case PrefetchContainer::LoadState::kFailedDeterminedHead:
      action_prefetch_container_load_state_int = 8;
      break;
    case PrefetchContainer::LoadState::kFailed:
      action_prefetch_container_load_state_int = 9;
      break;
  }

  int action_is_expired;
  if (!match_resolver_action.is_expired().has_value()) {
    action_is_expired = 1;
  } else if (!match_resolver_action.is_expired().value()) {
    action_is_expired = 2;
  } else {
    action_is_expired = 3;
  }

  return servable_state_int * 1000 + action_kind_int * 100 +
         action_prefetch_container_load_state_int * 10 + action_is_expired;
}

int GetCodeOfPotentialCandidateServingResultAndServableStateAndMatcherAction(
    PrefetchPotentialCandidateServingResult serving_result,
    PrefetchServableState servable_state,
    const PrefetchMatchResolverAction& match_resolver_action) {
  return static_cast<int>(serving_result) * 10000 +
         GetCodeOfPrefetchServableStateAndPrefetchMatchResolverActionForDebug(
             servable_state, match_resolver_action);
}
}  // namespace content
