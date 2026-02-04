// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_MEMORY_COORDINATOR_CHILD_MEMORY_COORDINATOR_H_
#define CONTENT_CHILD_MEMORY_COORDINATOR_CHILD_MEMORY_COORDINATOR_H_

#include "base/memory_coordinator/memory_consumer_registry.h"
#include "content/child/memory_coordinator/child_memory_consumer_registry.h"
#include "content/common/content_export.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"

namespace content {

// ChildMemoryCoordinator is a singleton that owns both the
// ChildMemoryConsumerRegistry and the MemoryCoordinatorPolicyManager.
class CONTENT_EXPORT ChildMemoryCoordinator {
 public:
  static ChildMemoryCoordinator& Get();

  ChildMemoryCoordinator();

  ChildMemoryCoordinator(const ChildMemoryCoordinator&) = delete;
  ChildMemoryCoordinator& operator=(const ChildMemoryCoordinator&) = delete;

  ~ChildMemoryCoordinator();

  ChildMemoryConsumerRegistry& registry() { return registry_.Get(); }
  MemoryCoordinatorPolicyManager& policy_manager() { return policy_manager_; }

 private:
  MemoryCoordinatorPolicyManager policy_manager_;
  base::ScopedMemoryConsumerRegistry<ChildMemoryConsumerRegistry> registry_{
      policy_manager_};
};

}  // namespace content

#endif  // CONTENT_CHILD_MEMORY_COORDINATOR_CHILD_MEMORY_COORDINATOR_H_
