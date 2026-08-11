// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_INPUT_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_INPUT_METRICS_H_

#include "base/metrics/histogram_base.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "third_party/blink/public/common/input/web_gesture_device.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

PLATFORM_EXPORT void RecordScrollReasonsMetric(
    WebGestureDevice,
    cc::MainThreadRepaintReasons,
    cc::MainThreadHitTestReasons,
    cc::MainThreadScrollingOtherReasons);

PLATFORM_EXPORT base::HistogramBase::Sample32 ToHistogramBucketForTesting(
    cc::MainThreadRepaintReason);
PLATFORM_EXPORT base::HistogramBase::Sample32 ToHistogramBucketForTesting(
    cc::MainThreadHitTestReason);
PLATFORM_EXPORT base::HistogramBase::Sample32 ToHistogramBucketForTesting(
    cc::MainThreadScrollingOtherReason);

// Used in implementation. Defined here for testing.
// LINT.IfChange(MainThreadScrollingCategory)
inline constexpr base::HistogramBase::Sample32 kNotScrollingOnMainBucket = 0;
inline constexpr base::HistogramBase::Sample32
    kScrollingOnMainForAnyReasonBucket = 1;
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:MainThreadScrollingCategory)

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_INPUT_METRICS_H_
