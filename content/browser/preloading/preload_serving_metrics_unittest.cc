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

// Scenario:
//
// - Navigation A started.
// - A committed.
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
  log->is_prerender_aborted_by_prerender_url_loader_throttle = false;
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
      "PotentialMatchThen.PotentialCandidateServingResult.Last",
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
      "PrefetchMatchMetrics.PotentialMatchThen.PotentialCandidateServingResult."
      "Last",
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
      "PrefetchMatchMetrics.PotentialMatchThen.PotentialCandidateServingResult."
      "Last",
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

// Scenario:
//
// - Prefetch A is triggered.
// - Navigation B started.
//   - B is blocked by A.
// - A succeeded.
//   - It unblocks B.
// - B committed.
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
      ->prefetch_container_metrics->time_initial_eligibility_got = Millis(20);
  log->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics->time_prefetch_started = Millis(30);
  log->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics->time_url_request_started = Millis(40);
  log->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics->time_header_determined_successfully =
      Millis(500000);
  log->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics->time_prefetch_completed_successfully =
      std::nullopt;
  log->prefetch_match_metrics_list[0]
      ->prefetch_potential_candidate_serving_result_last =
      PrefetchPotentialCandidateServingResult::kServed;
  log->prefetch_match_metrics_list[0]
      ->prefetch_potential_candidate_serving_result_ahead_of_prerender =
      std::nullopt;
  log->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender = nullptr;
  log->prefetch_match_metrics_list[0]->prerender_debug_metrics = nullptr;
  log->is_prerender_aborted_by_prerender_url_loader_throttle = false;
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
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics."
      "PrefetchMatchMetrics.PotentialMatchThen.PotentialCandidateServingResult."
      "Last",
      0);
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
      "PrefetchMatchMetrics.PotentialMatchThen.PotentialCandidateServingResult."
      "Last",
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
      "PrefetchMatchMetrics.PotentialMatchThen.PotentialCandidateServingResult."
      "Last",
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

// Scenario:
//
// - Prefetch A is triggered.
// - Prerender B is triggered.
//   - B is blocked by A.
// - Navigation C started.
//   - C is blocked by B.
// - A succeeded.
//   - It unblocks B.
// - B committed.
// - C commmtted.
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
      ->prefetch_potential_candidate_serving_result_last =
      PrefetchPotentialCandidateServingResult::kServed;
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_potential_candidate_serving_result_ahead_of_prerender =
      std::nullopt;
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender = nullptr;
  // Actually, this case should have non null
  // `prerender_debug_metrics`, but we omit it as we
  // don't check the UMAs in test.
  log_prerender->prefetch_match_metrics_list[0]->prerender_debug_metrics =
      nullptr;
  log_prerender->is_prerender_aborted_by_prerender_url_loader_throttle = false;
  log_prerender->prerender_initial_preload_serving_metrics = nullptr;
  auto log = MakeSkeletonPreloadServingMetrics({.n_prefetch_match_metrics = 0});
  log->is_prerender_aborted_by_prerender_url_loader_throttle = false;
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
      "PreloadServingMetrics.ForNavigationCommitted.PreloadServingMetrics."
      "PrefetchMatchMetrics.PotentialMatchThen.PotentialCandidateServingResult."
      "Last",
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
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.PotentialMatchThen.PotentialCandidateServingResult."
      "Last",
      PrefetchPotentialCandidateServingResult::kServed, 1);
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
      "PrefetchMatchMetrics.PotentialMatchThen.PotentialCandidateServingResult."
      "Last",
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

// Scenario:
//
// - Prefetch A is triggered.
// - Prerender B is triggered.
//   - B is blocked by A.
// - Navigation C started.
//   - C is blocked by B.
// - A failed. (Timeout of `PrefetchStreamingURLLoader`)
//   - It unblocks B.
//   - B failed due to the prefetch failure.
// - C passes prefech matching, not blocked by A as it has been failed.
// - C falls back to network.
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
  log_prerender->prefetch_match_metrics_list[0]->time_match_end = Millis(1057);
  log_prerender->prefetch_match_metrics_list[0]->n_initial_candidates = 1;
  log_prerender->prefetch_match_metrics_list[0]
      ->n_initial_candidates_block_until_head = 1;
  log_prerender->prefetch_match_metrics_list[0]->prefetch_container_metrics =
      nullptr;
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_potential_candidate_serving_result_last =
      PrefetchPotentialCandidateServingResult::kNotServedLoadFailed;
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_potential_candidate_serving_result_ahead_of_prerender =
      PrefetchPotentialCandidateServingResult::kNotServedLoadFailed;
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender =
      std::make_unique<PrefetchContainerMetrics>();
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender
      ->time_added_to_prefetch_service = Millis(10);
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender
      ->time_initial_eligibility_got = Millis(20);
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender->time_prefetch_started =
      Millis(300);
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender
      ->time_url_request_started = Millis(400);
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender
      ->time_header_determined_successfully = std::nullopt;
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender
      ->time_prefetch_completed_successfully = std::nullopt;
  // Actually, this case should have non null
  // `prerender_debug_metrics`, but we omit it as we
  // don't check the UMAs in test.
  log_prerender->prefetch_match_metrics_list[0]->prerender_debug_metrics =
      nullptr;
  log_prerender->is_prerender_aborted_by_prerender_url_loader_throttle = true;
  log_prerender->prerender_initial_preload_serving_metrics = nullptr;
  auto log = MakeSkeletonPreloadServingMetrics({.n_prefetch_match_metrics = 1});
  log->prefetch_match_metrics_list[0]->time_match_start = Millis(1157);
  log->prefetch_match_metrics_list[0]->time_match_end = Millis(1157);
  log->prefetch_match_metrics_list[0]->n_initial_candidates = 0;
  log->prefetch_match_metrics_list[0]->n_initial_candidates_block_until_head =
      0;
  log->prefetch_match_metrics_list[0]->prefetch_container_metrics = nullptr;
  log->prefetch_match_metrics_list[0]
      ->prefetch_potential_candidate_serving_result_last =
      PrefetchPotentialCandidateServingResult::kNotServedLoadFailed;
  log->prefetch_match_metrics_list[0]
      ->prefetch_potential_candidate_serving_result_ahead_of_prerender =
      std::nullopt;
  log->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender = nullptr;
  log->prefetch_match_metrics_list[0]->prerender_debug_metrics = nullptr;
  log->is_prerender_aborted_by_prerender_url_loader_throttle = false;
  log->prerender_initial_preload_serving_metrics = nullptr;

  log_prerender->RecordMetricsForPrerenderInitialNavigationFailed();
  log->RecordMetricsForNonPrerenderNavigationCommitted();
  log->RecordFirstContentfulPaint(base::Milliseconds(2157));

  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics.Count",
      1, 1);
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
      "PreloadServingMetrics.ForNavigationCommitted.PreloadServingMetrics."
      "PrefetchMatchMetrics.PotentialMatchThen.PotentialCandidateServingResult."
      "Last",
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
      "PrefetchMatchMetrics.PotentialMatchThen.PotentialCandidateServingResult."
      "Last",
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
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.PotentialCandidateServingResult."
      "Last",
      PrefetchPotentialCandidateServingResult::kNotServedLoadFailed, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration",
      Millis(1057) - Millis(42), 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration.ForActualMatch",
      0);
  histogram_tester.ExpectUniqueTimeSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.MatchDuration.ForNotActualMatch",
      Millis(1057) - Millis(42), 1);
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
      PrefetchPotentialCandidateServingResult::kNotServedLoadFailed, 1);

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
      "PotentialCandidateServingResult.Last",
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
      base::Milliseconds(2157), 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrefetch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrerender",
      0);
}

// Variant of PrefetchTriggeredPrerenderTriggeredNavigationStartedPrefetchFailed
//
// Prefetch matching took greater than or equal to 10000ms.
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
      ->prefetch_potential_candidate_serving_result_last =
      PrefetchPotentialCandidateServingResult::kNotServedLoadFailed;
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_potential_candidate_serving_result_ahead_of_prerender =
      PrefetchPotentialCandidateServingResult::kNotServedLoadFailed;
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
  // Actually, this case should have non null
  // `prerender_debug_metrics`, but we omit it as we
  // don't check the UMAs in test.
  log_prerender->prefetch_match_metrics_list[0]->prerender_debug_metrics =
      nullptr;
  log_prerender->is_prerender_aborted_by_prerender_url_loader_throttle = true;
  log_prerender->prerender_initial_preload_serving_metrics = nullptr;
  auto log = MakeSkeletonPreloadServingMetrics({.n_prefetch_match_metrics = 1});
  log->prefetch_match_metrics_list[0]->time_match_start = Millis(10157);
  log->prefetch_match_metrics_list[0]->time_match_end = Millis(10157);
  log->prefetch_match_metrics_list[0]->n_initial_candidates = 0;
  log->prefetch_match_metrics_list[0]->n_initial_candidates_block_until_head =
      0;
  log->prefetch_match_metrics_list[0]->prefetch_container_metrics = nullptr;
  log->prefetch_match_metrics_list[0]
      ->prefetch_potential_candidate_serving_result_last = std::nullopt;
  log->prefetch_match_metrics_list[0]
      ->prefetch_potential_candidate_serving_result_ahead_of_prerender =
      std::nullopt;
  log->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender = nullptr;
  log->prefetch_match_metrics_list[0]->prerender_debug_metrics = nullptr;
  log->is_prerender_aborted_by_prerender_url_loader_throttle = false;
  log->prerender_initial_preload_serving_metrics = nullptr;

  log_prerender->RecordMetricsForPrerenderInitialNavigationFailed();
  log->RecordMetricsForNonPrerenderNavigationCommitted();
  log->RecordFirstContentfulPaint(base::Milliseconds(10334));

  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics.Count",
      1, 1);
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
      "PotentialMatchThen.PotentialCandidateServingResult.Last",
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
      "PrefetchMatchMetrics.PotentialMatchThen.PotentialCandidateServingResult."
      "Last",
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
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "PrefetchMatchMetrics.PotentialMatchThen.PotentialCandidateServingResult."
      "Last",
      PrefetchPotentialCandidateServingResult::kNotServedLoadFailed, 1);
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
      PrefetchPotentialCandidateServingResult::kNotServedLoadFailed, 1);

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
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "WithMatchDurationGe10000.PrefetchMatchMetrics.PotentialMatchThen."
      "PotentialCandidateServingResult.Last",
      PrefetchPotentialCandidateServingResult::kNotServedLoadFailed, 1);
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
      PrefetchPotentialCandidateServingResult::kNotServedLoadFailed, 1);

  histogram_tester.ExpectUniqueTimeSample(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithoutPreload",
      base::Milliseconds(10334), 1);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrefetch",
      0);
  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrerender",
      0);
}

// Check for `PrefetchMatchPrerenderDebugMetrics`
//
// Scenario:
//
// - Prefetch A is triggered.
// - Prerender B is triggered.
//   - But B is not blocked by prefech matching as A is not
//     `OnPrefetchStarted()`. (We don't expect this case occurs if
//     `UsePrefetchPrerenderIntegration()`.)
// - B is cancelled by `PrerenderURLLoaderThrottle`.
// - Navigation C started.
// - C falls back to network.
TEST(PreloadServingMetricsTest, PrefetchMatchPrerenderDebugMetrics) {
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
  log_prerender->prefetch_match_metrics_list[0]->time_match_end = Millis(43);
  log_prerender->prefetch_match_metrics_list[0]->n_initial_candidates = 0;
  log_prerender->prefetch_match_metrics_list[0]
      ->n_initial_candidates_block_until_head = 0;
  log_prerender->prefetch_match_metrics_list[0]->prefetch_container_metrics =
      nullptr;
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_potential_candidate_serving_result_last = std::nullopt;
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_potential_candidate_serving_result_ahead_of_prerender =
      std::nullopt;
  // `prefetch_container_metrics_ahead_of_prerender` is null as it is not
  // `PrefetchMatchResolver::RegisterCandidate()`ed.
  log_prerender->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender = nullptr;
  log_prerender->prerender_initial_preload_serving_metrics = nullptr;
  log_prerender->prefetch_match_metrics_list[0]->prerender_debug_metrics =
      std::make_unique<PrefetchMatchPrerenderDebugMetrics>();
  log_prerender->prefetch_match_metrics_list[0]
      ->prerender_debug_metrics->prefetch_ahead_of_prerender_debug_metrics =
      std::make_unique<PrefetchMatchPrefetchAheadOfPrerenderDebugMetrics>();
  log_prerender->prefetch_match_metrics_list[0]
      ->prerender_debug_metrics->prefetch_ahead_of_prerender_debug_metrics
      ->prefetch_status = PrefetchStatus::kPrefetchNotStarted;
  log_prerender->prefetch_match_metrics_list[0]
      ->prerender_debug_metrics->prefetch_ahead_of_prerender_debug_metrics
      ->servable_state = PrefetchServableState::kNotServable;
  log_prerender->prefetch_match_metrics_list[0]
      ->prerender_debug_metrics->prefetch_ahead_of_prerender_debug_metrics
      ->match_resolver_action = PrefetchMatchResolverAction(
      PrefetchMatchResolverAction::ActionKind::kDrop,
      PrefetchContainer::LoadState::kEligible,
      /*is_expired=*/std::nullopt);
  log_prerender->prefetch_match_metrics_list[0]
      ->prerender_debug_metrics->prefetch_ahead_of_prerender_debug_metrics
      ->queue_size = 5;
  log_prerender->prefetch_match_metrics_list[0]
      ->prerender_debug_metrics->prefetch_ahead_of_prerender_debug_metrics
      ->queue_index = 3;
  log_prerender->is_prerender_aborted_by_prerender_url_loader_throttle = true;
  auto log = MakeSkeletonPreloadServingMetrics({.n_prefetch_match_metrics = 1});
  log->prefetch_match_metrics_list[0]->time_match_start = Millis(10157);
  log->prefetch_match_metrics_list[0]->time_match_end = Millis(10157);
  log->prefetch_match_metrics_list[0]->n_initial_candidates = 0;
  log->prefetch_match_metrics_list[0]->n_initial_candidates_block_until_head =
      0;
  log->prefetch_match_metrics_list[0]->prefetch_container_metrics = nullptr;
  log->prefetch_match_metrics_list[0]
      ->prefetch_potential_candidate_serving_result_last = std::nullopt;
  log->prefetch_match_metrics_list[0]
      ->prefetch_potential_candidate_serving_result_ahead_of_prerender =
      std::nullopt;
  log->prefetch_match_metrics_list[0]
      ->prefetch_container_metrics_ahead_of_prerender = nullptr;
  log->prefetch_match_metrics_list[0]->prerender_debug_metrics = nullptr;
  log->is_prerender_aborted_by_prerender_url_loader_throttle = false;
  log->prefetch_match_metrics_list[0]->prerender_debug_metrics = nullptr;
  log->is_prerender_aborted_by_prerender_url_loader_throttle = false;
  log->prerender_initial_preload_serving_metrics = nullptr;

  log_prerender->RecordMetricsForPrerenderInitialNavigationFailed();
  log->RecordMetricsForNonPrerenderNavigationCommitted();
  log->RecordFirstContentfulPaint(base::Milliseconds(10334));

  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "FallbackAborted.Match0.PrefetchMatchMetrics.Count",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "FallbackAborted.Match0.PrefetchMatchMetrics.IsPotentialMatch",
      false, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "FallbackAborted.Match0.PrefetchMatchMetrics.ExistsPaop",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "FallbackAborted.Match0.PrefetchMatchMetrics.ExistsPaopThen."
      "PrefetchStatus",
      PrefetchStatus::kPrefetchNotStarted, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "FallbackAborted.Match0.PrefetchMatchMetrics.ExistsPaopThen."
      "ServableStateAndMatcherAction",
      // 4 = PrefetchServableState::kNotServable
      // 1 = PrefetchMatchResolverAction::ActionKind::kDrop
      // 2 = PrefetchContainer::LoadState::kEligible
      // 1 = is_expired == false
      4121, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "FallbackAborted.Match0.PrefetchMatchMetrics.ExistsPaopThen."
      "PotentialCandidateServingResultAndServableStateAndMatcherAction",
      // 14 = PrefetchPotentialCandidateServingResult::kNotServedNoCandidates
      // 4 = PrefetchServableState::kNotServable
      // 1 = PrefetchMatchResolverAction::ActionKind::kDrop
      // 2 = PrefetchContainer::LoadState::kEligible
      // 1 = is_expired == false
      144121, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "FallbackAborted.Match0.PrefetchMatchMetrics.ExistsPaopThen.QueueSize",
      5, 1);
  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "FallbackAborted.Match0.PrefetchMatchMetrics.ExistsPaopThen."
      "QueueIndexPlus1",
      4, 1);

  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationFailed."
      "FallbackAborted.Match1.PrefetchMatchMetrics.Count",
      0);
}

}  // namespace content
