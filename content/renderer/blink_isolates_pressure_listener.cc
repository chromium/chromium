// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/blink_isolates_pressure_listener.h"

#include "base/feature_list.h"
#include "content/common/buildflags.h"
// #include "build/build_config.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/web/blink.h"
#include "v8/include/v8-isolate.h"

namespace content {

BlinkIsolatesPressureListener::BlinkIsolatesPressureListener()
    : memory_pressure_listener_registration_(
          FROM_HERE,
          base::MemoryPressureListenerTag::kBlinkIsolatesPressureListener,
          this) {}

BlinkIsolatesPressureListener::~BlinkIsolatesPressureListener() = default;

void BlinkIsolatesPressureListener::OnMemoryPressure(
    base::MemoryPressureLevel level) {
  v8::MemoryPressureLevel v8_memory_pressure_level =
      static_cast<v8::MemoryPressureLevel>(level);

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
