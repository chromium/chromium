// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACK_FORWARD_CACHE_TEST_UTIL_H_
#define CONTENT_BROWSER_BACK_FORWARD_CACHE_TEST_UTIL_H_

#include <string_view>
#include <vector>

#include "base/location.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/renderer_host/back_forward_cache_metrics.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

namespace content {

namespace {
// TODO(yuzus): Make a way to use a non sticky dummy feature.
constexpr auto* kBlockingPagePath =
    "/back_forward_cache/page_with_blocking_feature.html";
constexpr auto* kBlockingReasonString = "webxrdevice";
constexpr auto kBlockingReasonEnum =
    blink::scheduler::WebSchedulerTrackedFeature::kWebXR;
constexpr auto* kBlockingScript = "navigator.xr.isSessionSupported('inline');";
}  // namespace

// `BackForwardCacheMetricsTestMatcher` provides common matchers and
// expectations to help make test assertions on BackForwardCache-related
// metrics.
//
// This class tries to remove the efforts to reset HistogramTester by keeping
// local copies of expected metrics in expected_* members such that subsequent
// calls to Expect*() won't need to take previous metrics into account.
//
// For example:
// ...
//   DoActionCauseFeatureA();
//   // The implementation compares {FeatureA} with metrics from
//   // `histogram_tester()`, which is {FeatureA}.
//   // FeatureA will be stored in in `expected_blocklisted_features_`.
//   ExpectBlocklistedFeature(FeatureA);
//
//   // `histogram_tester()` keeps FeatureA and continues to accumulate metrics.
//
//   DoActionCauseFeatureB();
//   // The implementation compares {FeatureA, FeatureB} with accumulated
//   // metrics from `histogram_tester()`, which is also {FeatureA, FeatureB}.
//   ExpectBlocklistedFeature(FeatureB);
//
class BackForwardCacheMetricsTestMatcher {
 protected:
  using UkmMetrics = ukm::TestUkmRecorder::HumanReadableUkmMetrics;

  BackForwardCacheMetricsTestMatcher();
  ~BackForwardCacheMetricsTestMatcher();

  // Disables checking metrics that are recorded regardless of the domains. By
  // default, this class' Expect* function checks the metrics both for the
  // specific domain and for all domains at the same time. In the case when the
  // test results need to be different, call this function.
  void DisableCheckingMetricsForAllSites();

  // Tests that the observed outcomes match the current expected outcomes
  // without adding any new expected outcomes.
  void ExpectOutcomeDidNotChange(base::Location location);

  void ExpectRestored(base::Location location);

  void ExpectNotRestored(
      std::vector<BackForwardCacheMetrics::NotRestoredReason> not_restored,
      std::vector<blink::scheduler::WebSchedulerTrackedFeature> block_listed,
      const std::vector<ShouldSwapBrowsingInstance>& not_swapped,
      const std::vector<BackForwardCache::DisabledReason>&
          disabled_for_render_frame_host,
      const std::vector<uint64_t>& disallow_activation,
      base::Location location);

  void ExpectNotRestoredDidNotChange(base::Location location);

  void ExpectBlocklistedFeature(
      blink::scheduler::WebSchedulerTrackedFeature feature,
      base::Location location);

  void ExpectBrowsingInstanceNotSwappedReason(ShouldSwapBrowsingInstance reason,
                                              base::Location location);

  void ExpectEvictedAfterCommitted(
      std::vector<BackForwardCacheMetrics::EvictedAfterDocumentRestoredReason>
          reasons,
      base::Location location);

  template <typename T>
  void ExpectBucketCount(std::string_view name,
                         T sample,
                         base::HistogramBase::Count expected_count) {
    histogram_tester().ExpectBucketCount(name, sample, expected_count);
  }

  // Implementation needs to provide access to their own ukm_recorder.
  // Note that TestAutoSetUkmRecorder's ctor requires a sequenced context.
  virtual const ukm::TestAutoSetUkmRecorder& ukm_recorder() = 0;

  // Implementation needs to provide access to their own histogram_tester.
  // Note that HistogramTester accumulates all data after it is constructed.
  virtual const base::HistogramTester& histogram_tester() = 0;

 private:
  // Adds a new outcome to the set of expected outcomes (restored or not) and
  // tests that it occurred.
  void ExpectOutcome(
      BackForwardCacheMetrics::HistoryNavigationOutcome outcome,
      std::vector<BackForwardCacheMetrics::NotRestoredReason> not_restored,
      base::Location location);

  void ExpectReasons(
      std::vector<BackForwardCacheMetrics::NotRestoredReason> not_restored,
      std::vector<blink::scheduler::WebSchedulerTrackedFeature> block_listed,
      const std::vector<ShouldSwapBrowsingInstance>& not_swapped,
      const std::vector<BackForwardCache::DisabledReason>&
          disabled_for_render_frame_host,
      const std::vector<uint64_t>& disallow_activation,
      base::Location location);

  void ExpectNotRestoredReasons(
      std::vector<BackForwardCacheMetrics::NotRestoredReason> reasons,
      base::Location location);

  void ExpectBlocklistedFeatures(
      std::vector<blink::scheduler::WebSchedulerTrackedFeature> features,
      base::Location location);

  void ExpectDisabledWithReasons(
      const std::vector<BackForwardCache::DisabledReason>& reasons,
      base::Location location);

  void ExpectDisallowActivationReasons(const std::vector<uint64_t>& reasons,
                                       base::Location location);

  void ExpectBrowsingInstanceNotSwappedReasons(
      const std::vector<ShouldSwapBrowsingInstance>& reasons,
      base::Location location);

  std::vector<base::Bucket> expected_outcomes_;
  std::vector<base::Bucket> expected_not_restored_;
  std::vector<base::Bucket> expected_blocklisted_features_;
  std::vector<base::Bucket> expected_disabled_reasons_;
  std::vector<base::Bucket> expected_disallow_activation_reasons_;
  std::vector<base::Bucket> expected_browsing_instance_not_swapped_reasons_;
  std::vector<base::Bucket> expected_eviction_after_committing_;

  std::vector<UkmMetrics> expected_ukm_outcomes_;
  std::vector<UkmMetrics> expected_ukm_not_restored_reasons_;

  // Indicates whether metrics for all sites regardless of the domains are
  // checked or not.
  bool check_all_sites_ = true;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACK_FORWARD_CACHE_TEST_UTIL_H_
