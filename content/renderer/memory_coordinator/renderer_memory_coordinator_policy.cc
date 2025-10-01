// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/memory_coordinator/renderer_memory_coordinator_policy.h"

#include "base/memory_coordinator/traits.h"
#include "content/child/memory_coordinator/child_memory_consumer_registry.h"

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
    ChildMemoryConsumerRegistry& registry)
    : registry_(registry) {
  CHECK(!g_instance);
  g_instance = this;
}

RendererMemoryCoordinatorPolicy::~RendererMemoryCoordinatorPolicy() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void RendererMemoryCoordinatorPolicy::OnV8HeapLastResortGC() {
  if (!base::FeatureList::IsEnabled(kMemoryCoordinatorLastResortGC)) {
    return;
  }

  // The V8 heap is full and can't free enough memory. To help the impending GC,
  // notify consumers that retain references to the v8 heap.
  for (auto& consumer_info : *registry_) {
    if (consumer_info.traits.release_gc_references ==
        base::MemoryConsumerTraits::ReleaseGCReferences::kYes) {
      consumer_info.consumer.UpdateMemoryLimit(0);
      consumer_info.consumer.ReleaseMemory();
    }
  }

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
  for (auto& consumer_info : *registry_) {
    if (consumer_info.traits.release_gc_references ==
        base::MemoryConsumerTraits::ReleaseGCReferences::kYes) {
      consumer_info.consumer.UpdateMemoryLimit(100);
    }
  }
}

}  // namespace content
