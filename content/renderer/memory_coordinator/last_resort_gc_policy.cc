// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/memory_coordinator/last_resort_gc_policy.h"

#include <string>
#include <string_view>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace content {

namespace {

LastResortGCPolicy* g_instance = nullptr;

}  // namespace

BASE_FEATURE(kMemoryCoordinatorLastResortGC, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kRestoreLimitSeconds,
                   &kMemoryCoordinatorLastResortGC,
                   "restore_limit_seconds",
                   0);

// static
LastResortGCPolicy* LastResortGCPolicy::Get() {
  return g_instance;
}

LastResortGCPolicy::LastResortGCPolicy(MemoryCoordinatorPolicyManager& manager)
    : PredicateMemoryCoordinatorPolicy(
          manager,
          base::BindRepeating([](uint32_t consumer_id,
                                 base::MemoryConsumerTraits traits,
                                 ProcessType process_type,
                                 ChildProcessId child_process_id) {
            return traits.release_gc_references ==
                   base::MemoryConsumerTraits::ReleaseGCReferences::kYes;
          })),
      policy_registration_(manager, *this) {
  CHECK(!g_instance);
  g_instance = this;
}

LastResortGCPolicy::~LastResortGCPolicy() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void LastResortGCPolicy::OnV8HeapLastResortGC() {
  // The V8 heap is full and can't free enough memory. To help the impending GC,
  // notify consumers that retain references to the v8 heap.
  SetLimit(0, /*release_memory=*/true);

  // Immediately restore the limit if there is no delay.
  if (kRestoreLimitSeconds.Get() == 0) {
    OnRestoreLimitTimerFired();
    return;
  }

  auto restore_limit_delay = base::Seconds(kRestoreLimitSeconds.Get());
  restore_limit_timer_.Start(FROM_HERE, restore_limit_delay, this,
                             &LastResortGCPolicy::OnRestoreLimitTimerFired);
}

void LastResortGCPolicy::OnRestoreLimitTimerFired() {
  SetLimit(base::MemoryConsumer::kDefaultMemoryLimit,
           /*release_memory=*/false);
}

}  // namespace content
