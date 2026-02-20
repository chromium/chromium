// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/memory_coordinator_policy.h"

#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"

namespace content {

MemoryCoordinatorPolicy::MemoryCoordinatorPolicy(
    MemoryCoordinatorPolicyManager& manager)
    : manager_(manager) {}

namespace internal {

MemoryCoordinatorPolicyRegistrationInternal::
    MemoryCoordinatorPolicyRegistrationInternal(
        MemoryCoordinatorPolicyManager& manager,
        MemoryCoordinatorPolicy* policy,
        MemoryCoordinatorPolicyManager::Observer* observer)
    : manager_(manager), policy_(policy), observer_(observer) {
  manager_->AddPolicy(policy_);
  if (observer_) {
    manager_->AddObserver(observer_);
  }
}

MemoryCoordinatorPolicyRegistrationInternal::
    ~MemoryCoordinatorPolicyRegistrationInternal() {
  if (observer_) {
    manager_->RemoveObserver(observer_);
  }
  manager_->RemovePolicy(policy_);
}

}  // namespace internal

}  // namespace content
