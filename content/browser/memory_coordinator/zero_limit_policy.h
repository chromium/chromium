// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_COORDINATOR_ZERO_LIMIT_POLICY_H_
#define CONTENT_BROWSER_MEMORY_COORDINATOR_ZERO_LIMIT_POLICY_H_

#include "base/feature_list.h"
#include "content/common/content_export.h"
#include "content/public/common/memory_coordinator/memory_coordinator_policy.h"
#include "content/public/common/memory_coordinator/predicate_memory_coordinator_policy.h"

namespace content {

CONTENT_EXPORT BASE_DECLARE_FEATURE(kMemoryCoordinatorZeroLimit);

// A policy that sets the memory limit of all stateful MemoryConsumers to zero.
// This is used for Finch experimentation to determine the maximum potential
// effect of the MemoryCoordinator on system and process metrics.
class CONTENT_EXPORT ZeroLimitPolicy : public PredicateMemoryCoordinatorPolicy {
 public:
  explicit ZeroLimitPolicy(MemoryCoordinatorPolicyManager& manager);
  ~ZeroLimitPolicy() override;

 private:
  MemoryCoordinatorPolicyRegistration policy_registration_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_COORDINATOR_ZERO_LIMIT_POLICY_H_
