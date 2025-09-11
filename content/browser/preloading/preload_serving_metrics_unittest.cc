// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preload_serving_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/preloading/prefetch/prefetch_match_resolver.h"
#include "content/browser/preloading/prerender/prerender_features.h"
#include "content/common/features.h"
#include "content/public/browser/preloading.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

struct MakeSkeltonPreloadServingMetricsArgs {
  int n_prefetch_match_metrics;
};

std::unique_ptr<PreloadServingMetrics> MakeSkeletonPreloadServingMetrics(
    MakeSkeltonPreloadServingMetricsArgs args) {
  auto ret = std::make_unique<PreloadServingMetrics>();

  for (int i = 0; i < args.n_prefetch_match_metrics; ++i) {
    ret->prefetch_match_metrics_list.push_back(
        std::make_unique<PrefetchMatchMetrics>());
  }

  return ret;
}

base::TimeTicks Millis(int ms) {
  return base::TimeTicks::UnixEpoch() + base::Milliseconds(ms);
}

TEST(PreloadServingMetricsTest, NavigationWithoutPreload) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {
          {
              features::kPrerender2FallbackPrefetchSpecRules,
              {
                  {"kPrerender2FallbackUsePreloadServingMetrics", "true"},
              },
          },
      },
      {});
  base::HistogramTester histogram_tester;

  auto log = MakeSkeletonPreloadServingMetrics({.n_prefetch_match_metrics = 0});
  log->prerender_initial_preload_serving_metrics = nullptr;

  log->RecordMetricsForNonPrerenderNavigationCommitted();
  log->RecordFirstContentfulPaint(base::Milliseconds(334));

  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics.Count",
      0, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "IsPotentialMatch",
      0, 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.NumberOfInitialCandidates",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.NumberOfInitialCandidatesBlockUntilHead",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.IsActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.MatchDuration",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.MatchDuration.ForActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.MatchDuration.ForNotActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "ActualMatchThen.TimeFromPrefetchContainerAddedToMatchStart",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "IsPotentialMatch.WithAheadOfPrerender",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.WithAheadOfPrerender.PotentialCandidateServingResult",
      0);

  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.Count",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.IsPotentialMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.NumberOfInitialCandidates",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen."
      "NumberOfInitialCandidatesBlockUntilHead",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.IsActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration.ForActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration.ForNotActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.ActualMatchThen."
      "TimeFromPrefetchContainerAddedToMatchStart",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.IsPotentialMatch.WithAheadOfPrerender",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.WithAheadOfPrerender."
      "PotentialCandidateServingResult",
      0);

  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.Count",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.IsPotentialMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.NumberOfInitialCandidates",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen."
      "NumberOfInitialCandidatesBlockUntilHead",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.IsActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration.ForActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration.ForNotActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.ActualMatchThen."
      "TimeFromPrefetchContainerAddedToMatchStart",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.IsPotentialMatch.WithAheadOfPrerender",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.WithAheadOfPrerender."
      "PotentialCandidateServingResult",
      0);

  histogram_tester.ExpectUniqueTimeSample(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithoutPreload",
      base::Milliseconds(334), 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrefetch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrerender",
      0);
}

TEST(PreloadServingMetricsTest, NavigationWithPrefetch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {
          {
              features::kPrerender2FallbackPrefetchSpecRules,
              {
                  {"kPrerender2FallbackUsePreloadServingMetrics", "true"},
              },
          },
      },
      {});
  base::HistogramTester histogram_tester;

  auto log = MakeSkeletonPreloadServingMetrics({.n_prefetch_match_metrics = 1});
  log->prefetch_match_metrics_list[0]->time_match_start = Millis(42);
  log->prefetch_match_metrics_list[0]->time_match_end = Millis(57);
  log->prefetch_match_metrics_list[0]->n_initial_candidates = 1;
  log->prefetch_match_metrics_list[0]->n_initial_candidates_block_until_head =
      1;
  log->prefetch_match_metrics_list[0]->prefetch_container_metrics =
      std::make_unique<PrefetchContainerMetrics>();
  log->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics->time_added_to_prefetch_service = Millis(10);
  log->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics->time_initial_eligibility_got = Millis(200);
  log->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics->time_prefetch_started = Millis(3000);
  log->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics->time_url_request_started = Millis(40000);
  log->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics->time_header_determined_successfully =
      Millis(500000);
  log->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics->time_prefetch_completed_successfully =
      std::nullopt;
  log->prefetch_match_metrics_list[0]
      ->prefetch_potential_candidate_serving_result_ahead_of_prerender =
      std::nullopt;
  log->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender = nullptr;
  log->prerender_initial_preload_serving_metrics = nullptr;

  log->RecordMetricsForNonPrerenderNavigationCommitted();
  log->RecordFirstContentfulPaint(base::Milliseconds(334));

  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics.Count",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "IsPotentialMatch",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.NumberOfInitialCandidates",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.NumberOfInitialCandidatesBlockUntilHead",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.IsActualMatch",
      1, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.MatchDuration",
      Millis(57) - Millis(42), 1);
  histogram_tester.ExpectUniqueTimeSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.MatchDuration.ForActualMatch",
      Millis(57) - Millis(42), 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.MatchDuration.ForNotActualMatch",
      0);
  histogram_tester.ExpectUniqueTimeSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "ActualMatchThen.TimeFromPrefetchContainerAddedToMatchStart",
      Millis(42) - Millis(10), 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "IsPotentialMatch.WithAheadOfPrerender",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.WithAheadOfPrerender.PotentialCandidateServingResult",
      0);

  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.Count",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.IsPotentialMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.NumberOfInitialCandidates",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen."
      "NumberOfInitialCandidatesBlockUntilHead",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.IsActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration.ForActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration.ForNotActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.ActualMatchThen."
      "TimeFromPrefetchContainerAddedToMatchStart",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.IsPotentialMatch.WithAheadOfPrerender",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.WithAheadOfPrerender."
      "PotentialCandidateServingResult",
      0);

  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.Count",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.IsPotentialMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.NumberOfInitialCandidates",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen."
      "NumberOfInitialCandidatesBlockUntilHead",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.IsActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration.ForActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration.ForNotActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.ActualMatchThen."
      "TimeFromPrefetchContainerAddedToMatchStart",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.IsPotentialMatch.WithAheadOfPrerender",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.WithAheadOfPrerender."
      "PotentialCandidateServingResult",
      0);

  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithoutPreload",
      0);
  histogram_tester.ExpectUniqueTimeSample(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrefetch",
      base::Milliseconds(334), 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrerender",
      0);
}

TEST(PreloadServingMetricsTest,
     NavigationWithPrerenderWithPrefetchAheadOfPrerender) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {
          {
              features::kPrerender2FallbackPrefetchSpecRules,
              {
                  {"kPrerender2FallbackUsePreloadServingMetrics", "true"},
              },
          },
      },
      {});
  base::HistogramTester histogram_tester;

  auto log_prerender =
      MakeSkeletonPreloadServingMetrics({.n_prefetch_match_metrics = 1});
  log_prerender->prefetch_match_metrics_list[0]->time_match_start =
      Millis(3042);
  log_prerender->prefetch_match_metrics_list[0]->time_match_end = Millis(3057);
  log_prerender->prefetch_match_metrics_list[0]->n_initial_candidates = 1;
  log_prerender->prefetch_match_metrics_list[0]
      ->n_initial_candidates_block_until_head = 1;
  log_prerender->prefetch_match_metrics_list[0]->prefetch_container_metrics =
      std::make_unique<PrefetchContainerMetrics>();
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics->time_added_to_prefetch_service = Millis(10);
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics->time_initial_eligibility_got = Millis(200);
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics->time_prefetch_started = Millis(3000);
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics->time_url_request_started = Millis(40000);
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics->time_header_determined_successfully =
      Millis(500000);
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics->time_prefetch_completed_successfully =
      std::nullopt;
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_potential_candidate_serving_result_ahead_of_prerender =
      std::nullopt;
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender = nullptr;
  log_prerender->prerender_initial_preload_serving_metrics = nullptr;
  auto log = MakeSkeletonPreloadServingMetrics({.n_prefetch_match_metrics = 0});
  log->prerender_initial_preload_serving_metrics = std::move(log_prerender);

  log->RecordMetricsForNonPrerenderNavigationCommitted();
  log->RecordFirstContentfulPaint(base::Milliseconds(334));

  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics.Count",
      0, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "IsPotentialMatch",
      false, 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.NumberOfInitialCandidates",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.NumberOfInitialCandidatesBlockUntilHead",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PreloadServingMetrics."
      "ForNavigationCommitted.PrefetchMatchMetrics.PotentialMatchThen."
      "IsActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.MatchDuration",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.MatchDuration.ForActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.MatchDuration.ForNotActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "ActualMatchThen.TimeFromPrefetchContainerAddedToMatchStart",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "IsPotentialMatch.WithAheadOfPrerender",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.WithAheadOfPrerender.PotentialCandidateServingResult",
      0);

  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.Count",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.IsPotentialMatch",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.NumberOfInitialCandidates",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen."
      "NumberOfInitialCandidatesBlockUntilHead",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.IsActualMatch",
      1, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration",
      Millis(3057) - Millis(3042), 1);
  histogram_tester.ExpectUniqueTimeSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration.ForActualMatch",
      Millis(3057) - Millis(3042), 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration.ForNotActualMatch",
      0);
  histogram_tester.ExpectUniqueTimeSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.ActualMatchThen."
      "TimeFromPrefetchContainerAddedToMatchStart",
      Millis(3042) - Millis(10), 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.IsPotentialMatch.WithAheadOfPrerender",
      false, 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.WithAheadOfPrerender."
      "PotentialCandidateServingResult",
      0);

  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.Count",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.IsPotentialMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.NumberOfInitialCandidates",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen."
      "NumberOfInitialCandidatesBlockUntilHead",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.IsActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration.ForActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration.ForNotActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.ActualMatchThen."
      "TimeFromPrefetchContainerAddedToMatchStart",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.IsPotentialMatch.WithAheadOfPrerender",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.WithAheadOfPrerender."
      "PotentialCandidateServingResult",
      0);

  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithoutPreload",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrefetch",
      0);
  histogram_tester.ExpectUniqueTimeSample(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrerender",
      base::Milliseconds(334), 1);
}

TEST(PreloadServingMetricsTest,
     PrefetchTriggeredPrerenderTriggeredNavigationStartedPrefetchFailed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {
          {
              features::kPrerender2FallbackPrefetchSpecRules,
              {
                  {"kPrerender2FallbackUsePreloadServingMetrics", "true"},
              },
          },
      },
      {});
  base::HistogramTester histogram_tester;

  auto log_prerender =
      MakeSkeletonPreloadServingMetrics({.n_prefetch_match_metrics = 1});
  log_prerender->prefetch_match_metrics_list[0]->time_match_start = Millis(42);
  log_prerender->prefetch_match_metrics_list[0]->time_match_end = Millis(57);
  log_prerender->prefetch_match_metrics_list[0]->n_initial_candidates = 1;
  log_prerender->prefetch_match_metrics_list[0]
      ->n_initial_candidates_block_until_head = 1;
  log_prerender->prefetch_match_metrics_list[0]->prefetch_container_metrics =
      nullptr;
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_potential_candidate_serving_result_ahead_of_prerender =
      PrefetchPotentialCandidateServingResult::kNotServedBlockUntilHeadTimeout;
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender =
      std::make_unique<PrefetchContainerMetrics>();
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender
      ->time_added_to_prefetch_service = Millis(10);
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender
      ->time_initial_eligibility_got = Millis(200);
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender->time_prefetch_started =
      Millis(3000);
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender
      ->time_url_request_started = Millis(40000);
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender
      ->time_header_determined_successfully = std::nullopt;
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender
      ->time_prefetch_completed_successfully = std::nullopt;
  log_prerender->prerender_initial_preload_serving_metrics = nullptr;
  auto log = MakeSkeletonPreloadServingMetrics({.n_prefetch_match_metrics = 1});
  log->prefetch_match_metrics_list[0]->time_match_start = Millis(57);
  log->prefetch_match_metrics_list[0]->time_match_end = Millis(57);
  log->prefetch_match_metrics_list[0]->n_initial_candidates = 1;
  log->prefetch_match_metrics_list[0]->n_initial_candidates_block_until_head =
      0;
  log->prefetch_match_metrics_list[0]->prefetch_container_metrics = nullptr;
  log->prefetch_match_metrics_list[0]
      ->prefetch_potential_candidate_serving_result_ahead_of_prerender =
      std::nullopt;
  log->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender = nullptr;
  log->prerender_initial_preload_serving_metrics = nullptr;

  log_prerender->RecordMetricsForPrerenderInitialNavigationFailed();
  log->RecordMetricsForNonPrerenderNavigationCommitted();
  log->RecordFirstContentfulPaint(base::Milliseconds(334));

  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics.Count",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "IsPotentialMatch",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.NumberOfInitialCandidates",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.NumberOfInitialCandidatesBlockUntilHead",
      0, 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PreloadServingMetrics."
      "ForNavigationCommitted.PrefetchMatchMetrics.PotentialMatchThen."
      "IsActualMatch",
      0);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.MatchDuration",
      0, 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.MatchDuration.ForActualMatch",
      0);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.MatchDuration.ForNotActualMatch",
      0, 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "ActualMatchThen.TimeFromPrefetchContainerAddedToMatchStart",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "IsPotentialMatch.WithAheadOfPrerender",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.WithAheadOfPrerender.PotentialCandidateServingResult",
      0);

  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.Count",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.IsPotentialMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.NumberOfInitialCandidates",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen."
      "NumberOfInitialCandidatesBlockUntilHead",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.IsActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration.ForActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration.ForNotActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.ActualMatchThen."
      "TimeFromPrefetchContainerAddedToMatchStart",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.IsPotentialMatch.WithAheadOfPrerender",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.WithAheadOfPrerender."
      "PotentialCandidateServingResult",
      0);

  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.Count",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.IsPotentialMatch",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.NumberOfInitialCandidates",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen."
      "NumberOfInitialCandidatesBlockUntilHead",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.IsActualMatch",
      false, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration",
      Millis(57) - Millis(42), 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration.ForActualMatch",
      0);
  histogram_tester.ExpectUniqueTimeSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration.ForNotActualMatch",
      Millis(57) - Millis(42), 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.ActualMatchThen."
      "TimeFromPrefetchContainerAddedToMatchStart",
      0);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.IsPotentialMatch.WithAheadOfPrerender",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.WithAheadOfPrerender."
      "PotentialCandidateServingResult",
      PrefetchPotentialCandidateServingResult::kNotServedBlockUntilHeadTimeout,
      1);

  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "WithMatchDurationGe10000.PrefetchMatchMetrics.Count",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "WithMatchDurationGe10000.PrefetchMatchMetrics.IsPotentialMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "WithMatchDurationGe10000.PrefetchMatchMetrics.PotentialMatchThen."
      "NumberOfInitialCandidates",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "WithMatchDurationGe10000.PrefetchMatchMetrics.PotentialMatchThen."
      "NumberOfInitialCandidatesBlockUntilHead",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "WithMatchDurationGe10000.PrefetchMatchMetrics.PotentialMatchThen."
      "IsActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "WithMatchDurationGe10000.PrefetchMatchMetrics.PotentialMatchThen."
      "MatchDuration",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "WithMatchDurationGe10000.PrefetchMatchMetrics.PotentialMatchThen."
      "MatchDuration.ForActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "WithMatchDurationGe10000.PrefetchMatchMetrics.PotentialMatchThen."
      "MatchDuration.ForNotActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "WithMatchDurationGe10000.PrefetchMatchMetrics.ActualMatchThen."
      "TimeFromPrefetchContainerAddedToMatchStart",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "WithMatchDurationGe10000.PrefetchMatchMetrics.IsPotentialMatch."
      "WithAheadOfPrerender",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "WithMatchDurationGe10000.PrefetchMatchMetrics.PotentialMatchThen."
      "WithAheadOfPrerender.PotentialCandidateServingResult",
      0);

  histogram_tester.ExpectUniqueTimeSample(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithoutPreload",
      base::Milliseconds(334), 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrefetch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrerender",
      0);
}

TEST(
    PreloadServingMetricsTest,
    PrefetchTriggeredPrerenderTriggeredNavigationStartedPrefetchFailedDurationGe10000) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {
          {
              features::kPrerender2FallbackPrefetchSpecRules,
              {
                  {"kPrerender2FallbackUsePreloadServingMetrics", "true"},
              },
          },
      },
      {});
  base::HistogramTester histogram_tester;

  auto log_prerender =
      MakeSkeletonPreloadServingMetrics({.n_prefetch_match_metrics = 1});
  log_prerender->prefetch_match_metrics_list[0]->time_match_start = Millis(42);
  log_prerender->prefetch_match_metrics_list[0]->time_match_end = Millis(10057);
  log_prerender->prefetch_match_metrics_list[0]->n_initial_candidates = 1;
  log_prerender->prefetch_match_metrics_list[0]
      ->n_initial_candidates_block_until_head = 1;
  log_prerender->prefetch_match_metrics_list[0]->prefetch_container_metrics =
      nullptr;
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_potential_candidate_serving_result_ahead_of_prerender =
      PrefetchPotentialCandidateServingResult::kNotServedBlockUntilHeadTimeout;
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender =
      std::make_unique<PrefetchContainerMetrics>();
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender
      ->time_added_to_prefetch_service = Millis(10);
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender
      ->time_initial_eligibility_got = Millis(200);
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender->time_prefetch_started =
      Millis(3000);
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender
      ->time_url_request_started = Millis(40000);
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender
      ->time_header_determined_successfully = std::nullopt;
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender
      ->time_prefetch_completed_successfully = std::nullopt;
  log_prerender->prerender_initial_preload_serving_metrics = nullptr;
  auto log = MakeSkeletonPreloadServingMetrics({.n_prefetch_match_metrics = 1});
  log->prefetch_match_metrics_list[0]->time_match_start = Millis(57);
  log->prefetch_match_metrics_list[0]->time_match_end = Millis(57);
  log->prefetch_match_metrics_list[0]->n_initial_candidates = 1;
  log->prefetch_match_metrics_list[0]->n_initial_candidates_block_until_head =
      0;
  log->prefetch_match_metrics_list[0]->prefetch_container_metrics = nullptr;
  log->prefetch_match_metrics_list[0]
      ->prefetch_potential_candidate_serving_result_ahead_of_prerender =
      std::nullopt;
  log->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender = nullptr;
  log->prerender_initial_preload_serving_metrics = nullptr;

  log_prerender->RecordMetricsForPrerenderInitialNavigationFailed();
  log->RecordMetricsForNonPrerenderNavigationCommitted();
  log->RecordFirstContentfulPaint(base::Milliseconds(334));

  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics.Count",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "IsPotentialMatch",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.NumberOfInitialCandidates",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.NumberOfInitialCandidatesBlockUntilHead",
      0, 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PreloadServingMetrics."
      "ForNavigationCommitted.PrefetchMatchMetrics.PotentialMatchThen."
      "IsActualMatch",
      0);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.MatchDuration",
      0, 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.MatchDuration.ForActualMatch",
      0);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.MatchDuration.ForNotActualMatch",
      0, 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "ActualMatchThen.TimeFromPrefetchContainerAddedToMatchStart",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "IsPotentialMatch.WithAheadOfPrerender",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PotentialMatchThen.WithAheadOfPrerender.PotentialCandidateServingResult",
      0);

  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.Count",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.IsPotentialMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.NumberOfInitialCandidates",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen."
      "NumberOfInitialCandidatesBlockUntilHead",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.IsActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration.ForActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration.ForNotActualMatch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.ActualMatchThen."
      "TimeFromPrefetchContainerAddedToMatchStart",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.IsPotentialMatch.WithAheadOfPrerender",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.WithAheadOfPrerender."
      "PotentialCandidateServingResult",
      0);

  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.Count",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.IsPotentialMatch",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.NumberOfInitialCandidates",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen."
      "NumberOfInitialCandidatesBlockUntilHead",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.IsActualMatch",
      false, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration",
      Millis(10057) - Millis(42), 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration.ForActualMatch",
      0);
  histogram_tester.ExpectUniqueTimeSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration.ForNotActualMatch",
      Millis(10057) - Millis(42), 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.ActualMatchThen."
      "TimeFromPrefetchContainerAddedToMatchStart",
      0);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.IsPotentialMatch.WithAheadOfPrerender",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.WithAheadOfPrerender."
      "PotentialCandidateServingResult",
      PrefetchPotentialCandidateServingResult::kNotServedBlockUntilHeadTimeout,
      1);

  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "WithMatchDurationGe10000.PrefetchMatchMetrics.Count",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "WithMatchDurationGe10000.PrefetchMatchMetrics.IsPotentialMatch",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "WithMatchDurationGe10000.PrefetchMatchMetrics.PotentialMatchThen."
      "NumberOfInitialCandidates",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "WithMatchDurationGe10000.PrefetchMatchMetrics.PotentialMatchThen."
      "NumberOfInitialCandidatesBlockUntilHead",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "WithMatchDurationGe10000.PrefetchMatchMetrics.PotentialMatchThen."
      "IsActualMatch",
      false, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "WithMatchDurationGe10000.PrefetchMatchMetrics.PotentialMatchThen."
      "MatchDuration",
      Millis(10057) - Millis(42), 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "WithMatchDurationGe10000.PrefetchMatchMetrics.PotentialMatchThen."
      "MatchDuration.ForActualMatch",
      0);
  histogram_tester.ExpectUniqueTimeSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "WithMatchDurationGe10000.PrefetchMatchMetrics.PotentialMatchThen."
      "MatchDuration.ForNotActualMatch",
      Millis(10057) - Millis(42), 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "WithMatchDurationGe10000.PrefetchMatchMetrics.ActualMatchThen."
      "TimeFromPrefetchContainerAddedToMatchStart",
      0);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "WithMatchDurationGe10000.PrefetchMatchMetrics.IsPotentialMatch."
      "WithAheadOfPrerender",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "WithMatchDurationGe10000.PrefetchMatchMetrics.PotentialMatchThen."
      "WithAheadOfPrerender.PotentialCandidateServingResult",
      PrefetchPotentialCandidateServingResult::kNotServedBlockUntilHeadTimeout,
      1);

  histogram_tester.ExpectUniqueTimeSample(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithoutPreload",
      base::Milliseconds(334), 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrefetch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrerender",
      0);
}

}  // namespace content
