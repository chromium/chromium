// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"

#include <algorithm>
#include <memory>

#include "base/check_op.h"
#include "content/common/memory_coordinator/memory_consumer_group_host.h"
#include "content/common/memory_coordinator/memory_coordinator_policy.h"

namespace content {

// MemoryCoordinatorPolicyManager::GroupState ----------------------------------

MemoryCoordinatorPolicyManager::GroupState::GroupState(
    base::MemoryConsumerTraits traits,
    ProcessType process_type)
    : traits(traits), process_type(process_type) {}

MemoryCoordinatorPolicyManager::GroupState::~GroupState() = default;

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

void MemoryCoordinatorPolicyManager::AddPolicy(
    MemoryCoordinatorPolicy* policy) {
  auto [_, inserted] = policies_.insert(policy);
  CHECK(inserted);

  for (auto const& [child_id, host_state] : hosts_) {
    for (auto const& [consumer_id, group_state] : host_state->groups) {
      policy->OnConsumerGroupAdded(consumer_id, group_state->traits,
                                   group_state->process_type, child_id);
    }
  }
}

void MemoryCoordinatorPolicyManager::RemovePolicy(
    MemoryCoordinatorPolicy* policy) {
  size_t removed = policies_.erase(policy);
  CHECK_EQ(removed, 1u);

  for (auto const& [child_id, host_state] : hosts_) {
    for (auto const& [consumer_id, group_state] : host_state->groups) {
      if (group_state->requested_limits.erase(policy)) {
        int new_limit = RecomputeMemoryLimit(group_state->requested_limits);
        if (new_limit != group_state->current_limit) {
          group_state->current_limit = new_limit;
          host_state->host->UpdateMemoryLimit(consumer_id, new_limit);
        }
      }
    }
  }
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
    base::MemoryConsumerTraits traits,
    ProcessType process_type,
    ChildProcessId child_process_id) {
  auto host_it = hosts_.find(child_process_id);
  CHECK(host_it != hosts_.end());
  HostState& host_state = *host_it->second;

  auto [_, inserted] = host_state.groups.try_emplace(
      consumer_id, std::make_unique<GroupState>(traits, process_type));
  CHECK(inserted);

  for (auto& policy : policies_) {
    policy->OnConsumerGroupAdded(consumer_id, traits, process_type,
                                 child_process_id);
  }
}

void MemoryCoordinatorPolicyManager::OnConsumerGroupRemoved(
    std::string_view consumer_id,
    ChildProcessId child_process_id) {
  for (auto& policy : policies_) {
    policy->OnConsumerGroupRemoved(consumer_id, child_process_id);
  }

  auto host_it = hosts_.find(child_process_id);
  CHECK(host_it != hosts_.end());
  HostState& host_state = *host_it->second;

  size_t removed = host_state.groups.erase(consumer_id);
  CHECK_EQ(removed, 1u);
}

void MemoryCoordinatorPolicyManager::UpdateMemoryLimit(
    MemoryCoordinatorPolicy* policy,
    std::string_view consumer_id,
    ChildProcessId child_process_id,
    int percentage) {
  auto host_it = hosts_.find(child_process_id);
  CHECK(host_it != hosts_.end());
  HostState& host_state = *host_it->second;

  auto group_it = host_state.groups.find(consumer_id);
  CHECK(group_it != host_state.groups.end());
  GroupState& group_state = *group_it->second;

  group_state.requested_limits[policy] = percentage;
  int new_limit = RecomputeMemoryLimit(group_state.requested_limits);
  if (new_limit != group_state.current_limit) {
    group_state.current_limit = new_limit;
    host_state.host->UpdateMemoryLimit(consumer_id, new_limit);
  }
}

void MemoryCoordinatorPolicyManager::ReleaseMemory(
    std::string_view consumer_id,
    ChildProcessId child_process_id) {
  auto host_it = hosts_.find(child_process_id);
  CHECK(host_it != hosts_.end());
  HostState& host_state = *host_it->second;

  host_state.host->ReleaseMemory(consumer_id);
}

void MemoryCoordinatorPolicyManager::NotifyReleaseMemoryForTesting() {
  for (auto const& [_, host_state] : hosts_) {
    for (auto const& [consumer_id, _] : host_state->groups) {
      host_state->host->ReleaseMemory(consumer_id);
    }
  }
}

void MemoryCoordinatorPolicyManager::NotifyUpdateMemoryLimitForTesting(
    int percentage) {
  for (auto const& [_, host_state] : hosts_) {
    for (auto const& [consumer_id, group_state] : host_state->groups) {
      if (percentage != group_state->current_limit) {
        group_state->current_limit = percentage;
        host_state->host->UpdateMemoryLimit(consumer_id, percentage);
      }
    }
  }
}

int MemoryCoordinatorPolicyManager::RecomputeMemoryLimit(
    const base::flat_map<MemoryCoordinatorPolicy*, int>& requested_limits) {
  // Note: This will reset the limit to its default value if all policies are
  // removed.
  int min_limit = base::MemoryConsumer::kDefaultMemoryLimit;
  for (auto const& [policy, limit] : requested_limits) {
    min_limit = std::min(min_limit, limit);
  }

  return min_limit;
}

}  // namespace content
