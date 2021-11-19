// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/back_forward_cache_browsertest.h"

#include <unordered_map>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/common/task_annotator.h"
#include "base/task/post_task.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/back_forward_cache_can_store_document_result.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/should_swap_browsing_instance.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/render_accessibility.mojom.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/commit_message_delayer.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/text_input_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"
#include "content/test/web_contents_observer_test_utils.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/frame/event_page_show_persisted.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#include "third_party/blink/public/common/switches.h"

// This file has too many tests.
//
// Before adding new tests to this file, consider if they will fit better into
// one of the other back_forward_cache_*_browsertest.cc files or if there are
// enough new tests to justify a new file.

using testing::_;
using testing::Each;
using testing::ElementsAre;
using testing::Not;
using testing::UnorderedElementsAreArray;

namespace content {

namespace {

class DOMContentLoadedObserver : public WebContentsObserver {
 public:
  explicit DOMContentLoadedObserver(RenderFrameHostImpl* render_frame_host)
      : WebContentsObserver(
            WebContents::FromRenderFrameHost(render_frame_host)),
        render_frame_host_(render_frame_host) {}

  void DOMContentLoaded(RenderFrameHost* render_frame_host) override {
    if (render_frame_host_ == render_frame_host)
      run_loop_.Quit();
  }

  void Wait() {
    if (render_frame_host_->IsDOMContentLoaded())
      run_loop_.Quit();
    run_loop_.Run();
  }

 private:
  RenderFrameHostImpl* render_frame_host_;
  base::RunLoop run_loop_;
};

}  // namespace

void WaitForDOMContentLoaded(RenderFrameHostImpl* rfh) {
  DOMContentLoadedObserver observer(rfh);
  observer.Wait();
}

BackForwardCacheBrowserTest::BackForwardCacheBrowserTest() = default;

BackForwardCacheBrowserTest::~BackForwardCacheBrowserTest() {
  if (fail_for_unexpected_messages_while_cached_) {
    // If this is triggered, see MojoInterfaceName in
    // tools/metrics/histograms/enums.xml for which values correspond which
    // messages.
    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    "BackForwardCache.UnexpectedRendererToBrowserMessage."
                    "InterfaceName"),
                testing::ElementsAre());
  }
}

  // Disables checking metrics that are recorded recardless of the domains. By
  // default, this class' Expect* function checks the metrics both for the
  // specific domain and for all domains at the same time. In the case when the
  // test results need to be different, call this function.
void BackForwardCacheBrowserTest::DisableCheckingMetricsForAllSites() {
  check_all_sites_ = false;
}

void BackForwardCacheBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  ContentBrowserTest::SetUpCommandLine(command_line);
  mock_cert_verifier_.SetUpCommandLine(command_line);

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kUseFakeUIForMediaStream);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalWebPlatformFeatures);
  // TODO(sreejakshetty): Initialize ScopedFeatureLists from test constructor.
  EnableFeatureAndSetParams(features::kBackForwardCache,
                            "TimeToLiveInBackForwardCacheInSeconds", "3600");
  EnableFeatureAndSetParams(features::kBackForwardCache,
                            "message_handling_when_cached", "log");
  EnableFeatureAndSetParams(
      features::kBackForwardCache, "enable_same_site",
      same_site_back_forward_cache_enabled_ ? "true" : "false");
  EnableFeatureAndSetParams(
      features::kBackForwardCache, "skip_same_site_if_unload_exists",
      skip_same_site_if_unload_exists_ ? "true" : "false");
  EnableFeatureAndSetParams(features::kBackForwardCache, "unload_support",
                            unload_support_);
  EnableFeatureAndSetParams(
      blink::features::kLogUnexpectedIPCPostedToBackForwardCachedDocuments,
      "delay_before_tracking_ms", "0");
  // TODO(crbug.com/1243600): Remove this per-request byte limit.
  EnableFeatureAndSetParams(blink::features::kLoadingTasksUnfreezable,
                            "max_buffered_bytes",
                            base::NumberToString(kMaxBufferedBytesPerRequest));
  EnableFeatureAndSetParams(blink::features::kLoadingTasksUnfreezable,
                            "max_buffered_bytes_per_process",
                            base::NumberToString(kMaxBufferedBytesPerProcess));
  EnableFeatureAndSetParams(
      blink::features::kLoadingTasksUnfreezable,
      "grace_period_to_finish_loading_in_seconds",
      base::NumberToString(kGracePeriodToFinishLoading.InSeconds()));
#if defined(OS_ANDROID)
    EnableFeatureAndSetParams(features::kBackForwardCache,
                              "process_binding_strength", "NORMAL");
#endif
    // Allow BackForwardCache for all devices regardless of their memory.
    DisableFeature(features::kBackForwardCacheMemoryControls);

    SetupFeaturesAndParameters();

    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
    // Unfortunately needed for one test on slow bots, TextInputStateUpdated,
    // where deferred commits delays input too much.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
}

void BackForwardCacheBrowserTest::SetUpInProcessBrowserTestFixture() {
  ContentBrowserTest::SetUpInProcessBrowserTestFixture();
  mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
}

void BackForwardCacheBrowserTest::TearDownInProcessBrowserTestFixture() {
  ContentBrowserTest::TearDownInProcessBrowserTestFixture();
  mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
}

void BackForwardCacheBrowserTest::SetupFeaturesAndParameters() {
  std::vector<base::test::ScopedFeatureList::FeatureAndParams> enabled_features;

  for (auto& features_with_param : features_with_params_) {
    enabled_features.emplace_back(features_with_param.first,
                                  features_with_param.second);
  }

  feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                              disabled_features_);
}

void BackForwardCacheBrowserTest::EnableFeatureAndSetParams(
    base::Feature feature,
    std::string param_name,
    std::string param_value) {
  features_with_params_[feature][param_name] = param_value;
}

void BackForwardCacheBrowserTest::DisableFeature(base::Feature feature) {
  disabled_features_.push_back(feature);
}

void BackForwardCacheBrowserTest::SetUpOnMainThread() {
  mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
  host_resolver()->AddRule("*", "127.0.0.1");
  // TestAutoSetUkmRecorder's constructor requires a sequenced context.
  ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  ContentBrowserTest::SetUpOnMainThread();
}

void BackForwardCacheBrowserTest::TearDownOnMainThread() {
  ukm_recorder_.reset();
  ContentBrowserTest::TearDownOnMainThread();
}

WebContentsImpl* BackForwardCacheBrowserTest::web_contents() const {
  return static_cast<WebContentsImpl*>(shell()->web_contents());
}

RenderFrameHostImpl* BackForwardCacheBrowserTest::current_frame_host() {
  return web_contents()->GetPrimaryFrameTree().root()->current_frame_host();
}

RenderFrameHostManager*
BackForwardCacheBrowserTest::render_frame_host_manager() {
  return web_contents()->GetPrimaryFrameTree().root()->render_manager();
}

std::string BackForwardCacheBrowserTest::DepictFrameTree(FrameTreeNode* node) {
  return visualizer_.DepictFrameTree(node);
}

bool BackForwardCacheBrowserTest::HistogramContainsIntValue(
    base::HistogramBase::Sample sample,
    std::vector<base::Bucket> histogram_values) {
  auto it = std::find_if(histogram_values.begin(), histogram_values.end(),
                         [sample](const base::Bucket& bucket) {
                           return bucket.min == static_cast<int>(sample);
                         });
  return it != histogram_values.end();
}

void BackForwardCacheBrowserTest::ExpectOutcomeDidNotChange(
    base::Location location) {
  EXPECT_EQ(expected_outcomes_,
            histogram_tester_.GetAllSamples(
                "BackForwardCache.HistoryNavigationOutcome"))
      << location.ToString();

  if (!check_all_sites_)
    return;

  EXPECT_EQ(expected_outcomes_,
            histogram_tester_.GetAllSamples(
                "BackForwardCache.AllSites.HistoryNavigationOutcome"))
      << location.ToString();

  std::string is_served_from_bfcache =
      "BackForwardCache.IsServedFromBackForwardCache";
  EXPECT_THAT(
      ukm_recorder_->GetMetrics("HistoryNavigation", {is_served_from_bfcache}),
      expected_ukm_outcomes_)
      << location.ToString();
}

void BackForwardCacheBrowserTest::ExpectRestored(base::Location location) {
  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored,
                location);
  ExpectReasons({}, {}, {}, {}, {}, location);
}

void BackForwardCacheBrowserTest::ExpectNotRestored(
    std::vector<BackForwardCacheMetrics::NotRestoredReason> not_restored,
    std::vector<blink::scheduler::WebSchedulerTrackedFeature> block_listed,
    const std::vector<ShouldSwapBrowsingInstance>& not_swapped,
    const std::vector<BackForwardCache::DisabledReason>&
        disabled_for_render_frame_host,
    const std::vector<uint64_t>& disallow_activation,
    base::Location location) {
  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kNotRestored,
                location);
  ExpectReasons(not_restored, block_listed, not_swapped,
                disabled_for_render_frame_host, disallow_activation, location);
}

void BackForwardCacheBrowserTest::ExpectNotRestoredDidNotChange(
    base::Location location) {
  EXPECT_EQ(expected_not_restored_,
            histogram_tester_.GetAllSamples(
                "BackForwardCache.HistoryNavigationOutcome."
                "NotRestoredReason"))
      << location.ToString();

  std::string not_restored_reasons = "BackForwardCache.NotRestoredReasons";

  if (!check_all_sites_)
    return;

  EXPECT_EQ(expected_not_restored_,
            histogram_tester_.GetAllSamples(
                "BackForwardCache.AllSites.HistoryNavigationOutcome."
                "NotRestoredReason"))
      << location.ToString();

  EXPECT_THAT(
      ukm_recorder_->GetMetrics("HistoryNavigation", {not_restored_reasons}),
      expected_ukm_not_restored_reasons_)
      << location.ToString();
}

void BackForwardCacheBrowserTest::ExpectBlocklistedFeature(
    blink::scheduler::WebSchedulerTrackedFeature feature,
    base::Location location) {
  ExpectBlocklistedFeatures({feature}, location);
}

void BackForwardCacheBrowserTest::ExpectBrowsingInstanceNotSwappedReason(
    ShouldSwapBrowsingInstance reason,
    base::Location location) {
  ExpectBrowsingInstanceNotSwappedReasons({reason}, location);
}

void BackForwardCacheBrowserTest::ExpectEvictedAfterCommitted(
    std::vector<BackForwardCacheMetrics::EvictedAfterDocumentRestoredReason>
        reasons,
    base::Location location) {
  for (BackForwardCacheMetrics::EvictedAfterDocumentRestoredReason reason :
       reasons) {
    base::HistogramBase::Sample sample = base::HistogramBase::Sample(reason);
    AddSampleToBuckets(&expected_eviction_after_committing_, sample);
  }

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "BackForwardCache.EvictedAfterDocumentRestoredReason"),
              UnorderedElementsAreArray(expected_eviction_after_committing_))
      << location.ToString();
  if (!check_all_sites_)
    return;

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "BackForwardCache.AllSites.EvictedAfterDocumentRestoredReason"),
      UnorderedElementsAreArray(expected_eviction_after_committing_))
      << location.ToString();
}

void BackForwardCacheBrowserTest::EvictByJavaScript(RenderFrameHostImpl* rfh) {
  // Run JavaScript on a page in the back-forward cache. The page should be
  // evicted. As the frame is deleted, ExecJs returns false without executing.
  // Run without user gesture to prevent UpdateUserActivationState message
  // being sent back to browser.
  EXPECT_FALSE(
      ExecJs(rfh, "console.log('hi');", EXECUTE_SCRIPT_NO_USER_GESTURE));
}

void BackForwardCacheBrowserTest::StartRecordingEvents(
    RenderFrameHostImpl* rfh) {
  EXPECT_TRUE(ExecJs(rfh, R"(
      window.testObservedEvents = [];
      let event_list = [
        'visibilitychange',
        'pagehide',
        'pageshow',
        'freeze',
        'resume',
        'unload',
      ];
      for (event_name of event_list) {
        let result = event_name;
        window.addEventListener(event_name, event => {
          if (event.persisted)
            result += '.persisted';
          window.testObservedEvents.push('window.' + result);
        });
        document.addEventListener(event_name,
            () => window.testObservedEvents.push('document.' + result));
      }
    )"));
}

void BackForwardCacheBrowserTest::MatchEventList(RenderFrameHostImpl* rfh,
                                                 base::ListValue list,
                                                 base::Location location) {
  EXPECT_EQ(list, EvalJs(rfh, "window.testObservedEvents"))
      << location.ToString();
}

  // Creates a minimal HTTPS server, accessible through https_server().
  // Returns a pointer to the server.
net::EmbeddedTestServer* BackForwardCacheBrowserTest::CreateHttpsServer() {
  https_server_ = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_->AddDefaultHandlers(GetTestDataFilePath());
  https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  return https_server();
}

net::EmbeddedTestServer* BackForwardCacheBrowserTest::https_server() {
  return https_server_.get();
}

void BackForwardCacheBrowserTest::ExpectTotalCount(
    base::StringPiece name,
    base::HistogramBase::Count count) {
  histogram_tester_.ExpectTotalCount(name, count);
}

  // Do not fail this test if a message from a renderer arrives at the browser
  // for a cached page.
void BackForwardCacheBrowserTest::DoNotFailForUnexpectedMessagesWhileCached() {
  fail_for_unexpected_messages_while_cached_ = false;
}

  // Navigates to a page at |page_url| with an img element with src set to
  // "image.png".
RenderFrameHostImpl* BackForwardCacheBrowserTest::NavigateToPageWithImage(
    const GURL& page_url) {
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  RenderFrameHostImpl* rfh = current_frame_host();
  // Wait for the document to load DOM to ensure that kLoading is not
  // one of the reasons why the document wasn't cached.
  WaitForDOMContentLoaded(rfh);

  EXPECT_TRUE(ExecJs(rfh, R"(
      var image = document.createElement("img");
      image.src = "image.png";
      document.body.appendChild(image);

      var image_load_status = new Promise((resolve, reject) => {
        image.onload = () => { resolve("loaded"); }
        image.onerror = () => { resolve("error"); }
      });
    )"));
  return rfh;
}

void BackForwardCacheBrowserTest::AcquireKeyboardLock(
    RenderFrameHostImpl* rfh) {
  EXPECT_TRUE(ExecJs(rfh, R"(
        new Promise(resolve => {
          navigator.keyboard.lock();
          resolve();
        });
      )"));
}

void BackForwardCacheBrowserTest::ReleaseKeyboardLock(
    RenderFrameHostImpl* rfh) {
  EXPECT_TRUE(ExecJs(rfh, R"(
        new Promise(resolve => {
          navigator.keyboard.unlock();
          resolve();
        });
      )"));
}

void BackForwardCacheBrowserTest::AddSampleToBuckets(
    std::vector<base::Bucket>* buckets,
    base::HistogramBase::Sample sample) {
  auto it = std::find_if(
      buckets->begin(), buckets->end(),
      [sample](const base::Bucket& bucket) { return bucket.min == sample; });
  if (it == buckets->end()) {
    buckets->push_back(base::Bucket(sample, 1));
  } else {
    it->count++;
  }
}

void BackForwardCacheBrowserTest::ExpectOutcome(
    BackForwardCacheMetrics::HistoryNavigationOutcome outcome,
    base::Location location) {
  base::HistogramBase::Sample sample = base::HistogramBase::Sample(outcome);
  AddSampleToBuckets(&expected_outcomes_, sample);

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "BackForwardCache.HistoryNavigationOutcome"),
              UnorderedElementsAreArray(expected_outcomes_))
      << location.ToString();
  if (!check_all_sites_)
    return;

  EXPECT_THAT(histogram_tester_.GetAllSamples(
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
      ukm_recorder_->GetMetrics("HistoryNavigation", {is_served_from_bfcache}),
      expected_ukm_outcomes_)
      << location.ToString();
}

void BackForwardCacheBrowserTest::ExpectReasons(
    std::vector<BackForwardCacheMetrics::NotRestoredReason> not_restored,
    std::vector<blink::scheduler::WebSchedulerTrackedFeature> block_listed,
    const std::vector<ShouldSwapBrowsingInstance>& not_swapped,
    const std::vector<BackForwardCache::DisabledReason>&
        disabled_for_render_frame_host,
    const std::vector<uint64_t>& disallow_activation,
    base::Location location) {
  // Check that the expected reasons are consistent.
  bool expect_blocklisted =
      std::count(
          not_restored.begin(), not_restored.end(),
          BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures) > 0;
  bool has_blocklisted = block_listed.size() > 0;
  EXPECT_EQ(expect_blocklisted, has_blocklisted);
  bool expect_disabled_for_render_frame_host =
      std::count(not_restored.begin(), not_restored.end(),
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

void BackForwardCacheBrowserTest::ExpectNotRestoredReasons(
    std::vector<BackForwardCacheMetrics::NotRestoredReason> reasons,
    base::Location location) {
  uint64_t not_restored_reasons_bits = 0;
  for (BackForwardCacheMetrics::NotRestoredReason reason : reasons) {
    base::HistogramBase::Sample sample = base::HistogramBase::Sample(reason);
    AddSampleToBuckets(&expected_not_restored_, sample);
    not_restored_reasons_bits |= 1ull << static_cast<int>(reason);
  }

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "BackForwardCache.HistoryNavigationOutcome."
                  "NotRestoredReason"),
              UnorderedElementsAreArray(expected_not_restored_))
      << location.ToString();

  if (!check_all_sites_)
    return;

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "BackForwardCache.AllSites.HistoryNavigationOutcome."
                  "NotRestoredReason"),
              UnorderedElementsAreArray(expected_not_restored_))
      << location.ToString();

  std::string not_restored_reasons = "BackForwardCache.NotRestoredReasons";
  expected_ukm_not_restored_reasons_.push_back(
      {{not_restored_reasons, not_restored_reasons_bits}});
  EXPECT_THAT(
      ukm_recorder_->GetMetrics("HistoryNavigation", {not_restored_reasons}),
      expected_ukm_not_restored_reasons_)
      << location.ToString();
}

void BackForwardCacheBrowserTest::ExpectBlocklistedFeatures(
    std::vector<blink::scheduler::WebSchedulerTrackedFeature> features,
    base::Location location) {
  for (auto feature : features) {
    base::HistogramBase::Sample sample = base::HistogramBase::Sample(feature);
    AddSampleToBuckets(&expected_blocklisted_features_, sample);
  }

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "BackForwardCache.HistoryNavigationOutcome."
                  "BlocklistedFeature"),
              UnorderedElementsAreArray(expected_blocklisted_features_))
      << location.ToString();

  if (!check_all_sites_)
    return;

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "BackForwardCache.AllSites.HistoryNavigationOutcome."
                  "BlocklistedFeature"),
              UnorderedElementsAreArray(expected_blocklisted_features_))
      << location.ToString();
}

void BackForwardCacheBrowserTest::ExpectDisabledWithReasons(
    const std::vector<BackForwardCache::DisabledReason>& reasons,
    base::Location location) {
  for (BackForwardCache::DisabledReason reason : reasons) {
    base::HistogramBase::Sample sample = base::HistogramBase::Sample(
        content::BackForwardCacheMetrics::MetricValue(reason));
    AddSampleToBuckets(&expected_disabled_reasons_, sample);
  }
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "BackForwardCache.HistoryNavigationOutcome."
                  "DisabledForRenderFrameHostReason2"),
              UnorderedElementsAreArray(expected_disabled_reasons_))
      << location.ToString();
}

void BackForwardCacheBrowserTest::ExpectDisallowActivationReasons(
    const std::vector<uint64_t>& reasons,
    base::Location location) {
  for (const uint64_t& reason : reasons) {
    base::HistogramBase::Sample sample(reason);
    AddSampleToBuckets(&expected_disallow_activation_reasons_, sample);
  }
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "BackForwardCache.HistoryNavigationOutcome."
                  "DisallowActivationReason"),
              UnorderedElementsAreArray(expected_disallow_activation_reasons_))
      << location.ToString();
}

void BackForwardCacheBrowserTest::ExpectBrowsingInstanceNotSwappedReasons(
    const std::vector<ShouldSwapBrowsingInstance>& reasons,
    base::Location location) {
  for (auto reason : reasons) {
    base::HistogramBase::Sample sample = base::HistogramBase::Sample(reason);
    AddSampleToBuckets(&expected_browsing_instance_not_swapped_reasons_,
                       sample);
  }
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "BackForwardCache.HistoryNavigationOutcome."
                  "BrowsingInstanceNotSwappedReason"),
              UnorderedElementsAreArray(
                  expected_browsing_instance_not_swapped_reasons_))
      << location.ToString();
  if (!check_all_sites_)
    return;

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "BackForwardCache.AllSites.HistoryNavigationOutcome."
                  "BrowsingInstanceNotSwappedReason"),
              UnorderedElementsAreArray(
                  expected_browsing_instance_not_swapped_reasons_))
      << location.ToString();
}

std::initializer_list<RenderFrameHostImpl*> Elements(
    std::initializer_list<RenderFrameHostImpl*> t) {
  return t;
}

// Execute a custom callback when navigation is ready to commit. This is
// useful for simulating race conditions happening when a page enters the
// BackForwardCache and receive inflight messages sent when it wasn't frozen
// yet.
class ReadyToCommitNavigationCallback : public WebContentsObserver {
 public:
  ReadyToCommitNavigationCallback(
      WebContents* content,
      base::OnceCallback<void(NavigationHandle*)> callback)
      : WebContentsObserver(content), callback_(std::move(callback)) {}

  ReadyToCommitNavigationCallback(const ReadyToCommitNavigationCallback&) =
      delete;
  ReadyToCommitNavigationCallback& operator=(
      const ReadyToCommitNavigationCallback&) = delete;

 private:
  // WebContentsObserver:
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
    if (callback_)
      std::move(callback_).Run(navigation_handle);
  }

  base::OnceCallback<void(NavigationHandle*)> callback_;
};

class FirstVisuallyNonEmptyPaintObserver : public WebContentsObserver {
 public:
  explicit FirstVisuallyNonEmptyPaintObserver(WebContents* contents)
      : WebContentsObserver(contents) {}
  void DidFirstVisuallyNonEmptyPaint() override {
    if (observed_)
      return;
    observed_ = true;
    run_loop_.Quit();
  }

  bool did_fire() const { return observed_; }

  void Wait() { run_loop_.Run(); }

 private:
  bool observed_ = false;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

void WaitForFirstVisuallyNonEmptyPaint(WebContents* contents) {
  if (contents->CompletedFirstVisuallyNonEmptyPaint())
    return;
  FirstVisuallyNonEmptyPaintObserver observer(contents);
  observer.Wait();
}

class ThemeColorObserver : public WebContentsObserver {
 public:
  explicit ThemeColorObserver(WebContents* contents)
      : WebContentsObserver(contents) {}
  void DidChangeThemeColor() override { observed_ = true; }

  bool did_fire() const { return observed_; }

 private:
  bool observed_ = false;
};

// Navigate from A to B and go back.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, Basic) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  url::Origin origin_a = url::Origin::Create(url_a);
  url::Origin origin_b = url::Origin::Create(url_b);

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_EQ(rfh_a->GetVisibilityState(), PageVisibilityState::kHidden);
  EXPECT_EQ(origin_a, rfh_a->GetLastCommittedOrigin());
  EXPECT_EQ(origin_b, rfh_b->GetLastCommittedOrigin());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());
  EXPECT_EQ(rfh_b->GetVisibilityState(), PageVisibilityState::kVisible);

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_EQ(origin_a, rfh_a->GetLastCommittedOrigin());
  EXPECT_EQ(origin_b, rfh_b->GetLastCommittedOrigin());
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_EQ(rfh_a->GetVisibilityState(), PageVisibilityState::kVisible);
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_EQ(rfh_b->GetVisibilityState(), PageVisibilityState::kHidden);

  ExpectRestored(FROM_HERE);
}

// Navigate from A to B and go back.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, BasicDocumentInitiated) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURLFromRenderer(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());

  // The two pages are using different BrowsingInstances.
  EXPECT_FALSE(rfh_a->GetSiteInstance()->IsRelatedSiteInstance(
      rfh_b->GetSiteInstance()));

  // 3) Go back to A.
  EXPECT_TRUE(ExecJs(shell(), "history.back();"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());

  ExpectRestored(FROM_HERE);
}

// Navigate from back and forward repeatedly.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       NavigateBackForwardRepeatedly) {
  // Do not check for unexpected messages because the input task queue is not
  // currently frozen, causing flakes in this test: crbug.com/1099395.
  DoNotFailForUnexpectedMessagesWhileCached();
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());

  ExpectRestored(FROM_HERE);

  // 4) Go forward to B.
  ASSERT_TRUE(HistoryGoForward(web_contents()));

  EXPECT_EQ(rfh_b, current_frame_host());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());

  ExpectRestored(FROM_HERE);

  // 5) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());

  ExpectRestored(FROM_HERE);

  // 6) Go forward to B.
  ASSERT_TRUE(HistoryGoForward(web_contents()));

  EXPECT_EQ(rfh_b, current_frame_host());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());

  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());

  ExpectRestored(FROM_HERE);
}

// The current page can't enter the BackForwardCache if another page can script
// it. This can happen when one document opens a popup using window.open() for
// instance. It prevents the BackForwardCache from being used.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, WindowOpen) {
  // This test assumes cross-site navigation staying in the same
  // BrowsingInstance to use a different SiteInstance. Otherwise, it will
  // timeout at step 2).
  if (!SiteIsolationPolicy::UseDedicatedProcessesForAllSites())
    return;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A and open a popup.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  EXPECT_EQ(1u, rfh_a->GetSiteInstance()->GetRelatedActiveContentsCount());
  Shell* popup = OpenPopup(rfh_a.get(), url_a, "");
  EXPECT_EQ(2u, rfh_a->GetSiteInstance()->GetRelatedActiveContentsCount());

  // 2) Navigate to B. The previous document can't enter the BackForwardCache,
  // because of the popup.
  ASSERT_TRUE(NavigateToURLFromRenderer(rfh_a.get(), url_b));
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
  RenderFrameHostImplWrapper rfh_b(current_frame_host());
  EXPECT_EQ(2u, rfh_b->GetSiteInstance()->GetRelatedActiveContentsCount());

  // 3) Go back to A. The previous document can't enter the BackForwardCache,
  // because of the popup.
  ASSERT_TRUE(ExecJs(rfh_b.get(), "history.back();"));
  ASSERT_TRUE(rfh_b.WaitUntilRenderFrameDeleted());

  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kRelatedActiveContentsExist,
       BackForwardCacheMetrics::NotRestoredReason::kBrowsingInstanceNotSwapped},
      {}, {ShouldSwapBrowsingInstance::kNo_HasRelatedActiveContents}, {}, {},
      FROM_HERE);

  // 4) Make the popup drop the window.opener connection. It happens when the
  //    user does an omnibox-initiated navigation, which happens in a new
  //    BrowsingInstance.
  RenderFrameHostImplWrapper rfh_a_new(current_frame_host());
  EXPECT_EQ(2u, rfh_a_new->GetSiteInstance()->GetRelatedActiveContentsCount());
  ASSERT_TRUE(NavigateToURL(popup, url_b));
  EXPECT_EQ(1u, rfh_a_new->GetSiteInstance()->GetRelatedActiveContentsCount());

  // 5) Navigate to B again. As the scripting relationship with the popup is
  // now severed, the current page (|rfh_a_new|) can enter back-forward cache.
  ASSERT_TRUE(NavigateToURLFromRenderer(rfh_a_new.get(), url_b));
  EXPECT_FALSE(rfh_a_new.IsRenderFrameDeleted());
  EXPECT_TRUE(rfh_a_new->IsInBackForwardCache());

  // 6) Go back to A. The current document can finally enter the
  // BackForwardCache, because it is alone in its BrowsingInstance and has never
  // been related to any other document.
  RenderFrameHostImplWrapper rfh_b_new(current_frame_host());
  ASSERT_TRUE(ExecJs(rfh_b_new.get(), "history.back();"));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_FALSE(rfh_b_new.IsRenderFrameDeleted());
  EXPECT_TRUE(rfh_b_new->IsInBackForwardCache());
}

// A popup will prevent a page from entering BFCache. Test that after closing a
// popup, the page is not stopped from entering. This tries to close the popup
// at the last moment.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, WindowOpenThenClose) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/title2.html");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.test", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.test", "/title2.html"));

  // Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  EXPECT_EQ(1u, rfh_a->GetSiteInstance()->GetRelatedActiveContentsCount());

  // Open a popup.
  Shell* popup = OpenPopup(rfh_a.get(), url_a, "");
  EXPECT_EQ(2u, rfh_a->GetSiteInstance()->GetRelatedActiveContentsCount());

  // Start navigating to B, the response will be delayed.
  TestNavigationObserver observer(web_contents());
  shell()->LoadURL(url_b);

  // When the request is received, close the popup.
  response.WaitForRequest();
  RenderFrameHostImplWrapper rfh_popup(popup->web_contents()->GetMainFrame());
  ASSERT_TRUE(ExecJs(rfh_popup.get(), "window.close();"));
  ASSERT_TRUE(rfh_popup.WaitUntilRenderFrameDeleted());

  EXPECT_EQ(1u, rfh_a->GetSiteInstance()->GetRelatedActiveContentsCount());

  // Send the response.
  response.Send(net::HTTP_OK, "text/html", "foo");
  response.Done();
  observer.Wait();

  // A is in BFCache.
  EXPECT_EQ(0u, rfh_a->GetSiteInstance()->GetRelatedActiveContentsCount());
  ASSERT_TRUE(rfh_a->IsInBackForwardCache());

  // Go back.
  web_contents()->GetController().GoBack();
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // A is restored from BFCache.
  EXPECT_FALSE(rfh_a.IsRenderFrameDeleted());
  ExpectRestored(FROM_HERE);
}

// Navigate from A(B) to C and go back.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, BasicIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to A(B).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // 2) Navigate to C.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  RenderFrameHostImpl* rfh_c = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_c(rfh_c);
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_FALSE(rfh_c->IsInBackForwardCache());

  // 3) Go back to A(B).
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_FALSE(delete_observer_rfh_c.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());
  EXPECT_TRUE(rfh_c->IsInBackForwardCache());

  ExpectRestored(FROM_HERE);
}

// Check the visible URL in the omnibox is properly updated when restoring a
// document from the BackForwardCache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, VisibleURL) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Go to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  // 2) Go to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(url_a, web_contents()->GetVisibleURL());

  // 4) Go forward to B.
  ASSERT_TRUE(HistoryGoForward(web_contents()));
  EXPECT_EQ(url_b, web_contents()->GetVisibleURL());
}

// Test only 1 document is kept in the at a time BackForwardCache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       CacheSizeLimitedToOneDocumentPerTab) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  // BackForwardCache is empty.
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  // BackForwardCache contains only rfh_a.
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  // BackForwardCache contains only rfh_b.
  delete_observer_rfh_a.WaitUntilDeleted();
  EXPECT_FALSE(delete_observer_rfh_b.deleted());

  // If/when the cache size is increased, this can be tested iteratively, see
  // deleted code in: https://crrev.com/c/1782902.

  ASSERT_TRUE(HistoryGoToOffset(web_contents(), -2));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::kCacheLimit},
                    {}, {}, {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, ResponseHeaders) {
  CreateHttpsServer();
  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/set-header?X-Foo: bar"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  NavigationHandleObserver observer1(web_contents(), url_a);
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  EXPECT_TRUE(observer1.has_committed());
  EXPECT_EQ("bar", observer1.GetNormalizedResponseHeader("x-foo"));

  // 2) Navigate to B.
  NavigationHandleObserver observer2(web_contents(), url_b);
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());
  EXPECT_TRUE(observer2.has_committed());

  // 3) Go back to A.
  NavigationHandleObserver observer3(web_contents(), url_a);
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_TRUE(observer3.has_committed());
  EXPECT_EQ("bar", observer3.GetNormalizedResponseHeader("x-foo"));

  ExpectRestored(FROM_HERE);
}

void HighCacheSizeBackForwardCacheBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  EnableFeatureAndSetParams(features::kBackForwardCache, "cache_size",
                            base::NumberToString(kBackForwardCacheSize));
  BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
}

// Test documents are evicted from the BackForwardCache at some point.
IN_PROC_BROWSER_TEST_F(HighCacheSizeBackForwardCacheBrowserTest,
                       CacheEvictionWithIncreasedCacheSize) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a));  // BackForwardCache size is 0.
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  EXPECT_TRUE(NavigateToURL(shell(), url_b));  // BackForwardCache size is 1.
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  for (size_t i = 2; i < kBackForwardCacheSize; ++i) {
    EXPECT_TRUE(NavigateToURL(shell(), i % 2 ? url_b : url_a));
    // After |i+1| navigations, |i| documents went into the BackForwardCache.
    // When |i| is greater than the BackForwardCache size limit, they are
    // evicted:
    EXPECT_EQ(i >= kBackForwardCacheSize + 1, delete_observer_rfh_a.deleted());
    EXPECT_EQ(i >= kBackForwardCacheSize + 2, delete_observer_rfh_b.deleted());
  }
}

// Tests that evicting a page in between the time the back/forward cache
// NavigationRequest restore was created and when the NavigationRequest actually
// starts after finishing beforeunload won't result in a crash.
// See https://crbug.com/1218114.
IN_PROC_BROWSER_TEST_F(HighCacheSizeBackForwardCacheBrowserTest,
                       EvictedWhileWaitingForBeforeUnload) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title3.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  RenderFrameDeletedObserver delete_observer(rfh_a.get());

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImplWrapper rfh_b(current_frame_host());
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b.get());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Navigate to C, which has a beforeunload handler that never finishes.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  RenderFrameHostImplWrapper rfh_c(current_frame_host());
  EXPECT_TRUE(ExecJs(rfh_c.get(), R"(
    window.onbeforeunload = () => {
      while (true) {}
    }
  )"));
  // Both A & B are in the back/forward cache.
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());

  // 4) Evict entry A. This will post a task that destroys all evicted entries
  // when it runs (task #1).
  DisableBFCacheForRFHForTesting(rfh_a->GetGlobalId());
  EXPECT_FALSE(rfh_a.IsDestroyed());
  EXPECT_TRUE(rfh_a->is_evicted_from_back_forward_cache());

  // 5) Trigger a back navigation to B. This will create a BFCache restore
  // navigation to B, but will wait for C's beforeunload handler to finish
  // running before continuing.
  web_contents()->GetController().GoBack();

  // 6) Post a task to run BeforeUnloadCompleted (task #2). This will continue
  // the BFCache restore navigation to B from step 5, which is currently waiting
  // for a BeforeUnloadCompleted call.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        root->navigator().BeforeUnloadCompleted(root, true /* proceed */,
                                                base::TimeTicks::Now());
      }));

  // 7) Evict entry B. This will post a task (task #3) to restart the navigation
  // to B, and also another task (task #4) to destroy all evicted entries.
  DisableBFCacheForRFHForTesting(rfh_b->GetGlobalId());
  EXPECT_FALSE(rfh_b.IsDestroyed());
  EXPECT_TRUE(rfh_b->is_evicted_from_back_forward_cache());

  // 8) Wait until the back navigation to B finishes. This will run posted tasks
  // in order. So:
  // - Task #1 from step 4 will run and destroy all evicted entries. As both the
  // entries for A & B have been evicted, they are both destroyed.
  // - Task #2 from step 6 will run and continue the back/forward cache restore
  // NavigationRequest to B. However, it would notice that the entry for B is
  // now gone, and should handle it gracefully.
  // - Task #3 from step 7 to restart navigation to B runs, and should create a
  // NavigationRequest to replace the previous NavigationRequest to B.
  // - Task #4 from step 7 to destroy evicted entries runs and won't destroy
  // any entry since there's no longer any entry in the back/forward cache.
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url_b);
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    {}, {}, {RenderFrameHostDisabledForTestingReason()}, {},
                    FROM_HERE);
}

// Similar to BackForwardCacheBrowserTest.SubframeSurviveCache*
// Test case: a1(b2) -> c3 -> a1(b2)
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, SubframeSurviveCache1) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  std::vector<RenderFrameDeletedObserver*> rfh_observer;

  // 1) Navigate to a1(b2).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* a1 = current_frame_host();
  RenderFrameHostImpl* b2 = a1->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver a1_observer(a1), b2_observer(b2);
  rfh_observer.insert(rfh_observer.end(), {&a1_observer, &b2_observer});
  EXPECT_TRUE(ExecJs(b2, "window.alive = 'I am alive';"));

  // 2) Navigate to c3.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  RenderFrameHostImpl* c3 = current_frame_host();
  RenderFrameDeletedObserver c3_observer(c3);
  rfh_observer.push_back(&c3_observer);
  ASSERT_THAT(rfh_observer, Each(Not(Deleted())));
  EXPECT_THAT(Elements({a1, b2}), Each(InBackForwardCache()));
  EXPECT_THAT(c3, Not(InBackForwardCache()));

  // 3) Go back to a1(b2).
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ASSERT_THAT(rfh_observer, Each(Not(Deleted())));
  EXPECT_THAT(Elements({a1, b2}), Each(Not(InBackForwardCache())));
  EXPECT_THAT(c3, InBackForwardCache());

  // Even after a new IPC round trip with the renderer, b2 must still be alive.
  EXPECT_EQ("I am alive", EvalJs(b2, "window.alive"));
  EXPECT_FALSE(b2_observer.deleted());

  ExpectRestored(FROM_HERE);
}

// Similar to BackForwardCacheBrowserTest.SubframeSurviveCache*
// Test case: a1(b2) -> b3 -> a1(b2).
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, SubframeSurviveCache2) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  std::vector<RenderFrameDeletedObserver*> rfh_observer;

  // 1) Navigate to a1(b2).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* a1 = current_frame_host();
  RenderFrameHostImpl* b2 = a1->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver a1_observer(a1), b2_observer(b2);
  rfh_observer.insert(rfh_observer.end(), {&a1_observer, &b2_observer});
  EXPECT_TRUE(ExecJs(b2, "window.alive = 'I am alive';"));

  // 2) Navigate to b3.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* b3 = current_frame_host();
  RenderFrameDeletedObserver b3_observer(b3);
  rfh_observer.push_back(&b3_observer);
  ASSERT_THAT(rfh_observer, Each(Not(Deleted())));
  EXPECT_THAT(Elements({a1, b2}), Each(InBackForwardCache()));
  EXPECT_THAT(b3, Not(InBackForwardCache()));

  // 3) Go back to a1(b2).
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ASSERT_THAT(rfh_observer, Each(Not(Deleted())));
  EXPECT_EQ(a1, current_frame_host());
  EXPECT_THAT(Elements({a1, b2}), Each(Not(InBackForwardCache())));
  EXPECT_THAT(b3, InBackForwardCache());

  // Even after a new IPC round trip with the renderer, b2 must still be alive.
  EXPECT_EQ("I am alive", EvalJs(b2, "window.alive"));
  EXPECT_FALSE(b2_observer.deleted());

  ExpectRestored(FROM_HERE);
}

// Similar to BackForwardCacheBrowserTest.tSubframeSurviveCache*
// Test case: a1(b2) -> b3(a4) -> a1(b2) -> b3(a4)
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, SubframeSurviveCache3) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(a)"));

  std::vector<RenderFrameDeletedObserver*> rfh_observer;

  // 1) Navigate to a1(b2).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* a1 = current_frame_host();
  RenderFrameHostImpl* b2 = a1->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver a1_observer(a1), b2_observer(b2);
  rfh_observer.insert(rfh_observer.end(), {&a1_observer, &b2_observer});
  EXPECT_TRUE(ExecJs(b2, "window.alive = 'I am alive';"));

  // 2) Navigate to b3(a4)
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* b3 = current_frame_host();
  RenderFrameHostImpl* a4 = b3->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver b3_observer(b3), a4_observer(a4);
  rfh_observer.insert(rfh_observer.end(), {&b3_observer, &a4_observer});
  ASSERT_THAT(rfh_observer, Each(Not(Deleted())));
  EXPECT_THAT(Elements({a1, b2}), Each(InBackForwardCache()));
  EXPECT_THAT(Elements({b3, a4}), Each(Not(InBackForwardCache())));
  EXPECT_TRUE(ExecJs(a4, "window.alive = 'I am alive';"));

  // 3) Go back to a1(b2).
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ASSERT_THAT(rfh_observer, Each(Not(Deleted())));
  EXPECT_EQ(a1, current_frame_host());
  EXPECT_THAT(Elements({a1, b2}), Each(Not(InBackForwardCache())));
  EXPECT_THAT(Elements({b3, a4}), Each(InBackForwardCache()));

  // Even after a new IPC round trip with the renderer, b2 must still be alive.
  EXPECT_EQ("I am alive", EvalJs(b2, "window.alive"));
  EXPECT_FALSE(b2_observer.deleted());

  ExpectRestored(FROM_HERE);

  // 4) Go forward to b3(a4).
  ASSERT_TRUE(HistoryGoForward(web_contents()));
  ASSERT_THAT(rfh_observer, Each(Not(Deleted())));
  EXPECT_EQ(b3, current_frame_host());
  EXPECT_THAT(Elements({a1, b2}), Each(InBackForwardCache()));
  EXPECT_THAT(Elements({b3, a4}), Each(Not(InBackForwardCache())));

  // Even after a new IPC round trip with the renderer, a4 must still be alive.
  EXPECT_EQ("I am alive", EvalJs(a4, "window.alive"));
  EXPECT_FALSE(a4_observer.deleted());

  ExpectRestored(FROM_HERE);
}

// Similar to BackForwardCacheBrowserTest.SubframeSurviveCache*
// Test case: a1(b2) -> b3 -> a4 -> b5 -> a1(b2).
IN_PROC_BROWSER_TEST_F(HighCacheSizeBackForwardCacheBrowserTest,
                       SubframeSurviveCache4) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_ab(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  std::vector<RenderFrameDeletedObserver*> rfh_observer;

  // 1) Navigate to a1(b2).
  EXPECT_TRUE(NavigateToURL(shell(), url_ab));
  RenderFrameHostImpl* a1 = current_frame_host();
  RenderFrameHostImpl* b2 = a1->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver a1_observer(a1), b2_observer(b2);
  rfh_observer.insert(rfh_observer.end(), {&a1_observer, &b2_observer});
  EXPECT_TRUE(ExecJs(b2, "window.alive = 'I am alive';"));

  // 2) Navigate to b3.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* b3 = current_frame_host();
  RenderFrameDeletedObserver b3_observer(b3);
  rfh_observer.push_back(&b3_observer);
  ASSERT_THAT(rfh_observer, Each(Not(Deleted())));
  EXPECT_THAT(Elements({a1, b2}), Each(InBackForwardCache()));
  EXPECT_THAT(b3, Not(InBackForwardCache()));

  // 3) Navigate to a4.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* a4 = current_frame_host();
  RenderFrameDeletedObserver a4_observer(a4);
  rfh_observer.push_back(&a4_observer);
  ASSERT_THAT(rfh_observer, Each(Not(Deleted())));

  // 4) Navigate to b5
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* b5 = current_frame_host();
  RenderFrameDeletedObserver b5_observer(b5);
  rfh_observer.push_back(&b5_observer);
  ASSERT_THAT(rfh_observer, Each(Not(Deleted())));
  EXPECT_THAT(Elements({a1, b2, b3, a4}), Each(InBackForwardCache()));
  EXPECT_THAT(b5, Not(InBackForwardCache()));

  // 3) Go back to a1(b2).
  ASSERT_TRUE(HistoryGoToOffset(web_contents(), -3));
  EXPECT_EQ(a1, current_frame_host());
  ASSERT_THAT(rfh_observer, Each(Not(Deleted())));
  EXPECT_THAT(Elements({b3, a4, b5}), Each(InBackForwardCache()));
  EXPECT_THAT(Elements({a1, b2}), Each(Not(InBackForwardCache())));

  // Even after a new IPC round trip with the renderer, b2 must still be alive.
  EXPECT_EQ("I am alive", EvalJs(b2, "window.alive"));
  EXPECT_FALSE(b2_observer.deleted());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       SubframeWithOngoingNavigationNotCached) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/hung");
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a page with an iframe.
  TestNavigationObserver navigation_observer1(web_contents());
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/back_forward_cache/page_with_hung_iframe.html"));
  shell()->LoadURL(main_url);
  navigation_observer1.WaitForNavigationFinished();

  RenderFrameHostImpl* main_frame = current_frame_host();
  RenderFrameDeletedObserver frame_deleted_observer(main_frame);
  response.WaitForRequest();

  // Navigate away.
  TestNavigationObserver navigation_observer2(web_contents());
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));
  navigation_observer2.WaitForNavigationFinished();

  // The page with the unsupported feature should be deleted (not cached).
  frame_deleted_observer.WaitUntilDeleted();
}

// Check that unload event handlers are not dispatched when the page goes
// into BackForwardCache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       ConfirmUnloadEventNotFired) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Set unload handler and check the title.
  EXPECT_TRUE(ExecJs(rfh_a,
                     "document.title = 'loaded!';"
                     "window.addEventListener('unload', () => {"
                     "  document.title = 'unloaded!';"
                     "});"));
  {
    std::u16string title_when_loaded = u"loaded!";
    TitleWatcher title_watcher(web_contents(), title_when_loaded);
    EXPECT_EQ(title_watcher.WaitAndGetTitle(), title_when_loaded);
  }

  // 3) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());

  // 4) Go back to A and check the title again.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  {
    std::u16string title_when_loaded = u"loaded!";
    TitleWatcher title_watcher(web_contents(), title_when_loaded);
    EXPECT_EQ(title_watcher.WaitAndGetTitle(), title_when_loaded);
  }
}

// TODO(https://crbug.com/1075936) disabled due to flakiness
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DISABLED_DoesNotCacheIfMainFrameStillLoading) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page that doesn't finish loading.
  GURL url(embedded_test_server()->GetURL("a.com", "/main_document"));
  TestNavigationManager navigation_manager(shell()->web_contents(), url);
  shell()->LoadURL(url);

  // The navigation starts.
  EXPECT_TRUE(navigation_manager.WaitForRequestStart());
  navigation_manager.ResumeNavigation();

  // The server sends the first part of the response and waits.
  response.WaitForRequest();
  response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "<html><body> ... ");

  // The navigation finishes while the body is still loading.
  navigation_manager.WaitForNavigationFinished();
  RenderFrameDeletedObserver delete_observer_rfh_a(current_frame_host());

  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // The page was still loading when we navigated away, so it shouldn't have
  // been cached.
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_FALSE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::kLoading}, {},
                    {}, {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoesNotCacheLoadingSubframe) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/controlled");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an iframe that loads forever.
  GURL url(embedded_test_server()->GetURL(
      "a.com", "/back_forward_cache/controllable_subframe.html"));
  TestNavigationManager navigation_manager(shell()->web_contents(), url);
  shell()->LoadURL(url);

  // The navigation finishes while the iframe is still loading.
  navigation_manager.WaitForNavigationFinished();

  // Wait for the iframe request to arrive, and leave it hanging with no
  // response.
  response.WaitForRequest();

  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // The page should not have been added to cache, since it had a subframe that
  // was still loading at the time it was navigated away from.
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored(
      {
          BackForwardCacheMetrics::NotRestoredReason::kLoading,
          BackForwardCacheMetrics::NotRestoredReason::kSubframeIsNavigating,
      },
      {}, {}, {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoesNotCacheLoadingSubframeOfSubframe) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/controlled");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an iframe that contains yet another iframe, that
  // hangs while loading.
  GURL url(embedded_test_server()->GetURL(
      "a.com", "/back_forward_cache/controllable_subframe_of_subframe.html"));
  TestNavigationManager navigation_manager(shell()->web_contents(), url);
  shell()->LoadURL(url);

  // The navigation finishes while the iframe within an iframe is still loading.
  navigation_manager.WaitForNavigationFinished();

  // Wait for the innermost iframe request to arrive, and leave it hanging with
  // no response.
  response.WaitForRequest();

  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_rfh_a(rfh_a);

  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // The page should not have been added to the cache, since it had an iframe
  // that was still loading at the time it was navigated away from.
  delete_rfh_a.WaitUntilDeleted();

  // 3) Go back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored(
      {
          BackForwardCacheMetrics::NotRestoredReason::kLoading,
          BackForwardCacheMetrics::NotRestoredReason::kSubframeIsNavigating,
      },
      {}, {}, {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, DoesNotCacheIfHttpError) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL error_url(embedded_test_server()->GetURL("a.com", "/page404.html"));
  GURL url(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to an error page.
  EXPECT_TRUE(NavigateToURL(shell(), error_url));
  EXPECT_EQ(net::HTTP_NOT_FOUND, current_frame_host()->last_http_status_code());
  RenderFrameDeletedObserver delete_rfh_a(current_frame_host());

  // Navigate away.
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // The page did not return 200 (OK), so it shouldn't have been cached.
  delete_rfh_a.WaitUntilDeleted();

  // Go back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kHTTPStatusNotOK}, {}, {},
      {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoesNotCacheIfPageUnreachable) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL error_url(embedded_test_server()->GetURL("a.com", "/empty.html"));
  GURL url(embedded_test_server()->GetURL("b.com", "/title1.html"));

  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      URLLoaderInterceptor::SetupRequestFailForURL(error_url,
                                                   net::ERR_DNS_TIMED_OUT);

  // Start with a successful navigation to a document.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_EQ(net::HTTP_OK, current_frame_host()->last_http_status_code());

  // Navigate to an error page.
  NavigationHandleObserver observer(shell()->web_contents(), error_url);
  EXPECT_FALSE(NavigateToURL(shell(), error_url));
  EXPECT_TRUE(observer.is_error());
  EXPECT_EQ(net::ERR_DNS_TIMED_OUT, observer.net_error_code());
  EXPECT_EQ(
      GURL(kUnreachableWebDataURL),
      shell()->web_contents()->GetMainFrame()->GetSiteInstance()->GetSiteURL());
  EXPECT_EQ(net::OK, current_frame_host()->last_http_status_code());

  RenderFrameDeletedObserver delete_rfh_a(current_frame_host());

  // Navigate away.
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // The page had a networking error, so it shouldn't have been cached.
  delete_rfh_a.WaitUntilDeleted();

  // Go back.
  web_contents()->GetController().GoBack();
  EXPECT_FALSE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kHTTPStatusNotOK,
       BackForwardCacheMetrics::NotRestoredReason::kNoResponseHead},
      {}, {}, {}, {}, FROM_HERE);
}

// Tests the events are fired when going back from the cache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, Events) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/back_forward_cache/record_events.html"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.com", "/back_forward_cache/record_events.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // At A, a page-show event is recorded for the first loading.
  MatchEventList(rfh_a.get(), ListValueOf("window.pageshow"));

  constexpr char kEventPageShowPersisted[] = "Event.PageShow.Persisted";

  content::FetchHistogramsFromChildProcesses();
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kEventPageShowPersisted),
      testing::UnorderedElementsAre(base::Bucket(
          static_cast<int>(blink::EventPageShowPersisted::kNoInRenderer), 1)));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImplWrapper rfh_b(current_frame_host());

  EXPECT_FALSE(rfh_a.IsRenderFrameDeleted());
  EXPECT_FALSE(rfh_b.IsRenderFrameDeleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());
  // TODO(yuzus): Post message to the frozen page, and make sure that the
  // messages arrive after the page visibility events, not before them.

  // As |rfh_a| is in back-forward cache, we cannot get the event list of A.
  // At B, a page-show event is recorded for the first loading.
  MatchEventList(rfh_b.get(), ListValueOf("window.pageshow"));
  content::FetchHistogramsFromChildProcesses();
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kEventPageShowPersisted),
      testing::UnorderedElementsAre(base::Bucket(
          static_cast<int>(blink::EventPageShowPersisted::kNoInRenderer), 2)));

  // 3) Go back to A. Confirm that expected events are fired.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_FALSE(rfh_a.IsRenderFrameDeleted());
  EXPECT_FALSE(rfh_b.IsRenderFrameDeleted());
  EXPECT_EQ(rfh_a.get(), current_frame_host());
  // visibilitychange events are added twice per each because it is fired for
  // both window and document.
  MatchEventList(
      rfh_a.get(),
      ListValueOf("window.pageshow", "window.pagehide.persisted",
                  "document.visibilitychange", "window.visibilitychange",
                  "document.freeze", "document.resume",
                  "document.visibilitychange", "window.visibilitychange",
                  "window.pageshow.persisted"));

  content::FetchHistogramsFromChildProcesses();
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kEventPageShowPersisted),
      testing::UnorderedElementsAre(
          base::Bucket(
              static_cast<int>(blink::EventPageShowPersisted::kNoInRenderer),
              2),
          base::Bucket(
              static_cast<int>(blink::EventPageShowPersisted::kYesInBrowser),
              1),
          base::Bucket(
              static_cast<int>(blink::EventPageShowPersisted::kYesInRenderer),
              1),
          base::Bucket(
              static_cast<int>(
                  blink::EventPageShowPersisted::kBrowserYesInRenderer),
              1),
          base::Bucket(
              static_cast<int>(
                  blink::EventPageShowPersisted::kBrowserYesInRendererWithPage),
              1),
          base::Bucket(
              static_cast<int>(
                  blink::EventPageShowPersisted::
                      kYesInBrowser_BackForwardCache_WillCommitNavigationToCachedEntry),
              1),
          base::Bucket(
              static_cast<int>(
                  blink::EventPageShowPersisted::
                      kYesInBrowser_BackForwardCache_RestoreEntry_Attempt),
              1),
          base::Bucket(
              static_cast<int>(
                  blink::EventPageShowPersisted::
                      kYesInBrowser_BackForwardCache_RestoreEntry_Succeed),
              1),
          base::Bucket(
              static_cast<int>(
                  blink::EventPageShowPersisted::
                      kYesInBrowser_RenderFrameHostManager_CommitPending),
              1)));
}

// Tests the events are fired for subframes when going back from the cache.
// Test case: a(b) -> c -> a(b)
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, EventsForSubframes) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to A(B).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  StartRecordingEvents(rfh_a);
  StartRecordingEvents(rfh_b);

  // 2) Navigate to C.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  RenderFrameHostImpl* rfh_c = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_c(rfh_c);
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_FALSE(rfh_c->IsInBackForwardCache());
  // TODO(yuzus): Post message to the frozen page, and make sure that the
  // messages arrive after the page visibility events, not before them.

  // 3) Go back to A(B). Confirm that expected events are fired on the subframe.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_FALSE(delete_observer_rfh_c.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());
  EXPECT_TRUE(rfh_c->IsInBackForwardCache());
  // visibilitychange events are added twice per each because it is fired for
  // both window and document.
  MatchEventList(
      rfh_a,
      ListValueOf("window.pagehide.persisted", "document.visibilitychange",
                  "window.visibilitychange", "document.freeze",
                  "document.resume", "document.visibilitychange",
                  "window.visibilitychange", "window.pageshow.persisted"));
  MatchEventList(
      rfh_b,
      ListValueOf("window.pagehide.persisted", "document.visibilitychange",
                  "window.visibilitychange", "document.freeze",
                  "document.resume", "document.visibilitychange",
                  "window.visibilitychange", "window.pageshow.persisted"));
}

// Tests the events are fired when going back from the cache.
// Same as: BackForwardCacheBrowserTest.Events, but with a document-initiated
// navigation. This is a regression test for https://crbug.com/1000324
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       EventsAfterDocumentInitiatedNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  StartRecordingEvents(rfh_a);

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURLFromRenderer(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());
  // TODO(yuzus): Post message to the frozen page, and make sure that the
  // messages arrive after the page visibility events, not before them.

  // 3) Go back to A. Confirm that expected events are fired.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());
  // visibilitychange events are added twice per each because it is fired for
  // both window and document.
  MatchEventList(
      rfh_a,
      ListValueOf("window.pagehide.persisted", "document.visibilitychange",
                  "window.visibilitychange", "document.freeze",
                  "document.resume", "document.visibilitychange",
                  "window.visibilitychange", "window.pageshow.persisted"));
}

// Track the events dispatched when a page is deemed ineligible for back-forward
// cache after we've dispatched the 'pagehide' event with persisted set to true.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       EventsForPageIneligibleAfterPagehidePersisted) {
  ASSERT_TRUE(CreateHttpsServer()->Start());
  GURL url_1(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_2(https_server()->GetURL("a.com", "/title2.html"));

  // 1) Navigate to |url_1|.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  RenderFrameHostImpl* rfh_1 = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_1(rfh_1);
  // 2) Use BroadcastChannel (a non-sticky blocklisted feature), so that we
  // would still do a RFH swap on same-site navigation and fire the 'pagehide'
  // event during commit of the new page with 'persisted' set to true, but the
  // page will not be eligible for back-forward cache after commit.
  EXPECT_TRUE(ExecJs(rfh_1, "window.foo = new BroadcastChannel('foo');"));

  EXPECT_TRUE(ExecJs(rfh_1, R"(
    window.onpagehide = (e) => {
      if (e.persisted) {
        window.domAutomationController.send('pagehide.persisted');
      }
    }
    document.onvisibilitychange = () => {
      if (document.visibilityState == 'hidden') {
        window.domAutomationController.send('visibilitychange.hidden');
      }
    }
    window.onunload = () => {
      window.domAutomationController.send('unload');
    }
  )"));

  DOMMessageQueue dom_message_queue(shell()->web_contents());
  // 3) Navigate to |url_2|.
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  // |rfh_1| will not get into the back-forward cache and eventually get deleted
  // because it uses a blocklisted feature.
  delete_observer_rfh_1.WaitUntilDeleted();

  // Only the pagehide and visibilitychange events will be dispatched.
  int num_messages_received = 0;
  std::string expected_messages[] = {"\"pagehide.persisted\"",
                                     "\"visibilitychange.hidden\""};
  std::string message;
  while (dom_message_queue.PopMessage(&message)) {
    EXPECT_EQ(expected_messages[num_messages_received], message);
    num_messages_received++;
  }
  EXPECT_EQ(num_messages_received, 2);
}

// Track the events dispatched when a page is deemed ineligible for back-forward
// cache before we've dispatched the pagehide event on it.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       EventsForPageIneligibleBeforePagehide) {
  ASSERT_TRUE(CreateHttpsServer()->Start());
  GURL url_1(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_2(https_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to |url_1|.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  RenderFrameHostImpl* rfh_1 = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_1(rfh_1);
  // 2) Use a dummy sticky blocklisted feature, so that the page is known to be
  // ineligible for bfcache at commit time, before we dispatch pagehide event.
  rfh_1->UseDummyStickyBackForwardCacheDisablingFeatureForTesting();

  EXPECT_TRUE(ExecJs(rfh_1, R"(
    window.onpagehide = (e) => {
      if (!e.persisted) {
        window.domAutomationController.send('pagehide.not_persisted');
      }
    }
    document.onvisibilitychange = () => {
      if (document.visibilityState == 'hidden') {
        window.domAutomationController.send('visibilitychange.hidden');
      }
    }
    window.onunload = () => {
      window.domAutomationController.send('unload');
    }
  )"));

  DOMMessageQueue dom_message_queue(shell()->web_contents());
  // 3) Navigate to |url_2|.
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  // |rfh_1| will not get into the back-forward cache and eventually get deleted
  // because it uses a blocklisted feature.
  delete_observer_rfh_1.WaitUntilDeleted();

  // "pagehide", "visibilitychange", and "unload" events will be dispatched.
  int num_messages_received = 0;
  std::string expected_messages[] = {"\"pagehide.not_persisted\"",
                                     "\"visibilitychange.hidden\"",
                                     "\"unload\""};
  std::string message;
  while (dom_message_queue.PopMessage(&message)) {
    EXPECT_EQ(expected_messages[num_messages_received], message);
    num_messages_received++;
  }
  EXPECT_EQ(num_messages_received, 3);
}

// Only HTTP/HTTPS main document can enter the BackForwardCache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, CacheHTTPDocumentOnly) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(CreateHttpsServer()->Start());

  GURL http_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL https_url(https_server()->GetURL("a.com", "/title1.html"));
  GURL file_url = net::FilePathToFileURL(GetTestFilePath("", "title1.html"));
  GURL data_url = GURL("data:text/html,");
  GURL blank_url = GURL(url::kAboutBlankURL);
  GURL webui_url = GetWebUIURL("gpu");

  enum { STORED, DELETED };
  struct {
    int expectation;
    GURL url;
  } test_cases[] = {
      // Only document with HTTP/HTTPS URLs are allowed to enter the
      // BackForwardCache.
      {STORED, http_url},
      {STORED, https_url},

      // Others aren't allowed.
      {DELETED, file_url},
      {DELETED, data_url},
      {DELETED, webui_url},
      {DELETED, blank_url},
  };

  char hostname[] = "a.unique";
  for (auto& test_case : test_cases) {
    SCOPED_TRACE(testing::Message()
                 << std::endl
                 << "expectation = " << test_case.expectation << std::endl
                 << "url = " << test_case.url << std::endl);

    // 1) Navigate to.
    EXPECT_TRUE(NavigateToURL(shell(), test_case.url));
    RenderFrameHostImplWrapper rfh(current_frame_host());

    // 2) Navigate away.
    hostname[0]++;
    GURL reset_url(embedded_test_server()->GetURL(hostname, "/title1.html"));
    EXPECT_TRUE(NavigateToURL(shell(), reset_url));

    if (test_case.expectation == STORED) {
      EXPECT_FALSE(rfh.IsRenderFrameDeleted());
      EXPECT_TRUE(rfh->IsInBackForwardCache());
      continue;
    }

    if (rfh.get() == current_frame_host()) {
      // If the RenderFrameHost is reused, it won't be deleted, so don't wait
      // for deletion. Just check that it's not saved in the back-forward cache.
      EXPECT_FALSE(rfh.IsRenderFrameDeleted());
      EXPECT_FALSE(rfh->IsInBackForwardCache());
      continue;
    }

    // When the RenderFrameHost is not reused and it's not stored in the
    // back-forward cache, it will eventually be deleted.
    ASSERT_TRUE(rfh.WaitUntilRenderFrameDeleted());
  }
}

// Regression test for https://crbug.com/993337.
//
// A note about sharing BrowsingInstances and the BackForwardCache:
//
// We should never keep around more than one main frame that belongs to the same
// BrowsingInstance. When swapping two pages, when one is stored in the
// back-forward cache or one is restored from it, the current code expects the
// two to live in different BrowsingInstances.
//
// History navigation can recreate a page with the same BrowsingInstance as the
// one stored in the back-forward cache. This case must to be handled. When it
// happens, the back-forward cache page is evicted.
//
// Since cache eviction is asynchronous, it's is possible for two main frames
// belonging to the same BrowsingInstance to be alive for a brief period of time
// (the new page being navigated to, and a page in the cache, until it is
// destroyed asynchronously via eviction).
//
// The test below tests that the brief period of time where two main frames are
// alive in the same BrowsingInstance does not cause anything to blow up.

// TODO(crbug.com/1127979): Flaky on Linux and Windows
#if defined(OS_LINUX) || defined(OS_WIN)
#define MAYBE_NavigateToTwoPagesOnSameSite DISABLED_NavigateToTwoPagesOnSameSite
#else
#define MAYBE_NavigateToTwoPagesOnSameSite NavigateToTwoPagesOnSameSite
#endif
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       MAYBE_NavigateToTwoPagesOnSameSite) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_b3(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));

  // 2) Navigate to A2.
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));
  RenderFrameHostImpl* rfh_a2 = current_frame_host();
  RenderFrameDeletedObserver delete_rfh_a2(current_frame_host());

  // 3) Navigate to B3.
  EXPECT_TRUE(NavigateToURL(shell(), url_b3));
  EXPECT_TRUE(rfh_a2->IsInBackForwardCache());
  RenderFrameHostImpl* rfh_b3 = current_frame_host();

  // 4) Do a history navigation back to A1.
  ASSERT_TRUE(HistoryGoToIndex(web_contents(), 0));
  EXPECT_TRUE(rfh_b3->IsInBackForwardCache());

  // Note that the frame for A1 gets created before A2 is deleted from the
  // cache, so there will be a brief period where two the main frames (A1 and
  // A2) are alive in the same BrowsingInstance/SiteInstance, at the same time.
  // That is the scenario this test is covering. This used to cause a CHECK,
  // because the two main frames shared a single RenderViewHost (no longer the
  // case after https://crrev.com/c/1833616).

  // A2 should be evicted from the cache and asynchronously deleted, due to the
  // cache size limit (B3 took its place in the cache).
  delete_rfh_a2.WaitUntilDeleted();
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       NavigateToTwoPagesOnSameSiteWithSubframes) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // This test covers the same scenario as NavigateToTwoPagesOnSameSite, except
  // the pages contain subframes:
  // A1(B) -> A2(B(C)) -> D3 -> A1(B)
  //
  // The subframes shouldn't make a difference, so the expected behavior is the
  // same as NavigateToTwoPagesOnSameSite.
  GURL url_a1(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_a2(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  GURL url_d3(embedded_test_server()->GetURL("d.com", "/title1.html"));

  // 1) Navigate to A1(B).
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));

  // 2) Navigate to A2(B(C)).
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));
  RenderFrameHostImpl* rfh_a2 = current_frame_host();
  RenderFrameDeletedObserver delete_rfh_a2(current_frame_host());

  // 3) Navigate to D3.
  EXPECT_TRUE(NavigateToURL(shell(), url_d3));
  EXPECT_TRUE(rfh_a2->IsInBackForwardCache());
  RenderFrameHostImpl* rfh_d3 = current_frame_host();

  // 4) Do a history navigation back to A1(B).
  ASSERT_TRUE(HistoryGoToIndex(web_contents(), 0));

  // D3 takes A2(B(C))'s place in the cache.
  EXPECT_TRUE(rfh_d3->IsInBackForwardCache());
  delete_rfh_a2.WaitUntilDeleted();
}

IN_PROC_BROWSER_TEST_F(HighCacheSizeBackForwardCacheBrowserTest,
                       CanCacheMultiplesPagesOnSameDomain) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b2(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_a3(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_b4(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));
  RenderFrameHostImpl* rfh_a1 = current_frame_host();

  // 2) Navigate to B2.
  EXPECT_TRUE(NavigateToURL(shell(), url_b2));
  RenderFrameHostImpl* rfh_b2 = current_frame_host();
  EXPECT_TRUE(rfh_a1->IsInBackForwardCache());

  // 3) Navigate to A3.
  EXPECT_TRUE(NavigateToURL(shell(), url_a3));
  RenderFrameHostImpl* rfh_a3 = current_frame_host();
  EXPECT_TRUE(rfh_a1->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b2->IsInBackForwardCache());
  // A1 and A3 shouldn't be treated as the same site instance.
  EXPECT_NE(rfh_a1->GetSiteInstance(), rfh_a3->GetSiteInstance());

  // 4) Navigate to B4.
  // Make sure we can store A1 and A3 in the cache at the same time.
  EXPECT_TRUE(NavigateToURL(shell(), url_b4));
  RenderFrameHostImpl* rfh_b4 = current_frame_host();
  EXPECT_TRUE(rfh_a1->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b2->IsInBackForwardCache());
  EXPECT_TRUE(rfh_a3->IsInBackForwardCache());

  // 5) Go back to A3.
  // Make sure we can restore A3, while A1 remains in the cache.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_TRUE(rfh_a1->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b2->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b4->IsInBackForwardCache());
  EXPECT_EQ(rfh_a3, current_frame_host());
  // B2 and B4 shouldn't be treated as the same site instance.
  EXPECT_NE(rfh_b2->GetSiteInstance(), rfh_b4->GetSiteInstance());

  // 6) Do a history navigation back to A1.
  // Make sure we can restore A1, while coming from A3.
  ASSERT_TRUE(HistoryGoToIndex(web_contents(), 0));
  EXPECT_TRUE(rfh_b2->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b4->IsInBackForwardCache());
  EXPECT_TRUE(rfh_a3->IsInBackForwardCache());
  EXPECT_EQ(rfh_a1, current_frame_host());
}

class BackForwardCacheBrowserTestSkipSameSiteUnload
    : public BackForwardCacheBrowserTest {
 public:
  BackForwardCacheBrowserTestSkipSameSiteUnload() = default;
  ~BackForwardCacheBrowserTestSkipSameSiteUnload() override = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    skip_same_site_if_unload_exists_ = true;
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// We won't cache pages with unload handler on same-site navigations when
// skip_same_site_if_unload_exists is set to true.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestSkipSameSiteUnload,
                       SameSiteNavigationFromPageWithUnload) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title2.html"));

  // 1) Navigate to A1 and add an unload handler.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));

  RenderFrameHostImpl* rfh_a1 = current_frame_host();
  EXPECT_TRUE(ExecJs(rfh_a1, "window.onunload = () => {} "));

  // 2) Navigate to A2.
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));
  RenderFrameHostImpl* rfh_a2 = current_frame_host();
  // We should not swap RFHs and A1 should not be in the back-forward cache.
  EXPECT_EQ(rfh_a1, rfh_a2);
  EXPECT_FALSE(rfh_a1->IsInBackForwardCache());
}

// We won't cache pages with an unload handler in a same-SiteInstance subframe
// on same-site navigations when skip_same_site_if_unload_exists is set to true.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestSkipSameSiteUnload,
                       SameSiteNavigationFromPageWithUnloadInSameSiteSubframe) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(a))"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title2.html"));

  // 1) Navigate to A1 and add an unload handler to a.com subframe.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));
  RenderFrameHostImpl* rfh_a_main = current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a_main->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_a_subframe =
      rfh_b->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(rfh_a_subframe, "window.onunload = () => {} "));

  // 2) Navigate to A2.
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));
  RenderFrameHostImpl* rfh_a2 = current_frame_host();
  // We should not swap RFHs and A1 should not be in the back-forward cache.
  EXPECT_EQ(rfh_a_main, rfh_a2);
  EXPECT_FALSE(rfh_a_main->IsInBackForwardCache());
}

// We won't cache pages with an unload handler in a cross-site subframe on
// same-site navigations when skip_same_site_if_unload_exists is set to true
// iff the cross-site subframe is in the same SiteInstance as the mainframe.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestSkipSameSiteUnload,
    SameSiteNavigationFromPageWithUnloadInCrossSiteSubframe) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title2.html"));

  // 1) Navigate to A1 and add an unload handler to b.com subframe.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));
  RenderFrameHostImpl* rfh_a1 = current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a1->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(rfh_b, "window.onunload = () => {} "));
  EXPECT_EQ(AreStrictSiteInstancesEnabled(),
            rfh_a1->GetSiteInstance() != rfh_b->GetSiteInstance());

  // 2) Navigate to A2.
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));
  RenderFrameHostImpl* rfh_a2 = current_frame_host();
  if (AreStrictSiteInstancesEnabled()) {
    // We should swap RFH & BIs and A1 should be in the back-forward cache.
    EXPECT_NE(rfh_a1, rfh_a2);
    EXPECT_FALSE(rfh_a1->GetSiteInstance()->IsRelatedSiteInstance(
        rfh_a2->GetSiteInstance()));
    EXPECT_TRUE(rfh_a1->IsInBackForwardCache());
  } else {
    // We should not swap RFHs and A1 should not be in the back-forward cache.
    EXPECT_EQ(rfh_a1, rfh_a2);
    EXPECT_FALSE(rfh_a1->IsInBackForwardCache());
  }
}

// We will cache pages with unload handler on cross-site navigations even when
// skip_same_site_if_unload_exists is set to true.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestSkipSameSiteUnload,
                       CrossSiteNavigationFromPageWithUnload) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a2(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to A and add an unload handler.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  EXPECT_TRUE(ExecJs(rfh_a, "window.onunload = () => {} "));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  // We should swap RFHs and A should be in the back-forward cache.
  EXPECT_NE(rfh_a, rfh_b);
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
}

// Sub-frame doesn't transition from LifecycleStateImpl::kInBackForwardCache to
// LifecycleStateImpl::kRunningUnloadHandlers even when the sub-frame having
// unload handlers is being evicted from BackForwardCache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, SubframeWithUnloadHandler) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a.com(a.com)"));
  GURL child_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a.com()");
  GURL url_2(embedded_test_server()->GetURL("a.com", "/title1.html"));

  // 1) Navigate to |main_url|.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImpl* main_rfh = current_frame_host();
  ASSERT_EQ(1U, main_rfh->child_count());
  RenderFrameHostImpl* child_rfh = main_rfh->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver main_rfh_observer(main_rfh),
      child_rfh_observer(child_rfh);

  // 2) Add an unload handler to the child RFH.
  EXPECT_TRUE(ExecJs(child_rfh, "window.onunload = () => {} "));

  // 3) Navigate to |url_2|.
  EXPECT_TRUE(NavigateToURL(shell(), url_2));

  // 4) The previous main RFH and child RFH should be in the back-forward
  // cache.
  EXPECT_FALSE(main_rfh_observer.deleted());
  EXPECT_FALSE(child_rfh_observer.deleted());
  EXPECT_TRUE(main_rfh->IsInBackForwardCache());
  EXPECT_TRUE(child_rfh->IsInBackForwardCache());

  // Destruction of bfcached page happens after shutdown and it should not
  // trigger unload handlers and be destroyed directly.
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, Encoding) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/back_forward_cache/charset_windows-1250.html"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.com", "/back_forward_cache/charset_utf-8.html"));
  url::Origin origin_a = url::Origin::Create(url_a);
  url::Origin origin_b = url::Origin::Create(url_b);

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  EXPECT_EQ(web_contents()->GetEncoding(), "windows-1250");

  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_EQ(web_contents()->GetEncoding(), "UTF-8");

  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(web_contents()->GetEncoding(), "windows-1250");
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoesNotCacheCrossSiteHttpPost) {
  SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Note we do a cross-site post because same-site navigations of any kind
  // aren't cached currently.
  GURL form_url(embedded_test_server()->GetURL(
      "a.com", "/form_that_posts_cross_site.html"));
  GURL redirect_target_url(embedded_test_server()->GetURL("x.com", "/echoall"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to the page with form that posts via 307 redirection to
  // |redirect_target_url| (cross-site from |form_url|).
  EXPECT_TRUE(NavigateToURL(shell(), form_url));

  // Submit the form.
  TestNavigationObserver form_post_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecJs(shell(), "document.getElementById('text-form').submit()"));
  form_post_observer.Wait();

  // Verify that we arrived at the expected, redirected location.
  EXPECT_EQ(redirect_target_url,
            shell()->web_contents()->GetLastCommittedURL());
  RenderFrameDeletedObserver delete_observer_rfh(current_frame_host());

  // Navigate away. |redirect_target_url|'s page should not be cached.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  delete_observer_rfh.WaitUntilDeleted();
}

// On windows, the expected value is off by ~20ms. In order to get the
// feature out to canary, the test is disabled for WIN.
// TODO(crbug.com/1022191): Fix this for Win.
// TODO(crbug.com/1211428): Flaky on other platforms.
// Make sure we are exposing the duration between back navigation's
// navigationStart and the page's original navigationStart through pageshow
// event's timeStamp, and that we aren't modifying
// performance.timing.navigationStart.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, DISABLED_NavigationStart) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/back_forward_cache/record_navigation_start_time_stamp.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  double initial_page_show_time_stamp =
      EvalJs(shell(), "window.initialPageShowTimeStamp").ExtractDouble();
  EXPECT_DOUBLE_EQ(
      initial_page_show_time_stamp,
      EvalJs(shell(), "window.latestPageShowTimeStamp").ExtractDouble());
  double initial_navigation_start =
      EvalJs(shell(), "window.initialNavigationStart").ExtractDouble();

  // 2) Navigate to B. A should be in the back forward cache.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Navigate back and expect everything to be restored.
  NavigationHandleObserver observer(web_contents(), url_a);
  base::TimeTicks time_before_navigation = base::TimeTicks::Now();
  double js_time_before_navigation =
      EvalJs(shell(), "performance.now()").ExtractDouble();
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  base::TimeTicks time_after_navigation = base::TimeTicks::Now();
  double js_time_after_navigation =
      EvalJs(shell(), "performance.now()").ExtractDouble();

  // The navigation start time should be between the time we saved just before
  // calling GoBack() and the time we saved just after calling GoBack().
  base::TimeTicks back_navigation_start = observer.navigation_start();
  EXPECT_LT(time_before_navigation, back_navigation_start);
  EXPECT_GT(time_after_navigation, back_navigation_start);

  // Check JS values. window.initialNavigationStart should not change.
  EXPECT_DOUBLE_EQ(
      initial_navigation_start,
      EvalJs(shell(), "window.initialNavigationStart").ExtractDouble());
  // performance.timing.navigationStart should not change.
  EXPECT_DOUBLE_EQ(
      initial_navigation_start,
      EvalJs(shell(), "performance.timing.navigationStart").ExtractDouble());
  // window.initialPageShowTimeStamp should not change.
  EXPECT_DOUBLE_EQ(
      initial_page_show_time_stamp,
      EvalJs(shell(), "window.initialPageShowTimeStamp").ExtractDouble());
  // window.latestPageShowTimeStamp should be updated with the timestamp of the
  // last pageshow event, which occurs after the page is restored. This should
  // be greater than the initial pageshow event's timestamp.
  double latest_page_show_time_stamp =
      EvalJs(shell(), "window.latestPageShowTimeStamp").ExtractDouble();
  EXPECT_LT(initial_page_show_time_stamp, latest_page_show_time_stamp);

  // |latest_page_show_time_stamp| should be the duration between initial
  // navigation start and |back_navigation_start|. Note that since
  // performance.timing.navigationStart returns a 64-bit integer instead of
  // double, we might be losing somewhere between 0 to 1 milliseconds of
  // precision, hence the usage of EXPECT_NEAR.
  EXPECT_NEAR(
      (back_navigation_start - base::TimeTicks::UnixEpoch()).InMillisecondsF(),
      latest_page_show_time_stamp + initial_navigation_start, 1.0);
  // Expect that the back navigation start value calculated from the JS results
  // are between time taken before & after navigation, just like
  // |before_navigation_start|.
  EXPECT_LT(js_time_before_navigation, latest_page_show_time_stamp);
  EXPECT_GT(js_time_after_navigation, latest_page_show_time_stamp);
}

// Do a same document navigation and make sure we do not fire the
// DidFirstVisuallyNonEmptyPaint again
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    DoesNotFireDidFirstVisuallyNonEmptyPaintForSameDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a_1(embedded_test_server()->GetURL(
      "a.com", "/accessibility/html/a-name.html"));
  GURL url_a_2(embedded_test_server()->GetURL(
      "a.com", "/accessibility/html/a-name.html#id"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a_1));
  WaitForFirstVisuallyNonEmptyPaint(shell()->web_contents());

  FirstVisuallyNonEmptyPaintObserver observer(web_contents());
  EXPECT_TRUE(NavigateToURL(shell(), url_a_2));
  // Make sure the bfcache restore code does not fire the event during commit
  // navigation.
  EXPECT_FALSE(observer.did_fire());
  EXPECT_TRUE(web_contents()->CompletedFirstVisuallyNonEmptyPaint());
}

// Make sure we fire DidFirstVisuallyNonEmptyPaint when restoring from bf-cache.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    FiresDidFirstVisuallyNonEmptyPaintWhenRestoredFromCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  WaitForFirstVisuallyNonEmptyPaint(shell()->web_contents());
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  WaitForFirstVisuallyNonEmptyPaint(shell()->web_contents());

  // 3) Navigate to back to A.
  FirstVisuallyNonEmptyPaintObserver observer(web_contents());
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  // Make sure the bfcache restore code does fire the event during commit
  // navigation.
  EXPECT_TRUE(web_contents()->CompletedFirstVisuallyNonEmptyPaint());
  EXPECT_TRUE(observer.did_fire());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       SetsThemeColorWhenRestoredFromCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/theme_color.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  WaitForFirstVisuallyNonEmptyPaint(web_contents());
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  EXPECT_EQ(web_contents()->GetThemeColor(), 0xFFFF0000u);

  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  WaitForFirstVisuallyNonEmptyPaint(web_contents());
  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_EQ(web_contents()->GetThemeColor(), absl::nullopt);

  ThemeColorObserver observer(web_contents());
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_TRUE(observer.did_fire());
  EXPECT_EQ(web_contents()->GetThemeColor(), 0xFFFF0000u);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       ContentsMimeTypeWhenRestoredFromCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  EXPECT_EQ(web_contents()->GetContentsMimeType(), "text/html");

  // Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Go back to A, which restores A from bfcache. ContentsMimeType should be
  // restored as well.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());
  ExpectRestored(FROM_HERE);
  EXPECT_EQ(web_contents()->GetContentsMimeType(), "text/html");
}

// Check BackForwardCache is enabled and works for devices with very low memory.
// Navigate from A -> B and go back.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       BackForwardCacheEnabledOnLowMemoryDevices) {
  // Set device physical memory to 10 MB.
  blink::ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(10);
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameDeletedObserver delete_observer_rfh_a(current_frame_host());
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2) Navigate to B. A should be in BackForwardCache.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Go back to A. B should be in BackForwardCache.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
}

// Test for functionality of memory controls in back-forward cache for low
// memory devices.
class BackForwardCacheBrowserTestForLowMemoryDevices
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);

    // Set the value of memory threshold more than the physical memory and check
    // if back-forward cache is disabled or not.
    std::string memory_threshold =
        base::NumberToString(base::SysInfo::AmountOfPhysicalMemoryMB() + 1);
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kBackForwardCacheMemoryControls,
          {{"memory_threshold_for_back_forward_cache_in_mb",
            memory_threshold}}},
         {blink::features::kLoadingTasksUnfreezable, {}}},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Navigate from A to B and go back.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestForLowMemoryDevices,
                       DisableBFCacheForLowEndDevices) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Ensure that the trial starts inactive.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));

  EXPECT_FALSE(IsBackForwardCacheEnabled());

  // Ensure that we do not activate the trial when querying bfcache status,
  // which is protected by low-memory setting.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) A shouldn't be stored in back-forward cache because the physical
  // memory is less than the memory threshold.
  delete_observer_rfh_a.WaitUntilDeleted();

  // Nothing is recorded when the memory is less than the threshold value.
  ExpectOutcomeDidNotChange(FROM_HERE);
  ExpectNotRestoredDidNotChange(FROM_HERE);

  // Ensure that the trial still hasn't been activated.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));
}

// Trigger network reqeuests, then navigate from A to B, then go back.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestForLowMemoryDevices,
                       DisableBFCacheForLowEndDevices_NetworkRequests) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Ensure that the trials starts inactive.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(
          blink::features::kLoadingTasksUnfreezable)
          ->trial_name()));

  EXPECT_FALSE(IsBackForwardCacheEnabled());

  // Ensure that we do not activate the trials for kBackForwardCache and
  // kLoadingTasksUnfreezable when querying bfcache or unfreezable loading tasks
  // status.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(
          blink::features::kLoadingTasksUnfreezable)
          ->trial_name()));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Request for an image and send a response to trigger loading code. This is
  // to ensure kLoadingTasksUnfreezable won't trigger bfcache activation.
  EXPECT_TRUE(ExecJs(rfh_a, R"(
      var image = document.createElement("img");
      image.src = "image.png";
      document.body.appendChild(image);
    )"));
  image_response.WaitForRequest();
  image_response.Send(net::HTTP_OK, "image/png");
  image_response.Send("image_body");
  image_response.Done();

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) A shouldn't be stored in back-forward cache because the physical
  // memory is less than the memory threshold.
  delete_observer_rfh_a.WaitUntilDeleted();

  // Nothing is recorded when the memory is less than the threshold value.
  ExpectOutcomeDidNotChange(FROM_HERE);
  ExpectNotRestoredDidNotChange(FROM_HERE);

  // Ensure that the trials still haven't been activated.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(
          blink::features::kLoadingTasksUnfreezable)
          ->trial_name()));
}

// Test for functionality of memory controls in back-forward cache for high
// memory devices.
class BackForwardCacheBrowserTestForHighMemoryDevices
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Set the value of memory threshold less than the physical memory and check
    // if back-forward cache is enabled or not.
    std::string memory_threshold =
        base::NumberToString(base::SysInfo::AmountOfPhysicalMemoryMB() - 1);
    EnableFeatureAndSetParams(features::kBackForwardCacheMemoryControls,
                              "memory_threshold_for_back_forward_cache_in_mb",
                              memory_threshold);
    EnableFeatureAndSetParams(blink::features::kLoadingTasksUnfreezable,
                              "max_buffered_bytes", "1000");

    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// Navigate from A to B and go back.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestForHighMemoryDevices,
                       EnableBFCacheForHighMemoryDevices) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) A should be stored in back-forward cache because the physical memory is
  // greater than the memory threshold.
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
}

// Trigger network reqeuests, then navigate from A to B, then go back.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestForHighMemoryDevices,
                       EnableBFCacheForHighMemoryDevices_NetworkRequests) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Ensure that back-forward cache flag is enabled and the trial is active.
  EXPECT_TRUE(IsBackForwardCacheEnabled());
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));

  // Ensure that the LoadingTasksUnfreezable trials starts as inactive.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(
          blink::features::kLoadingTasksUnfreezable)
          ->trial_name()));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Request for an image and send a response to trigger loading code.
  EXPECT_TRUE(ExecJs(rfh_a, R"(
      var image = document.createElement("img");
      image.src = "image.png";
      document.body.appendChild(image);
    )"));
  image_response.WaitForRequest();
  image_response.Send(net::HTTP_OK, "image/png");
  image_response.Send("image_body");
  image_response.Done();

  // The loading code activates the LoadingTasksUnfreezable trial.
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(
          blink::features::kLoadingTasksUnfreezable)
          ->trial_name()));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) A should be stored in back-forward cache because the physical memory is
  // greater than the memory threshold.
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Ensure that the trials stay activated.
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(
          blink::features::kLoadingTasksUnfreezable)
          ->trial_name()));
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       CanUseCacheWhenNavigatingAwayToErrorPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL error_url(embedded_test_server()->GetURL("b.com", "/empty.html"));
  auto url_interceptor = URLLoaderInterceptor::SetupRequestFailForURL(
      error_url, net::ERR_DNS_TIMED_OUT);

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2) Navigate to an error page and expect the old page to be stored in
  // bfcache.
  EXPECT_FALSE(NavigateToURL(shell(), error_url));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Navigate back and expect the page to be restored from bfcache.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
}

// Start an inifite dialogs in JS, yielding after each. The first dialog should
// be dismissed by navigation. The later dialogs should be handled gracefully
// and not appear while in BFCache. Finally, when the page comes out of BFCache,
// dialogs should appear again.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       CanUseCacheWhenPageAlertsInTimeoutLoop) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  AppModalDialogWaiter dialog_waiter(shell());

  EXPECT_TRUE(ExecJs(rfh_a, R"(
    function alertLoop() {
      setTimeout(alertLoop, 0);
      window.alert("alert");
    }
    // Don't block this script.
    setTimeout(alertLoop, 0);
  )"));

  dialog_waiter.Wait();

  // Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();

  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  ASSERT_THAT(rfh_a, InBackForwardCache());
  ASSERT_NE(rfh_a, rfh_b);

  dialog_waiter.Restart();

  // Go back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());

  // The page should still be requesting dialogs in a loop. Wait for one to be
  // requested.
  dialog_waiter.Wait();
}

// UnloadOldFrame will clear all dialogs. We test that further requests for
// dialogs coming from JS do not result in the creation of a dialog. This test
// posts some dialog creation JS to the render from inside the
// CommitNavigationCallback task. This JS is then able to post a task back to
// the renders to show a dialog. By the time this task runs, we the
// RenderFrameHostImpl's is_active() should be false.
//
// This test is not perfect, it can pass simply because the renderer thread does
// not run the JS in time. Ideally it would block until the renderer posts the
// request for a dialog but it's possible to do that without creating a nested
// message loop and if we do that, we risk processing the dialog request.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DialogsCancelledAndSuppressedWhenCached) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Let's us know whether the following callback ran. Not strictly necessary
  // since it really should run.
  bool posted_dialog_js = false;
  // Create a callback that will be called during the DidCommitNavigation task.
  WillEnterBackForwardCacheCallbackForTesting
      will_enter_back_forward_cache_callback =
          base::BindLambdaForTesting([&]() {
            // Post a dialog, it should not result in a dialog being created.
            ExecuteScriptAsync(rfh_a, R"(window.alert("alert");)");
            posted_dialog_js = true;
          });
  rfh_a->render_view_host()->SetWillEnterBackForwardCacheCallbackForTesting(
      will_enter_back_forward_cache_callback);

  AppModalDialogWaiter dialog_waiter(shell());

  // Try show another dialog. It should work.
  ExecuteScriptAsync(rfh_a, R"(window.alert("alert");)");
  dialog_waiter.Wait();

  dialog_waiter.Restart();

  // Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();

  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  ASSERT_THAT(rfh_a, InBackForwardCache());
  ASSERT_NE(rfh_a, rfh_b);
  // Test that the JS was run and that it didn't result in a dialog.
  ASSERT_TRUE(posted_dialog_js);
  ASSERT_FALSE(dialog_waiter.WasDialogRequestedCallbackCalled());

  // Go back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());

  // Try show another dialog. It should work.
  ExecuteScriptAsync(rfh_a, R"(window.alert("alert");)");
  dialog_waiter.Wait();
}

// RenderFrameHostImpl::coep_reporter() must be preserved when doing a back
// navigation using the BackForwardCache.
// Regression test for https://crbug.com/1102285.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, CoepReporter) {
  ASSERT_TRUE(CreateHttpsServer()->Start());
  GURL url_a(https_server()->GetURL("a.com",
                                    "/set-header?"
                                    "Cross-Origin-Embedder-Policy-Report-Only: "
                                    "require-corp; report-to%3d\"a\""));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  // Navigate to a document that set RenderFrameHostImpl::coep_reporter().
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  EXPECT_TRUE(rfh_a->coep_reporter());

  // Navigate away and back using the BackForwardCache. The
  // RenderFrameHostImpl::coep_reporter() must still be there.
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());

  EXPECT_TRUE(rfh_a->coep_reporter());
}

// RenderFrameHostImpl::coop_reporter() must be preserved when doing a back
// navigation using the BackForwardCache.
// Regression test for https://crbug.com/1102285.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, CoopReporter) {
  ASSERT_TRUE(CreateHttpsServer()->Start());
  GURL url_a(https_server()->GetURL("a.com",
                                    "/set-header?"
                                    "Cross-Origin-Opener-Policy-Report-Only: "
                                    "same-origin; report-to%3d\"a\""));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  // Navigate to a document that set RenderFrameHostImpl::coop_reporter().
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  EXPECT_TRUE(rfh_a->coop_access_report_manager()->coop_reporter());

  // Navigate away and back using the BackForwardCache. The
  // RenderFrameHostImpl::coop_reporter() must still be there.
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());

  EXPECT_TRUE(rfh_a->coop_access_report_manager()->coop_reporter());
}

// RenderFrameHostImpl::cross_origin_embedder_policy() must be preserved when
// doing a back navigation using the BackForwardCache.
// Regression test for https://crbug.com/1021846.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, Coep) {
  ASSERT_TRUE(CreateHttpsServer()->Start());
  GURL url_a(https_server()->GetURL(
      "a.com", "/set-header?Cross-Origin-Embedder-Policy: require-corp"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  // Navigate to a document that sets COEP.
  network::CrossOriginEmbedderPolicy coep;
  coep.value = network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  EXPECT_EQ(coep, rfh_a->cross_origin_embedder_policy());

  // Navigate away and back using the BackForwardCache.
  // RenderFrameHostImpl::cross_origin_embedder_policy() should return the same
  // result.
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());

  EXPECT_EQ(coep, rfh_a->cross_origin_embedder_policy());
}

// Tests that pagehide and visibilitychange handlers of the old RFH are run for
// bfcached pages.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       PagehideAndVisibilitychangeRuns) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL url_3(embedded_test_server()->GetURL("a.com", "/title2.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // 1) Navigate to |url_1|.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  RenderFrameHostImpl* main_frame_1 = web_contents->GetMainFrame();

  // Create a pagehide handler that sets item "pagehide_storage" and a
  // visibilitychange handler that sets item "visibilitychange_storage" in
  // localStorage.
  EXPECT_TRUE(ExecJs(main_frame_1,
                     R"(
    localStorage.setItem('pagehide_storage', 'not_dispatched');
    var dispatched_pagehide = false;
    window.onpagehide = function(e) {
      if (dispatched_pagehide) {
        // We shouldn't dispatch pagehide more than once.
        localStorage.setItem('pagehide_storage', 'dispatched_more_than_once');
      } else if (!e.persisted) {
        localStorage.setItem('pagehide_storage', 'wrong_persisted');
      } else {
        localStorage.setItem('pagehide_storage', 'dispatched_once');
      }
      dispatched_pagehide = true;
    }
    localStorage.setItem('visibilitychange_storage', 'not_dispatched');
    var dispatched_visibilitychange = false;
    document.onvisibilitychange = function(e) {
      if (dispatched_visibilitychange) {
        // We shouldn't dispatch visibilitychange more than once.
        localStorage.setItem('visibilitychange_storage',
          'dispatched_more_than_once');
      } else if (document.visibilityState != 'hidden') {
        // We should dispatch the event when the visibilityState is 'hidden'.
        localStorage.setItem('visibilitychange_storage', 'not_hidden');
      } else {
        localStorage.setItem('visibilitychange_storage', 'dispatched_once');
      }
      dispatched_visibilitychange = true;
    }
  )"));

  // 2) Navigate cross-site to |url_2|. We need to navigate cross-site to make
  // sure we won't run pagehide and visibilitychange during new page's commit,
  // which is tested in ProactivelySwapBrowsingInstancesSameSiteTest.
  EXPECT_TRUE(NavigateToURL(shell(), url_2));

  // |main_frame_1| should be in the back-forward cache.
  EXPECT_TRUE(main_frame_1->IsInBackForwardCache());

  // 3) Navigate to |url_3| which is same-origin with |url_1|, so we can check
  // the localStorage values.
  EXPECT_TRUE(NavigateToURL(shell(), url_3));
  RenderFrameHostImpl* main_frame_3 = web_contents->GetMainFrame();

  // Check that the value for 'pagehide_storage' and 'visibilitychange_storage'
  // are set correctly.
  EXPECT_EQ("dispatched_once",
            EvalJs(main_frame_3, "localStorage.getItem('pagehide_storage')"));
  EXPECT_EQ(
      "dispatched_once",
      EvalJs(main_frame_3, "localStorage.getItem('visibilitychange_storage')"));
}

// Tests that pagehide handlers of the old RFH are run for bfcached pages even
// if the page is already hidden (and visibilitychange won't run).
// Disabled on Linux and Win because of flakiness, see crbug.com/1170802.
// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
#if (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || defined(OS_WIN)
#define MAYBE_PagehideRunsWhenPageIsHidden DISABLED_PagehideRunsWhenPageIsHidden
#else
#define MAYBE_PagehideRunsWhenPageIsHidden PagehideRunsWhenPageIsHidden
#endif
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       MAYBE_PagehideRunsWhenPageIsHidden) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL url_3(embedded_test_server()->GetURL("a.com", "/title2.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // 1) Navigate to |url_1| and hide the tab.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  RenderFrameHostImpl* main_frame_1 = web_contents->GetMainFrame();
  // We need to set it to Visibility::VISIBLE first in case this is the first
  // time the visibility is updated.
  web_contents->UpdateWebContentsVisibility(Visibility::VISIBLE);
  web_contents->UpdateWebContentsVisibility(Visibility::HIDDEN);
  EXPECT_EQ(Visibility::HIDDEN, web_contents->GetVisibility());

  // Create a pagehide handler that sets item "pagehide_storage" and a
  // visibilitychange handler that sets item "visibilitychange_storage" in
  // localStorage.
  EXPECT_TRUE(ExecJs(main_frame_1,
                     R"(
    localStorage.setItem('pagehide_storage', 'not_dispatched');
    var dispatched_pagehide = false;
    window.onpagehide = function(e) {
      if (dispatched_pagehide) {
        // We shouldn't dispatch pagehide more than once.
        localStorage.setItem('pagehide_storage', 'dispatched_more_than_once');
      } else if (!e.persisted) {
        localStorage.setItem('pagehide_storage', 'wrong_persisted');
      } else {
        localStorage.setItem('pagehide_storage', 'dispatched_once');
      }
      dispatched_pagehide = true;
    }
    localStorage.setItem('visibilitychange_storage', 'not_dispatched');
    document.onvisibilitychange = function(e) {
      localStorage.setItem('visibilitychange_storage',
        'should_not_be_dispatched');
    }
  )"));
  // |visibilitychange_storage| should be set to its initial correct value.
  EXPECT_EQ(
      "not_dispatched",
      EvalJs(main_frame_1, "localStorage.getItem('visibilitychange_storage')"));

  // 2) Navigate cross-site to |url_2|. We need to navigate cross-site to make
  // sure we won't run pagehide and visibilitychange during new page's commit,
  // which is tested in ProactivelySwapBrowsingInstancesSameSiteTest.
  EXPECT_TRUE(NavigateToURL(shell(), url_2));

  // |main_frame_1| should be in the back-forward cache.
  EXPECT_TRUE(main_frame_1->IsInBackForwardCache());

  // 3) Navigate to |url_3| which is same-origin with |url_1|, so we can check
  // the localStorage values.
  EXPECT_TRUE(NavigateToURL(shell(), url_3));
  RenderFrameHostImpl* main_frame_3 = web_contents->GetMainFrame();

  // Check that the value for 'pagehide_storage' and 'visibilitychange_storage'
  // are set correctly.
  EXPECT_EQ("dispatched_once",
            EvalJs(main_frame_3, "localStorage.getItem('pagehide_storage')"));
  EXPECT_EQ(
      "not_dispatched",
      EvalJs(main_frame_3, "localStorage.getItem('visibilitychange_storage')"));
}

// Tests that we're getting the correct TextInputState and focus updates when a
// page enters the back-forward cache and when it gets restored.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, TextInputStateUpdated) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to |url_1| and add a text input with "foo" as the value.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  RenderFrameHostImpl* rfh_1 = current_frame_host();
  EXPECT_TRUE(ExecJs(rfh_1,
                     "document.title='bfcached';"
                     "var input = document.createElement('input');"
                     "input.setAttribute('type', 'text');"
                     "input.setAttribute('value', 'foo');"
                     "document.body.appendChild(input);"
                     "var focusCount = 0;"
                     "var blurCount = 0;"
                     "input.onfocus = () => { focusCount++;};"
                     "input.onblur = () => { blurCount++; };"));

  {
    TextInputManagerTypeObserver type_observer(web_contents(),
                                               ui::TEXT_INPUT_TYPE_TEXT);
    TextInputManagerValueObserver value_observer(web_contents(), "foo");
    // 2) Press tab key to focus the <input>, and verify the type & value.
    SimulateKeyPress(web_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                     ui::VKEY_TAB, false, false, false, false);
    type_observer.Wait();
    value_observer.Wait();

    EXPECT_EQ(rfh_1, web_contents()->GetFocusedFrame());
    EXPECT_EQ(EvalJs(rfh_1, "focusCount").ExtractInt(), 1);
    EXPECT_EQ(EvalJs(rfh_1, "blurCount").ExtractInt(), 0);
  }

  {
    TextInputManagerTester tester(web_contents());
    TextInputManagerValueObserver value_observer(web_contents(), "A");
    // 3) Press the "A" key to change the text input value. This should notify
    // the browser that the text input value has changed.
    SimulateKeyPress(web_contents(), ui::DomKey::FromCharacter('A'),
                     ui::DomCode::US_A, ui::VKEY_A, false, false, false, false);
    value_observer.Wait();

    EXPECT_EQ(rfh_1, web_contents()->GetFocusedFrame());
    EXPECT_EQ(EvalJs(rfh_1, "focusCount").ExtractInt(), 1);
    EXPECT_EQ(EvalJs(rfh_1, "blurCount").ExtractInt(), 0);
  }

  {
    TextInputManagerTypeObserver type_observer(web_contents(),
                                               ui::TEXT_INPUT_TYPE_NONE);
    // 4) Navigating to |url_2| should reset type to TEXT_INPUT_TYPE_NONE.
    EXPECT_TRUE(NavigateToURL(shell(), url_2));
    type_observer.Wait();
    // |rfh_1| should get into the back-forward cache.
    EXPECT_TRUE(rfh_1->IsInBackForwardCache());
    EXPECT_EQ(current_frame_host(), web_contents()->GetFocusedFrame());
    EXPECT_NE(rfh_1, web_contents()->GetFocusedFrame());
  }

  {
    // 5) Navigating back to |url_1|, we shouldn't restore the focus to the
    // text input, but |rfh_1| will be focused again as we will restore focus
    // to main frame after navigation.
    ASSERT_TRUE(HistoryGoBack(web_contents()));

    EXPECT_EQ(rfh_1, web_contents()->GetFocusedFrame());
    EXPECT_EQ(EvalJs(rfh_1, "focusCount").ExtractInt(), 1);
    EXPECT_EQ(EvalJs(rfh_1, "blurCount").ExtractInt(), 1);
  }

  {
    TextInputManagerTypeObserver type_observer(web_contents(),
                                               ui::TEXT_INPUT_TYPE_TEXT);
    TextInputManagerValueObserver value_observer(web_contents(), "A");
    // 6) Press tab key to focus the <input> again. Note that we need to press
    // the tab key twice here, because the last "tab focus" point was the
    // <input> element. The first tab key press would focus on the UI/url bar,
    // then the second tab key would go back to the <input>.
    SimulateKeyPress(web_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                     ui::VKEY_TAB, false, false, false, false);
    SimulateKeyPress(web_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                     ui::VKEY_TAB, false, false, false, false);
    type_observer.Wait();
    value_observer.Wait();

    EXPECT_EQ(rfh_1, web_contents()->GetFocusedFrame());
    EXPECT_EQ(EvalJs(rfh_1, "focusCount").ExtractInt(), 2);
    EXPECT_EQ(EvalJs(rfh_1, "blurCount").ExtractInt(), 1);
  }
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       SubframeTextInputStateUpdated) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(a))"));
  GURL url_2(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to |url_1| and add a text input with "foo" as the value in the
  // a.com subframe.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_subframe_a =
      rfh_b->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(rfh_subframe_a,
                     "var input = document.createElement('input');"
                     "input.setAttribute('type', 'text');"
                     "input.setAttribute('value', 'foo');"
                     "document.body.appendChild(input);"
                     "var focusCount = 0;"
                     "var blurCount = 0;"
                     "input.onfocus = () => { focusCount++;};"
                     "input.onblur = () => { blurCount++; };"));

  {
    TextInputManagerTypeObserver type_observer(web_contents(),
                                               ui::TEXT_INPUT_TYPE_TEXT);
    TextInputManagerValueObserver value_observer(web_contents(), "foo");
    // 2) Press tab key to focus the <input>, and verify the type & value.
    SimulateKeyPress(web_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                     ui::VKEY_TAB, false, false, false, false);
    type_observer.Wait();
    value_observer.Wait();

    EXPECT_EQ(rfh_subframe_a, web_contents()->GetFocusedFrame());
    EXPECT_EQ(EvalJs(rfh_subframe_a, "focusCount").ExtractInt(), 1);
    EXPECT_EQ(EvalJs(rfh_subframe_a, "blurCount").ExtractInt(), 0);
  }

  {
    TextInputManagerTester tester(web_contents());
    TextInputManagerValueObserver value_observer(web_contents(), "A");
    // 3) Press the "A" key to change the text input value. This should notify
    // the browser that the text input value has changed.
    SimulateKeyPress(web_contents(), ui::DomKey::FromCharacter('A'),
                     ui::DomCode::US_A, ui::VKEY_A, false, false, false, false);
    value_observer.Wait();

    EXPECT_EQ(rfh_subframe_a, web_contents()->GetFocusedFrame());
    EXPECT_EQ(EvalJs(rfh_subframe_a, "focusCount").ExtractInt(), 1);
    EXPECT_EQ(EvalJs(rfh_subframe_a, "blurCount").ExtractInt(), 0);
  }

  {
    TextInputManagerTypeObserver type_observer(web_contents(),
                                               ui::TEXT_INPUT_TYPE_NONE);
    // 4) Navigating to |url_2| should reset type to TEXT_INPUT_TYPE_NONE and
    // changed focus to the new page's main frame.
    EXPECT_TRUE(NavigateToURL(shell(), url_2));
    type_observer.Wait();

    // |rfh_a| and its subframes should get into the back-forward cache.
    EXPECT_TRUE(rfh_a->IsInBackForwardCache());
    EXPECT_TRUE(rfh_b->IsInBackForwardCache());
    EXPECT_TRUE(rfh_subframe_a->IsInBackForwardCache());
    EXPECT_EQ(current_frame_host(), web_contents()->GetFocusedFrame());
  }

  {
    // 5) Navigating back to |url_1|, we shouldn't restore the focus to the
    // text input in the subframe (we will focus on the main frame |rfh_a|
    // instead).
    ASSERT_TRUE(HistoryGoBack(web_contents()));

    EXPECT_EQ(rfh_a, web_contents()->GetFocusedFrame());
    EXPECT_EQ(EvalJs(rfh_subframe_a, "focusCount").ExtractInt(), 1);
    EXPECT_EQ(EvalJs(rfh_subframe_a, "blurCount").ExtractInt(), 1);
  }

  {
    TextInputManagerTypeObserver type_observer(web_contents(),
                                               ui::TEXT_INPUT_TYPE_TEXT);
    TextInputManagerValueObserver value_observer(web_contents(), "A");
    // 6) Press tab key to focus the <input> again.
    SimulateKeyPress(web_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                     ui::VKEY_TAB, false, false, false, false);
    type_observer.Wait();
    value_observer.Wait();

    EXPECT_EQ(rfh_subframe_a, web_contents()->GetFocusedFrame());
    EXPECT_EQ(EvalJs(rfh_subframe_a, "focusCount").ExtractInt(), 2);
    EXPECT_EQ(EvalJs(rfh_subframe_a, "blurCount").ExtractInt(), 1);
  }
}

// Tests that trying to focus on a BFCached cross-site iframe won't crash.
// See https://crbug.com/1250218.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       FocusSameSiteSubframeOnPagehide) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  GURL main_url_2(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to a page with a same-site iframe.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImplWrapper rfh_1(current_frame_host());
  EXPECT_EQ(rfh_1.get(), web_contents()->GetFocusedFrame());

  // 2) Navigate away from the page while trying to focus the subframe on
  // pagehide. The DidFocusFrame IPC should arrive after the page gets into
  // BFCache and should be ignored by the browser. The focus after navigation
  // should go to the new main frame.
  EXPECT_TRUE(ExecJs(rfh_1.get(), R"(
    window.onpagehide = function(e) {
      document.getElementById("test_iframe").focus();
  })"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url_2));
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());
  EXPECT_NE(rfh_1.get(), web_contents()->GetFocusedFrame());
  EXPECT_EQ(current_frame_host(), web_contents()->GetFocusedFrame());

  // 3) Navigate back to the page. The focus should be on the main frame.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(rfh_1.get(), web_contents()->GetFocusedFrame());
  ExpectRestored(FROM_HERE);
}

// Tests that trying to focus on a BFCached cross-site iframe won't crash.
// See https://crbug.com/1250218.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       FocusCrossSiteSubframeOnPagehide) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL main_url_2(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to a page with a cross-site iframe.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImplWrapper rfh_1(current_frame_host());
  EXPECT_EQ(rfh_1.get(), web_contents()->GetFocusedFrame());

  // 2) Navigate away from the page while trying to focus the subframe on
  // pagehide. The DidFocusFrame IPC should arrive after the page gets into
  // BFCache and should be ignored by the browser. The focus after navigation
  // should go to the new main frame.
  EXPECT_TRUE(ExecJs(rfh_1.get(), R"(
    window.onpagehide = function(e) {
      document.getElementById("child-0").focus();
    })"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url_2));
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());
  EXPECT_NE(rfh_1.get(), web_contents()->GetFocusedFrame());

  // 3) Navigate back to the page. The focus should be on the original page's
  // main frame.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(rfh_1.get(), current_frame_host());
  ExpectRestored(FROM_HERE);
}

// Tests that the history value saved in the renderer is updated correctly when
// a page gets restored from the back-forward cache through browser-initiated
// navigation.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       RendererHistory_BrowserInitiated) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url2(embedded_test_server()->GetURL("a.com", "/title1.html"));

  // 1) Go to |url1|, then |url2|. Both pages should have script to save the
  // history.length value when getting restored from the back-forward cache.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* subframe = root->child_at(0);

  std::string restore_time_length_saver_script =
      "var resumeLength = -1;"
      "var pageshowLength = -1;"
      "document.onresume = () => {"
      "  resumeLength = history.length;"
      "};"
      "window.onpageshow  = () => {"
      "  pageshowLength = history.length;"
      "};";
  EXPECT_TRUE(ExecJs(root, restore_time_length_saver_script));
  EXPECT_TRUE(ExecJs(subframe, restore_time_length_saver_script));
  // We should have one history entry.
  EXPECT_EQ(EvalJs(root, "history.length").ExtractInt(), 1);
  EXPECT_EQ(EvalJs(subframe, "history.length").ExtractInt(), 1);

  EXPECT_TRUE(NavigateToURL(shell(), url2));
  EXPECT_TRUE(ExecJs(root, restore_time_length_saver_script));
  // We should have two history entries.
  EXPECT_EQ(EvalJs(root, "history.length").ExtractInt(), 2);

  // 2) Go back to |url1|, browser-initiated.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url1);

  // We should still have two history entries, and recorded the correct length
  // when the 'resume' and 'pageshow' events were dispatched.
  EXPECT_EQ(EvalJs(root, "history.length").ExtractInt(), 2);
  EXPECT_EQ(EvalJs(root, "resumeLength").ExtractInt(), 2);
  EXPECT_EQ(EvalJs(root, "pageshowLength").ExtractInt(), 2);
  EXPECT_EQ(EvalJs(subframe, "history.length").ExtractInt(), 2);
  EXPECT_EQ(EvalJs(subframe, "resumeLength").ExtractInt(), 2);
  EXPECT_EQ(EvalJs(subframe, "pageshowLength").ExtractInt(), 2);

  // 3) Go forward to |url2|, browser-initiated.
  ASSERT_TRUE(HistoryGoForward(web_contents()));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url2);

  // We should still have two history entries, and recorded the correct length
  // when the 'resume' and 'pageshow' events were dispatched.
  EXPECT_EQ(EvalJs(root, "history.length").ExtractInt(), 2);
  EXPECT_EQ(EvalJs(root, "resumeLength").ExtractInt(), 2);
  EXPECT_EQ(EvalJs(root, "pageshowLength").ExtractInt(), 2);
}

// Tests that the history value saved in the renderer is updated correctly when
// a page gets restored from the back-forward cache through renderer-initiated
// navigation.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       RendererHistory_RendererInitiated) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url1(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url2(embedded_test_server()->GetURL("a.com", "/title1.html"));

  // 1) Go to |url1|, then |url2|. Both pages should have script to save the
  // history.length value when getting restored from the back-forward cache.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* subframe = root->child_at(0);

  std::string restore_time_length_saver_script =
      "var resumeLength = -1;"
      "var pageshowLength = -1;"
      "document.onresume = () => {"
      "  resumeLength = history.length;"
      "};"
      "window.onpageshow  = () => {"
      "  pageshowLength = history.length;"
      "};";
  EXPECT_TRUE(ExecJs(root, restore_time_length_saver_script));
  EXPECT_TRUE(ExecJs(subframe, restore_time_length_saver_script));
  // We should have one history entry.
  EXPECT_EQ(EvalJs(root, "history.length").ExtractInt(), 1);
  EXPECT_EQ(EvalJs(subframe, "history.length").ExtractInt(), 1);

  EXPECT_TRUE(NavigateToURL(shell(), url2));
  EXPECT_TRUE(ExecJs(root, restore_time_length_saver_script));
  // We should have two history entries.
  EXPECT_EQ(EvalJs(root, "history.length").ExtractInt(), 2);

  // 2) Go back to |url1|, renderer-initiated.
  EXPECT_TRUE(ExecJs(root, "history.back()"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url1);

  // We should still have two history entries, and recorded the correct length
  // when the 'resume' and 'pageshow' events were dispatched.
  EXPECT_EQ(EvalJs(root, "history.length").ExtractInt(), 2);
  EXPECT_EQ(EvalJs(root, "resumeLength").ExtractInt(), 2);
  EXPECT_EQ(EvalJs(root, "pageshowLength").ExtractInt(), 2);
  EXPECT_EQ(EvalJs(subframe, "history.length").ExtractInt(), 2);
  EXPECT_EQ(EvalJs(subframe, "resumeLength").ExtractInt(), 2);
  EXPECT_EQ(EvalJs(subframe, "pageshowLength").ExtractInt(), 2);

  // 3) Go forward to |url2|, renderer-initiated.
  EXPECT_TRUE(ExecJs(root, "history.forward()"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url2);

  // We should still have two history entries, and recorded the correct length
  // when the 'resume' and 'pageshow' events were dispatched.
  EXPECT_EQ(EvalJs(root, "history.length").ExtractInt(), 2);
  EXPECT_EQ(EvalJs(root, "resumeLength").ExtractInt(), 2);
  EXPECT_EQ(EvalJs(root, "pageshowLength").ExtractInt(), 2);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       MainDocumentCSPHeadersAreRestored) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL(
      "a.com",
      "/set-header?"
      "Content-Security-Policy: frame-src 'none'"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A, which should set CSP.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // Check that CSP was set.
  {
    const std::vector<network::mojom::ContentSecurityPolicyPtr>& root_csp =
        current_frame_host()
            ->policy_container_host()
            ->policies()
            .content_security_policies;
    EXPECT_EQ(1u, root_csp.size());
    EXPECT_EQ("frame-src 'none'", root_csp[0]->header->header_value);
  }

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Navigate back and expect that the CSP headers are present on the main
  // frame.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());
  ExpectRestored(FROM_HERE);

  // Check that CSP was restored.
  {
    const std::vector<network::mojom::ContentSecurityPolicyPtr>& root_csp =
        current_frame_host()
            ->policy_container_host()
            ->policies()
            .content_security_policies;
    EXPECT_EQ(1u, root_csp.size());
    EXPECT_EQ("frame-src 'none'", root_csp[0]->header->header_value);
  }
}

// Check that sandboxed documents are cached and won't lose their sandbox flags
// after restoration.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, CspSandbox) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(
      embedded_test_server()->GetURL("a.com",
                                     "/set-header?"
                                     "Content-Security-Policy: sandbox"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A, which should set CSP.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  {
    const std::vector<network::mojom::ContentSecurityPolicyPtr>& root_csp =
        current_frame_host()
            ->policy_container_host()
            ->policies()
            .content_security_policies;
    ASSERT_EQ(1u, root_csp.size());
    ASSERT_EQ("sandbox", root_csp[0]->header->header_value);
    ASSERT_EQ(network::mojom::WebSandboxFlags::kAll,
              current_frame_host()->active_sandbox_flags());
  };

  // 2) Navigate to B. Expect the previous RenderFrameHost to enter the bfcache.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  {
    const std::vector<network::mojom::ContentSecurityPolicyPtr>& root_csp =
        current_frame_host()
            ->policy_container_host()
            ->policies()
            .content_security_policies;
    ASSERT_EQ(0u, root_csp.size());
    ASSERT_EQ(network::mojom::WebSandboxFlags::kNone,
              current_frame_host()->active_sandbox_flags());
  };

  // 3) Navigate back and expect the page to be restored, with the correct
  // CSP and sandbox flags.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_EQ(current_frame_host(), rfh_a);
  {
    const std::vector<network::mojom::ContentSecurityPolicyPtr>& root_csp =
        current_frame_host()
            ->policy_container_host()
            ->policies()
            .content_security_policies;
    ASSERT_EQ(1u, root_csp.size());
    ASSERT_EQ("sandbox", root_csp[0]->header->header_value);
    ASSERT_EQ(network::mojom::WebSandboxFlags::kAll,
              current_frame_host()->active_sandbox_flags());
  };
}

// Check that about:blank is not cached.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, AboutBlankWillNotBeCached) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to about:blank.
  GURL blank_url(url::kAboutBlankURL);
  EXPECT_TRUE(NavigateToURL(shell(), blank_url));

  // 2) Navigate to a.com.
  GURL url_a(embedded_test_server()->GetURL("a.com", "/empty.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  // 3) Navigate back to about:blank.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  // This about:blank document does not have a SiteInstance and then loading a
  // page on it doesn't swap the browsing instance.
  ExpectNotRestored(
      {
          BackForwardCacheMetrics::NotRestoredReason::
              kBrowsingInstanceNotSwapped,
      },
      {}, {ShouldSwapBrowsingInstance::kNo_DoesNotHaveSite}, {}, {}, FROM_HERE);
}

// Check that an eligible page is cached when navigating to about:blank.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       NavigatingToAboutBlankDoesNotPreventCaching) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a.com,
  GURL url_a(embedded_test_server()->GetURL("a.com", "/empty.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  // 2) Navigate to about:blank.
  GURL blank_url(url::kAboutBlankURL);
  EXPECT_TRUE(NavigateToURL(shell(), blank_url));

  // 3) Navigate back to a.com.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  ExpectRestored(FROM_HERE);
}

// Check that browsing instances are not swapped when a navigation redirects
// toward the last committed URL and the reasons are recorded correctly.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, RedirectToSelf) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigationControllerImpl& controller = web_contents()->GetController();

  // 1) Navigate to a.com/empty.html.
  GURL url_a(embedded_test_server()->GetURL("a.com", "/empty.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(url_a, controller.GetLastCommittedEntry()->GetURL());

  // 2) Navigate to the same page by redirection.
  GURL url_a2(embedded_test_server()->GetURL(
      "a.com", "/server-redirect-301?" + url_a.spec()));
  EXPECT_TRUE(NavigateToURL(shell(), url_a2, url_a));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  EXPECT_EQ(2, controller.GetEntryCount());

  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_a->GetSiteInstance()->IsRelatedSiteInstance(
      rfh_b->GetSiteInstance()));
  EXPECT_EQ(url_a, controller.GetLastCommittedEntry()->GetURL());

  // 3) Navigate back to the previous page.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(url_a, controller.GetLastCommittedEntry()->GetURL());

  // TODO(crbug.com/1198030): Investigate whether these navigation results are
  // expected.

  ExpectNotRestored(
      {
          BackForwardCacheMetrics::NotRestoredReason::
              kBrowsingInstanceNotSwapped,
      },
      {}, {ShouldSwapBrowsingInstance::kNo_SameUrlNavigation}, {}, {},
      FROM_HERE);
}

// Check that the response 204 No Content doesn't affect back-forward cache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, NoContent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigationControllerImpl& controller = web_contents()->GetController();

  // 1) Navigate to a.com.
  GURL url_a(embedded_test_server()->GetURL("a.com", "/empty.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(url_a, controller.GetLastCommittedEntry()->GetURL());

  // 2) Navigate to b.com
  GURL url_b(embedded_test_server()->GetURL("b.com", "/empty.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(url_b, controller.GetLastCommittedEntry()->GetURL());

  // 3) Navigate to c.com with 204 No Content, then the URL will still be b.com.
  GURL url_c(embedded_test_server()->GetURL("c.com", "/echo?status=204"));
  EXPECT_TRUE(NavigateToURL(shell(), url_c, url_b));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(url_b, controller.GetLastCommittedEntry()->GetURL());

  // 4) Navigate back to a.com.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(url_a, controller.GetLastCommittedEntry()->GetURL());

  ExpectRestored(FROM_HERE);
}

// Check that reloading doesn't affect the back-forward cache usage.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, ReloadDoesntAffectCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigationControllerImpl& controller = web_contents()->GetController();

  // 1) Navigate to a.com.
  GURL url_a(embedded_test_server()->GetURL("a.com", "/empty.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(url_a, controller.GetLastCommittedEntry()->GetURL());

  // 2) Navigate to b.com.
  GURL url_b(embedded_test_server()->GetURL("b.com", "/empty.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(url_b, controller.GetLastCommittedEntry()->GetURL());

  // 3) Go back to a.com and reload.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(url_a, controller.GetLastCommittedEntry()->GetURL());

  ExpectRestored(FROM_HERE);

  // 4) Reload the tab.
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(url_a, controller.GetLastCommittedEntry()->GetURL());

  // By reloading the tab, ShouldSwapBrowsingInstance::
  // kNo_AlreadyHasMatchingBrowsingInstance is set once. This should be reset
  // when the navigation 4)'s commit finishes and should not prevent putting the
  // page into the back-forward cache.
  //
  // Note that SetBrowsingInstanceSwapResult might not be called for every
  // navigation because we might not get to this point for some navigations,
  // e.g. if the navigation uses a pre-existing RenderFrameHost and SiteInstance
  // for navigation.
  //
  // TODO(crbug.com/1176061): Tie BrowsingInstanceSwapResult to
  // NavigationRequest instead and move the SetBrowsingInstanceSwapResult call
  // for navigations to happen at commit time instead.

  // 5) Go forward to b.com and reload.
  ASSERT_TRUE(HistoryGoForward(web_contents()));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(url_b, controller.GetLastCommittedEntry()->GetURL());

  // The page loaded at B) is correctly cached and restored. Reloading doesn't
  // affect the cache usage.
  ExpectRestored(FROM_HERE);

  // 6) Go back to a.com.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(url_a, controller.GetLastCommittedEntry()->GetURL());

  // The page loaded at 3) is correctly cached and restored. Reloading doesn't
  // affect the cache usage.
  ExpectRestored(FROM_HERE);
}

// Regression test for crbug.com/1183313. Checks that CommitNavigationParam's
// |has_user_gesture| value reflects the gesture from the latest navigation
// after the commit finished.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    SameDocumentNavAfterRestoringDocumentLoadedWithUserGesture) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a_foo(embedded_test_server()->GetURL("a.com", "/title1.html#foo"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  NavigationControllerImpl& controller = web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Initial navigation (so that we can initiate a navigation from renderer).
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  // 1) Navigate to A with user gesture.
  {
    FrameNavigateParamsCapturer params_capturer(root);
    EXPECT_TRUE(NavigateToURLFromRenderer(shell(), url_a));
    params_capturer.Wait();
    EXPECT_TRUE(params_capturer.has_user_gesture());
    EXPECT_TRUE(root->current_frame_host()
                    ->last_navigation_started_with_transient_activation());
  }
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2) Navigate to B. A should be stored in the back-forward cache.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(root->current_frame_host()
                   ->last_navigation_started_with_transient_activation());

  // 3) GoBack to A. RenderFrameHost of A should be restored from the
  // back-forward cache, and "has_user_gesture" is set to false correctly.
  // Note that since this is a back-forward cache restore we create the
  // DidCommitProvisionalLoadParams completely in the browser, so we got the
  // correct value from the latest navigation. However, we did not update the
  // renderer's navigation-related values, so the renderer's DocumentLoader
  // still thinks the last "gesture" value is "true", which will get corrected
  // on the next navigation.
  {
    FrameNavigateParamsCapturer params_capturer(root);
    controller.GoBack();
    params_capturer.Wait();
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
    EXPECT_EQ(rfh_a, current_frame_host());
    // The navigation doesn't have user gesture.
    EXPECT_FALSE(params_capturer.has_user_gesture());
    EXPECT_FALSE(root->current_frame_host()
                     ->last_navigation_started_with_transient_activation());
  }

  // 4) Same-document navigation to A#foo without user gesture. At this point
  // we will update the renderer's DocumentLoader's latest gesture value to
  // "no user gesture", and we'll get the correct gesture value in
  // DidCommitProvisionalLoadParams.
  {
    FrameNavigateParamsCapturer params_capturer(root);
    EXPECT_TRUE(
        NavigateToURLFromRendererWithoutUserGesture(shell(), url_a_foo));
    params_capturer.Wait();
    // The navigation doesn't have user gesture.
    EXPECT_FALSE(params_capturer.has_user_gesture());
    EXPECT_FALSE(root->current_frame_host()
                     ->last_navigation_started_with_transient_activation());
  }
}

class BackForwardCacheOptInBrowserTest : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(features::kBackForwardCache,
                              "opt_in_header_required", "true");
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(BackForwardCacheOptInBrowserTest, NoCacheWithoutHeader) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Go back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kOptInUnloadHeaderNotPresent},
                    {}, {}, {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheOptInBrowserTest,
                       CacheIfHeaderIsPresent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com",
                                            "/set-header?"
                                            "BFCache-Opt-In: unload"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Go back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheOptInBrowserTest,
                       NoCacheIfHeaderOnlyPresentOnDestinationPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com",
                                            "/set-header?"
                                            "BFCache-Opt-In: unload"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Go back. - A doesn't have header so it shouldn't be cached.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kOptInUnloadHeaderNotPresent},
                    {}, {}, {}, {}, FROM_HERE);

  // 4) Go forward. - B has the header, so it should be cached.
  ASSERT_TRUE(HistoryGoForward(web_contents()));

  ExpectRestored(FROM_HERE);
}

class BackForwardCacheUnloadStrategyBrowserTest
    : public BackForwardCacheBrowserTest,
      public testing::WithParamInterface<std::string> {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    unload_support_ = GetParam();
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }

  void InstallUnloadHandlerOnSubFrame() {
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    EXPECT_TRUE(ExecJs(current_frame_host(), R"(
      const iframeElement = document.createElement("iframe");
      iframeElement.src = "%s";
      document.body.appendChild(iframeElement);
      )"));
    navigation_observer.Wait();
    RenderFrameHostImpl* subframe_render_frame_host =
        current_frame_host()->child_at(0)->current_frame_host();
    EXPECT_TRUE(
        ExecJs(subframe_render_frame_host, "window.onunload = () => 42;"));
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         BackForwardCacheUnloadStrategyBrowserTest,
                         testing::Values("always",
                                         "opt_in_header_required",
                                         "no"));

IN_PROC_BROWSER_TEST_P(BackForwardCacheUnloadStrategyBrowserTest,
                       UnloadHandlerPresentWithOptInHeader) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com",
                                            "/set-header?"
                                            "BFCache-Opt-In: unload"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(ExecJs(current_frame_host(), "window.onunload = () => 42;"));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Go back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  if (GetParam() == "always" || GetParam() == "opt_in_header_required") {
    ExpectRestored(FROM_HERE);
  } else {
    ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                           kUnloadHandlerExistsInMainFrame},
                      {}, {}, {}, {}, FROM_HERE);
  }

  // 4) Go forward.
  ASSERT_TRUE(HistoryGoForward(web_contents()));

  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_P(BackForwardCacheUnloadStrategyBrowserTest,
                       UnloadHandlerPresentWithoutOptInHeader) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/unload.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Go back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  if (GetParam() == "always") {
    ExpectRestored(FROM_HERE);
  } else if (GetParam() == "opt_in_header_required") {
    ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                           kOptInUnloadHeaderNotPresent},
                      {}, {}, {}, {}, FROM_HERE);
  } else {
    ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                           kUnloadHandlerExistsInMainFrame},
                      {}, {}, {}, {}, FROM_HERE);
  }

  // 4) Go forward.
  ASSERT_TRUE(HistoryGoForward(web_contents()));

  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_P(BackForwardCacheUnloadStrategyBrowserTest,
                       UnloadHandlerPresentInSubFrameWithOptInHeader) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com",
                                            "/set-header?"
                                            "BFCache-Opt-In: unload"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  InstallUnloadHandlerOnSubFrame();

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Go back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  if (GetParam() == "always" || GetParam() == "opt_in_header_required") {
    ExpectRestored(FROM_HERE);
  } else {
    ASSERT_EQ("no", GetParam());
    ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                           kUnloadHandlerExistsInSubFrame},
                      {}, {}, {}, {}, FROM_HERE);
  }

  // 4) Go forward.
  ASSERT_TRUE(HistoryGoForward(web_contents()));

  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_P(BackForwardCacheUnloadStrategyBrowserTest,
                       UnloadHandlerPresentInSubFrameWithoutOptInHeader) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  InstallUnloadHandlerOnSubFrame();

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Go back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  if (GetParam() == "always") {
    ExpectRestored(FROM_HERE);
  } else {
    ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                           kUnloadHandlerExistsInSubFrame},
                      {}, {}, {}, {}, FROM_HERE);
  }

  // 4) Go forward.
  ASSERT_TRUE(HistoryGoForward(web_contents()));

  ExpectRestored(FROM_HERE);
}

namespace {
enum class SubframeType { SameSite, CrossSite };
}

class BackForwardCacheEvictionDueToSubframeNavigationBrowserTest
    : public BackForwardCacheBrowserTest,
      public ::testing::WithParamInterface<SubframeType> {
 public:
  // Provides meaningful param names instead of /0 and /1.
  static std::string DescribeParams(
      const ::testing::TestParamInfo<ParamType>& info) {
    switch (info.param) {
      case SubframeType::SameSite:
        return "SameSite";
      case SubframeType::CrossSite:
        return "CrossSite";
    }
  }
};

IN_PROC_BROWSER_TEST_P(
    BackForwardCacheEvictionDueToSubframeNavigationBrowserTest,
    SubframePendingCommitShouldPreventCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  bool use_cross_origin_subframe = GetParam() == SubframeType::CrossSite;
  GURL subframe_url = embedded_test_server()->GetURL(
      use_cross_origin_subframe ? "b.com" : "a.com", "/title1.html");

  IsolateOriginsForTesting(embedded_test_server(), web_contents(),
                           {"a.com", "b.com"});

  // 1) Navigate to a.com.
  EXPECT_TRUE(NavigateToURL(shell(), a_url));
  RenderFrameHostImpl* main_frame = current_frame_host();

  // 2) Add subframe and wait for empty document to commit.
  CreateSubframe(web_contents(), "child", GURL(""), true);

  CommitMessageDelayer commit_message_delayer(
      web_contents(), subframe_url,
      base::BindLambdaForTesting([&](RenderFrameHost*) {
        // 5) Test that page cannot be stored in bfcache when subframe is
        // pending commit.
        BackForwardCacheCanStoreDocumentResult can_store_result =
            web_contents()
                ->GetController()
                .GetBackForwardCache()
                .CanStorePageNow(static_cast<RenderFrameHostImpl*>(main_frame));
        EXPECT_TRUE(can_store_result.HasNotStoredReason(
            BackForwardCacheMetrics::NotRestoredReason::kSubframeIsNavigating));
      }));

  // 3) Start navigation in subframe to |subframe_url|.
  EXPECT_TRUE(ExecJs(
      main_frame,
      JsReplace("document.querySelector('#child').src = $1;", subframe_url)));
  // 4) Wait until subframe navigation is pending commit.
  commit_message_delayer.Wait();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BackForwardCacheEvictionDueToSubframeNavigationBrowserTest,
    ::testing::Values(SubframeType::SameSite, SubframeType::CrossSite),
    &BackForwardCacheEvictionDueToSubframeNavigationBrowserTest::
        DescribeParams);

class BackForwardCacheFencedFrameBrowserTest
    : public BackForwardCacheBrowserTest {
 public:
  BackForwardCacheFencedFrameBrowserTest() = default;
  ~BackForwardCacheFencedFrameBrowserTest() override = default;
  BackForwardCacheFencedFrameBrowserTest(
      const BackForwardCacheFencedFrameBrowserTest&) = delete;

  BackForwardCacheFencedFrameBrowserTest& operator=(
      const BackForwardCacheFencedFrameBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
    fenced_frame_helper_ = std::make_unique<test::FencedFrameTestHelper>();
  }

  test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return *fenced_frame_helper_;
  }

 private:
  std::unique_ptr<test::FencedFrameTestHelper> fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(BackForwardCacheFencedFrameBrowserTest,
                       FencedFramePageNotStoredInBackForwardCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  // 2) Create a fenced frame.
  content::RenderFrameHostImpl* fenced_frame_host =
      static_cast<content::RenderFrameHostImpl*>(
          fenced_frame_test_helper().CreateFencedFrame(
              web_contents()->GetMainFrame(), url_b));
  RenderFrameHostWrapper fenced_frame_host_wrapper(fenced_frame_host);

  // 3) Navigate to C on the fenced frame host.
  fenced_frame_test_helper().NavigateFrameInFencedFrameTree(fenced_frame_host,
                                                            url_c);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  if (!fenced_frame_host_wrapper.IsRenderFrameDeleted())
    EXPECT_FALSE(fenced_frame_host->IsInBackForwardCache());
}

// A testing subclass that limits the cache size to 1 for ease of testing
// evictions.
class CacheSizeOneBackForwardCacheBrowserTest
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(features::kBackForwardCache, "cache_size",
                              base::NumberToString(1));
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(CacheSizeOneBackForwardCacheBrowserTest,
                       ReplacedNavigationEntry) {
  // Set the bfcache value to 1 to ensure that the test fails if a page
  // that replaces the current history entry is stored in back-forward cache.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.test", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.test", "/title1.html"));
  GURL url_c(embedded_test_server()->GetURL("c.test", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImplWrapper rfh_b(current_frame_host());
  EXPECT_FALSE(rfh_a.IsRenderFrameDeleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());

  // 3) Navigate to a new page by replacing the location. The old page can't
  // be navigated back to and we should not store it in the back-forward
  // cache.
  EXPECT_TRUE(
      ExecJs(shell(), JsReplace("window.location.replace($1);", url_c)));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImplWrapper rfh_c(current_frame_host());

  // 4) Confirm A is still in BackForwardCache and it wasn't evicted due to the
  // cache size limit, which would happen if we tried to store a new page in the
  // cache in the previous step.
  EXPECT_FALSE(rfh_a.IsRenderFrameDeleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 5) Confirm that navigating backwards goes back to A.
  ASSERT_TRUE(HistoryGoBack(shell()->web_contents()));
  EXPECT_EQ(rfh_a.get(), current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_EQ(rfh_a->GetVisibilityState(), PageVisibilityState::kVisible);

  // Go forward again, should return to C
  ASSERT_TRUE(HistoryGoForward(shell()->web_contents()));
  EXPECT_EQ(rfh_c.get(), current_frame_host());
  EXPECT_EQ(rfh_c->GetVisibilityState(), PageVisibilityState::kVisible);
}

// BEFORE ADDING A NEW TEST HERE
// Read the note at the top about the other files you could add it to.
}  // namespace content
