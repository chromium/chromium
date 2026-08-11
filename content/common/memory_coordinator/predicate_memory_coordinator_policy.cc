// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/predicate_memory_coordinator_policy.h"

#include <utility>

#include "base/feature_list.h"
#include "base/memory_coordinator/memory_coordinator_features.h"
#include "content/common/memory_coordinator/memory_coordinator_policy.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"

namespace content {

PredicateMemoryCoordinatorPolicy::PredicateMemoryCoordinatorPolicy(
    MemoryCoordinatorPolicyManager& manager,
    ConsumerPredicate predicate)
    : MemoryCoordinatorPolicy(manager), predicate_(std::move(predicate)) {}

PredicateMemoryCoordinatorPolicy::~PredicateMemoryCoordinatorPolicy() = default;

void PredicateMemoryCoordinatorPolicy::OnConsumerGroupAdded(
    uint32_t consumer_id,
    std::string_view consumer_name,
    base::MemoryConsumerTraits traits,
    ProcessType process_type,
    ChildProcessId child_process_id) {
  if (predicate_.Run(consumer_id, traits, process_type, child_process_id)) {
    // Only update if the limit is not the default or if memory release is
    // requested.
    if (percentage_ != base::MemoryConsumer::kDefaultMemoryLimit ||
        release_memory_) {
      manager().UpdateConsumers(
          this,
          {GlobalMemoryConsumerUpdate{
              child_process_id, {consumer_id, percentage_, release_memory_}}});
    }
  }
}

void PredicateMemoryCoordinatorPolicy::OnConsumerGroupRemoved(
    uint32_t consumer_id,
    ChildProcessId child_process_id) {}

void PredicateMemoryCoordinatorPolicy::SetLimit(int percentage,
                                                bool release_memory) {
  if (percentage == percentage_ && release_memory == release_memory_) {
    // If this is a repeated request to release memory, and we are actually
    // under pressure (limit < 100%), trigger a repeated release for stateless
    // consumers.
    if (release_memory &&
        percentage < base::MemoryConsumer::kDefaultMemoryLimit) {
      TriggerRepeatedRelease();
    }
    return;
  }

  percentage_ = percentage;
  release_memory_ = release_memory;

  manager().UpdateConsumers(
      this,
      [this](uint32_t consumer_id, base::MemoryConsumerTraits traits,
             ProcessType process_type, ChildProcessId child_process_id) {
        return predicate_.Run(consumer_id, traits, process_type,
                              child_process_id);
      },
      percentage_, release_memory_);
}

void PredicateMemoryCoordinatorPolicy::TriggerRepeatedRelease() {
  manager().UpdateConsumers(
      this,
      [this](uint32_t consumer_id, base::MemoryConsumerTraits traits,
             ProcessType process_type, ChildProcessId child_process_id) {
        // Don't repeat the signal for consumers that don't match the policy's
        // predicate.
        if (!predicate_.Run(consumer_id, traits, process_type,
                            child_process_id)) {
          return false;
        }

        // Don't repeat the signal for stateful consumers. They are trusted to
        // self-regulate once they receive the limit. However, if the
        // kStatefulMemoryPressure feature is disabled, consumers operate in
        // stateless mode regardless of their declared traits, so they still
        // require repeated signals.
        if (base::FeatureList::IsEnabled(base::kStatefulMemoryPressure) &&
            traits.is_stateful ==
                base::MemoryConsumerTraits::IsStateful::kYes) {
          return false;
        }

        return true;
      },
      std::nullopt, true);
}

}  // namespace content
