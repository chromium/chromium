// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/input_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "third_party/blink/public/common/input/web_gesture_device.h"

namespace blink {

void RecordScrollReasonMetric(WebGestureDevice device, uint32_t reason) {
  constexpr uint32_t kMainThreadScrollingReasonEnumMax =
      cc::MainThreadScrollingReason::kMainThreadScrollingReasonCount + 1;
  // Note the use of `UMA_HISTOGRAM_EXACT_LINEAR` here. This is because the enum
  // defined in cc::MainThreadScrollingReason defines both bitmasks and bitmask
  // positions and doesn't correspond well to how the UMA helpers for
  // enumerations are typically used.
  if (device == WebGestureDevice::kTouchscreen) {
    UMA_HISTOGRAM_EXACT_LINEAR("Renderer4.MainThreadGestureScrollReason",
                               reason, kMainThreadScrollingReasonEnumMax);
  } else {
    UMA_HISTOGRAM_EXACT_LINEAR("Renderer4.MainThreadWheelScrollReason", reason,
                               kMainThreadScrollingReasonEnumMax);
  }
}

}  // namespace blink
