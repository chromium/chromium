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

class PreloadServingMetricsTest : public ::testing::Test {};

struct MakeSkeltonPreloadServingMetricsArgs {
  int n_prefetch_match_metrics;
};

std::unique_ptr<PreloadServingMetrics> MakeSkeltonPreloadServingMetrics(
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

TEST_F(PreloadServingMetricsTest, NavigationWithPrefetch) {
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

  auto log = MakeSkeltonPreloadServingMetrics({.n_prefetch_match_metrics = 1});
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
  log->prerender_initial_preload_serving_metrics = nullptr;

  log->RecordMetricsForNonPrerenderNavigationCommitted();
  log->RecordFirstContentfulPaint(base::Milliseconds(334));

  histogram_tester.ExpectUniqueSample(
      "PreloadServingMetrics.ForNavigationCommitted.PrefetchMatchMetrics.Count",
      1, 1);

  histogram_tester.ExpectTotalCount(
      "PreloadServingMetrics.ForPrerenderInitialNavigationUsed."
      "PrefetchMatchMetrics.Count",
      0);
}

}  // namespace content
