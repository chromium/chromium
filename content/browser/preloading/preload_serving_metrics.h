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
#include "content/public/browser/preload_serving_metrics_capsule.h"

namespace content {

enum class PrefetchPotentialCandidateServingResult;
class NavigationHandle;

// All the structs in this file are "Logs" as defined in
// https://chromium.googlesource.com/chromium/src/+/main/content/browser/preloading/preload_serving_metrics.md#Logs

// Log of `PrefetchContainer`.
//
// `PreloadContainerMetrics` is a "Log" object as defined in
// https://chromium.googlesource.com/chromium/src/+/main/content/browser/preloading/preload_serving_metrics.md#Logs
//
// `PrefetchContainerMetrics` is owned by a `PrefetchContainer`, filled by the
// `PrefetchContainer`, and used for the per-`PrefetchContainer` metrics (e.g.
// `PrefetchContainer::RecordPrefetchDurationHistogram()`).
//
// `PrefetchContainerMetrics` is also used for `PreloadServingMetrics`. In this
// case, the `PrefetchContainerMetrics` at the time of serving is copied
// (indirectly) into `PreloadServingMetrics`.
struct CONTENT_EXPORT PrefetchContainerMetrics {
  PrefetchContainerMetrics();
  ~PrefetchContainerMetrics();

  // Not movable but copyable.
  PrefetchContainerMetrics(PrefetchContainerMetrics&& other) = delete;
  PrefetchContainerMetrics& operator=(PrefetchContainerMetrics&& other) =
      delete;
  PrefetchContainerMetrics(const PrefetchContainerMetrics&) = default;
  PrefetchContainerMetrics& operator=(const PrefetchContainerMetrics&) =
      default;

  // Timing information for metrics
  //
  // Constraint: That earlier one is null implies that later one is null.
  // E.g. `time_prefetch_start` is null implies
  // `time_header_determined_successfully` is null.
  std::optional<base::TimeTicks> time_added_to_prefetch_service;
  std::optional<base::TimeTicks> time_initial_eligibility_got;
  std::optional<base::TimeTicks> time_prefetch_started;
  std::optional<base::TimeTicks> time_url_request_started;
  std::optional<base::TimeTicks> time_header_determined_successfully;
  std::optional<base::TimeTicks> time_prefetch_completed_successfully;
};

// Log of prefetch matching.
//
// `PreloadMatchMetrics` is a "Log" object as defined in
// https://chromium.googlesource.com/chromium/src/+/main/content/browser/preloading/preload_serving_metrics.md#Logs
//
// The members are filled by `PrefetchMatchResolver`.
struct CONTENT_EXPORT PrefetchMatchMetrics {
  PrefetchMatchMetrics();
  ~PrefetchMatchMetrics();

  // Not movable nor copyable.
  PrefetchMatchMetrics(PrefetchMatchMetrics&& other) = delete;
  PrefetchMatchMetrics& operator=(PrefetchMatchMetrics&& other) = delete;
  PrefetchMatchMetrics(const PrefetchMatchMetrics&) = delete;
  PrefetchMatchMetrics& operator=(const PrefetchMatchMetrics&) = delete;

  // Optional, may be null. Non-null iff matched at
  // `PrefetchMatchResolver::UnblockInternal()`.
  std::unique_ptr<PrefetchContainerMetrics> prefetch_container_metrics =
      nullptr;
};

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

  void RecordMetricsForNonPrerenderNavigationCommitted() const;
  void RecordFirstContentfulPaint(
      base::TimeDelta corrected_first_contentful_paint) const;

  // Added per prefetch matching.
  std::vector<std::unique_ptr<PrefetchMatchMetrics>>
      prefetch_match_metrics_list;
  // If `this` is for a prerender activation navigation, it's
  // `PreloadServingMetrics` of the corresponding prerender initial navigation.
  // Otherwise null.
  //
  // If there are multiple navigations in the frame tree for prerender, this is
  // the first navigation and the `PreloadServingMetrics`s for the other
  // navigations are discarded.
  std::unique_ptr<PreloadServingMetrics>
      prerender_initial_preload_serving_metrics = nullptr;
};

// Allows `PageLoadMetricsObserver` to get/hold/record `PreloadServingMetrics`.
class CONTENT_EXPORT PreloadServingMetricsCapsuleImpl final
    : public PreloadServingMetricsCapsule {
 public:
  static bool IsEnabled();
  // Take `PreloadServingMetrics` from `PreloadServingMetricsHolder` of
  // `NavigationHandle`.
  static std::unique_ptr<PreloadServingMetricsCapsule> TakeFromNavigationHandle(
      NavigationHandle& navigation_handle);

  void RecordMetricsForNonPrerenderNavigationCommitted() const override;
  void RecordFirstContentfulPaint(
      base::TimeDelta corrected_first_contentful_paint) const override;

 private:
  explicit PreloadServingMetricsCapsuleImpl(
      std::unique_ptr<PreloadServingMetrics> preload_serving_metrics);
  ~PreloadServingMetricsCapsuleImpl() override;
  friend struct std::default_delete<PreloadServingMetricsCapsuleImpl>;

  std::unique_ptr<PreloadServingMetrics> preload_serving_metrics_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRELOAD_SERVING_METRICS_H_
