// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRELOAD_SERVING_METRICS_CAPSULE_H_
#define CONTENT_PUBLIC_BROWSER_PRELOAD_SERVING_METRICS_CAPSULE_H_

#include <memory>

#include "content/common/content_export.h"

namespace content {

class NavigationHandle;

// Instant load technology used for the navigation.
enum class UsedInstantLoad {
  kNoInstantLoad,
  kPrefetchWithoutPrePrefetch,
  kPrefetchWithPrePrefetch,
  kPrerender,
  kBFCache,
};

// Allows `PageLoadMetricsObserver` to get/hold/record `PreloadServingMetrics`.
class CONTENT_EXPORT PreloadServingMetricsCapsule {
 public:
  // Takes `PreloadServingMetrics` from `PreloadServingMetricsHolder` of
  // `NavigationHandle`.
  static std::unique_ptr<PreloadServingMetricsCapsule> TakeFromNavigationHandle(
      NavigationHandle& navigation_handle);

  PreloadServingMetricsCapsule() = default;
  virtual ~PreloadServingMetricsCapsule();

  // Not movable nor copyable.
  PreloadServingMetricsCapsule(PreloadServingMetricsCapsule&&) = delete;
  PreloadServingMetricsCapsule& operator=(PreloadServingMetricsCapsule&&) =
      delete;
  PreloadServingMetricsCapsule(const PreloadServingMetricsCapsule&) = delete;
  PreloadServingMetricsCapsule& operator=(const PreloadServingMetricsCapsule&) =
      delete;

  virtual void RecordMetricsForNonPrerenderNavigationCommitted() const = 0;

  virtual UsedInstantLoad GetUsedInstantLoad(
      bool nav_used_bfcache,
      bool is_served_by_legacy_search_prefetch) const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRELOAD_SERVING_METRICS_CAPSULE_H_
