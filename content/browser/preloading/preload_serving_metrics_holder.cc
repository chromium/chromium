// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preload_serving_metrics_holder.h"

namespace content {

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(PreloadServingMetricsHolder);

PreloadServingMetricsHolder::PreloadServingMetricsHolder(
    NavigationHandle& handle)
    : preload_serving_metrics_(std::make_unique<PreloadServingMetrics>()) {
  CHECK(PreloadServingMetrics::IsEnabled());
}

PreloadServingMetricsHolder::~PreloadServingMetricsHolder() = default;

void PreloadServingMetricsHolder::AddPrefetchMatchMetrics(
    std::unique_ptr<PrefetchMatchMetrics> prefetch_match_metrics) {
  CHECK(prefetch_match_metrics);
  CHECK(preload_serving_metrics_);

  preload_serving_metrics_->prefetch_match_metrics_list.push_back(
      std::move(prefetch_match_metrics));
}

std::unique_ptr<PreloadServingMetrics> PreloadServingMetricsHolder::Take() {
  // Ensures not to take it twice.
  CHECK(preload_serving_metrics_);

  return std::move(preload_serving_metrics_);
}

}  // namespace content
