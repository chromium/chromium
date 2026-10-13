// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_PERFORMANCE_TIMINGS_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_PERFORMANCE_TIMINGS_H_

#include <stdint.h>

#include "base/time/time.h"
#include "third_party/blink/public/platform/web_common.h"

namespace blink {

class WindowPerformance;

// Web-exposed performance timings in milliseconds, coarsened for security and
// privacy per the High Resolution Time specification.
// `navigation_start` is relative to the Unix epoch (equivalent to
// `performance.timeOrigin`); all other fields are durations relative to
// `navigation_start` (corresponding to `PerformanceNavigationTiming`
// attributes, the `'first-paint'` `PerformancePaintTiming` entry, and
// `performance.now()`).
struct BLINK_EXPORT WebPerformanceTimings {
  double navigation_start = 0.0;
  double response_start = 0.0;
  double dom_content_loaded_event_end = 0.0;
  double load_event_end = 0.0;
  double first_paint = 0.0;
  double now = 0.0;

  base::TimeDelta ToEpochTime() const {
    return base::Milliseconds(static_cast<int64_t>(navigation_start));
  }
  base::TimeDelta ToEpochTime(double delta_ms) const {
    return delta_ms > 0.0 ? base::Milliseconds(static_cast<int64_t>(
                                navigation_start + delta_ms))
                          : base::TimeDelta();
  }

  WebPerformanceTimings() = default;
#if INSIDE_BLINK
  explicit WebPerformanceTimings(WindowPerformance*);
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_PERFORMANCE_TIMINGS_H_
