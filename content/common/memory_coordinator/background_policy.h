// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEMORY_COORDINATOR_BACKGROUND_POLICY_H_
#define CONTENT_COMMON_MEMORY_COORDINATOR_BACKGROUND_POLICY_H_

#include <array>
#include <cstdint>
#include <string_view>

#include "base/containers/enum_set.h"
#include "base/memory/post_delayed_memory_reduction_task.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/memory_coordinator/traits.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/common/memory_coordinator/memory_coordinator_policy.h"

namespace content {

// A memory coordinator policy that reduces the memory usage of consumers in the
// local process while it is backgrounded.
//
// When the process is backgrounded, the memory limit of consumers that opted in
// through the AffectedByBackgrounding trait is lowered right away, which stops
// them from growing. They are asked to release memory after a delay that
// depends on the value of that trait. On Android 14+, if the process is about
// to be frozen before that delay expires, they are asked to release memory
// right away instead (see base::OneShotDelayedBackgroundTimer). When the
// process is foregrounded, the limit is restored.
//
// The policy doesn't track the process's priority itself: the memory
// coordinator that creates it (BrowserMemoryCoordinatorImpl or
// ChildMemoryCoordinator) must call OnProcessPriorityChanged().
class CONTENT_EXPORT BackgroundPolicy : public MemoryCoordinatorPolicy {
 public:
  // Reusing the critical pressure threshold means that consumers that treat
  // anything at or below it as critical pressure handle backgrounding the same
  // way without any changes.
  //
  // Since the limits requested by all policies are multiplied, a consumer whose
  // limit was lowered by this policy releases all its memory when any other
  // policy asks it to release memory (e.g. under memory pressure), even before
  // its delay expires.
  static constexpr base::MemoryLimit kBackgroundLimit =
      base::MemoryLimit::CriticalPressureThreshold();

  // How long to wait after the process is backgrounded before asking consumers
  // to release memory, for each AffectedByBackgrounding value (see
  // base/memory_coordinator/traits.h).
  static constexpr base::TimeDelta kShortDelay = base::Seconds(10);
  static constexpr base::TimeDelta kMediumDelay = base::Minutes(1);
  static constexpr base::TimeDelta kLongDelay = base::Minutes(5);

  explicit BackgroundPolicy(MemoryCoordinatorPolicyManager& manager);
  ~BackgroundPolicy() override;

  // Must be called when the priority of the process changes. Only kBestEffort
  // counts as backgrounded. Repeated calls with the same state are ignored.
  void OnProcessPriorityChanged(base::Process::Priority process_priority);

 private:
  using AffectedByBackgrounding =
      base::MemoryConsumerTraits::AffectedByBackgrounding;

  // All AffectedByBackgrounding values that come with a release delay, i.e. all
  // of them except kNo.
  using DelayedValues = base::EnumSet<AffectedByBackgrounding,
                                      AffectedByBackgrounding::kAfterShortDelay,
                                      AffectedByBackgrounding::kMaxValue>;

  // Invoked by OnProcessPriorityChanged() when the process is backgrounded or
  // foregrounded. Repeated calls with the same state are ignored.
  void OnBackgrounded();
  void OnForegrounded();

  // MemoryCoordinatorPolicy:
  void OnConsumerGroupAdded(uint32_t consumer_id,
                            std::string_view consumer_name,
                            base::MemoryConsumerTraits traits,
                            ProcessType process_type,
                            ChildProcessId child_process_id) override;
  void OnConsumerGroupRemoved(uint32_t consumer_id,
                              ChildProcessId child_process_id) override;

  // Returns the timer for `affected_by_backgrounding`, which can't be kNo.
  base::OneShotDelayedBackgroundTimer& GetReleaseTimer(
      AffectedByBackgrounding affected_by_backgrounding);

  // Invoked when the delay associated with `affected_by_backgrounding` expires,
  // or earlier if the process is about to be frozen.
  void OnReleaseDelayExpired(AffectedByBackgrounding affected_by_backgrounding);

  SEQUENCE_CHECKER(sequence_checker_);

  bool is_backgrounded_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // One timer for each AffectedByBackgrounding value except kNo. While
  // backgrounded, a running timer means its consumers haven't been asked to
  // release memory yet.
  std::array<base::OneShotDelayedBackgroundTimer, DelayedValues::kValueCount>
      release_timers_ GUARDED_BY_CONTEXT(sequence_checker_);

  MemoryCoordinatorPolicyRegistration registration_;
};

}  // namespace content

#endif  // CONTENT_COMMON_MEMORY_COORDINATOR_BACKGROUND_POLICY_H_
