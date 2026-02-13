// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_COORDINATOR_POLICY_H_
#define CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_COORDINATOR_POLICY_H_

#include <string_view>

#include "base/memory/raw_ref.h"
#include "base/memory_coordinator/traits.h"
#include "content/common/content_export.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/process_type.h"

namespace content {

class MemoryCoordinatorPolicyManager;

// An interface for implementing memory management policies.
//
// A MemoryCoordinatorPolicy observes the lifecycle of memory consumers and
// can apply specific memory management logic (e.g., setting memory limits)
// through the associated MemoryCoordinatorPolicyManager.
//
// For example, a policy might be implemented to reduce the memory footprint of
// backgrounded renderers or to respond to system-level memory pressure events.
//
// Implementations must:
// - Register themselves with the MemoryCoordinatorPolicyManager.
// - Implement OnConsumerGroupAdded/Removed to track the consumers they are
//   interested in.
// - Use manager().UpdateConsumers() to request memory constraints on specific
//   consumer groups.
class CONTENT_EXPORT MemoryCoordinatorPolicy {
 public:
  virtual ~MemoryCoordinatorPolicy() = default;

  // Called when a new consumer group is added to the registry.
  virtual void OnConsumerGroupAdded(std::string_view consumer_id,
                                    base::MemoryConsumerTraits traits,
                                    ProcessType process_type,
                                    ChildProcessId child_process_id) = 0;

  // Called when a consumer group is removed from the registry.
  virtual void OnConsumerGroupRemoved(std::string_view consumer_id,
                                      ChildProcessId child_process_id) = 0;

 protected:
  explicit MemoryCoordinatorPolicy(MemoryCoordinatorPolicyManager& manager);

  MemoryCoordinatorPolicyManager& manager() { return manager_.get(); }

 private:
  const raw_ref<MemoryCoordinatorPolicyManager> manager_;
};

}  // namespace content

#endif  // CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_COORDINATOR_POLICY_H_
