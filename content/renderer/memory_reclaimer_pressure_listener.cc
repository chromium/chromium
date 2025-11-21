// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/memory_reclaimer_pressure_listener.h"

#include "partition_alloc/memory_reclaimer.h"

namespace content {

MemoryReclaimerPressureListener::MemoryReclaimerPressureListener()
    : memory_pressure_listener_registration_(
          FROM_HERE,
          base::MemoryPressureListenerTag::kMemoryReclaimerPressureListener,
          this) {}

MemoryReclaimerPressureListener::~MemoryReclaimerPressureListener() = default;

void MemoryReclaimerPressureListener::OnMemoryPressure(
    base::MemoryPressureLevel level) {
  if (level == base::MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }

  ::partition_alloc::MemoryReclaimer::Instance()->ReclaimAll();
}

}  // namespace content
