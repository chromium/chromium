// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_CONTAINER_TIMING_TEST_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_CONTAINER_TIMING_TEST_UTILS_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/core/paint/timing/container_timing.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

// Calls ContainerTiming::OnElementPainted(), as paint would. Shared by unit
// tests and perf tests.
inline void SimulateContainerTimingPaint(ContainerTiming& container_timing,
                                         Element* element,
                                         const gfx::RectF& rect) {
  container_timing.OnElementPainted(*element, rect);
}

// Ends the paint and presents the frame with the current time, reporting the
// entries created since the last call.
inline void SimulateContainerTimingPresentation(
    ContainerTiming& container_timing,
    uint64_t navigation_id) {
  OptionalPaintTimingCallback callback =
      container_timing.TakePaintTimingCallback();
  if (!callback) {
    return;
  }
  const base::TimeTicks now = base::TimeTicks::Now();
  const DOMHighResTimeStamp now_ms = now.since_origin().InMillisecondsF();
  std::move(*callback).Run(
      now,
      DOMPaintTimingInfo{.paint_time = now_ms, .presentation_time = now_ms},
      navigation_id);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_CONTAINER_TIMING_TEST_UTILS_H_
