// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEMORY_COORDINATOR_PREDICATE_MEMORY_COORDINATOR_POLICY_H_
#define CONTENT_COMMON_MEMORY_COORDINATOR_PREDICATE_MEMORY_COORDINATOR_POLICY_H_

#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/traits.h"
#include "content/common/content_export.h"
#include "content/common/memory_coordinator/memory_coordinator_policy.h"

namespace content {

class MemoryCoordinatorPolicyManager;

// A base class for memory coordinator policies that apply memory limits or
// release requests to all memory consumers that match a single predicate.
//
// This base class manages the lifecycle of these "predicate-based" rules. When
// a limit is set via SetLimit(), it is automatically applied to all currently
// registered consumers that match the predicate. It also observes the
// registration of new consumers and ensures the rule is applied if they match
// the predicate.
//
// This allows policies to implement "persistent" settings (e.g., "all
// renderers should be capped at 50%") without manually tracking every consumer
// registration.
class CONTENT_EXPORT PredicateMemoryCoordinatorPolicy
    : public MemoryCoordinatorPolicy {
 public:
  using ConsumerPredicate =
      base::RepeatingCallback<bool(uint32_t consumer_id,
                                   base::MemoryConsumerTraits traits,
                                   ProcessType process_type,
                                   ChildProcessId child_process_id)>;

  PredicateMemoryCoordinatorPolicy(MemoryCoordinatorPolicyManager& manager,
                                   ConsumerPredicate predicate);

  PredicateMemoryCoordinatorPolicy(const PredicateMemoryCoordinatorPolicy&) =
      delete;
  PredicateMemoryCoordinatorPolicy& operator=(
      const PredicateMemoryCoordinatorPolicy&) = delete;

  ~PredicateMemoryCoordinatorPolicy() override;

  // MemoryCoordinatorPolicy:
  void OnConsumerGroupAdded(uint32_t consumer_id,
                            std::string_view consumer_name,
                            base::MemoryConsumerTraits traits,
                            ProcessType process_type,
                            ChildProcessId child_process_id) override;
  void OnConsumerGroupRemoved(uint32_t consumer_id,
                              ChildProcessId child_process_id) override;

  // Updates the memory limit and/or release memory request for all consumers
  // matching the predicate. This will also apply to any matching consumer
  // added in the future.
  void SetLimit(int percentage, bool release_memory);

 private:
  const ConsumerPredicate predicate_;
  int percentage_ = base::MemoryConsumer::kDefaultMemoryLimit;
  bool release_memory_ = false;

  void TriggerRepeatedRelease();
};

}  // namespace content

#endif  // CONTENT_COMMON_MEMORY_COORDINATOR_PREDICATE_MEMORY_COORDINATOR_POLICY_H_
