// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/skia_graphics_pressure_listener.h"

#include "third_party/skia/include/core/SkGraphics.h"

namespace content {

SkiaGraphicsPressureListener::SkiaGraphicsPressureListener()
    : memory_pressure_listener_registration_(
          FROM_HERE,
          base::MemoryPressureListenerTag::kSkiaGraphicsPressureListener,
          this) {}

SkiaGraphicsPressureListener::~SkiaGraphicsPressureListener() = default;

void SkiaGraphicsPressureListener::OnMemoryPressure(
    base::MemoryPressureLevel level) {
  if (level == base::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    SkGraphics::PurgeAllCaches();
  }
}

}  // namespace content
