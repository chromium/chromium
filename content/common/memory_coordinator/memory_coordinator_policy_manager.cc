// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/memory_coordinator/memory_coordinator_features.h"
#include "base/memory_coordinator/traits.h"
#include "content/common/memory_coordinator/memory_consumer_group_host.h"
#include "content/public/common/memory_coordinator/memory_coordinator_policy.h"

namespace content {

bool IsEffectivelyStateful(const base::MemoryConsumerTraits& traits) {
  return base::FeatureList::IsEnabled(base::kStatefulMemoryPressure) &&
         traits.is_stateful == base::MemoryConsumerTraits::IsStateful::kYes;
}

// MemoryCoordinatorPolicyManager::GroupState ----------------------------------

MemoryCoordinatorPolicyManager::GroupState::GroupState(
    std::string_view consumer_name,
    base::MemoryConsumerTraits traits)
    : consumer_name_(consumer_name), traits_(traits) {}

MemoryCoordinatorPolicyManager::GroupState::~GroupState() = default;

std::optional<base::MemoryLimit>
MemoryCoordinatorPolicyManager::GroupState::SetMemoryLimitForPolicy(
    MemoryCoordinatorPolicy* policy,
    base::MemoryLimit memory_limit) {
  // Get the previous requested limit for this policy.
  auto it = requested_limits_.find(policy);
  const base::MemoryLimit old_policy_limit = (it != requested_limits_.end())
                                                 ? it->second
                                                 : base::MemoryLimit::Default();

  // Early exit if it didn't change.
  if (memory_limit == old_policy_limit) {
    return std::nullopt;
  }

  // Update the map, keeping it small by removing default entries.
  if (memory_limit == base::MemoryLimit::Default()) {
    DCHECK(it != requested_limits_.end());
    requested_limits_.erase(it);
  } else {
    requested_limits_[policy] = memory_limit;
  }

  // Recompute the aggregate limit across all policies. If the result is the
  // same as the current limit, no update is needed.
  base::MemoryLimit new_limit = RecomputeMemoryLimit();
  if (new_limit == current_limit_) {
    return std::nullopt;
  }
  SetCurrentLimit(new_limit);
  return new_limit;
}

std::optional<base::MemoryLimit>
MemoryCoordinatorPolicyManager::GroupState::SetOverrideLimit(
    std::optional<base::MemoryLimit> memory_limit) {
  if (override_limit_ == memory_limit) {
    return std::nullopt;
  }
  override_limit_ = memory_limit;
  base::MemoryLimit new_limit = RecomputeMemoryLimit();
  if (new_limit == current_limit_) {
    return std::nullopt;
  }
  SetCurrentLimit(new_limit);
  return new_limit;
}

void MemoryCoordinatorPolicyManager::GroupState::SetCurrentLimit(
    base::MemoryLimit memory_limit) {
  current_limit_ = memory_limit;
  peak_limit_since_release_ =
      std::max(peak_limit_since_release_, current_limit_);
}

base::MemoryLimit
MemoryCoordinatorPolicyManager::GroupState::RecomputeMemoryLimit() const {
  if (override_limit_) {
    return *override_limit_;
  }

  // The aggregate limit is the product of all policy limits.
  // For example, if policy A requests 80% and policy B requests 50%, the
  // aggregate limit is 40% (0.8 * 0.5 = 0.4).
  double result = base::MemoryLimit::Default().percent();
  for (auto const& [policy, limit] : requested_limits_) {
    result *= limit.ratio();
  }
  return base::MemoryLimit::FromPercent(
      static_cast<int>(std::nearbyint(result)));
}

void MemoryCoordinatorPolicyManager::GroupState::SetMemoryReleaseSuppression(
    MemoryCoordinatorPolicy* policy) {
  memory_release_suppressors_.insert(policy);
}

bool MemoryCoordinatorPolicyManager::GroupState::ClearMemoryReleaseSuppression(
    MemoryCoordinatorPolicy* policy) {
  if (!memory_release_suppressors_.erase(policy) ||
      !memory_release_suppressors_.empty()) {
    return false;
  }
  if (!std::exchange(has_pending_release_, false)) {
    return false;
  }
  // The release is only needed if the consumer may be using more memory than
  // its current limit allows. A stateful consumer can't be above the highest
  // limit applied since it last released memory. A stateless consumer ignores
  // limit updates, so it may be above its limit whenever under pressure.
  return IsEffectivelyStateful(traits_)
             ? current_limit_ < peak_limit_since_release_
             : current_limit_ < base::MemoryLimit::Default();
}

void MemoryCoordinatorPolicyManager::GroupState::OnReleaseSent() {
  peak_limit_since_release_ = current_limit_;
}

// MemoryCoordinatorPolicyManager::HostState -----------------------------------

MemoryCoordinatorPolicyManager::HostState::HostState(
    MemoryConsumerGroupHost* host,
    ProcessType process_type)
    : host(host), process_type(process_type) {}

MemoryCoordinatorPolicyManager::HostState::~HostState() {
  CHECK(groups.empty());
}

// MemoryCoordinatorPolicyManager ----------------------------------------------

MemoryCoordinatorPolicyManager::MemoryCoordinatorPolicyManager() = default;

MemoryCoordinatorPolicyManager::~MemoryCoordinatorPolicyManager() = default;

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
  CHECK(!is_notifying_);
  auto [_, inserted] = policies_.insert(policy);
  CHECK(inserted);

  base::AutoReset<bool> reset(&is_notifying_, true);
  // Catch up the new policy with existing groups.
  for (auto const& [child_id, host_state] : hosts_) {
    for (auto const& [consumer_id, group_state] : host_state->groups) {
      policy->OnConsumerGroupAdded(consumer_id, group_state->consumer_name(),
                                   group_state->traits(),
                                   host_state->process_type, child_id);
    }
  }
}

void MemoryCoordinatorPolicyManager::RemovePolicy(
    MemoryCoordinatorPolicy* policy) {
  CHECK(!is_notifying_);
  size_t removed = policies_.erase(policy);
  CHECK_EQ(removed, 1u);

  const bool was_suppressing_memory_releases =
      memory_release_suppression_filters_.erase(policy) > 0;

  // When a policy is removed, its requested limits and its memory release
  // suppression are cleared from all consumer groups.
  for (auto const& [child_id, host_state] : hosts_) {
    std::vector<MemoryConsumerUpdate> updates;
    updates.reserve(host_state->groups.size());
    for (auto const& [consumer_id, group_state] : host_state->groups) {
      // Setting the default limit clears the policy's requested limit.
      std::optional<base::MemoryLimit> new_limit =
          group_state->SetMemoryLimitForPolicy(policy,
                                               base::MemoryLimit::Default());

      // The limit is updated first so that a dropped release request is only
      // replayed if still needed with the new limit.
      bool release_memory = was_suppressing_memory_releases &&
                            group_state->ClearMemoryReleaseSuppression(policy);
      if (release_memory) {
        group_state->OnReleaseSent();
      }

      if (new_limit || release_memory) {
        updates.push_back({consumer_id, new_limit, release_memory});
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
    ProcessType process_type,
    ChildProcessId child_process_id,
    MemoryConsumerGroupHost* host) {
  auto [_, inserted] = hosts_.try_emplace(
      child_process_id, std::make_unique<HostState>(host, process_type));
  CHECK(inserted);

  for (auto const& [consumer_id, memory_limit] : memory_limit_overrides_) {
    host->SetOverrideLimit(consumer_id, memory_limit);
  }
}

void MemoryCoordinatorPolicyManager::RemoveMemoryConsumerGroupHost(
    ChildProcessId child_process_id) {
  size_t removed = hosts_.erase(child_process_id);
  CHECK_EQ(removed, 1u);
}

void MemoryCoordinatorPolicyManager::OnConsumerGroupAdded(
    uint32_t consumer_id,
    std::string_view consumer_name,
    base::MemoryConsumerTraits traits,
    ChildProcessId child_process_id) {
  HostState& host_state = GetHostState(child_process_id);

  auto [group_it, inserted] = host_state.groups.try_emplace(
      consumer_id, std::make_unique<GroupState>(consumer_name, traits));
  CHECK(inserted);

  // Apply any pending override for this consumer that was set before
  // registration.
  auto it = memory_limit_overrides_.find(consumer_id);
  if (it != memory_limit_overrides_.end()) {
    group_it->second->SetOverrideLimit(it->second);

    // Out-of-process child hosts already received the override during
    // AddMemoryConsumerGroupHost. Only in-process hosts need direct
    // notification when a new consumer group is created.
    if (child_process_id.is_null()) {
      host_state.host->SetOverrideLimit(consumer_id, it->second);
    }
  }

  // Apply the memory release suppression before notifying policies, so that
  // release requests made in response to the notification are suppressed.
  for (auto const& [policy, filter] : memory_release_suppression_filters_) {
    if (filter.Run(consumer_id, consumer_name, traits, host_state.process_type,
                   child_process_id)) {
      group_it->second->SetMemoryReleaseSuppression(policy);
    }
  }

  base::AutoReset<bool> reset(&is_notifying_, true);
  for (MemoryCoordinatorPolicy* policy : policies_) {
    policy->OnConsumerGroupAdded(consumer_id, consumer_name, traits,
                                 host_state.process_type, child_process_id);
  }
}

void MemoryCoordinatorPolicyManager::OnConsumerGroupRemoved(
    uint32_t consumer_id,
    ChildProcessId child_process_id) {
  base::AutoReset<bool> reset(&is_notifying_, true);
  for (MemoryCoordinatorPolicy* policy : policies_) {
    policy->OnConsumerGroupRemoved(consumer_id, child_process_id);
  }

  HostState& host_state = GetHostState(child_process_id);

  size_t removed = host_state.groups.erase(consumer_id);
  CHECK_EQ(removed, 1u);
}

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
void MemoryCoordinatorPolicyManager::OnMemoryLimitChanged(
    uint32_t consumer_id,
    ChildProcessId child_process_id,
    base::MemoryLimit memory_limit) {
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
    ConsumerFilterRef filter,
    std::optional<base::MemoryLimit> memory_limit,
    bool release_memory) {
  for (auto const& [child_id, host_state] : hosts_) {
    std::vector<MemoryConsumerUpdate> updates;
    for (auto const& [consumer_id, group_state] : host_state->groups) {
      if (filter(consumer_id, group_state->consumer_name(),
                 group_state->traits(), host_state->process_type, child_id)) {
        updates.push_back({consumer_id, memory_limit, release_memory});
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

    // Passive consumers can't release memory (their OnReleaseMemory() is a
    // no-op), so don't send them the request.
    if (group_state.traits().consumer_type ==
        base::MemoryConsumerTraits::ConsumerType::kPassive) {
      update.release_memory = false;
    }

    std::optional<base::MemoryLimit> new_effective_limit;
    if (update.memory_limit) {
      new_effective_limit =
          group_state.SetMemoryLimitForPolicy(policy, *update.memory_limit);
    }

    // Release requests are dropped while suppressed for the group, and replayed
    // if needed once it is no longer suppressed.
    if (update.release_memory) {
      if (group_state.IsMemoryReleaseSuppressed()) {
        group_state.SetHasPendingRelease();
        update.release_memory = false;
      } else {
        group_state.OnReleaseSent();
      }
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
    update.memory_limit = new_effective_limit;
    return false;
  });

  if (!updates.empty()) {
    host_state.host->UpdateConsumers(std::move(updates));
  }
}

void MemoryCoordinatorPolicyManager::SetMemoryReleaseSuppression(
    MemoryCoordinatorPolicy* policy,
    ConsumerFilter filter) {
  CHECK(!is_notifying_);
  CHECK(policies_.contains(policy));
  auto [it, _] = memory_release_suppression_filters_.insert_or_assign(
      policy, std::move(filter));
  ApplyMemoryReleaseSuppression(policy, &it->second);
}

void MemoryCoordinatorPolicyManager::ClearMemoryReleaseSuppression(
    MemoryCoordinatorPolicy* policy) {
  CHECK(!is_notifying_);
  if (memory_release_suppression_filters_.erase(policy) == 0) {
    return;
  }
  ApplyMemoryReleaseSuppression(policy, nullptr);
}

void MemoryCoordinatorPolicyManager::ApplyMemoryReleaseSuppression(
    MemoryCoordinatorPolicy* policy,
    const ConsumerFilter* filter) {
  for (auto const& [child_id, host_state] : hosts_) {
    std::vector<MemoryConsumerUpdate> updates;
    for (auto const& [consumer_id, group_state] : host_state->groups) {
      if (filter && filter->Run(consumer_id, group_state->consumer_name(),
                                group_state->traits(), host_state->process_type,
                                child_id)) {
        group_state->SetMemoryReleaseSuppression(policy);
        continue;
      }

      // Release requests made while suppressed were dropped. Ask the group to
      // release memory so that it converges to its aggregate limit.
      if (group_state->ClearMemoryReleaseSuppression(policy)) {
        group_state->OnReleaseSent();
        updates.push_back({consumer_id, std::nullopt, /*release_memory=*/true});
      }
    }
    if (!updates.empty()) {
      host_state->host->UpdateConsumers(std::move(updates));
    }
  }
}

void MemoryCoordinatorPolicyManager::ApplyMemoryLimitOverride(
    uint32_t consumer_id,
    base::MemoryLimit memory_limit) {
  for (auto const& [child_id, host_state] : hosts_) {
    auto it = host_state->groups.find(consumer_id);
    if (it != host_state->groups.end()) {
      it->second->SetOverrideLimit(memory_limit);
    }
    host_state->host->SetOverrideLimit(consumer_id, memory_limit);
  }
}

void MemoryCoordinatorPolicyManager::SetMemoryLimitOverride(
    uint32_t consumer_id,
    base::MemoryLimit memory_limit) {
  memory_limit_overrides_[consumer_id] = memory_limit;

  ApplyMemoryLimitOverride(consumer_id, memory_limit);
}

void MemoryCoordinatorPolicyManager::ClearMemoryLimitOverride(
    uint32_t consumer_id) {
  size_t removed = memory_limit_overrides_.erase(consumer_id);
  CHECK_EQ(removed, 1u);

  for (auto const& [child_id, host_state] : hosts_) {
    auto it = host_state->groups.find(consumer_id);
    base::MemoryLimit policy_limit = base::MemoryLimit::Default();
    if (it != host_state->groups.end()) {
      it->second->SetOverrideLimit(std::nullopt);
      policy_limit = it->second->current_limit();
    }
    host_state->host->ClearOverrideLimit(consumer_id, policy_limit);
  }
}

void MemoryCoordinatorPolicyManager::NotifyReleaseMemoryForTesting(
    uint32_t consumer_id) {
  for (auto const& [child_id, host_state] : hosts_) {
    auto it = host_state->groups.find(consumer_id);
    if (it != host_state->groups.end()) {
      it->second->OnReleaseSent();
      host_state->host->UpdateConsumers({{consumer_id, std::nullopt, true}});
    }
  }
}

}  // namespace content
