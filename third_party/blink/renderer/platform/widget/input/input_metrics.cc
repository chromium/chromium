// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/input_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "third_party/blink/public/common/input/web_gesture_device.h"

namespace blink {

namespace {

constexpr uint32_t kMax =
    cc::MainThreadScrollingReason::kMainThreadScrollingReasonLast;

static void RecordOneScrollReasonMetric(WebGestureDevice device,
                                        uint32_t reason_index) {
  if (device == WebGestureDevice::kTouchscreen) {
    UMA_HISTOGRAM_EXACT_LINEAR("Renderer4.MainThreadGestureScrollReason",
                               reason_index, kMax + 1);
  } else {
    UMA_HISTOGRAM_EXACT_LINEAR("Renderer4.MainThreadWheelScrollReason",
                               reason_index, kMax + 1);
  }
}

}  // anonymous namespace

void RecordScrollReasonsMetric(WebGestureDevice device, uint32_t reasons) {
  if (!reasons) {
    RecordOneScrollReasonMetric(device, 0);
    return;
  }

  // The enum in cc::MainThreadScrollingReason simultaneously defines actual
  // bitmask values and indices into the bitmask, but kNotScrollingMain is
  // recorded in the histograms as value 0, so the 0th bit should never be used.
  DCHECK(!(reasons & (1 << 0)));

  for (uint32_t i = 1; i <= kMax; ++i) {
    if (reasons & (1 << i))
      RecordOneScrollReasonMetric(device, i);
  }
}

}  // namespace blink
