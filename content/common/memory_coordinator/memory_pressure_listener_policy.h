// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_PRESSURE_LISTENER_POLICY_H_
#define CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_PRESSURE_LISTENER_POLICY_H_

#include "base/callback_list.h"
#include "base/memory/memory_pressure_level.h"
#include "content/common/content_export.h"
#include "content/public/common/memory_coordinator/predicate_memory_coordinator_policy.h"

namespace content {

// A memory coordinator policy that responds to system memory pressure signals.
//
// This policy listens for memory pressure notifications and, when they occur,
// updates the memory limit of all consumers in the current process and
// requests them to release memory. While the memory limit is below
// base::MemoryLimit::ModeratePressureThreshold(), consumers that release memory
// by writing it to disk get the lowered limit but are not asked to release
// memory (see
// base::MemoryConsumerTraits::ReleaseMemoryDataAccess::kReadsAndWritesToDisk).
class CONTENT_EXPORT MemoryPressureListenerPolicy
    : public PredicateMemoryCoordinatorPolicy {
 public:
  explicit MemoryPressureListenerPolicy(
      MemoryCoordinatorPolicyManager& manager);
  ~MemoryPressureListenerPolicy() override;

 private:
  // Updates the memory limit of the consumers based on `level`.
  void OnMemoryPressureLevelUpdated(base::MemoryPressureLevel level);

  // Subscription to the memory pressure notifications of the
  // MemoryPressureSource.
  base::CallbackListSubscription memory_pressure_subscription_;
};
}  // namespace content

#endif  // CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_PRESSURE_LISTENER_POLICY_H_
