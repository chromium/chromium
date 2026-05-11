// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/memory_reclaimer_pressure_listener.h"

#include "base/memory_coordinator/utils.h"
#include "partition_alloc/memory_reclaimer.h"

namespace content {

MemoryReclaimerPressureListener::MemoryReclaimerPressureListener()
    : memory_consumer_registration_(
          "MemoryReclaimerPressureListener",
          /*traits=*/std::nullopt,  // TODO(crbug.com/489671163): Fill traits.
          this,
          base::AsyncMemoryConsumerRegistration::CheckUnregister::kDisabled,
          base::AsyncMemoryConsumerRegistration::CheckRegistryExists::
              kDisabled) {}

MemoryReclaimerPressureListener::~MemoryReclaimerPressureListener() = default;

void MemoryReclaimerPressureListener::OnUpdateMemoryLimit() {}

void MemoryReclaimerPressureListener::OnReleaseMemory() {
  if (memory_limit() <= base::kModerateMemoryPressureThreshold) {
    ::partition_alloc::MemoryReclaimer::Instance()->ReclaimAll();
  }
}

}  // namespace content
