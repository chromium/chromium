// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/skia_graphics_pressure_listener.h"

#include "base/memory_coordinator/utils.h"
#include "third_party/skia/include/core/SkGraphics.h"

namespace content {

SkiaGraphicsPressureListener::SkiaGraphicsPressureListener()
    : memory_consumer_registration_(
          "SkiaGraphics",
          /*traits=*/std::nullopt,  // TODO(crbug.com/489671163): Fill traits.
          this,
          base::AsyncMemoryConsumerRegistration::CheckUnregister::kDisabled,
          base::AsyncMemoryConsumerRegistration::CheckRegistryExists::
              kDisabled) {}

SkiaGraphicsPressureListener::~SkiaGraphicsPressureListener() = default;

void SkiaGraphicsPressureListener::OnUpdateMemoryLimit() {}

void SkiaGraphicsPressureListener::OnReleaseMemory() {
  if (memory_limit() <= base::kCriticalMemoryPressureThreshold) {
    SkGraphics::PurgeAllCaches();
  }
}

}  // namespace content
