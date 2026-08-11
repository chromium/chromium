// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/input_metrics.h"

#include <array>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "cc/base/features.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "third_party/blink/public/common/input/web_gesture_device.h"

namespace blink {

namespace {

// LINT.IfChange(MainThreadScrollingReason)

base::HistogramBase::Sample32 ToBucket(
    const cc::MainThreadRepaintReason reason) {
  switch (reason) {
    case cc::MainThreadRepaintReason::kHasBackgroundAttachmentFixedObjects:
      return 2;
    case cc::MainThreadRepaintReason::kNotOpaqueForTextAndLCDText:
      return 5;
    case cc::MainThreadRepaintReason::kPreferNonCompositedScrolling:
      return 15;
    case cc::MainThreadRepaintReason::kBackgroundNeedsRepaintOnScroll:
      return 16;
  }
}

base::HistogramBase::Sample32 ToBucket(
    const cc::MainThreadHitTestReason reason) {
  switch (reason) {
    case cc::MainThreadHitTestReason::kScrollbarScrolling:
      return 7;
    case cc::MainThreadHitTestReason::kMainThreadScrollHitTestRegion:
      return 8;
    case cc::MainThreadHitTestReason::kFailedHitTest:
      return 9;
  }
}

base::HistogramBase::Sample32 ToBucket(
    const cc::MainThreadScrollingOtherReason reason) {
  switch (reason) {
    case cc::MainThreadScrollingOtherReason::kPopupNoThreadedInput:
      return 4;
    case cc::MainThreadScrollingOtherReason::kWheelEventHandlerRegion:
      return 13;
    case cc::MainThreadScrollingOtherReason::kTouchEventHandlerRegion:
      return 14;
  }
}

constexpr base::HistogramBase::Sample32 kExclusiveMax = 17;

// LINT.ThenChange(//tools/metrics/histograms/enums.xml:MainThreadScrollingReason)

static void RecordOneScrollReasonMetric(WebGestureDevice device,
                                        base::HistogramBase::Sample32 bucket) {
  CHECK_LT(bucket, kExclusiveMax);
  if (device == WebGestureDevice::kTouchscreen) {
    UMA_HISTOGRAM_EXACT_LINEAR("Renderer4.MainThreadGestureScrollReason2",
                               bucket, kExclusiveMax);
  } else {
    UMA_HISTOGRAM_EXACT_LINEAR("Renderer4.MainThreadWheelScrollReason2", bucket,
                               kExclusiveMax);
  }
}

}  // anonymous namespace

void RecordScrollReasonsMetric(
    WebGestureDevice device,
    cc::MainThreadRepaintReasons repaint_reasons,
    cc::MainThreadHitTestReasons hit_test_reasons,
    cc::MainThreadScrollingOtherReasons other_reasons) {
  if (repaint_reasons.empty() && hit_test_reasons.empty() &&
      other_reasons.empty()) {
    // Record the histogram for non-main-thread scrolls.
    RecordOneScrollReasonMetric(device, kNotScrollingOnMainBucket);
    return;
  }

  // Record the histogram for main-thread scrolls for any reason.
  RecordOneScrollReasonMetric(device, kScrollingOnMainForAnyReasonBucket);

  for (auto reason : repaint_reasons) {
    RecordOneScrollReasonMetric(device, ToBucket(reason));
  }
  for (auto reason : hit_test_reasons) {
    RecordOneScrollReasonMetric(device, ToBucket(reason));
  }
  for (auto reason : other_reasons) {
    RecordOneScrollReasonMetric(device, ToBucket(reason));
  }
}

base::HistogramBase::Sample32 ToHistogramBucketForTesting(
    cc::MainThreadRepaintReason reason) {
  return ToBucket(reason);
}
base::HistogramBase::Sample32 ToHistogramBucketForTesting(
    cc::MainThreadHitTestReason reason) {
  return ToBucket(reason);
}
base::HistogramBase::Sample32 ToHistogramBucketForTesting(
    cc::MainThreadScrollingOtherReason reason) {
  return ToBucket(reason);
}

}  // namespace blink
