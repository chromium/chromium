// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRELOAD_SERVING_METRICS_H_
#define CONTENT_BROWSER_PRELOADING_PRELOAD_SERVING_METRICS_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

enum class PrefetchPotentialCandidateServingResult;
class NavigationHandle;

// All the structs in this file are "Logs" as defined in
// https://chromium.googlesource.com/chromium/src/+/main/content/browser/preloading/preload_serving_metrics.md#Logs

// Log of preloads related to a navigation
//
// `PreloadServingMetrics` is a "Log" object as defined in
// https://chromium.googlesource.com/chromium/src/+/main/content/browser/preloading/preload_serving_metrics.md#Logs
//
// The members are filled by `PreloadServingMetrics`.
struct CONTENT_EXPORT PreloadServingMetrics {
  // Plumbs a feature param in //content to page load metrics observer.
  static bool IsEnabled();
  // Take `PreloadServingMetrics` from `PreloadServingMetricsHolder` of
  // `NavigationHandle`.
  //
  // See
  // https://chromium.googlesource.com/chromium/src/+/main/content/browser/preloading/preload_serving_metrics.md#life-of-PreloadServingMetrics
  static std::unique_ptr<PreloadServingMetrics> TakeFromNavigationHandle(
      NavigationHandle& navigation_handle);

  PreloadServingMetrics();
  ~PreloadServingMetrics();

  // Not movable nor copyable.
  PreloadServingMetrics(PreloadServingMetrics&& other) = delete;
  PreloadServingMetrics& operator=(PreloadServingMetrics&& other) = delete;
  PreloadServingMetrics(const PreloadServingMetrics&) = delete;
  PreloadServingMetrics& operator=(const PreloadServingMetrics&) = delete;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRELOAD_SERVING_METRICS_H_
