// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/memory_coordinator_browsertest_util.h"

#include "base/hash/hash.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"
#include "content/public/browser/browser_memory_coordinator.h"

namespace content::test {

ScopedMemoryLimitOverride::ScopedMemoryLimitOverride(
    std::string_view consumer_name)
    : consumer_id_(base::PersistentHash(consumer_name)) {}

ScopedMemoryLimitOverride::~ScopedMemoryLimitOverride() {
  ClearLimit();
}

void ScopedMemoryLimitOverride::SetLimit(base::MemoryLimit memory_limit) {
  BrowserMemoryCoordinator::Get().policy_manager().SetMemoryLimitOverride(
      consumer_id_, memory_limit);
  limit_ = memory_limit;
}

void ScopedMemoryLimitOverride::ClearLimit() {
  if (limit_.has_value()) {
    BrowserMemoryCoordinator::Get().policy_manager().ClearMemoryLimitOverride(
        consumer_id_);
    limit_.reset();
  }
}

void ScopedMemoryLimitOverride::NotifyReleaseMemory() {
  BrowserMemoryCoordinator::Get()
      .policy_manager()
      .NotifyReleaseMemoryForTesting(consumer_id_);
}

}  // namespace content::test
