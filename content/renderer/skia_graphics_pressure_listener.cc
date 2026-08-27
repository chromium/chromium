// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/skia_graphics_pressure_listener.h"

#include "base/memory_coordinator/traits.h"
#include "base/memory_coordinator/utils.h"
#include "third_party/skia/include/core/SkGraphics.h"

namespace content {

namespace {

constexpr base::MemoryConsumerTraits kSkiaGraphicsTraits(
    // Skia resource caches typically consume tens of MBs.
    base::MemoryConsumerTraits::EstimatedMemoryUsage::kMedium,
    // Purging requires traversing hash maps/queues.
    base::MemoryConsumerTraits::ReleaseMemoryCost::kRequiresTraversal,
    // Resources can be regenerated.
    base::MemoryConsumerTraits::InformationRetention::kLossless,
    // Asynchronous since AsyncMemoryConsumerRegistration is used.
    base::MemoryConsumerTraits::ExecutionType::kAsynchronous,
    // Binary check; either keeps or drops everything.
    base::MemoryConsumerTraits::SupportsMemoryLimit::kNo,
    // Regenerating the resources requires CPU-intensive computations.
    base::MemoryConsumerTraits::RecreateMemoryCost::kExpensive,
    // Stateless consumer. Performs one-shot evictions.
    base::MemoryConsumerTraits::IsStateful::kNo);

}  // namespace

SkiaGraphicsPressureListener::SkiaGraphicsPressureListener()
    : memory_consumer_registration_(
          "SkiaGraphics",
          kSkiaGraphicsTraits,
          this,
          base::AsyncMemoryConsumerRegistration::CheckUnregister::kDisabled) {}

SkiaGraphicsPressureListener::~SkiaGraphicsPressureListener() = default;

void SkiaGraphicsPressureListener::OnUpdateMemoryLimit() {}

void SkiaGraphicsPressureListener::OnReleaseMemory() {
  if (memory_limit() <= base::MemoryLimit::CriticalPressureThreshold()) {
    SkGraphics::PurgeAllCaches();
  }
}

}  // namespace content
