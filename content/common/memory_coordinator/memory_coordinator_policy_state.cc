// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/memory_coordinator_policy_state.h"

#include <optional>
#include <string_view>

#include "base/memory_coordinator/memory_consumer.h"
#include "content/common/memory_coordinator/memory_coordinator_policy.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"

namespace content {

MemoryCoordinatorPolicyState::MemoryCoordinatorPolicyState(
    MemoryCoordinatorPolicy& policy,
    MemoryCoordinatorPolicyManager& manager,
    ConsumerPredicate predicate)
    : policy_(policy), manager_(manager), predicate_(predicate) {
  manager_->AddObserver(this);
}

MemoryCoordinatorPolicyState::~MemoryCoordinatorPolicyState() {
  manager_->RemoveObserver(this);
}

void MemoryCoordinatorPolicyState::OnConsumerGroupAdded(
    uint32_t consumer_id,
    std::string_view consumer_name,
    std::optional<base::MemoryConsumerTraits> traits,
    ProcessType process_type,
    ChildProcessId child_process_id) {
  if (predicate_.Run(consumer_id, traits, process_type, child_process_id)) {
    // Only update if the limit is not the default or if memory release is
    // requested.
    if (percentage_ != base::MemoryConsumer::kDefaultMemoryLimit ||
        release_memory_) {
      manager_->UpdateConsumers(
          &*policy_,
          {GlobalMemoryConsumerUpdate{
              child_process_id, {consumer_id, percentage_, release_memory_}}});
    }
  }
}

void MemoryCoordinatorPolicyState::OnConsumerGroupRemoved(
    uint32_t consumer_id,
    ChildProcessId child_process_id) {}

void MemoryCoordinatorPolicyState::SetLimit(int percentage,
                                            bool release_memory) {
  if (percentage == percentage_ && release_memory == release_memory_) {
    // If this is a repeated request to release memory, and we are actually
    // under pressure (limit < 100%), trigger a repeated release for stateless
    // consumers and consumers without defined traits.
    if (release_memory &&
        percentage < base::MemoryConsumer::kDefaultMemoryLimit) {
      TriggerRepeatedRelease();
    }
    return;
  }

  percentage_ = percentage;
  release_memory_ = release_memory;

  manager_->UpdateConsumers(
      &*policy_,
      [this](uint32_t consumer_id,
             std::optional<base::MemoryConsumerTraits> traits,
             ProcessType process_type, ChildProcessId child_process_id) {
        return predicate_.Run(consumer_id, traits, process_type,
                              child_process_id);
      },
      percentage_, release_memory_);
}

void MemoryCoordinatorPolicyState::TriggerRepeatedRelease() {
  manager_->UpdateConsumers(
      &*policy_,
      [this](uint32_t consumer_id,
             std::optional<base::MemoryConsumerTraits> traits,
             ProcessType process_type, ChildProcessId child_process_id) {
        // Don't repeat the signal for consumers that don't match the state's
        // predicate.
        if (!predicate_.Run(consumer_id, traits, process_type,
                            child_process_id)) {
          return false;
        }

        // Don't repeat the signal for stateful consumers. They are trusted to
        // self-regulate once they receive the limit.
        if (traits.has_value() &&
            traits->is_stateful ==
                base::MemoryConsumerTraits::IsStateful::kYes) {
          return false;
        }

        return true;
      },
      std::nullopt, true);
}

}  // namespace content
