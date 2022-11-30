// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_navigation_timing_activation_start.h"

#include "third_party/blink/renderer/core/loader/document_load_timing.h"
#include "third_party/blink/renderer/core/timing/performance.h"

namespace blink {

// static
DOMHighResTimeStamp PerformanceNavigationTimingActivationStart::activationStart(
    const PerformanceNavigationTiming& performance_navigation_timing) {
  DocumentLoadTiming* timing =
      performance_navigation_timing.GetDocumentLoadTiming();
  if (!timing)
    return 0.0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      performance_navigation_timing.TimeOrigin(), timing->ActivationStart(),
      false /* allow_negative_value */,
      performance_navigation_timing.CrossOriginIsolatedCapability());
}

}  // namespace blink
