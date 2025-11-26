// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRELOAD_SERVING_METRICS_H_
#define CONTENT_BROWSER_PRELOADING_PRELOAD_SERVING_METRICS_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/time/time.h"
#include "content/browser/preloading/prefetch/prefetch_servable_state.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader_common_types.h"
#include "content/common/content_export.h"
#include "content/public/browser/preload_serving_metrics_capsule.h"

namespace content {

enum class PrefetchPotentialCandidateServingResult;
class NavigationHandle;
class PrefetchContainer;
enum class PrefetchStatus;

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
struct CONTENT_EXPORT PrefetchContainerMetrics final {
  PrefetchContainerMetrics();
  ~PrefetchContainerMetrics();

  // Not movable but copyable.
  PrefetchContainerMetrics(PrefetchContainerMetrics&& other) = delete;
  PrefetchContainerMetrics& operator=(PrefetchContainerMetrics&& other) =
      delete;
  PrefetchContainerMetrics(const PrefetchContainerMetrics&);
  PrefetchContainerMetrics& operator=(const PrefetchContainerMetrics&);

  // Timing information for metrics
  //
  // Constraint: That earlier one is null implies that later one is null.
  // E.g. `time_prefetch_start` is null implies
  // `time_header_determined_successfully` is null.
  std::optional<base::TimeTicks> time_added_to_prefetch_service;
  std::optional<base::TimeTicks> time_initial_eligibility_got;
  std::optional<base::TimeTicks> time_prefetch_started;
  std::optional<base::TimeTicks> time_url_request_started;
  std::optional<base::TimeTicks> time_domain_lookup_started;
  std::optional<base::TimeTicks> time_header_determined_successfully;
  std::optional<base::TimeTicks> time_prefetch_completed_successfully;

  // TimeDelta information for metrics.
  std::optional<base::TimeDelta> create_stream_delay;
  std::optional<base::TimeDelta> connected_callback_delay;
  std::optional<base::TimeDelta> initialize_stream_delay;
};

// Debug information of prefetch ahead of prerender at prefetch matching.
//
// `PrefetchMatchPrefetchAheadOfPrerenderDebugMetrics` is a "Log" object as
// defined in
// https://chromium.googlesource.com/chromium/src/+/main/content/browser/preloading/preload_serving_metrics.md#Logs
//
// The members are filled by `PrefetchMatchResolver`.
struct CONTENT_EXPORT PrefetchMatchPrefetchAheadOfPrerenderDebugMetrics final {
  PrefetchMatchPrefetchAheadOfPrerenderDebugMetrics();
  ~PrefetchMatchPrefetchAheadOfPrerenderDebugMetrics();

  // Not movable nor copyable.
  PrefetchMatchPrefetchAheadOfPrerenderDebugMetrics(
      PrefetchMatchPrefetchAheadOfPrerenderDebugMetrics&& other) = delete;
  PrefetchMatchPrefetchAheadOfPrerenderDebugMetrics& operator=(
      PrefetchMatchPrefetchAheadOfPrerenderDebugMetrics&& other) = delete;
  PrefetchMatchPrefetchAheadOfPrerenderDebugMetrics(
      const PrefetchMatchPrefetchAheadOfPrerenderDebugMetrics&) = delete;
  PrefetchMatchPrefetchAheadOfPrerenderDebugMetrics& operator=(
      const PrefetchMatchPrefetchAheadOfPrerenderDebugMetrics&) = delete;

  PrefetchStatus prefetch_status;
  PrefetchServableState servable_state;
  PrefetchMatchResolverAction match_resolver_action;
  int queue_size;
  std::optional<int> queue_index;
};

// Debug information of prefetch ahead of prerender at prefetch matching.
//
// `PreloadMatchPrerenderDebugMetrics` is a "Log" object as defined in
// https://chromium.googlesource.com/chromium/src/+/main/content/browser/preloading/preload_serving_metrics.md#Logs
//
// The members are filled by `PrefetchMatchResolver`.
struct CONTENT_EXPORT PrefetchMatchPrerenderDebugMetrics final {
  PrefetchMatchPrerenderDebugMetrics();
  ~PrefetchMatchPrerenderDebugMetrics();

  // Not movable nor copyable.
  PrefetchMatchPrerenderDebugMetrics(
      PrefetchMatchPrerenderDebugMetrics&& other) = delete;
  PrefetchMatchPrerenderDebugMetrics& operator=(
      PrefetchMatchPrerenderDebugMetrics&& other) = delete;
  PrefetchMatchPrerenderDebugMetrics(
      const PrefetchMatchPrerenderDebugMetrics&) = delete;
  PrefetchMatchPrerenderDebugMetrics& operator=(
      const PrefetchMatchPrerenderDebugMetrics&) = delete;

  // Non null iff prefetch ahead of prerender exists at the timing of prefetch
  // matching.
  std::unique_ptr<PrefetchMatchPrefetchAheadOfPrerenderDebugMetrics>
      prefetch_ahead_of_prerender_debug_metrics;
};

// Log of prefetch matching.
//
// `PreloadMatchMetrics` is a "Log" object as defined in
// https://chromium.googlesource.com/chromium/src/+/main/content/browser/preloading/preload_serving_metrics.md#Logs
//
// The members are filled by `PrefetchMatchResolver`.
struct CONTENT_EXPORT PrefetchMatchMetrics final {
  PrefetchMatchMetrics();
  ~PrefetchMatchMetrics();

  // Not movable nor copyable.
  PrefetchMatchMetrics(PrefetchMatchMetrics&& other) = delete;
  PrefetchMatchMetrics& operator=(PrefetchMatchMetrics&& other) = delete;
  PrefetchMatchMetrics(const PrefetchMatchMetrics&) = delete;
  PrefetchMatchMetrics& operator=(const PrefetchMatchMetrics&) = delete;

  bool IsPotentialMatch() const;
  bool IsActualMatch() const;
  PrefetchPotentialCandidateServingResult
  GetPrefetchPotentialCandidateServingResultLast() const;

  PrefetchServiceWorkerState expected_service_worker_state;

  base::TimeTicks time_match_start;
  base::TimeTicks time_match_end;

  // Number of initial candidates of prefetch matching, including already failed
  // ones.
  int n_initial_candidates = -1;

  // Number of initial candidates of prefetch matching, blocking ones.
  int n_initial_candidates_block_until_head = -1;

  // The `PrefetchContainerMetrics` of the `PrefetchContainer` candidate that
  // was successfully matched with the `PrefetchMatchResolver`, if any.
  // Otherwise null.
  std::unique_ptr<PrefetchContainerMetrics> prefetch_container_metrics =
      nullptr;
  // The last serving result. Non null iff `n_initial_candidates > 0`.
  //
  // Note that `n_initial_candidates` is 0 or 1 in almost all cases. For more
  // details, see
  // https://docs.google.com/document/d/1ITMr_qyysUPIMZpLkmpQABwtVseMBduRqxHGZxIJ1R0/edit?resourcekey=0-ccZ-G6JV4WO-1bP4TiNvjQ&tab=t.x99jls7s2xug
  std::optional<PrefetchPotentialCandidateServingResult>
      prefetch_potential_candidate_serving_result_last = std::nullopt;
  // The information of the prefetch-ahead-prerender `PrefetchContainer`
  // candidate, if any. Otherwise null. More precisely, this is non-null iff:
  //
  // - `PrefetchMatchResolver::navigation_request_for_metrics_` is for a
  //   prerender initial navigation; and
  // - The `PrefetchContainer` of the prefetch-ahead-of-prerender of the
  //   prerendering (if any) is potentially matching with the
  //   `PrefetchMatchResolver`.
  std::optional<PrefetchPotentialCandidateServingResult>
      prefetch_potential_candidate_serving_result_ahead_of_prerender =
          std::nullopt;
  // The condition is the same to the above.
  std::unique_ptr<PrefetchContainerMetrics>
      prefetch_container_metrics_ahead_of_prerender = nullptr;

  // Null if `!UsePrefetchScheduler()`.
  //
  // TODO(crbug.com/406402069): Remove the above comment.
  //
  // Non null if the navigation is prerender initial navigation.
  std::unique_ptr<PrefetchMatchPrerenderDebugMetrics> prerender_debug_metrics;
};

// Log of preloads related to a navigation
//
// `PreloadServingMetrics` is a "Log" object as defined in
// https://chromium.googlesource.com/chromium/src/+/main/content/browser/preloading/preload_serving_metrics.md#Logs
//
// The members are filled by `PreloadServingMetrics`.
struct CONTENT_EXPORT PreloadServingMetrics final {
  PreloadServingMetrics();
  ~PreloadServingMetrics();

  // Not movable nor copyable.
  PreloadServingMetrics(PreloadServingMetrics&& other) = delete;
  PreloadServingMetrics& operator=(PreloadServingMetrics&& other) = delete;
  PreloadServingMetrics(const PreloadServingMetrics&) = delete;
  PreloadServingMetrics& operator=(const PreloadServingMetrics&) = delete;

  // Gets "meaningful" `PrefetchMatchMetrics`
  //
  // For initial fetch of navigation (i.e. before redirect),
  // `PrefetchURLLoaderInterceptor` tries to intercept twice, with
  // `PrefetchServiceWorkerState::kControlled` and
  // `PrefetchServiceWorkerState::kDisallowed`. This method returns meaningful
  // one.
  //
  // Returns nullptr if there is no `PrefetchMatchMetrics`.
  const PrefetchMatchMetrics* GetMeaningfulPrefetchMatchMetrics() const;

  void RecordMetricsForNonPrerenderNavigationCommitted() const;
  void RecordMetricsForPrerenderInitialNavigationFailed() const;
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
  bool is_prerender_aborted_by_prerender_url_loader_throttle = false;
};

// Allows `PageLoadMetricsObserver` to get/hold/record `PreloadServingMetrics`.
class CONTENT_EXPORT PreloadServingMetricsCapsuleImpl final
    : public PreloadServingMetricsCapsule {
 public:
  // Take `PreloadServingMetrics` from `PreloadServingMetricsHolder` of
  // `NavigationHandle`.
  //
  // See
  // https://chromium.googlesource.com/chromium/src/+/main/content/browser/preloading/preload_serving_metrics.md#life-of-PreloadServingMetrics
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
