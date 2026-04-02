// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEMORY_COORDINATOR_LAST_RESORT_GC_POLICY_H_
#define CONTENT_RENDERER_MEMORY_COORDINATOR_LAST_RESORT_GC_POLICY_H_

#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/field_trial_params.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/common/memory_coordinator/memory_coordinator_policy.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_state.h"

namespace content {

CONTENT_EXPORT BASE_DECLARE_FEATURE(kMemoryCoordinatorLastResortGC);
CONTENT_EXPORT BASE_DECLARE_FEATURE_PARAM(int, kRestoreLimitSeconds);

class ChildMemoryCoordinator;

// A policy that is triggered when V8 is about to perform a last resort garbage
// collection. It instructs memory consumers that retain references to the V8
// heap to release as much memory as possible to help the impending GC and avoid
// an out-of-memory (OOM) crash.
class CONTENT_EXPORT LastResortGCPolicy : public MemoryCoordinatorPolicy {
 public:
  // Returns the global instance, or null if it hasn't been instantiated.
  static LastResortGCPolicy* Get();

  explicit LastResortGCPolicy(ChildMemoryCoordinator& coordinator);
  ~LastResortGCPolicy() override;

  // Notifies the policy that V8 is about to run its last resort GC.
  void OnV8HeapLastResortGC();

 private:
  void OnRestoreLimitTimerFired();

  // The coordinator outlives `this`.
  raw_ref<ChildMemoryCoordinator> coordinator_;

  base::OneShotTimer restore_limit_timer_;
  MemoryCoordinatorPolicyState state_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEMORY_COORDINATOR_LAST_RESORT_GC_POLICY_H_
