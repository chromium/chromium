// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/preload_serving_metrics_capsule.h"

#include <optional>

#include "base/time/time.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/preload_serving_metrics.h"
#include "content/browser/preloading/preload_serving_metrics_holder.h"
#include "content/browser/preloading/prerender/prerender_features.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {

PreloadServingMetricsCapsule::~PreloadServingMetricsCapsule() = default;

// static
std::unique_ptr<PreloadServingMetricsCapsule>
PreloadServingMetricsCapsule::TakeFromNavigationHandle(
    NavigationHandle& navigation_handle) {
  return PreloadServingMetricsCapsuleImpl::TakeFromNavigationHandle(
      navigation_handle);
}

}  // namespace content
