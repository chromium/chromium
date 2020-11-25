// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_INPUT_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_INPUT_METRICS_H_

#include "cc/input/main_thread_scrolling_reason.h"
#include "third_party/blink/public/common/input/web_gesture_device.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

// `reason` is derived from `cc::MainThreadScrollingReason`. If recording
// `kNotScrollingOnMain`, simply pass it as-is. Hoewver, if recording the
// position of a set bit, the index of the set bit must be incremented by one.
//
// This stems from the fact that kNotScrollingOnMain is recorded in the
// histograms as value 0. However, the 0th bit is not actually reserved and
// has a separate, well-defined meaning. kNotScrollingOnMain is only
// recorded when *no* bits are set.
PLATFORM_EXPORT void RecordScrollReasonMetric(WebGestureDevice device,
                                              uint32_t reason);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_INPUT_METRICS_H_
