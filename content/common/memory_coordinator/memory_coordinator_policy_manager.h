// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_COORDINATOR_POLICY_MANAGER_H_
#define CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_COORDINATOR_POLICY_MANAGER_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "content/common/content_export.h"
#include "content/common/memory_coordinator/memory_consumer_group_controller.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/process_type.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace content {

class MemoryCoordinatorPolicy;

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
  void AddMemoryConsumerGroupHost(ChildProcessId child_process_id,
                                  MemoryConsumerGroupHost* host) override;
  void RemoveMemoryConsumerGroupHost(ChildProcessId child_process_id) override;
  void OnConsumerGroupAdded(std::string_view consumer_id,
                            base::MemoryConsumerTraits traits,
                            ProcessType process_type,
                            ChildProcessId child_process_id) override;
  void OnConsumerGroupRemoved(std::string_view consumer_id,
                              ChildProcessId child_process_id) override;

  // Called by policies to request actions on a consumer group.
  void UpdateMemoryLimit(MemoryCoordinatorPolicy* policy,
                         std::string_view consumer_id,
                         ChildProcessId child_process_id,
                         int percentage);
  void ReleaseMemory(std::string_view consumer_id,
                     ChildProcessId child_process_id);

  // For testing only. Notifies all registered consumer groups.
  void NotifyReleaseMemoryForTesting();
  void NotifyUpdateMemoryLimitForTesting(int percentage);

 private:
  struct GroupState {
    GroupState(base::MemoryConsumerTraits traits, ProcessType process_type);
    ~GroupState();

    base::MemoryConsumerTraits traits;
    ProcessType process_type;

    // The limit requested by each policy.
    base::flat_map<MemoryCoordinatorPolicy*, int> requested_limits;

    // The last memory limit that was applied to this group.
    int current_limit = base::MemoryConsumer::kDefaultMemoryLimit;
  };

  struct HostState {
    explicit HostState(MemoryConsumerGroupHost* host);
    ~HostState();

    raw_ptr<MemoryConsumerGroupHost> host;
    absl::flat_hash_map<std::string, std::unique_ptr<GroupState>> groups;
  };

  // Computes the memory limit based on existing policies.
  int RecomputeMemoryLimit(
      const base::flat_map<MemoryCoordinatorPolicy*, int>& requested_limits);

  base::flat_set<MemoryCoordinatorPolicy*> policies_;

  absl::flat_hash_map<ChildProcessId, std::unique_ptr<HostState>> hosts_;
};

}  // namespace content

#endif  // CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_COORDINATOR_POLICY_MANAGER_H_
