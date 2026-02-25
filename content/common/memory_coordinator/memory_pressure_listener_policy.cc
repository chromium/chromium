// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/memory_pressure_listener_policy.h"

#include <optional>

#include "base/memory/memory_pressure_listener.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"
#include "content/public/common/child_process_id.h"

namespace content {

MemoryPressureListenerPolicy::MemoryPressureListenerPolicy(
    MemoryCoordinatorPolicyManager& manager)
    : MemoryCoordinatorPolicy(manager),
      registration_(
          base::MemoryPressureListenerTag::kMemoryPressureListenerPolicy,
          this) {}

MemoryPressureListenerPolicy::~MemoryPressureListenerPolicy() = default;

void MemoryPressureListenerPolicy::OnMemoryPressure(
    base::MemoryPressureLevel level) {
  int limit = GetMemoryLimit();

  // Always request to release memory here. The signal was originally designed
  // for the MemoryPressureListener, which never made a distinction between
  // capping memory usage and actively freeing it.
  bool release_memory = true;

  manager().UpdateConsumers(
      this,
      [](std::string_view consumer_id,
         std::optional<base::MemoryConsumerTraits> traits,
         ProcessType process_type, ChildProcessId child_process_id) {
        return child_process_id.is_null();
      },
      limit, release_memory);
}

}  // namespace content
