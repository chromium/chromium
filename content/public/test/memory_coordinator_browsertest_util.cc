// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/memory_coordinator_browsertest_util.h"

#include "base/hash/hash.h"
#include "content/browser/memory_coordinator/browser_memory_coordinator.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"

namespace content::test {

ScopedMemoryLimitOverride::ScopedMemoryLimitOverride(
    std::string_view consumer_name)
    : consumer_id_(base::PersistentHash(consumer_name)) {}

ScopedMemoryLimitOverride::~ScopedMemoryLimitOverride() {
  ClearLimit();
}

void ScopedMemoryLimitOverride::SetLimit(int percentage) {
  BrowserMemoryCoordinator::Get()
      .policy_manager_for_testing()
      .SetMemoryLimitOverride(consumer_id_, percentage);
  limit_ = percentage;
}

void ScopedMemoryLimitOverride::ClearLimit() {
  if (limit_.has_value()) {
    BrowserMemoryCoordinator::Get()
        .policy_manager_for_testing()
        .ClearMemoryLimitOverride(consumer_id_);
    limit_.reset();
  }
}

void ScopedMemoryLimitOverride::NotifyReleaseMemory() {
  BrowserMemoryCoordinator::Get()
      .policy_manager_for_testing()
      .NotifyReleaseMemoryForTesting(consumer_id_);
}

}  // namespace content::test
