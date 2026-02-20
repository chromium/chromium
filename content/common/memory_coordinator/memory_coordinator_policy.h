// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_COORDINATOR_POLICY_H_
#define CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_COORDINATOR_POLICY_H_

#include <type_traits>

#include "base/memory/raw_ref.h"
#include "content/common/content_export.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"

namespace content {

class MemoryCoordinatorPolicyManager;

// An interface for implementing memory management policies.
//
// A MemoryCoordinatorPolicy listens to signals (e.g. from the browser or from
// memory pressure listeners) and can apply specific memory management logic
// (e.g., setting memory limits) through the associated
// MemoryCoordinatorPolicyManager.
//
// If an implementation needs to track individual memory consumer groups (e.g.
// to maintain per-group state), it should also implement
// MemoryCoordinatorPolicyManager::Observer.
//
// For example, a policy might be implemented to reduce the memory footprint of
// backgrounded renderers or to respond to system-level memory pressure events.
//
// Implementations should be registered with the manager using
// MemoryCoordinatorPolicyRegistration.
class CONTENT_EXPORT MemoryCoordinatorPolicy {
 public:
  virtual ~MemoryCoordinatorPolicy() = default;

 protected:
  explicit MemoryCoordinatorPolicy(MemoryCoordinatorPolicyManager& manager);

  // TODO(pmonette): Move the UpdateConsumers API here and make it private in
  // MemoryCoordinatorPolicyManager.
  MemoryCoordinatorPolicyManager& manager() { return manager_.get(); }

 private:
  const raw_ref<MemoryCoordinatorPolicyManager> manager_;
};

namespace internal {

// Note: The registration object is split into a base class to reduce code bloat
// caused by template instantiation.
class CONTENT_EXPORT MemoryCoordinatorPolicyRegistrationInternal {
 public:
  MemoryCoordinatorPolicyRegistrationInternal(
      MemoryCoordinatorPolicyManager& manager,
      MemoryCoordinatorPolicy* policy,
      MemoryCoordinatorPolicyManager::Observer* observer);
  ~MemoryCoordinatorPolicyRegistrationInternal();

  MemoryCoordinatorPolicyRegistrationInternal(
      const MemoryCoordinatorPolicyRegistrationInternal&) = delete;
  MemoryCoordinatorPolicyRegistrationInternal& operator=(
      const MemoryCoordinatorPolicyRegistrationInternal&) = delete;

 private:
  const raw_ref<MemoryCoordinatorPolicyManager> manager_;
  const raw_ptr<MemoryCoordinatorPolicy> policy_;
  const raw_ptr<MemoryCoordinatorPolicyManager::Observer> observer_;
};

}  // namespace internal

// Scoped registration helper for MemoryCoordinatorPolicy.
//
// This object handles the registration of a policy with its manager. It is
// interface-aware: if the policy also implements the
// MemoryCoordinatorPolicyManager::Observer interface, this helper will
// automatically register it as an observer to receive consumer lifecycle
// notifications.
//
// The policy is unregistered when this helper is destroyed. This is the
// recommended way to manage policy registration as it ensures that both the
// policy and its optional observer part are registered and unregistered
// correctly and consistently.
template <typename T>
class MemoryCoordinatorPolicyRegistration {
 public:
  MemoryCoordinatorPolicyRegistration(MemoryCoordinatorPolicyManager& manager,
                                      T& policy)
      : impl_(manager, &policy, AsObserver(&policy)) {}

 private:
  // Overload resolution to detect Observer interface at compile time.
  static MemoryCoordinatorPolicyManager::Observer* AsObserver(
      MemoryCoordinatorPolicyManager::Observer* observer) {
    return observer;
  }
  static MemoryCoordinatorPolicyManager::Observer* AsObserver(...) {
    return nullptr;
  }

  internal::MemoryCoordinatorPolicyRegistrationInternal impl_;
};

}  // namespace content

#endif  // CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_COORDINATOR_POLICY_H_
