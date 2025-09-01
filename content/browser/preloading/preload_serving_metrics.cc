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

namespace {

// Copied from components/page_load_metrics/browser/page_load_metrics_util.h
#define PAGE_LOAD_HISTOGRAM(name, sample)                             \
  base::UmaHistogramCustomTimes(name, sample, base::Milliseconds(10), \
                                base::Minutes(10), 100)

#define WITH(prefix, name) base::StrCat({prefix, name})

void RecordMetricsInternal(const PreloadServingMetrics& metrics,
                           const char* prefix) {
  // We expect that prefetch match count is zero or one.
  base::UmaHistogramCounts100(WITH(prefix, "PrefetchMatchMetrics.Count"),
                              metrics.prefetch_match_metrics_list.size());

  [&]() {
    // We only checks the first prefetch matching, as it is most likely to have
    // meaningful data and checking other ones is costly with UMAs.
    //
    // TODO(crbug.com/360094997): Consider to use UKM.

    const bool is_potential_match =
        metrics.prefetch_match_metrics_list.size() > 0 &&
        metrics.prefetch_match_metrics_list[0] &&
        metrics.prefetch_match_metrics_list[0]->n_initial_candidates > 0;

    base::UmaHistogramBoolean(
        WITH(prefix, "PrefetchMatchMetrics.IsPotentialMatch"),
        is_potential_match);

    if (!is_potential_match) {
      return;
    }
    auto& prefetch_match_metrics = *metrics.prefetch_match_metrics_list[0];

    base::UmaHistogramCounts100(WITH(prefix,
                                     "PrefetchMatchMetrics.PotentialMatchThen."
                                     "NumberOfInitialCandidates"),
                                prefetch_match_metrics.n_initial_candidates);
    base::UmaHistogramCounts100(
        WITH(prefix,
             "PrefetchMatchMetrics.PotentialMatchThen."
             "NumberOfInitialCandidatesBlockUntilHead"),
        prefetch_match_metrics.n_initial_candidates_block_until_head);
    const bool is_actual_match =
        !!prefetch_match_metrics.prefetch_container_metrics;
    base::UmaHistogramBoolean(
        WITH(prefix, "PrefetchMatchMetrics.PotentialMatchThen.IsActualMatch"),
        is_actual_match);
  }();
}

}  // namespace

PrefetchContainerMetrics::PrefetchContainerMetrics() = default;

PrefetchContainerMetrics::~PrefetchContainerMetrics() = default;

PrefetchMatchMetrics::PrefetchMatchMetrics() = default;

PrefetchMatchMetrics::~PrefetchMatchMetrics() = default;

// static
bool PreloadServingMetrics::IsEnabled() {
  return features::kPrerender2FallbackUsePreloadServingMetrics.Get() ||
         GetContentClient()->browser()->UsePreloadServingMetrics();
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

void PreloadServingMetrics::RecordMetricsForNonPrerenderNavigationCommitted()
    const {
  RecordMetricsInternal(*this, "PreloadServingMetrics.ForNavigationCommitted.");
  if (prerender_initial_preload_serving_metrics) {
    RecordMetricsInternal(
        *prerender_initial_preload_serving_metrics,
        "PreloadServingMetrics.ForPrerenderInitialNavigationUsed.");
  }
}

void PreloadServingMetrics::RecordFirstContentfulPaint(
    base::TimeDelta corrected_first_contentful_paint) const {
  const bool is_prerender_used = !!prerender_initial_preload_serving_metrics;
  const bool is_prefetch_potential_match =
      prefetch_match_metrics_list.size() > 0 &&
      prefetch_match_metrics_list[0] &&
      prefetch_match_metrics_list[0]->n_initial_candidates > 0;
  const bool is_prefetch_actual_match =
      is_prefetch_potential_match &&
      !!prefetch_match_metrics_list[0]->prefetch_container_metrics;

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
  CHECK(PreloadServingMetrics::IsEnabled());
}

PreloadServingMetrics::~PreloadServingMetrics() = default;

// static
std::unique_ptr<PreloadServingMetricsCapsule>
PreloadServingMetricsCapsuleImpl::TakeFromNavigationHandle(
    NavigationHandle& navigation_handle) {
  CHECK(PreloadServingMetrics::IsEnabled());

  return base::WrapUnique(new PreloadServingMetricsCapsuleImpl(
      PreloadServingMetricsHolder::GetOrCreateForNavigationHandle(
          navigation_handle)
          ->Take()));
}

PreloadServingMetricsCapsuleImpl::PreloadServingMetricsCapsuleImpl(
    std::unique_ptr<PreloadServingMetrics> preload_serving_metrics)
    : preload_serving_metrics_(std::move(preload_serving_metrics)) {
  CHECK(PreloadServingMetrics::IsEnabled());
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
