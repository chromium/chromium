// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preload_serving_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "content/browser/preloading/prefetch/prefetch_match_resolver.h"
#include "content/browser/preloading/preload_serving_metrics_holder.h"

namespace content {

namespace {

// Copied from components/page_load_metrics/browser/page_load_metrics_util.h
#define PAGE_LOAD_HISTOGRAM(name, sample)                             \
  base::UmaHistogramCustomTimes(name, sample, base::Milliseconds(10), \
                                base::Minutes(10), 100)

#define WITH(prefix, name) base::StrCat({prefix, name})

void RecordMetricsInternal(const PreloadServingMetrics& metrics,
                           const char* prefix,
                           bool is_prerender_initial_navigation) {
  // We expect that prefetch match count is zero or one.
  base::UmaHistogramCounts100(WITH(prefix, "PrefetchMatchMetrics.Count"),
                              metrics.prefetch_match_metrics_list.size());

  [&]() {
    // We only checks the first two prefetch matching, as they are most likely
    // to have meaningful data and checking other ones is costly with UMAs.
    //
    // TODO(crbug.com/360094997): Consider to use UKM.
    const PrefetchMatchMetrics* meaningful_prefetch_match_metrics =
        metrics.GetMeaningfulPrefetchMatchMetrics();

    const bool is_potential_match =
        meaningful_prefetch_match_metrics &&
        meaningful_prefetch_match_metrics->IsPotentialMatch();
    const bool is_potential_match_with_ahead_of_prerender =
        is_potential_match &&
        meaningful_prefetch_match_metrics
            ->prefetch_potential_candidate_serving_result_ahead_of_prerender
            .has_value();

    base::UmaHistogramBoolean(
        WITH(prefix, "PrefetchMatchMetrics.IsPotentialMatch"),
        is_potential_match);
    if (is_prerender_initial_navigation) {
      base::UmaHistogramBoolean(
          WITH(prefix,
               "PrefetchMatchMetrics.IsPotentialMatch.WithAheadOfPrerender"),
          is_potential_match_with_ahead_of_prerender);
    }

    if (!is_potential_match) {
      return;
    }
    auto& prefetch_match_metrics = *meaningful_prefetch_match_metrics;

    base::UmaHistogramCounts100(WITH(prefix,
                                     "PrefetchMatchMetrics.PotentialMatchThen."
                                     "NumberOfInitialCandidates"),
                                prefetch_match_metrics.n_initial_candidates);
    base::UmaHistogramCounts100(
        WITH(prefix,
             "PrefetchMatchMetrics.PotentialMatchThen."
             "NumberOfInitialCandidatesBlockUntilHead"),
        prefetch_match_metrics.n_initial_candidates_block_until_head);
    const bool is_actual_match = prefetch_match_metrics.IsActualMatch();
    base::UmaHistogramBoolean(
        WITH(prefix, "PrefetchMatchMetrics.PotentialMatchThen.IsActualMatch"),
        is_actual_match);

    base::TimeDelta prefetch_match_duration =
        prefetch_match_metrics.time_match_end -
        prefetch_match_metrics.time_match_start;
    // We use `UmaHistogramMediumTimes()` (1ms to 3min) because timeout of
    // `PrefetchStreamingURLLoader` is 10sec and `UmaHistogramTimes()` (1ms to
    // 10sec) has too small range.
    base::UmaHistogramMediumTimes(
        WITH(prefix, "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration"),
        prefetch_match_duration);
    if (is_actual_match) {
      base::UmaHistogramMediumTimes(
          WITH(prefix,
               "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration."
               "ForActualMatch"),
          prefetch_match_duration);
    } else {
      base::UmaHistogramMediumTimes(
          WITH(prefix,
               "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration."
               "ForNotActualMatch"),
          prefetch_match_duration);
    }

    if (is_actual_match) {
      CHECK(prefetch_match_metrics.prefetch_container_metrics
                ->time_added_to_prefetch_service.has_value());
      base::TimeDelta time_from_prefetch_container_added_to_match_start =
          prefetch_match_metrics.time_match_start -
          prefetch_match_metrics.prefetch_container_metrics
              ->time_added_to_prefetch_service.value();
      // Actually matched `PrefetchContainer` was potentially matched at the
      // timing of match start, and was necessarily added to `PrefetchService`
      // ahead.
      CHECK_LE(base::Seconds(0),
               time_from_prefetch_container_added_to_match_start);
      base::UmaHistogramMediumTimes(
          WITH(prefix,
               "PrefetchMatchMetrics.ActualMatchThen."
               "TimeFromPrefetchContainerAddedToMatchStart"),
          time_from_prefetch_container_added_to_match_start);
    }

    if (is_prerender_initial_navigation &&
        prefetch_match_metrics
            .prefetch_potential_candidate_serving_result_ahead_of_prerender
            .has_value()) {
      base::UmaHistogramEnumeration(
          WITH(prefix,
               "PrefetchMatchMetrics.PotentialMatchThen.WithAheadOfPrerender."
               "PotentialCandidateServingResult"),
          prefetch_match_metrics
              .prefetch_potential_candidate_serving_result_ahead_of_prerender
              .value());
    }
  }();
}

}  // namespace

PrefetchContainerMetrics::PrefetchContainerMetrics() = default;

PrefetchContainerMetrics::~PrefetchContainerMetrics() = default;

PrefetchMatchMetrics::PrefetchMatchMetrics() = default;

PrefetchMatchMetrics::~PrefetchMatchMetrics() = default;

bool PrefetchMatchMetrics::IsPotentialMatch() const {
  return n_initial_candidates > 0;
}

bool PrefetchMatchMetrics::IsActualMatch() const {
  return !!prefetch_container_metrics;
}

const PrefetchMatchMetrics*
PreloadServingMetrics::GetMeaningfulPrefetchMatchMetrics() const {
  // There is no `PrefetchMatchMetrics` if an interceptor ahead of
  // `PrefetchURLLoaderInterceptor` intercepted.
  if (prefetch_match_metrics_list.size() == 0) {
    return nullptr;
  }

  CHECK(prefetch_match_metrics_list[0]);

  // There is one `PrefetchMatchMetrics` if `PrefetchURLLoaderInterceptor` with
  // `PrefetchServiceWorkerState::kControlled` intercepted.
  if (prefetch_match_metrics_list.size() == 1) {
    return prefetch_match_metrics_list[0].get();
  }

  CHECK(prefetch_match_metrics_list[1]);

  // If `PrefetchURLLoaderInterceptor` with
  // `PrefetchServiceWorkerState::kControlled` didn't intercept and one with
  // `PrefetchServiceWorkerState::kDisallowed` entered prefetch matching, return
  // the latter. Return the first one otherwise.
  //
  // (We are not confident whether `size() >= 2` implies the first two is such
  // types or not.)
  if (prefetch_match_metrics_list[0]->expected_service_worker_state ==
          PrefetchServiceWorkerState::kControlled &&
      prefetch_match_metrics_list[1]->expected_service_worker_state ==
          PrefetchServiceWorkerState::kDisallowed &&
      prefetch_match_metrics_list[1]->IsPotentialMatch()) {
    return prefetch_match_metrics_list[1].get();
  } else {
    return prefetch_match_metrics_list[0].get();
  }
}

void PreloadServingMetrics::RecordMetricsForNonPrerenderNavigationCommitted()
    const {
  RecordMetricsInternal(*this, "PreloadServingMetrics.ForNavigationCommitted.",
                        /*is_prerender_initial_navigation=*/false);
  if (prerender_initial_preload_serving_metrics) {
    RecordMetricsInternal(
        *prerender_initial_preload_serving_metrics,
        "PreloadServingMetrics.ForPrerenderInitialNavigationUsed.",
        /*is_prerender_initial_navigation=*/true);
  }
}

void PreloadServingMetrics::RecordMetricsForPrerenderInitialNavigationFailed()
    const {
  CHECK(PreloadServingMetricsCapsule::IsFeatureEnabled());

  RecordMetricsInternal(
      *this, "PreloadServingMetrics.ForPrerenderInitialNavigationFailed.",
      /*is_prerender_initial_navigation=*/true);

  auto& metrics = *this;
  [&]() {
    const PrefetchMatchMetrics* meaningful_prefetch_match_metrics =
        metrics.GetMeaningfulPrefetchMatchMetrics();
    const bool is_potential_match =
        meaningful_prefetch_match_metrics &&
        meaningful_prefetch_match_metrics->IsPotentialMatch();
    if (!is_potential_match) {
      return;
    }
    auto& prefetch_match_metrics = *meaningful_prefetch_match_metrics;

    base::TimeDelta prefetch_match_duration =
        prefetch_match_metrics.time_match_end -
        prefetch_match_metrics.time_match_start;
    if (prefetch_match_duration >= base::Milliseconds(10000)) {
      RecordMetricsInternal(
          *this,
          "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
          "WithMatchDurationGe10000.",
          /*is_prerender_initial_navigation=*/true);
    }
  }();
}

void PreloadServingMetrics::RecordFirstContentfulPaint(
    base::TimeDelta corrected_first_contentful_paint) const {
  const bool is_prerender_used = !!prerender_initial_preload_serving_metrics;
  const PrefetchMatchMetrics* meaningful_prefetch_match_metrics =
      GetMeaningfulPrefetchMatchMetrics();
  const bool is_prefetch_actual_match =
      meaningful_prefetch_match_metrics &&
      meaningful_prefetch_match_metrics->IsActualMatch();

  const char* suffix;
  if (is_prerender_used) {
    suffix = ".WithPrerender";
  } else if (is_prefetch_actual_match) {
    suffix = ".WithPrefetch";
  } else {
    suffix = ".WithoutPreload";
  }
  PAGE_LOAD_HISTOGRAM(
      base::StrCat({"PreloadServingMetrics.PageLoad.Clients.PaintTiming."
                    "NavigationToFirstContentfulPaint",
                    suffix}),
      corrected_first_contentful_paint);
}

PreloadServingMetrics::PreloadServingMetrics() {
  CHECK(PreloadServingMetricsCapsule::IsFeatureEnabled());
}

PreloadServingMetrics::~PreloadServingMetrics() = default;

// static
std::unique_ptr<PreloadServingMetricsCapsule>
PreloadServingMetricsCapsuleImpl::TakeFromNavigationHandle(
    NavigationHandle& navigation_handle) {
  CHECK(PreloadServingMetricsCapsule::IsFeatureEnabled());

  return base::WrapUnique(new PreloadServingMetricsCapsuleImpl(
      PreloadServingMetricsHolder::GetOrCreateForNavigationHandle(
          navigation_handle)
          ->Take()));
}

PreloadServingMetricsCapsuleImpl::PreloadServingMetricsCapsuleImpl(
    std::unique_ptr<PreloadServingMetrics> preload_serving_metrics)
    : preload_serving_metrics_(std::move(preload_serving_metrics)) {
  CHECK(PreloadServingMetricsCapsule::IsFeatureEnabled());
}

PreloadServingMetricsCapsuleImpl::~PreloadServingMetricsCapsuleImpl() = default;

void PreloadServingMetricsCapsuleImpl::
    RecordMetricsForNonPrerenderNavigationCommitted() const {
  preload_serving_metrics_->RecordMetricsForNonPrerenderNavigationCommitted();
}

void PreloadServingMetricsCapsuleImpl::RecordFirstContentfulPaint(
    base::TimeDelta corrected_first_contentful_paint) const {
  preload_serving_metrics_->RecordFirstContentfulPaint(
      std::move(corrected_first_contentful_paint));
}

}  // namespace content
