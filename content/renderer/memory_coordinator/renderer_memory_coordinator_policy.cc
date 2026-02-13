// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/memory_coordinator/renderer_memory_coordinator_policy.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "content/child/memory_coordinator/child_memory_coordinator.h"

namespace content {

namespace {

RendererMemoryCoordinatorPolicy* g_instance = nullptr;

}  // namespace

BASE_FEATURE(kMemoryCoordinatorLastResortGC, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kRestoreLimitSeconds,
                   &kMemoryCoordinatorLastResortGC,
                   "restore_limit_seconds",
                   0);

// static
RendererMemoryCoordinatorPolicy& RendererMemoryCoordinatorPolicy::Get() {
  CHECK(g_instance);
  return *g_instance;
}

RendererMemoryCoordinatorPolicy::RendererMemoryCoordinatorPolicy(
    ChildMemoryCoordinator& coordinator)
    : MemoryCoordinatorPolicy(coordinator.policy_manager()),
      coordinator_(coordinator) {
  CHECK(!g_instance);
  g_instance = this;
  coordinator_->policy_manager().AddPolicy(this);
}

RendererMemoryCoordinatorPolicy::~RendererMemoryCoordinatorPolicy() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
  coordinator_->policy_manager().RemovePolicy(this);
}

void RendererMemoryCoordinatorPolicy::OnConsumerGroupAdded(
    std::string_view consumer_id,
    base::MemoryConsumerTraits traits,
    ProcessType process_type,
    ChildProcessId child_process_id) {
  if (traits.release_gc_references !=
      base::MemoryConsumerTraits::ReleaseGCReferences::kYes) {
    return;
  }

  auto [_, inserted] = consumer_ids_.emplace(consumer_id);
  CHECK(inserted);
}

void RendererMemoryCoordinatorPolicy::OnConsumerGroupRemoved(
    std::string_view consumer_id,
    ChildProcessId child_process_id) {
  consumer_ids_.erase(consumer_id);
}

void RendererMemoryCoordinatorPolicy::OnV8HeapLastResortGC() {
  if (!base::FeatureList::IsEnabled(kMemoryCoordinatorLastResortGC)) {
    return;
  }

  // The V8 heap is full and can't free enough memory. To help the impending GC,
  // notify consumers that retain references to the v8 heap.
  std::vector<MemoryConsumerUpdate> updates;
  updates.reserve(consumer_ids_.size());
  for (const std::string& consumer_id : consumer_ids_) {
    updates.push_back({consumer_id, 0, true});
  }
  manager().UpdateConsumers(this, std::move(updates));

  // Immediately restore the limit if there is no delay.
  if (kRestoreLimitSeconds.Get() == 0) {
    OnRestoreLimitTimerFired();
    return;
  }

  auto restore_limit_delay = base::Seconds(kRestoreLimitSeconds.Get());
  restore_limit_timer_.Start(
      FROM_HERE, restore_limit_delay, this,
      &RendererMemoryCoordinatorPolicy::OnRestoreLimitTimerFired);
}

void RendererMemoryCoordinatorPolicy::OnRestoreLimitTimerFired() {
  std::vector<MemoryConsumerUpdate> updates;
  updates.reserve(consumer_ids_.size());
  for (const std::string& consumer_id : consumer_ids_) {
    updates.push_back({consumer_id, base::MemoryConsumer::kDefaultMemoryLimit,
                       /*release_memory=*/false});
  }
  manager().UpdateConsumers(this, std::move(updates));
}

}  // namespace content
