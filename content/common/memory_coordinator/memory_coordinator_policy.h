// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_COORDINATOR_POLICY_H_
#define CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_COORDINATOR_POLICY_H_

#include <string_view>
#include <type_traits>

#include "base/memory/raw_ref.h"
#include "base/memory_coordinator/traits.h"
#include "content/common/content_export.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/process_type.h"

namespace content {

class MemoryCoordinatorPolicyManager;
class MemoryCoordinatorPolicy;

// An interface for implementing memory management policies.
//
// A MemoryCoordinatorPolicy listens to signals (e.g. from the browser or from
// memory pressure listeners) and can apply specific memory management logic
// (e.g., setting memory limits) through the associated
// MemoryCoordinatorPolicyManager.
//
// For example, a policy might be implemented to reduce the memory footprint of
// backgrounded renderers or to respond to system-level memory pressure events.
class CONTENT_EXPORT MemoryCoordinatorPolicy {
 public:
  virtual ~MemoryCoordinatorPolicy() = default;

  // Called when a new consumer group is added/removed.
  virtual void OnConsumerGroupAdded(uint32_t consumer_id,
                                    std::string_view consumer_name,
                                    base::MemoryConsumerTraits traits,
                                    ProcessType process_type,
                                    ChildProcessId child_process_id) = 0;
  virtual void OnConsumerGroupRemoved(uint32_t consumer_id,
                                      ChildProcessId child_process_id) = 0;

 protected:
  explicit MemoryCoordinatorPolicy(MemoryCoordinatorPolicyManager& manager);

  // TODO(pmonette): Move the UpdateConsumers API here and make it private in
  // MemoryCoordinatorPolicyManager.
  MemoryCoordinatorPolicyManager& manager() { return manager_.get(); }

 private:
  const raw_ref<MemoryCoordinatorPolicyManager> manager_;
};

// Scoped registration helper for MemoryCoordinatorPolicy.
//
// Automatically registers the policy with the manager on construction,
// and unregisters it on destruction.
class CONTENT_EXPORT MemoryCoordinatorPolicyRegistration {
 public:
  MemoryCoordinatorPolicyRegistration(MemoryCoordinatorPolicyManager& manager,
                                      MemoryCoordinatorPolicy& policy);

  MemoryCoordinatorPolicyRegistration(
      const MemoryCoordinatorPolicyRegistration&) = delete;
  MemoryCoordinatorPolicyRegistration& operator=(
      const MemoryCoordinatorPolicyRegistration&) = delete;

  ~MemoryCoordinatorPolicyRegistration();

 private:
  const raw_ref<MemoryCoordinatorPolicyManager> manager_;
  const raw_ref<MemoryCoordinatorPolicy> policy_;
};

}  // namespace content

#endif  // CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_COORDINATOR_POLICY_H_
