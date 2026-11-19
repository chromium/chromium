// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/worklet_performance.h"

#include "third_party/blink/renderer/core/timing/performance.h"

namespace blink {

WorkletPerformance::WorkletPerformance(
    base::TimeTicks time_origin,
    bool has_cross_origin_isolated_capability)
    : time_origin_(time_origin),
      has_cross_origin_isolated_capability_(
          has_cross_origin_isolated_capability) {}

DOMHighResTimeStamp WorkletPerformance::now() const {
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, base::TimeTicks::Now(), /*allow_negative_value=*/false,
      has_cross_origin_isolated_capability_);
}

}  // namespace blink
