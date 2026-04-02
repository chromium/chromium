// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_COORDINATOR_POLICY_STATE_H_
#define CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_COORDINATOR_POLICY_STATE_H_

#include <optional>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "content/common/content_export.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"

namespace content {

class MemoryCoordinatorPolicy;

// A component that helps memory coordinator policies apply memory limits or
// release requests to all memory consumers that match a single predicate.
//
// This component manages the lifecycle of these "predicate-based" rules. When
// a limit is set via SetLimit(), it is automatically applied to all currently
// registered consumers that match the predicate. It also observes the
// registration of new consumers and ensures the rule is applied if they match
// the predicate.
//
// This allows policies to implement "persistent" settings (e.g., "all
// renderers should be capped at 50%") without manually tracking every consumer
// registration.
class CONTENT_EXPORT MemoryCoordinatorPolicyState
    : public MemoryCoordinatorPolicyManager::Observer {
 public:
  using ConsumerPredicate = base::RepeatingCallback<bool(
      uint32_t consumer_id,
      std::optional<base::MemoryConsumerTraits> traits,
      ProcessType process_type,
      ChildProcessId child_process_id)>;

  MemoryCoordinatorPolicyState(MemoryCoordinatorPolicy& policy,
                               MemoryCoordinatorPolicyManager& manager,
                               ConsumerPredicate predicate);

  MemoryCoordinatorPolicyState(const MemoryCoordinatorPolicyState&) = delete;
  MemoryCoordinatorPolicyState& operator=(const MemoryCoordinatorPolicyState&) =
      delete;

  ~MemoryCoordinatorPolicyState() override;

  // MemoryCoordinatorPolicyManager::Observer:
  void OnConsumerGroupAdded(uint32_t consumer_id,
                            std::string_view consumer_name,
                            std::optional<base::MemoryConsumerTraits> traits,
                            ProcessType process_type,
                            ChildProcessId child_process_id) override;
  void OnConsumerGroupRemoved(uint32_t consumer_id,
                              ChildProcessId child_process_id) override;

  // Updates the memory limit and/or release memory request for all consumers
  // matching the predicate. This will also apply to any matching consumer
  // added in the future.
  void SetLimit(int percentage, bool release_memory);

 private:
  // `policy_` and `manager_` must outlive this.
  const raw_ref<MemoryCoordinatorPolicy> policy_;
  const raw_ref<MemoryCoordinatorPolicyManager> manager_;
  const ConsumerPredicate predicate_;
  int percentage_ = base::MemoryConsumer::kDefaultMemoryLimit;
  bool release_memory_ = false;
};

}  // namespace content

#endif  // CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_COORDINATOR_POLICY_STATE_H_
