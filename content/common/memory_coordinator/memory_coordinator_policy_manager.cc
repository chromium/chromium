// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "base/check.h"
#include "base/check_op.h"
#include "content/common/memory_coordinator/memory_consumer_group_host.h"
#include "content/common/memory_coordinator/memory_coordinator_policy.h"

namespace content {

// MemoryCoordinatorPolicyManager::GroupState ----------------------------------

MemoryCoordinatorPolicyManager::GroupState::GroupState(
    std::string_view consumer_name,
    std::optional<base::MemoryConsumerTraits> traits,
    ProcessType process_type)
    : consumer_name_(consumer_name),
      traits_(traits),
      process_type_(process_type) {}

MemoryCoordinatorPolicyManager::GroupState::~GroupState() = default;

std::optional<int>
MemoryCoordinatorPolicyManager::GroupState::SetMemoryLimitForPolicy(
    MemoryCoordinatorPolicy* policy,
    int percentage) {
  CHECK_GE(percentage, 0);

  // Get the previous requested limit for this policy.
  auto it = requested_limits_.find(policy);
  const int old_policy_limit = (it != requested_limits_.end())
                                   ? it->second
                                   : base::MemoryConsumer::kDefaultMemoryLimit;

  // Early exit if it didn't change.
  if (percentage == old_policy_limit) {
    return std::nullopt;
  }

  // Update the map, keeping it small by removing default entries.
  if (percentage == base::MemoryConsumer::kDefaultMemoryLimit) {
    DCHECK(it != requested_limits_.end());
    requested_limits_.erase(it);
  } else {
    requested_limits_[policy] = percentage;
  }

  // Recompute the aggregate limit across all policies. If the result is the
  // same as the current limit, no update is needed.
  int new_limit = RecomputeMemoryLimit();
  if (new_limit == current_limit_) {
    return std::nullopt;
  }
  current_limit_ = new_limit;
  return new_limit;
}

std::optional<int>
MemoryCoordinatorPolicyManager::GroupState::SetOverrideLimitForTesting(
    std::optional<int> percentage) {
  if (override_limit_ == percentage) {
    return std::nullopt;
  }
  override_limit_ = percentage;
  int new_limit = RecomputeMemoryLimit();
  if (new_limit == current_limit_) {
    return std::nullopt;
  }
  current_limit_ = new_limit;
  return new_limit;
}

int MemoryCoordinatorPolicyManager::GroupState::RecomputeMemoryLimit() const {
  if (override_limit_) {
    return *override_limit_;
  }

  // The aggregate limit is the product of all policy limits.
  // For example, if policy A requests 80% and policy B requests 50%, the
  // aggregate limit is 40% (0.8 * 0.5 = 0.4).
  double result = base::MemoryConsumer::kDefaultMemoryLimit;
  for (auto const& [policy, limit] : requested_limits_) {
    result *= limit / 100.0;
  }
  return std::nearbyint(result);
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
      observer->OnConsumerGroupAdded(consumer_id, group_state->consumer_name(),
                                     group_state->traits(),
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
  // consumer groups.
  for (auto const& [child_id, host_state] : hosts_) {
    std::vector<MemoryConsumerUpdate> updates;
    updates.reserve(host_state->groups.size());
    for (auto const& [consumer_id, group_state] : host_state->groups) {
      // Setting the default limit clears the policy's requested limit.
      if (std::optional<int> new_limit = group_state->SetMemoryLimitForPolicy(
              policy, base::MemoryConsumer::kDefaultMemoryLimit)) {
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
                                              uint32_t consumer_id) {
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
    uint32_t consumer_id,
    std::string_view consumer_name,
    std::optional<base::MemoryConsumerTraits> traits,
    ProcessType process_type,
    ChildProcessId child_process_id) {
  HostState& host_state = GetHostState(child_process_id);

  auto [_, inserted] = host_state.groups.try_emplace(
      consumer_id,
      std::make_unique<GroupState>(consumer_name, traits, process_type));
  CHECK(inserted);

  // Apply any pending override for this consumer that was set before
  // registration.
  auto it = memory_limit_overrides_.find(consumer_name);
  if (it != memory_limit_overrides_.end()) {
    auto& group_state = host_state.groups[consumer_id];
    if (std::optional<int> new_limit =
            group_state->SetOverrideLimitForTesting(it->second)) {
      host_state.host->UpdateConsumers({{consumer_id, *new_limit, false}});
    }
  }

  for (auto& observer : observers_) {
    observer.OnConsumerGroupAdded(consumer_id, consumer_name, traits,
                                  process_type, child_process_id);
  }
}

void MemoryCoordinatorPolicyManager::OnConsumerGroupRemoved(
    uint32_t consumer_id,
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
    uint32_t consumer_id,
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

void MemoryCoordinatorPolicyManager::ApplyMemoryLimitOverrideForTesting(
    std::string_view consumer_name,
    int percentage) {
  for (auto const& [child_id, host_state] : hosts_) {
    std::vector<MemoryConsumerUpdate> updates;
    for (auto const& [consumer_id, group_state] : host_state->groups) {
      if (group_state->consumer_name() == consumer_name) {
        if (std::optional<int> new_limit =
                group_state->SetOverrideLimitForTesting(percentage)) {
          updates.push_back({consumer_id, *new_limit, false});
        }
      }
    }
    if (!updates.empty()) {
      host_state->host->UpdateConsumers(std::move(updates));
    }
  }
}

void MemoryCoordinatorPolicyManager::AddMemoryLimitOverrideForTesting(
    std::string_view consumer_name,
    int percentage) {
  auto [it, inserted] =
      memory_limit_overrides_.try_emplace(consumer_name, percentage);
  CHECK(inserted);

  ApplyMemoryLimitOverrideForTesting(consumer_name, percentage);
}

void MemoryCoordinatorPolicyManager::UpdateMemoryLimitOverrideForTesting(
    std::string_view consumer_name,
    int percentage) {
  auto it = memory_limit_overrides_.find(consumer_name);
  CHECK(it != memory_limit_overrides_.end());
  it->second = percentage;

  ApplyMemoryLimitOverrideForTesting(consumer_name, percentage);
}

void MemoryCoordinatorPolicyManager::ClearMemoryLimitOverrideForTesting(
    std::string_view consumer_name) {
  size_t removed = memory_limit_overrides_.erase(consumer_name);
  CHECK_EQ(removed, 1u);

  for (auto const& [child_id, host_state] : hosts_) {
    std::vector<MemoryConsumerUpdate> updates;
    for (auto const& [consumer_id, group_state] : host_state->groups) {
      if (group_state->consumer_name() == consumer_name) {
        if (std::optional<int> new_limit =
                group_state->SetOverrideLimitForTesting(std::nullopt)) {
          updates.push_back({consumer_id, *new_limit, false});
        }
      }
    }
    if (!updates.empty()) {
      host_state->host->UpdateConsumers(std::move(updates));
    }
  }
}

void MemoryCoordinatorPolicyManager::NotifyReleaseMemoryForTesting(
    std::string_view consumer_name) {
  for (auto const& [child_id, host_state] : hosts_) {
    std::vector<MemoryConsumerUpdate> updates;
    for (auto const& [consumer_id, group_state] : host_state->groups) {
      if (group_state->consumer_name() == consumer_name) {
        updates.push_back({consumer_id, std::nullopt, true});
      }
    }
    if (!updates.empty()) {
      host_state->host->UpdateConsumers(std::move(updates));
    }
  }
}

}  // namespace content
