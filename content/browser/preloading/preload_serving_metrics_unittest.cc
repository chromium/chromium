// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preload_serving_metrics.h"

#include <optional>

#include "base/strings/strcat.h"
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
  return base::TimeTicks() + base::Milliseconds(ms);
}

void ExpectFCP(const base::HistogramTester& histogram_tester,
               const std::string& suffix,
               std::optional<int> fcp) {
  const std::string name =
      base::StrCat({"PreloadServingMetrics.PageLoad.Clients.PaintTiming."
                    "NavigationToFirstContentfulPaint.",
                    suffix});
  if (fcp) {
    histogram_tester.ExpectUniqueTimeSample(name, base::Milliseconds(*fcp), 1);
  } else {
    histogram_tester.ExpectTotalCount(name, 0);
  }
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
              },
          },
      },
      {});
  base::HistogramTester histogram_tester;

  auto log = MakeSkeletonPreloadServingMetrics({.n_prefetch_match_metrics = 0});
  log->is_prerender_aborted_by_prerender_url_loader_throttle = false;
  log->prerender_initial_preload_serving_metrics = nullptr;

  log->RecordMetricsForNonPrerenderNavigationCommitted();
  log->RecordPreloadServingMetricsByNavigationInitiator(
      /*did_nav_use_bfcache=*/false, "Other", /*is_url_srp=*/false);
  log->RecordFirstContentfulPaint(base::Milliseconds(334),
                                  /*is_in_foreground=*/true, "Other",
                                  /*is_url_srp=*/false);

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

  ExpectFCP(histogram_tester, "WithoutPreload", {334});
  ExpectFCP(histogram_tester, "WithPrefetch", {});
  ExpectFCP(histogram_tester, "WithPrerender", {});

  ExpectFCP(histogram_tester, "All.All.All", {334});
  ExpectFCP(histogram_tester, "All.All.NoInstantLoad", {334});
  ExpectFCP(histogram_tester, "All.All.Prefetch", {});
  ExpectFCP(histogram_tester, "All.All.Prerender", {});

  ExpectFCP(histogram_tester, "Other.All.All", {334});
  ExpectFCP(histogram_tester, "Other.All.NoInstantLoad", {334});
  ExpectFCP(histogram_tester, "Other.All.Prefetch", {});
  ExpectFCP(histogram_tester, "Other.All.Prerender", {});

  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.NoInstantLoad", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prefetch", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prerender", {});

  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.NoInstantLoad",
            {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prefetch", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prerender", {});

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.Other.All",
                                      0 /* kNoInstantLoad */, 1);
  histogram_tester.ExpectTotalCount("PreloadServingMetrics.Other.SRP", 0);
}

// Tests that FCP metrics with foreground filtering are not recorded for
// background navigations.
//
// Scenario:
//
// - Navigation A committed.
// - FCP occurred in background.
TEST(PreloadServingMetricsTest, NavigationWithoutPreloadInBackground) {
  base::HistogramTester histogram_tester;

  auto log = MakeSkeletonPreloadServingMetrics({.n_prefetch_match_metrics = 0});
  log->is_prerender_aborted_by_prerender_url_loader_throttle = false;
  log->prerender_initial_preload_serving_metrics = nullptr;

  log->RecordMetricsForNonPrerenderNavigationCommitted();
  log->RecordPreloadServingMetricsByNavigationInitiator(
      /*did_nav_use_bfcache=*/false, "Other", /*is_url_srp=*/false);
  log->RecordFirstContentfulPaint(base::Milliseconds(334),
                                  /*is_in_foreground=*/false, "Other",
                                  /*is_url_srp=*/false);

  ExpectFCP(histogram_tester, "WithoutPreload", {334});
  ExpectFCP(histogram_tester, "WithPrefetch", {});
  ExpectFCP(histogram_tester, "WithPrerender", {});

  ExpectFCP(histogram_tester, "All.All.All", {});
  ExpectFCP(histogram_tester, "All.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "All.All.Prefetch", {});
  ExpectFCP(histogram_tester, "All.All.Prerender", {});

  ExpectFCP(histogram_tester, "Other.All.All", {});
  ExpectFCP(histogram_tester, "Other.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "Other.All.Prefetch", {});
  ExpectFCP(histogram_tester, "Other.All.Prerender", {});

  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.NoInstantLoad", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prefetch", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prerender", {});

  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.NoInstantLoad",
            {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prefetch", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prerender", {});
}

// Scenario:
//
// - Navigation A started.
// - A committed.
// - A entered BackForwardCache.
// - Navigation B is started and used A, which is restored from
//   BackForwardCache.
TEST(PreloadServingMetricsTest, NavigationWithBFCacheRestore) {
  base::HistogramTester histogram_tester;

  auto log = MakeSkeletonPreloadServingMetrics({.n_prefetch_match_metrics = 0});
  log->is_prerender_aborted_by_prerender_url_loader_throttle = false;
  log->prerender_initial_preload_serving_metrics = nullptr;

  log->RecordMetricsForNonPrerenderNavigationCommitted();
  log->RecordPreloadServingMetricsByNavigationInitiator(
      /*did_nav_use_bfcache=*/true, "Other", /*is_url_srp=*/false);

  // Note: `RecordFirstContentfulPaint` is not called for BFCache restore
  // because `PreloadServingMetricsPageLoadMetricsObserver` does not handle
  // `OnFirstPaintAfterBackForwardCacheRestoreInPage` (and BFCache restore only
  // provides FirstPaint, not FirstContentfulPaint).

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.Other.All",
                                      3 /* kBFCache */, 1);
  histogram_tester.ExpectTotalCount("PreloadServingMetrics.Other.SRP", 0);
}

// Scenario:
//
// - Navigation A started to a search result page (SRP).
// - A committed.
TEST(PreloadServingMetricsTest, NavigationWithSRP) {
  base::HistogramTester histogram_tester;

  auto log = MakeSkeletonPreloadServingMetrics({.n_prefetch_match_metrics = 0});
  log->is_prerender_aborted_by_prerender_url_loader_throttle = false;
  log->prerender_initial_preload_serving_metrics = nullptr;

  log->RecordMetricsForNonPrerenderNavigationCommitted();
  log->RecordPreloadServingMetricsByNavigationInitiator(
      /*did_nav_use_bfcache=*/false, "Other", /*is_url_srp=*/true);
  log->RecordFirstContentfulPaint(base::Milliseconds(334),
                                  /*is_in_foreground=*/true, "Other",
                                  /*is_url_srp=*/true);

  ExpectFCP(histogram_tester, "All.All.All", {334});
  ExpectFCP(histogram_tester, "All.All.NoInstantLoad", {334});
  ExpectFCP(histogram_tester, "All.SRP.All", {334});
  ExpectFCP(histogram_tester, "All.SRP.NoInstantLoad", {334});

  ExpectFCP(histogram_tester, "Other.All.All", {334});
  ExpectFCP(histogram_tester, "Other.All.NoInstantLoad", {334});
  ExpectFCP(histogram_tester, "Other.SRP.All", {334});
  ExpectFCP(histogram_tester, "Other.SRP.NoInstantLoad", {334});

  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.NoInstantLoad", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.SRP.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.SRP.NoInstantLoad", {334});

  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.NoInstantLoad",
            {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.SRP.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.SRP.NoInstantLoad",
            {334});

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.Other.All",
                                      0 /* kNoInstantLoad */, 1);
  histogram_tester.ExpectTotalCount("PreloadServingMetrics.Other.SRP", 1);
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
              {},
          },
          {
              features::kPrefetchOffTheMainThread,
              {},
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
  log->RecordPreloadServingMetricsByNavigationInitiator(
      /*did_nav_use_bfcache=*/false, "Other", /*is_url_srp=*/false);
  log->RecordFirstContentfulPaint(base::Milliseconds(334),
                                  /*is_in_foreground=*/true, "Other",
                                  /*is_url_srp=*/false);

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

  ExpectFCP(histogram_tester, "WithoutPreload", {});
  ExpectFCP(histogram_tester, "WithPrefetch", {334});
  ExpectFCP(histogram_tester, "WithPrefetch.WithPrePrefetch", {});
  ExpectFCP(histogram_tester, "WithPrefetch.WithoutPrePrefetch", {334});
  ExpectFCP(histogram_tester, "WithPrerender", {});

  ExpectFCP(histogram_tester, "All.All.All", {334});
  ExpectFCP(histogram_tester, "All.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "All.All.Prefetch", {334});
  ExpectFCP(histogram_tester, "All.All.Prefetch.WithPrePrefetch", {});
  ExpectFCP(histogram_tester, "All.All.Prefetch.WithoutPrePrefetch", {334});
  ExpectFCP(histogram_tester, "All.All.Prerender", {});

  ExpectFCP(histogram_tester, "Other.All.All", {334});
  ExpectFCP(histogram_tester, "Other.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "Other.All.Prefetch", {334});
  ExpectFCP(histogram_tester, "Other.All.Prefetch.WithPrePrefetch", {});
  ExpectFCP(histogram_tester, "Other.All.Prefetch.WithoutPrePrefetch", {334});
  ExpectFCP(histogram_tester, "Other.All.Prerender", {});

  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prefetch", {334});
  ExpectFCP(histogram_tester,
            "WithoutFiltering.All.All.Prefetch.WithPrePrefetch", {});
  ExpectFCP(histogram_tester,
            "WithoutFiltering.All.All.Prefetch.WithoutPrePrefetch", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prerender", {});

  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prefetch", {334});
  ExpectFCP(histogram_tester,
            "WithoutFiltering.Other.All.Prefetch.WithPrePrefetch", {});
  ExpectFCP(histogram_tester,
            "WithoutFiltering.Other.All.Prefetch.WithoutPrePrefetch", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prerender", {});

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.Other.All",
                                      1 /* kPrefetch */, 1);
  histogram_tester.ExpectTotalCount("PreloadServingMetrics.Other.SRP", 0);
}

TEST(PreloadServingMetricsTest, NavigationWithPrefetchWithPrePrefetch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {
          {
              features::kPrerender2FallbackPrefetchSpecRules,
              {},
          },
          {
              features::kPrefetchOffTheMainThread,
              {},
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
      ->prefetch_container_metrics->is_constructed_from_pre_prefetch = true;
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
  log->RecordPreloadServingMetricsByNavigationInitiator(
      /*did_nav_use_bfcache=*/false, "Other", /*is_url_srp=*/false);
  log->RecordFirstContentfulPaint(base::Milliseconds(334),
                                  /*is_in_foreground=*/true, "Other",
                                  /*is_url_srp=*/false);

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

  ExpectFCP(histogram_tester, "WithoutPreload", {});
  ExpectFCP(histogram_tester, "WithPrefetch", {334});
  ExpectFCP(histogram_tester, "WithPrefetch.WithPrePrefetch", {334});
  ExpectFCP(histogram_tester, "WithPrefetch.WithoutPrePrefetch", {});
  ExpectFCP(histogram_tester, "WithPrerender", {});

  ExpectFCP(histogram_tester, "All.All.All", {334});
  ExpectFCP(histogram_tester, "All.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "All.All.Prefetch", {334});
  ExpectFCP(histogram_tester, "All.All.Prefetch.WithPrePrefetch", {334});
  ExpectFCP(histogram_tester, "All.All.Prefetch.WithoutPrePrefetch", {});
  ExpectFCP(histogram_tester, "All.All.Prerender", {});

  ExpectFCP(histogram_tester, "Other.All.All", {334});
  ExpectFCP(histogram_tester, "Other.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "Other.All.Prefetch", {334});
  ExpectFCP(histogram_tester, "Other.All.Prefetch.WithPrePrefetch", {334});
  ExpectFCP(histogram_tester, "Other.All.Prefetch.WithoutPrePrefetch", {});
  ExpectFCP(histogram_tester, "Other.All.Prerender", {});

  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prefetch", {334});
  ExpectFCP(histogram_tester,
            "WithoutFiltering.All.All.Prefetch.WithPrePrefetch", {334});
  ExpectFCP(histogram_tester,
            "WithoutFiltering.All.All.Prefetch.WithoutPrePrefetch", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prerender", {});

  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prefetch", {334});
  ExpectFCP(histogram_tester,
            "WithoutFiltering.Other.All.Prefetch.WithPrePrefetch", {334});
  ExpectFCP(histogram_tester,
            "WithoutFiltering.Other.All.Prefetch.WithoutPrePrefetch", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prerender", {});

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.Other.All",
                                      1 /* kPrefetch */, 1);
  histogram_tester.ExpectTotalCount("PreloadServingMetrics.Other.SRP", 0);
}

// Tests that PreloadServingMetrics.*.{All,SRP} are recorded depending on the
// navigation initiator string.
TEST(PreloadServingMetricsTest, RecordByNavigationInitiator) {
  base::HistogramTester histogram_tester;

  auto log = MakeSkeletonPreloadServingMetrics({.n_prefetch_match_metrics = 0});
  log->is_prerender_aborted_by_prerender_url_loader_throttle = false;
  log->prerender_initial_preload_serving_metrics = nullptr;

  log->RecordPreloadServingMetricsByNavigationInitiator(
      /*did_nav_use_bfcache=*/false, "TestInitiator", /*is_url_srp=*/true);
  log->RecordFirstContentfulPaint(base::Milliseconds(334),
                                  /*is_in_foreground=*/true, "TestInitiator",
                                  /*is_url_srp=*/true);

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.TestInitiator.All",
                                      0 /* kNoInstantLoad */, 1);
  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.TestInitiator.SRP",
                                      0 /* kNoInstantLoad */, 1);

  ExpectFCP(histogram_tester, "All.All.All", {334});
  ExpectFCP(histogram_tester, "All.All.NoInstantLoad", {334});
  ExpectFCP(histogram_tester, "All.SRP.All", {334});
  ExpectFCP(histogram_tester, "All.SRP.NoInstantLoad", {334});

  ExpectFCP(histogram_tester, "TestInitiator.All.All", {334});
  ExpectFCP(histogram_tester, "TestInitiator.All.NoInstantLoad", {334});
  ExpectFCP(histogram_tester, "TestInitiator.SRP.All", {334});
  ExpectFCP(histogram_tester, "TestInitiator.SRP.NoInstantLoad", {334});

  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.NoInstantLoad", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.SRP.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.SRP.NoInstantLoad", {334});

  ExpectFCP(histogram_tester, "WithoutFiltering.TestInitiator.All.All", {334});
  ExpectFCP(histogram_tester,
            "WithoutFiltering.TestInitiator.All.NoInstantLoad", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.TestInitiator.SRP.All", {334});
  ExpectFCP(histogram_tester,
            "WithoutFiltering.TestInitiator.SRP.NoInstantLoad", {334});
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
              {},
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
  log->RecordPreloadServingMetricsByNavigationInitiator(
      /*did_nav_use_bfcache=*/false, "Other", /*is_url_srp=*/false);
  log->RecordFirstContentfulPaint(base::Milliseconds(334),
                                  /*is_in_foreground=*/true, "Other",
                                  /*is_url_srp=*/false);

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

  ExpectFCP(histogram_tester, "WithoutPreload", {});
  ExpectFCP(histogram_tester, "WithPrefetch", {});
  ExpectFCP(histogram_tester, "WithPrerender", {334});

  ExpectFCP(histogram_tester, "All.All.All", {334});
  ExpectFCP(histogram_tester, "All.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "All.All.Prefetch", {});
  ExpectFCP(histogram_tester, "All.All.Prerender", {334});

  ExpectFCP(histogram_tester, "Other.All.All", {334});
  ExpectFCP(histogram_tester, "Other.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "Other.All.Prefetch", {});
  ExpectFCP(histogram_tester, "Other.All.Prerender", {334});

  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prefetch", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prerender", {334});

  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prefetch", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prerender", {334});

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.Other.All",
                                      2 /* kPrerender */, 1);
  histogram_tester.ExpectTotalCount("PreloadServingMetrics.Other.SRP", 0);
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
  log->RecordPreloadServingMetricsByNavigationInitiator(
      /*did_nav_use_bfcache=*/false, "Other", /*is_url_srp=*/false);
  log->RecordFirstContentfulPaint(base::Milliseconds(2157),
                                  /*is_in_foreground=*/true, "Other",
                                  /*is_url_srp=*/false);

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

  ExpectFCP(histogram_tester, "WithoutPreload", {2157});
  ExpectFCP(histogram_tester, "WithPrefetch", {});
  ExpectFCP(histogram_tester, "WithPrerender", {});

  ExpectFCP(histogram_tester, "All.All.All", {2157});
  ExpectFCP(histogram_tester, "All.All.NoInstantLoad", {2157});
  ExpectFCP(histogram_tester, "All.All.Prefetch", {});
  ExpectFCP(histogram_tester, "All.All.Prerender", {});

  ExpectFCP(histogram_tester, "Other.All.All", {2157});
  ExpectFCP(histogram_tester, "Other.All.NoInstantLoad", {2157});
  ExpectFCP(histogram_tester, "Other.All.Prefetch", {});
  ExpectFCP(histogram_tester, "Other.All.Prerender", {});

  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.All", {2157});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.NoInstantLoad", {2157});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prefetch", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prerender", {});

  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.All", {2157});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.NoInstantLoad",
            {2157});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prefetch", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prerender", {});

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.Other.All",
                                      0 /* kNoInstantLoad */, 1);
  histogram_tester.ExpectTotalCount("PreloadServingMetrics.Other.SRP", 0);
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
  log->RecordPreloadServingMetricsByNavigationInitiator(
      /*did_nav_use_bfcache=*/false, "Other", /*is_url_srp=*/false);
  log->RecordFirstContentfulPaint(base::Milliseconds(10334),
                                  /*is_in_foreground=*/true, "Other",
                                  /*is_url_srp=*/false);

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

  ExpectFCP(histogram_tester, "WithoutPreload", {10334});
  ExpectFCP(histogram_tester, "WithPrefetch", {});
  ExpectFCP(histogram_tester, "WithPrerender", {});

  ExpectFCP(histogram_tester, "All.All.All", {10334});
  ExpectFCP(histogram_tester, "All.All.NoInstantLoad", {10334});
  ExpectFCP(histogram_tester, "All.All.Prefetch", {});
  ExpectFCP(histogram_tester, "All.All.Prerender", {});

  ExpectFCP(histogram_tester, "Other.All.All", {10334});
  ExpectFCP(histogram_tester, "Other.All.NoInstantLoad", {10334});
  ExpectFCP(histogram_tester, "Other.All.Prefetch", {});
  ExpectFCP(histogram_tester, "Other.All.Prerender", {});

  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.All", {10334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.NoInstantLoad",
            {10334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prefetch", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prerender", {});

  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.All", {10334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.NoInstantLoad",
            {10334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prefetch", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prerender", {});

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.Other.All",
                                      0 /* kNoInstantLoad */, 1);
  histogram_tester.ExpectTotalCount("PreloadServingMetrics.Other.SRP", 0);
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
  log_prerender->prefetch_match_metrics_list[0]
      ->prerender_debug_metrics->prefetch_ahead_of_prerender_debug_metrics
      ->collect_result = PrefetchPotentialCandidateCollectResult::kAvailable;
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
  log->RecordPreloadServingMetricsByNavigationInitiator(
      /*did_nav_use_bfcache=*/false, "Other", /*is_url_srp=*/false);
  log->RecordFirstContentfulPaint(base::Milliseconds(10334),
                                  /*is_in_foreground=*/true, "Other",
                                  /*is_url_srp=*/false);

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
      "PotentialCandidateCollectResultAndServableStateAndMatcherAction",
      // 1 = PrefetchPotentialCandidateCollectResult::kAvailable
      // 4 = PrefetchServableState::kNotServable
      // 1 = PrefetchMatchResolverAction::ActionKind::kDrop
      // 2 = PrefetchContainer::LoadState::kEligible
      // 1 = is_expired == false
      14121, 1);
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

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.Other.All",
                                      0 /* kNoInstantLoad */, 1);
  histogram_tester.ExpectTotalCount("PreloadServingMetrics.Other.SRP", 0);
}

}  // namespace content
