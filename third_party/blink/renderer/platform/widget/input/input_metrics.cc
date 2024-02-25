// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/input_metrics.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "cc/base/features.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "third_party/blink/public/common/input/web_gesture_device.h"

namespace blink {

namespace {

constexpr uint32_t kMax =
    cc::MainThreadScrollingReason::kMainThreadScrollingReasonLast;

static void RecordOneScrollReasonMetric(WebGestureDevice device,
                                        uint32_t reason_index) {
  if (device == WebGestureDevice::kTouchscreen) {
    UMA_HISTOGRAM_EXACT_LINEAR("Renderer4.MainThreadGestureScrollReason2",
                               reason_index, kMax + 1);
  } else {
    UMA_HISTOGRAM_EXACT_LINEAR("Renderer4.MainThreadWheelScrollReason2",
                               reason_index, kMax + 1);
  }
}

}  // anonymous namespace

void RecordScrollReasonsMetric(WebGestureDevice device, uint32_t reasons) {
  if (reasons == cc::MainThreadScrollingReason::kNotScrollingOnMain) {
    // Record the histogram for non-main-thread scrolls.
    RecordOneScrollReasonMetric(
        device, cc::MainThreadScrollingReason::kNotScrollingOnMain);
    return;
  }

  // Record the histogram for main-thread scrolls for any reason.
  RecordOneScrollReasonMetric(
      device, cc::MainThreadScrollingReason::kScrollingOnMainForAnyReason);

  // The enum in cc::MainThreadScrollingReason simultaneously defines actual
  // bitmask values and indices into the bitmask, but kNotScrollingMain and
  // kScrollingOnMainForAnyReason are recorded in the histograms, so these
  // bits should never be used.
  DCHECK(
      !(reasons & (1 << cc::MainThreadScrollingReason::kNotScrollingOnMain)));
  DCHECK(!(reasons &
           (1 << cc::MainThreadScrollingReason::kScrollingOnMainForAnyReason)));

  // Record histograms for individual main-thread scrolling reasons.
  for (uint32_t i =
           cc::MainThreadScrollingReason::kScrollingOnMainForAnyReason + 1;
       i <= kMax; ++i) {
    if (reasons & (1 << i))
      RecordOneScrollReasonMetric(device, i);
  }
}

}  // namespace blink
