// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"

#include <algorithm>
#include <memory>

#include "base/check.h"
#include "base/check_op.h"
#include "content/common/memory_coordinator/memory_consumer_group_host.h"
#include "content/common/memory_coordinator/memory_coordinator_policy.h"

namespace content {

// MemoryCoordinatorPolicyManager::GroupState ----------------------------------

MemoryCoordinatorPolicyManager::GroupState::GroupState(
    std::optional<base::MemoryConsumerTraits> traits,
    ProcessType process_type)
    : traits_(traits), process_type_(process_type) {}

MemoryCoordinatorPolicyManager::GroupState::~GroupState() = default;

std::optional<int>
MemoryCoordinatorPolicyManager::GroupState::SetMemoryLimitForPolicy(
    MemoryCoordinatorPolicy* policy,
    int percentage) {
  requested_limits_[policy] = percentage;
  int new_limit = RecomputeMemoryLimit();
  if (new_limit == current_limit_) {
    return std::nullopt;
  }
  current_limit_ = new_limit;
  return new_limit;
}

std::optional<int>
MemoryCoordinatorPolicyManager::GroupState::ClearMemoryLimitForPolicy(
    MemoryCoordinatorPolicy* policy) {
  if (requested_limits_.erase(policy) == 0) {
    return std::nullopt;
  }
  int new_limit = RecomputeMemoryLimit();
  if (new_limit == current_limit_) {
    return std::nullopt;
  }
  current_limit_ = new_limit;
  return new_limit;
}

int MemoryCoordinatorPolicyManager::GroupState::RecomputeMemoryLimit() const {
  // If no policies specify a limit, the default limit is used.
  int min_limit = base::MemoryConsumer::kDefaultMemoryLimit;
  for (auto const& [policy, limit] : requested_limits_) {
    min_limit = std::min(min_limit, limit);
  }
  return min_limit;
}

// MemoryCoordinatorPolicyManager::HostState -----------------------------------

MemoryCoordinatorPolicyManager::HostState::HostState(
    MemoryConsumerGroupHost* host)
    : host(host) {}

MemoryCoordinatorPolicyManager::HostState::~HostState() {
  CHECK(groups.empty());
}

// MemoryCoordinatorPolicyManager ----------------------------------------------

MemoryCoordinatorPolicyManager::MemoryCoordinatorPolicyManager() = default;

MemoryCoordinatorPolicyManager::~MemoryCoordinatorPolicyManager() = default;

void MemoryCoordinatorPolicyManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);

  for (auto const& [child_id, host_state] : hosts_) {
    for (auto const& [consumer_id, group_state] : host_state->groups) {
      observer->OnConsumerGroupAdded(consumer_id, group_state->traits(),
                                     group_state->process_type(), child_id);
    }
  }
}

void MemoryCoordinatorPolicyManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
void MemoryCoordinatorPolicyManager::AddDiagnosticObserver(
    DiagnosticObserver* observer) {
  diagnostic_observers_.AddObserver(observer);

  // Catch up with existing limits.
  for (auto const& [child_id, host_state] : hosts_) {
    for (auto const& [consumer_id, group_state] : host_state->groups) {
      observer->OnMemoryLimitChanged(consumer_id, child_id,
                                     group_state->current_limit());
    }
  }
}

void MemoryCoordinatorPolicyManager::RemoveDiagnosticObserver(
    DiagnosticObserver* observer) {
  diagnostic_observers_.RemoveObserver(observer);
}
#endif  // BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)

void MemoryCoordinatorPolicyManager::AddPolicy(
    MemoryCoordinatorPolicy* policy) {
  auto [_, inserted] = policies_.insert(policy);
  CHECK(inserted);
}

void MemoryCoordinatorPolicyManager::RemovePolicy(
    MemoryCoordinatorPolicy* policy) {
  size_t removed = policies_.erase(policy);
  CHECK_EQ(removed, 1u);

  // When a policy is removed, its requested limits are cleared from all
  // consumer groups and aggregate limits are recomputed to ensure the
  // remaining policies are correctly applied.
  for (auto const& [child_id, host_state] : hosts_) {
    std::vector<MemoryConsumerUpdate> updates;
    updates.reserve(host_state->groups.size());
    for (auto const& [consumer_id, group_state] : host_state->groups) {
      if (std::optional<int> new_limit =
              group_state->ClearMemoryLimitForPolicy(policy)) {
        updates.push_back({consumer_id, *new_limit, /*release_memory=*/false});
      }
    }
    if (!updates.empty()) {
      host_state->host->UpdateConsumers(std::move(updates));
    }
  }
}

MemoryCoordinatorPolicyManager::HostState&
MemoryCoordinatorPolicyManager::GetHostState(ChildProcessId child_process_id) {
  auto it = hosts_.find(child_process_id);
  CHECK(it != hosts_.end());
  return *it->second;
}

MemoryCoordinatorPolicyManager::GroupState&
MemoryCoordinatorPolicyManager::GetGroupState(HostState& host_state,
                                              std::string_view consumer_id) {
  auto it = host_state.groups.find(consumer_id);
  CHECK(it != host_state.groups.end());
  return *it->second;
}

void MemoryCoordinatorPolicyManager::AddMemoryConsumerGroupHost(
    ChildProcessId child_process_id,
    MemoryConsumerGroupHost* host) {
  auto [_, inserted] =
      hosts_.try_emplace(child_process_id, std::make_unique<HostState>(host));
  CHECK(inserted);
}

void MemoryCoordinatorPolicyManager::RemoveMemoryConsumerGroupHost(
    ChildProcessId child_process_id) {
  size_t removed = hosts_.erase(child_process_id);
  CHECK_EQ(removed, 1u);
}

void MemoryCoordinatorPolicyManager::OnConsumerGroupAdded(
    std::string_view consumer_id,
    std::optional<base::MemoryConsumerTraits> traits,
    ProcessType process_type,
    ChildProcessId child_process_id) {
  HostState& host_state = GetHostState(child_process_id);

  auto [_, inserted] = host_state.groups.try_emplace(
      consumer_id, std::make_unique<GroupState>(traits, process_type));
  CHECK(inserted);

  for (auto& observer : observers_) {
    observer.OnConsumerGroupAdded(consumer_id, traits, process_type,
                                  child_process_id);
  }
}

void MemoryCoordinatorPolicyManager::OnConsumerGroupRemoved(
    std::string_view consumer_id,
    ChildProcessId child_process_id) {
  for (auto& observer : observers_) {
    observer.OnConsumerGroupRemoved(consumer_id, child_process_id);
  }

  HostState& host_state = GetHostState(child_process_id);

  size_t removed = host_state.groups.erase(consumer_id);
  CHECK_EQ(removed, 1u);
}

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
void MemoryCoordinatorPolicyManager::OnMemoryLimitChanged(
    std::string_view consumer_id,
    ChildProcessId child_process_id,
    int memory_limit) {
  for (auto& observer : diagnostic_observers_) {
    observer.OnMemoryLimitChanged(consumer_id, child_process_id, memory_limit);
  }
}
#endif

void MemoryCoordinatorPolicyManager::UpdateConsumers(
    MemoryCoordinatorPolicy* policy,
    std::vector<GlobalMemoryConsumerUpdate> updates) {
  // Global updates are split by child process ID so they can be processed
  // individually for each host.
  base::flat_map<ChildProcessId, std::vector<MemoryConsumerUpdate>>
      process_to_updates;

  for (auto& global_update : updates) {
    process_to_updates[global_update.child_process_id].push_back(
        std::move(global_update.update));
  }

  for (auto& [child_id, ipc_updates] : process_to_updates) {
    UpdateConsumersForProcess(policy, child_id, std::move(ipc_updates));
  }
}

void MemoryCoordinatorPolicyManager::UpdateConsumers(
    MemoryCoordinatorPolicy* policy,
    std::vector<MemoryConsumerUpdate> updates) {
  // Only consumer groups in the current process are updated, so a null child
  // process ID is passed.
  UpdateConsumersForProcess(policy, ChildProcessId(), std::move(updates));
}

void MemoryCoordinatorPolicyManager::UpdateConsumers(
    MemoryCoordinatorPolicy* policy,
    ConsumerFilter filter,
    std::optional<int> percentage,
    bool release_memory) {
  for (auto const& [child_id, host_state] : hosts_) {
    std::vector<MemoryConsumerUpdate> updates;
    for (auto const& [consumer_id, group_state] : host_state->groups) {
      if (filter(consumer_id, group_state->traits(),
                 group_state->process_type(), child_id)) {
        updates.push_back({consumer_id, percentage, release_memory});
      }
    }
    if (!updates.empty()) {
      UpdateConsumersForProcess(policy, child_id, std::move(updates));
    }
  }
}

void MemoryCoordinatorPolicyManager::UpdateConsumersForProcess(
    MemoryCoordinatorPolicy* policy,
    ChildProcessId child_process_id,
    std::vector<MemoryConsumerUpdate> updates) {
  HostState& host_state = GetHostState(child_process_id);

  // The `updates` vector is modified in-place, removing entries that do not
  // result in a change to the aggregate limit or a memory release request.
  std::erase_if(updates, [&](auto& update) {
    GroupState& group_state = GetGroupState(host_state, update.consumer_id);

    std::optional<int> new_effective_limit;
    if (update.percentage) {
      new_effective_limit =
          group_state.SetMemoryLimitForPolicy(policy, *update.percentage);
    }

    // Redundant updates that have no observable effect on the consumer group
    // state are filtered out to avoid sending unnecessary work to the host.
    if (!new_effective_limit && !update.release_memory) {
      return true;
    }

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
    if (new_effective_limit) {
      OnMemoryLimitChanged(update.consumer_id, child_process_id,
                           *new_effective_limit);
    }
#endif

    // Replace the policy request with the computed aggregate limit for the IPC.
    update.percentage = new_effective_limit;
    return false;
  });

  if (!updates.empty()) {
    host_state.host->UpdateConsumers(std::move(updates));
  }
}

void MemoryCoordinatorPolicyManager::NotifyReleaseMemoryForTesting() {
  for (auto const& [_, host_state] : hosts_) {
    std::vector<MemoryConsumerUpdate> updates;
    updates.reserve(host_state->groups.size());
    for (auto const& [consumer_id, group_state] : host_state->groups) {
      updates.push_back({std::string(consumer_id), std::nullopt, true});
    }
    if (!updates.empty()) {
      host_state->host->UpdateConsumers(std::move(updates));
    }
  }
}

void MemoryCoordinatorPolicyManager::NotifyUpdateMemoryLimitForTesting(
    int percentage) {
  for (auto const& [_, host_state] : hosts_) {
    std::vector<MemoryConsumerUpdate> updates;
    updates.reserve(host_state->groups.size());
    for (auto const& [consumer_id, group_state] : host_state->groups) {
      if (percentage != group_state->current_limit()) {
        group_state->SetCurrentLimitForTesting(percentage);
        updates.push_back({std::string(consumer_id), percentage, false});
      }
    }
    if (!updates.empty()) {
      host_state->host->UpdateConsumers(std::move(updates));
    }
  }
}

}  // namespace content
