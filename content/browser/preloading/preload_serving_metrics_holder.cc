// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preload_serving_metrics_holder.h"

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/no_destructor.h"

namespace content {

base::RepeatingCallback<void(std::unique_ptr<PreloadServingMetrics>)>&
GetDestructorCallbackForTesting() {
  static base::NoDestructor<
      base::RepeatingCallback<void(std::unique_ptr<PreloadServingMetrics>)>>
      dtor_callback_for_testing;
  return *dtor_callback_for_testing;
}

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(PreloadServingMetricsHolder);

// static
void PreloadServingMetricsHolder::SetDestructorCallbackForTesting(
    base::RepeatingCallback<void(std::unique_ptr<PreloadServingMetrics>)>
        callback) {
  GetDestructorCallbackForTesting() = std::move(callback);  // IN-TEST
}

PreloadServingMetricsHolder::PreloadServingMetricsHolder(
    NavigationHandle& handle)
    : preload_serving_metrics_(std::make_unique<PreloadServingMetrics>()) {
  CHECK(PreloadServingMetricsCapsule::IsFeatureEnabled());
}

PreloadServingMetricsHolder::~PreloadServingMetricsHolder() {
  if (GetDestructorCallbackForTesting()) {
    GetDestructorCallbackForTesting().Run(  // IN-TEST
        std::move(preload_serving_metrics_));
  }
}

void PreloadServingMetricsHolder::AddPrefetchMatchMetrics(
    std::unique_ptr<PrefetchMatchMetrics> prefetch_match_metrics) {
  CHECK(prefetch_match_metrics);

  // Do nothing if `PreloadServingMetrics` is already taken.
  //
  // This happens if a prerender that is potentially matching to a prefetch
  // while the prefetch is blocking the prerender.
  // `PrerenderHost::OnWillBeCancelled()` took the `PreloadServingMetrics`, and
  // this path is reached when something of the prefetch is updated and unblocks
  // the prerender.
  //
  // For more details, see
  // https://docs.google.com/document/d/1ITMr_qyysUPIMZpLkmpQABwtVseMBduRqxHGZxIJ1R0/edit?resourcekey=0-ccZ-G6JV4WO-1bP4TiNvjQ&tab=t.x99jls7s2xug
  if (!preload_serving_metrics_) {
    return;
  }

  preload_serving_metrics_->prefetch_match_metrics_list.push_back(
      std::move(prefetch_match_metrics));
}

void PreloadServingMetricsHolder::SetPrerenderInitialPreloadServingMetrics(
    std::unique_ptr<PreloadServingMetrics>
        prerender_initial_preload_serving_metrics) {
  CHECK(prerender_initial_preload_serving_metrics);
  CHECK(!preload_serving_metrics_->prerender_initial_preload_serving_metrics);

  preload_serving_metrics_->prerender_initial_preload_serving_metrics =
      std::move(prerender_initial_preload_serving_metrics);
}

void PreloadServingMetricsHolder::
    SetIsPrerenderAbortedByPrerenderURLLoaderThrottle(bool value) {
  preload_serving_metrics_
      ->is_prerender_aborted_by_prerender_url_loader_throttle = value;
}

std::unique_ptr<PreloadServingMetrics> PreloadServingMetricsHolder::Take() {
  // Ensures not to take it twice.
  CHECK(preload_serving_metrics_);

  return std::move(preload_serving_metrics_);
}

}  // namespace content
