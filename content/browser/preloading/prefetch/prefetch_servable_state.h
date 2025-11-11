// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVABLE_STATE_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVABLE_STATE_H_

#include <optional>
#include <ostream>

#include "content/common/content_export.h"

namespace content {

enum class PrefetchContainerLoadState;
enum class PrefetchPotentialCandidateServingResult;

// TODO(crbug.com/372186548): Revisit the shape of `PrefetchServableState`.
//
// See also https://crrev.com/c/5831122
enum class PrefetchServableState {
  // `PrefetchService` is checking eligibility of the prefetch or waiting load
  // start after eligibility check.
  //
  // Prefetch matching process should block until eligibility is got (and load
  // start) not to fall back normal navigation without waiting prefetch ahead of
  // prerender and send a duplicated fetch request.
  //
  // This state occurs only if `kPrerender2FallbackPrefetchSpecRules` is
  // enabled. Otherwise, `kNotServable` is returned for this period.
  kShouldBlockUntilEligibilityGot,

  // The load is started but non redirect header is not received yet.
  //
  // Prefetch matching process should block until the head of this is received
  // on a navigation to a matching URL, as a server can send a response header
  // including NoVarySearch header that contradicts NoVarySearch hint.
  kShouldBlockUntilHeadReceived,

  // This received non redirect header and is not expired.
  //
  // Note that it needs more checks to serve, e.g. cookie check. See also e.g.
  // `PrefetchMatchResolver::OnDeterminedHead()`.
  kServable,

  // Not other states.
  kNotServable,
};

CONTENT_EXPORT std::ostream& operator<<(std::ostream& ostream,
                                        PrefetchServableState servable_state);

// Action for wait loop of prefetch matching
//
// This represents an action of `PrefetchMatchResolver` for `PrefetchContainer`.
//
// Currently, this is an intermediate data and converted to
// `PrefetchServableState`.
//
// Mid-term plan:
// https://docs.google.com/document/d/1yRYq7GekwIjvvF5XRjDa6bGoba3HQKrlcw8vZIjTIo0
class CONTENT_EXPORT PrefetchMatchResolverAction {
 public:
  enum class ActionKind {
    // The `PrefetchContainer` is not available now. Please drop and don't wait.
    kDrop,
    // The `PrefetchContainer` will be loaded. Please wait further events.
    kWait,
    // The `PrefetchContainer` is likely servebale. Please check further
    // conditions e.g. `IsCandidateAvailable()` in prefetch_match_resolver.h,
    // and try to start serving it.
    kMaybeServe,
  };

  // `is_expired` must be non null iff `kind == ActionKind::kMaybeServe`.
  PrefetchMatchResolverAction(ActionKind kind,
                              PrefetchContainerLoadState reason,
                              std::optional<bool> is_expired);
  ~PrefetchMatchResolverAction();

  // Movable but not copyable.
  PrefetchMatchResolverAction(PrefetchMatchResolverAction&& other) = default;
  PrefetchMatchResolverAction& operator=(PrefetchMatchResolverAction&& other) =
      default;
  PrefetchMatchResolverAction(const PrefetchMatchResolverAction&) = delete;
  PrefetchMatchResolverAction& operator=(const PrefetchMatchResolverAction&) =
      delete;

  PrefetchServableState ToServableState() const;

  ActionKind kind() const { return kind_; }
  PrefetchContainerLoadState prefetch_container_load_state() const {
    return prefetch_container_load_state_;
  }
  std::optional<bool> is_expired() const { return is_expired_; }

 private:
  ActionKind kind_;
  PrefetchContainerLoadState prefetch_container_load_state_;
  std::optional<bool> is_expired_;
};

// Encodes servable state and matcher action to int for debug
//
// Cardinality for `base::UmaHistogramSparse()`:
//
// ```
//    cardinality
// <= #PrefetchServableState * #PrefetchMatchResolverAction * 2
// =  3 * 8 * 2
// =  48
// ```
//
// For more details, see the implementation of
// `PrefetchContainer::GetMatchResolverAction()`. (Each `case` has at most two
// possible return values.)
//
// We expect `PrefetchServableState` derived from `PrefetchMatchResolverAction`
// coicides with `GetPrefetchServableState()`, and actual cardinality is at
// most 16.
int GetCodeOfPrefetchServableStateAndPrefetchMatchResolverActionForDebug(
    PrefetchServableState servable_state,
    const PrefetchMatchResolverAction& match_resolver_action);

// Encodes serving result, servable state and matcher action to int for debug.
//
// See the comment of
// `GetCodeOfPrefetchServableStateAndPrefetchMatchResolverActionForDebug()` for
// cardinality.
int GetCodeOfPotentialCandidateServingResultAndServableStateAndMatcherAction(
    content::PrefetchPotentialCandidateServingResult serving_result,
    PrefetchServableState servable_state,
    const PrefetchMatchResolverAction& match_resolver_action);
}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVABLE_STATE_H_
