// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <unordered_map>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/hash/hash.h"
#include "base/location.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/common/task_annotator.h"
#include "base/task/post_task.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/bad_message.h"
#include "content/browser/generic_sensor/sensor_provider_proxy_impl.h"
#include "content/browser/presentation/presentation_test_utils.h"
#include "content/browser/renderer_host/back_forward_cache_can_store_document_result.h"
#include "content/browser/renderer_host/back_forward_cache_disable.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/page_lifecycle_state_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/should_swap_browsing_instance.h"
#include "content/browser/web_contents/file_chooser_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/render_accessibility.mojom.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/document_service_base.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/idle_manager.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/commit_message_delayer.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/idle_test_utils.h"
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
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/echo.test-mojom.h"
#include "content/test/web_contents_observer_test_utils.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "services/device/public/cpp/test/fake_sensor_and_provider.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "services/device/public/mojom/vibration_manager.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/app_banner/app_banner.mojom.h"

using testing::_;
using testing::Each;
using testing::ElementsAre;
using testing::Not;
using testing::UnorderedElementsAreArray;

namespace content {

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

// Test about the BackForwardCache.
class BackForwardCacheBrowserTest : public ContentBrowserTest,
                                    public WebContentsObserver {
 public:
  ~BackForwardCacheBrowserTest() override {
    if (fail_for_unexpected_messages_while_cached_) {
      // If this is triggered, see
      // tools/metrics/histograms/histograms_xml/navigation/histograms.xml for
      // which values correspond which messages.
      EXPECT_THAT(histogram_tester_.GetAllSamples(
                      "BackForwardCache.UnexpectedRendererToBrowserMessage."
                      "InterfaceName"),
                  testing::ElementsAre());
    }
  }

 protected:
  using UkmMetrics = ukm::TestUkmRecorder::HumanReadableUkmMetrics;

  // Disables checking metrics that are recorded recardless of the domains. By
  // default, this class' Expect* function checks the metrics both for the
  // specific domain and for all domains at the same time. In the case when the
  // test results need to be different, call this function.
  void DisableCheckingMetricsForAllSites() { check_all_sites_ = false; }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeUIForMediaStream);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kIgnoreCertificateErrors);
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
        features::kBackForwardCache, "check_eligibility_after_pagehide",
        check_eligibility_after_pagehide_ ? "true" : "false");
    EnableFeatureAndSetParams(
        blink::features::kLogUnexpectedIPCPostedToBackForwardCachedDocuments,
        "delay_before_tracking_ms", "0");
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
    ContentBrowserTest::SetUpCommandLine(command_line);
  }

  void SetupFeaturesAndParameters() {
    std::vector<base::test::ScopedFeatureList::FeatureAndParams>
        enabled_features;

    for (auto& features_with_param : features_with_params_) {
      enabled_features.emplace_back(features_with_param.first,
                                    features_with_param.second);
    }

    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features_);
  }

  void EnableFeatureAndSetParams(base::Feature feature,
                                 std::string param_name,
                                 std::string param_value) {
    features_with_params_[feature][param_name] = param_value;
  }

  void DisableFeature(base::Feature feature) {
    disabled_features_.push_back(feature);
  }

  void SetUp() override {
    // Fake the BluetoothAdapter to say it's present.
    // Used in WebBluetooth test.
    adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // In CHROMEOS build, even when |adapter_| object is released at TearDown()
    // it causes the test to fail on exit with an error indicating |adapter_| is
    // leaked.
    testing::Mock::AllowLeak(adapter_.get());
#endif

    ContentBrowserTest::SetUp();
  }

  void TearDown() override {
    testing::Mock::VerifyAndClearExpectations(adapter_.get());
    adapter_.reset();
    ContentBrowserTest::TearDown();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    // TestAutoSetUkmRecorder's constructor requires a sequenced context.
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    ContentBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    ukm_recorder_.reset();
    ContentBrowserTest::TearDownOnMainThread();
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetFrameTree()->root()->current_frame_host();
  }

  RenderFrameHostManager* render_frame_host_manager() {
    return web_contents()->GetFrameTree()->root()->render_manager();
  }

  std::string DepictFrameTree(FrameTreeNode* node) {
    return visualizer_.DepictFrameTree(node);
  }

  bool HistogramContainsIntValue(base::HistogramBase::Sample sample,
                                 std::vector<base::Bucket> histogram_values) {
    auto it = std::find_if(histogram_values.begin(), histogram_values.end(),
                           [sample](const base::Bucket& bucket) {
                             return bucket.min == static_cast<int>(sample);
                           });
    return it != histogram_values.end();
  }

  void ExpectOutcomeDidNotChange(base::Location location) {
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
    EXPECT_THAT(ukm_recorder_->GetMetrics("HistoryNavigation",
                                          {is_served_from_bfcache}),
                expected_ukm_outcomes_)
        << location.ToString();
  }

  void ExpectRestored(base::Location location) {
    ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored,
                  location);
    ExpectReasons({}, {}, {}, {}, location);
  }

  void ExpectNotRestored(
      std::vector<BackForwardCacheMetrics::NotRestoredReason> not_restored,
      std::vector<blink::scheduler::WebSchedulerTrackedFeature> block_listed,
      const std::vector<ShouldSwapBrowsingInstance>& not_swapped,
      const std::vector<BackForwardCache::DisabledReason>&
          disabled_for_render_frame_host,
      base::Location location) {
    ExpectOutcome(
        BackForwardCacheMetrics::HistoryNavigationOutcome::kNotRestored,
        location);
    ExpectReasons(not_restored, block_listed, not_swapped,
                  disabled_for_render_frame_host, location);
  }

  void ExpectNotRestoredDidNotChange(base::Location location) {
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

  void ExpectBlocklistedFeature(
      blink::scheduler::WebSchedulerTrackedFeature feature,
      base::Location location) {
    ExpectBlocklistedFeatures({feature}, location);
  }

  void ExpectBrowsingInstanceNotSwappedReason(ShouldSwapBrowsingInstance reason,
                                              base::Location location) {
    ExpectBrowsingInstanceNotSwappedReasons({reason}, location);
  }

  void ExpectEvictedAfterCommitted(
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

  void EvictByJavaScript(RenderFrameHostImpl* rfh) {
    // Run JavaScript on a page in the back-forward cache. The page should be
    // evicted. As the frame is deleted, ExecJs returns false without executing.
    // Run without user gesture to prevent UpdateUserActivationState message
    // being sent back to browser.
    EXPECT_FALSE(
        ExecJs(rfh, "console.log('hi');", EXECUTE_SCRIPT_NO_USER_GESTURE));
  }

  void StartRecordingEvents(RenderFrameHostImpl* rfh) {
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

  void MatchEventList(RenderFrameHostImpl* rfh,
                      base::ListValue list,
                      base::Location location = base::Location::Current()) {
    EXPECT_EQ(list, EvalJs(rfh, "window.testObservedEvents"))
        << location.ToString();
  }

  // Creates a minimal HTTPS server, accessible through https_server().
  // Returns a pointer to the server.
  net::EmbeddedTestServer* CreateHttpsServer() {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->AddDefaultHandlers(GetTestDataFilePath());
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    return https_server();
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  void ExpectTotalCount(base::StringPiece name,
                        base::HistogramBase::Count count) {
    histogram_tester_.ExpectTotalCount(name, count);
  }

  template <typename T>
  void ExpectBucketCount(base::StringPiece name,
                         T sample,
                         base::HistogramBase::Count expected_count) {
    histogram_tester_.ExpectBucketCount(name, sample, expected_count);
  }

  // Do not fail this test if a message from a renderer arrives at the browser
  // for a cached page.
  void DoNotFailForUnexpectedMessagesWhileCached() {
    fail_for_unexpected_messages_while_cached_ = false;
  }

  base::HistogramTester histogram_tester_;

 protected:
  bool same_site_back_forward_cache_enabled_ = true;
  bool skip_same_site_if_unload_exists_ = false;
  bool check_eligibility_after_pagehide_ = false;
  std::string unload_support_ = "always";

 private:
  void AddSampleToBuckets(std::vector<base::Bucket>* buckets,
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

  void ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome outcome,
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
    EXPECT_THAT(ukm_recorder_->GetMetrics("HistoryNavigation",
                                          {is_served_from_bfcache}),
                expected_ukm_outcomes_)
        << location.ToString();
  }

  void ExpectReasons(
      std::vector<BackForwardCacheMetrics::NotRestoredReason> not_restored,
      std::vector<blink::scheduler::WebSchedulerTrackedFeature> block_listed,
      const std::vector<ShouldSwapBrowsingInstance>& not_swapped,
      const std::vector<BackForwardCache::DisabledReason>&
          disabled_for_render_frame_host,
      base::Location location) {
    // Check that the expected reasons are consistent.
    bool expect_blocklisted =
        std::count(
            not_restored.begin(), not_restored.end(),
            BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures) >
        0;
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
  }

  void ExpectNotRestoredReasons(
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

  void ExpectBlocklistedFeatures(
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

  void ExpectDisabledWithReasons(
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

  void ExpectBrowsingInstanceNotSwappedReasons(
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

  base::test::ScopedFeatureList feature_list_;

  FrameTreeVisualizer visualizer_;
  std::vector<base::Bucket> expected_outcomes_;
  std::vector<base::Bucket> expected_not_restored_;
  std::vector<base::Bucket> expected_blocklisted_features_;
  std::vector<base::Bucket> expected_disabled_reasons_;
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

  // Indicates whether metrics for all sites regardless of the domains are
  // checked or not.
  bool check_all_sites_ = true;
  // Whether we should fail the test if a message arrived at the browser from a
  // renderer for a bfcached page.
  bool fail_for_unexpected_messages_while_cached_ = true;

  scoped_refptr<device::MockBluetoothAdapter> adapter_;
};

// Match RenderFrameHostImpl* that are in the BackForwardCache.
MATCHER(InBackForwardCache, "") {
  return arg->IsInBackForwardCache();
}

// Match RenderFrameDeleteObserver* which observed deletion of the RenderFrame.
MATCHER(Deleted, "") {
  return arg->deleted();
}

// Helper function to pass an initializer list to the EXPECT_THAT macro. This is
// indeed the identity function.
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

 private:
  // WebContentsObserver:
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
    if (callback_)
      std::move(callback_).Run(navigation_handle);
  }

  base::OnceCallback<void(NavigationHandle*)> callback_;

  DISALLOW_COPY_AND_ASSIGN(ReadyToCommitNavigationCallback);
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

void WaitForDOMContentLoaded(RenderFrameHostImpl* rfh) {
  DOMContentLoadedObserver observer(rfh);
  observer.Wait();
}

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

class FakeIdleTimeProvider : public IdleManager::IdleTimeProvider {
 public:
  FakeIdleTimeProvider() = default;
  ~FakeIdleTimeProvider() override = default;
  FakeIdleTimeProvider(const FakeIdleTimeProvider&) = delete;
  FakeIdleTimeProvider& operator=(const FakeIdleTimeProvider&) = delete;

  base::TimeDelta CalculateIdleTime() override {
    return base::TimeDelta::FromSeconds(0);
  }

  bool CheckIdleStateIsLocked() override { return false; }
};

}  // namespace

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
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  EXPECT_EQ(1u, rfh_a->GetSiteInstance()->GetRelatedActiveContentsCount());
  Shell* popup = OpenPopup(rfh_a, url_a, "");
  EXPECT_EQ(2u, rfh_a->GetSiteInstance()->GetRelatedActiveContentsCount());

  // 2) Navigate to B. The previous document can't enter the BackForwardCache,
  // because of the popup.
  EXPECT_TRUE(ExecJs(rfh_a, JsReplace("location = $1;", url_b.spec())));
  delete_observer_rfh_a.WaitUntilDeleted();
  RenderFrameHostImpl* rfh_b = current_frame_host();
  EXPECT_EQ(2u, rfh_b->GetSiteInstance()->GetRelatedActiveContentsCount());

  // 3) Go back to A. The previous document can't enter the BackForwardCache,
  // because of the popup.
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  EXPECT_TRUE(ExecJs(rfh_b, "history.back();"));
  delete_observer_rfh_b.WaitUntilDeleted();

  // 4) Make the popup drop the window.opener connection. It happens when the
  //    user does an omnibox-initiated navigation, which happens in a new
  //    BrowsingInstance.
  RenderFrameHostImpl* rfh_a_new = current_frame_host();
  EXPECT_EQ(2u, rfh_a_new->GetSiteInstance()->GetRelatedActiveContentsCount());
  EXPECT_TRUE(NavigateToURL(popup, url_b));
  EXPECT_EQ(1u, rfh_a_new->GetSiteInstance()->GetRelatedActiveContentsCount());

  // 5) Navigate to B again. As the scripting relationship with the popup is
  // now severed, the current page (|rfh_a_new|) can enter back-forward cache.
  RenderFrameDeletedObserver delete_observer_rfh_a_new(rfh_a_new);
  EXPECT_TRUE(ExecJs(rfh_a_new, JsReplace("location = $1;", url_b.spec())));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a_new.deleted());
  EXPECT_TRUE(rfh_a_new->IsInBackForwardCache());

  // 6) Go back to A. The current document can finally enter the
  // BackForwardCache, because it is alone in its BrowsingInstance and has never
  // been related to any other document.
  RenderFrameHostImpl* rfh_b_new = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b_new(rfh_b_new);
  EXPECT_TRUE(ExecJs(rfh_b_new, "history.back();"));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_FALSE(delete_observer_rfh_b_new.deleted());
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
                    {}, {}, {}, FROM_HERE);
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
        rfhs[j].WaitUntilRenderFrameDeleted();
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
          {}, {}, {}, FROM_HERE);
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
        rfhs[j].WaitUntilRenderFrameDeleted();
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
          FROM_HERE);
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
  rfhs[1].WaitUntilRenderFrameDeleted();

  // Check that a0 is cached and backgrounded.
  ExpectCached(rfhs[0], /*cached=*/true, /*backgrounded=*/true);
  // Check that a2-3 are cached and foregrounded.
  ExpectCached(rfhs[2], /*cached=*/true, /*backgrounded=*/false);
  ExpectCached(rfhs[3], /*cached=*/true, /*backgrounded=*/false);
}

// Tests that |RenderFrameHost::ForEachRenderFrameHost| and
// |WebContents::ForEachRenderFrameHost| behave correctly with bfcached
// RenderFrameHosts.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, ForEachRenderFrameHost) {
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
                       NavigationsAreFullyCommitted) {
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
                       ProxiesAreStoredAndRestored) {
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
  EXPECT_EQ(2u, cached_entry->proxy_hosts.size());

  // 3. Navigate from an uncacheable to a cached page page (B->A).
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  // Note: We still have a transition proxy that will be used to perform the
  // frame swap. It gets deleted with rfh_b below.
  EXPECT_EQ(3u, render_frame_host_manager()->GetProxyCount());

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
  EXPECT_EQ(2u, cached_entry->proxy_hosts.size());

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
  EXPECT_EQ(3u, cached_entry->proxy_hosts.size());
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
  GURL url_b(embedded_test_server()->GetURL(
      "b.com", "/back_forward_cache/page_with_dedicated_worker.html"));
  GURL test_url(embedded_test_server()->GetURL("c.com", "/title1.html"));

  NavigationControllerImpl& controller = web_contents()->GetController();

  // 1. Navigate to a cacheable page (A).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2. Navigate from a cacheable page to an uncacheable page (A->B).
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3. Navigate from an uncacheable to a cached page page (B->A).
  // This restores the top frame's proxy in the z.com (iframe's) process.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // 4. Verify that the main frame's z.com proxy is still functional.
  RenderFrameHostImpl* iframe =
      rfh_a->frame_tree_node()->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe, "top.location.href = '" + test_url.spec() + "';"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // We expect to have navigated through the proxy.
  EXPECT_EQ(test_url, controller.GetLastCommittedEntry()->GetURL());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       PageWithDedicatedWorkerNotCached) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL(
          "a.com", "/back_forward_cache/page_with_dedicated_worker.html")));
  RenderFrameDeletedObserver delete_observer_rfh_a(current_frame_host());

  // Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  // The page with the unsupported feature should be deleted (not cached).
  delete_observer_rfh_a.WaitUntilDeleted();
}

// TODO(https://crbug.com/154571): Shared workers are not available on Android.
#if defined(OS_ANDROID)
#define MAYBE_PageWithSharedWorkerNotCached \
  DISABLED_PageWithSharedWorkerNotCached
#else
#define MAYBE_PageWithSharedWorkerNotCached PageWithSharedWorkerNotCached
#endif
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       MAYBE_PageWithSharedWorkerNotCached) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL(
          "a.com", "/back_forward_cache/page_with_shared_worker.html")));
  RenderFrameDeletedObserver delete_observer_rfh_a(current_frame_host());

  // Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  // The page with the unsupported feature should be deleted (not cached).
  delete_observer_rfh_a.WaitUntilDeleted();

  // Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::kSharedWorker}, {}, {},
      FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       SubframeWithDisallowedFeatureNotCached) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a page with an iframe that contains a dedicated worker.
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL(
          "a.com", "/back_forward_cache/dedicated_worker_in_subframe.html")));
  EXPECT_EQ(42, EvalJs(current_frame_host()->child_at(0)->current_frame_host(),
                       "window.receivedMessagePromise"));

  RenderFrameDeletedObserver delete_rfh_a(current_frame_host());

  // Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  // The page with the unsupported feature should be deleted (not cached).
  delete_rfh_a.WaitUntilDeleted();

  // Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::kDedicatedWorkerOrWorklet},
      {}, {}, FROM_HERE);
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

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoesNotCacheIfRecordingAudio) {
  ASSERT_TRUE(embedded_test_server()->Start());

  BackForwardCacheDisabledTester tester;

  // Navigate to an empty page.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  int process_id = current_frame_host()->GetProcess()->GetID();
  int routing_id = current_frame_host()->GetRoutingID();

  // Request for audio recording.
  EXPECT_EQ("success", EvalJs(current_frame_host(), R"(
    new Promise(resolve => {
      navigator.mediaDevices.getUserMedia({audio: true})
        .then(m => { window.keepaliveMedia = m; resolve("success"); })
        .catch(() => { resolve("error"); });
    });
  )"));

  RenderFrameDeletedObserver deleted(current_frame_host());

  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // The page was still recording audio when we navigated away, so it shouldn't
  // have been cached.
  deleted.WaitUntilDeleted();

  // 3) Go back. Note that the reason for kWasGrantedMediaAccess occurs after
  // MediaDevicesDispatcherHost is called, hence, both are reasons for the page
  // not being restored.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  auto reason = BackForwardCacheDisable::DisabledReason(
      BackForwardCacheDisable::DisabledReasonId::kMediaDevicesDispatcherHost);
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kWasGrantedMediaAccess,
       BackForwardCacheMetrics::NotRestoredReason::
           kDisableForRenderFrameHostCalled},
      {}, {}, {reason}, FROM_HERE);
  EXPECT_TRUE(
      tester.IsDisabledForFrameWithReason(process_id, routing_id, reason));
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoesNotCacheIfSubframeRecordingAudio) {
  ASSERT_TRUE(embedded_test_server()->Start());

  BackForwardCacheDisabledTester tester;

  // Navigate to a page with an iframe.
  GURL url(embedded_test_server()->GetURL("/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  RenderFrameHostImpl* rfh = current_frame_host();
  int process_id =
      rfh->child_at(0)->current_frame_host()->GetProcess()->GetID();
  int routing_id = rfh->child_at(0)->current_frame_host()->GetRoutingID();

  // Request for audio recording from the subframe.
  EXPECT_EQ("success", EvalJs(rfh->child_at(0)->current_frame_host(), R"(
    new Promise(resolve => {
      navigator.mediaDevices.getUserMedia({audio: true})
        .then(m => { resolve("success"); })
        .catch(() => { resolve("error"); });
    });
  )"));

  RenderFrameDeletedObserver deleted(current_frame_host());

  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // The page was still recording audio when we navigated away, so it shouldn't
  // have been cached.
  deleted.WaitUntilDeleted();

  // 3) Go back. Note that the reason for kWasGrantedMediaAccess occurs after
  // MediaDevicesDispatcherHost is called, hence, both are reasons for the page
  // not being restored.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  auto reason = BackForwardCacheDisable::DisabledReason(
      BackForwardCacheDisable::DisabledReasonId::kMediaDevicesDispatcherHost);

  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kWasGrantedMediaAccess,
       BackForwardCacheMetrics::NotRestoredReason::
           kDisableForRenderFrameHostCalled},
      {}, {}, {reason}, FROM_HERE);
  EXPECT_TRUE(
      tester.IsDisabledForFrameWithReason(process_id, routing_id, reason));
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoesNotCacheIfMediaDeviceSubscribed) {
  ASSERT_TRUE(embedded_test_server()->Start());

  BackForwardCacheDisabledTester tester;

  // Navigate to a page with an iframe.
  GURL url(embedded_test_server()->GetURL("/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  RenderFrameHostImpl* rfh = current_frame_host();
  int process_id =
      rfh->child_at(0)->current_frame_host()->GetProcess()->GetID();
  int routing_id = rfh->child_at(0)->current_frame_host()->GetRoutingID();

  EXPECT_EQ("success", EvalJs(rfh->child_at(0)->current_frame_host(), R"(
    new Promise(resolve => {
      navigator.mediaDevices.addEventListener('devicechange', function(event){});
      resolve("success");
    });
  )"));

  RenderFrameDeletedObserver deleted(current_frame_host());

  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // The page was subscribed to media devices when we navigated away, so it
  // shouldn't have been cached.
  deleted.WaitUntilDeleted();

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  auto reason = BackForwardCacheDisable::DisabledReason(
      BackForwardCacheDisable::DisabledReasonId::kMediaDevicesDispatcherHost);
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    {}, {}, {reason}, FROM_HERE);
  EXPECT_TRUE(
      tester.IsDisabledForFrameWithReason(process_id, routing_id, reason));
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
                    {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoesNotCacheIfImageStillLoading) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an image that loads forever.
  GURL url(embedded_test_server()->GetURL("a.com",
                                          "/infinitely_loading_image.html"));
  TestNavigationManager navigation_manager(shell()->web_contents(), url);
  shell()->LoadURL(url);

  // The navigation finishes while the image is still loading.
  navigation_manager.WaitForNavigationFinished();
  // Wait for the document to load DOM to ensure that kLoading is not
  // one of the reasons why the document wasn't cached.
  WaitForDOMContentLoaded(current_frame_host());

  RenderFrameDeletedObserver delete_observer_rfh_a(current_frame_host());

  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // The page was still loading when we navigated away, so it shouldn't have
  // been cached.
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back.
  TestNavigationManager navigation_manager_back(shell()->web_contents(), url);
  web_contents()->GetController().GoBack();
  navigation_manager_back.WaitForNavigationFinished();
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::
           kOutstandingNetworkRequestOthers},
      {}, {}, FROM_HERE);
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
      {}, {}, {}, FROM_HERE);
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
      {}, {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, CacheIfWebGL) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with WebGL usage
  GURL url(embedded_test_server()->GetURL(
      "example.com", "/back_forward_cache/page_with_webgl.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  // The page had an active WebGL context when we navigated away,
  // but it should be cached.

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectRestored(FROM_HERE);
}

// Since blink::mojom::HidService binder is not added in
// content/browser/browser_interface_binders.cc for Android, this test is not
// applicable for this OS.
#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, DoesNotCacheIfWebHID) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to an empty page.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Request for HID devices.
  EXPECT_EQ("success", EvalJs(current_frame_host(), R"(
    new Promise(resolve => {
      navigator.hid.getDevices()
        .then(m => { resolve("success"); })
        .catch(() => { resolve("error"); });
    });
  )"));

  RenderFrameDeletedObserver deleted(current_frame_host());

  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // The page uses WebHID so it should be deleted.
  deleted.WaitUntilDeleted();

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::kWebHID}, {}, {},
      FROM_HERE);
}
#endif  // !defined(OS_ANDROID)

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       WakeLockReleasedUponEnteringBfcache) {
  ASSERT_TRUE(CreateHttpsServer()->Start());

  // 1) Navigate to a page with WakeLock usage.
  GURL url(https_server()->GetURL(
      "a.com", "/back_forward_cache/page_with_wakelock.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* rfh_a = current_frame_host();
  // Acquire WakeLock.
  EXPECT_EQ("DONE", EvalJs(rfh_a, "acquireWakeLock()"));
  // Make sure that WakeLock is not released yet.
  EXPECT_FALSE(EvalJs(rfh_a, "wakeLockIsReleased()").ExtractBool());

  // 2) Navigate away.
  shell()->LoadURL(https_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Go back to the page with WakeLock, restored from BackForwardCache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(current_frame_host(), rfh_a);
  EXPECT_TRUE(EvalJs(rfh_a, "wakeLockIsReleased()").ExtractBool());
  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoesNotCacheIfWebFileSystem) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with WebFileSystem usage.
  GURL url(embedded_test_server()->GetURL("/fileapi/request_test.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver deleted(rfh_a);

  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));
  // The page uses WebFilesystem so it should be deleted.
  deleted.WaitUntilDeleted();

  // 3) Go back to the page with WebFileSystem.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::kWebFileSystem}, {}, {},
      FROM_HERE);
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
      {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, DoesNotCacheIdleManager) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page and start using the IdleManager class.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver deleted(rfh_a);

  content::IdleManagerHelper::SetIdleTimeProviderForTest(
      rfh_a, std::make_unique<FakeIdleTimeProvider>());

  EXPECT_TRUE(ExecJs(rfh_a, R"(
    new Promise(async resolve => {
      let idleDetector = new IdleDetector();
      idleDetector.start();
      resolve();
    });
  )"));

  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // The page uses IdleManager so it should be deleted.
  deleted.WaitUntilDeleted();

  // 3) Go back and make sure the IdleManager page wasn't in the cache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::kIdleManager}, {}, {},
      FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, DoesNotCacheSMSService) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page and start using the SMSService.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver rfh_a_deleted(rfh_a);

  EXPECT_TRUE(ExecJs(rfh_a, R"(
    navigator.credentials.get({otp: {transport: ["sms"]}});
  )",
                     EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  // The page uses SMSService so it should be deleted.
  rfh_a_deleted.WaitUntilDeleted();

  // 3) Go back and make sure the SMSService page wasn't in the cache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Note that on certain linux tests, there is occasionally a not restored
  // reason of kDisableForRenderFrameHostCalled. This is due to the javascript
  // navigator.credentials.get, which will call on authentication code for linux
  // but not other operating systems. The authenticator code explicitly invokes
  // kDisableForRenderFrameHostCalled. This causes flakiness if we check against
  // all not restored reasons. As a result, we only check for the blocklist
  // reason.
  ExpectBlocklistedFeature(
      blink::scheduler::WebSchedulerTrackedFeature::kWebOTPService, FROM_HERE);
}

// crbug.com/1090223
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DISABLED_DoesNotCachePaymentManager) {
  ASSERT_TRUE(CreateHttpsServer()->Start());

  // 1) Navigate to a page which includes PaymentManager functionality. Note
  // that service workers are used, and therefore we use https server instead of
  // embedded_server()
  EXPECT_TRUE(NavigateToURL(
      shell(), https_server()->GetURL(
                   "a.com", "/payments/payment_app_invocation.html")));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver rfh_a_deleted(rfh_a);

  // Execute functionality that calls PaymentManager.
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    new Promise(async resolve => {
      registerPaymentApp();
      resolve();
    });
  )"));

  // 2) Navigate away.
  EXPECT_TRUE(
      NavigateToURL(shell(), https_server()->GetURL("b.com", "/title1.html")));

  // The page uses PaymentManager so it should be deleted.
  rfh_a_deleted.WaitUntilDeleted();

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::kPaymentManager}, {}, {},
      FROM_HERE);

  // Note that on Mac10.10, there is occasionally blocklisting for network
  // requests (kOutstandingNetworkRequestOthers). This causes flakiness if we
  // check against all blocklisted features. As a result, we only check for the
  // blocklist we care about.
  base::HistogramBase::Sample sample = base::HistogramBase::Sample(
      blink::scheduler::WebSchedulerTrackedFeature::kPaymentManager);
  std::vector<base::Bucket> blocklist_values = histogram_tester_.GetAllSamples(
      "BackForwardCache.HistoryNavigationOutcome."
      "BlocklistedFeature");
  auto it = std::find_if(
      blocklist_values.begin(), blocklist_values.end(),
      [sample](const base::Bucket& bucket) { return bucket.min == sample; });
  EXPECT_TRUE(it != blocklist_values.end());

  std::vector<base::Bucket> all_sites_blocklist_values =
      histogram_tester_.GetAllSamples(
          "BackForwardCache.AllSites.HistoryNavigationOutcome."
          "BlocklistedFeature");

  auto all_sites_it = std::find_if(
      all_sites_blocklist_values.begin(), all_sites_blocklist_values.end(),
      [sample](const base::Bucket& bucket) { return bucket.min == sample; });
  EXPECT_TRUE(all_sites_it != all_sites_blocklist_values.end());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoesNotCacheOnKeyboardLock) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page and start using the IdleManager class.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver rfh_a_deleted(rfh_a);

  EXPECT_TRUE(ExecJs(rfh_a, R"(
    new Promise(resolve => {
      navigator.keyboard.lock();
      resolve();
    });
  )"));

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  // The page uses IdleManager so it should be deleted.
  rfh_a_deleted.WaitUntilDeleted();

  // 3) Go back and make sure the IdleManager page wasn't in the cache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::kKeyboardLock}, {}, {},
      FROM_HERE);
}

// Tests which blocklisted features are tracked in the metrics when we used
// blocklisted features (sticky and non-sticky) and do a browser-initiated
// cross-site navigation.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       BlocklistedFeaturesTracking_CrossSite_BrowserInitiated) {
  ASSERT_TRUE(CreateHttpsServer()->Start());
  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(https_server()->GetURL("b.com", "/title2.html"));
  // 1) Navigate to a page.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  RenderFrameHostImpl* rfh_a = current_frame_host();
  scoped_refptr<SiteInstanceImpl> site_instance_a =
      static_cast<SiteInstanceImpl*>(rfh_a->GetSiteInstance());
  RenderFrameDeletedObserver rfh_a_deleted(rfh_a);

  // 2) Use BroadcastChannel (non-sticky) and KeyboardLock (sticky) blocklisted
  // features.
  EXPECT_TRUE(ExecJs(rfh_a, "window.foo = new BroadcastChannel('foo');"));
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    new Promise(resolve => {
      navigator.keyboard.lock();
      resolve();
    });
  )"));

  // 3) Navigate cross-site, browser-initiated.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // The previous page won't get into the back-forward cache because of the
  // blocklisted features. Because we used sticky blocklisted features, we will
  // not do a proactive BrowsingInstance swap, however the RFH will still change
  // and get deleted.
  rfh_a_deleted.WaitUntilDeleted();
  EXPECT_FALSE(site_instance_a->IsRelatedSiteInstance(
      web_contents()->GetMainFrame()->GetSiteInstance()));

  // 4) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // All features (sticky and non-sticky) will be tracked, because they're
  // tracked in RenderFrameHostManager::UnloadOldFrame.
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::kBroadcastChannel,
       blink::scheduler::WebSchedulerTrackedFeature::kKeyboardLock},
      {}, {}, FROM_HERE);
}

// Tests which blocklisted features are tracked in the metrics when we used
// blocklisted features (sticky and non-sticky) and do a renderer-initiated
// cross-site navigation.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    BlocklistedFeaturesTracking_CrossSite_RendererInitiated) {
  ASSERT_TRUE(CreateHttpsServer()->Start());
  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(https_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to a page.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  RenderFrameHostImpl* rfh_a = current_frame_host();
  scoped_refptr<SiteInstanceImpl> site_instance_a =
      static_cast<SiteInstanceImpl*>(rfh_a->GetSiteInstance());

  // 2) Use BroadcastChannel (non-sticky) and KeyboardLock (sticky) blocklisted
  // features.
  EXPECT_TRUE(ExecJs(rfh_a, "window.foo = new BroadcastChannel('foo');"));
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    new Promise(resolve => {
      navigator.keyboard.lock();
      resolve();
    });
  )"));

  // 3) Navigate cross-site, renderer-inititated.
  EXPECT_TRUE(ExecJs(shell(), JsReplace("location = $1;", url_b.spec())));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  // The previous page won't get into the back-forward cache because of the
  // blocklisted features. Because we used sticky blocklisted features, we will
  // not do a proactive BrowsingInstance swap.
  EXPECT_TRUE(site_instance_a->IsRelatedSiteInstance(
      web_contents()->GetMainFrame()->GetSiteInstance()));

  // 4) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  if (AreStrictSiteInstancesEnabled()) {
    // All features (sticky and non-sticky) will be tracked, because they're
    // tracked in RenderFrameHostManager::UnloadOldFrame.
    ExpectNotRestored(
        {BackForwardCacheMetrics::NotRestoredReason::
             kRelatedActiveContentsExist,
         BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures,
         BackForwardCacheMetrics::NotRestoredReason::
             kBrowsingInstanceNotSwapped},
        {blink::scheduler::WebSchedulerTrackedFeature::kBroadcastChannel,
         blink::scheduler::WebSchedulerTrackedFeature::kKeyboardLock},
        {ShouldSwapBrowsingInstance::kNo_NotNeededForBackForwardCache}, {},
        FROM_HERE);

    web_contents()->GetController().GoForward();
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

    ExpectBrowsingInstanceNotSwappedReason(
        ShouldSwapBrowsingInstance::kNo_AlreadyHasMatchingBrowsingInstance,
        FROM_HERE);

    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

    ExpectBrowsingInstanceNotSwappedReason(
        ShouldSwapBrowsingInstance::kNo_AlreadyHasMatchingBrowsingInstance,
        FROM_HERE);
  } else {
    // Non-sticky reasons are not recorded here.
    ExpectNotRestored(
        {
            BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures,
            BackForwardCacheMetrics::NotRestoredReason::
                kBrowsingInstanceNotSwapped,
        },
        {blink::scheduler::WebSchedulerTrackedFeature::kKeyboardLock},
        {ShouldSwapBrowsingInstance::kNo_NotNeededForBackForwardCache}, {},
        FROM_HERE);
  }
}

// Tests which blocklisted features are tracked in the metrics when we used
// blocklisted features (sticky and non-sticky) and do a same-site navigation.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       BlocklistedFeaturesTracking_SameSite) {
  ASSERT_TRUE(CreateHttpsServer()->Start());

  ASSERT_TRUE(CreateHttpsServer()->Start());
  GURL url_1(https_server()->GetURL("/title1.html"));
  GURL url_2(https_server()->GetURL("/title2.html"));

  // 1) Navigate to a page.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  RenderFrameHostImpl* rfh_1 = current_frame_host();
  scoped_refptr<SiteInstanceImpl> site_instance_1 =
      static_cast<SiteInstanceImpl*>(rfh_1->GetSiteInstance());

  // 2) Use BroadcastChannel (non-sticky) and KeyboardLock (sticky) blocklisted
  // features.
  EXPECT_TRUE(ExecJs(rfh_1, "window.foo = new BroadcastChannel('foo');"));
  EXPECT_TRUE(ExecJs(rfh_1, R"(
    new Promise(resolve => {
      navigator.keyboard.lock();
      resolve();
    });
  )"));

  // 3) Navigate same-site.
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Because we used sticky blocklisted features, we will not do a proactive
  // BrowsingInstance swap.
  EXPECT_TRUE(site_instance_1->IsRelatedSiteInstance(
      web_contents()->GetMainFrame()->GetSiteInstance()));

  // 4) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Non-sticky reasons are not recorded here.
  ExpectNotRestored(
      {
          BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures,
          BackForwardCacheMetrics::NotRestoredReason::
              kBrowsingInstanceNotSwapped,
      },
      {blink::scheduler::WebSchedulerTrackedFeature::kKeyboardLock},
      {ShouldSwapBrowsingInstance::kNo_NotNeededForBackForwardCache}, {},
      FROM_HERE);
}

// Tests which blocklisted features are tracked in the metrics when we used a
// non-sticky blocklisted feature and do a browser-initiated cross-site
// navigation.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    BlocklistedFeaturesTracking_CrossSite_BrowserInitiated_NonSticky) {
  ASSERT_TRUE(CreateHttpsServer()->Start());

  // 1) Navigate to an empty page.
  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(https_server()->GetURL("b.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  RenderFrameHostImpl* rfh_a = current_frame_host();
  // 2) Use BroadcastChannel (a non-sticky blocklisted feature).
  EXPECT_TRUE(ExecJs(rfh_a, "window.foo = new BroadcastChannel('foo');"));
  scoped_refptr<SiteInstanceImpl> site_instance_a =
      static_cast<SiteInstanceImpl*>(
          web_contents()->GetMainFrame()->GetSiteInstance());

  // 3) Navigate cross-site, browser-initiated.
  // The previous page won't get into the back-forward cache because of the
  // blocklisted feature.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  // Because we only used non-sticky blocklisted features, we will still do a
  // proactive BrowsingInstance swap.
  EXPECT_FALSE(site_instance_a->IsRelatedSiteInstance(
      web_contents()->GetMainFrame()->GetSiteInstance()));

  // 4) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Because the RenderFrameHostManager changed, the blocklisted features will
  // be tracked in RenderFrameHostManager::UnloadOldFrame.
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::kBroadcastChannel}, {}, {},
      FROM_HERE);
}

// Tests which blocklisted features are tracked in the metrics when we used a
// non-sticky blocklisted feature and do a renderer-initiated cross-site
// navigation.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    BlocklistedFeaturesTracking_CrossSite_RendererInitiated_NonSticky) {
  ASSERT_TRUE(CreateHttpsServer()->Start());

  // 1) Navigate to an empty page.
  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  RenderFrameHostImpl* rfh_a = current_frame_host();
  // 2) Use BroadcastChannel (a non-sticky blocklisted feature).
  EXPECT_TRUE(ExecJs(rfh_a, "window.foo = new BroadcastChannel('foo');"));
  scoped_refptr<SiteInstanceImpl> site_instance_a =
      static_cast<SiteInstanceImpl*>(
          web_contents()->GetMainFrame()->GetSiteInstance());

  // 3) Navigate cross-site, renderer-inititated.
  // The previous page won't get into the back-forward cache because of the
  // blocklisted feature.
  EXPECT_TRUE(ExecJs(shell(), JsReplace("location = $1;", url_b.spec())));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  // Because we only used non-sticky blocklisted features, we will still do a
  // proactive BrowsingInstance swap.
  EXPECT_FALSE(site_instance_a->IsRelatedSiteInstance(
      web_contents()->GetMainFrame()->GetSiteInstance()));

  // 4) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Because the RenderFrameHostManager changed, the blocklisted features will
  // be tracked in RenderFrameHostManager::UnloadOldFrame.
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::kBroadcastChannel}, {}, {},
      FROM_HERE);
}

// Tests which blocklisted features are tracked in the metrics when we used a
// non-sticky blocklisted feature and do a same-site navigation.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       BlocklistedFeaturesTracking_SameSite_NonSticky) {
  ASSERT_TRUE(CreateHttpsServer()->Start());

  // 1) Navigate to an empty page.
  GURL url_1(https_server()->GetURL("/title1.html"));
  GURL url_2(https_server()->GetURL("/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  RenderFrameHostImpl* rfh_1 = current_frame_host();
  // 2) Use BroadcastChannel (a non-sticky blocklisted feature).
  EXPECT_TRUE(ExecJs(rfh_1, "window.foo = new BroadcastChannel('foo');"));
  scoped_refptr<SiteInstanceImpl> site_instance_1 =
      static_cast<SiteInstanceImpl*>(
          web_contents()->GetMainFrame()->GetSiteInstance());

  // 3) Navigate same-site.
  // The previous page won't get into the back-forward cache because of the
  // blocklisted feature.
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  // Because we only used non-sticky blocklisted features, we will still do a
  // proactive BrowsingInstance swap.
  EXPECT_FALSE(site_instance_1->IsRelatedSiteInstance(
      web_contents()->GetMainFrame()->GetSiteInstance()));

  // 4) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Because the RenderFrameHostManager changed, the blocklisted features will
  // be tracked in RenderFrameHostManager::UnloadOldFrame.
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::kBroadcastChannel}, {}, {},
      FROM_HERE);
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

class MockAppBannerService : public blink::mojom::AppBannerService {
 public:
  MockAppBannerService() = default;
  ~MockAppBannerService() override = default;

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(mojo::PendingReceiver<blink::mojom::AppBannerService>(
        std::move(handle)));
  }

  mojo::Remote<blink::mojom::AppBannerController>& controller() {
    return controller_;
  }

  void OnBannerPromptRequested(bool) {}

  void SendBannerPromptRequest() {
    blink::mojom::AppBannerController* controller_ptr = controller_.get();
    base::OnceCallback<void(bool)> callback = base::BindOnce(
        &MockAppBannerService::OnBannerPromptRequested, base::Unretained(this));
    controller_ptr->BannerPromptRequest(
        receiver_.BindNewPipeAndPassRemote(),
        event_.BindNewPipeAndPassReceiver(), {"web"},
        base::BindOnce(&MockAppBannerService::OnBannerPromptReply,
                       base::Unretained(this), std::move(callback)));
  }

  void OnBannerPromptReply(base::OnceCallback<void(bool)> callback,
                           blink::mojom::AppBannerPromptReply reply) {
    std::move(callback).Run(reply ==
                            blink::mojom::AppBannerPromptReply::CANCEL);
  }

  // blink::mojom::AppBannerService:
  void DisplayAppBanner() override {}

 private:
  mojo::Receiver<blink::mojom::AppBannerService> receiver_{this};
  mojo::Remote<blink::mojom::AppBannerEvent> event_;
  mojo::Remote<blink::mojom::AppBannerController> controller_;

  DISALLOW_COPY_AND_ASSIGN(MockAppBannerService);
};

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, DoesNotCacheIfAppBanner) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to A and request a PWA app banner.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Connect the MockAppBannerService mojom to the renderer's frame.
  MockAppBannerService mock_app_banner_service;
  web_contents()->GetMainFrame()->GetRemoteInterfaces()->GetInterface(
      mock_app_banner_service.controller().BindNewPipeAndPassReceiver());
  // Send the request to the renderer's frame.
  mock_app_banner_service.SendBannerPromptRequest();

  RenderFrameDeletedObserver delete_observer_rfh(current_frame_host());

  // 2) Navigate away. Page A requested a PWA app banner, and thus not cached.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));
  delete_observer_rfh.WaitUntilDeleted();

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::kAppBanner}, {}, {},
      FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, DoesNotCacheIfWebDatabase) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with WebDatabase usage.
  GURL url(embedded_test_server()->GetURL("/simple_database.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver deleted(rfh_a);

  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));
  // The page uses WebDatabase so it should be deleted.
  deleted.WaitUntilDeleted();

  // 3) Go back to the page with WebDatabase.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::kWebDatabase}, {}, {},
      FROM_HERE);
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
      {BackForwardCacheMetrics::NotRestoredReason::kHTTPStatusNotOK}, {}, {},
      {}, FROM_HERE);
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
      {}, {}, FROM_HERE);
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
      {}, {}, FROM_HERE);
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
      {}, {}, FROM_HERE);
}

// Tests the events are fired when going back from the cache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, Events) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  StartRecordingEvents(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
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

class BackForwardCacheBrowserTestShouldConsiderPagehideForEligibility
    : public BackForwardCacheBrowserTest {
 public:
  BackForwardCacheBrowserTestShouldConsiderPagehideForEligibility() = default;
  ~BackForwardCacheBrowserTestShouldConsiderPagehideForEligibility() override =
      default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    check_eligibility_after_pagehide_ = true;
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestShouldConsiderPagehideForEligibility,
    DoesNotCacheIfBroadcastChannelStillOpen) {
  ASSERT_TRUE(CreateHttpsServer()->Start());

  // 1) Navigate to an empty page.
  GURL url_a(https_server()->GetURL(
      "a.com", "/back_forward_cache/page_with_broadcastchannel.html"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // 2) Use BroadcastChannel (a non-sticky blocklisted feature).
  RenderFrameHostImpl* rfh_a = current_frame_host();
  EXPECT_TRUE(ExecJs(rfh_a, "acquireBroadcastChannel();"));
  EXPECT_TRUE(ExecJs(rfh_a, "setShouldCloseChannelInPageHide(false);"));

  // 3) Navigate cross-site, browser-initiated.
  // The previous page won't get into the back-forward cache because of the
  // blocklisted feature.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // 4) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Because the RenderFrameHostManager changed, the blocklisted features will
  // be tracked in RenderFrameHostManager::UnloadOldFrame.
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::kBroadcastChannel}, {}, {},
      FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestShouldConsiderPagehideForEligibility,
    CacheIfBroadcastChannelIsClosedInPagehide) {
  ASSERT_TRUE(CreateHttpsServer()->Start());

  // 1) Navigate to an empty page.
  GURL url_a(https_server()->GetURL(
      "a.com", "/back_forward_cache/page_with_broadcastchannel.html"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  RenderFrameHostImpl* rfh_a = current_frame_host();
  // 2) Use BroadcastChannel (a non-sticky blocklisted feature).
  EXPECT_TRUE(ExecJs(rfh_a, "acquireBroadcastChannel();"));
  EXPECT_TRUE(ExecJs(rfh_a, "setShouldCloseChannelInPageHide(true);"));

  // 3) Navigate cross-site, browser-initiated.
  // The previous page won't get into the back-forward cache because of the
  // blocklisted feature.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // 4) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectRestored(FROM_HERE);
}

// Navigates from page A -> page B -> page C -> page B -> page C. Page B becomes
// ineligible for bfcache in pagehide handler, so Page A stays in bfcache
// without being evicted even after the navigation to Page C.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestShouldConsiderPagehideForEligibility,
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
      FROM_HERE);
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
  // 2) Use keyboard lock (a sticky blocklisted feature), so that the page is
  // known to be ineligible for bfcache at commit time, before we dispatch the
  // pagehide event.
  EXPECT_TRUE(ExecJs(rfh_1, R"(
    new Promise(resolve => {
      navigator.keyboard.lock();
      resolve();
    });
  )"));

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
      {}, {}, FROM_HERE);
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
      {}, {}, FROM_HERE);
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
      {}, {}, FROM_HERE);
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

// A fetch request starts during the "freeze" event. The current behavior is to
// send the request anyway. However evicting the page from the BackForwardCache
// might be a better behavior.
//
// ┌───────┐┌────────┐       ┌───────────────┐
// │browser││renderer│       │network service│
// └───┬───┘└───┬────┘       └───────┬───────┘
//     │Freeze()│                    │
//     │───────>│                    │
//     │     (onfreeze)              │
//     │        │CreateLoaderAndStart│
//     │        │───────────────────>│
//     │      (frozen)               │
// ┌───┴───┐┌───┴────┐       ┌───────┴───────┐
// │browser││renderer│       │network service│
// └───────┘└────────┘       └───────────────┘
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, FetchWhileStoring) {
  net::test_server::ControllableHttpResponse fetch_response(
      embedded_test_server(), "/fetch");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Use "fetch" immediately before being frozen.
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    document.addEventListener('freeze', event => {
      my_fetch = fetch('/fetch', { keepalive: true});
    });
  )"));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  fetch_response.WaitForRequest();
  fetch_response.Send(net::HTTP_OK, "text/html", "TheResponse");
  fetch_response.Done();

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  delete_observer_rfh_a.WaitUntilDeleted();
}

class BackForwardCacheBrowserTestWithUnfreezableLoading
    : public BackForwardCacheBrowserTest {
 public:
  BackForwardCacheBrowserTestWithUnfreezableLoading() = default;
  ~BackForwardCacheBrowserTestWithUnfreezableLoading() override = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(
        blink::features::kLoadingTasksUnfreezable, "max_buffered_bytes",
        base::NumberToString(kMaxBufferedBytesPerRequest));
    EnableFeatureAndSetParams(
        blink::features::kLoadingTasksUnfreezable,
        "max_buffered_bytes_per_process",
        base::NumberToString(kMaxBufferedBytesPerProcess));
    EnableFeatureAndSetParams(
        blink::features::kLoadingTasksUnfreezable,
        "grace_period_to_finish_loading_in_seconds",
        base::NumberToString(kGracePeriodToFinishLoading.InSeconds()));
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }

  // Navigates to a page at |page_url| with an img element with src set to
  // "image.png".
  RenderFrameHostImpl* NavigateToPageWithImage(const GURL& page_url) {
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

  const int kMaxBufferedBytesPerRequest = 7000;
  const int kMaxBufferedBytesPerProcess = 10000;
  const base::TimeDelta kGracePeriodToFinishLoading =
      base::TimeDelta::FromSeconds(5);
};

// When loading task is unfreezable with the feature flag
// kLoadingTaskUnfreezable, a page will keep processing the in-flight network
// requests while the page is frozen in BackForwardCache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithUnfreezableLoading,
                       FetchWhileStoring) {
  net::test_server::ControllableHttpResponse fetch_response(
      embedded_test_server(), "/fetch");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Use "fetch" immediately before being frozen.
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    document.addEventListener('freeze', event => {
      my_fetch = fetch('/fetch', { keepalive: true});
    });
  )"));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  fetch_response.WaitForRequest();
  fetch_response.Send(net::HTTP_OK, "text/html", "TheResponse");
  fetch_response.Done();
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(delete_observer_rfh_a.deleted());

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectRestored(FROM_HERE);
}

// Eviction is triggered when a normal fetch request gets redirected while the
// page is in back-forward cache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithUnfreezableLoading,
                       FetchRedirectedWhileStoring) {
  net::test_server::ControllableHttpResponse fetch_response(
      embedded_test_server(), "/fetch");
  net::test_server::ControllableHttpResponse fetch2_response(
      embedded_test_server(), "/fetch2");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Trigger a fetch.
  ExecuteScriptAsync(rfh_a, "my_fetch = fetch('/fetch');");

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // Page A is initially stored in the back-forward cache.
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Respond the fetch with a redirect.
  fetch_response.WaitForRequest();
  fetch_response.Send(
      "HTTP/1.1 302 Moved Temporarily\r\n"
      "Location: /fetch2");
  fetch_response.Done();

  // Ensure that the request to /fetch2 was never sent (because the page is
  // immediately evicted) by checking after 3 seconds.
  base::RunLoop loop;
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, base::TimeDelta::FromSeconds(3), loop.QuitClosure());
  loop.Run();
  EXPECT_EQ(nullptr, fetch2_response.http_request());

  // Page A should be evicted from the back-forward cache.
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kNetworkRequestRedirected},
      {}, {}, {}, FROM_HERE);
}

// Eviction is triggered when a keepalive fetch request gets redirected while
// the page is in back-forward cache.
// TODO(https://crbug.com/1137682): We should not trigger eviction on redirects
// of keepalive fetches.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithUnfreezableLoading,
                       KeepAliveFetchRedirectedWhileStoring) {
  net::test_server::ControllableHttpResponse fetch_response(
      embedded_test_server(), "/fetch");
  net::test_server::ControllableHttpResponse fetch2_response(
      embedded_test_server(), "/fetch2");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Trigger a keepalive fetch.
  ExecuteScriptAsync(rfh_a, "my_fetch = fetch('/fetch', { keepalive: true });");

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // Page A is initially stored in the back-forward cache.
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Respond the fetch with a redirect.
  fetch_response.WaitForRequest();
  fetch_response.Send(
      "HTTP/1.1 302 Moved Temporarily\r\n"
      "Location: /fetch2");
  fetch_response.Done();

  // Ensure that the request to /fetch2 was never sent (because the page is
  // immediately evicted) by checking after 3 seconds.
  // TODO(https://crbug.com/1137682): We should not trigger eviction on
  // redirects of keepalive fetches and the redirect request should be sent.
  base::RunLoop loop;
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, base::TimeDelta::FromSeconds(3), loop.QuitClosure());
  loop.Run();
  EXPECT_EQ(nullptr, fetch2_response.http_request());

  // Page A should be evicted from the back-forward cache.
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kNetworkRequestRedirected},
      {}, {}, {}, FROM_HERE);
}

// Tests the case when the header was received before the page is frozen,
// but parts of the response body is received when the page is frozen.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithUnfreezableLoading,
                       PageWithDrainedDatapipeRequestsForFetchShouldBeEvicted) {
  net::test_server::ControllableHttpResponse fetch_response(
      embedded_test_server(), "/fetch");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Call fetch before navigating away.
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    var fetch_response_promise = my_fetch = fetch('/fetch').then(response => {
        return response.text();
    });
  )"));
  // Send response header and a piece of the body before navigating away.
  fetch_response.WaitForRequest();
  fetch_response.Send(net::HTTP_OK, "text/plain");
  fetch_response.Send("body");

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kNetworkRequestDatapipeDrainedAsBytesConsumer},
                    {}, {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestWithUnfreezableLoading,
    PageWithDrainedDatapipeRequestsForScriptStreamerShouldNotBeEvicted) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/small_script.js");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/empty.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/empty.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  // Append the script tag.
  EXPECT_TRUE(ExecJs(shell(), R"(
    var script = document.createElement('script');
    script.src = 'small_script.js'
    document.body.appendChild(script);
  )"));

  response.WaitForRequest();
  // Send the small_script.js but not complete, so that the datapipe is passed
  // to ScriptStreamer upon bfcache entrance.
  const char kHttpResponseHeader[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n";
  response.Send(kHttpResponseHeader);
  response.Send("alert('more than 4 bytes');");

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  // Complete the response after navigating away.
  response.Send("alert('more than 4 bytes');");
  response.Done();

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestWithUnfreezableLoading,
    PageWithDrainedDatapipeRequestsForScriptStreamerShouldBeEvictedIfStreamedTooMuch) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/small_script.js");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/empty.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/empty.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  // Append the script tag.
  EXPECT_TRUE(ExecJs(shell(), R"(
    var script = document.createElement('script');
    script.src = 'small_script.js'
    document.body.appendChild(script);
  )"));

  response.WaitForRequest();
  // Send the small_script.js but not complete, so that the datapipe is passed
  // to ScriptStreamer upon bfcache entrance.
  const char kHttpResponseHeader[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n";
  response.Send(kHttpResponseHeader);
  response.Send("alert('more than 4 bytes');");

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // Complete the response after navigating away.
  std::string body(kMaxBufferedBytesPerRequest + 1, '*');
  response.Send(body);
  response.Done();

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kNetworkExceedsBufferLimit},
      {}, {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithUnfreezableLoading,
                       ImageStillLoading_ResponseStartedWhileFrozen) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an image with src == "image.png".
  RenderFrameHostImpl* rfh_1 = NavigateToPageWithImage(
      embedded_test_server()->GetURL("a.com", "/title1.html"));
  image_response.WaitForRequest();

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  // The page was still loading when we navigated away, but it's still eligible
  // for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  // Start sending the image body while in the back-forward cache.
  image_response.Send(net::HTTP_OK, "image/png");
  image_response.Send("image_body");
  image_response.Done();

  // 3) Go back to the first page. We should restore the page from the
  // back-forward cache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectRestored(FROM_HERE);

  // Wait until the deferred body is processed. Since it's not a valid image
  // value, we'll get the "error" event.
  EXPECT_EQ("error", EvalJs(rfh_1, "image_load_status"));
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestWithUnfreezableLoading,
    ImageStillLoading_ResponseStartedWhileFrozen_ExceedsPerRequestBytesLimit) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an image with src == "image.png".
  RenderFrameHostImpl* rfh_1 = NavigateToPageWithImage(
      embedded_test_server()->GetURL("a.com", "/title1.html"));

  // Wait for the image request, but don't send anything yet.
  image_response.WaitForRequest();

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  // The page was still loading when we navigated away, but it's still eligible
  // for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  RenderFrameDeletedObserver delete_observer(rfh_1);
  // Start sending the image response while in the back-forward cache.
  image_response.Send(net::HTTP_OK, "image/png");
  std::string body(kMaxBufferedBytesPerRequest + 1, '*');
  image_response.Send(body);
  image_response.Done();
  delete_observer.WaitUntilDeleted();

  // 3) Go back to the first page. We should not restore the page from the
  // back-forward cache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kNetworkExceedsBufferLimit},
      {}, {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestWithUnfreezableLoading,
    ImageStillLoading_ResponseStartedWhileRestoring_DoNotTriggerEviction) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an image with src == "image.png".
  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  RenderFrameHostImpl* rfh_1 = NavigateToPageWithImage(url);

  // Wait for the image request, but don't send anything yet.
  image_response.WaitForRequest();

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  // The page was still loading when we navigated away, but it's still eligible
  // for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  // 3) Go back to the first page using TestNavigationManager so that we split
  // the navigation into stages.
  TestNavigationManager navigation_manager_back(shell()->web_contents(), url);
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(navigation_manager_back.WaitForResponse());

  // Before we try to commit the navigation, BFCache will defer to wait
  // asynchronously for renderers to reply that they've unfrozen. Finish the
  // image response in that time.
  navigation_manager_back.ResumeNavigation();
  ASSERT_TRUE(
      NavigationRequest::From(navigation_manager_back.GetNavigationHandle())
          ->IsCommitDeferringConditionDeferredForTesting());
  ASSERT_FALSE(navigation_manager_back.GetNavigationHandle()->HasCommitted());

  image_response.Send(net::HTTP_OK, "image/png");
  std::string body(kMaxBufferedBytesPerRequest + 1, '*');
  image_response.Send(body);
  image_response.Done();

  // Finish the navigation.
  navigation_manager_back.WaitForNavigationFinished();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestWithUnfreezableLoading,
    ImageStillLoading_ResponseStartedWhileFrozen_ExceedsPerProcessBytesLimit) {
  net::test_server::ControllableHttpResponse image1_response(
      embedded_test_server(), "/image1.png");
  net::test_server::ControllableHttpResponse image2_response(
      embedded_test_server(), "/image2.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with 2 images.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  RenderFrameHostImpl* rfh_1 = current_frame_host();
  // Wait for the document to load DOM to ensure that kLoading is not
  // one of the reasons why the document wasn't cached.
  WaitForDOMContentLoaded(rfh_1);

  EXPECT_TRUE(ExecJs(rfh_1, R"(
      var image1 = document.createElement("img");
      image1.src = "image1.png";
      document.body.appendChild(image1);
      var image2 = document.createElement("img");
      image2.src = "image2.png";
      document.body.appendChild(image1);

      var image1_load_status = new Promise((resolve, reject) => {
        image1.onload = () => { resolve("loaded"); }
        image1.onerror = () => { resolve("error"); }
      });

      var image2_load_status = new Promise((resolve, reject) => {
        image2.onload = () => { resolve("loaded"); }
        image2.onerror = () => { resolve("error"); }
      });
    )"));

  // Wait for the image requests, but don't send anything yet.
  image1_response.WaitForRequest();
  image2_response.WaitForRequest();

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  // The page was still loading when we navigated away, but it's still eligible
  // for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  RenderFrameDeletedObserver delete_observer(rfh_1);
  // Start sending the image responses while in the back-forward cache. The
  // body size of the responses individually is less than the per-request limit,
  // but together they surpass the per-process limit.
  const int image_body_size = kMaxBufferedBytesPerProcess / 2 + 1;
  DCHECK_LT(image_body_size, kMaxBufferedBytesPerRequest);
  std::string body(image_body_size, '*');
  image1_response.Send(net::HTTP_OK, "image/png");
  image1_response.Send(body);
  image1_response.Done();
  image2_response.Send(net::HTTP_OK, "image/png");
  image2_response.Send(body);
  image2_response.Done();
  delete_observer.WaitUntilDeleted();

  // 3) Go back to the first page. We should not restore the page from the
  // back-forward cache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kNetworkExceedsBufferLimit},
      {}, {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestWithUnfreezableLoading,
    ImageStillLoading_ResponseStartedWhileFrozen_ExceedsPerProcessBytesLimit_SameSiteSubframe) {
  net::test_server::ControllableHttpResponse image1_response(
      embedded_test_server(), "/image1.png");
  net::test_server::ControllableHttpResponse image2_response(
      embedded_test_server(), "/image2.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate main frame to a page with 1 image.
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "a.com", "/page_with_iframe.html")));
  RenderFrameHostImpl* main_rfh = current_frame_host();
  // Wait for the document to load DOM to ensure that kLoading is not
  // one of the reasons why the document wasn't cached.
  WaitForDOMContentLoaded(main_rfh);

  EXPECT_TRUE(ExecJs(main_rfh, R"(
      var image1 = document.createElement("img");
      image1.src = "image1.png";
      document.body.appendChild(image1);
      var image1_load_status = new Promise((resolve, reject) => {
        image1.onload = () => { resolve("loaded"); }
        image1.onerror = () => { resolve("error"); }
      });
    )"));

  // 2) Add 1 image to the subframe.
  RenderFrameHostImpl* subframe_rfh =
      main_rfh->child_at(0)->current_frame_host();

  // First, wait for the subframe document to load DOM to ensure that kLoading
  // is not one of the reasons why the document wasn't cached.
  WaitForDOMContentLoaded(subframe_rfh);

  EXPECT_TRUE(ExecJs(subframe_rfh, R"(
      var image2 = document.createElement("img");
      image2.src = "image2.png";
      document.body.appendChild(image2);
      var image2_load_status = new Promise((resolve, reject) => {
        image2.onload = () => { resolve("loaded"); }
        image2.onerror = () => { resolve("error"); }
      });
    )"));

  // Wait for the image requests, but don't send anything yet.
  image1_response.WaitForRequest();
  image2_response.WaitForRequest();

  // 3) Navigate away on the main frame.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  // The page was still loading images when we navigated away, but it's still
  // eligible for back-forward cache.
  EXPECT_TRUE(main_rfh->IsInBackForwardCache());
  EXPECT_TRUE(subframe_rfh->IsInBackForwardCache());

  RenderFrameDeletedObserver delete_observer_1(main_rfh);
  RenderFrameDeletedObserver delete_observer_2(subframe_rfh);
  // Start sending the image responses while in the back-forward cache. The
  // body size of the responses individually is less than the per-request limit,
  // but together they surpass the per-process limit since both the main frame
  // and the subframe are put in the same renderer process (because they're
  // same-site).
  const int image_body_size = kMaxBufferedBytesPerProcess / 2 + 1;
  DCHECK_LT(image_body_size, kMaxBufferedBytesPerRequest);
  std::string body(image_body_size, '*');
  image1_response.Send(net::HTTP_OK, "image/png");
  image1_response.Send(body);
  image1_response.Done();
  image2_response.Send(net::HTTP_OK, "image/png");
  image2_response.Send(body);
  image2_response.Done();
  delete_observer_1.WaitUntilDeleted();
  delete_observer_2.WaitUntilDeleted();

  // 3) Go back to the first page. We should not restore the page from the
  // back-forward cache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kNetworkExceedsBufferLimit},
      {}, {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestWithUnfreezableLoading,
    ImageStillLoading_ResponseStartedWhileFrozen_ExceedsPerProcessBytesLimit_ResetOnRestore) {
  net::test_server::ControllableHttpResponse image1_response(
      embedded_test_server(), "/image.png");
  net::test_server::ControllableHttpResponse image2_response(
      embedded_test_server(), "/image2.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an image with src == "image.png".
  RenderFrameHostImpl* rfh_1 = NavigateToPageWithImage(
      embedded_test_server()->GetURL("a.com", "/title1.html"));

  // Wait for the image request, but don't send anything yet.
  image1_response.WaitForRequest();

  // 2) Navigate away on the main frame.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title2.html")));
  RenderFrameHostImpl* rfh_2 = current_frame_host();
  WaitForDOMContentLoaded(rfh_2);

  // The first page was still loading images when we navigated away, but it's
  // still eligible for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  // 3) Add 1 image to the second page.
  EXPECT_TRUE(ExecJs(rfh_2, R"(
      var image2 = document.createElement("img");
      image2.src = "image2.png";
      document.body.appendChild(image2);
      var image2_load_status = new Promise((resolve, reject) => {
        image2.onload = () => { resolve("loaded"); }
        image2.onerror = () => { resolve("error"); }
      });
    )"));
  image2_response.WaitForRequest();

  // Start sending the image response for the first page while in the
  // back-forward cache. The body size of the response is half of the
  // per-process limit.
  const int image_body_size = kMaxBufferedBytesPerProcess / 2 + 1;
  DCHECK_LT(image_body_size, kMaxBufferedBytesPerRequest);
  std::string body(image_body_size, '*');
  image1_response.Send(net::HTTP_OK, "image/png");
  image1_response.Send(body);
  image1_response.Done();

  // 4) Go back to the first page. We should restore the page from the
  // back-forward cache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectRestored(FROM_HERE);

  // The second page was still loading images when we navigated away, but it's
  // still eligible for back-forward cache.
  EXPECT_TRUE(rfh_2->IsInBackForwardCache());

  // Start sending the image response for the second page's image request.
  // The second page should still stay in the back-forward cache since the
  // per-process buffer limit is reset back to 0 after the first page gets
  // restored from the back-forward cache, so we wouldn't go over the
  // per-process buffer limit even when the total body size buffered during the
  // lifetime of the test actually exceeds the per-process buffer limit.
  image2_response.Send(net::HTTP_OK, "image/png");
  image2_response.Send(body);
  image2_response.Done();

  EXPECT_TRUE(rfh_2->IsInBackForwardCache());

  // 5) Go forward. We should restore the second page from the back-forward
  // cache.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestWithUnfreezableLoading,
    ImageStillLoading_ResponseStartedWhileFrozen_ExceedsPerProcessBytesLimit_ResetOnDetach) {
  net::test_server::ControllableHttpResponse image1_response(
      embedded_test_server(), "/image.png");
  net::test_server::ControllableHttpResponse image2_response(
      embedded_test_server(), "/image2.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an image with src == "image.png".
  RenderFrameHostImpl* rfh_1 = NavigateToPageWithImage(
      embedded_test_server()->GetURL("a.com", "/title1.html"));

  // Wait for the image request, but don't send anything yet.
  image1_response.WaitForRequest();

  // 2) Navigate away on the main frame.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title2.html")));
  RenderFrameHostImpl* rfh_2 = current_frame_host();
  WaitForDOMContentLoaded(rfh_2);

  // The first page was still loading images when we navigated away, but it's
  // still eligible for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  // 3) Add 1 image to the second page.
  EXPECT_TRUE(ExecJs(rfh_2, R"(
      var image2 = document.createElement("img");
      image2.src = "image2.png";
      document.body.appendChild(image2);
      var image2_load_status = new Promise((resolve, reject) => {
        image2.onload = () => { resolve("loaded"); }
        image2.onerror = () => { resolve("error"); }
      });
    )"));
  image2_response.WaitForRequest();

  RenderFrameDeletedObserver delete_observer_1(rfh_1);
  // Start sending an image response that's larger than the per-process and
  // per-request buffer limit, causing the page to get evicted from the
  // back-forward cache.
  std::string body(kMaxBufferedBytesPerProcess + 1, '*');
  image1_response.Send(net::HTTP_OK, "image/png");
  image1_response.Send(body);
  image1_response.Done();
  delete_observer_1.WaitUntilDeleted();

  // 4) Go back to the first page. We should not restore the page from the
  // back-forward cache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kNetworkExceedsBufferLimit},
      {}, {}, {}, FROM_HERE);

  // The second page was still loading images when we navigated away, but it's
  // still eligible for back-forward cache.
  EXPECT_TRUE(rfh_2->IsInBackForwardCache());

  // Start sending a small image response for the second page's image request.
  // The second page should still stay in the back-forward cache since the
  // per-process buffer limit is reset back to 0 after the first page gets
  // evicted and deleted
  image2_response.Send(net::HTTP_OK, "image/png");
  image2_response.Send("*");
  image2_response.Done();

  EXPECT_TRUE(rfh_2->IsInBackForwardCache());

  // 5) Go forward. We should restore the second page from the back-forward
  // cache.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectRestored(FROM_HERE);

  // Wait until the deferred body is processed. Since it's not a valid image
  // value, we'll get the "error" event.
  EXPECT_EQ("error", EvalJs(rfh_2, "image2_load_status"));
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithUnfreezableLoading,
                       ImageStillLoading_ResponseStartedWhileFrozen_Timeout) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an image with src == "image.png".
  RenderFrameHostImpl* rfh_1 = NavigateToPageWithImage(
      embedded_test_server()->GetURL("a.com", "/title1.html"));

  // Wait for the image request, but don't send anything yet.
  image_response.WaitForRequest();

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  // The page was still loading when we navigated away, but it's still eligible
  // for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  RenderFrameDeletedObserver delete_observer(rfh_1);
  // Start sending the image response while in the back-forward cache, but never
  // finish the request. Eventually the page will get deleted due to network
  // request timeout.
  image_response.Send(net::HTTP_OK, "image/png");
  std::string body(kMaxBufferedBytesPerRequest + 1, '*');
  delete_observer.WaitUntilDeleted();

  // 3) Go back to the first page. We should not restore the page from the
  // back-forward cache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kNetworkRequestTimeout}, {},
      {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestWithUnfreezableLoading,
    ImageStillLoading_ResponseStartedBeforeFreezing_ExceedsPerRequestBytesLimit) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an image with src == "image.png".
  RenderFrameHostImpl* rfh_1 = NavigateToPageWithImage(
      embedded_test_server()->GetURL("a.com", "/title1.html"));

  // Start sending response before the page gets in the back-forward cache.
  image_response.WaitForRequest();
  image_response.Send(net::HTTP_OK, "image/png");
  image_response.Send(" ");
  // Run some script to ensure the renderer processed its pending tasks.
  EXPECT_TRUE(ExecJs(rfh_1, "var foo = 42;"));

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  // The page was still loading when we navigated away, but it's still eligible
  // for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  // Send the image response body while in the back-forward cache.
  RenderFrameDeletedObserver delete_observer(rfh_1);
  std::string body(kMaxBufferedBytesPerRequest + 1, '*');
  image_response.Send(body);
  image_response.Done();
  delete_observer.WaitUntilDeleted();

  // 3) Go back to the first page. We should not restore the page from the
  // back-forward cache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kNetworkExceedsBufferLimit},
      {}, {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestWithUnfreezableLoading,
    ImageStillLoading_ResponseStartedBeforeFreezing_ExceedsPerProcessBytesLimit) {
  net::test_server::ControllableHttpResponse image1_response(
      embedded_test_server(), "/image1.png");
  net::test_server::ControllableHttpResponse image2_response(
      embedded_test_server(), "/image2.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with 2 images.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  RenderFrameHostImpl* rfh_1 = current_frame_host();
  // Wait for the document to load DOM to ensure that kLoading is not
  // one of the reasons why the document wasn't cached.
  WaitForDOMContentLoaded(rfh_1);

  EXPECT_TRUE(ExecJs(rfh_1, R"(
      var image1 = document.createElement("img");
      image1.src = "image1.png";
      document.body.appendChild(image1);
      var image2 = document.createElement("img");
      image2.src = "image2.png";
      document.body.appendChild(image1);

      var image1_load_status = new Promise((resolve, reject) => {
        image1.onload = () => { resolve("loaded"); }
        image1.onerror = () => { resolve("error"); }
      });

      var image2_load_status = new Promise((resolve, reject) => {
        image2.onload = () => { resolve("loaded"); }
        image2.onerror = () => { resolve("error"); }
      });
    )"));

  // Wait for the image requests, but don't send anything yet.

  // Start sending response before the page gets in the back-forward cache.
  image1_response.WaitForRequest();
  image1_response.Send(net::HTTP_OK, "image/png");
  image1_response.Send(" ");
  image2_response.WaitForRequest();
  image2_response.Send(net::HTTP_OK, "image/png");
  image2_response.Send(" ");
  // Run some script to ensure the renderer processed its pending tasks.
  EXPECT_TRUE(ExecJs(rfh_1, "var foo = 42;"));

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  // The page was still loading when we navigated away, but it's still eligible
  // for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  RenderFrameDeletedObserver delete_observer(rfh_1);
  // Send the image response body while in the back-forward cache. The body size
  // of the responses individually is less than the per-request limit, but
  // together they surpass the per-process limit.
  const int image_body_size = kMaxBufferedBytesPerProcess / 2 + 1;
  DCHECK_LT(image_body_size, kMaxBufferedBytesPerRequest);
  std::string body(image_body_size, '*');
  image1_response.Send(body);
  image1_response.Done();
  image2_response.Send(body);
  image2_response.Done();
  delete_observer.WaitUntilDeleted();

  // 3) Go back to the first page. We should not restore the page from the
  // back-forward cache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kNetworkExceedsBufferLimit},
      {}, {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithUnfreezableLoading,
                       TimeoutNotTriggeredAfterDone) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());
  // 1) Navigate to a page with an image with src == "image.png".
  RenderFrameHostImpl* rfh_1 = NavigateToPageWithImage(
      embedded_test_server()->GetURL("a.com", "/title1.html"));

  // Wait for the image request, but don't send anything yet.
  image_response.WaitForRequest();

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  // The page was still loading when we navigated away, but it's still eligible
  // for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  RenderFrameDeletedObserver delete_observer(rfh_1);
  // Start sending the image response while in the back-forward cache and finish
  // the request before the active request timeout hits.
  image_response.Send(net::HTTP_OK, "image/png");
  image_response.Send(" ");
  image_response.Done();

  // Make sure enough time passed to trigger network request eviction if the
  // load above didn't finish.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      kGracePeriodToFinishLoading + base::TimeDelta::FromSeconds(1));
  run_loop.Run();

  // Ensure that the page is still in bfcache.
  EXPECT_FALSE(delete_observer.deleted());
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  // 3) Go back to the first page. We should restore the page from the
  // back-forward cache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestWithUnfreezableLoading,
    TimeoutNotTriggeredAfterDone_ResponseStartedBeforeFreezing) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());
  // 1) Navigate to a page with an image with src == "image.png".
  RenderFrameHostImpl* rfh_1 = NavigateToPageWithImage(
      embedded_test_server()->GetURL("a.com", "/title1.html"));

  // Start sending response before the page gets in the back-forward cache.
  image_response.WaitForRequest();
  image_response.Send(net::HTTP_OK, "image/png");
  image_response.Send(" ");

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  // The page was still loading when we navigated away, but it's still eligible
  // for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  RenderFrameDeletedObserver delete_observer(rfh_1);
  // Finish the request before the active request timeout hits.
  image_response.Done();

  // Make sure enough time passed to trigger network request eviction if the
  // load above didn't finish.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      kGracePeriodToFinishLoading + base::TimeDelta::FromSeconds(1));
  run_loop.Run();

  // Ensure that the page is still in bfcache.
  EXPECT_FALSE(delete_observer.deleted());
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  // 3) Go back to the first page. We should restore the page from the
  // back-forward cache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithUnfreezableLoading,
                       ImageStillLoading_ResponseStartedBeforeFreezing) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an image with src == "image.png".
  RenderFrameHostImpl* rfh_1 = NavigateToPageWithImage(
      embedded_test_server()->GetURL("a.com", "/title1.html"));

  // Start sending response before the page gets in the back-forward cache.
  image_response.WaitForRequest();
  image_response.Send(net::HTTP_OK, "image/png");
  image_response.Send(" ");
  // Run some script to ensure the renderer processed its pending tasks.
  EXPECT_TRUE(ExecJs(rfh_1, "var foo = 42;"));

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  // The page was still loading when we navigated away, but it's still eligible
  // for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  // Send body while in the back-forward cache.
  image_response.Send("image_body");
  image_response.Done();

  // 3) Go back to the first page. We should restore the page from the
  // back-forward cache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectRestored(FROM_HERE);

  // Wait until the deferred body is processed. Since it's not a valid image
  // value, we'll get the "error" event.
  EXPECT_EQ("error", EvalJs(rfh_1, "image_load_status"));
}

// Disabled on Android, since we have problems starting up the websocket test
// server in the host
#if defined(OS_ANDROID)
#define MAYBE_WebSocketNotCached DISABLED_WebSocketNotCached
#else
#define MAYBE_WebSocketNotCached WebSocketNotCached
#endif
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, MAYBE_WebSocketNotCached) {
  net::SpawnedTestServer ws_server(net::SpawnedTestServer::TYPE_WS,
                                   net::GetWebSocketTestDataDirectory());
  ASSERT_TRUE(ws_server.Start());

  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Open a WebSocket.
  const char script[] = R"(
      new Promise(resolve => {
        const socket = new WebSocket($1);
        socket.addEventListener('open', () => resolve());
      });)";
  ASSERT_TRUE(ExecJs(
      rfh_a, JsReplace(script, ws_server.GetURL("echo-with-no-extension"))));

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // Confirm A is evicted.
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
    rfh.WaitUntilRenderFrameDeleted();
  }
}

namespace {

void RegisterServiceWorker(RenderFrameHostImpl* rfh) {
  EXPECT_EQ("success", EvalJs(rfh, R"(
    let controller_changed_promise = new Promise(resolve_controller_change => {
      navigator.serviceWorker.oncontrollerchange = resolve_controller_change;
    });

    new Promise(async resolve => {
      try {
        await navigator.serviceWorker.register(
          "./service-worker.js", {scope: "./"})
      } catch (e) {
        resolve("error: registration has failed");
      }

      await controller_changed_promise;

      if (navigator.serviceWorker.controller) {
        resolve("success");
      } else {
        resolve("error: not controlled by service worker");
      }
    });
  )"));
}

// Returns a unique script for each request, to test service worker update.
std::unique_ptr<net::test_server::HttpResponse> RequestHandlerForUpdateWorker(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != "/back_forward_cache/service-worker.js")
    return nullptr;
  static int counter = 0;
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  const char script[] = R"(
    // counter = $1
    self.addEventListener('activate', function(event) {
      event.waitUntil(self.clients.claim());
    });
  )";
  http_response->set_content(JsReplace(script, counter++));
  http_response->set_content_type("text/javascript");
  http_response->AddCustomHeader("Cache-Control",
                                 "no-cache, no-store, must-revalidate");
  return http_response;
}

}  // namespace

class BackForwardCacheBrowserTestWithVibration
    : public BackForwardCacheBrowserTest,
      public device::mojom::VibrationManager {
 public:
  BackForwardCacheBrowserTestWithVibration() {
    OverrideVibrationManagerBinderForTesting(base::BindRepeating(
        &BackForwardCacheBrowserTestWithVibration::BindVibrationManager,
        base::Unretained(this)));
  }

  ~BackForwardCacheBrowserTestWithVibration() override {
    OverrideVibrationManagerBinderForTesting(base::NullCallback());
  }

  void BindVibrationManager(
      mojo::PendingReceiver<device::mojom::VibrationManager> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  bool TriggerVibrate(RenderFrameHostImpl* rfh,
                      int duration,
                      base::OnceClosure vibrate_done) {
    vibrate_done_ = std::move(vibrate_done);
    return EvalJs(rfh, JsReplace("navigator.vibrate($1)", duration))
        .ExtractBool();
  }

  bool TriggerShortVibrationSequence(RenderFrameHostImpl* rfh,
                                     base::OnceClosure vibrate_done) {
    vibrate_done_ = std::move(vibrate_done);
    return EvalJs(rfh, "navigator.vibrate([10] * 1000)").ExtractBool();
  }

  bool IsCancelled() { return cancelled_; }

 private:
  // device::mojom::VibrationManager:
  void Vibrate(int64_t milliseconds, VibrateCallback callback) override {
    cancelled_ = false;
    std::move(callback).Run();
    std::move(vibrate_done_).Run();
  }

  void Cancel(CancelCallback callback) override {
    cancelled_ = true;
    std::move(callback).Run();
  }

  bool cancelled_ = false;
  base::OnceClosure vibrate_done_;
  mojo::Receiver<device::mojom::VibrationManager> receiver_{this};
};

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithVibration,
                       VibrationStopsAfterEnteringCache) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with a long vibration.
  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  base::RunLoop run_loop;
  RenderFrameHostImpl* rfh_a = current_frame_host();
  ASSERT_TRUE(TriggerVibrate(rfh_a, 10000, run_loop.QuitClosure()));
  EXPECT_FALSE(IsCancelled());

  // 2) Navigate away and expect the vibration to be canceled.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));
  EXPECT_NE(current_frame_host(), rfh_a);
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(IsCancelled());

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithVibration,
                       ShortVibrationSequenceStopsAfterEnteringCache) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with a long vibration.
  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  base::RunLoop run_loop;
  RenderFrameHostImpl* rfh_a = current_frame_host();
  ASSERT_TRUE(TriggerShortVibrationSequence(rfh_a, run_loop.QuitClosure()));
  EXPECT_FALSE(IsCancelled());

  // 2) Navigate away and expect the vibration to be canceled.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));
  EXPECT_NE(current_frame_host(), rfh_a);
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(IsCancelled());

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       CachedPagesWithServiceWorkers) {
  CreateHttpsServer();
  SetupCrossSiteRedirector(https_server());
  ASSERT_TRUE(https_server()->Start());

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(
      shell(),
      https_server()->GetURL("a.com", "/back_forward_cache/empty.html")));

  // Register a service worker.
  RegisterServiceWorker(current_frame_host());

  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver deleted(rfh_a);

  // 2) Navigate away.
  EXPECT_TRUE(
      NavigateToURL(shell(), https_server()->GetURL("b.com", "/title1.html")));

  EXPECT_FALSE(deleted.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Go back to A. The navigation should be served from the cache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_FALSE(deleted.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       EvictIfCacheBlocksServiceWorkerVersionActivation) {
  CreateHttpsServer();
  https_server()->RegisterRequestHandler(
      base::BindRepeating(&RequestHandlerForUpdateWorker));
  SetupCrossSiteRedirector(https_server());
  ASSERT_TRUE(https_server()->Start());
  Shell* tab_x = shell();
  Shell* tab_y = CreateBrowser();
  // 1) Navigate to A in tab X.
  EXPECT_TRUE(NavigateToURL(
      tab_x,
      https_server()->GetURL("a.com", "/back_forward_cache/empty.html")));
  // 2) Register a service worker.
  RegisterServiceWorker(current_frame_host());

  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver deleted(rfh_a);
  // 3) Navigate away to B in tab X.
  EXPECT_TRUE(
      NavigateToURL(tab_x, https_server()->GetURL("b.com", "/title1.html")));
  EXPECT_FALSE(deleted.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  // 4) Navigate to A in tab Y.
  EXPECT_TRUE(NavigateToURL(
      tab_y,
      https_server()->GetURL("a.com", "/back_forward_cache/empty.html")));
  // 5) Close tab Y to activate a service worker version.
  // This should evict |rfh_a| from the cache.
  tab_y->Close();
  deleted.WaitUntilDeleted();
  // 6) Navigate to A in tab X.
  tab_x->web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(tab_x->web_contents()));
  ExpectNotRestored(
      {
          BackForwardCacheMetrics::NotRestoredReason::
              kServiceWorkerVersionActivation,
      },
      {}, {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       EvictWithPostMessageToCachedClient) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.RegisterRequestHandler(
      base::BindRepeating(&RequestHandlerForUpdateWorker));
  https_server.AddDefaultHandlers(GetTestDataFilePath());
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());
  Shell* tab_to_execute_service_worker = shell();
  Shell* tab_to_be_bfcached = CreateBrowser();

  // Observe the new WebContents to trace the navigtion ID.
  WebContentsObserver::Observe(tab_to_be_bfcached->web_contents());

  // 1) Navigate to A in |tab_to_execute_service_worker|.
  EXPECT_TRUE(NavigateToURL(
      tab_to_execute_service_worker,
      https_server.GetURL(
          "a.com", "/back_forward_cache/service_worker_post_message.html")));

  // 2) Register a service worker.
  EXPECT_EQ("DONE", EvalJs(tab_to_execute_service_worker,
                           "register('service_worker_post_message.js')"));

  // 3) Navigate to A in |tab_to_be_bfcached|.
  EXPECT_TRUE(NavigateToURL(
      tab_to_be_bfcached,
      https_server.GetURL(
          "a.com", "/back_forward_cache/service_worker_post_message.html")));
  const std::string script_to_store =
      "executeCommandOnServiceWorker('StoreClients')";
  EXPECT_EQ("DONE", EvalJs(tab_to_execute_service_worker, script_to_store));
  RenderFrameHostImplWrapper rfh(
      tab_to_be_bfcached->web_contents()->GetMainFrame());

  // 4) Navigate away to B in |tab_to_be_bfcached|.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached,
                            https_server.GetURL("b.com", "/title1.html")));
  EXPECT_FALSE(rfh.IsDestroyed());
  EXPECT_TRUE(rfh->IsInBackForwardCache());

  // 5) Trigger client.postMessage via |tab_to_execute_service_worker|. Cache in
  // |tab_to_be_bfcached| will be evicted.
  const std::string script_to_post_message =
      "executeCommandOnServiceWorker('PostMessageToStoredClients')";
  EXPECT_EQ("DONE",
            EvalJs(tab_to_execute_service_worker, script_to_post_message));
  rfh.WaitUntilRenderFrameDeleted();

  // 6) Go back to A in |tab_to_be_bfcached|.
  tab_to_be_bfcached->web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(tab_to_be_bfcached->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kServiceWorkerPostMessage},
      {}, {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, EvictOnServiceWorkerClaim) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.RegisterRequestHandler(
      base::BindRepeating(&RequestHandlerForUpdateWorker));
  https_server.AddDefaultHandlers(GetTestDataFilePath());
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_execute_service_worker = CreateBrowser();

  // 1) Navigate to A in |tab_to_be_bfcached|.
  EXPECT_TRUE(NavigateToURL(
      tab_to_be_bfcached,
      https_server.GetURL(
          "a.com", "/back_forward_cache/service_worker_registration.html")));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver deleted(rfh_a);

  // 2) Navigate away to B in |tab_to_be_bfcached|.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached,
                            https_server.GetURL("b.com", "/title1.html")));
  EXPECT_FALSE(deleted.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Navigate to A in |tab_to_execute_service_worker|.
  EXPECT_TRUE(NavigateToURL(
      tab_to_execute_service_worker,
      https_server.GetURL(
          "a.com", "/back_forward_cache/service_worker_registration.html")));

  // 4) Register a service worker for |tab_to_execute_service_worker|.
  EXPECT_EQ("DONE", EvalJs(tab_to_execute_service_worker,
                           "register('service_worker_registration.js')"));

  // 5) The service worker calls clients.claim(). |rfh_a| would normally be
  //    claimed but because it's in bfcache, it is evicted from the cache.
  EXPECT_EQ("DONE", EvalJs(tab_to_execute_service_worker, "claim()"));

  // 6) Navigate to A in |tab_to_be_bfcached|.
  tab_to_be_bfcached->web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(tab_to_be_bfcached->web_contents()));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_TRUE(deleted.deleted());
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kServiceWorkerClaim}, {}, {},
      {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       EvictOnServiceWorkerUnregistration) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.RegisterRequestHandler(
      base::BindRepeating(&RequestHandlerForUpdateWorker));
  https_server.AddDefaultHandlers(GetTestDataFilePath());
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_unregister_service_worker = CreateBrowser();

  // 1) Navigate to A in |tab_to_be_bfcached|. This tab will be controlled by a
  // service worker.
  EXPECT_TRUE(NavigateToURL(
      tab_to_be_bfcached,
      https_server.GetURL("a.com",
                          "/back_forward_cache/"
                          "service_worker_registration.html?to_be_bfcached")));

  // 2) Register a service worker for |tab_to_be_bfcached|, but with a narrow
  // scope with URL param. This is to prevent |tab_to_unregister_service_worker|
  // from being controlled by the service worker.
  EXPECT_EQ("DONE",
            EvalJs(tab_to_be_bfcached,
                   "register('service_worker_registration.js', "
                   "'service_worker_registration.html?to_be_bfcached')"));
  EXPECT_EQ("DONE", EvalJs(tab_to_be_bfcached, "claim()"));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver deleted(rfh_a);

  // 3) Navigate to A in |tab_to_unregister_service_worker|. This tab is not
  // controlled by the service worker.
  EXPECT_TRUE(NavigateToURL(
      tab_to_unregister_service_worker,
      https_server.GetURL(
          "a.com", "/back_forward_cache/service_worker_registration.html")));

  // 5) Navigate from A to B in |tab_to_be_bfcached|. Now |tab_to_be_bfcached|
  // should be in bfcache.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached,
                            https_server.GetURL("b.com", "/title1.html")));
  EXPECT_FALSE(deleted.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 6) The service worker gets unregistered. Now |tab_to_be_bfcached| should be
  // notified of the unregistration and evicted from bfcache.
  EXPECT_EQ(
      "DONE",
      EvalJs(tab_to_unregister_service_worker,
             "unregister('service_worker_registration.html?to_be_bfcached')"));

  // 7) Navigate back to A in |tab_to_be_bfcached|.
  tab_to_be_bfcached->web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(tab_to_be_bfcached->web_contents()));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_TRUE(deleted.deleted());
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kServiceWorkerUnregistration},
                    {}, {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, CachePagesWithBeacon) {
  constexpr char kKeepalivePath[] = "/keepalive";

  net::test_server::ControllableHttpResponse keepalive(embedded_test_server(),
                                                       kKeepalivePath);
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_ping(embedded_test_server()->GetURL("a.com", kKeepalivePath));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  EXPECT_TRUE(
      ExecJs(shell(), JsReplace(R"(navigator.sendBeacon($1, "");)", url_ping)));

  // 2) Navigate to B.
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // Ensure that the keepalive request is sent.
  keepalive.WaitForRequest();
  // Don't actually send the response.

  // Page A should be in the cache.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
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
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       NavigateToTwoPagesOnSameSite) {
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
  DisableForRenderFrameHostForTesting(rfh_b3);

  // 4) Do a history navigation back to A1.  At this point, A1 is going to have
  // the same BrowsingInstance as A2. This should cause A2 to get
  // evicted from the BackForwardCache due to its conflicting BrowsingInstance.
  web_contents()->GetController().GoToIndex(0);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(current_frame_host()->GetLastCommittedURL(), url_a1);
  delete_rfh_a2.WaitUntilDeleted();

  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBrowsingInstanceNotSwapped},
      {}, {ShouldSwapBrowsingInstance::kNo_SameSiteNavigation}, {}, FROM_HERE);

  // 5) Go to A2.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ExpectNotRestored(
      {
          BackForwardCacheMetrics::NotRestoredReason::
              kConflictingBrowsingInstance,
      },
      {}, {}, {}, FROM_HERE);
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
  int browsing_instance_id = rfh_a1->GetSiteInstance()->GetBrowsingInstanceId();

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
  DisableForRenderFrameHostForTesting(rfh_a1);

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
                    {}, {}, {RenderFrameHostDisabledForTestingReason()},
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
  int browsing_instance_id = rfh_a1->GetSiteInstance()->GetBrowsingInstanceId();

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

class GeolocationBackForwardCacheBrowserTest
    : public BackForwardCacheBrowserTest {
 protected:
  GeolocationBackForwardCacheBrowserTest() : geo_override_(0.0, 0.0) {}

  device::ScopedGeolocationOverrider geo_override_;
};

// Test that a page which has queried geolocation in the past, but have no
// active geolocation query, can be bfcached.
IN_PROC_BROWSER_TEST_F(GeolocationBackForwardCacheBrowserTest,
                       CacheAfterGeolocationRequest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // Query current position, and wait for the query to complete.
  EXPECT_EQ("received", EvalJs(rfh_a, R"(
      new Promise(resolve => {
        navigator.geolocation.getCurrentPosition(() => resolve('received'));
      });
  )"));

  RenderFrameDeletedObserver deleted(rfh_a);

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // The page has no inflight geolocation request when we navigated away,
  // so it should have been cached.
  EXPECT_FALSE(deleted.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
}

// Test that a page which has an inflight geolocation query can be bfcached,
// and verify that the page does not observe any geolocation while the page
// was inside bfcache.
// The test is flaky on multiple platforms: crbug.com/1033270
IN_PROC_BROWSER_TEST_F(GeolocationBackForwardCacheBrowserTest,
                       DISABLED_CancelGeolocationRequestInFlight) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // Continuously query current geolocation.
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    window.longitude_log = [];
    window.err_log = [];
    window.wait_for_first_position = new Promise(resolve => {
      navigator.geolocation.watchPosition(
        pos => {
          window.longitude_log.push(pos.coords.longitude);
          resolve("resolved");
        },
        err => window.err_log.push(err)
      );
    })
  )"));
  geo_override_.UpdateLocation(0.0, 0.0);
  EXPECT_EQ("resolved", EvalJs(rfh_a, "window.wait_for_first_position"));

  // Pause resolving Geoposition queries to keep the request inflight.
  geo_override_.Pause();
  geo_override_.UpdateLocation(1.0, 1.0);
  EXPECT_EQ(1u, geo_override_.GetGeolocationInstanceCount());

  // 2) Navigate away.
  base::RunLoop loop_until_close;
  geo_override_.SetGeolocationCloseCallback(loop_until_close.QuitClosure());

  RenderFrameDeletedObserver deleted(rfh_a);
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  loop_until_close.Run();

  // The page has no inflight geolocation request when we navigated away,
  // so it should have been cached.
  EXPECT_FALSE(deleted.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Resume resolving Geoposition queries.
  geo_override_.Resume();

  // We update the location while the page is BFCached, but this location should
  // not be observed.
  geo_override_.UpdateLocation(2.0, 2.0);

  // 3) Navigate back to A.

  // The location when navigated back can be observed
  geo_override_.UpdateLocation(3.0, 3.0);

  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());

  // Wait for an update after the user navigates back to A.
  EXPECT_EQ("resolved", EvalJs(rfh_a, R"(
    window.wait_for_position_after_resume = new Promise(resolve => {
      navigator.geolocation.watchPosition(
        pos => {
          window.longitude_log.push(pos.coords.longitude);
          resolve("resolved");
        },
        err => window.err_log.push(err)
      );
    })
  )"));

  EXPECT_LE(0, EvalJs(rfh_a, "longitude_log.indexOf(0.0)").ExtractInt())
      << "Geoposition before the page is put into BFCache should be visible";
  EXPECT_EQ(-1, EvalJs(rfh_a, "longitude_log.indexOf(1.0)").ExtractInt())
      << "Geoposition while the page is put into BFCache should be invisible";
  EXPECT_EQ(-1, EvalJs(rfh_a, "longitude_log.indexOf(2.0)").ExtractInt())
      << "Geoposition while the page is put into BFCache should be invisible";
  EXPECT_LT(0, EvalJs(rfh_a, "longitude_log.indexOf(3.0)").ExtractInt())
      << "Geoposition when the page is restored from BFCache should be visible";
  EXPECT_EQ(0, EvalJs(rfh_a, "err_log.length"))
      << "watchPosition API should have reported no errors";
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
  EXPECT_EQ(time_to_live_in_back_forward_cache,
            base::TimeDelta::FromSeconds(3600));

  base::TimeDelta delta = base::TimeDelta::FromMilliseconds(1);

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
                    {}, {}, FROM_HERE);
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
  DisableForRenderFrameHostForTesting(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    {}, {}, {RenderFrameHostDisabledForTestingReason()},
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
  DisableForRenderFrameHostForTesting(rfh_a_id);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  delete_observer_rfh_a.WaitUntilDeleted();

  // This should not die
  DisableForRenderFrameHostForTesting(rfh_a_id);

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    {}, {}, {RenderFrameHostDisabledForTestingReason()},
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

  DisableForRenderFrameHostForTesting(rfh_b);

  // 2) Navigate to C. A and B are deleted.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  delete_observer_rfh_a.WaitUntilDeleted();
  delete_observer_rfh_b.WaitUntilDeleted();

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    {}, {}, {RenderFrameHostDisabledForTestingReason()},
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

  DisableForRenderFrameHostForTesting(rfh_a);

  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    {}, {}, {RenderFrameHostDisabledForTestingReason()},
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
  int browsing_instance_id = rfh_a->GetSiteInstance()->GetBrowsingInstanceId();

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
      {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, WebBluetooth) {
  // The test requires a mock Bluetooth adapter to perform a
  // WebBluetooth API call. To avoid conflicts with the default Bluetooth
  // adapter, e.g. Windows adapter, which is configured during Bluetooth
  // initialization, the mock adapter is configured in SetUp().

  // WebBluetooth requires HTTPS.
  ASSERT_TRUE(CreateHttpsServer()->Start());
  GURL url(https_server()->GetURL("a.com", "/back_forward_cache/empty.html"));

  ASSERT_TRUE(NavigateToURL(web_contents(), url));
  BackForwardCacheDisabledTester tester;

  EXPECT_EQ("device not found", EvalJs(current_frame_host(), R"(
    new Promise(resolve => {
      navigator.bluetooth.requestDevice({
        filters: [
          { services: [0x1802, 0x1803] },
        ]
      })
      .then(() => resolve("device found"))
      .catch(() => resolve("device not found"))
    });
  )"));
  auto reason = BackForwardCacheDisable::DisabledReason(
      BackForwardCacheDisable::DisabledReasonId::kWebBluetooth);
  EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
      current_frame_host()->GetProcess()->GetID(),
      current_frame_host()->GetRoutingID(), reason));

  ASSERT_TRUE(NavigateToURL(web_contents(),
                            https_server()->GetURL("b.com", "/title1.html")));
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    {}, {}, {reason}, FROM_HERE);
}

// Check the BackForwardCache is disabled when the WebUSB feature is used.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, WebUSB) {
  // WebUSB requires HTTPS.
  ASSERT_TRUE(CreateHttpsServer()->Start());

  auto web_usb_reason = BackForwardCacheDisable::DisabledReason(
      BackForwardCacheDisable::DisabledReasonId::kWebUSB);

  // Main document.
  {
    content::BackForwardCacheDisabledTester tester;
    GURL url(https_server()->GetURL("a.com", "/title1.html"));

    EXPECT_TRUE(NavigateToURL(shell(), url));

    EXPECT_FALSE(current_frame_host()->IsBackForwardCacheDisabled());
    EXPECT_EQ("Found 0 devices", content::EvalJs(current_frame_host(), R"(
        new Promise(async resolve => {
          let devices = await navigator.usb.getDevices();
          resolve("Found " + devices.length + " devices");
        });
    )"));
    EXPECT_TRUE(current_frame_host()->IsBackForwardCacheDisabled());
    EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
        current_frame_host()->GetProcess()->GetID(),
        current_frame_host()->GetRoutingID(), web_usb_reason));
  }

  // Nested document.
  {
    content::BackForwardCacheDisabledTester tester;
    GURL url(https_server()->GetURL("c.com",
                                    "/cross_site_iframe_factory.html?c(d)"));
    EXPECT_TRUE(NavigateToURL(shell(), url));
    RenderFrameHostImpl* rfh_c = current_frame_host();
    RenderFrameHostImpl* rfh_d = rfh_c->child_at(0)->current_frame_host();

    EXPECT_FALSE(rfh_c->IsBackForwardCacheDisabled());
    EXPECT_FALSE(rfh_d->IsBackForwardCacheDisabled());
    EXPECT_EQ("Found 0 devices", content::EvalJs(rfh_c, R"(
        new Promise(async resolve => {
          let devices = await navigator.usb.getDevices();
          resolve("Found " + devices.length + " devices");
        });
    )"));
    EXPECT_TRUE(rfh_c->IsBackForwardCacheDisabled());
    EXPECT_FALSE(rfh_d->IsBackForwardCacheDisabled());
    EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
        rfh_c->GetProcess()->GetID(), rfh_c->GetRoutingID(), web_usb_reason));
  }

  // Worker.
  {
    content::BackForwardCacheDisabledTester tester;
    GURL url(https_server()->GetURL("e.com", "/title1.html"));
    EXPECT_TRUE(NavigateToURL(shell(), url));
    EXPECT_FALSE(current_frame_host()->IsBackForwardCacheDisabled());
    EXPECT_EQ("Found 0 devices", content::EvalJs(current_frame_host(), R"(
        new Promise(async resolve => {
          const worker = new Worker("/back_forward_cache/webusb/worker.js");
          worker.onmessage = message => resolve(message.data);
          worker.postMessage("Run");
        });
    )"));
    EXPECT_TRUE(current_frame_host()->IsBackForwardCacheDisabled());
    EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
        current_frame_host()->GetProcess()->GetID(),
        current_frame_host()->GetRoutingID(), web_usb_reason));
  }

  // Nested worker.
  {
    content::BackForwardCacheDisabledTester tester;
    GURL url(https_server()->GetURL("f.com", "/title1.html"));
    EXPECT_TRUE(NavigateToURL(shell(), url));
    EXPECT_FALSE(current_frame_host()->IsBackForwardCacheDisabled());
    EXPECT_EQ("Found 0 devices", content::EvalJs(current_frame_host(), R"(
        new Promise(async resolve => {
          const worker = new Worker(
            "/back_forward_cache/webusb/nested-worker.js");
          worker.onmessage = message => resolve(message.data);
          worker.postMessage("Run");
        });
    )"));
    EXPECT_TRUE(current_frame_host()->IsBackForwardCacheDisabled());
    EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
        current_frame_host()->GetProcess()->GetID(),
        current_frame_host()->GetRoutingID(), web_usb_reason));
  }
}

#if !defined(OS_ANDROID)
// Check that the back-forward cache is disabled when the Serial API is used.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, Serial) {
  // Serial API requires HTTPS.
  ASSERT_TRUE(CreateHttpsServer()->Start());

  auto serial_reason = BackForwardCacheDisable::DisabledReason(
      BackForwardCacheDisable::DisabledReasonId::kSerial);
  // Main document.
  {
    content::BackForwardCacheDisabledTester tester;
    GURL url(https_server()->GetURL("a.com", "/title1.html"));

    EXPECT_TRUE(NavigateToURL(shell(), url));

    EXPECT_FALSE(current_frame_host()->IsBackForwardCacheDisabled());
    EXPECT_EQ("Found 0 ports", content::EvalJs(current_frame_host(), R"(
        new Promise(async resolve => {
          let ports = await navigator.serial.getPorts();
          resolve("Found " + ports.length + " ports");
        });
    )"));
    EXPECT_TRUE(current_frame_host()->IsBackForwardCacheDisabled());
    EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
        current_frame_host()->GetProcess()->GetID(),
        current_frame_host()->GetRoutingID(), serial_reason));
  }

  // Nested document.
  {
    content::BackForwardCacheDisabledTester tester;
    GURL url(https_server()->GetURL("c.com",
                                    "/cross_site_iframe_factory.html?c(d)"));
    EXPECT_TRUE(NavigateToURL(shell(), url));
    RenderFrameHostImpl* rfh_c = current_frame_host();
    RenderFrameHostImpl* rfh_d = rfh_c->child_at(0)->current_frame_host();

    EXPECT_FALSE(rfh_c->IsBackForwardCacheDisabled());
    EXPECT_FALSE(rfh_d->IsBackForwardCacheDisabled());
    EXPECT_EQ("Found 0 ports", content::EvalJs(rfh_c, R"(
        new Promise(async resolve => {
          let ports = await navigator.serial.getPorts();
          resolve("Found " + ports.length + " ports");
        });
    )"));
    EXPECT_TRUE(rfh_c->IsBackForwardCacheDisabled());
    EXPECT_FALSE(rfh_d->IsBackForwardCacheDisabled());
    EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
        rfh_c->GetProcess()->GetID(), rfh_c->GetRoutingID(), serial_reason));
  }

  // Worker.
  {
    content::BackForwardCacheDisabledTester tester;
    GURL url(https_server()->GetURL("e.com", "/title1.html"));
    EXPECT_TRUE(NavigateToURL(shell(), url));
    EXPECT_FALSE(current_frame_host()->IsBackForwardCacheDisabled());
    EXPECT_EQ("Found 0 ports", content::EvalJs(current_frame_host(), R"(
        new Promise(async resolve => {
          const worker = new Worker("/back_forward_cache/serial/worker.js");
          worker.onmessage = message => resolve(message.data);
          worker.postMessage("Run");
        });
    )"));
    EXPECT_TRUE(current_frame_host()->IsBackForwardCacheDisabled());
    EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
        current_frame_host()->GetProcess()->GetID(),
        current_frame_host()->GetRoutingID(), serial_reason));
  }

  // Nested worker.
  {
    content::BackForwardCacheDisabledTester tester;
    GURL url(https_server()->GetURL("f.com", "/title1.html"));
    EXPECT_TRUE(NavigateToURL(shell(), url));
    EXPECT_FALSE(current_frame_host()->IsBackForwardCacheDisabled());
    EXPECT_EQ("Found 0 ports", content::EvalJs(current_frame_host(), R"(
        new Promise(async resolve => {
          const worker = new Worker(
            "/back_forward_cache/serial/nested-worker.js");
          worker.onmessage = message => resolve(message.data);
          worker.postMessage("Run");
        });
    )"));
    EXPECT_TRUE(current_frame_host()->IsBackForwardCacheDisabled());
    EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
        current_frame_host()->GetProcess()->GetID(),
        current_frame_host()->GetRoutingID(), serial_reason));
  }
}
#endif

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

namespace {

const char kResponseWithNoCache[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Cache-Control: no-store\r\n"
    "\r\n"
    "The server speaks HTTP!";

}  // namespace

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       MainFrameWithNoStoreNotCached) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/main_document"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1. Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(web_contents());
  shell()->LoadURL(url_a);
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();

  // 2. Navigate away and expect frame to be deleted.
  RenderFrameDeletedObserver delete_observer_rfh_a(current_frame_host());
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  delete_observer_rfh_a.WaitUntilDeleted();
}

// Disabled for being flaky. See crbug.com/1116190.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DISABLED_SubframeWithNoStoreCached) {
  // iframe will try to load title1.html.
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/title1.html");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(web_contents());
  shell()->LoadURL(url_a);
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(current_frame_host());

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Navigate back and expect everything to be restored.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());
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

// Check that an audio suspends when the page goes to the cache and can resume
// after restored.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, AudioSuspendAndResume) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    var audio = document.createElement('audio');
    document.body.appendChild(audio);

    audio.testObserverEvents = [];
    let event_list = [
      'canplaythrough',
      'pause',
      'play',
      'error',
    ];
    for (event_name of event_list) {
      let result = event_name;
      audio.addEventListener(event_name, event => {
        document.title = result;
        audio.testObserverEvents.push(result);
      });
    }

    audio.src = 'media/bear-opus.ogg';

    var timeOnFrozen = 0.0;
    audio.addEventListener('pause', () => {
      timeOnFrozen = audio.currentTime;
    });
  )"));

  // Load the media.
  {
    TitleWatcher title_watcher(shell()->web_contents(), u"canplaythrough");
    title_watcher.AlsoWaitForTitle(u"error");
    EXPECT_EQ(u"canplaythrough", title_watcher.WaitAndGetTitle());
  }

  EXPECT_TRUE(ExecJs(rfh_a, R"(
    new Promise(async resolve => {
      audio.play();
      while (audio.currentTime === 0)
        await new Promise(r => setTimeout(r, 1));
      resolve();
    });
  )"));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Navigate back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());

  // Check that the media position is not changed when the page is in cache.
  double duration1 = EvalJs(rfh_a, "timeOnFrozen;").ExtractDouble();
  double duration2 = EvalJs(rfh_a, "audio.currentTime;").ExtractDouble();
  EXPECT_LE(0.0, duration2 - duration1);
  EXPECT_GT(0.01, duration2 - duration1);

  // Resume the media.
  EXPECT_TRUE(ExecJs(rfh_a, "audio.play();"));

  // Confirm that the media pauses automatically when going to the cache.
  // TODO(hajimehoshi): Confirm that this media automatically resumes if
  // autoplay attribute exists.
  EXPECT_EQ(ListValueOf("canplaythrough", "play", "pause", "play"),
            EvalJs(rfh_a, "audio.testObserverEvents"));
}

// Check that a video suspends when the page goes to the cache and can resume
// after restored.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, VideoSuspendAndResume) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    var video = document.createElement('video');
    document.body.appendChild(video);

    video.testObserverEvents = [];
    let event_list = [
      'canplaythrough',
      'pause',
      'play',
      'error',
    ];
    for (event_name of event_list) {
      let result = event_name;
      video.addEventListener(event_name, event => {
        document.title = result;
        // Ignore 'canplaythrough' event as we can randomly get extra
        // 'canplaythrough' events after playing here.
        if (result != 'canplaythrough')
          video.testObserverEvents.push(result);
      });
    }

    video.src = 'media/bear.webm';

    var timeOnFrozen = 0.0;
    video.addEventListener('pause', () => {
      timeOnFrozen = video.currentTime;
    });
  )"));

  // Load the media.
  {
    TitleWatcher title_watcher(shell()->web_contents(), u"canplaythrough");
    title_watcher.AlsoWaitForTitle(u"error");
    EXPECT_EQ(u"canplaythrough", title_watcher.WaitAndGetTitle());
  }

  EXPECT_TRUE(ExecJs(rfh_a, R"(
    new Promise(async resolve => {
      video.play();
      while (video.currentTime == 0)
        await new Promise(r => setTimeout(r, 1));
      resolve();
    });
  )"));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Navigate back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());

  // Check that the media position is not changed when the page is in cache.
  double duration1 = EvalJs(rfh_a, "timeOnFrozen;").ExtractDouble();
  double duration2 = EvalJs(rfh_a, "video.currentTime;").ExtractDouble();
  EXPECT_LE(0.0, duration2 - duration1);
  EXPECT_GT(0.02, duration2 - duration1);

  // Resume the media.
  EXPECT_TRUE(ExecJs(rfh_a, "video.play();"));

  // Confirm that the media pauses automatically when going to the cache.
  // TODO(hajimehoshi): Confirm that this media automatically resumes if
  // autoplay attribute exists.
  EXPECT_EQ(ListValueOf("play", "pause", "play"),
            EvalJs(rfh_a, "video.testObserverEvents"));
}

class SensorBackForwardCacheBrowserTest : public BackForwardCacheBrowserTest {
 protected:
  SensorBackForwardCacheBrowserTest() {
    SensorProviderProxyImpl::OverrideSensorProviderBinderForTesting(
        base::BindRepeating(
            &SensorBackForwardCacheBrowserTest::BindSensorProvider,
            base::Unretained(this)));
  }

  ~SensorBackForwardCacheBrowserTest() override {
    SensorProviderProxyImpl::OverrideSensorProviderBinderForTesting(
        base::NullCallback());
  }

  void SetUpOnMainThread() override {
    provider_ = std::make_unique<device::FakeSensorProvider>();
    provider_->SetAccelerometerData(1.0, 2.0, 3.0);

    BackForwardCacheBrowserTest::SetUpOnMainThread();
  }

  std::unique_ptr<device::FakeSensorProvider> provider_;

 private:
  void BindSensorProvider(
      mojo::PendingReceiver<device::mojom::SensorProvider> receiver) {
    provider_->Bind(std::move(receiver));
  }
};

IN_PROC_BROWSER_TEST_F(SensorBackForwardCacheBrowserTest,
                       AccelerometerNotCached) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  EXPECT_TRUE(ExecJs(rfh_a, R"(
    new Promise(resolve => {
      const sensor = new Accelerometer();
      sensor.addEventListener('reading', () => { resolve(); });
      sensor.start();
    })
  )"));

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // - Page A should not be in the cache.
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::
           kRequestedBackForwardCacheBlockedSensors},
      {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(SensorBackForwardCacheBrowserTest, OrientationCached) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  EXPECT_TRUE(ExecJs(rfh_a, R"(
    window.addEventListener("deviceorientation", () => {});
  )"));

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_THAT(rfh_a, InBackForwardCache());
}

// Tests that the orientation sensor's events are not delivered to a page in the
// back-forward cache.
//
// This sets some JS functions in the pages to enable the sensors, capture and
// validate the events. The a-page should only receive events with alpha=0, the
// b-page is allowed to receive any alpha value. The test captures 3 events in
// the a-page, then navigates to the b-page and changes the reading to have
// alpha=1. While on the b-page it captures 3 more events. If the a-page is
// still receiving events it should receive one or more of these. Finally it
// resets the reasing back to have alpha=0 and navigates back to the a-page and
// catpures 3 more events and verifies that all events on the a-page have
// alpha=1.
IN_PROC_BROWSER_TEST_F(SensorBackForwardCacheBrowserTest,
                       SensorPausedWhileCached) {
  ASSERT_TRUE(CreateHttpsServer()->Start());
  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  provider_->SetRelativeOrientationSensorData(0, 0, 0);

  // JS to cause a page to listen to, capture and validate orientation events.
  const std::string sensor_js = R"(
    // Collects events that have happened so far.
    var events = [];
    // If set, will be called by handleEvent.
    var pendingResolve = null;

    // Handles one event, pushing it to |events| and calling |pendingResolve| if
    // set.
    function handleEvent(event) {
      events.push(event);
      if (pendingResolve !== null) {
        pendingResolve('event');
        pendingResolve = null;
      }
    }

    // Returns a promise that will resolve when the events array has at least
    // |eventCountMin| elements. Returns the number of elements.
    function waitForEventsPromise(eventCountMin) {
      if (events.length >= eventCountMin) {
        return Promise.resolve(events.length);
      }
      return new Promise(resolve => {
        pendingResolve = resolve;
      }).then(() => waitForEventsPromise(eventCountMin));
    }

    // Pretty print an orientation event.
    function eventToString(event) {
      return `${event.alpha} ${event.beta} ${event.gamma}`;
    }

    // Ensure that that |expectedAlpha| matches the alpha of all events.
    function validateEvents(expectedAlpha = null) {
      if (expectedAlpha !== null) {
        let count = 0;
        for (event of events) {
          count++;
          if (Math.abs(event.alpha - expectedAlpha) > 0.01) {
            return `fail - ${count}/${events.length}: ` +
                `${expectedAlpha} != ${event.alpha} (${eventToString(event)})`;
          }
        }
      }
      return 'pass';
    }

    window.addEventListener('deviceorientation', handleEvent);
  )";

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  ASSERT_TRUE(ExecJs(rfh_a, sensor_js));

  // Collect 3 orientation events.
  ASSERT_EQ(1, EvalJs(rfh_a, "waitForEventsPromise(1)"));
  provider_->UpdateRelativeOrientationSensorData(0, 0, 0.2);
  ASSERT_EQ(2, EvalJs(rfh_a, "waitForEventsPromise(2)"));
  provider_->UpdateRelativeOrientationSensorData(0, 0, 0.4);
  ASSERT_EQ(3, EvalJs(rfh_a, "waitForEventsPromise(3)"));
  // We should have 3 events with alpha=0.
  ASSERT_EQ("pass", EvalJs(rfh_a, "validateEvents(0)"));

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_b = current_frame_host();

  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  ASSERT_THAT(rfh_a, InBackForwardCache());
  ASSERT_NE(rfh_a, rfh_b);

  ASSERT_TRUE(ExecJs(rfh_b, sensor_js));

  // Collect 3 orientation events.
  provider_->SetRelativeOrientationSensorData(1, 0, 0);
  ASSERT_EQ(1, EvalJs(rfh_b, "waitForEventsPromise(1)"));
  provider_->UpdateRelativeOrientationSensorData(1, 0, 0.2);
  ASSERT_EQ(2, EvalJs(rfh_b, "waitForEventsPromise(2)"));
  provider_->UpdateRelativeOrientationSensorData(1, 0, 0.4);
  ASSERT_EQ(3, EvalJs(rfh_b, "waitForEventsPromise(3)"));
  // We should have 3 events with alpha=1.
  ASSERT_EQ("pass", EvalJs(rfh_b, "validateEvents()"));

  // 3) Go back to A.
  provider_->UpdateRelativeOrientationSensorData(0, 0, 0);
  web_contents()->GetController().GoBack();
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ASSERT_EQ(rfh_a, current_frame_host());

  // Collect 3 orientation events.
  provider_->UpdateRelativeOrientationSensorData(0, 0, 0);
  // There are 2 processes so, it's possible that more events crept in. So we
  // capture how many there are at this point and uses to wait for at least 3
  // more.
  int count = EvalJs(rfh_a, "waitForEventsPromise(4)").ExtractInt();
  provider_->UpdateRelativeOrientationSensorData(0, 0, 0.2);
  count++;
  ASSERT_EQ(count, EvalJs(rfh_a, base::StringPrintf("waitForEventsPromise(%d)",
                                                    count)));
  provider_->UpdateRelativeOrientationSensorData(0, 0, 0.4);
  count++;
  ASSERT_EQ(count, EvalJs(rfh_a, base::StringPrintf("waitForEventsPromise(%d)",
                                                    count)));

  // We should have the earlier 3 plus another 3 events with alpha=0.
  ASSERT_EQ("pass", EvalJs(rfh_a, "validateEvents(0)"));
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       AllowedFeaturesForSubframesDoNotEvict) {
  // The main purpose of this test is to check that when a state of a subframe
  // is updated, CanStoreDocument is still called for the main frame - otherwise
  // we would always evict the document, even when the feature is allowed as
  // CanStoreDocument always returns false for non-main frames.

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // 2) Navigate to C.
  ASSERT_TRUE(NavigateToURL(shell(), url_c));

  // 3) No-op feature update on a subframe while in cache, should be no-op.
  ASSERT_FALSE(delete_observer_rfh_b.deleted());
  static_cast<blink::mojom::LocalFrameHost*>(rfh_b)
      ->DidChangeActiveSchedulerTrackedFeatures(0);

  // 4) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(current_frame_host(), rfh_a);

  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       IsInactiveAndDisallowActivationIsNoopWhenActive) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_FALSE(current_frame_host()->IsInactiveAndDisallowActivation());

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
  EXPECT_TRUE(rfh_a->IsInactiveAndDisallowActivation());

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kIgnoreEventAndEvict}, {},
      {}, {}, FROM_HERE);
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

// This tests that even if a page initializes WebRTC, tha page can be cached as
// long as it doesn't make a connection.
// On the Android test environments, the test might fail due to IP restrictions.
// See the discussion at http://crrev.com/c/2564926.
#if !defined(OS_ANDROID)

// TODO(https://crbug.com/1213145): The test is consistently failing on some Mac
// bots.
#if defined(OS_MAC)
#define MAYBE_TrivialRTCPeerConnectionCached \
  DISABLED_TrivialRTCPeerConnectionCached
#else
#define MAYBE_TrivialRTCPeerConnectionCached TrivialRTCPeerConnectionCached
#endif
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       MAYBE_TrivialRTCPeerConnectionCached) {
  ASSERT_TRUE(CreateHttpsServer()->Start());

  GURL url_a(https_server()->GetURL("/title1.html"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // Create an RTCPeerConnection without starting a connection.
  EXPECT_TRUE(ExecJs(rfh_a, "const pc1 = new RTCPeerConnection()"));

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectRestored(FROM_HERE);

  // RTCPeerConnection object, that is created before being put into the cache,
  // is still available.
  EXPECT_EQ("success", EvalJs(rfh_a, R"(
    new Promise(async resolve => {
      const pc1 = new RTCPeerConnection();
      const pc2 = new RTCPeerConnection();
      pc1.onicecandidate = e => {
        if (e.candidate)
          pc2.addIceCandidate(e.candidate);
      }
      pc2.onicecandidate = e => {
        if (e.candidate)
          pc1.addIceCandidate(e.candidate);
      }
      pc1.addTransceiver("audio");
      const connectionEstablished = new Promise((resolve, reject) => {
        pc1.oniceconnectionstatechange = () => {
          const state = pc1.iceConnectionState;
          switch (state) {
          case "connected":
          case "completed":
            resolve();
            break;
          case "failed":
          case "disconnected":
          case "closed":
            reject(state);
            break;
          }
        }
      });
      await pc1.setLocalDescription();
      await pc2.setRemoteDescription(pc1.localDescription);
      await pc2.setLocalDescription();
      await pc1.setRemoteDescription(pc2.localDescription);
      try {
        await connectionEstablished;
      } catch (e) {
        resolve("fail " + e);
        return;
      }
      resolve("success");
    });
  )"));
}
#endif  // !defined(OS_ANDROID)

// This tests that a page using WebRTC and creating actual connections cannot be
// cached.
// On the Android test environments, the test might fail due to IP restrictions.
// See the discussion at http://crrev.com/c/2564926.
#if !defined(OS_ANDROID)

// TODO(https://crbug.com/1213145): The test is consistently failing on some Mac
// bots.
#if defined(OS_MAC)
#define MAYBE_NonTrivialRTCPeerConnectionNotCached \
  DISABLED_NonTrivialRTCPeerConnectionNotCached
#else
#define MAYBE_NonTrivialRTCPeerConnectionNotCached \
  NonTrivialRTCPeerConnectionNotCached
#endif
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       MAYBE_NonTrivialRTCPeerConnectionNotCached) {
  ASSERT_TRUE(CreateHttpsServer()->Start());

  GURL url_a(https_server()->GetURL("/title1.html"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Create an RTCPeerConnection with starting a connection.
  EXPECT_EQ("success", EvalJs(rfh_a, R"(
    new Promise(async resolve => {
      const pc1 = new RTCPeerConnection();
      const pc2 = new RTCPeerConnection();
      pc1.onicecandidate = e => {
        if (e.candidate)
          pc2.addIceCandidate(e.candidate);
      }
      pc2.onicecandidate = e => {
        if (e.candidate)
          pc1.addIceCandidate(e.candidate);
      }
      pc1.addTransceiver("audio");
      const connectionEstablished = new Promise(resolve => {
        pc1.oniceconnectionstatechange = () => {
          const state = pc1.iceConnectionState;
          switch (state) {
          case "connected":
          case "completed":
            resolve();
            break;
          case "failed":
          case "disconnected":
          case "closed":
            reject(state);
            break;
          }
        }
      });
      await pc1.setLocalDescription();
      await pc2.setRemoteDescription(pc1.localDescription);
      await pc2.setLocalDescription();
      await pc1.setRemoteDescription(pc2.localDescription);
      await connectionEstablished;
      try {
        await connectionEstablished;
      } catch (e) {
        resolve("fail " + e);
        return;
      }
      resolve("success");
    });
  )"));

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // - Page A should not be in the cache.
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::kWebRTC}, {}, {},
      FROM_HERE);
}
#endif  // !defined(OS_ANDROID)

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, WebLocksNotCached) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Wait for the page to acquire a lock and ensure that it continues to do so.
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    const never_resolved = new Promise(resolve => {});
    new Promise(continue_test => {
      navigator.locks.request('test', async () => {
        continue_test();
        await never_resolved;
      });
    })
  )"));

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // - Page A should not be in the cache.
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::kWebLocks}, {}, {},
      FROM_HERE);
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

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, WebMidiNotCached) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Request access to MIDI. This should prevent the page from entering the
  // BackForwardCache.
  EXPECT_TRUE(ExecJs(rfh_a, "navigator.requestMIDIAccess()",
                     EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // - Page A should not be in the cache.
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::kRequestedMIDIPermission},
      {}, {}, FROM_HERE);
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

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       PresentationConnectionClosed) {
  ASSERT_TRUE(CreateHttpsServer()->Start());
  GURL url_a(https_server()->GetURL(
      "a.com", "/back_forward_cache/presentation_controller.html"));

  // Navigate to A (presentation controller page).
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  auto* rfh_a = current_frame_host();
  // Start a presentation connection in A.
  MockPresentationServiceDelegate mock_presentation_service_delegate;
  auto& presentation_service = rfh_a->GetPresentationServiceForTesting();
  presentation_service.SetControllerDelegateForTesting(
      &mock_presentation_service_delegate);
  EXPECT_CALL(mock_presentation_service_delegate, StartPresentation(_, _, _));
  EXPECT_TRUE(ExecJs(rfh_a, "presentationRequest.start().then(setConnection)",
                     EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Send a mock connection to the renderer.
  MockPresentationConnection mock_controller_connection;
  mojo::Receiver<PresentationConnection> controller_connection_receiver(
      &mock_controller_connection);
  mojo::Remote<PresentationConnection> receiver_connection;
  const std::string presentation_connection_id = "foo";
  presentation_service.OnStartPresentationSucceeded(
      presentation_service.start_presentation_request_id_,
      PresentationConnectionResult::New(
          blink::mojom::PresentationInfo::New(GURL("fake-url"),
                                              presentation_connection_id),
          controller_connection_receiver.BindNewPipeAndPassRemote(),
          receiver_connection.BindNewPipeAndPassReceiver()));

  // Navigate to B, make sure that the connection started in A is closed.
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));
  EXPECT_CALL(
      mock_controller_connection,
      DidClose(blink::mojom::PresentationConnectionCloseReason::WENT_AWAY));
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Navigate back to A. Ensure that connection state has been updated
  // accordingly.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_EQ(presentation_connection_id, EvalJs(rfh_a, "connection.id"));
  EXPECT_EQ("closed", EvalJs(rfh_a, "connection.state"));
  EXPECT_TRUE(EvalJs(rfh_a, "connectionClosed").ExtractBool());

  // Try to start another connection, should successfully reach the browser side
  // PresentationServiceDelegate.
  EXPECT_CALL(mock_presentation_service_delegate,
              ReconnectPresentation(_, presentation_connection_id, _, _));
  EXPECT_TRUE(ExecJs(rfh_a, "presentationRequest.reconnect(connection.id);",
                     EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  base::RunLoop().RunUntilIdle();

  // Reset |presentation_service|'s controller delegate so that it won't try to
  // call Reset() on it on destruction time.
  presentation_service.OnDelegateDestroyed();
}

namespace {

// Subclass of DocumentServiceBase for test.
class EchoImpl final : public DocumentServiceBase<mojom::Echo> {
 public:
  EchoImpl(RenderFrameHost* render_frame_host,
           mojo::PendingReceiver<mojom::Echo> receiver,
           bool* deleted)
      : DocumentServiceBase(render_frame_host, std::move(receiver)),
        deleted_(deleted) {}
  ~EchoImpl() final { *deleted_ = true; }

  // mojom::Echo implementation
  void EchoString(const std::string& input, EchoStringCallback callback) final {
    std::move(callback).Run(input);
  }

 private:
  bool* deleted_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, DocumentServiceBase) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  mojo::Remote<mojom::Echo> echo_remote;
  bool echo_deleted = false;
  new EchoImpl(rfh_a, echo_remote.BindNewPipeAndPassReceiver(), &echo_deleted);

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // - Page A should be in the cache.
  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(echo_deleted);

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_FALSE(echo_deleted);

  // 4) Prevent caching and navigate to B.
  DisableForRenderFrameHostForTesting(rfh_a);
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  delete_observer_rfh_a.WaitUntilDeleted();
  EXPECT_TRUE(echo_deleted);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, OutstandingFetchNotCached) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/fetch");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  // Ensure that there are no lingering requests from page load itself.
  EXPECT_FALSE(rfh_a->scheduler_tracked_features().Has(
      blink::scheduler::WebSchedulerTrackedFeature::
          kOutstandingNetworkRequestFetch));

  // 2) Create a fetch() request.
  ExecuteScriptAsync(rfh_a, "fetch('/fetch');");
  response.WaitForRequest();

  // 3) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // 4) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::
           kOutstandingNetworkRequestFetch},
      {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, OutstandingXHRNotCached) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/xhr");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  // Ensure that there are no lingering requests from page load itself.
  EXPECT_FALSE(rfh_a->scheduler_tracked_features().Has(
      blink::scheduler::WebSchedulerTrackedFeature::
          kOutstandingNetworkRequestXHR));

  // 2) Create a XMLHttpRequest.
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    var req = new XMLHttpRequest();
    req.open("GET", "/xhr");
    req.send();
  )"));
  response.WaitForRequest();

  // 3) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // 4) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::
           kOutstandingNetworkRequestXHR},
      {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, NotFetchedScriptNotCached) {
  net::test_server::ControllableHttpResponse response(
      embedded_test_server(),
      "/back_forward_cache/script-which-does-not-exist.js");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/back_forward_cache/page_with_nonexistent_script.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  TestNavigationObserver navigation_observer1(web_contents());
  shell()->LoadURL(url_a);
  navigation_observer1.WaitForNavigationFinished();
  response.WaitForRequest();

  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  TestNavigationObserver navigation_observer2(web_contents());
  shell()->LoadURL(url_b);
  navigation_observer2.WaitForNavigationFinished();

  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::
           kOutstandingNetworkRequestOthers},
      {}, {}, FROM_HERE);
}

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
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_b->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kActive,
            rfh_b->GetLifecycleState());
  EXPECT_TRUE(rfh_b->GetPage().IsPrimary());

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
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache,
            rfh_b->lifecycle_state());
  EXPECT_FALSE(rfh_b->GetPage().IsPrimary());
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

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoesNotCacheIfSpeechRecognitionIsStarted) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to url_a.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Start SpeechRecognition.
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    new Promise(async resolve => {
    var r = new webkitSpeechRecognition();
    r.start();
    resolve();
    });
  )"));

  // 3) Navigate away.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 4) The page uses SpeechRecognition so it should be deleted.
  delete_observer_rfh_a.WaitUntilDeleted();

  // 5) Go back to the page with SpeechRecognition.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::kSpeechRecognizer}, {}, {},
      FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       CanCacheIfSpeechRecognitionIsNotStarted) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to url_a.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Initialise SpeechRecognition but don't start it yet.
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    new Promise(async resolve => {
    var r = new webkitSpeechRecognition();
    resolve();
    });
  )"));

  // 3) Navigate away.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 4) The page didn't start using SpeechRecognition so it shouldn't be deleted
  // and enter BackForwardCache.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 5) Go back to the page with SpeechRecognition.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());

  ExpectRestored(FROM_HERE);
}

// This test is not important for Chrome OS if TTS is called in content. For
// more details refer (content/browser/speech/tts_platform_impl.cc).
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_DoesNotCacheIfUsingSpeechSynthesis \
  DISABLED_DoesNotCacheIfUsingSpeechSynthesis
#else
#define MAYBE_DoesNotCacheIfUsingSpeechSynthesis \
  DoesNotCacheIfUsingSpeechSynthesis
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       MAYBE_DoesNotCacheIfUsingSpeechSynthesis) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to a page and start using SpeechSynthesis.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver rhf_a_deleted(rfh_a);

  EXPECT_TRUE(ExecJs(rfh_a, R"(
    new Promise(async resolve => {
    var u = new SpeechSynthesisUtterance(" ");
    speechSynthesis.speak(u);
    resolve();
    });
  )"));

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // The page uses SpeechSynthesis so it should be deleted.
  rhf_a_deleted.WaitUntilDeleted();

  // 3) Go back to the page with SpeechSynthesis.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::kSpeechSynthesis}, {}, {},
      FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoesNotCacheIfRunFileChooserIsInvoked) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to url_a and open file chooser.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver deleted_rfh_a(rfh_a);
  content::BackForwardCacheDisabledTester tester;

  // 2) Bind FileChooser to RenderFrameHost.
  mojo::Remote<blink::mojom::FileChooser> chooser =
      FileChooserImpl::CreateBoundForTesting(rfh_a);

  auto quit_run_loop = [](base::OnceClosure callback,
                          blink::mojom::FileChooserResultPtr result) {
    std::move(callback).Run();
  };

  // 3) Run OpenFileChooser and wait till its run.
  base::RunLoop run_loop;
  chooser->OpenFileChooser(
      blink::mojom::FileChooserParams::New(),
      base::BindOnce(quit_run_loop, run_loop.QuitClosure()));
  run_loop.Run();

  // 4) rfh_a should be disabled for BackForwardCache after opening file
  // chooser.
  EXPECT_TRUE(rfh_a->IsBackForwardCacheDisabled());
  auto reason = BackForwardCacheDisable::DisabledReason(
      BackForwardCacheDisable::DisabledReasonId::kFileChooser);
  EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
      rfh_a->GetProcess()->GetID(), rfh_a->GetRoutingID(), reason));

  // 5) Navigate to B having the file chooser open.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // The page uses FileChooser so it should be deleted.
  deleted_rfh_a.WaitUntilDeleted();

  // 6) Go back to the page with FileChooser.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    {}, {}, {reason}, FROM_HERE);
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
  EXPECT_TRUE(rfh_a->coop_reporter());

  // Navigate away and back using the BackForwardCache. The
  // RenderFrameHostImpl::coop_reporter() must still be there.
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());

  EXPECT_TRUE(rfh_a->coop_reporter());
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
  DisableForRenderFrameHostForTesting(rfh_2->GetGlobalId());

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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
  EXPECT_TRUE(rfh_a->IsInactiveAndDisallowActivation());
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
  EXPECT_TRUE(a1->IsInactiveAndDisallowActivation());
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
  EXPECT_TRUE(a1->IsInactiveAndDisallowActivation());
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
                    {}, {}, {}, FROM_HERE);
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
      {}, {ShouldSwapBrowsingInstance::kNo_DoesNotHaveSite}, {}, FROM_HERE);
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
      {}, {ShouldSwapBrowsingInstance::kNo_SameUrlNavigation}, {}, FROM_HERE);
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

class BackForwardCacheBrowserTestWithFileSystemAPISupported
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(features::kBackForwardCache,
                              "file_system_api_supported", "true");
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithFileSystemAPISupported,
                       DISABLED_CacheWithFileSystemAPI) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("/fileapi/request_test.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to a page with WebFileSystem usage.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver deleted(rfh_a);

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_FALSE(deleted.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Go back to the page with WebFileSystem.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());
  ExpectRestored(FROM_HERE);
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

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoNotCacheIfMediaSessionExists) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page using MediaSession.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  RenderFrameHost* rfh_a = shell()->web_contents()->GetMainFrame();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    navigator.mediaSession.metadata = new MediaMetadata({
      artwork: [
        {src: "test_image.jpg", sizes: "1x1", type: "image/jpeg"},
        {src: "test_image.jpg", sizes: "10x10", type: "image/jpeg"}
      ]
    });
  )"));

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  // The page should not have been cached in the back forward cache.
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  auto reason = BackForwardCacheDisable::DisabledReason(
      BackForwardCacheDisable::DisabledReasonId::
          kMediaSessionImplOnServiceCreated);
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    {}, {}, {reason}, FROM_HERE);
}

class BackForwardCacheBrowserTestWithSupportedFeatures
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(features::kBackForwardCache, "supported_features",
                              "BroadcastChannel,KeyboardLock");
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithSupportedFeatures,
                       CacheWithSpecifiedFeatures) {
  ASSERT_TRUE(CreateHttpsServer()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to the page A with BroadcastChannel.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver deleted(rfh_a);
  EXPECT_TRUE(ExecJs(rfh_a, "window.foo = new BroadcastChannel('foo');"));

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_FALSE(deleted.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Go back to the page A
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());
  ExpectRestored(FROM_HERE);

  // 4) Use KeyboardLock
  EXPECT_EQ("DONE", EvalJs(rfh_a, R"(
    new Promise(resolve => {
      navigator.keyboard.lock();
      resolve('DONE');
    });
  )"));

  // 5) Navigate away again.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_FALSE(deleted.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 6) Go back to the page A again.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());
  ExpectRestored(FROM_HERE);
}

class BackForwardCacheBrowserTestWithNoSupportedFeatures
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Specify empty supported features explicitly.
    EnableFeatureAndSetParams(features::kBackForwardCache, "supported_features",
                              "");
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithNoSupportedFeatures,
                       DontCache) {
  ASSERT_TRUE(CreateHttpsServer()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to the page A with BoradcastChannel.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a1 = current_frame_host();
  RenderFrameDeletedObserver deleted_a1(rfh_a1);
  EXPECT_TRUE(ExecJs(rfh_a1, "window.foo = new BroadcastChannel('foo');"));

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  deleted_a1.WaitUntilDeleted();

  // 3) Go back to the page A
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::kBroadcastChannel}, {}, {},
      FROM_HERE);

  RenderFrameHostImpl* rfh_a2 = current_frame_host();
  RenderFrameDeletedObserver deleted_a2(rfh_a2);

  // 4) Use KeyboardLock
  EXPECT_EQ("DONE", EvalJs(rfh_a2, R"(
    new Promise(resolve => {
      navigator.keyboard.lock();
      resolve('DONE');
    });
  )"));

  // 5) Navigate away again.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  deleted_a2.WaitUntilDeleted();

  // 6) Go back to the page A again.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::kKeyboardLock}, {}, {},
      FROM_HERE);
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
                            ->GetFrameTree()
                            ->root();

  // Initial navigation (so that we can initiate a navigation from renderer).
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  // 1) Navigate to A with user gesture.
  {
    FrameNavigateParamsCapturer params_capturer(root);
    EXPECT_TRUE(NavigateToURLFromRenderer(shell(), url_a));
    params_capturer.Wait();
    EXPECT_TRUE(params_capturer.has_user_gesture());
  }
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2) Navigate to B. A should be stored in the back-forward cache.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

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
                            ->GetFrameTree()
                            ->root();

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
        {}, {}, {}, FROM_HERE);
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
        {}, {}, {}, FROM_HERE);
  }
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       RestoreWhenUserAgentOverrideDiffers) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  NavigationControllerImpl& controller = web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

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

class BackForwardCacheBrowserTestWithMediaSession
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(features::kBackForwardCache, "supported_features",
                              "MediaSessionImplOnServiceCreated");
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithMediaSession,
                       DoNotCacheIfMediaSessionPlaybackStateChanged) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page using MediaSession.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  RenderFrameHost* rfh_a = shell()->web_contents()->GetMainFrame();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    navigator.mediaSession.metadata = new MediaMetadata({
      artwork: [
        {src: "test_image.jpg", sizes: "1x1", type: "image/jpeg"},
        {src: "test_image.jpg", sizes: "10x10", type: "image/jpeg"}
      ]
    });
  )"));

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // The page is restored since the playback state is not changed.
  ExpectRestored(FROM_HERE);

  // 4) Modify the playback state of the media session.
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    navigator.mediaSession.playbackState = 'playing';
  )"));

  // 5) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  // 6) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // The page is not restored due to the playback.
  auto reason = BackForwardCacheDisable::DisabledReason(
      BackForwardCacheDisable::DisabledReasonId::kMediaSession);
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    {}, {}, {reason}, FROM_HERE);
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
                    {}, {}, {}, FROM_HERE);
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
                    {}, {}, {}, FROM_HERE);
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
                    {}, {}, {}, FROM_HERE);

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
                         testing::Values("always", "opt_in_header_required"));

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

  ExpectRestored(FROM_HERE);

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
  } else {
    EXPECT_EQ(GetParam(), "opt_in_header_required");
    ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                           kOptInUnloadHeaderNotPresent},
                      {}, {}, {}, FROM_HERE);
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

  ExpectRestored(FROM_HERE);

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
                      {}, {}, {}, FROM_HERE);
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

class BackForwardCacheBrowserTestAllowCacheControlNoStore
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(features::kBackForwardCache, "", "");
    EnableFeatureAndSetParams(kCacheControlNoStoreEnterBackForwardCache, "",
                              "");
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// but does not get restored and gets evicted.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestAllowCacheControlNoStore,
                       PagesWithCacheControlNoStoreEnterBfcacheAndEvicted) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  net::test_server::ControllableHttpResponse response2(embedded_test_server(),
                                                       "/main_document");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/main_document"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(web_contents());
  shell()->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();

  // 2) Navigate away. |rfh_a| should enter the bfcache.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Go back. |rfh_a| should be evicted upon restoration.
  TestNavigationObserver observer2(web_contents());
  web_contents()->GetController().GoBack();
  response2.WaitForRequest();
  response2.Send(kResponseWithNoCache);
  response2.Done();
  observer2.Wait();

  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kCacheControlNoStore}, {},
      {}, {}, FROM_HERE);
}

#if BUILDFLAG(IS_CHROMECAST)
#define MAYBE_PagesWithCacheControlNoStoreCookieModifiedThroughJavaScript \
  DISABLED_PagesWithCacheControlNoStoreCookieModifiedThroughJavaScript
#else
#define MAYBE_PagesWithCacheControlNoStoreCookieModifiedThroughJavaScript \
  PagesWithCacheControlNoStoreCookieModifiedThroughJavaScript
#endif

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and if a cookie is modified while it is in bfcache via JavaScript, gets
// evicted with cookie modified marked.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestAllowCacheControlNoStore,
    MAYBE_PagesWithCacheControlNoStoreCookieModifiedThroughJavaScript) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  net::test_server::ControllableHttpResponse response2(embedded_test_server(),
                                                       "/main_document");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/main_document"));
  GURL url_a_2(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_modify_cookie = CreateBrowser();

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();

  // 2) Set a normal cookie from JavaScript.
  EXPECT_TRUE(ExecJs(tab_to_be_bfcached, "document.cookie='foo=bar'"));
  EXPECT_EQ("foo=bar", EvalJs(tab_to_be_bfcached, "document.cookie"));

  // 3) Navigate away. |rfh_a| should enter bfcache.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 4) Navigate to a.com in |tab_to_modify_cookie| and modify cookie from
  // JavaScript.
  EXPECT_TRUE(NavigateToURL(tab_to_modify_cookie, url_a_2));
  EXPECT_EQ("foo=bar", EvalJs(tab_to_modify_cookie, "document.cookie"));
  EXPECT_TRUE(ExecJs(tab_to_modify_cookie, "document.cookie='foo=baz'"));
  EXPECT_EQ("foo=baz", EvalJs(tab_to_modify_cookie, "document.cookie"));

  // 5) Go back. |rfh_a| should be evicted upon restoration.
  TestNavigationObserver observer2(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->web_contents()->GetController().GoBack();
  response2.WaitForRequest();
  response2.Send(kResponseWithNoCache);
  response2.Done();
  observer2.Wait();

  EXPECT_EQ("foo=baz", EvalJs(tab_to_be_bfcached, "document.cookie"));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kCacheControlNoStoreCookieModified},
                    {}, {}, {}, FROM_HERE);
}

// Disabled due to flakiness on Cast Audio Linux https://crbug.com/1229182
#if BUILDFLAG(IS_CHROMECAST)
#define MAYBE_PagesWithCacheControlNoStoreCookieModifiedBackTwice \
  DISABLED_PagesWithCacheControlNoStoreCookieModifiedBackTwice
#else
#define MAYBE_PagesWithCacheControlNoStoreCookieModifiedBackTwice \
  PagesWithCacheControlNoStoreCookieModifiedBackTwice
#endif

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and if a cookie is modified, it gets evicted with cookie changed, but if
// navigated away again and navigated back, it gets evicted without cookie
// change marked.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestAllowCacheControlNoStore,
    MAYBE_PagesWithCacheControlNoStoreCookieModifiedBackTwice) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  net::test_server::ControllableHttpResponse response2(embedded_test_server(),
                                                       "/main_document");
  net::test_server::ControllableHttpResponse response3(embedded_test_server(),
                                                       "/main_document");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/main_document"));
  GURL url_a_2(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_modify_cookie = CreateBrowser();

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();

  // 2) Set a normal cookie from JavaScript.
  EXPECT_TRUE(ExecJs(tab_to_be_bfcached, "document.cookie='foo=bar'"));
  EXPECT_EQ("foo=bar", EvalJs(tab_to_be_bfcached, "document.cookie"));

  // 3) Navigate away. |rfh_a| should enter bfcache.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 4) Navigate to a.com in |tab_to_modify_cookie| and modify cookie from
  // JavaScript.
  EXPECT_TRUE(NavigateToURL(tab_to_modify_cookie, url_a_2));
  EXPECT_EQ("foo=bar", EvalJs(tab_to_modify_cookie, "document.cookie"));
  EXPECT_TRUE(ExecJs(tab_to_modify_cookie, "document.cookie='foo=baz'"));
  EXPECT_EQ("foo=baz", EvalJs(tab_to_modify_cookie, "document.cookie"));

  // 5) Go back. |rfh_a| should be evicted upon restoration.
  TestNavigationObserver observer2(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->web_contents()->GetController().GoBack();
  response2.WaitForRequest();
  response2.Send(kResponseWithNoCache);
  response2.Done();
  observer2.Wait();

  EXPECT_EQ("foo=baz", EvalJs(tab_to_be_bfcached, "document.cookie"));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kCacheControlNoStoreCookieModified},
                    {}, {}, {}, FROM_HERE);
  RenderFrameHostImplWrapper rfh_a_2(current_frame_host());

  // 6) Navigate away to b.com. |rfh_a_2| should enter bfcache again.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a_2->IsInBackForwardCache());

  // 7) Navigate back to a.com. This time the cookie change has to be reset and
  // gets evicted with a different reason.
  TestNavigationObserver observer3(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->web_contents()->GetController().GoBack();
  response3.WaitForRequest();
  response3.Send(kResponseWithNoCache);
  response3.Done();
  observer3.Wait();
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kCacheControlNoStore}, {},
      {}, {}, FROM_HERE);
}

// Disabled due to flakiness on Cast Audio Linux https://crbug.com/1229182
#if BUILDFLAG(IS_CHROMECAST)
#define MAYBE_PagesWithCacheControlNoStoreCookieModifiedThroughJavaScriptOnDifferentDomain \
  DISABLED_PagesWithCacheControlNoStoreCookieModifiedThroughJavaScriptOnDifferentDomain
#else
#define MAYBE_PagesWithCacheControlNoStoreCookieModifiedThroughJavaScriptOnDifferentDomain \
  PagesWithCacheControlNoStoreCookieModifiedThroughJavaScriptOnDifferentDomain
#endif

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and even if a cookie is modified on a different domain than the entry, the
// entry is not marked as cookie modified.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestAllowCacheControlNoStore,
    MAYBE_PagesWithCacheControlNoStoreCookieModifiedThroughJavaScriptOnDifferentDomain) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  net::test_server::ControllableHttpResponse response2(embedded_test_server(),
                                                       "/main_document");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/main_document"));
  GURL url_a_2(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_modify_cookie = CreateBrowser();

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();

  // 2) Set a normal cookie from JavaScript.
  EXPECT_TRUE(ExecJs(tab_to_be_bfcached, "document.cookie='foo=bar'"));
  EXPECT_EQ("foo=bar", EvalJs(tab_to_be_bfcached, "document.cookie"));

  // 3) Navigate away. |rfh_a| should enter bfcache.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 4) Navigate to b.com in |tab_to_modify_cookie| and modify cookie from
  // JavaScript.
  EXPECT_TRUE(NavigateToURL(tab_to_modify_cookie, url_b));
  EXPECT_TRUE(ExecJs(tab_to_modify_cookie, "document.cookie='foo=baz'"));
  EXPECT_EQ("foo=baz", EvalJs(tab_to_modify_cookie, "document.cookie"));

  // 5) Go back. |rfh_a| should be evicted upon restoration.
  TestNavigationObserver observer2(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->web_contents()->GetController().GoBack();
  response2.WaitForRequest();
  response2.Send(kResponseWithNoCache);
  response2.Done();
  observer2.Wait();
  EXPECT_EQ("foo=bar", EvalJs(tab_to_be_bfcached, "document.cookie"));

  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kCacheControlNoStore}, {},
      {}, {}, FROM_HERE);
}

namespace {
const char kResponseWithNoCacheWithCookie[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Set-Cookie: foo=bar\r\n"
    "Cache-Control: no-store\r\n"
    "\r\n"
    "The server speaks HTTP!";

const char kResponseWithNoCacheWithHTTPOnlyCookie[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Set-Cookie: foo=bar; Secure; HttpOnly;\r\n"
    "Cache-Control: no-store\r\n"
    "\r\n"
    "The server speaks HTTP!";

const char kResponseWithNoCacheWithHTTPOnlyCookie2[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Set-Cookie: foo=baz; Secure; HttpOnly;\r\n"
    "Cache-Control: no-store\r\n"
    "\r\n"
    "The server speaks HTTP!";
}  // namespace

// Disabled due to flakiness on Cast Audio Linux https://crbug.com/1229182
#if BUILDFLAG(IS_CHROMECAST)
#define MAYBE_PagesWithCacheControlNoStoreSetFromResponseHeader \
  DISABLED_PagesWithCacheControlNoStoreSetFromResponseHeader
#else
#define MAYBE_PagesWithCacheControlNoStoreSetFromResponseHeader \
  PagesWithCacheControlNoStoreSetFromResponseHeader
#endif

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and if a cookie is modified while it is in bfcache via response header, gets
// evicted with cookie modified marked.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestAllowCacheControlNoStore,
    MAYBE_PagesWithCacheControlNoStoreSetFromResponseHeader) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  net::test_server::ControllableHttpResponse response2(embedded_test_server(),
                                                       "/main_document");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/main_document"));
  GURL url_a_2(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_modify_cookie = CreateBrowser();

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCacheWithCookie);
  response.Done();
  observer.Wait();
  EXPECT_EQ("foo=bar", EvalJs(tab_to_be_bfcached, "document.cookie"));

  // 2) Navigate away. |rfh_a| should enter bfcache.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Navigate to a.com in |tab_to_modify_cookie| and modify cookie from
  // JavaScript.
  EXPECT_TRUE(NavigateToURL(tab_to_modify_cookie, url_a_2));
  EXPECT_EQ("foo=bar", EvalJs(tab_to_modify_cookie, "document.cookie"));
  EXPECT_TRUE(ExecJs(tab_to_modify_cookie, "document.cookie='foo=baz'"));
  EXPECT_EQ("foo=baz", EvalJs(tab_to_modify_cookie, "document.cookie"));

  // 4) Go back. |rfh_a| should be evicted upon restoration.
  TestNavigationObserver observer2(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->web_contents()->GetController().GoBack();
  response2.WaitForRequest();
  // Send the response without the cookie header to avoid overwriting the
  // cookie.
  response2.Send(kResponseWithNoCache);
  response2.Done();
  observer2.Wait();
  EXPECT_EQ("foo=baz", EvalJs(tab_to_be_bfcached, "document.cookie"));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kCacheControlNoStoreCookieModified},
                    {}, {}, {}, FROM_HERE);
}

// Disabled due to flakiness on Cast Audio Linux https://crbug.com/1229182
#if BUILDFLAG(IS_CHROMECAST)
#define MAYBE_PagesWithCacheControlNoStoreSetFromResponseHeaderHTTPOnlyCookie \
  DISABLED_PagesWithCacheControlNoStoreSetFromResponseHeaderHTTPOnlyCookie
#else
#define MAYBE_PagesWithCacheControlNoStoreSetFromResponseHeaderHTTPOnlyCookie \
  PagesWithCacheControlNoStoreSetFromResponseHeaderHTTPOnlyCookie
#endif

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and if HTTPOnly cookie is modified while it is in bfcache, gets evicted with
// HTTPOnly cookie modified marked.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestAllowCacheControlNoStore,
    MAYBE_PagesWithCacheControlNoStoreSetFromResponseHeaderHTTPOnlyCookie) {
  // HTTPOnly cookie can be only set over HTTPS.
  CreateHttpsServer();
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      "/main_document");
  net::test_server::ControllableHttpResponse response2(https_server(),
                                                       "/main_document2");
  net::test_server::ControllableHttpResponse response3(https_server(),
                                                       "/main_document");
  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/main_document"));
  GURL url_a_2(https_server()->GetURL("a.com", "/main_document2"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_modify_cookie = CreateBrowser();

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCacheWithHTTPOnlyCookie);
  response.Done();
  observer.Wait();
  // HTTPOnly cookie should not be accessible from JavaScript.
  EXPECT_EQ("", EvalJs(tab_to_be_bfcached, "document.cookie"));

  // 2) Navigate away. |rfh_a| should enter bfcache.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Navigate to a.com in |tab_to_modify_cookie| and modify HTTPOnly cookie
  // from the response.
  TestNavigationObserver observer2(tab_to_modify_cookie->web_contents());
  tab_to_modify_cookie->LoadURL(url_a_2);
  response2.WaitForRequest();
  response2.Send(kResponseWithNoCacheWithHTTPOnlyCookie2);
  response2.Done();
  observer2.Wait();

  // 4) Go back. |rfh_a| should be evicted upon restoration.
  TestNavigationObserver observer3(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->web_contents()->GetController().GoBack();
  response3.WaitForRequest();
  response3.Send(kResponseWithNoCacheWithHTTPOnlyCookie);
  response3.Done();
  observer3.Wait();
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kCacheControlNoStoreHTTPOnlyCookieModified},
                    {}, {}, {}, FROM_HERE);
}

// Disabled due to flakiness on Cast Audio Linux https://crbug.com/1229182
#if BUILDFLAG(IS_CHROMECAST)
#define MAYBE_PagesWithCacheControlNoStoreHTTPOnlyCookieModifiedBackTwice \
  DISABLED_PagesWithCacheControlNoStoreHTTPOnlyCookieModifiedBackTwice
#else
#define MAYBE_PagesWithCacheControlNoStoreHTTPOnlyCookieModifiedBackTwice \
  PagesWithCacheControlNoStoreHTTPOnlyCookieModifiedBackTwice
#endif

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and if a HTTPOnly cookie is modified, it gets evicted with cookie changed,
// but if navigated away again and navigated back, it gets evicted without
// HTTPOnly cookie change marked.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestAllowCacheControlNoStore,
    MAYBE_PagesWithCacheControlNoStoreHTTPOnlyCookieModifiedBackTwice) {
  CreateHttpsServer();
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      "/main_document");
  net::test_server::ControllableHttpResponse response2(https_server(),
                                                       "/main_document2");
  net::test_server::ControllableHttpResponse response3(https_server(),
                                                       "/main_document");
  net::test_server::ControllableHttpResponse response4(https_server(),
                                                       "/main_document");
  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/main_document"));
  GURL url_a_2(https_server()->GetURL("a.com", "/main_document2"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_modify_cookie = CreateBrowser();

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCacheWithHTTPOnlyCookie);
  response.Done();
  observer.Wait();

  // 2) Navigate away. |rfh_a| should enter bfcache.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Navigate to a.com in |tab_to_modify_cookie| and modify cookie from
  // response header.
  TestNavigationObserver observer2(tab_to_modify_cookie->web_contents());
  tab_to_modify_cookie->LoadURL(url_a_2);
  response2.WaitForRequest();
  response2.Send(kResponseWithNoCacheWithHTTPOnlyCookie2);
  response2.Done();
  observer2.Wait();

  // 4) Go back. |rfh_a| should be evicted upon restoration.
  TestNavigationObserver observer3(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->web_contents()->GetController().GoBack();
  response3.WaitForRequest();
  response3.Send(kResponseWithNoCache);
  response3.Done();
  observer3.Wait();

  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kCacheControlNoStoreHTTPOnlyCookieModified},
                    {}, {}, {}, FROM_HERE);
  RenderFrameHostImplWrapper rfh_a_2(current_frame_host());

  // 5) Navigate away to b.com. |rfh_a_2| should enter bfcache again.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a_2->IsInBackForwardCache());

  // 6) Navigate back to a.com. This time the cookie change has to be reset and
  // gets evicted with a different reason.
  TestNavigationObserver observer4(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->web_contents()->GetController().GoBack();
  response4.WaitForRequest();
  response4.Send(kResponseWithNoCache);
  response4.Done();
  observer4.Wait();
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kCacheControlNoStore}, {},
      {}, {}, FROM_HERE);
}

class BackForwardCacheBrowserTestRestoreCacheControlNoStoreUnlessCookieChange
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(features::kBackForwardCache, "", "");
    EnableFeatureAndSetParams(kCacheControlNoStoreEnterBackForwardCache, "",
                              "");
    EnableFeatureAndSetParams(
        kCacheControlNoStoreRestoreFromBackForwardCacheUnlessCookieChange, "",
        "");
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and gets restored if cookies do not change.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreCacheControlNoStoreUnlessCookieChange,
    PagesWithCacheControlNoStoreRestoreFromBackForwardCache) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  net::test_server::ControllableHttpResponse response2(embedded_test_server(),
                                                       "/main_document");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/main_document"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(web_contents());
  shell()->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();

  // 2) Navigate away. |rfh_a| should enter the bfcache.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Go back. |rfh_a| should be restored.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  ExpectRestored(FROM_HERE);
}

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// but gets evicted if cookies change.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreCacheControlNoStoreUnlessCookieChange,
    PagesWithCacheControlNoStoreEvictedIfCookieChange) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  net::test_server::ControllableHttpResponse response2(embedded_test_server(),
                                                       "/main_document");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/main_document"));
  GURL url_a_2(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_modify_cookie = CreateBrowser();

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();

  // 2) Set a normal cookie from JavaScript.
  EXPECT_TRUE(ExecJs(tab_to_be_bfcached, "document.cookie='foo=bar'"));
  EXPECT_EQ("foo=bar", EvalJs(tab_to_be_bfcached, "document.cookie"));

  // 3) Navigate away. |rfh_a| should enter bfcache.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 4) Navigate to a.com in |tab_to_modify_cookie| and modify cookie from
  // JavaScript.
  EXPECT_TRUE(NavigateToURL(tab_to_modify_cookie, url_a_2));
  EXPECT_EQ("foo=bar", EvalJs(tab_to_modify_cookie, "document.cookie"));
  EXPECT_TRUE(ExecJs(tab_to_modify_cookie, "document.cookie='foo=baz'"));
  EXPECT_EQ("foo=baz", EvalJs(tab_to_modify_cookie, "document.cookie"));

  // 5) Go back. |rfh_a| should be evicted upon restoration.
  TestNavigationObserver observer2(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->web_contents()->GetController().GoBack();
  response2.WaitForRequest();
  response2.Send(kResponseWithNoCache);
  response2.Done();
  observer2.Wait();

  EXPECT_EQ("foo=baz", EvalJs(tab_to_be_bfcached, "document.cookie"));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kCacheControlNoStoreCookieModified},
                    {}, {}, {}, FROM_HERE);
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

}  // namespace content
