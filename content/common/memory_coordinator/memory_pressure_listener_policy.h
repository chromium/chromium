// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_PRESSURE_LISTENER_POLICY_H_
#define CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_PRESSURE_LISTENER_POLICY_H_

#include <string_view>

#include "base/memory/memory_pressure_listener.h"
#include "content/common/content_export.h"
#include "content/common/memory_coordinator/memory_coordinator_policy.h"

namespace content {

// A memory coordinator policy that responds to system memory pressure signals.
//
// This policy listens for memory pressure notifications and, when they occur,
// updates the memory limit of all consumers in the current process and
// requests them to release memory.
class CONTENT_EXPORT MemoryPressureListenerPolicy
    : public MemoryCoordinatorPolicy,
      public base::MemoryPressureListener {
 public:
  explicit MemoryPressureListenerPolicy(
      MemoryCoordinatorPolicyManager& manager);
  ~MemoryPressureListenerPolicy() override;

  // base::MemoryPressureListener:
  void OnMemoryPressure(base::MemoryPressureLevel level) override;

 private:
  base::MemoryPressureListenerRegistration registration_;
};
}  // namespace content

#endif  // CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_PRESSURE_LISTENER_POLICY_H_
