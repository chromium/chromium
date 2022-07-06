// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACK_FORWARD_CACHE_BROWSERTEST_H_
#define CONTENT_BROWSER_BACK_FORWARD_CACHE_BROWSERTEST_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_logging_settings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/renderer_host/page_lifecycle_state_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_host_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

using NotRestoredReasons =
    BackForwardCacheCanStoreDocumentResult::NotRestoredReasons;
using NotRestoredReason = BackForwardCacheMetrics::NotRestoredReason;

// Match RenderFrameHostImpl* that are in the BackForwardCache.
MATCHER(InBackForwardCache, "") {
  return arg->IsInBackForwardCache();
}

// Match RenderFrameDeletedObserver* which observed deletion of the RenderFrame.
MATCHER(Deleted, "") {
  return arg->deleted();
}

// Helper function to pass an initializer list to the EXPECT_THAT macro. This is
// indeed the identity function.
std::initializer_list<RenderFrameHostImpl*> Elements(
    std::initializer_list<RenderFrameHostImpl*> t);

namespace {

// hash for std::unordered_map.
struct FeatureHash {
  size_t operator()(base::Feature feature) const {
    return base::FastHash(feature.name);
  }
};

// compare operator for std::unordered_map.
struct FeatureEqualOperator {
  bool operator()(base::Feature feature1, base::Feature feature2) const {
    return std::strcmp(feature1.name, feature2.name) == 0;
  }
};

}  // namespace

// Test about the BackForwardCache.
class BackForwardCacheBrowserTest
    : public ContentBrowserTest,
      public WebContentsObserver,
      public BackForwardCacheMetrics::TestObserver {
 public:
  BackForwardCacheBrowserTest();
  ~BackForwardCacheBrowserTest() override;

  // TestObserver:
  void NotifyNotRestoredReasons(
      std::unique_ptr<BackForwardCacheCanStoreTreeResult> tree_result) override;

 protected:
  using UkmMetrics = ukm::TestUkmRecorder::HumanReadableUkmMetrics;

  // Disables checking metrics that are recorded recardless of the domains. By
  // default, this class' Expect* function checks the metrics both for the
  // specific domain and for all domains at the same time. In the case when the
  // test results need to be different, call this function.
  void DisableCheckingMetricsForAllSites();

  void SetUpCommandLine(base::CommandLine* command_line) override;

  void SetUpInProcessBrowserTestFixture() override;

  void TearDownInProcessBrowserTestFixture() override;

  void SetupFeaturesAndParameters();

  void EnableFeatureAndSetParams(base::Feature feature,
                                 std::string param_name,
                                 std::string param_value);

  void DisableFeature(base::Feature feature);

  void SetUpOnMainThread() override;

  void TearDownOnMainThread() override;

  WebContentsImpl* web_contents() const;

  RenderFrameHostImpl* current_frame_host();

  RenderFrameHostManager* render_frame_host_manager();

  std::string DepictFrameTree(FrameTreeNode* node);

  bool HistogramContainsIntValue(base::HistogramBase::Sample sample,
                                 std::vector<base::Bucket> histogram_values);

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

  void EvictByJavaScript(RenderFrameHostImpl* rfh);

  void StartRecordingEvents(RenderFrameHostImpl* rfh);

  void MatchEventList(RenderFrameHostImpl* rfh,
                      base::Value list,
                      base::Location location = base::Location::Current());

  // Creates a minimal HTTPS server, accessible through https_server().
  // Returns a pointer to the server.
  net::EmbeddedTestServer* CreateHttpsServer();

  net::EmbeddedTestServer* https_server();

  void ExpectTotalCount(base::StringPiece name,
                        base::HistogramBase::Count count);

  template <typename T>
  void ExpectBucketCount(base::StringPiece name,
                         T sample,
                         base::HistogramBase::Count expected_count) {
    histogram_tester_.ExpectBucketCount(name, sample, expected_count);
  }

  // Do not fail this test if a message from a renderer arrives at the browser
  // for a cached page.
  void DoNotFailForUnexpectedMessagesWhileCached();

  // Navigates to a page at |page_url| with an img element with src set to
  // "image.png".
  RenderFrameHostImpl* NavigateToPageWithImage(const GURL& page_url);

  void AcquireKeyboardLock(RenderFrameHostImpl* rfh);

  void ReleaseKeyboardLock(RenderFrameHostImpl* rfh);

  // Start a navigation to |url| but block it on an error. If |history_offset|
  // is not 0, then the navigation will be a history navigation and this will
  // assert that the URL after navigation is |url|.
  void NavigateAndBlock(GURL url, int history_offset);

  static testing::Matcher<BackForwardCacheCanStoreDocumentResult>
  MatchesDocumentResult(testing::Matcher<NotRestoredReasons> not_stored,
                        BlockListedFeatures block_listed);

  // Access the tree result of NotRestoredReason for the last main frame
  // navigation.
  BackForwardCacheCanStoreTreeResult* GetTreeResult() {
    return tree_result_.get();
  }

  void InstallUnloadHandlerOnMainFrame();
  void InstallUnloadHandlerOnSubFrame();
  EvalJsResult GetUnloadRunCount();

  bool IsUnloadAllowedToEnterBackForwardCache();

  // Adds a blocklisted feature to the document to prevent caching. Currently
  // this means adding a plugin. We expect that plugins will never become
  // cacheable, so this should be stable (at least until plugins cease to
  // exist). If you need the feature to be sticky, then
  // `RenderFrameHostImpl::UseDummyStickyBackForwardCacheDisablingFeatureForTesting`
  // provides that.
  [[nodiscard]] bool AddBlocklistedFeature(RenderFrameHost* rfh);
  // Check that the document was not restored for the reason added by
  // `AddBlocklistedFeature`.
  void ExpectNotRestoredDueToBlocklistedFeature(base::Location location);

  base::HistogramTester histogram_tester_;

  bool same_site_back_forward_cache_enabled_ = true;
  bool skip_same_site_if_unload_exists_ = false;

  const int kMaxBufferedBytesPerProcess = 10000;
  const base::TimeDelta kGracePeriodToFinishLoading = base::Seconds(5);

 private:
  void AddSampleToBuckets(std::vector<base::Bucket>* buckets,
                          base::HistogramBase::Sample sample);

  // Adds a new outcome to the set of expected outcomes (restored or not) and
  // tests that it occurred.
  void ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome outcome,
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

  content::ContentMockCertVerifier mock_cert_verifier_;

  base::test::ScopedFeatureList feature_list_;
  logging::ScopedVmoduleSwitches vmodule_switches_;

  FrameTreeVisualizer visualizer_;
  std::vector<base::Bucket> expected_outcomes_;
  std::vector<base::Bucket> expected_not_restored_;
  std::vector<base::Bucket> expected_blocklisted_features_;
  std::vector<base::Bucket> expected_disabled_reasons_;
  std::vector<base::Bucket> expected_disallow_activation_reasons_;
  std::vector<base::Bucket> expected_browsing_instance_not_swapped_reasons_;
  std::vector<base::Bucket> expected_eviction_after_committing_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unordered_map<base::Feature,
                     std::map<std::string, std::string>,
                     FeatureHash,
                     FeatureEqualOperator>
      features_with_params_;
  std::vector<base::Feature> disabled_features_;

  std::vector<UkmMetrics> expected_ukm_outcomes_;
  std::vector<UkmMetrics> expected_ukm_not_restored_reasons_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;

  // Store the tree result of NotRestoredReasons for the last main frame
  // navigation.
  std::unique_ptr<BackForwardCacheCanStoreTreeResult> tree_result_;

  // Indicates whether metrics for all sites regardless of the domains are
  // checked or not.
  bool check_all_sites_ = true;
  // Whether we should fail the test if a message arrived at the browser from a
  // renderer for a bfcached page.
  bool fail_for_unexpected_messages_while_cached_ = true;
};

[[nodiscard]] bool WaitForDOMContentLoaded(RenderFrameHostImpl* rfh);

class HighCacheSizeBackForwardCacheBrowserTest
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;

  // The number of pages the BackForwardCache can hold per tab.
  // The number 5 was picked since Android ASAN trybot failed to keep more than
  // 6 pages in memory.
  const size_t kBackForwardCacheSize = 5;
};

// An implementation of PageLifecycleStateManager::TestDelegate for testing.
class PageLifecycleStateManagerTestDelegate
    : public PageLifecycleStateManager::TestDelegate {
 public:
  explicit PageLifecycleStateManagerTestDelegate(
      PageLifecycleStateManager* manager);

  ~PageLifecycleStateManagerTestDelegate() override;

  // Waits for the renderer finishing to set the state of being in back/forward
  // cache.
  void WaitForInBackForwardCacheAck();

  void OnStoreInBackForwardCacheSent(base::OnceClosure cb);
  void OnDisableJsEvictionSent(base::OnceClosure cb);
  void OnRestoreFromBackForwardCacheSent(base::OnceClosure cb);

 private:
  // PageLifecycleStateManager::TestDelegate:
  void OnLastAcknowledgedStateChanged(
      const blink::mojom::PageLifecycleState& old_state,
      const blink::mojom::PageLifecycleState& new_state) override;
  void OnUpdateSentToRenderer(
      const blink::mojom::PageLifecycleState& new_state) override;
  void OnDeleted() override;

  raw_ptr<PageLifecycleStateManager, DanglingUntriaged> manager_;
  base::OnceClosure store_in_back_forward_cache_sent_;
  base::OnceClosure store_in_back_forward_cache_ack_received_;
  base::OnceClosure restore_from_back_forward_cache_sent_;
  base::OnceClosure disable_eviction_sent_;
};

// Gets the value of a key in local storage by evaluating JS.
EvalJsResult GetLocalStorage(RenderFrameHostImpl* rfh, std::string key);

}  // namespace content

#endif  // CONTENT_BROWSER_BACK_FORWARD_CACHE_BROWSERTEST_H_
