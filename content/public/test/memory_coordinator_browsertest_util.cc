// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/memory_coordinator_browsertest_util.h"

#include "content/browser/memory_coordinator/browser_memory_coordinator.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"

namespace content::test {

ScopedMemoryLimitOverride::ScopedMemoryLimitOverride(
    std::string_view consumer_name)
    : consumer_name_(consumer_name) {}

ScopedMemoryLimitOverride::~ScopedMemoryLimitOverride() {
  ClearLimit();
}

void ScopedMemoryLimitOverride::SetLimit(int percentage) {
  if (!limit_.has_value()) {
    BrowserMemoryCoordinator::Get()
        .policy_manager_for_testing()
        .AddMemoryLimitOverrideForTesting(consumer_name_, percentage);
  } else {
    BrowserMemoryCoordinator::Get()
        .policy_manager_for_testing()
        .UpdateMemoryLimitOverrideForTesting(consumer_name_, percentage);
  }
  limit_ = percentage;
}

void ScopedMemoryLimitOverride::ClearLimit() {
  if (limit_.has_value()) {
    BrowserMemoryCoordinator::Get()
        .policy_manager_for_testing()
        .ClearMemoryLimitOverrideForTesting(consumer_name_);
    limit_.reset();
  }
}

void ScopedMemoryLimitOverride::NotifyReleaseMemory() {
  BrowserMemoryCoordinator::Get()
      .policy_manager_for_testing()
      .NotifyReleaseMemoryForTesting(consumer_name_);
}

}  // namespace content::test
