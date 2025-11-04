// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRELOAD_SERVING_METRICS_HOLDER_H_
#define CONTENT_BROWSER_PRELOADING_PRELOAD_SERVING_METRICS_HOLDER_H_

#include "content/browser/preloading/preload_serving_metrics.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_handle_user_data.h"

namespace content {

// Holds `PreloadServingMetrics` to collect logs in navigation as
// `NavigationHandleUserData`.
//
// See
// https://chromium.googlesource.com/chromium/src/+/main/content/browser/preloading/preload_serving_metrics.md
// for design overview.
class CONTENT_EXPORT PreloadServingMetricsHolder final
    : public NavigationHandleUserData<PreloadServingMetricsHolder> {
 public:
  // The callback is called when `PreloadServingMetricsHolder` is destroyed,
  // with the argument of the underlying `PreloadServingMetrics` at that time
  // (which can be nullptr).
  //
  // For inspection without `PageLoadMetricsObserver` in //content browser
  // tests, as `PageLoadMetricsObserver` is in //components.
  static void SetDestructorCallbackForTesting(
      base::RepeatingCallback<void(std::unique_ptr<PreloadServingMetrics>)>
          callback);

  ~PreloadServingMetricsHolder() override;

  // Not movable nor copyable.
  PreloadServingMetricsHolder(PreloadServingMetricsHolder&& other) = delete;
  PreloadServingMetricsHolder& operator=(PreloadServingMetricsHolder&& other) =
      delete;
  PreloadServingMetricsHolder(const PreloadServingMetricsHolder&) = delete;
  PreloadServingMetricsHolder& operator=(const PreloadServingMetricsHolder&) =
      delete;

  // Add `PrefetchMatchMetrics` at the end of prefetch matching.
  void AddPrefetchMatchMetrics(
      std::unique_ptr<PrefetchMatchMetrics> prefetch_match_metrics);
  void SetPrerenderInitialPreloadServingMetrics(
      std::unique_ptr<PreloadServingMetrics>
          prerender_initial_preload_serving_metrics);
  void SetIsPrerenderAbortedByPrerenderURLLoaderThrottle(bool value);
  // Take metrics for recording UMAs/UKMs.
  //
  // Precondition: It is allowed to call this once per navigation. Otherwise, it
  // crashes.
  //
  // Postcondition: The return value is non null.
  //
  // Note that this is called in two paths: From `PrerenderHost` for prerender
  // initial navigation and from `PreloadServingMetricsHolder` otherwise.
  //
  // For more details, see
  // https://chromium.googlesource.com/chromium/src/+/main/content/browser/preloading/preload_serving_metrics.md#life-of-PreloadServingMetrics
  std::unique_ptr<PreloadServingMetrics> Take();

 private:
  friend NavigationHandleUserData;

  explicit PreloadServingMetricsHolder(NavigationHandle& navigation_handle);

  // Non null until `Take()` is called.
  std::unique_ptr<PreloadServingMetrics> preload_serving_metrics_;

  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRELOAD_SERVING_METRICS_HOLDER_H_
