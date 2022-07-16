// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/back_forward_cache_browsertest.h"

#include <unordered_map>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/common/task_annotator.h"
#include "base/task/post_task.h"
#include "base/test/bind.h"
#include "base/test/test_mock_time_task_runner.h"
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
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/should_swap_browsing_instance.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/render_accessibility.mojom.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
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
#include "content/public/test/mock_web_contents_observer.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/text_input_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"
#include "content/test/did_commit_navigation_interceptor.h"
#include "content/test/echo.test-mojom.h"
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

class PageLifecycleStateManagerTestDelegate
    : public PageLifecycleStateManager::TestDelegate {
 public:
  explicit PageLifecycleStateManagerTestDelegate(
      PageLifecycleStateManager* manager)
      : manager_(manager) {
    manager->SetDelegateForTesting(this);
  }

  ~PageLifecycleStateManagerTestDelegate() override {
    if (manager_)
      manager_->SetDelegateForTesting(nullptr);
  }

  void WaitForInBackForwardCacheAck() {
    DCHECK(manager_);
    if (manager_->last_acknowledged_state().is_in_back_forward_cache) {
      return;
    }
    base::RunLoop loop;
    store_in_back_forward_cache_ack_received_ = loop.QuitClosure();
    loop.Run();
  }

  void OnStoreInBackForwardCacheSent(base::OnceClosure cb) {
    store_in_back_forward_cache_sent_ = std::move(cb);
  }

  void OnDisableJsEvictionSent(base::OnceClosure cb) {
    disable_eviction_sent_ = std::move(cb);
  }

  void OnRestoreFromBackForwardCacheSent(base::OnceClosure cb) {
    restore_from_back_forward_cache_sent_ = std::move(cb);
  }

 private:
  void OnLastAcknowledgedStateChanged(
      const blink::mojom::PageLifecycleState& old_state,
      const blink::mojom::PageLifecycleState& new_state) override {
    if (store_in_back_forward_cache_ack_received_ &&
        new_state.is_in_back_forward_cache)
      std::move(store_in_back_forward_cache_ack_received_).Run();
  }

  void OnUpdateSentToRenderer(
      const blink::mojom::PageLifecycleState& new_state) override {
    if (store_in_back_forward_cache_sent_ &&
        new_state.is_in_back_forward_cache) {
      std::move(store_in_back_forward_cache_sent_).Run();
    }

    if (disable_eviction_sent_ && new_state.eviction_enabled == false) {
      std::move(disable_eviction_sent_).Run();
    }

    if (restore_from_back_forward_cache_sent_ &&
        !new_state.is_in_back_forward_cache) {
      std::move(restore_from_back_forward_cache_sent_).Run();
    }
  }

  void OnDeleted() override { manager_ = nullptr; }

  PageLifecycleStateManager* manager_;
  base::OnceClosure store_in_back_forward_cache_sent_;
  base::OnceClosure store_in_back_forward_cache_ack_received_;
  base::OnceClosure restore_from_back_forward_cache_sent_;
  base::OnceClosure disable_eviction_sent_;
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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
  EXPECT_TRUE(ExecJs(shell(), JsReplace("location = $1;", url_b.spec())));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());

  ExpectRestored(FROM_HERE);

  // 4) Go forward to B.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(rfh_b, current_frame_host());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());

  ExpectRestored(FROM_HERE);

  // 5) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());

  ExpectRestored(FROM_HERE);

  // 6) Go forward to B.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

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
  ASSERT_TRUE(ExecJs(rfh_a.get(), JsReplace("location = $1;", url_b.spec())));
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
  RenderFrameHostImplWrapper rfh_b(current_frame_host());
  EXPECT_EQ(2u, rfh_b->GetSiteInstance()->GetRelatedActiveContentsCount());

  // 3) Go back to A. The previous document can't enter the BackForwardCache,
  // because of the popup.
  ASSERT_TRUE(ExecJs(rfh_b.get(), "history.back();"));
  ASSERT_TRUE(rfh_b.WaitUntilRenderFrameDeleted());

  // 4) Make the popup drop the window.opener connection. It happens when the
  //    user does an omnibox-initiated navigation, which happens in a new
  //    BrowsingInstance.
  RenderFrameHostImplWrapper rfh_a_new(current_frame_host());
  EXPECT_EQ(2u, rfh_a_new->GetSiteInstance()->GetRelatedActiveContentsCount());
  ASSERT_TRUE(NavigateToURL(popup, url_b));
  EXPECT_EQ(1u, rfh_a_new->GetSiteInstance()->GetRelatedActiveContentsCount());

  // 5) Navigate to B again. As the scripting relationship with the popup is
  // now severed, the current page (|rfh_a_new|) can enter back-forward cache.
  ASSERT_TRUE(
      ExecJs(rfh_a_new.get(), JsReplace("location = $1;", url_b.spec())));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_FALSE(delete_observer_rfh_c.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());
  EXPECT_TRUE(rfh_c->IsInBackForwardCache());

  ExpectRestored(FROM_HERE);
}

// Ensure flushing the BackForwardCache works properly.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, BackForwardCacheFlush) {
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
  EXPECT_FALSE(delete_observer_rfh_a.deleted());

  // 3) Flush A.
  web_contents()->GetController().GetBackForwardCache().Flush();
  delete_observer_rfh_a.WaitUntilDeleted();
  EXPECT_FALSE(delete_observer_rfh_b.deleted());

  // 4) Go back to a new A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_FALSE(delete_observer_rfh_b.deleted());

  // 5) Flush B.
  web_contents()->GetController().GetBackForwardCache().Flush();
  delete_observer_rfh_b.WaitUntilDeleted();
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(url_a, web_contents()->GetVisibleURL());

  // 4) Go forward to B.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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

  web_contents()->GetController().GoToOffset(-2);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_TRUE(observer3.has_committed());
  EXPECT_EQ("bar", observer3.GetNormalizedResponseHeader("x-foo"));

  ExpectRestored(FROM_HERE);
}

class HighCacheSizeBackForwardCacheBrowserTest
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(features::kBackForwardCache, "cache_size",
                              base::NumberToString(kBackForwardCacheSize));
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }

  // The number of pages the BackForwardCache can hold per tab.
  // The number 5 was picked since Android ASAN trybot failed to keep more than
  // 6 pages in memory.
  const size_t kBackForwardCacheSize = 5;
};

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

class BackgroundForegroundProcessLimitBackForwardCacheBrowserTest
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(features::kBackForwardCache, "cache_size",
                              base::NumberToString(kBackForwardCacheSize));
    EnableFeatureAndSetParams(
        features::kBackForwardCache, "foreground_cache_size",
        base::NumberToString(kForegroundBackForwardCacheSize));
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }

  void ExpectCached(const RenderFrameHostImplWrapper& rfh,
                    bool cached,
                    bool backgrounded) {
    EXPECT_FALSE(rfh.IsDestroyed());
    EXPECT_EQ(cached, rfh->IsInBackForwardCache());
    EXPECT_EQ(backgrounded, rfh->GetProcess()->IsProcessBackgrounded());
  }
  // The number of pages the BackForwardCache can hold per tab.
  const size_t kBackForwardCacheSize = 4;
  const size_t kForegroundBackForwardCacheSize = 2;
};

// Test that a series of same-site navigations (which use the same process)
// uses the foreground limit.
IN_PROC_BROWSER_TEST_F(
    BackgroundForegroundProcessLimitBackForwardCacheBrowserTest,
    CacheEvictionSameSite) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::vector<RenderFrameHostImplWrapper> rfhs;

  for (size_t i = 0; i <= kBackForwardCacheSize * 2; ++i) {
    SCOPED_TRACE(i);
    GURL url(embedded_test_server()->GetURL(
        "a.com", base::StringPrintf("/title1.html?i=%zu", i)));
    ASSERT_TRUE(NavigateToURL(shell(), url));
    rfhs.emplace_back(current_frame_host());
    EXPECT_FALSE(rfhs.back()->GetProcess()->IsProcessBackgrounded());

    for (size_t j = 0; j <= i; ++j) {
      SCOPED_TRACE(j);
      // The last page is active, the previous |kForegroundBackForwardCacheSize|
      // should be in the cache, any before that should be deleted.
      if (i - j <= kForegroundBackForwardCacheSize) {
        // All of the processes should be in the foreground.
        ExpectCached(rfhs[j], /*cached=*/i != j,
                     /*backgrounded=*/false);
      } else {
        ASSERT_TRUE(rfhs[j].WaitUntilRenderFrameDeleted());
      }
    }
  }

  // Navigate back but not to the initial about:blank.
  for (size_t i = 0; i <= kBackForwardCacheSize * 2 - 1; ++i) {
    SCOPED_TRACE(i);
    web_contents()->GetController().GoBack();
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
    // The first |kBackForwardCacheSize| navigations should be restored from the
    // cache. The rest should not.
    if (i < kForegroundBackForwardCacheSize) {
      ExpectRestored(FROM_HERE);
    } else {
      ExpectNotRestored(
          {BackForwardCacheMetrics::NotRestoredReason::kForegroundCacheLimit},
          {}, {}, {}, {}, FROM_HERE);
    }
  }
}

// Test that a series of cross-site navigations (which use different processes)
// use the background limit.
//
// TODO(crbug.com/1203418): This test is flaky.
IN_PROC_BROWSER_TEST_F(
    BackgroundForegroundProcessLimitBackForwardCacheBrowserTest,
    DISABLED_CacheEvictionCrossSite) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::vector<RenderFrameHostImplWrapper> rfhs;

  for (size_t i = 0; i <= kBackForwardCacheSize * 2; ++i) {
    SCOPED_TRACE(i);
    GURL url(embedded_test_server()->GetURL(base::StringPrintf("a%zu.com", i),
                                            "/title1.html"));
    ASSERT_TRUE(NavigateToURL(shell(), url));
    rfhs.emplace_back(current_frame_host());
    EXPECT_FALSE(rfhs.back()->GetProcess()->IsProcessBackgrounded());

    for (size_t j = 0; j <= i; ++j) {
      SCOPED_TRACE(j);
      // The last page is active, the previous |kBackgroundBackForwardCacheSize|
      // should be in the cache, any before that should be deleted.
      if (i - j <= kBackForwardCacheSize) {
        EXPECT_FALSE(rfhs[j].IsDestroyed());
        // Pages except the active one should be cached and in the background.
        ExpectCached(rfhs[j], /*cached=*/i != j,
                     /*backgrounded=*/i != j);
      } else {
        ASSERT_TRUE(rfhs[j].WaitUntilRenderFrameDeleted());
      }
    }
  }

  // Navigate back but not to the initial about:blank.
  for (size_t i = 0; i <= kBackForwardCacheSize * 2 - 1; ++i) {
    SCOPED_TRACE(i);
    web_contents()->GetController().GoBack();
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
    // The first |kBackForwardCacheSize| navigations should be restored from the
    // cache. The rest should not.
    if (i < kBackForwardCacheSize) {
      ExpectRestored(FROM_HERE);
    } else {
      ExpectNotRestored(
          {BackForwardCacheMetrics::NotRestoredReason::kCacheLimit}, {}, {}, {},
          {}, FROM_HERE);
    }
  }
}

// Test that the cache responds to processes switching from background to
// foreground. We set things up so that we have
// Cached sites:
//   a0.com
//   a1.com
//   a2.com
//   a3.com
// and the active page is a4.com. Then set the process for a[1-3] to
// foregrounded so that there are 3 entries whose processes are foregrounded.
// BFCache should evict the eldest (a1) leaving a0 because despite being older,
// it is backgrounded. Setting the priority directly is not ideal but there is
// no reliable way to cause the processes to go into the foreground just by
// navigating because proactive browsing instance swap makes it impossible to
// reliably create a new a1.com renderer in the same process as the old a1.com.
IN_PROC_BROWSER_TEST_F(
    BackgroundForegroundProcessLimitBackForwardCacheBrowserTest,
    ChangeToForeground) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::vector<RenderFrameHostImplWrapper> rfhs;

  // Navigate through a[0-3].com.
  for (size_t i = 0; i < kBackForwardCacheSize; ++i) {
    SCOPED_TRACE(i);
    GURL url(embedded_test_server()->GetURL(base::StringPrintf("a%zu.com", i),
                                            "/title1.html"));
    ASSERT_TRUE(NavigateToURL(shell(), url));
    rfhs.emplace_back(current_frame_host());
    EXPECT_FALSE(rfhs.back()->GetProcess()->IsProcessBackgrounded());
  }
  // Check that a0-2 are cached and backgrounded.
  for (size_t i = 0; i < kBackForwardCacheSize - 1; ++i) {
    SCOPED_TRACE(i);
    ExpectCached(rfhs[i], /*cached=*/true, /*backgrounded=*/true);
  }

  // Navigate to a page which causes the processes for a[1-3] to be
  // foregrounded.
  GURL url(embedded_test_server()->GetURL("a4.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url));

  // Assert that we really have set up the situation we want where the processes
  // are shared and in the foreground.
  RenderFrameHostImpl* rfh = current_frame_host();
  ASSERT_FALSE(rfh->GetProcess()->IsProcessBackgrounded());

  rfhs[1]->GetProcess()->SetPriorityOverride(
      /*foreground=*/true);
  rfhs[2]->GetProcess()->SetPriorityOverride(
      /*foreground=*/true);
  rfhs[3]->GetProcess()->SetPriorityOverride(
      /*foreground=*/true);

  // The page should be evicted.
  ASSERT_TRUE(rfhs[1].WaitUntilRenderFrameDeleted());

  // Check that a0 is cached and backgrounded.
  ExpectCached(rfhs[0], /*cached=*/true, /*backgrounded=*/true);
  // Check that a2-3 are cached and foregrounded.
  ExpectCached(rfhs[2], /*cached=*/true, /*backgrounded=*/false);
  ExpectCached(rfhs[3], /*cached=*/true, /*backgrounded=*/false);
}

// Tests that |RenderFrameHost::ForEachRenderFrameHost| and
// |WebContents::ForEachRenderFrameHost| behave correctly with bfcached
// RenderFrameHosts.
#if defined(OS_MAC)
// Flaky: https://crbug.com/1263536
#define MAYBE_ForEachRenderFrameHost DISABLED_ForEachRenderFrameHost
#else
#define MAYBE_ForEachRenderFrameHost ForEachRenderFrameHost
#endif
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       MAYBE_ForEachRenderFrameHost) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c),d)"));
  GURL url_e(embedded_test_server()->GetURL("e.com", "/title1.html"));

  std::vector<RenderFrameDeletedObserver*> rfh_observers;

  // 1) Navigate to a(b(c),d).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_c = rfh_b->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_d = rfh_a->child_at(1)->current_frame_host();
  RenderFrameDeletedObserver a_observer(rfh_a), b_observer(rfh_b),
      c_observer(rfh_c), d_observer(rfh_d);
  rfh_observers.insert(rfh_observers.end(),
                       {&a_observer, &b_observer, &c_observer, &d_observer});

  // Ensure the visited frames are what we would expect for the page before
  // entering bfcache.
  EXPECT_THAT(CollectAllRenderFrameHosts(rfh_a),
              testing::ElementsAre(rfh_a, rfh_b, rfh_d, rfh_c));
  EXPECT_THAT(CollectAllRenderFrameHosts(web_contents()),
              testing::ElementsAre(rfh_a, rfh_b, rfh_d, rfh_c));

  // 2) Navigate to e.
  EXPECT_TRUE(NavigateToURL(shell(), url_e));
  RenderFrameHostImpl* rfh_e = current_frame_host();
  RenderFrameDeletedObserver e_observer(rfh_e);
  rfh_observers.push_back(&e_observer);
  ASSERT_THAT(rfh_observers, Each(Not(Deleted())));
  EXPECT_THAT(Elements({rfh_a, rfh_b, rfh_c, rfh_d}),
              Each(InBackForwardCache()));
  EXPECT_THAT(rfh_e, Not(InBackForwardCache()));

  // When starting iteration from the primary frame, we shouldn't see any of the
  // frames in bfcache.
  EXPECT_THAT(CollectAllRenderFrameHosts(rfh_e), testing::ElementsAre(rfh_e));

  // When starting iteration from a bfcached RFH, we should see the frame itself
  // and its descendants in breadth first order.
  EXPECT_THAT(CollectAllRenderFrameHosts(rfh_a),
              testing::ElementsAre(rfh_a, rfh_b, rfh_d, rfh_c));

  // Ensure that starting iteration from a subframe of a bfcached frame also
  // works.
  EXPECT_THAT(CollectAllRenderFrameHosts(rfh_b),
              testing::ElementsAre(rfh_b, rfh_c));

  // When iterating over all RenderFrameHosts in a WebContents, we should see
  // the RFHs of both the primary page and the bfcached page.
  EXPECT_THAT(CollectAllRenderFrameHosts(web_contents()),
              testing::UnorderedElementsAre(rfh_a, rfh_b, rfh_c, rfh_d, rfh_e));

  {
    // If we stop iteration in |WebContents::ForEachRenderFrameHost|, we stop
    // the entire iteration, not just iteration in the page being iterated at
    // that point. In this case, if we stop iteration in the primary page, we do
    // not continue to iterate in the bfcached page.
    bool stopped = false;
    web_contents()->ForEachRenderFrameHost(
        base::BindLambdaForTesting([&](RenderFrameHostImpl* rfh) {
          EXPECT_FALSE(stopped);
          stopped = true;
          return RenderFrameHost::FrameIterationAction::kStop;
        }));
  }

  EXPECT_EQ(nullptr, rfh_a->GetParentOrOuterDocument());
  EXPECT_EQ(rfh_a, rfh_b->GetParentOrOuterDocument());
  EXPECT_EQ(rfh_b, rfh_c->GetParentOrOuterDocument());
  EXPECT_EQ(rfh_a, rfh_d->GetParentOrOuterDocument());
  EXPECT_EQ(nullptr, rfh_e->GetParentOrOuterDocument());
  // The outermost document of a bfcached page is the bfcached main
  // RenderFrameHost, not the primary main RenderFrameHost.
  EXPECT_EQ(rfh_a, rfh_a->GetOutermostMainFrame());
  EXPECT_EQ(rfh_a, rfh_b->GetOutermostMainFrame());
  EXPECT_EQ(rfh_a, rfh_c->GetOutermostMainFrame());
  EXPECT_EQ(rfh_a, rfh_d->GetOutermostMainFrame());
  EXPECT_EQ(rfh_e, rfh_e->GetOutermostMainFrame());
  EXPECT_EQ(nullptr, rfh_a->GetParentOrOuterDocumentOrEmbedder());
  EXPECT_EQ(rfh_a, rfh_b->GetParentOrOuterDocumentOrEmbedder());
  EXPECT_EQ(rfh_b, rfh_c->GetParentOrOuterDocumentOrEmbedder());
  EXPECT_EQ(rfh_a, rfh_d->GetParentOrOuterDocumentOrEmbedder());
  EXPECT_EQ(nullptr, rfh_e->GetParentOrOuterDocumentOrEmbedder());
  EXPECT_EQ(rfh_a, rfh_a->GetOutermostMainFrameOrEmbedder());
  EXPECT_EQ(rfh_a, rfh_b->GetOutermostMainFrameOrEmbedder());
  EXPECT_EQ(rfh_a, rfh_c->GetOutermostMainFrameOrEmbedder());
  EXPECT_EQ(rfh_a, rfh_d->GetOutermostMainFrameOrEmbedder());
  EXPECT_EQ(rfh_e, rfh_e->GetOutermostMainFrameOrEmbedder());
}

// Tests that |RenderFrameHostImpl::ForEachRenderFrameHostIncludingSpeculative|
// and |WebContentsImpl::ForEachRenderFrameHostIncludingSpeculative|
// behave correctly when a FrameTreeNode has both a speculative RFH and a
// bfcached RFH.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       ForEachRenderFrameHostWithSpeculative) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  std::vector<RenderFrameDeletedObserver*> rfh_observers;

  // 1) Navigate to a.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver a_observer(rfh_a);
  rfh_observers.push_back(&a_observer);

  // 2) Navigate to b.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver b_observer(rfh_b);
  rfh_observers.push_back(&b_observer);
  ASSERT_THAT(rfh_observers, Each(Not(Deleted())));

  // 3) Begin navigation to c.
  TestNavigationManager nav_manager(web_contents(), url_c);
  shell()->LoadURL(url_c);
  ASSERT_TRUE(nav_manager.WaitForRequestStart());

  RenderFrameHostImpl* rfh_c =
      rfh_b->frame_tree_node()->render_manager()->speculative_frame_host();
  ASSERT_TRUE(rfh_c);
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache,
            rfh_a->lifecycle_state());
  EXPECT_FALSE(rfh_a->GetPage().IsPrimary());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_b->lifecycle_state());
  EXPECT_TRUE(rfh_b->GetPage().IsPrimary());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kSpeculative,
            rfh_c->lifecycle_state());
  EXPECT_FALSE(rfh_c->GetPage().IsPrimary());

  // When starting iteration from the bfcached RFH, we should not see the
  // speculative RFH.
  EXPECT_THAT(CollectAllRenderFrameHostsIncludingSpeculative(rfh_a),
              testing::ElementsAre(rfh_a));

  // When starting iteration from the primary frame, we shouldn't see the
  // bfcached RFH, but we should see the speculative RFH.
  EXPECT_THAT(CollectAllRenderFrameHostsIncludingSpeculative(rfh_b),
              testing::UnorderedElementsAre(rfh_b, rfh_c));

  // When starting iteration from the speculative RFH, we should only see
  // the speculative RFH. In particular, we should not see the bfcached RFH.
  EXPECT_THAT(CollectAllRenderFrameHostsIncludingSpeculative(rfh_c),
              testing::ElementsAre(rfh_c));

  // When iterating over all RenderFrameHosts in a WebContents, we should see
  // the RFHs of both the primary page and the bfcached page.
  EXPECT_THAT(CollectAllRenderFrameHostsIncludingSpeculative(web_contents()),
              testing::UnorderedElementsAre(rfh_a, rfh_b, rfh_c));
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ASSERT_THAT(rfh_observer, Each(Not(Deleted())));
  EXPECT_EQ(a1, current_frame_host());
  EXPECT_THAT(Elements({a1, b2}), Each(Not(InBackForwardCache())));
  EXPECT_THAT(Elements({b3, a4}), Each(InBackForwardCache()));

  // Even after a new IPC round trip with the renderer, b2 must still be alive.
  EXPECT_EQ("I am alive", EvalJs(b2, "window.alive"));
  EXPECT_FALSE(b2_observer.deleted());

  ExpectRestored(FROM_HERE);

  // 4) Go forward to b3(a4).
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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
  web_contents()->GetController().GoToOffset(-3);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(a1, current_frame_host());
  ASSERT_THAT(rfh_observer, Each(Not(Deleted())));
  EXPECT_THAT(Elements({b3, a4, b5}), Each(InBackForwardCache()));
  EXPECT_THAT(Elements({a1, b2}), Each(Not(InBackForwardCache())));

  // Even after a new IPC round trip with the renderer, b2 must still be alive.
  EXPECT_EQ("I am alive", EvalJs(b2, "window.alive"));
  EXPECT_FALSE(b2_observer.deleted());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DISABLED_NavigationsAreFullyCommitted) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // During a navigation, the document being navigated *away from* can either be
  // deleted or stored into the BackForwardCache. The document being navigated
  // *to* can either be new or restored from the BackForwardCache.
  //
  // This test covers every combination:
  //
  //  1. Navigate to a cacheable page (()->A)
  //  2. Navigate to an uncacheable page (A->B)
  //  3. Go Back to a cached page (B->A)
  //  4. Navigate to a cacheable page (A->C)
  //  5. Go Back to a cached page (C->A)
  //
  // +-+-------+----------------+---------------+
  // |#|nav    | curr_document  | dest_document |
  // +-+-------+----------------+---------------|
  // |1|(()->A)| N/A            | new           |
  // |2|(A->B) | cached         | new           |
  // |3|(B->A) | deleted        | restored      |
  // |4|(A->C) | cached         | new           |
  // |5|(C->A) | cached         | restored      |
  // +-+-------+----------------+---------------+
  //
  // As part of these navigations we check that LastCommittedURL was updated,
  // to verify that the frame wasn't simply swapped in without actually
  // committing.

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.com", "/back_forward_cache/page_with_dedicated_worker.html"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1. Navigate to a cacheable page (A).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2. Navigate from a cacheable page to an uncacheable page (A->B).
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url_b);
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // Page A should be in the cache.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3. Navigate from an uncacheable to a cached page page (B->A).
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url_a);

  // Page B should be deleted (not cached).
  delete_observer_rfh_b.WaitUntilDeleted();

  ExpectRestored(FROM_HERE);

  // 4. Navigate from a cacheable page to a cacheable page (A->C).
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url_c);
  RenderFrameHostImpl* rfh_c = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_c(rfh_c);

  // Page A should be in the cache.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 5. Navigate from a cacheable page to a cached page (C->A).
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url_a);

  // Page C should be in the cache.
  EXPECT_FALSE(delete_observer_rfh_c.deleted());
  EXPECT_TRUE(rfh_c->IsInBackForwardCache());

  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DISABLED_ProxiesAreStoredAndRestored) {
  // This test makes assumption about where iframe processes live.
  if (!AreAllSitesIsolatedForTesting())
    return;

  ASSERT_TRUE(embedded_test_server()->Start());

  // During a navigation, the document being navigated *away from* can either be
  // deleted or stored into the BackForwardCache. The document being navigated
  // *to* can either be new or restored from the BackForwardCache.
  //
  // This test covers every combination:
  //
  //  1. Navigate to a cacheable page (()->A)
  //  2. Navigate to an uncacheable page (A->B)
  //  3. Go Back to a cached page (B->A)
  //  4. Navigate to a cacheable page (A->C)
  //  5. Go Back to a cached page (C->A)
  //
  // +-+-------+----------------+---------------+
  // |#|nav    | curr_document  | dest_document |
  // +-+-------+----------------+---------------|
  // |1|(()->A)| N/A            | new           |
  // |2|(A->B) | cached         | new           |
  // |3|(B->A) | deleted        | restored      |
  // |4|(A->C) | cached         | new           |
  // |5|(C->A) | cached         | restored      |
  // +-+-------+----------------+---------------+
  //
  // We use pages with cross process iframes to verify that proxy storage and
  // retrieval works well in every possible combination.

  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(i,j)"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.com", "/back_forward_cache/page_with_dedicated_worker.html"));
  GURL url_c(embedded_test_server()->GetURL(
      "c.com", "/cross_site_iframe_factory.html?c(k,l,m)"));

  NavigationControllerImpl& controller = web_contents()->GetController();
  BackForwardCacheImpl& cache = controller.GetBackForwardCache();

  // 1. Navigate to a cacheable page (A).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_EQ(2u, render_frame_host_manager()->GetProxyCount());
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  std::string frame_tree_a = DepictFrameTree(rfh_a->frame_tree_node());

  // 2. Navigate from a cacheable page to an uncacheable page (A->B).
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_EQ(0u, render_frame_host_manager()->GetProxyCount());
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // Page A should be in the cache.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Verify proxies are stored as well.
  auto* cached_entry = cache.GetEntry(rfh_a->nav_entry_id());
  EXPECT_EQ(2u, cached_entry->proxy_hosts_size());

  // 3. Navigate from an uncacheable to a cached page page (B->A).
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  // Note: Since we put the page B into BackForwardCache briefly, we do not
  // create a transition proxy. So there should be only proxies for i.com and
  // j.com.
  EXPECT_EQ(2u, render_frame_host_manager()->GetProxyCount());

  // Page B should be deleted (not cached).
  delete_observer_rfh_b.WaitUntilDeleted();
  EXPECT_EQ(2u, render_frame_host_manager()->GetProxyCount());

  // Page A should still have the correct frame tree.
  EXPECT_EQ(frame_tree_a,
            DepictFrameTree(current_frame_host()->frame_tree_node()));

  // 4. Navigate from a cacheable page to a cacheable page (A->C).
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  EXPECT_EQ(3u, render_frame_host_manager()->GetProxyCount());
  RenderFrameHostImpl* rfh_c = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_c(rfh_c);

  // Page A should be in the cache.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Verify proxies are stored as well.
  cached_entry = cache.GetEntry(rfh_a->nav_entry_id());
  EXPECT_EQ(2u, cached_entry->proxy_hosts_size());

  // 5. Navigate from a cacheable page to a cached page (C->A).
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(2u, render_frame_host_manager()->GetProxyCount());

  // Page A should still have the correct frame tree.
  EXPECT_EQ(frame_tree_a,
            DepictFrameTree(current_frame_host()->frame_tree_node()));

  // Page C should be in the cache.
  EXPECT_FALSE(delete_observer_rfh_c.deleted());
  EXPECT_TRUE(rfh_c->IsInBackForwardCache());

  // Verify proxies are stored as well.
  cached_entry = cache.GetEntry(rfh_c->nav_entry_id());
  EXPECT_EQ(3u, cached_entry->proxy_hosts_size());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       RestoredProxiesAreFunctional) {
  // This test makes assumption about where iframe processes live.
  if (!AreAllSitesIsolatedForTesting())
    return;

  ASSERT_TRUE(embedded_test_server()->Start());

  // Page A is cacheable, while page B is not.
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(z)"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title2.html"));

  NavigationControllerImpl& controller = web_contents()->GetController();

  // 1. Navigate to a cacheable page (A).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2. Navigate from a cacheable page to an uncacheable page (A->B).
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  DisableBFCacheForRFHForTesting(rfh_b);

  // 3. Navigate from an uncacheable to a cached page page (B->A).
  // This restores the top frame's proxy in the z.com (iframe's) process.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // 4. Verify that the main frame's z.com proxy is still functional.
  RenderFrameHostImpl* iframe =
      rfh_a->frame_tree_node()->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe, "top.location.href = '" + url_c.spec() + "';"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // We expect to have navigated through the proxy.
  EXPECT_EQ(url_c, controller.GetLastCommittedEntry()->GetURL());
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kHTTPStatusNotOK}, {}, {},
      {}, {}, FROM_HERE);
}

// Flaky on Android, see crbug.com/1135601 and on other platforms, see
// crbug.com/1128772.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DISABLED_LogIpcPostedToCachedFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2) Navigate away. The first page should be in the cache.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  // 3) Post IPC tasks to the page, testing both mojo remote and associated
  // remote objects.

  // Send a message via an associated interface - which will post a task with an
  // IPC hash and will be routed to the per-thread task queue.
  base::RunLoop run_loop;
  rfh_a->RequestTextSurroundingSelection(
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, const std::u16string& str,
             uint32_t num, uint32_t num2) { quit_closure.Run(); },
          run_loop.QuitClosure()),
      1);
  run_loop.Run();

  // Post a non-associated interface. Will be routed to a frame-specific task
  // queue with IPC set in SimpleWatcher.
  base::RunLoop run_loop2;
  rfh_a->GetHighPriorityLocalFrame()->DispatchBeforeUnload(
      false,
      base::BindOnce([](base::RepeatingClosure quit_closure, bool proceed,
                        base::TimeTicks start_time,
                        base::TimeTicks end_time) { quit_closure.Run(); },
                     run_loop2.QuitClosure()));
  run_loop2.Run();

  // 4) Check the histogram.
  std::vector<base::HistogramBase::Sample> samples = {
      base::HistogramBase::Sample(
          base::TaskAnnotator::ScopedSetIpcHash::MD5HashMetricName(
              "blink.mojom.HighPriorityLocalFrame")),
      base::HistogramBase::Sample(
          base::TaskAnnotator::ScopedSetIpcHash::MD5HashMetricName(
              "blink.mojom.LocalFrame"))};

  for (base::HistogramBase::Sample sample : samples) {
    FetchHistogramsFromChildProcesses();
    EXPECT_TRUE(HistogramContainsIntValue(
        sample, histogram_tester_.GetAllSamples(
                    "BackForwardCache.Experimental."
                    "UnexpectedIPCMessagePostedToCachedFrame.MethodHash")));
  }
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

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DisableBackforwardCacheForTesting) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Disable the BackForwardCache.
  web_contents()->GetController().GetBackForwardCache().DisableForTesting(
      BackForwardCacheImpl::TEST_ASSUMES_NO_CACHING);

  // Navigate to a page that would normally be cacheable.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  RenderFrameDeletedObserver delete_observer_rfh_a(current_frame_host());

  // Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  // The page should be deleted (not cached).
  delete_observer_rfh_a.WaitUntilDeleted();
}

// Navigate from A to B, then cause JavaScript execution on A, then go back.
// Test the RenderFrameHost in the cache is evicted by JavaScript.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       EvictionOnJavaScriptExecution) {
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

  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());

  // 3) Execute JavaScript on A.
  EvictByJavaScript(rfh_a);

  // RenderFrameHost A is evicted from the BackForwardCache:
  delete_observer_rfh_a.WaitUntilDeleted();

  // 4) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kJavaScriptExecution}, {},
      {}, {}, {}, FROM_HERE);
}

// Similar to BackForwardCacheBrowserTest.EvictionOnJavaScriptExecution.
// Test case: A(B) -> C -> JS on B -> A(B)
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       EvictionOnJavaScriptExecutionIframe) {
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
  EXPECT_FALSE(delete_observer_rfh_c.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_FALSE(rfh_c->IsInBackForwardCache());

  // 3) Execute JavaScript on B.
  //
  EvictByJavaScript(rfh_b);

  // The A(B) page is evicted. So A and B are removed:
  delete_observer_rfh_a.WaitUntilDeleted();
  delete_observer_rfh_b.WaitUntilDeleted();

  // 4) Go back to A(B).
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kJavaScriptExecution}, {},
      {}, {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       EvictionOnJavaScriptExecutionInAnotherWorld) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Execute JavaScript on A in a new world. This ensures a new world.
  const int32_t kNewWorldId = content::ISOLATED_WORLD_ID_CONTENT_END + 1;
  EXPECT_TRUE(ExecJs(rfh_a, "console.log('hi');",
                     EXECUTE_SCRIPT_DEFAULT_OPTIONS, kNewWorldId));

  // 3) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());

  // 4) Execute JavaScript on A in the new world.
  EXPECT_FALSE(ExecJs(rfh_a, "console.log('hi');",
                      EXECUTE_SCRIPT_DEFAULT_OPTIONS, kNewWorldId));

  // RenderFrameHost A is evicted from the BackForwardCache:
  delete_observer_rfh_a.WaitUntilDeleted();

  // 5) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kJavaScriptExecution}, {},
      {}, {}, {}, FROM_HERE);
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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
  EXPECT_TRUE(ExecJs(shell(), JsReplace("location = $1;", url_b.spec())));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());
  // TODO(yuzus): Post message to the frozen page, and make sure that the
  // messages arrive after the page visibility events, not before them.

  // 3) Go back to A. Confirm that expected events are fired.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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

// Navigates from page A -> page B -> page C -> page B -> page C. Page B becomes
// ineligible for bfcache in pagehide handler, so Page A stays in bfcache
// without being evicted even after the navigation to Page C.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    PagehideMakesPageIneligibleForBackForwardCacheAndNotCountedInCacheSize) {
  ASSERT_TRUE(CreateHttpsServer()->Start());
  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(https_server()->GetURL(
      "b.com", "/back_forward_cache/page_with_broadcastchannel.html"));
  GURL url_c(https_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to a.com.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2) Navigate to b.com.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver deleted_observer_rfh_b(rfh_b);
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  // Acquire broadcast in pagehide. Now b.com is not eligible for bfcache.
  EXPECT_TRUE(
      ExecJs(rfh_b, "setShouldAcquireBroadcastChannelInPageHide(true);"));

  // 3) Navigate to c.com.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  // RenderFrameHostImpl* rfh_c = current_frame_host();
  // Since the b.com is not eligible for bfcache, |rfh_a| should stay in
  // bfcache.
  deleted_observer_rfh_b.WaitUntilDeleted();
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 4) Navigate back to b.com.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::kBroadcastChannel}, {}, {},
      {}, FROM_HERE);
  RenderFrameHostImpl* rfh_b_2 = current_frame_host();
  // Do not acquire broadcast channel. Now b.com is eligible for bfcache.
  EXPECT_TRUE(
      ExecJs(rfh_b_2, "setShouldAcquireBroadcastChannelInPageHide(false);"));

  // 5) Navigate forward to c.com.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectRestored(FROM_HERE);
  // b.com was eligible for bfcache and should stay in bfcache.
  EXPECT_TRUE(rfh_b_2->IsInBackForwardCache());
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

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, EvictPageWithInfiniteLoop) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  ExecuteScriptAsync(rfh_a, R"(
    let i = 0;
    while (true) { i++; }
  )");

  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  RenderProcessHost* process = rfh_a->GetProcess();
  RenderProcessHostWatcher destruction_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // rfh_a should be destroyed (not kept in the cache).
  destruction_observer.Wait();
  delete_observer_rfh_a.WaitUntilDeleted();

  // rfh_b should still be the current frame.
  EXPECT_EQ(current_frame_host(), rfh_b);
  EXPECT_FALSE(delete_observer_rfh_b.deleted());

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kTimeoutPuttingInCache}, {},
      {}, {}, {}, FROM_HERE);
}

// Test the race condition where a document is evicted from the BackForwardCache
// while it is in the middle of being restored and before URL loader starts a
// response.
//
// ┌───────┐                 ┌────────┐
// │Browser│                 │Renderer│
// └───┬───┘                 └───┬────┘
// (Freeze & store the cache)    │
//     │────────────────────────>│
//     │                         │
// (Navigate to cached document) │
//     │──┐                      │
//     │  │                      │
//     │EvictFromBackForwardCache│
//     │<────────────────────────│
//     │  │                      │
//     │  x Navigation cancelled │
//     │    and reissued         │
// ┌───┴───┐                 ┌───┴────┐
// │Browser│                 │Renderer│
// └───────┘                 └────────┘
//
// When the eviction occurs, the in flight NavigationRequest to the cached
// document should be reissued (cancelled and replaced by a normal navigation).
//
// Flaky on most platforms (see crbug.com/1136683)
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    DISABLED_ReissuesNavigationIfEvictedDuringNavigation_BeforeResponse) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to page A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to page B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_NE(rfh_a, rfh_b);

  // 3) Start navigation to page A, and cause the document to be evicted during
  // the navigation immediately before navigation makes any meaningful progress.
  web_contents()->GetController().GoBack();
  EvictByJavaScript(rfh_a);

  // rfh_a should have been deleted, and page A navigated to normally.
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  delete_observer_rfh_a.WaitUntilDeleted();
  RenderFrameHostImpl* rfh_a2 = current_frame_host();
  EXPECT_NE(rfh_a2, rfh_b);
  EXPECT_EQ(rfh_a2->GetLastCommittedURL(), url_a);

  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kJavaScriptExecution}, {},
      {}, {}, {}, FROM_HERE);
}

// Similar to ReissuesNavigationIfEvictedDuringNavigation, except that
// BackForwardCache::Flush is the source of the eviction.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       FlushCacheDuringNavigationToCachedPage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to page A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a1 = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a1(rfh_a1);

  // 2) Navigate to page B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b2 = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b2(rfh_b2);
  EXPECT_FALSE(delete_observer_rfh_a1.deleted());
  EXPECT_TRUE(rfh_a1->IsInBackForwardCache());
  EXPECT_NE(rfh_a1, rfh_b2);

  // 3) Start navigation to page A, and flush the cache during the navigation.
  TestNavigationManager navigation_manager(shell()->web_contents(), url_a);
  web_contents()->GetController().GoBack();

  EXPECT_TRUE(navigation_manager.WaitForResponse());

  // Flush the cache, which contains the document being navigated to.
  web_contents()->GetController().GetBackForwardCache().Flush();

  // The navigation should get canceled, then reissued; ultimately resulting in
  // a successful navigation using a new RenderFrameHost.
  navigation_manager.WaitForNavigationFinished();

  // rfh_a should have been deleted, and page A navigated to normally.
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  delete_observer_rfh_a1.WaitUntilDeleted();
  EXPECT_TRUE(rfh_b2->IsInBackForwardCache());
  RenderFrameHostImpl* rfh_a3 = current_frame_host();
  EXPECT_EQ(rfh_a3->GetLastCommittedURL(), url_a);
}

// Test that if the renderer process crashes while a document is in the
// BackForwardCache, it gets evicted.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       EvictsFromCacheIfRendererProcessCrashes) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();

  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Crash A's renderer process while it is in the cache.
  {
    RenderProcessHost* process = rfh_a->GetProcess();
    RenderProcessHostWatcher crash_observer(
        process, RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
    EXPECT_TRUE(process->Shutdown(0));
    crash_observer.Wait();
  }

  // rfh_b should still be the current frame.
  EXPECT_EQ(current_frame_host(), rfh_b);

  // 4) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kRendererProcessKilled}, {},
      {}, {}, {}, FROM_HERE);
}

// The test is simulating a race condition. The scheduler tracked features are
// updated during the "freeze" event in a way that would have prevented the
// document from entering the BackForwardCache in the first place.
//
// TODO(https://crbug.com/996267): The document should be evicted.
//
// ┌───────┐                     ┌────────┐
// │browser│                     │renderer│
// └───┬───┘                     └────┬───┘
//  (enter cache)                     │
//     │           Freeze()           │
//     │─────────────────────────────>│
//     │                          (onfreeze)
//     │OnSchedulerTrackedFeaturesUsed│
//     │<─────────────────────────────│
//     │                           (frozen)
//     │                              │
// ┌───┴───┐                     ┌────┴───┐
// │browser│                     │renderer│
// └───────┘                     └────────┘
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       SchedulerTrackedFeaturesUpdatedWhileStoring) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // When the page will enter the BackForwardCache, just before being frozen,
  // use a feature that would have been prevented the document from being
  // cached.
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    document.addEventListener('freeze', event => {
      window.foo = new BroadcastChannel('foo');
    });
  )"));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // rfh_a should be evicted from the cache and destroyed.
  delete_observer_rfh_a.WaitUntilDeleted();
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
  web_contents()->GetController().GoToIndex(0);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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
  web_contents()->GetController().GoToIndex(0);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // D3 takes A2(B(C))'s place in the cache.
  EXPECT_TRUE(rfh_d3->IsInBackForwardCache());
  delete_rfh_a2.WaitUntilDeleted();
}

class BackForwardCacheBrowserTestWithSameSiteDisabled
    : public BackForwardCacheBrowserTest {
 public:
  BackForwardCacheBrowserTestWithSameSiteDisabled() = default;
  ~BackForwardCacheBrowserTestWithSameSiteDisabled() override = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    same_site_back_forward_cache_enabled_ = false;
    DisableFeature(features::kProactivelySwapBrowsingInstance);
    // Ensure that the bot flags won't override the same-site back/forward cache
    // disabling.
    DisableFeature(features::kBackForwardCacheSameSiteForBots);
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithSameSiteDisabled,
                       ConflictingBrowsingInstances) {
  // This test assumes navigation from A1 to A2 will not switch
  // BrowsingInstances, which is not true when either BackForwardCache or
  // ProactivelySwapBrowsingInstance is enabled on same-site navigations.
  DCHECK(!CanSameSiteMainFrameNavigationsChangeSiteInstances());
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
  // Make B3 ineligible for caching, so that navigating doesn't evict A2
  // due to the cache size limit.
  DisableBFCacheForRFHForTesting(rfh_b3);

  // 4) Do a history navigation back to A1.  At this point, A1 is going to have
  // the same BrowsingInstance as A2. This should cause A2 to get
  // evicted from the BackForwardCache due to its conflicting BrowsingInstance.
  web_contents()->GetController().GoToIndex(0);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(current_frame_host()->GetLastCommittedURL(), url_a1);
  delete_rfh_a2.WaitUntilDeleted();

  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBrowsingInstanceNotSwapped},
      {}, {ShouldSwapBrowsingInstance::kNo_SameSiteNavigation}, {}, {},
      FROM_HERE);

  // 5) Go to A2.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ExpectNotRestored(
      {
          BackForwardCacheMetrics::NotRestoredReason::
              kConflictingBrowsingInstance,
      },
      {}, {}, {}, {}, FROM_HERE);
}

// When same-site bfcache is disabled, we should not cache on same-site
// navigations.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithSameSiteDisabled,
                       DoesNotCacheOnSameSiteNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_a3(
      embedded_test_server()->GetURL("subdomain.a.com", "/title3.html"));

  // 1) Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));
  RenderFrameHostImpl* rfh_a1 = current_frame_host();
  RenderFrameDeletedObserver delete_rfh_a1(rfh_a1);
  auto browsing_instance_id =
      rfh_a1->GetSiteInstance()->GetBrowsingInstanceId();

  // 2) Navigate same-site and same-origin to A2.
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));
  RenderFrameHostImpl* rfh_a2 = current_frame_host();
  // The BrowsingInstance shouldn't have changed.
  EXPECT_EQ(browsing_instance_id,
            rfh_a2->GetSiteInstance()->GetBrowsingInstanceId());
  // The previous page should not be cached.
  EXPECT_FALSE(rfh_a1->IsInBackForwardCache());

  // 2) Navigate same-site but cross-origin to A3.
  EXPECT_TRUE(NavigateToURL(shell(), url_a3));
  RenderFrameHostImpl* rfh_a3 = current_frame_host();
  // The BrowsingInstance shouldn't have changed.
  EXPECT_EQ(browsing_instance_id,
            rfh_a3->GetSiteInstance()->GetBrowsingInstanceId());
  // The previous page should not be cached.
  EXPECT_FALSE(rfh_a2->IsInBackForwardCache());
}

// Check that during a same-RenderFrameHost cross-document navigation, the
// disabled reasons is still tracked.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithSameSiteDisabled,
                       DisableForRenderFrameHostPersistsAcrossNavigations) {
  // This test assumes navigation from A1 to A2 will not switch
  // RenderFrameHosts which is not true when BackForwardCache,
  // ProactivelySwapBrowsingInstance or RenderDocument is enabled on same-site
  // main frame navigations.
  DCHECK(!CanSameSiteMainFrameNavigationsChangeRenderFrameHosts());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_b3(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));
  RenderFrameHostImpl* rfh_a1 = current_frame_host();
  RenderFrameDeletedObserver deleted_observer_rfh_a1(rfh_a1);
  // Disable back-forward cache for A.
  DisableBFCacheForRFHForTesting(rfh_a1);

  // 2) Navigate to A2.
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));
  EXPECT_FALSE(deleted_observer_rfh_a1.deleted());
  EXPECT_EQ(rfh_a1, current_frame_host());

  // 3) Navigate to B3.
  EXPECT_TRUE(NavigateToURL(shell(), url_b3));
  deleted_observer_rfh_a1.WaitUntilDeleted();

  // 4) Go back to A2.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    {}, {}, {RenderFrameHostDisabledForTestingReason()}, {},
                    FROM_HERE);
}

// The BackForwardCache caches same-website navigations.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, SameSiteNavigationCaching) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title2.html"));

  // 1) Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));
  RenderFrameHostImpl* rfh_a1 = current_frame_host();
  RenderFrameDeletedObserver delete_rfh_a1(rfh_a1);
  auto browsing_instance_id =
      rfh_a1->GetSiteInstance()->GetBrowsingInstanceId();

  // 2) Navigate to A2.
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));
  RenderFrameHostImpl* rfh_a2 = current_frame_host();
  EXPECT_NE(browsing_instance_id,
            rfh_a2->GetSiteInstance()->GetBrowsingInstanceId());
  EXPECT_TRUE(rfh_a1->IsInBackForwardCache());
  EXPECT_NE(rfh_a1, rfh_a2);
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_TRUE(rfh_a1->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b2->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b4->IsInBackForwardCache());
  EXPECT_EQ(rfh_a3, current_frame_host());
  // B2 and B4 shouldn't be treated as the same site instance.
  EXPECT_NE(rfh_b2->GetSiteInstance(), rfh_b4->GetSiteInstance());

  // 6) Do a history navigation back to A1.
  // Make sure we can restore A1, while coming from A3.
  web_contents()->GetController().GoToIndex(0);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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

// Test that documents are evicted correctly from BackForwardCache after time to
// live.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, TimedEviction) {
  // Inject mock time task runner to be used in the eviction timer, so we can,
  // check for the functionality we are interested before and after the time to
  // live. We don't replace ThreadTaskRunnerHandle::Get to ensure that it
  // doesn't affect other unrelated callsites.
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();

  web_contents()->GetController().GetBackForwardCache().SetTaskRunnerForTesting(
      task_runner);

  base::TimeDelta time_to_live_in_back_forward_cache =
      BackForwardCacheImpl::GetTimeToLiveInBackForwardCache();
  // This should match the value we set in EnableFeatureAndSetParams.
  EXPECT_EQ(time_to_live_in_back_forward_cache, base::Seconds(3600));

  base::TimeDelta delta = base::Milliseconds(1);

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

  // 3) Fast forward to just before eviction is due.
  task_runner->FastForwardBy(time_to_live_in_back_forward_cache - delta);

  // 4) Confirm A is still in BackForwardCache.
  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 5) Fast forward to when eviction is due.
  task_runner->FastForwardBy(delta);

  // 6) Confirm A is evicted.
  delete_observer_rfh_a.WaitUntilDeleted();
  EXPECT_EQ(current_frame_host(), rfh_b);

  // 7) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::kTimeout}, {},
                    {}, {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    DisableBackForwardCachePreventsDocumentsFromBeingCached) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  url::Origin origin_a = url::Origin::Create(url_a);
  url::Origin origin_b = url::Origin::Create(url_b);

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  DisableBFCacheForRFHForTesting(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    {}, {}, {RenderFrameHostDisabledForTestingReason()}, {},
                    FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DisableBackForwardIsNoOpIfRfhIsGone) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  url::Origin origin_a = url::Origin::Create(url_a);
  url::Origin origin_b = url::Origin::Create(url_b);

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  GlobalRenderFrameHostId rfh_a_id = rfh_a->GetGlobalId();
  DisableBFCacheForRFHForTesting(rfh_a_id);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  delete_observer_rfh_a.WaitUntilDeleted();

  // This should not die
  DisableBFCacheForRFHForTesting(rfh_a_id);

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    {}, {}, {RenderFrameHostDisabledForTestingReason()}, {},
                    FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DisableBackForwardCacheIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  DisableBFCacheForRFHForTesting(rfh_b);

  // 2) Navigate to C. A and B are deleted.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  delete_observer_rfh_a.WaitUntilDeleted();
  delete_observer_rfh_b.WaitUntilDeleted();

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    {}, {}, {RenderFrameHostDisabledForTestingReason()}, {},
                    FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DisableBackForwardEvictsIfAlreadyInCache) {
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
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_a->is_evicted_from_back_forward_cache());

  DisableBFCacheForRFHForTesting(rfh_a);

  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    {}, {}, {RenderFrameHostDisabledForTestingReason()}, {},
                    FROM_HERE);
}

// Confirm that same-document navigation and not history-navigation does not
// record metrics.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, MetricsNotRecorded) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_b2(embedded_test_server()->GetURL("b.com", "/title1.html#2"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // 3) Navigate to B#2 (same document navigation).
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), url_b2));

  // 4) Go back to B.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectOutcomeDidNotChange(FROM_HERE);

  // 5) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectOutcomeDidNotChange(FROM_HERE);
}

// Test for functionality of domain specific controls in back-forward cache.
class BackForwardCacheBrowserTestWithDomainControlEnabled
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets the allowed websites for testing, additionally adding the params
    // used by BackForwardCacheBrowserTest.
    std::string allowed_websites =
        "https://a.allowed/back_forward_cache/, "
        "https://b.allowed/back_forward_cache/allowed_path.html";
    EnableFeatureAndSetParams(features::kBackForwardCache, "allowed_websites",
                              allowed_websites);

    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// Check the RenderFrameHost allowed to enter the BackForwardCache are the ones
// matching with the "allowed_websites" feature params.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithDomainControlEnabled,
                       CachePagesWithMatchedURLs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.allowed", "/back_forward_cache/allowed_path.html"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.allowed", "/back_forward_cache/allowed_path.html?query=bar"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // 3) Check if rfh_a is stored in back-forward cache, since it matches to
  // the list of allowed urls, it should be stored.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 4) Now go back to the last stored page, which in our case should be A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());

  // 5) Check if rfh_b is stored in back-forward cache, since it matches to
  // the list of allowed urls, it should be stored.
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
}

// We don't want to allow websites which doesn't match "allowed_websites" of
// feature params to be stored in back-forward cache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithDomainControlEnabled,
                       DoNotCachePagesWithUnMatchedURLs) {
  DisableCheckingMetricsForAllSites();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.disallowed", "/back_forward_cache/disallowed_path.html"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.allowed", "/back_forward_cache/disallowed_path.html"));
  GURL url_c(embedded_test_server()->GetURL(
      "c.disallowed", "/back_forward_cache/disallowed_path.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // 3) Since url of A doesn't match to the the list of allowed urls it should
  // not be stored in back-forward cache.
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  delete_observer_rfh_a.WaitUntilDeleted();

  // 4) Navigate to C.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));

  // 5) Since url of B doesn't match to the the list of allowed urls it should
  // not be stored in back-forward cache.
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  delete_observer_rfh_b.WaitUntilDeleted();

  // 6) Go back to B.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Nothing is recorded when the domain does not match.
  ExpectOutcomeDidNotChange(FROM_HERE);
  ExpectNotRestoredDidNotChange(FROM_HERE);
}

// Test the "blocked_websites" feature params in back-forward cache.
class BackForwardCacheBrowserTestWithBlockedWebsites
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets the blocked websites for testing, additionally adding the params
    // used by BackForwardCacheBrowserTest.
    std::string blocked_websites =
        "https://a.blocked/, "
        "https://b.blocked/";
    EnableFeatureAndSetParams(features::kBackForwardCache, "blocked_websites",
                              blocked_websites);

    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// Check the disallowed page isn't bfcached when it's navigated from allowed
// page.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithBlockedWebsites,
                       NavigateFromAllowedPageToDisallowedPage) {
  // Skip checking the AllSites metrics since BackForwardCacheMetrics stop
  // recording except BackForwardCache.AllSites.* metrics when the target URL is
  // disallowed by allowed_websites or blocked_websites.
  DisableCheckingMetricsForAllSites();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.allowed", "/back_forward_cache/allowed_path.html"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.blocked", "/back_forward_cache/disallowed_path.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // 3) Check if rfh_a is stored in back-forward cache, since it doesn't match
  // to the blocked_websites, and allowed_websites are empty, so it should
  // be stored.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 4) Now go back to the last stored page, which in our case should be A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());
  ExpectRestored(FROM_HERE);

  // 5) Check if rfh_b is not stored in back-forward cache, since it matches to
  // the blocked_websites.
  delete_observer_rfh_b.WaitUntilDeleted();
  EXPECT_TRUE(delete_observer_rfh_b.deleted());

  // 6) Go forward to B. B should not restored from the back-forward cache.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Nothing is recorded since B is disallowed.
  ExpectOutcomeDidNotChange(FROM_HERE);
  ExpectNotRestoredDidNotChange(FROM_HERE);
}

// Check the allowed page is bfcached when it's navigated from disallowed
// page.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithBlockedWebsites,
                       NavigateFromDisallowedPageToAllowedPage) {
  // Skip checking the AllSites metrics since BackForwardCacheMetrics stop
  // recording except BackForwardCache.AllSites.* metrics when the target URL is
  // disallowed by allowed_websites or blocked_websites.
  DisableCheckingMetricsForAllSites();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.blocked", "/back_forward_cache/disallowed_path.html"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.allowed", "/back_forward_cache/allowed_path.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // 3) Check if rfh_a is not stored in back-forward cache, since it matches to
  // the blocked_websites.
  delete_observer_rfh_a.WaitUntilDeleted();
  EXPECT_TRUE(delete_observer_rfh_a.deleted());

  // 4) Now go back to url_a which is not bfcached.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Nothing is recorded since A is disallowed.
  ExpectOutcomeDidNotChange(FROM_HERE);
  ExpectNotRestoredDidNotChange(FROM_HERE);

  // 5) Check if rfh_b is stored in back-forward cache, since it doesn't match
  // to the blocked_websites, and allowed_websites are empty, so it should
  // be stored.
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());

  // 6) Go forward to url_b which is bfcached.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectRestored(FROM_HERE);
}

// Test BackForwardCache::IsAllowed() with several allowed_websites URL
// patterns.
class BackForwardCacheBrowserTestForAllowedWebsitesUrlPatterns
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets the allowed websites for testing, additionally adding the params
    // used by BackForwardCacheBrowserTest.
    std::string allowed_websites =
        "https://a.com/,"
        "https://b.com/path,"
        "https://c.com/path/";
    EnableFeatureAndSetParams(features::kBackForwardCache, "allowed_websites",
                              allowed_websites);

    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// Check if the URLs are allowed when allowed_websites are specified.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestForAllowedWebsitesUrlPatterns,
                       AllowedWebsitesUrlPatterns) {
  BackForwardCacheImpl& bfcache =
      web_contents()->GetController().GetBackForwardCache();

  // Doesn't match with any allowed_websites.
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://a.org/")));

  // Exact match with https://a.com/.
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://a.com/")));
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://a.com")));

  // Match with https://a.com/ since we don't take into account the difference
  // on port number.
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://a.com:123/")));

  // Match with https://a.com/ since we don't take into account the difference
  // on query.
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://a.com:123/?x=1")));

  // Match with https://a.com/ since we don't take into account the difference
  // on scheme.
  EXPECT_TRUE(bfcache.IsAllowed(GURL("http://a.com/")));

  // Match with https://a.com/ since we are checking the prefix on path.
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://a.com/path")));

  // Doesn't match with https://a.com/ since the host doesn't match with a.com.
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://prefix.a.com/")));

  // Doesn't match with https://b.com/path since the path prefix doesn't match.
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://b.com/")));

  // Exact match with https://b.com/path.
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://b.com/path")));

  // Match with https://b.com/path since we are checking the prefix on path.
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://b.com/path/")));
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://b.com/path_abc")));
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://b.com/path_abc?x=1")));

  // Doesn't match with https://c.com/path/ since the path prefix doesn't match.
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://c.com/path")));
}

// Test BackForwardCache::IsAllowed() with several blocked_websites URL
// patterns.
class BackForwardCacheBrowserTestForBlockedWebsitesUrlPatterns
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets the blocked websites for testing, additionally adding the params
    // used by BackForwardCacheBrowserTest.
    std::string blocked_websites =
        "https://a.com/,"
        "https://b.com/path,"
        "https://c.com/path/";
    EnableFeatureAndSetParams(features::kBackForwardCache, "blocked_websites",
                              blocked_websites);

    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// Check if the URLs are allowed when blocked_websites are specified.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestForBlockedWebsitesUrlPatterns,
                       BlockedWebsitesUrlPatterns) {
  BackForwardCacheImpl& bfcache =
      web_contents()->GetController().GetBackForwardCache();

  // Doesn't match with any blocked_websites.
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://a.org/")));

  // Exact match with https://a.com/.
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://a.com/")));
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://a.com")));

  // Match with https://a.com/ since we don't take into account the difference
  // on port number.
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://a.com:123/")));

  // Match with https://a.com/ since we don't take into account the difference
  // on query.
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://a.com:123/?x=1")));

  // Match with https://a.com/ since we don't take into account the difference
  // on scheme.
  EXPECT_FALSE(bfcache.IsAllowed(GURL("http://a.com/")));

  // Match with https://a.com/ since we are checking the prefix on path.
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://a.com/path")));

  // Doesn't match with https://a.com/ since the host doesn't match with a.com.
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://prefix.a.com/")));

  // Doesn't match with https://b.com/path since the path prefix doesn't match.
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://b.com/")));

  // Exact match with https://b.com/path.
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://b.com/path")));

  // Match with https://b.com/path since we are checking the prefix on path.
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://b.com/path/")));
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://b.com/path_abc")));
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://b.com/path_abc?x=1")));

  // Doesn't match with https://c.com/path/ since the path prefix doesn't match.
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://c.com/path")));
}

// Test BackForwardCache::IsAllowed() with several allowed_websites and
// blocked_websites URL patterns.
class BackForwardCacheBrowserTestForWebsitesUrlPatterns
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets the allowed websites for testing, additionally adding the params
    // used by BackForwardCacheBrowserTest.
    std::string allowed_websites = "https://a.com/";
    EnableFeatureAndSetParams(features::kBackForwardCache, "allowed_websites",
                              allowed_websites);

    // Sets the blocked websites for testing, additionally adding the params
    // used by BackForwardCacheBrowserTest.
    std::string blocked_websites = "https://a.com/";
    EnableFeatureAndSetParams(features::kBackForwardCache, "blocked_websites",
                              blocked_websites);

    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// Check if the URLs are allowed when allowed_websites and blocked_websites are
// specified.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestForWebsitesUrlPatterns,
                       WebsitesUrlPatterns) {
  BackForwardCacheImpl& bfcache =
      web_contents()->GetController().GetBackForwardCache();

  // https://a.com/ is not allowed since blocked_websites will be prioritized
  // when the same website is specified in allowed_websites and
  // blocked_websites.
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://a.com/")));
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://a.com")));
}

// Test the "blocked_cgi_params" feature params in back-forward cache.
class BackForwardCacheBrowserTestWithBlockedCgiParams
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets the blocked websites for testing, additionally adding the params
    // used by BackForwardCacheBrowserTest.
    std::string blocked_cgi_params = "ibp=1|tbm=1";
    EnableFeatureAndSetParams(features::kBackForwardCache, "blocked_cgi_params",
                              blocked_cgi_params);

    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// Check the disallowed page isn't bfcached when it's navigated from allowed
// page.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithBlockedCgiParams,
                       NavigateFromAllowedPageToDisallowedPage) {
  // Skip checking the AllSites metrics since BackForwardCacheMetrics stop
  // recording except BackForwardCache.AllSites.* metrics when the target URL is
  // disallowed by allowed_websites or blocked_websites.
  DisableCheckingMetricsForAllSites();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_allowed(
      embedded_test_server()->GetURL("a.llowed", "/title1.html?tbm=0"));
  GURL url_not_allowed(
      embedded_test_server()->GetURL("nota.llowed", "/title1.html?tbm=1"));

  // 1) Navigate to url_allowed.
  EXPECT_TRUE(NavigateToURL(shell(), url_allowed));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_allowed = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_allowed(rfh_allowed);

  // 2) Navigate to url_not_allowed.
  EXPECT_TRUE(NavigateToURL(shell(), url_not_allowed));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_not_allowed = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_not_allowed(rfh_not_allowed);

  // 3) Check that url_allowed is stored in back-forward cache.
  EXPECT_FALSE(delete_observer_rfh_allowed.deleted());
  EXPECT_TRUE(rfh_allowed->IsInBackForwardCache());

  // 4) Now go back to url_allowed.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(rfh_allowed, current_frame_host());
  ExpectRestored(FROM_HERE);

  // 5) Check that url_not_allowed is not stored in back-forward cache
  delete_observer_rfh_not_allowed.WaitUntilDeleted();
  EXPECT_TRUE(delete_observer_rfh_not_allowed.deleted());

  // 6) Go forward to url_not_allowed, it should not be restored from the
  // back-forward cache.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Nothing is recorded since it is disallowed.
  ExpectOutcomeDidNotChange(FROM_HERE);
  ExpectNotRestoredDidNotChange(FROM_HERE);
}

// Check the allowed page is bfcached when it's navigated from disallowed
// page.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithBlockedCgiParams,
                       NavigateFromDisallowedPageToAllowedPage) {
  // Skip checking the AllSites metrics since BackForwardCacheMetrics stop
  // recording except BackForwardCache.AllSites.* metrics when the target URL is
  // disallowed by allowed_websites or blocked_websites.
  DisableCheckingMetricsForAllSites();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_allowed(
      embedded_test_server()->GetURL("a.llowed", "/title1.html?tbm=0"));
  GURL url_not_allowed(
      embedded_test_server()->GetURL("nota.llowed", "/title1.html?tbm=1"));

  // 1) Navigate to url_not_allowed.
  EXPECT_TRUE(NavigateToURL(shell(), url_not_allowed));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_not_allowed = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_not_allowed(rfh_not_allowed);

  // 2) Navigate to url_allowed.
  EXPECT_TRUE(NavigateToURL(shell(), url_allowed));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_allowed = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_allowed(rfh_allowed);

  // 3) Check that url_not_allowed is not stored in back-forward cache.
  delete_observer_rfh_not_allowed.WaitUntilDeleted();
  EXPECT_TRUE(delete_observer_rfh_not_allowed.deleted());

  // 4) Now go back to url_not_allowed.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Nothing is recorded since it is disallowed.
  ExpectOutcomeDidNotChange(FROM_HERE);
  ExpectNotRestoredDidNotChange(FROM_HERE);

  // 5) Check that url_allowed is stored in back-forward cache
  EXPECT_FALSE(delete_observer_rfh_allowed.deleted());
  EXPECT_TRUE(rfh_allowed->IsInBackForwardCache());

  // 6) Go forward to url_allowed, it should be restored from the
  // back-forward cache.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectRestored(FROM_HERE);
}

// Check that if WebPreferences was changed while a page was bfcached, it will
// get up-to-date WebPreferences when it was restored.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, WebPreferences) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  auto browsing_instance_id = rfh_a->GetSiteInstance()->GetBrowsingInstanceId();

  // A should prefer light color scheme (which is the default).
  EXPECT_EQ(
      true,
      EvalJs(web_contents(),
             "window.matchMedia('(prefers-color-scheme: light)').matches"));

  // 2) Navigate to B. A should be stored in the back-forward cache.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  EXPECT_NE(browsing_instance_id,
            rfh_b->GetSiteInstance()->GetBrowsingInstanceId());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_NE(rfh_a, rfh_b);

  blink::web_pref::WebPreferences prefs =
      web_contents()->GetOrCreateWebPreferences();
  prefs.preferred_color_scheme = blink::mojom::PreferredColorScheme::kDark;
  web_contents()->SetWebPreferences(prefs);

  // 3) Set WebPreferences to prefer dark color scheme.
  EXPECT_EQ(
      true,
      EvalJs(web_contents(),
             "window.matchMedia('(prefers-color-scheme: dark)').matches"));

  // 4) Go back to A, which should also prefer the dark color scheme now.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());

  EXPECT_EQ(
      true,
      EvalJs(web_contents(),
             "window.matchMedia('(prefers-color-scheme: dark)').matches"));
}

// Check the BackForwardCache is disabled when there is a nested WebContents
// inside a page.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, NestedWebContents) {
  // 1) Navigate to a page.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameHostImpl* child = rfh_a->child_at(0)->current_frame_host();
  EXPECT_TRUE(child);

  // Create and attach an inner WebContents.
  CreateAndAttachInnerContents(child);
  RenderFrameDeletedObserver deleted(rfh_a);

  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));
  // The page has an inner WebContents so it should be deleted.
  deleted.WaitUntilDeleted();

  // 3) Go back to the page with an inner WebContents.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kHaveInnerContents}, {}, {},
      {}, {}, FROM_HERE);
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

  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(web_contents()->GetEncoding(), "windows-1250");
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, RestoreWhilePendingCommit) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL url3(embedded_test_server()->GetURL("c.com", "/main_document"));

  // Load a page and navigate away from it, so it is stored in the back-forward
  // cache.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  RenderFrameHost* rfh1 = current_frame_host();
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  // Try to navigate to a new page, but leave it in a pending state.
  shell()->LoadURL(url3);
  response.WaitForRequest();

  // Navigate back and restore page from the cache, cancelling the previous
  // navigation.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(rfh1, current_frame_host());
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());
  ExpectRestored(FROM_HERE);
  EXPECT_EQ(web_contents()->GetContentsMimeType(), "text/html");
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       IsInactiveAndDisallowActivationIsNoopWhenActive) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_FALSE(current_frame_host()->IsInactiveAndDisallowActivation(
      DisallowActivationReasonId::kForTesting));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    IsInactiveAndDisallowActivationDoesEvictForCachedFrames) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameDeletedObserver delete_observer_rfh_a(current_frame_host());
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  const uint64_t reason = DisallowActivationReasonId::kForTesting;
  EXPECT_TRUE(rfh_a->IsInactiveAndDisallowActivation(reason));

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kIgnoreEventAndEvict}, {},
      {}, {}, {reason}, FROM_HERE);
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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

// Test scenarios where the "BackForwardCache" content flag is enabled but
// the command line flag "DisableBackForwardCache" is turned on, resulting in
// the feature being disabled.
class BackForwardCacheDisabledThroughCommandLineBrowserTest
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableBackForwardCache);
    EnableFeatureAndSetParams(blink::features::kLoadingTasksUnfreezable,
                              "max_buffered_bytes", "1000");
  }
};

// Ensures that the back-forward cache trial stays inactivated.
IN_PROC_BROWSER_TEST_F(BackForwardCacheDisabledThroughCommandLineBrowserTest,
                       BFCacheDisabled) {
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

  // 3) A shouldn't be stored in back-forward cache because it's disabled.
  delete_observer_rfh_a.WaitUntilDeleted();

  // Nothing is recorded when back-forward cache is disabled.
  ExpectOutcomeDidNotChange(FROM_HERE);
  ExpectNotRestoredDidNotChange(FROM_HERE);

  // Ensure that the trial still hasn't been activated.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));
}

// Ensures that the back-forward cache trial stays inactivated even when
// renderer code related to back-forward cache runs (in this case, network
// request loading).
IN_PROC_BROWSER_TEST_F(BackForwardCacheDisabledThroughCommandLineBrowserTest,
                       BFCacheDisabled_NetworkRequests) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Ensure that the trials starts inactive.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));

  EXPECT_FALSE(IsBackForwardCacheEnabled());

  // Ensure that we do not activate the trials for kBackForwardCache and
  // kLoadingTasksUnfreezable when querying bfcache or unfreezable loading tasks
  // status.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
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

  // 3) A shouldn't be stored in back-forward cache because it's disabled.
  delete_observer_rfh_a.WaitUntilDeleted();

  // Nothing is recorded when back-forward cache is disabled.
  ExpectOutcomeDidNotChange(FROM_HERE);
  ExpectNotRestoredDidNotChange(FROM_HERE);

  // Ensure that the trials still haven't been activated.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    EvictingDocumentsInRelatedSiteInstancesDoesNotRestartNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html#part1"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title1.html#part2"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));

  // 2) Navigate to A2.
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));

  // 3) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 4) Go back to A2, but do not wait for the navigation to commit.
  web_contents()->GetController().GoBack();

  // 5) Go back to A1.
  // This will attempt to evict A2 from the cache because
  // their navigation entries have related site instances, while a navigation
  // to A2 is in flight. Ensure that we do not try to restart it as it should
  // be superseded by a navigation to A1.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(url_a1, web_contents()->GetURL());
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());

  // Try show another dialog. It should work.
  ExecuteScriptAsync(rfh_a, R"(window.alert("alert");)");
  dialog_waiter.Wait();
}

namespace {

class ExecJsInDidFinishNavigation : public WebContentsObserver {
 public:
  explicit ExecJsInDidFinishNavigation(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (!navigation_handle->IsInMainFrame() ||
        !navigation_handle->HasCommitted() ||
        navigation_handle->IsSameDocument()) {
      return;
    }

    ExecuteScriptAsync(navigation_handle->GetRenderFrameHost(),
                       "var foo = 42;");
  }
};

}  // namespace

// This test checks that the message posted from DidFinishNavigation
// (ExecuteScriptAsync) is received after the message restoring the page from
// the back-forward cache (PageMsg_RestorePageFromBackForwardCache).
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       MessageFromDidFinishNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  EXPECT_TRUE(ExecJs(rfh_a, "window.alive = 'I am alive';"));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  ExecJsInDidFinishNavigation observer(shell()->web_contents());

  // 3) Go back to A. Expect the page to be restored from the cache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ("I am alive", EvalJs(rfh_a, "window.alive"));

  // Make sure that the javascript execution requested from DidFinishNavigation
  // did not result in eviction. If the document was evicted, the document
  // would be reloaded - check that it didn't happen and the tab is not
  // loading.
  EXPECT_FALSE(web_contents()->IsLoading());

  EXPECT_EQ(rfh_a, current_frame_host());
}

#if defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       ChildImportanceTestForBackForwardCachedPagesTest) {
  web_contents()->SetPrimaryMainFrameImportance(
      ChildProcessImportance::MODERATE);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  ASSERT_FALSE(delete_observer_rfh_a.deleted());

  // 3) Verify the importance of page after entering back-forward cache to be
  // "NORMAL".
  EXPECT_EQ(ChildProcessImportance::NORMAL,
            rfh_a->GetProcess()->GetEffectiveImportance());

  // 4) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // 5) Verify the importance was restored correctly after page leaves
  // back-forward cache.
  EXPECT_EQ(ChildProcessImportance::MODERATE,
            rfh_a->GetProcess()->GetEffectiveImportance());
}
#endif

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, PageshowMetrics) {
  // TODO(https://crbug.com/1099395): Do not check for unexpected messages
  // because the input task queue is not currently frozen, causing flakes in
  // this test.
  DoNotFailForUnexpectedMessagesWhileCached();
  ASSERT_TRUE(embedded_test_server()->Start());

  const char kHistogramName[] =
      "BackForwardCache.MainFrameHasPageshowListenersOnRestore";

  const GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  const GURL url2(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to the page.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(ExecJs(current_frame_host(), R"(
    window.foo = 42;
  )"));

  // 2) Navigate away and back.
  EXPECT_TRUE(NavigateToURL(shell(), url2));
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // As we don't get an explicit ACK when the page is restored (yet), force
  // a round-trip to the renderer to effectively flush the queue.
  EXPECT_EQ(42, EvalJs(current_frame_host(), "window.foo"));

  // Expect the back-forward restore without pageshow to be detected.
  content::FetchHistogramsFromChildProcesses();
  EXPECT_THAT(histogram_tester_.GetAllSamples(kHistogramName),
              ElementsAre(base::Bucket(0, 1)));

  EXPECT_TRUE(ExecJs(current_frame_host(), R"(
    window.addEventListener("pageshow", () => {});
  )"));

  // 3) Navigate away and back (again).
  EXPECT_TRUE(NavigateToURL(shell(), url2));
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // As we don't get an explicit ACK when the page is restored (yet), force
  // a round-trip to the renderer to effectively flush the queue.
  EXPECT_EQ(42, EvalJs(current_frame_host(), "window.foo"));

  // Expect the back-forward restore with pageshow to be detected.
  content::FetchHistogramsFromChildProcesses();
  EXPECT_THAT(histogram_tester_.GetAllSamples(kHistogramName),
              ElementsAre(base::Bucket(0, 1), base::Bucket(1, 1)));
}

// Navigate from A(B) to C and check IsActive status for RenderFrameHost A
// and B before and after entering back-forward cache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, CheckIsActive) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to A(B).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();

  EXPECT_TRUE(rfh_a->IsActive());
  EXPECT_TRUE(rfh_b->IsActive());

  // 2) Navigate to C.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());

  EXPECT_FALSE(rfh_a->IsActive());
  EXPECT_FALSE(rfh_b->IsActive());
}

// Test that LifecycleStateImpl is updated correctly when page enters and
// restores back from BackForwardCache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       CheckLifecycleStateTransition) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to A and check the LifecycleStateImpl of A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_a->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kActive,
            rfh_a->GetLifecycleState());
  EXPECT_TRUE(rfh_a->GetPage().IsPrimary());
  EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());

  // 2) Navigate to B, now A enters BackForwardCache. Check the
  // LifecycleStateImpl of both RenderFrameHost A and B.
  {
    testing::NiceMock<MockWebContentsObserver> state_change_observer(
        web_contents());
    EXPECT_CALL(state_change_observer,
                RenderFrameHostStateChanged(
                    rfh_a, RenderFrameHost::LifecycleState::kActive,
                    RenderFrameHost::LifecycleState::kInBackForwardCache));
    // We don't know |rfh_b| yet, so we'll match any frame.
    EXPECT_CALL(state_change_observer,
                RenderFrameHostStateChanged(
                    testing::Not(rfh_a),
                    RenderFrameHost::LifecycleState::kPendingCommit,
                    RenderFrameHost::LifecycleState::kActive));

    EXPECT_TRUE(NavigateToURL(shell(), url_b));
  }
  RenderFrameHostImpl* rfh_b = current_frame_host();
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache,
            rfh_a->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kInBackForwardCache,
            rfh_a->GetLifecycleState());
  EXPECT_FALSE(rfh_a->GetPage().IsPrimary());
  EXPECT_FALSE(rfh_a->IsInPrimaryMainFrame());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_b->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kActive,
            rfh_b->GetLifecycleState());
  EXPECT_TRUE(rfh_b->GetPage().IsPrimary());
  EXPECT_TRUE(rfh_b->IsInPrimaryMainFrame());

  // 3) Go back to A and check again the LifecycleStateImpl of both
  // RenderFrameHost A and B.
  {
    testing::NiceMock<MockWebContentsObserver> state_change_observer(
        web_contents());
    EXPECT_CALL(state_change_observer,
                RenderFrameHostStateChanged(
                    rfh_a, RenderFrameHost::LifecycleState::kInBackForwardCache,
                    RenderFrameHost::LifecycleState::kActive));
    EXPECT_CALL(state_change_observer,
                RenderFrameHostStateChanged(
                    rfh_b, RenderFrameHost::LifecycleState::kActive,
                    RenderFrameHost::LifecycleState::kInBackForwardCache));

    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_a->lifecycle_state());
  EXPECT_TRUE(rfh_a->GetPage().IsPrimary());
  EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache,
            rfh_b->lifecycle_state());
  EXPECT_FALSE(rfh_b->GetPage().IsPrimary());
  EXPECT_FALSE(rfh_b->IsInPrimaryMainFrame());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       CheckLifecycleStateTransitionWithSubframes) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL(
      "c.com", "/cross_site_iframe_factory.html?c(d)"));

  // Navigate to A(B) and check the lifecycle states of A and B.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_a->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kActive,
            rfh_a->GetLifecycleState());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_b->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kActive,
            rfh_b->GetLifecycleState());

  // Navigate to C(D), now A(B) enters BackForwardCache.
  {
    testing::NiceMock<MockWebContentsObserver> state_change_observer(
        web_contents());
    EXPECT_CALL(state_change_observer,
                RenderFrameHostStateChanged(
                    rfh_a, RenderFrameHost::LifecycleState::kActive,
                    RenderFrameHost::LifecycleState::kInBackForwardCache));
    EXPECT_CALL(state_change_observer,
                RenderFrameHostStateChanged(
                    rfh_b, RenderFrameHost::LifecycleState::kActive,
                    RenderFrameHost::LifecycleState::kInBackForwardCache));
    // We don't know |rfh_c| and |rfh_d| yet, so we'll match any frame.
    EXPECT_CALL(state_change_observer,
                RenderFrameHostStateChanged(
                    testing::Not(testing::AnyOf(rfh_a, rfh_b)),
                    RenderFrameHost::LifecycleState::kPendingCommit,
                    RenderFrameHost::LifecycleState::kActive))
        .Times(2);
    // Deletion of frame D's initial RFH.
    EXPECT_CALL(state_change_observer,
                RenderFrameHostStateChanged(
                    testing::Not(testing::AnyOf(rfh_a, rfh_b)),
                    RenderFrameHost::LifecycleState::kActive,
                    RenderFrameHost::LifecycleState::kPendingDeletion));

    EXPECT_TRUE(NavigateToURL(shell(), url_c));
  }
  RenderFrameHostImpl* rfh_c = current_frame_host();
  RenderFrameHostImpl* rfh_d = rfh_c->child_at(0)->current_frame_host();
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_FALSE(rfh_c->IsInBackForwardCache());
  EXPECT_FALSE(rfh_d->IsInBackForwardCache());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache,
            rfh_a->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kInBackForwardCache,
            rfh_a->GetLifecycleState());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache,
            rfh_b->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kInBackForwardCache,
            rfh_b->GetLifecycleState());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_c->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kActive,
            rfh_c->GetLifecycleState());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_d->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kActive,
            rfh_d->GetLifecycleState());

  // Go back to A(B), A(B) is restored and C(D) enters BackForwardCache.
  {
    testing::NiceMock<MockWebContentsObserver> state_change_observer(
        web_contents());
    EXPECT_CALL(state_change_observer,
                RenderFrameHostStateChanged(
                    rfh_a, RenderFrameHost::LifecycleState::kInBackForwardCache,
                    RenderFrameHost::LifecycleState::kActive));
    EXPECT_CALL(state_change_observer,
                RenderFrameHostStateChanged(
                    rfh_b, RenderFrameHost::LifecycleState::kInBackForwardCache,
                    RenderFrameHost::LifecycleState::kActive));
    EXPECT_CALL(state_change_observer,
                RenderFrameHostStateChanged(
                    rfh_c, RenderFrameHost::LifecycleState::kActive,
                    RenderFrameHost::LifecycleState::kInBackForwardCache));
    EXPECT_CALL(state_change_observer,
                RenderFrameHostStateChanged(
                    rfh_d, RenderFrameHost::LifecycleState::kActive,
                    RenderFrameHost::LifecycleState::kInBackForwardCache));

    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());
  EXPECT_TRUE(rfh_c->IsInBackForwardCache());
  EXPECT_TRUE(rfh_d->IsInBackForwardCache());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_a->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kActive,
            rfh_a->GetLifecycleState());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_b->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kActive,
            rfh_b->GetLifecycleState());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache,
            rfh_c->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kInBackForwardCache,
            rfh_c->GetLifecycleState());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache,
            rfh_d->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kInBackForwardCache,
            rfh_d->GetLifecycleState());
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());

  EXPECT_EQ(coep, rfh_a->cross_origin_embedder_policy());
}

namespace {

class EchoFakeWithFilter final : public mojom::Echo {
 public:
  explicit EchoFakeWithFilter(mojo::PendingReceiver<mojom::Echo> receiver,
                              std::unique_ptr<mojo::MessageFilter> filter)
      : receiver_(this, std::move(receiver)) {
    receiver_.SetFilter(std::move(filter));
  }
  ~EchoFakeWithFilter() override = default;

  // mojom::Echo implementation
  void EchoString(const std::string& input,
                  EchoStringCallback callback) override {
    std::move(callback).Run(input);
  }

 private:
  mojo::Receiver<mojom::Echo> receiver_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       MessageReceivedOnAssociatedInterfaceWhileCached) {
  DoNotFailForUnexpectedMessagesWhileCached();
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  PageLifecycleStateManagerTestDelegate delegate(
      rfh_a->render_view_host()->GetPageLifecycleStateManager());

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  delegate.WaitForInBackForwardCacheAck();
  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  mojo::Remote<mojom::Echo> remote;
  EchoFakeWithFilter echo(
      remote.BindNewPipeAndPassReceiver(),
      rfh_a->CreateMessageFilterForAssociatedReceiver(mojom::Echo::Name_));

  base::RunLoop loop;
  remote->EchoString(
      "", base::BindLambdaForTesting([&](const std::string&) { loop.Quit(); }));
  loop.Run();

  ExpectBucketCount(
      "BackForwardCache.UnexpectedRendererToBrowserMessage.InterfaceName",
      base::HistogramBase::Sample(
          static_cast<int32_t>(base::HashMetricName(mojom::Echo::Name_))),
      1);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    MessageReceivedOnAssociatedInterfaceWhileCachedForProcessWithNonCachedPages) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("/title2.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  PageLifecycleStateManagerTestDelegate delegate(
      rfh_a->render_view_host()->GetPageLifecycleStateManager());

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  delegate.WaitForInBackForwardCacheAck();
  RenderFrameHostImpl* rfh_b = current_frame_host();
  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  // Make sure both pages are on the same process (they are same site so they
  // should).
  ASSERT_EQ(rfh_a->GetProcess(), rfh_b->GetProcess());

  mojo::Remote<mojom::Echo> remote;
  EchoFakeWithFilter echo(
      remote.BindNewPipeAndPassReceiver(),
      rfh_a->CreateMessageFilterForAssociatedReceiver(mojom::Echo::Name_));

  remote->EchoString("", base::NullCallback());
  // Give the killing a chance to run. (We do not expect a kill but need to
  // "wait" for it to not happen)
  base::RunLoop().RunUntilIdle();

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    HighCacheSizeBackForwardCacheBrowserTest,
    MessageReceivedOnAssociatedInterfaceForProcessWithMultipleCachedPages) {
  DoNotFailForUnexpectedMessagesWhileCached();
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a_1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a_2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Get url_a_1 and url_a_2 into the cache.
  EXPECT_TRUE(NavigateToURL(shell(), url_a_1));
  RenderFrameHostImpl* rfh_a_1 = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a_1(rfh_a_1);

  EXPECT_TRUE(NavigateToURL(shell(), url_a_2));
  RenderFrameHostImpl* rfh_a_2 = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a_2(rfh_a_2);

  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  ASSERT_FALSE(delete_observer_rfh_a_1.deleted());
  ASSERT_FALSE(delete_observer_rfh_a_2.deleted());
  EXPECT_TRUE(rfh_a_1->IsInBackForwardCache());
  EXPECT_TRUE(rfh_a_2->IsInBackForwardCache());
  ASSERT_EQ(rfh_a_1->GetProcess(), rfh_a_2->GetProcess());

  mojo::Remote<mojom::Echo> remote;
  EchoFakeWithFilter echo(
      remote.BindNewPipeAndPassReceiver(),
      rfh_a_1->CreateMessageFilterForAssociatedReceiver(mojom::Echo::Name_));

  base::RunLoop loop;
  remote->EchoString(
      "", base::BindLambdaForTesting([&](const std::string&) { loop.Quit(); }));
  loop.Run();

  ExpectBucketCount(
      "BackForwardCache.UnexpectedRendererToBrowserMessage.InterfaceName",
      base::HistogramBase::Sample(
          static_cast<int32_t>(base::HashMetricName(mojom::Echo::Name_))),
      1);

  EXPECT_FALSE(delete_observer_rfh_b.deleted());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       MessageReceivedOnAssociatedInterfaceWhileFreezing) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  url::Origin origin_a = url::Origin::Create(url_a);
  url::Origin origin_b = url::Origin::Create(url_b);

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  PageLifecycleStateManagerTestDelegate delegate(
      rfh_a->render_view_host()->GetPageLifecycleStateManager());

  mojo::Remote<mojom::Echo> remote;
  EchoFakeWithFilter echo(
      remote.BindNewPipeAndPassReceiver(),
      rfh_a->CreateMessageFilterForAssociatedReceiver(mojom::Echo::Name_));

  delegate.OnStoreInBackForwardCacheSent(base::BindLambdaForTesting(
      [&]() { remote->EchoString("", base::NullCallback()); }));

  delegate.OnRestoreFromBackForwardCacheSent(base::BindLambdaForTesting(
      [&]() { remote->EchoString("", base::NullCallback()); }));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ExpectRestored(FROM_HERE);
}

// Tests that if a page is already ineligible to be saved in the back-forward
// cache at navigation time, we shouldn't try to proactively swap
// BrowsingInstances.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       ShouldNotSwapBrowsingInstanceWhenPageWillNotBeCached) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));
  GURL url_3(embedded_test_server()->GetURL("/title3.html"));

  // 1) Navigate to |url_1| .
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  RenderFrameHostImpl* rfh_1 = current_frame_host();
  scoped_refptr<SiteInstanceImpl> site_instance_1 =
      static_cast<SiteInstanceImpl*>(rfh_1->GetSiteInstance());

  // 2) Navigate to |url_2|.
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  RenderFrameHostImpl* rfh_2 = current_frame_host();
  RenderFrameDeletedObserver rfh_2_deleted_observer(rfh_2);
  scoped_refptr<SiteInstanceImpl> site_instance_2 =
      static_cast<SiteInstanceImpl*>(rfh_2->GetSiteInstance());

  // |rfh_1| should get into the back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());
  // Check that title1.html and title2.html are in different BrowsingInstances.
  EXPECT_FALSE(site_instance_1->IsRelatedSiteInstance(site_instance_2.get()));

  // Disable the BackForwardCache for |rfh_2|.
  DisableBFCacheForRFHForTesting(rfh_2->GetGlobalId());

  // 3) Navigate to |url_3|.
  EXPECT_TRUE(NavigateToURL(shell(), url_3));
  RenderFrameHostImpl* rfh_3 = current_frame_host();
  scoped_refptr<SiteInstanceImpl> site_instance_3 =
      static_cast<SiteInstanceImpl*>(rfh_3->GetSiteInstance());

  // Check that |url_2| and |url_3| are reusing the same SiteInstance (and
  // BrowsingInstance).
  EXPECT_EQ(site_instance_2, site_instance_3);
  if (rfh_2 != rfh_3) {
    // If we aren't reusing the RenderFrameHost then |rfh_2| will eventually
    // get deleted because it's not saved in the back-forward cache.
    rfh_2_deleted_observer.WaitUntilDeleted();
  }
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
    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents()));

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
    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents()));

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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(rfh_1.get(), current_frame_host());
  ExpectRestored(FROM_HERE);
}

// We should try to reuse process on same-site renderer-initiated navigations.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       RendererInitiatedSameSiteNavigationReusesProcess) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));

  // Navigate to title1.html.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  scoped_refptr<SiteInstanceImpl> site_instance_1 =
      web_contents()->GetMainFrame()->GetSiteInstance();
  // Navigate to title2.html. The navigation is document/renderer initiated.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), url_2));
  scoped_refptr<SiteInstanceImpl> site_instance_2 =
      web_contents()->GetMainFrame()->GetSiteInstance();

  // Check that title1.html and title2.html are in different BrowsingInstances
  // but have the same renderer process.
  EXPECT_FALSE(site_instance_1->IsRelatedSiteInstance(site_instance_2.get()));
  EXPECT_EQ(site_instance_1->GetProcess(), site_instance_2->GetProcess());
}

// We should try to reuse process on same-site browser-initiated navigations.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       BrowserInitiatedSameSiteNavigationReusesProcess) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));

  // 1) Navigate to title1.html.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  scoped_refptr<SiteInstanceImpl> site_instance_1 =
      web_contents()->GetMainFrame()->GetSiteInstance();
  // 2) Navigate to title2.html. The navigation is browser initiated.
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  scoped_refptr<SiteInstanceImpl> site_instance_2 =
      web_contents()->GetMainFrame()->GetSiteInstance();

  // Check that title1.html and title2.html are in different BrowsingInstances
  // but have the same renderer process.
  EXPECT_FALSE(site_instance_1->IsRelatedSiteInstance(site_instance_2.get()));
  EXPECT_EQ(site_instance_1->GetProcess(), site_instance_2->GetProcess());

  // 3) Do a back navigation to title1.html.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url_1);
  scoped_refptr<SiteInstanceImpl> site_instance_1_history_nav =
      web_contents()->GetMainFrame()->GetSiteInstance();

  // We will reuse the SiteInstance and renderer process of |site_instance_1|.
  EXPECT_EQ(site_instance_1_history_nav, site_instance_1);
  EXPECT_EQ(site_instance_1_history_nav->GetProcess(),
            site_instance_1->GetProcess());
}

// We should not try to reuse process on cross-site navigations.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       CrossSiteNavigationDoesNotReuseProcess) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL a1_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL a2_url(embedded_test_server()->GetURL("a.com", "/title2.html"));

  // Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), a1_url));
  scoped_refptr<SiteInstanceImpl> a1_site_instance =
      web_contents()->GetMainFrame()->GetSiteInstance();
  // Navigate to B. The navigation is browser initiated.
  EXPECT_TRUE(NavigateToURL(shell(), b_url));
  scoped_refptr<SiteInstanceImpl> b_site_instance =
      web_contents()->GetMainFrame()->GetSiteInstance();

  // Check that A1 and B are in different BrowsingInstances and renderer
  // processes.
  EXPECT_FALSE(a1_site_instance->IsRelatedSiteInstance(b_site_instance.get()));
  EXPECT_NE(a1_site_instance->GetProcess(), b_site_instance->GetProcess());

  // Navigate to A2. The navigation is renderer-initiated.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), a2_url));
  scoped_refptr<SiteInstanceImpl> a2_site_instance =
      web_contents()->GetMainFrame()->GetSiteInstance();

  // Check that B and A2 are in different BrowsingInstances and renderer
  // processes.
  EXPECT_FALSE(b_site_instance->IsRelatedSiteInstance(a2_site_instance.get()));
  EXPECT_NE(b_site_instance->GetProcess(), a2_site_instance->GetProcess());
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
  web_contents()->GetController().GoBack();
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

  // 3) Go forward to |url2|, browser-initiated.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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

// This observer keeps tracks whether a given RenderViewHost is deleted or not
// to avoid accessing it and causing use-after-free condition.
class RenderViewHostDeletedObserver : public WebContentsObserver {
 public:
  explicit RenderViewHostDeletedObserver(RenderViewHost* rvh)
      : WebContentsObserver(WebContents::FromRenderViewHost(rvh)),
        render_view_host_(rvh),
        deleted_(false) {}

  void RenderViewDeleted(RenderViewHost* render_view_host) override {
    if (render_view_host_ == render_view_host)
      deleted_ = true;
  }

  bool deleted() const { return deleted_; }

 private:
  RenderViewHost* render_view_host_;
  bool deleted_;
};

// Tests that RenderViewHost is deleted on eviction along with
// RenderProcessHost.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       RenderViewHostDeletedOnEviction) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  NavigationControllerImpl& controller = web_contents()->GetController();
  BackForwardCacheImpl& cache = controller.GetBackForwardCache();

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  RenderViewHostDeletedObserver delete_observer_rvh_a(
      rfh_a->GetRenderViewHost());

  RenderProcessHost* process = rfh_a->GetProcess();
  RenderProcessHostWatcher destruction_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  cache.Flush();

  // 2) Navigate to B. A should be stored in cache, count of entries should
  // be 1.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_EQ(1u, cache.GetEntries().size());

  // 3) Initiate eviction of rfh_a from BackForwardCache. Entries should be 0.
  // RenderViewHost, RenderProcessHost and RenderFrameHost should all be
  // deleted.
  EXPECT_TRUE(rfh_a->IsInactiveAndDisallowActivation(
      DisallowActivationReasonId::kForTesting));
  destruction_observer.Wait();
  ASSERT_TRUE(delete_observer_rvh_a.deleted());
  delete_observer_rfh_a.WaitUntilDeleted();
  EXPECT_EQ(0u, cache.GetEntries().size());
}

// Tests that cross-process sub-frame's RenderViewHost is deleted on root
// RenderFrameHost eviction from BackForwardCache along with its
// RenderProcessHost.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       CrossProcessSubFrameRenderViewHostDeletedOnEviction) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* a1 = current_frame_host();
  RenderFrameHostImpl* b1 = a1->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b1(b1);

  RenderViewHostDeletedObserver delete_observer_rvh_b1(b1->GetRenderViewHost());

  RenderProcessHost* process = b1->GetProcess();
  RenderProcessHostWatcher destruction_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);

  // 2) Navigate to URL B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(a1->IsInBackForwardCache());

  // 3) Initiate eviction of rfh a1 from BackForwardCache. RenderViewHost,
  // RenderProcessHost and RenderFrameHost of sub-frame b1 should all be deleted
  // on eviction.
  EXPECT_TRUE(a1->IsInactiveAndDisallowActivation(
      DisallowActivationReasonId::kForTesting));
  destruction_observer.Wait();
  ASSERT_TRUE(delete_observer_rvh_b1.deleted());
  delete_observer_rfh_b1.WaitUntilDeleted();
}

// Tests that same-process sub-frame's RenderViewHost is deleted on root
// RenderFrameHost eviction from BackForwardCache along with its
// RenderProcessHost.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       SameProcessSubFrameRenderViewHostDeletedOnEviction) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* a1 = current_frame_host();
  RenderFrameHostImpl* a2 = a1->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a2(a2);

  RenderViewHostDeletedObserver delete_observer_rvh_a2(a2->GetRenderViewHost());

  RenderProcessHost* process = a2->GetProcess();
  RenderProcessHostWatcher destruction_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);

  // 2) Navigate to URL B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(a1->IsInBackForwardCache());

  // 3) Initiate eviction of rfh a1 from BackForwardCache. RenderViewHost,
  // RenderProcessHost and RenderFrameHost of sub-frame a2 should all be
  // deleted.
  EXPECT_TRUE(a1->IsInactiveAndDisallowActivation(
      DisallowActivationReasonId::kForTesting));
  destruction_observer.Wait();
  ASSERT_TRUE(delete_observer_rvh_a2.deleted());
  delete_observer_rfh_a2.WaitUntilDeleted();
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       NavigationCancelledAfterJsEvictionWasDisabled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  PageLifecycleStateManagerTestDelegate delegate(
      rfh_a->render_view_host()->GetPageLifecycleStateManager());

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  RenderFrameHostImpl* rfh_b = current_frame_host();

  delegate.OnDisableJsEvictionSent(base::BindLambdaForTesting([&]() {
    // Posted because Stop() will destroy the NavigationRequest but
    // DisableJsEviction will be called from inside the navigation which may
    // not be a safe place to destruct a NavigationRequest.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&WebContentsImpl::Stop,
                                  base::Unretained(web_contents())));
  }));

  // 3) Do not go back to A (navigation cancelled).
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(rfh_b, current_frame_host());

  delete_observer_rfh_a.WaitUntilDeleted();

  // 4) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kNavigationCancelledWhileRestoring},
                    {}, {}, {}, {}, FROM_HERE);
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
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
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(url_b, controller.GetLastCommittedEntry()->GetURL());

  // The page loaded at B) is correctly cached and restored. Reloading doesn't
  // affect the cache usage.
  ExpectRestored(FROM_HERE);

  // 6) Go back to a.com.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(url_a, controller.GetLastCommittedEntry()->GetURL());

  // The page loaded at 3) is correctly cached and restored. Reloading doesn't
  // affect the cache usage.
  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       SubframeNavigationDoesNotRecordMetrics) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to A(B).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2) Navigate from B to C.
  EXPECT_TRUE(NavigateFrameToURL(rfh_a->child_at(0), url_c));
  EXPECT_EQ(url_c,
            rfh_a->child_at(0)->current_frame_host()->GetLastCommittedURL());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());

  // 4) Go back from C to B.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_TRUE(
      rfh_a->child_at(0)->current_frame_host()->GetLastCommittedURL().DomainIs(
          "b.com"));
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());

  // The reason why the frame is not cached in a subframe navigation is not
  // recorded.
  ExpectOutcomeDidNotChange(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       EnsureIsolationInfoForSubresourcesNotEmpty) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  NavigationControllerImpl& controller = web_contents()->GetController();
  BackForwardCacheImpl& cache = controller.GetBackForwardCache();

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  cache.Flush();

  // 2) Navigate to B. A should be stored in cache, count of entries should
  // be 1.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_EQ(1u, cache.GetEntries().size());

  // 3) GoBack to A. RenderFrameHost of A should be restored and B should be
  // stored in cache, count of entries should be 1. IsolationInfoForSubresources
  // of rfh_a should not be empty.
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_EQ(1u, cache.GetEntries().size());
  EXPECT_FALSE(rfh_a->GetIsolationInfoForSubresources().IsEmpty());

  // 4) GoForward to B. RenderFrameHost of B should be restored and A should be
  // stored in cache, count of entries should be 1. IsolationInfoForSubresources
  // of rfh_b should not be empty.
  controller.GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(rfh_b, current_frame_host());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_EQ(1u, cache.GetEntries().size());
  EXPECT_FALSE(rfh_b->GetIsolationInfoForSubresources().IsEmpty());
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

// Regression test for crbug.com/1183313, but for is_overriding_user_agent.
// Checks that we won't restore an entry from the BackForwardCache if the
// is_overriding_user_agent value used in the entry differs from the one used
// in the restoring navigation.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoNotRestoreWhenIsOverridingUserAgentDiffers) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));
  NavigationControllerImpl& controller = web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  const std::string user_agent_override = "foo";

  // 1) Navigate to A without user agent override.
  {
    FrameNavigateParamsCapturer params_capturer(root);
    EXPECT_TRUE(NavigateToURL(shell(), url_a));
    params_capturer.Wait();
    EXPECT_FALSE(params_capturer.is_overriding_user_agent());
    EXPECT_NE(user_agent_override,
              EvalJs(shell()->web_contents(), "navigator.userAgent"));
  }

  RenderFrameHostImpl* rfh_a = current_frame_host();

  // Enable user agent override for future navigations.
  UserAgentInjector injector(shell()->web_contents(), user_agent_override);

  // 2) Navigate to B with user agent override.
  {
    FrameNavigateParamsCapturer params_capturer(root);
    EXPECT_TRUE(NavigateToURL(shell(), url_b));
    params_capturer.Wait();
    EXPECT_TRUE(params_capturer.is_overriding_user_agent());
    EXPECT_EQ(user_agent_override,
              EvalJs(shell()->web_contents(), "navigator.userAgent"));
  }

  // A should be stored in the back-forward cache.
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  RenderFrameHostImpl* rfh_b = current_frame_host();

  // 3) Go back to A. RenderFrameHost of A should not be restored from the
  // back-forward cache, and "is_overriding_user_agent" is set to true
  // correctly.
  {
    RenderFrameDeletedObserver delete_observer(rfh_a);
    FrameNavigateParamsCapturer params_capturer(root);
    controller.GoBack();
    params_capturer.Wait();
    delete_observer.WaitUntilDeleted();
    EXPECT_TRUE(params_capturer.is_overriding_user_agent());
    EXPECT_EQ(user_agent_override,
              EvalJs(shell()->web_contents(), "navigator.userAgent"));
    ExpectNotRestored(
        {BackForwardCacheMetrics::NotRestoredReason::kUserAgentOverrideDiffers},
        {}, {}, {}, {}, FROM_HERE);
  }

  // B should be stored in the back-forward cache.
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());

  // 4) Go forward to B. RenderFrameHost of B should be restored from the
  // back-forward cache, and "is_overriding_user_agent" is set to true
  // correctly.
  {
    FrameNavigateParamsCapturer params_capturer(root);
    controller.GoForward();
    params_capturer.Wait();
    EXPECT_TRUE(params_capturer.is_overriding_user_agent());
    EXPECT_EQ(user_agent_override,
              EvalJs(shell()->web_contents(), "navigator.userAgent"));
    EXPECT_EQ(rfh_b, current_frame_host());
    ExpectRestored(FROM_HERE);
  }

  // Stop overriding user agent from now on.
  injector.set_is_overriding_user_agent(false);

  // 5) Go to C, which should not do a user agent override.
  {
    FrameNavigateParamsCapturer params_capturer(root);
    EXPECT_TRUE(NavigateToURL(shell(), url_c));
    params_capturer.Wait();
    EXPECT_FALSE(params_capturer.is_overriding_user_agent());
    EXPECT_NE(user_agent_override,
              EvalJs(shell()->web_contents(), "navigator.userAgent"));
  }

  // B should be stored in the back-forward cache again.
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());

  // 6) Go back to B. RenderFrameHost of B should not be restored from the
  // back-forward cache, and "is_overriding_user_agent" is set to false
  // correctly.
  {
    FrameNavigateParamsCapturer params_capturer(root);
    RenderFrameDeletedObserver delete_observer(rfh_b);
    controller.GoBack();
    params_capturer.Wait();
    delete_observer.WaitUntilDeleted();
    EXPECT_FALSE(params_capturer.is_overriding_user_agent());
    EXPECT_NE(user_agent_override,
              EvalJs(shell()->web_contents(), "navigator.userAgent"));
    ExpectNotRestored(
        {BackForwardCacheMetrics::NotRestoredReason::kUserAgentOverrideDiffers},
        {}, {}, {}, {}, FROM_HERE);
  }
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       RestoreWhenUserAgentOverrideDiffers) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  NavigationControllerImpl& controller = web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Enable user agent override for future navigations.
  const std::string user_agent_override_1 = "foo";
  UserAgentInjector injector(shell()->web_contents(), user_agent_override_1);

  // 1) Start a new navigation to A with user agent override.
  {
    FrameNavigateParamsCapturer params_capturer(root);
    EXPECT_TRUE(NavigateToURL(shell(), url_a));
    params_capturer.Wait();
    EXPECT_TRUE(params_capturer.is_overriding_user_agent());
    EXPECT_EQ(user_agent_override_1,
              EvalJs(shell()->web_contents(), "navigator.userAgent"));
  }

  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2) Navigate to another page.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // A should be stored in the back-forward cache.
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Change the user agent override string.
  const std::string user_agent_override_2 = "bar";
  injector.set_user_agent_override(user_agent_override_2);

  // 3) Go back to A, which should restore the page saved in the back-forward
  // cache and use the old user agent.
  // TODO(https://crbug.com/1194880): This should use the new UA override.
  {
    FrameNavigateParamsCapturer params_capturer(root);
    controller.GoBack();
    params_capturer.Wait();
    EXPECT_TRUE(params_capturer.is_overriding_user_agent());
    EXPECT_EQ(user_agent_override_1,
              EvalJs(shell()->web_contents(), "navigator.userAgent"));
    EXPECT_EQ(rfh_a, current_frame_host());
    ExpectRestored(FROM_HERE);
  }

  // 4) Navigate to another page, which should use the new user agent. Note that
  // we didn't do this in step 2 instead because the UA override change during
  // navigation would trigger a RendererPreferences to the active page (page A).
  {
    FrameNavigateParamsCapturer params_capturer(root);
    EXPECT_TRUE(NavigateToURL(shell(), url_b));
    params_capturer.Wait();
    EXPECT_TRUE(params_capturer.is_overriding_user_agent());
    EXPECT_EQ(user_agent_override_2,
              EvalJs(shell()->web_contents(), "navigator.userAgent"));
  }
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       WebContentsDestroyedWhileRestoringThePageFromBFCache) {
  ASSERT_TRUE(embedded_test_server()->Start());

  Shell* shell = CreateBrowser();

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell, url_a));

  // 2) Navigate to another page.
  EXPECT_TRUE(NavigateToURL(shell, url_b));

  // 3) Start navigating back.
  TestNavigationManager nav_manager(shell->web_contents(), url_a);
  shell->web_contents()->GetController().GoBack();
  nav_manager.WaitForFirstYieldAfterDidStartNavigation();

  testing::NiceMock<MockWebContentsObserver> observer(shell->web_contents());
  EXPECT_CALL(observer, DidFinishNavigation(_))
      .WillOnce(testing::Invoke([](NavigationHandle* handle) {
        EXPECT_FALSE(handle->HasCommitted());
        EXPECT_TRUE(handle->IsServedFromBackForwardCache());
        // This call checks that |rfh_restored_from_back_forward_cache| is not
        // deleted and the virtual |GetRoutingID| does not crash.
        EXPECT_TRUE(NavigationRequest::From(handle)
                        ->rfh_restored_from_back_forward_cache()
                        ->GetRoutingID());
      }));

  shell->Close();
}

// Test if the delegate doesn't support BFCache that the reason is
// recorded correctly.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DelegateDoesNotSupportBackForwardCache) {
  // Set the delegate to null to force the default behavior.
  web_contents()->SetDelegate(nullptr);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  // BackForwardCache is empty.
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  // BackForwardCache contains only rfh_a.
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  web_contents()->GetController().GoToOffset(-1);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kBackForwardCacheDisabledForDelegate},
                    {}, {}, {}, {}, FROM_HERE);
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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kOptInUnloadHeaderNotPresent},
                    {}, {}, {}, {}, FROM_HERE);

  // 4) Go forward. - B has the header, so it should be cached.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  if (GetParam() == "always" || GetParam() == "opt_in_header_required") {
    ExpectRestored(FROM_HERE);
  } else {
    ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                           kUnloadHandlerExistsInMainFrame},
                      {}, {}, {}, {}, FROM_HERE);
  }

  // 4) Go forward.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

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
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  if (GetParam() == "always" || GetParam() == "opt_in_header_required") {
    ExpectRestored(FROM_HERE);
  } else {
    ASSERT_EQ("no", GetParam());
    ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                           kUnloadHandlerExistsInSubFrame},
                      {}, {}, {}, {}, FROM_HERE);
  }

  // 4) Go forward.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

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
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  if (GetParam() == "always") {
    ExpectRestored(FROM_HERE);
  } else {
    ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                           kUnloadHandlerExistsInSubFrame},
                      {}, {}, {}, {}, FROM_HERE);
  }

  // 4) Go forward.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, NoThrottlesOnCacheRestore) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  bool did_register_throttles = false;

  // This will track for each navigation whether we attempted to register
  // NavigationThrottles.
  content::ShellContentBrowserClient::Get()
      ->set_create_throttles_for_navigation_callback(base::BindLambdaForTesting(
          [&did_register_throttles](content::NavigationHandle* handle)
              -> std::vector<std::unique_ptr<content::NavigationThrottle>> {
            did_register_throttles = true;
            return std::vector<std::unique_ptr<content::NavigationThrottle>>();
          }));

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  ASSERT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(did_register_throttles);
  did_register_throttles = false;

  // 3) Go back to A which is in the BackForward cache and will be restored via
  // an IsPageActivation navigation. Ensure that we did not register
  // NavigationThrottles for this navigation since we already ran their checks
  // when we navigated to A in step 1.
  web_contents()->GetController().GoBack();
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_FALSE(did_register_throttles);

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

// Tests that a back navigation from a crashed page has the process state
// tracked correctly by WebContentsImpl.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       BackNavigationFromCrashedPage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  url::Origin origin_a = url::Origin::Create(url_a);
  url::Origin origin_b = url::Origin::Create(url_b);

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  EXPECT_FALSE(web_contents()->IsCrashed());

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
  EXPECT_FALSE(web_contents()->IsCrashed());

  // 3) Crash B.
  CrashTab(web_contents());
  EXPECT_TRUE(web_contents()->IsCrashed());
  EXPECT_TRUE(delete_observer_rfh_b.deleted());

  // 4) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_EQ(origin_a, rfh_a->GetLastCommittedOrigin());
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_EQ(rfh_a->GetVisibilityState(), PageVisibilityState::kVisible);
  EXPECT_FALSE(web_contents()->IsCrashed());

  ExpectRestored(FROM_HERE);
}

// Injects a blank subframe into the current document just before processing
// DidCommitNavigation for a specified URL.
class InjectCreateChildFrame : public DidCommitNavigationInterceptor {
 public:
  InjectCreateChildFrame(WebContents* web_contents, const GURL& url)
      : DidCommitNavigationInterceptor(web_contents), url_(url) {}

  InjectCreateChildFrame(const InjectCreateChildFrame&) = delete;
  InjectCreateChildFrame& operator=(const InjectCreateChildFrame&) = delete;

  bool was_called() { return was_called_; }

 private:
  // DidCommitNavigationInterceptor implementation.
  bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      mojom::DidCommitProvisionalLoadParamsPtr*,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    if (!was_called_ && navigation_request &&
        navigation_request->GetURL() == url_) {
      EXPECT_TRUE(ExecuteScript(
          web_contents(),
          "document.body.appendChild(document.createElement('iframe'));"));
    }
    was_called_ = true;
    return true;
  }

  bool was_called_ = false;
  GURL url_;
};

// Verify that when A navigates to B, and A creates a subframe just before B
// commits, the subframe does not inherit a proxy in B's process from its
// parent.  Otherwise, if A gets bfcached and later restored, the subframe's
// proxy would be (1) in a different BrowsingInstance than the rest of its
// page, and (2) preserved after the restore, which would cause crashes when
// later using that proxy (for example, when creating more subframes). See
// https://crbug.com/1243541.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    InjectSubframeDuringPendingCrossBrowsingInstanceNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  EXPECT_EQ(0U, rfh_a->child_count());

  // 2) Navigate to B, and inject a blank subframe just before it commits.
  {
    InjectCreateChildFrame injector(shell()->web_contents(), url_b);
    ASSERT_TRUE(NavigateToURL(shell(), url_b));
    EXPECT_TRUE(injector.was_called());
  }

  // `rfh_a` should be in BackForwardCache, and it should have a subframe.
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  ASSERT_EQ(1U, rfh_a->child_count());

  // The new subframe should not have any proxies at this point.  In
  // particular, it shouldn't inherit a proxy in b.com from its parent.
  EXPECT_TRUE(rfh_a->child_at(0)
                  ->render_manager()
                  ->GetAllProxyHostsForTesting()
                  .empty());

  RenderFrameHostImplWrapper rfh_b(current_frame_host());

  // 3) Go back.  This should restore `rfh_a` from the cache, and `rfh_b`
  // should go into the cache.
  web_contents()->GetController().GoBack();
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(rfh_a.get(), current_frame_host());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());

  // 4) Add a grandchild frame to `rfh_a`.  This shouldn't crash.
  RenderFrameHostCreatedObserver frame_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecuteScript(
      rfh_a->child_at(0),
      "document.body.appendChild(document.createElement('iframe'));"));
  frame_observer.Wait();
  EXPECT_EQ(1U, rfh_a->child_at(0)->child_count());

  // Make sure the grandchild is live.
  EXPECT_TRUE(ExecuteScript(rfh_a->child_at(0)->child_at(0), "true"));
}

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

// BEFORE ADDING A NEW TEST HERE
// Read the note at the top about the other files you could add it to.
}  // namespace content
