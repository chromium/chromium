// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_COORDINATOR_POLICY_MANAGER_H_
#define CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_COORDINATOR_POLICY_MANAGER_H_

#include <memory>
#include <string>
#include <tuple>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/common/memory_coordinator/memory_consumer_group_controller.h"
#include "content/common/memory_coordinator/memory_coordinator_policy.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace content {

// Manages memory coordinator policies and aggregates their memory limit
// requests for consumer groups, ensuring the most restrictive (lowest) limit is
// always applied.
class CONTENT_EXPORT MemoryCoordinatorPolicyManager
    : public MemoryConsumerGroupController {
 public:
  MemoryCoordinatorPolicyManager();
  ~MemoryCoordinatorPolicyManager() override;

  MemoryCoordinatorPolicyManager(const MemoryCoordinatorPolicyManager&) =
      delete;
  MemoryCoordinatorPolicyManager& operator=(
      const MemoryCoordinatorPolicyManager&) = delete;

  // Registers a policy with the manager. `policy` must remain valid until it is
  // removed with a call to RemovePolicy().
  void AddPolicy(MemoryCoordinatorPolicy* policy);

  // Unregisters a policy with the manager and clears all its associated data.
  void RemovePolicy(MemoryCoordinatorPolicy* policy);

  // MemoryConsumerGroupController:
  void OnConsumerGroupAdded(std::string_view consumer_id,
                            base::MemoryConsumerTraits traits,
                            ProcessType process_type,
                            ChildProcessId child_process_id,
                            base::RegisteredMemoryConsumer consumer) override;
  void OnConsumerGroupRemoved(std::string_view consumer_id,
                              ChildProcessId child_process_id) override;

  // Called by policies to request actions on a consumer group.
  void SetMemoryLimit(MemoryCoordinatorPolicy* policy,
                      std::string_view consumer_id,
                      ChildProcessId child_process_id,
                      int percentage);
  void ReleaseMemory(std::string_view consumer_id,
                     ChildProcessId child_process_id);

 private:
  using ConsumerGroupKey = std::tuple<std::string, ChildProcessId>;

  struct GroupState {
    GroupState(base::MemoryConsumerTraits traits,
               ProcessType process_type,
               base::RegisteredMemoryConsumer consumer);
    ~GroupState();

    base::MemoryConsumerTraits traits;
    ProcessType process_type;
    base::RegisteredMemoryConsumer consumer;

    // The limit requested by each policy.
    absl::flat_hash_map<MemoryCoordinatorPolicy*, int> requested_limits;
  };

  // Computes the memory limit based on existing policies applied to
  // `group_state`.
  void RecomputeMemoryLimit(GroupState& group_state);

  base::flat_set<MemoryCoordinatorPolicy*> policies_;

  absl::flat_hash_map<ConsumerGroupKey, std::unique_ptr<GroupState>> groups_;
};

}  // namespace content

#endif  // CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_COORDINATOR_POLICY_MANAGER_H_
