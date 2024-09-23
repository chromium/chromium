// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/back_forward_cache_test_util.h"

#include "base/ranges/algorithm.h"
#include "content/common/content_navigation_policy.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

namespace {

using ::testing::_;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Not;
using ::testing::UnorderedElementsAreArray;

void AddSampleToBuckets(std::vector<base::Bucket>* buckets,
                        base::HistogramBase::Sample sample) {
  auto it = base::ranges::find(*buckets, sample, &base::Bucket::min);
  if (it == buckets->end()) {
    buckets->push_back(base::Bucket(sample, 1));
  } else {
    it->count++;
  }
}

}  // namespace

BackForwardCacheMetricsTestMatcher::BackForwardCacheMetricsTestMatcher() =
    default;
BackForwardCacheMetricsTestMatcher::~BackForwardCacheMetricsTestMatcher() =
    default;

void BackForwardCacheMetricsTestMatcher::DisableCheckingMetricsForAllSites() {
  check_all_sites_ = false;
}

void BackForwardCacheMetricsTestMatcher::ExpectOutcomeDidNotChange(
    base::Location location) {
  if (IsBackForwardCacheEnabled()) {
    // The metric is only logged when there both the BackForwardCache feature
    // flag is enabled and the embedder supports BFCache. Note that this does
    // not actually check the latter part since there's no test that needs it
    // yet.
    EXPECT_EQ(expected_outcomes_,
              histogram_tester().GetAllSamples(
                  "BackForwardCache.HistoryNavigationOutcome"))
        << location.ToString();
  }

  if (!check_all_sites_)
    return;

  EXPECT_EQ(expected_outcomes_,
            histogram_tester().GetAllSamples(
                "BackForwardCache.AllSites.HistoryNavigationOutcome"))
      << location.ToString();

  std::string is_served_from_bfcache =
      "BackForwardCache.IsServedFromBackForwardCache";
  EXPECT_THAT(
      ukm_recorder().GetMetrics("HistoryNavigation", {is_served_from_bfcache}),
      expected_ukm_outcomes_)
      << location.ToString();
}

void BackForwardCacheMetricsTestMatcher::ExpectRestored(
    base::Location location) {
  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored,
                {}, location);
  ExpectReasons({}, {}, {}, {}, {}, location);
}

void BackForwardCacheMetricsTestMatcher::ExpectNotRestored(
    std::vector<BackForwardCacheMetrics::NotRestoredReason> not_restored,
    std::vector<blink::scheduler::WebSchedulerTrackedFeature> block_listed,
    const std::vector<ShouldSwapBrowsingInstance>& not_swapped,
    const std::vector<BackForwardCache::DisabledReason>&
        disabled_for_render_frame_host,
    const std::vector<uint64_t>& disallow_activation,
    base::Location location) {
  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kNotRestored,
                not_restored, location);
  ExpectReasons(not_restored, block_listed, not_swapped,
                disabled_for_render_frame_host, disallow_activation, location);
}

void BackForwardCacheMetricsTestMatcher::ExpectNotRestoredDidNotChange(
    base::Location location) {
  EXPECT_EQ(expected_not_restored_,
            histogram_tester().GetAllSamples(
                "BackForwardCache.HistoryNavigationOutcome."
                "NotRestoredReason"))
      << location.ToString();

  std::string not_restored_reasons = "BackForwardCache.NotRestoredReasons";

  if (!check_all_sites_)
    return;

  EXPECT_EQ(expected_not_restored_,
            histogram_tester().GetAllSamples(
                "BackForwardCache.AllSites.HistoryNavigationOutcome."
                "NotRestoredReason"))
      << location.ToString();

  EXPECT_THAT(
      ukm_recorder().GetMetrics("HistoryNavigation", {not_restored_reasons}),
      expected_ukm_not_restored_reasons_)
      << location.ToString();
}

void BackForwardCacheMetricsTestMatcher::ExpectBlocklistedFeature(
    blink::scheduler::WebSchedulerTrackedFeature feature,
    base::Location location) {
  ExpectBlocklistedFeatures({feature}, location);
}

void BackForwardCacheMetricsTestMatcher::ExpectBrowsingInstanceNotSwappedReason(
    ShouldSwapBrowsingInstance reason,
    base::Location location) {
  ExpectBrowsingInstanceNotSwappedReasons({reason}, location);
}

void BackForwardCacheMetricsTestMatcher::ExpectEvictedAfterCommitted(
    std::vector<BackForwardCacheMetrics::EvictedAfterDocumentRestoredReason>
        reasons,
    base::Location location) {
  for (BackForwardCacheMetrics::EvictedAfterDocumentRestoredReason reason :
       reasons) {
    base::HistogramBase::Sample sample = base::HistogramBase::Sample(reason);
    AddSampleToBuckets(&expected_eviction_after_committing_, sample);
  }

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "BackForwardCache.EvictedAfterDocumentRestoredReason"),
              UnorderedElementsAreArray(expected_eviction_after_committing_))
      << location.ToString();
  if (!check_all_sites_)
    return;

  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "BackForwardCache.AllSites.EvictedAfterDocumentRestoredReason"),
      UnorderedElementsAreArray(expected_eviction_after_committing_))
      << location.ToString();
}

void BackForwardCacheMetricsTestMatcher::ExpectOutcome(
    BackForwardCacheMetrics::HistoryNavigationOutcome outcome,
    std::vector<BackForwardCacheMetrics::NotRestoredReason> not_restored,
    base::Location location) {
  base::HistogramBase::Sample sample = base::HistogramBase::Sample(outcome);
  AddSampleToBuckets(&expected_outcomes_, sample);

  auto delegate_disabled_idx =
      std::find(not_restored.begin(), not_restored.end(),
                BackForwardCacheMetrics::NotRestoredReason::
                    kBackForwardCacheDisabledForDelegate);
  if (IsBackForwardCacheEnabled() &&
      delegate_disabled_idx == not_restored.end()) {
    // The metric is only logged when there both the BackForwardCache feature
    // flag is enabled and the embedder supports BFCache.
    EXPECT_THAT(histogram_tester().GetAllSamples(
                    "BackForwardCache.HistoryNavigationOutcome"),
                UnorderedElementsAreArray(expected_outcomes_))
        << location.ToString();
  }

  if (!check_all_sites_)
    return;

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "BackForwardCache.AllSites.HistoryNavigationOutcome"),
              UnorderedElementsAreArray(expected_outcomes_))
      << location.ToString();

  std::string is_served_from_bfcache =
      "BackForwardCache.IsServedFromBackForwardCache";
  bool ukm_outcome =
      outcome == BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored;
  expected_ukm_outcomes_.push_back(
      {{is_served_from_bfcache, static_cast<int64_t>(ukm_outcome)}});
  EXPECT_THAT(
      ukm_recorder().GetMetrics("HistoryNavigation", {is_served_from_bfcache}),
      expected_ukm_outcomes_)
      << location.ToString();
}

void BackForwardCacheMetricsTestMatcher::ExpectReasons(
    std::vector<BackForwardCacheMetrics::NotRestoredReason> not_restored,
    std::vector<blink::scheduler::WebSchedulerTrackedFeature> block_listed,
    const std::vector<ShouldSwapBrowsingInstance>& not_swapped,
    const std::vector<BackForwardCache::DisabledReason>&
        disabled_for_render_frame_host,
    const std::vector<uint64_t>& disallow_activation,
    base::Location location) {
  // Check that the expected reasons are consistent.
  bool expect_blocklisted =
      base::ranges::count(
          not_restored,
          BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures) > 0;
  bool has_blocklisted = block_listed.size() > 0;
  EXPECT_EQ(expect_blocklisted, has_blocklisted);
  bool expect_disabled_for_render_frame_host =
      base::ranges::count(not_restored,
                          BackForwardCacheMetrics::NotRestoredReason::
                              kDisableForRenderFrameHostCalled) > 0;
  bool has_disabled_for_render_frame_host =
      disabled_for_render_frame_host.size() > 0;
  EXPECT_EQ(expect_disabled_for_render_frame_host,
            has_disabled_for_render_frame_host);

  // Check that the reasons are as expected.
  ExpectNotRestoredReasons(not_restored, location);
  ExpectBlocklistedFeatures(block_listed, location);
  ExpectBrowsingInstanceNotSwappedReasons(not_swapped, location);
  ExpectDisabledWithReasons(disabled_for_render_frame_host, location);
  ExpectDisallowActivationReasons(disallow_activation, location);
}

void BackForwardCacheMetricsTestMatcher::ExpectNotRestoredReasons(
    std::vector<BackForwardCacheMetrics::NotRestoredReason> reasons,
    base::Location location) {
  uint64_t not_restored_reasons_bits = 0;
  for (BackForwardCacheMetrics::NotRestoredReason reason : reasons) {
    base::HistogramBase::Sample sample = base::HistogramBase::Sample(reason);
    AddSampleToBuckets(&expected_not_restored_, sample);
    not_restored_reasons_bits |= 1ull << static_cast<int>(reason);
  }

  auto delegate_disabled_idx =
      std::find(reasons.begin(), reasons.end(),
                BackForwardCacheMetrics::NotRestoredReason::
                    kBackForwardCacheDisabledForDelegate);
  if (IsBackForwardCacheEnabled() && delegate_disabled_idx == reasons.end()) {
    // The metric is only logged when there both the BackForwardCache feature
    // flag is enabled and the embedder supports BFCache.
    EXPECT_THAT(histogram_tester().GetAllSamples(
                    "BackForwardCache.HistoryNavigationOutcome."
                    "NotRestoredReason"),
                UnorderedElementsAreArray(expected_not_restored_))
        << location.ToString();
  }

  if (!check_all_sites_)
    return;

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "BackForwardCache.AllSites.HistoryNavigationOutcome."
                  "NotRestoredReason"),
              UnorderedElementsAreArray(expected_not_restored_))
      << location.ToString();

  std::string not_restored_reasons = "BackForwardCache.NotRestoredReasons";
  expected_ukm_not_restored_reasons_.push_back(
      {{not_restored_reasons, not_restored_reasons_bits}});
  EXPECT_THAT(
      ukm_recorder().GetMetrics("HistoryNavigation", {not_restored_reasons}),
      expected_ukm_not_restored_reasons_)
      << location.ToString();
}

void BackForwardCacheMetricsTestMatcher::ExpectBlocklistedFeatures(
    std::vector<blink::scheduler::WebSchedulerTrackedFeature> features,
    base::Location location) {
  for (auto feature : features) {
    base::HistogramBase::Sample sample = base::HistogramBase::Sample(feature);
    AddSampleToBuckets(&expected_blocklisted_features_, sample);
  }

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "BackForwardCache.HistoryNavigationOutcome."
                  "BlocklistedFeature"),
              UnorderedElementsAreArray(expected_blocklisted_features_))
      << location.ToString();

  if (!check_all_sites_)
    return;

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "BackForwardCache.AllSites.HistoryNavigationOutcome."
                  "BlocklistedFeature"),
              UnorderedElementsAreArray(expected_blocklisted_features_))
      << location.ToString();
}

void BackForwardCacheMetricsTestMatcher::ExpectDisabledWithReasons(
    const std::vector<BackForwardCache::DisabledReason>& reasons,
    base::Location location) {
  for (BackForwardCache::DisabledReason reason : reasons) {
    base::HistogramBase::Sample sample = base::HistogramBase::Sample(
        content::BackForwardCacheMetrics::MetricValue(reason));
    AddSampleToBuckets(&expected_disabled_reasons_, sample);
  }
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "BackForwardCache.HistoryNavigationOutcome."
                  "DisabledForRenderFrameHostReason2"),
              UnorderedElementsAreArray(expected_disabled_reasons_))
      << location.ToString();
}

void BackForwardCacheMetricsTestMatcher::ExpectDisallowActivationReasons(
    const std::vector<uint64_t>& reasons,
    base::Location location) {
  for (const uint64_t& reason : reasons) {
    base::HistogramBase::Sample sample(reason);
    AddSampleToBuckets(&expected_disallow_activation_reasons_, sample);
  }
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "BackForwardCache.HistoryNavigationOutcome."
                  "DisallowActivationReason"),
              UnorderedElementsAreArray(expected_disallow_activation_reasons_))
      << location.ToString();
}

void BackForwardCacheMetricsTestMatcher::
    ExpectBrowsingInstanceNotSwappedReasons(
        const std::vector<ShouldSwapBrowsingInstance>& reasons,
        base::Location location) {
  for (auto reason : reasons) {
    base::HistogramBase::Sample sample = base::HistogramBase::Sample(reason);
    AddSampleToBuckets(&expected_browsing_instance_not_swapped_reasons_,
                       sample);
  }
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "BackForwardCache.HistoryNavigationOutcome."
                  "BrowsingInstanceNotSwappedReason"),
              UnorderedElementsAreArray(
                  expected_browsing_instance_not_swapped_reasons_))
      << location.ToString();
  if (!check_all_sites_)
    return;

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "BackForwardCache.AllSites.HistoryNavigationOutcome."
                  "BrowsingInstanceNotSwappedReason"),
              UnorderedElementsAreArray(
                  expected_browsing_instance_not_swapped_reasons_))
      << location.ToString();
}

}  // namespace content
