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

// Constructs a DOMPaintTimingInfo with the current time and calls
// ContainerTiming::OnElementPainted(). Shared by unit tests and perf tests.
inline void SimulateContainerTimingPaint(ContainerTiming& container_timing,
                                         Element* element,
                                         const gfx::RectF& rect) {
  DOMPaintTimingInfo paint_info{
      .paint_time = static_cast<DOMHighResTimeStamp>(
          base::TimeTicks::Now().since_origin().InMillisecondsF()),
      .presentation_time = static_cast<DOMHighResTimeStamp>(
          base::TimeTicks::Now().since_origin().InMillisecondsF())};
  container_timing.OnElementPainted(paint_info, element, rect);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_CONTAINER_TIMING_TEST_UTILS_H_
