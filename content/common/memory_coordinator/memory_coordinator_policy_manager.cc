// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"

#include <algorithm>

namespace content {

// MemoryCoordinatorPolicyManager::GroupState ----------------------------------

MemoryCoordinatorPolicyManager::GroupState::GroupState(
    base::MemoryConsumerTraits traits,
    ProcessType process_type,
    base::RegisteredMemoryConsumer consumer)
    : traits(traits), process_type(process_type), consumer(consumer) {}

MemoryCoordinatorPolicyManager::GroupState::~GroupState() = default;

// MemoryCoordinatorPolicyManager ----------------------------------------------

MemoryCoordinatorPolicyManager::MemoryCoordinatorPolicyManager() = default;

MemoryCoordinatorPolicyManager::~MemoryCoordinatorPolicyManager() = default;

void MemoryCoordinatorPolicyManager::AddPolicy(
    MemoryCoordinatorPolicy* policy) {
  auto [_, inserted] = policies_.insert(policy);
  CHECK(inserted);

  for (auto const& [key, group_state] : groups_) {
    policy->OnConsumerGroupAdded(std::get<0>(key), group_state->traits,
                                 group_state->process_type, std::get<1>(key));
  }
}

void MemoryCoordinatorPolicyManager::RemovePolicy(
    MemoryCoordinatorPolicy* policy) {
  size_t removed = policies_.erase(policy);
  CHECK_EQ(removed, 1u);

  for (auto const& [key, group_state] : groups_) {
    if (group_state->requested_limits.erase(policy)) {
      RecomputeMemoryLimit(*group_state);
    }
  }
}

void MemoryCoordinatorPolicyManager::OnConsumerGroupAdded(
    std::string_view consumer_id,
    base::MemoryConsumerTraits traits,
    ProcessType process_type,
    ChildProcessId child_process_id,
    base::RegisteredMemoryConsumer consumer) {
  auto [_, inserted] = groups_.try_emplace(
      ConsumerGroupKey(std::string(consumer_id), child_process_id),
      std::make_unique<GroupState>(traits, process_type, consumer));
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

  size_t removed = groups_.erase(
      ConsumerGroupKey(std::string(consumer_id), child_process_id));
  CHECK_EQ(removed, 1u);
}

void MemoryCoordinatorPolicyManager::SetMemoryLimit(
    MemoryCoordinatorPolicy* policy,
    std::string_view consumer_id,
    ChildProcessId child_process_id,
    int percentage) {
  auto it = groups_.find(
      ConsumerGroupKey(std::string(consumer_id), child_process_id));
  CHECK(it != groups_.end());
  GroupState& group_state = *it->second;

  group_state.requested_limits[policy] = percentage;
  RecomputeMemoryLimit(group_state);
}

void MemoryCoordinatorPolicyManager::ReleaseMemory(
    std::string_view consumer_id,
    ChildProcessId child_process_id) {
  auto it = groups_.find(
      ConsumerGroupKey(std::string(consumer_id), child_process_id));
  CHECK(it != groups_.end());
  GroupState& group_state = *it->second;

  group_state.consumer.ReleaseMemory();
}

void MemoryCoordinatorPolicyManager::RecomputeMemoryLimit(
    GroupState& group_state) {
  // Note: This will reset the limit to its default value if all policies are
  // removed.
  int min_limit = base::MemoryConsumer::kDefaultMemoryLimit;
  for (auto const& [policy, limit] : group_state.requested_limits) {
    min_limit = std::min(min_limit, limit);
  }

  group_state.consumer.UpdateMemoryLimit(min_limit);
}

}  // namespace content
