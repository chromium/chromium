// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/memory_reclaimer_pressure_listener.h"

#include "base/memory_coordinator/traits.h"
#include "base/memory_coordinator/utils.h"
#include "partition_alloc/memory_reclaimer.h"

namespace content {

namespace {

constexpr base::MemoryConsumerTraits kMemoryReclaimerTraits{
    // Reclaims thread caches (megabytes).
    base::MemoryConsumerTraits::EstimatedMemoryUsage::kMedium,
    // Purging walks free lists, which are stored in the memory being released.
    base::MemoryConsumerTraits::ReleaseMemoryDataAccess::kReads,
    // Frees unused pages back to OS.
    base::MemoryConsumerTraits::InformationRetention::kLossless,
    // Tasks are posted to the main thread runner.
    base::MemoryConsumerTraits::ExecutionType::kAsynchronous,
    // Reacts to a binary threshold. No specified limit.
    base::MemoryConsumerTraits::SupportsMemoryLimit::kNo,
    // One-shot reclamation.
    base::MemoryConsumerTraits::IsStateful::kNo};

}  // namespace

MemoryReclaimerPressureListener::MemoryReclaimerPressureListener()
    : memory_consumer_registration_(
          "MemoryReclaimerPressureListener",
          kMemoryReclaimerTraits,
          this,
          base::AsyncMemoryConsumerRegistration::CheckUnregister::kDisabled) {}

MemoryReclaimerPressureListener::~MemoryReclaimerPressureListener() = default;

void MemoryReclaimerPressureListener::OnUpdateMemoryLimit() {}

void MemoryReclaimerPressureListener::OnReleaseMemory() {
  if (memory_limit() <= base::MemoryLimit::ModeratePressureThreshold()) {
    ::partition_alloc::MemoryReclaimer::Instance()->ReclaimAll();
  }
}

}  // namespace content
