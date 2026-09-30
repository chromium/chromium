// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/memory_coordinator/memory_coordinator_policy.h"

#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"

namespace content {

MemoryCoordinatorPolicy::MemoryCoordinatorPolicy(
    MemoryCoordinatorPolicyManager& manager)
    : manager_(manager) {}

MemoryCoordinatorPolicyRegistration::MemoryCoordinatorPolicyRegistration(
    MemoryCoordinatorPolicyManager& manager,
    MemoryCoordinatorPolicy& policy)
    : manager_(manager), policy_(policy) {
  manager_->AddPolicy(&policy_.get());
}

MemoryCoordinatorPolicyRegistration::~MemoryCoordinatorPolicyRegistration() {
  manager_->RemovePolicy(&policy_.get());
}

}  // namespace content
