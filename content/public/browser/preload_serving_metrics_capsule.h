// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRELOAD_SERVING_METRICS_CAPSULE_H_
#define CONTENT_PUBLIC_BROWSER_PRELOAD_SERVING_METRICS_CAPSULE_H_

#include <memory>

#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

class NavigationHandle;

// Allows `PageLoadMetricsObserver` to get/hold/record `PreloadServingMetrics`.
class CONTENT_EXPORT PreloadServingMetricsCapsule {
 public:
  // Used to control entering paths of `PreloadServingMetrics`, which records
  // serving metrics of preloads.
  static bool IsFeatureEnabled();

  // Takes `PreloadServingMetrics` from `PreloadServingMetricsHolder` of
  // `NavigationHandle`.
  static std::unique_ptr<PreloadServingMetricsCapsule> TakeFromNavigationHandle(
      NavigationHandle& navigation_handle);

  virtual ~PreloadServingMetricsCapsule();

  virtual void RecordMetricsForNonPrerenderNavigationCommitted() const = 0;

  // Records FirstContentfulPaint
  //
  // The parameter `corrected_first_contentful_paint` is the return value of
  // `page_load_metrics::CorrectEventAsNavigationOrActivationOrigined()`.
  virtual void RecordFirstContentfulPaint(
      base::TimeDelta corrected_first_contentful_paint) const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRELOAD_SERVING_METRICS_CAPSULE_H_
