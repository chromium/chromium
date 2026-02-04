// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory_coordinator/memory_coordinator_policy.h"

#include "content/browser/memory_coordinator/memory_coordinator_policy_manager.h"

namespace content {

MemoryCoordinatorPolicy::MemoryCoordinatorPolicy(
    MemoryCoordinatorPolicyManager& manager)
    : manager_(manager) {}

}  // namespace content
