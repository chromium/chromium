// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/blink_isolates_pressure_listener.h"

#include "base/feature_list.h"
#include "base/memory_coordinator/traits.h"
#include "base/memory_coordinator/utils.h"
#include "content/common/buildflags.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/web/blink.h"
#include "v8/include/v8-isolate.h"

namespace content {

namespace {

constexpr base::MemoryConsumerTraits kBlinkIsolatesTraits(
    // V8 GC across isolates can free tens or hundreds of MBs.
    base::MemoryConsumerTraits::EstimatedMemoryUsage::kLarge,
    // V8 heap traversal cost.
    base::MemoryConsumerTraits::ReleaseMemoryCost::kRequiresTraversal,
    // Reclaimable V8 objects and caches are lossless.
    base::MemoryConsumerTraits::InformationRetention::kLossless,
    // Asynchronous since AsyncMemoryConsumerRegistration is used.
    base::MemoryConsumerTraits::ExecutionType::kAsynchronous,
    // Supports memory limit.
    base::MemoryConsumerTraits::SupportsMemoryLimit::kYes,
    // Recreating V8 objects is N/A for stateless consumer.
    base::MemoryConsumerTraits::RecreateMemoryCost::kNA,
    // Manages V8 GC references.
    base::MemoryConsumerTraits::ReleaseGCReferences::kYes,
    // Triggers V8 GC across isolates.
    base::MemoryConsumerTraits::GarbageCollectsV8Heap::kYes,
    // Stateless consumer. Performs one-shot notifications.
    base::MemoryConsumerTraits::IsStateful::kNo);

}  // namespace

BlinkIsolatesPressureListener::BlinkIsolatesPressureListener()
    : memory_consumer_registration_(
          "BlinkIsolates",
          kBlinkIsolatesTraits,
          this,
          base::AsyncMemoryConsumerRegistration::CheckUnregister::kDisabled) {}

BlinkIsolatesPressureListener::~BlinkIsolatesPressureListener() = default;

void BlinkIsolatesPressureListener::OnUpdateMemoryLimit() {}

void BlinkIsolatesPressureListener::OnReleaseMemory() {
  v8::MemoryPressureLevel v8_memory_pressure_level =
      v8::MemoryPressureLevel::kNone;
  if (memory_limit() <= base::kCriticalMemoryPressureThreshold) {
    v8_memory_pressure_level = v8::MemoryPressureLevel::kCritical;
  } else if (memory_limit() <= base::kModerateMemoryPressureThreshold) {
    v8_memory_pressure_level = v8::MemoryPressureLevel::kModerate;
  }

#if !BUILDFLAG(ALLOW_CRITICAL_MEMORY_PRESSURE_HANDLING_IN_FOREGROUND)
  // In order to reduce performance impact, translate critical level to
  // moderate level for foreground renderer.
  if (is_renderer_visible_ &&
      v8_memory_pressure_level == v8::MemoryPressureLevel::kCritical) {
    v8_memory_pressure_level = v8::MemoryPressureLevel::kModerate;
  }
#endif  // !BUILDFLAG(ALLOW_CRITICAL_MEMORY_PRESSURE_HANDLING_IN_FOREGROUND)

  if (base::FeatureList::IsEnabled(
          features::kForwardMemoryPressureToBlinkIsolates)) {
    blink::MemoryPressureNotificationToAllIsolates(v8_memory_pressure_level);
  }
}

void BlinkIsolatesPressureListener::OnRendererVisible() {
  is_renderer_visible_ = true;
}

void BlinkIsolatesPressureListener::OnRendererHidden() {
  is_renderer_visible_ = false;
}

}  // namespace content
