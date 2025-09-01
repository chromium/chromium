// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preload_serving_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "content/browser/preloading/prefetch/prefetch_match_resolver.h"
#include "content/browser/preloading/preload_serving_metrics_holder.h"
#include "content/browser/preloading/prerender/prerender_features.h"

namespace content {

PrefetchContainerMetrics::PrefetchContainerMetrics() = default;

PrefetchContainerMetrics::~PrefetchContainerMetrics() = default;

// static
bool PreloadServingMetrics::IsEnabled() {
  return features::kPrerender2FallbackUsePreloadServingMetrics.Get();
}

// static
std::unique_ptr<PreloadServingMetrics>
PreloadServingMetrics::TakeFromNavigationHandle(
    NavigationHandle& navigation_handle) {
  CHECK(PreloadServingMetrics::IsEnabled());

  return PreloadServingMetricsHolder::GetOrCreateForNavigationHandle(
             navigation_handle)
      ->Take();
}

PreloadServingMetrics::PreloadServingMetrics() {
  CHECK(PreloadServingMetrics::IsEnabled());
}

PreloadServingMetrics::~PreloadServingMetrics() = default;

}  // namespace content
