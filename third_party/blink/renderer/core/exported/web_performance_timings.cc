// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_performance_timings.h"

#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/core/timing/performance_navigation_timing.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"

namespace blink {

WebPerformanceTimings::WebPerformanceTimings(WindowPerformance* performance) {
  if (!performance) {
    return;
  }
  PerformanceNavigationTiming* nav_timing = performance->NavigationTiming();
  PerformanceEntry* first_paint_entry = performance->FirstPaintTiming();
  navigation_start = performance->timeOrigin();
  response_start = nav_timing ? nav_timing->responseStart() : 0.0;
  dom_content_loaded_event_end =
      nav_timing ? nav_timing->domContentLoadedEventEnd() : 0.0;
  load_event_end = nav_timing ? nav_timing->loadEventEnd() : 0.0;
  first_paint = first_paint_entry ? first_paint_entry->startTime() : 0.0;
  now = performance->now();
}

}  // namespace blink
