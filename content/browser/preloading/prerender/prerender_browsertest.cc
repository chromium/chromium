// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <optional>
#include <tuple>

#include "base/barrier_closure.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/thread_annotations.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/features.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/services/storage/public/mojom/storage_service.mojom.h"
#include "components/services/storage/public/mojom/test_api.test-mojom.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/back_forward_cache_test_util.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_test_util_internal.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_attempt_impl.h"
#include "content/browser/preloading/preloading_data_impl.h"
#include "content/browser/preloading/preloading_decider.h"
#include "content/browser/preloading/prerender/prerender_features.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/preloading/prerender/prerender_metrics.h"
#include "content/browser/preloading/prerender/prerender_no_vary_search_hint_commit_deferring_condition.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigation_type.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/features.h"
#include "content/common/input/synthetic_tap_gesture.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/prerender_web_contents_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/background_color_change_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "content/public/test/mock_client_hints_controller_delegate.h"
#include "content/public/test/mock_web_contents_observer.h"
#include "content/public/test/mojo_capability_control_test_interfaces.mojom.h"
#include "content/public/test/mojo_capability_control_test_util.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/theme_change_waiter.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/mock_commit_deferring_condition.h"
#include "content/test/render_document_feature.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/loader_constants.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "third_party/blink/public/mojom/page/display_cutout.mojom.h"
#include "ui/events/base_event_utils.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/common/pepper_plugin.mojom.h"
#endif  // BUILDFLAG(ENABLE_PLUGINS)

using ::testing::Exactly;

namespace content {
namespace {

enum class BackForwardCacheType {
  kDisabled,
  kEnabled,
};

std::string ToString(const testing::TestParamInfo<BackForwardCacheType>& info) {
  switch (info.param) {
    case BackForwardCacheType::kDisabled:
      return "Disabled";
    case BackForwardCacheType::kEnabled:
      return "Enabled";
  }
}

int32_t InterfaceNameHasher(const std::string& interface_name) {
  return static_cast<int32_t>(base::HashMetricNameAs32Bits(interface_name));
}

RenderFrameHost* FindRenderFrameHost(Page& page, const GURL& url) {
  return FrameMatchingPredicate(page,
                                base::BindRepeating(&FrameHasSourceUrl, url));
}

ukm::SourceId ToSourceId(int64_t navigation_id) {
  return ukm::ConvertToSourceId(navigation_id,
                                ukm::SourceIdType::NAVIGATION_ID);
}

// A fake implementation of base::MemoryPressureMonitor. An instance of this
// class is used via a global variable. The base class sets itself in the
// global variable on the constructor and unsets it on the destructor.
// base::MemoryPressureMonitor::Get() provides access to the instance.
class FakeMemoryPressureMonitor : public base::MemoryPressureMonitor {
 public:
  FakeMemoryPressureMonitor(MemoryPressureLevel level) : level_(level) {}

  MemoryPressureLevel GetCurrentPressureLevel() const override {
    return level_;
  }

 private:
  const MemoryPressureLevel level_ =
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_NONE;
};

// Example class which inherits the DocumentUserData, all the data is
// associated to the lifetime of the document.
class DocumentData : public DocumentUserData<DocumentData> {
 public:
  ~DocumentData() override = default;

  base::WeakPtr<DocumentData> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  explicit DocumentData(RenderFrameHost* render_frame_host)
      : DocumentUserData<DocumentData>(render_frame_host) {}

  friend class DocumentUserData<DocumentData>;

  base::WeakPtrFactory<DocumentData> weak_ptr_factory_{this};

  DOCUMENT_USER_DATA_KEY_DECL();
};

DOCUMENT_USER_DATA_KEY_IMPL(DocumentData);

using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;
using ukm::builders::Preloading_Attempt;
using ukm::builders::Preloading_Attempt_PreviousPrimaryPage;
using ukm::builders::Preloading_Prediction;
using ukm::builders::Preloading_Prediction_PreviousPrimaryPage;

static const auto kMockElapsedTime =
    base::ScopedMockElapsedTimersForTest::kMockElapsedTime;

// Utility class to make building expected
// TestUkmRecorder::HumanReadableUkmEntry for EXPECT_EQ for
// PreloadingAttemptPreviousPrimaryPage.
class PreloadingAttemptPreviousPrimaryPageUkmEntryBuilder {
 public:
  explicit PreloadingAttemptPreviousPrimaryPageUkmEntryBuilder(
      PreloadingPredictor predictor)
      : predictor_(predictor) {}

  // Unlike PreloadingAttemptUkmEntryBuilder, this method assumes a navigation
  // has not occurred thus `TimeToNextNavigation` is not set.
  //
  // Optional `ready_time` should be set by the caller, if this attempt ever
  // reaches `PreloadingTriggeringOutcome::kReady` state, at the time of
  // reporting. Install `base::ScopedMockElapsedTimersForTest` into the test
  // fixture to assert the entry's latency values' correctness.
  ukm::TestUkmRecorder::HumanReadableUkmEntry BuildEntry(
      ukm::SourceId source_id,
      PreloadingType preloading_type,
      PreloadingEligibility eligibility,
      PreloadingHoldbackStatus holdback_status,
      PreloadingTriggeringOutcome triggering_outcome,
      PreloadingFailureReason failure_reason,
      bool accurate,
      std::optional<base::TimeDelta> ready_time = std::nullopt,
      std::optional<blink::mojom::SpeculationEagerness> eagerness =
          std::nullopt) const {
    std::map<std::string, int64_t> metrics = {
        {Preloading_Attempt::kPreloadingTypeName,
         static_cast<int64_t>(preloading_type)},
        {Preloading_Attempt::kPreloadingPredictorName, predictor_.ukm_value()},
        {Preloading_Attempt::kEligibilityName,
         static_cast<int64_t>(eligibility)},
        {Preloading_Attempt::kHoldbackStatusName,
         static_cast<int64_t>(holdback_status)},
        {Preloading_Attempt::kTriggeringOutcomeName,
         static_cast<int64_t>(triggering_outcome)},
        {Preloading_Attempt::kFailureReasonName,
         static_cast<int64_t>(failure_reason)},
        {Preloading_Attempt::kAccurateTriggeringName, accurate ? 1 : 0}};
    if (ready_time) {
      metrics.insert({Preloading_Attempt::kReadyTimeName,
                      ukm::GetExponentialBucketMinForCounts1000(
                          ready_time->InMilliseconds())});
    }
    if (eagerness) {
      metrics.insert({Preloading_Attempt::kSpeculationEagernessName,
                      static_cast<int64_t>(eagerness.value())});
    }
    return UkmEntry{source_id, std::move(metrics)};
  }

 private:
  PreloadingPredictor predictor_;
};

// Utility class to make building expected
// TestUkmRecorder::HumanReadableUkmEntry for EXPECT_EQ for
// PreloadingPredictionPreviousPrimaryPage.
class PreloadingPredictionPreviousPrimaryPageUkmEntryBuilder {
 public:
  explicit PreloadingPredictionPreviousPrimaryPageUkmEntryBuilder(
      PreloadingPredictor predictor)
      : predictor_(predictor) {}

  // Unlike PreloadingPredictionUkmEntryBuilder, this method assumes a
  // navigation has not occurred thus `TimeToNextNavigation` is not set.
  ukm::TestUkmRecorder::HumanReadableUkmEntry
  BuildEntry(ukm::SourceId source_id, int confidence, bool accurate) const {
    std::map<std::string, int64_t> metrics = {
        {Preloading_Prediction::kConfidenceName,
         static_cast<int64_t>(confidence)},
        {Preloading_Attempt::kPreloadingPredictorName,
         static_cast<int64_t>(predictor_.ukm_value())},
        {Preloading_Prediction::kAccuratePredictionName, accurate ? 1 : 0}};
    return UkmEntry{source_id, std::move(metrics)};
  }

 private:
  PreloadingPredictor predictor_;
};

// Tests the params of WebContentsImpl that contains a prerendered page for a
// new tab navigation.
void ExpectWebContentsIsForNewTabPrerendering(WebContents& web_contents) {
  auto& web_contents_impl = static_cast<WebContentsImpl&>(web_contents);

  // The primary page shows the initial blank page.
  EXPECT_TRUE(web_contents_impl.GetLastCommittedURL().is_empty());

  // The prerendering WebContents should not have an opener to avoid cross-page
  // scripting during prerendering.
  EXPECT_FALSE(web_contents_impl.HasOpener());

  // The prerendering WebContents should be hidden until prerender activation.
  EXPECT_TRUE(web_contents_impl.IsHidden());
}

// This is a fake implementation of WebContentsDelegate that allows
// prerendering.
class FakeWebContentsDelegate : public WebContentsDelegate {
 public:
  // WebContentsDelegate overrides.
  PreloadingEligibility IsPrerender2Supported(
      WebContents& web_contents) override {
    return PreloadingEligibility::kEligible;
  }
};

class PrerenderBrowserTest : public ContentBrowserTest,
                             public WebContentsObserver {
 public:
  using LifecycleStateImpl = RenderFrameHostImpl::LifecycleStateImpl;

  enum class OriginType {
    kSameOrigin,
    kSameSiteCrossOrigin,
    kCrossSite,
  };

  PrerenderBrowserTest() {
    prerender_helper_ =
        std::make_unique<test::PrerenderTestHelper>(base::BindRepeating(
            &PrerenderBrowserTest::web_contents, base::Unretained(this)));
    feature_list_.InitWithFeatures(
        {blink::features::kPrerender2MainFrameNavigation,
         ::features::kSuppressesPrerenderingOnSlowNetwork},
        {});
  }
  ~PrerenderBrowserTest() override = default;

  void SetUp() override {
    ssl_server_.RegisterRequestHandler(
        base::BindRepeating(&net::test_server::HandlePrefixedRequest,
                            "/server-redirect-credentialed-prerender",
                            base::BindRepeating(HandleCredentialedRequest)));
    prerender_helper_->RegisterServerRequestMonitor(&ssl_server_);
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    host_resolver()->AddRule("*", "127.0.0.1");
    attempt_ukm_entry_builder_ =
        std::make_unique<test::PreloadingAttemptUkmEntryBuilder>(
            PredictorToExpectInUkm());
    attempt_previous_ukm_entry_builder_ =
        std::make_unique<PreloadingAttemptPreviousPrimaryPageUkmEntryBuilder>(
            PredictorToExpectInUkm());
    prediction_ukm_entry_builder_ =
        std::make_unique<test::PreloadingPredictionUkmEntryBuilder>(
            PredictorToExpectInUkm());
    prediction_previous_ukm_entry_builder_ = std::make_unique<
        PreloadingPredictionPreviousPrimaryPageUkmEntryBuilder>(
        PredictorToExpectInUkm());
    ssl_server_.AddDefaultHandlers(GetTestDataFilePath());
    ssl_server_.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(ssl_server_.Start());
    WebContentsObserver::Observe(shell()->web_contents());

    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    scoped_test_timer_ =
        std::make_unique<base::ScopedMockElapsedTimersForTest>();
  }

  void TearDownOnMainThread() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    EXPECT_TRUE(ssl_server_.ShutdownAndWaitUntilComplete());
  }

  static std::unique_ptr<net::test_server::HttpResponse>
  HandleCredentialedRequest(const net::test_server::HttpRequest& request) {
    GURL request_url = request.GetURL();
    std::string dest =
        base::UnescapeBinaryURLComponent(request_url.query_piece());

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_FOUND);
    http_response->AddCustomHeader("Location", dest);
    http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    http_response->AddCustomHeader("Supports-Loading-Mode",
                                   "credentialed-prerender");
    http_response->set_content_type("text/html");
    http_response->set_content(base::StringPrintf(
        "<!doctype html><p>Redirecting to %s", dest.c_str()));
    return http_response;
  }

  // Waits until the request count for `url` reaches `count`.
  void WaitForRequest(const GURL& url, int count) {
    prerender_helper_->WaitForRequest(url, count);
  }

  FrameTreeNodeId AddPrerender(const GURL& prerendering_url,
                               int32_t world_id = ISOLATED_WORLD_ID_GLOBAL) {
    return prerender_helper_->AddPrerender(prerendering_url, world_id);
  }

  FrameTreeNodeId AddPrerender(const GURL& prerendering_url,
                               std::string no_vary_search_hint,
                               int32_t world_id = ISOLATED_WORLD_ID_GLOBAL) {
    return prerender_helper_->AddPrerender(
        prerendering_url, /*eagerness=*/std::nullopt, no_vary_search_hint,
        /*target_hint=*/std::string(), world_id);
  }

  void AddPrerenderAsync(const GURL& prerendering_url) {
    prerender_helper_->AddPrerenderAsync(prerendering_url);
  }

  void AddPrerenderAsync(const GURL& prerendering_url,
                         std::string no_vary_search_hint) {
    prerender_helper_->AddPrerendersAsync({prerendering_url},
                                          /*eagerness=*/std::nullopt,
                                          no_vary_search_hint,
                                          /*target_hint=*/std::string());
  }

  void AddPrefetchAsync(const GURL& prefetch_url) {
    prerender_helper_->AddPrefetchAsync(prefetch_url);
  }

  void AddPrerendersAsync(const std::vector<GURL>& prerendering_urls) {
    prerender_helper_->AddPrerendersAsync(prerendering_urls, std::nullopt,
                                          std::string());
  }

  void AddPrerendersAsync(
      const std::vector<GURL>& prerendering_urls,
      std::optional<blink::mojom::SpeculationEagerness> eagerness,
      const std::string& target_hint) {
    prerender_helper_->AddPrerendersAsync(prerendering_urls, eagerness,
                                          target_hint);
  }

  void AddPrerenderWithEagernessAsync(
      const GURL& prerendering_url,
      blink::mojom::SpeculationEagerness eagerness) {
    prerender_helper_->AddPrerendersAsync({prerendering_url}, eagerness,
                                          std::string());
  }

  std::unique_ptr<PrerenderHandle> AddEmbedderTriggeredPrerender(
      const GURL& prerendering_url,
      PreloadingAttempt* preloading_attempt = nullptr,
      bool should_warm_up_compositor = false) {
    std::unique_ptr<PrerenderHandle> handle =
        AddEmbedderTriggeredPrerenderAsync(prerendering_url, preloading_attempt,
                                           should_warm_up_compositor);
    EXPECT_TRUE(handle);
    test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(*web_contents(),
                                                              prerendering_url);
    return handle;
  }

  std::unique_ptr<PrerenderHandle> AddEmbedderTriggeredPrerenderAsync(
      const GURL& prerendering_url,
      PreloadingAttempt* preloading_attempt = nullptr,
      bool should_warm_up_compositor = false) {
    return web_contents_impl()->StartPrerendering(
        prerendering_url, PreloadingTriggerType::kEmbedder,
        "EmbedderSuffixForTest",
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
        should_warm_up_compositor, PreloadingHoldbackStatus::kUnspecified,
        preloading_attempt,
        /*url_match_predicate=*/{},
        /*prerender_navigation_handle_callback=*/{});
  }

  bool AddTestUtilJS(RenderFrameHost* host) {
    std::string js = R"(
        const script = document.createElement("script");
        new Promise(resolve => {
          script.addEventListener('load', () => {
            resolve(true);
          });
          script.addEventListener('error', () => {
            resolve(false);
          });
          script.src = "/prerender/test_utils.js";
          document.body.appendChild(script);
        });
    )";
    return EvalJs(host, js).ExtractBool();
  }

  void NavigatePrimaryPage(const GURL& url) {
    prerender_helper_->NavigatePrimaryPage(url);
  }

  void NavigatePrimaryPageFromAddressBar(const GURL& url) {
    web_contents()->OpenURL(
        OpenURLParams(
            url, Referrer(), WindowOpenDisposition::CURRENT_TAB,
            ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                      ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
            /*is_renderer_initiated=*/false),
        /*navigation_handle_callback=*/{});
  }

  FrameTreeNodeId GetHostForUrl(const GURL& url) {
    return prerender_helper_->GetHostForUrl(url);
  }

  RenderFrameHostImpl* GetPrerenderedMainFrameHost(FrameTreeNodeId host_id) {
    return static_cast<RenderFrameHostImpl*>(
        prerender_helper_->GetPrerenderedMainFrameHost(host_id));
  }

  void NavigatePrerenderedPage(FrameTreeNodeId host_id, const GURL& url) {
    return prerender_helper_->NavigatePrerenderedPage(host_id, url);
  }

  void CancelPrerenderedPage(FrameTreeNodeId host_id) {
    return prerender_helper_->CancelPrerenderedPage(host_id);
  }

  bool HasHostForUrl(WebContents& web_contents, const GURL& url) {
    FrameTreeNodeId host_id =
        content::test::PrerenderTestHelper::GetHostForUrl(web_contents, url);
    return !host_id.is_null();
  }

  bool HasHostForUrl(const GURL& url) {
    FrameTreeNodeId host_id = GetHostForUrl(url);
    return !host_id.is_null();
  }

  void WaitForPrerenderLoadCompletion(FrameTreeNodeId host_id) {
    prerender_helper_->WaitForPrerenderLoadCompletion(host_id);
  }

  void WaitForPrerenderLoadCompletion(const GURL& url) {
    prerender_helper_->WaitForPrerenderLoadCompletion(url);
  }

  GURL GetUrl(const std::string& path) const {
    return ssl_server_.GetURL("a.test", path);
  }

  GURL GetSameSiteCrossOriginUrl(const std::string& path) {
    return ssl_server().GetURL("b.a.test", path);
  }

  GURL GetCrossSiteUrl(const std::string& path) {
    return ssl_server_.GetURL("b.test", path);
  }

  void ResetSSLConfig(
      net::test_server::EmbeddedTestServer::ServerCertificate cert,
      const net::SSLServerConfig& ssl_config) {
    ASSERT_TRUE(ssl_server_.ResetSSLConfig(cert, ssl_config));
  }

  int GetRequestCount(const GURL& url) {
    return prerender_helper_->GetRequestCount(url);
  }

  net::test_server::HttpRequest::HeaderMap GetRequestHeaders(const GURL& url) {
    return prerender_helper_->GetRequestHeaders(url);
  }

  WebContents* web_contents() const { return shell()->web_contents(); }

  WebContentsImpl* web_contents_impl() const {
    return static_cast<WebContentsImpl*>(web_contents());
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents_impl()->GetPrimaryMainFrame();
  }

  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return ukm_recorder_.get();
  }

  ukm::SourceId PrimaryPageSourceId() {
    return current_frame_host()->GetPageUkmSourceId();
  }

  const test::PreloadingAttemptUkmEntryBuilder& attempt_ukm_entry_builder() {
    return *attempt_ukm_entry_builder_;
  }

  const PreloadingAttemptPreviousPrimaryPageUkmEntryBuilder&
  attempt_previous_ukm_entry_builder() {
    return *attempt_previous_ukm_entry_builder_;
  }

  const test::PreloadingPredictionUkmEntryBuilder&
  prediction_ukm_entry_builder() {
    return *prediction_ukm_entry_builder_;
  }

  const PreloadingPredictionPreviousPrimaryPageUkmEntryBuilder&
  prediction_previous_ukm_entry_builder() {
    return *prediction_previous_ukm_entry_builder_;
  }

  void ExpectPreloadingAttemptUkm(
      const std::vector<UkmEntry>& expected_attempt_entries) {
    test::ExpectPreloadingAttemptUkm(*test_ukm_recorder(),
                                     expected_attempt_entries);
  }

  void ExpectPreloadingAttemptPreviousPrimaryPageUkm(
      const UkmEntry& expected_attempt_entry) {
    auto attempt_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt_PreviousPrimaryPage::kEntryName,
        test::kPreloadingAttemptUkmMetrics);
    ASSERT_EQ(attempt_entries.size(), 1u);
    EXPECT_EQ(attempt_entries[0], expected_attempt_entry)
        << test::ActualVsExpectedUkmEntryToString(attempt_entries[0],
                                                  expected_attempt_entry);
  }

  void ExpectPreloadingPredictionUkm(
      const std::vector<UkmEntry>& expected_prediction_entries) {
    test::ExpectPreloadingPredictionUkm(*test_ukm_recorder(),
                                        expected_prediction_entries);
  }

  void ExpectPreloadingPredictioPreviousPrimaryPageUkm(
      const UkmEntry& expected_prediction_entry) {
    auto prediction_entries = test_ukm_recorder()->GetEntries(
        Preloading_Prediction_PreviousPrimaryPage::kEntryName,
        test::kPreloadingPredictionUkmMetrics);
    ASSERT_EQ(prediction_entries.size(), 1u);
    EXPECT_EQ(prediction_entries[0], expected_prediction_entry)
        << test::ActualVsExpectedUkmEntryToString(prediction_entries[0],
                                                  expected_prediction_entry);
  }

  void TestHostPrerenderingState(const GURL& prerender_url) {
    const GURL kInitialUrl = GetUrl("/empty.html");

    // Navigate to an initial page.
    ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

    // The initial page should not be in prerendered state.
    RenderFrameHostImpl* initiator_render_frame_host = current_frame_host();
    EXPECT_TRUE(initiator_render_frame_host->frame_tree()->is_primary());
    EXPECT_EQ(initiator_render_frame_host->lifecycle_state(),
              LifecycleStateImpl::kActive);

    // Start a prerender.
    AddPrerender(prerender_url);

    EXPECT_TRUE(prerender_helper_->VerifyPrerenderingState(prerender_url));

    // Activate the prerendered page.
    NavigatePrimaryPage(prerender_url);
    EXPECT_EQ(web_contents()->GetLastCommittedURL(), prerender_url);

    // The activated page should no longer be in the prerendering state.
    RenderFrameHostImpl* navigated_render_frame_host = current_frame_host();
    // The new page shouldn't be in the prerendering state.
    navigated_render_frame_host->ForEachRenderFrameHost(
        [](RenderFrameHostImpl* rfhi) {
          // All the subframes should be transitioned to
          // LifecycleStateImpl::kActive state after activation.
          EXPECT_EQ(rfhi->lifecycle_state(),
                    RenderFrameHostImpl::LifecycleStateImpl::kActive);
          EXPECT_FALSE(rfhi->frame_tree()->is_prerendering());

          // Check that each document can use a deferred Mojo interface. Choose
          // WebLocks API as the feature is enabled by default and does not
          // require permission.
          const std::string kMojoScript = R"(
            navigator.locks.request('hi', {mode:'shared'}, () => {});
          )";
          EXPECT_TRUE(ExecJs(rfhi, kMojoScript));
        });
  }

  void TestPrerenderAllowedOnIframeWithStatusCode(OriginType origin_type,
                                                  std::string status_code);

  test::PrerenderTestHelper* prerender_helper() {
    return prerender_helper_.get();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // The viewport meta tag is only enabled on Android.
#if BUILDFLAG(IS_ANDROID)
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "DisplayCutoutAPI");
#endif
  }

  void TestNavigationHistory(const GURL& expected_current_url,
                             int expected_history_index,
                             int expected_history_length) {
    ASSERT_EQ(expected_current_url, web_contents()->GetLastCommittedURL());
    EXPECT_EQ(expected_history_index,
              web_contents()->GetController().GetCurrentEntryIndex());
    EXPECT_EQ(expected_history_length,
              web_contents()->GetController().GetEntryCount());
    EXPECT_EQ(expected_history_length,
              EvalJs(web_contents(), "history.length"));
  }

  void AssertPrerenderHistoryLength(FrameTreeNodeId host_id,
                                    RenderFrameHost* prerender_frame_host) {
    EXPECT_EQ(1, FrameTreeNode::GloballyFindByID(host_id)
                     ->frame_tree()
                     .controller()
                     .GetEntryCount());
    ASSERT_EQ(1, EvalJs(prerender_frame_host, "history.length"));
  }

  void GoBack() {
    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
  }

  void GoForward() {
    web_contents()->GetController().GoForward();
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
  }

  void ExpectFinalStatus(const std::string& final_status_name,
                         PrerenderFinalStatus status) {
    // Check FinalStatus in UMA.
    histogram_tester().ExpectUniqueSample(final_status_name, status, 1);

    // Check all entries in UKM to make sure that the recorded FinalStatus is
    // equal to `status`. At least one entry should exist.
    bool final_status_entry_found = false;
    const auto entries = ukm_recorder_->GetEntriesByName(
        ukm::builders::PrerenderPageLoad::kEntryName);
    for (const ukm::mojom::UkmEntry* entry : entries) {
      if (ukm_recorder_->EntryHasMetric(
              entry, ukm::builders::PrerenderPageLoad::kFinalStatusName)) {
        final_status_entry_found = true;
        ukm_recorder_->ExpectEntryMetric(
            entry, ukm::builders::PrerenderPageLoad::kFinalStatusName,
            static_cast<int>(status));
      }
    }

    EXPECT_TRUE(final_status_entry_found);
  }

  void ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus status) {
    ExpectFinalStatus(
        "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
        status);
  }

  void ExpectFinalStatusForSpeculationRuleFromIsolatedWorld(
      PrerenderFinalStatus status) {
    ExpectFinalStatus(
        "Prerender.Experimental.PrerenderHostFinalStatus."
        "SpeculationRuleFromIsolatedWorld",
        status);
  }

  void ExpectFinalStatusForSpeculationRuleFromAutoSpeculationRules(
      PrerenderFinalStatus status) {
    ExpectFinalStatus(
        "Prerender.Experimental.PrerenderHostFinalStatus."
        "SpeculationRuleFromAutoSpeculationRules",
        status);
  }

  void ExpectFinalStatusForEmbedder(PrerenderFinalStatus status) {
    // UKM can be recorded in an initiator page and an activated page. Embedder
    // triggers don't have an initiator page, so UKM is not recorded anywhere
    // when prerendering is canceled.
    if (status != PrerenderFinalStatus::kActivated) {
      return;
    }

    ExpectFinalStatus(
        "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
        "EmbedderSuffixForTest",
        status);
  }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

  std::string GetBodyTextContent() {
    base::RunLoop loop;
    base::Value result;
    web_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        base::UTF8ToUTF16(std::string("document.body.textContent.trim()")),
        base::BindOnce(
            [](base::RunLoop* loop, base::Value* result, base::Value value) {
              *result = std::move(value);
              loop->QuitClosure().Run();
            },
            &loop, &result),
        ISOLATED_WORLD_ID_GLOBAL);
    loop.Run();
    CHECK(result.is_string());
    return result.GetString();
  }

  // Stores all the navigation_ids for all navigations. This is used to check
  // that we record UKMs for correct SourceIds.
  std::vector<int64_t> navigation_ids_;

 protected:
  void TestCancelPrerendersWhenTimeout(
      std::vector<Visibility> visibility_transitions);
  void TestCancelOnlyEmbedderTriggeredPrerenderWhenTimeout(
      std::vector<Visibility> visibility_transitions);
  void TestTimerResetWhenPageGoBackToForeground(Visibility visibility);
  void TestCancelPrerenderWithTargetBlankWhenTimeout(Visibility visibility);
  void TestEmbedderTriggerWithUnsupportedScheme(const GURL& prerendering_url);

  net::test_server::EmbeddedTestServer& ssl_server() { return ssl_server_; }

  // Override this in subclasses if you want the test_ukm_recorder() and friends
  // to expect a different predictor.
  virtual PreloadingPredictor PredictorToExpectInUkm() {
    return content_preloading_predictor::kSpeculationRules;
  }

 private:
  void DidStartNavigation(NavigationHandle* handle) override {
    navigation_ids_.push_back(handle->GetNavigationId());
  }

  net::test_server::EmbeddedTestServer ssl_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};

  std::unique_ptr<test::PrerenderTestHelper> prerender_helper_;

  base::HistogramTester histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  std::unique_ptr<test::PreloadingAttemptUkmEntryBuilder>
      attempt_ukm_entry_builder_;
  std::unique_ptr<PreloadingAttemptPreviousPrimaryPageUkmEntryBuilder>
      attempt_previous_ukm_entry_builder_;
  std::unique_ptr<test::PreloadingPredictionUkmEntryBuilder>
      prediction_ukm_entry_builder_;
  std::unique_ptr<PreloadingPredictionPreviousPrimaryPageUkmEntryBuilder>
      prediction_previous_ukm_entry_builder_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<base::ScopedMockElapsedTimersForTest> scoped_test_timer_;
};

class NoVarySearchPrerenderBrowserTest : public PrerenderBrowserTest {
 public:
  using StartedReason = PrerenderHost::WaitingForHeadersStartedReason;
  using FinishedReason = PrerenderHost::WaitingForHeadersFinishedReason;

  NoVarySearchPrerenderBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kPrerender2NoVarySearch,
        {{"wait_for_headers_timeout_eager_prerender", "500"}});
  }

  ~NoVarySearchPrerenderBrowserTest() override = default;

 protected:
  void TestNoVarySearchHeaderFailure(const std::string& no_vary_search_header,
                                     FinishedReason expected_finished_reason);

 private:
  base::test::ScopedFeatureList feature_list_;
};

class DisabledNoVarySearchPrerenderBrowserTest : public PrerenderBrowserTest {
 public:
  DisabledNoVarySearchPrerenderBrowserTest() {
    feature_list_.InitAndDisableFeature(
        blink::features::kPrerender2NoVarySearch);
  }

  ~DisabledNoVarySearchPrerenderBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

class NoVarySearchHintPrerenderHostObserver : public PrerenderHost::Observer {
 public:
  explicit NoVarySearchHintPrerenderHostObserver(
      PrerenderHost& prerender_host) {
    observation_.Observe(&prerender_host);
  }

  void OnWaitingForHeadersStarted(
      NavigationHandle& navigation_handle,
      PrerenderHost::WaitingForHeadersStartedReason reason) override {
    DCHECK(static_cast<NavigationRequest*>(&navigation_handle));
    ASSERT_FALSE(static_cast<NavigationRequest*>(&navigation_handle)
                     ->IsCommitDeferringConditionDeferredForTesting());
    ASSERT_FALSE(wait_for_headers_start_reason_.has_value());
    ASSERT_FALSE(wait_for_headers_finish_reason_.has_value());
    wait_for_headers_start_reason_ = reason;
  }

  void OnWaitingForHeadersFinished(
      NavigationHandle& navigation_handle,
      PrerenderHost::WaitingForHeadersFinishedReason reason) override {
    ASSERT_FALSE(wait_for_headers_finish_reason_.has_value());
    wait_for_headers_finish_reason_ = reason;

    // Reset the observation here, not in Observer::OnHostDestroyed(), as
    // OnWaitingForHeadersFinished() is supposed to be called after that.
    observation_.Reset();
  }

  std::optional<PrerenderHost::WaitingForHeadersStartedReason>
  wait_for_headers_start_reason() const {
    return wait_for_headers_start_reason_;
  }

  std::optional<PrerenderHost::WaitingForHeadersFinishedReason>
  wait_for_headers_finish_reason() const {
    return wait_for_headers_finish_reason_;
  }

 private:
  std::optional<PrerenderHost::WaitingForHeadersStartedReason>
      wait_for_headers_start_reason_;
  std::optional<PrerenderHost::WaitingForHeadersFinishedReason>
      wait_for_headers_finish_reason_;

  base::ScopedObservation<PrerenderHost, PrerenderHost::Observer> observation_{
      this};
};

}  // namespace

// Test that the timer is enabled and cleared appropriately when navigating to
// a No-Vary-Search hint matched prerender successfully.
IN_PROC_BROWSER_TEST_F(
    NoVarySearchPrerenderBrowserTest,
    EagerTimerWorksCorrectlyForHeadersThatArriveBeforeTimeout) {
  const std::string kTestingRelativeUrl =
      "/delayed_with_no_vary_search?prerender";
  const std::string kPrerenderingRelativeUrl = kTestingRelativeUrl + "&a=5";
  // Create a HTTP response to control prerendering main-frame navigation.
  net::test_server::ControllableHttpResponse main_prerender_response(
      embedded_test_server(), kPrerenderingRelativeUrl);

  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerenderingUrl =
      embedded_test_server()->GetURL(kPrerenderingRelativeUrl);
  const GURL kNavigationUrl =
      embedded_test_server()->GetURL(kTestingRelativeUrl + "&a=3");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Inject mock time task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  PrerenderNoVarySearchHintCommitDeferringCondition::
      SetTimerTaskRunnerForTesting(task_runner);

  // Start prerendering `kPrerenderingUrl`.
  content::test::PrerenderHostCreationWaiter host_creation_waiter;
  AddPrerenderAsync(kPrerenderingUrl, R"(params=(\\\"a\\\"))");
  FrameTreeNodeId host_id = host_creation_waiter.Wait();
  auto* host =
      web_contents_impl()->GetPrerenderHostRegistry()->FindNonReservedHostById(
          host_id);
  ASSERT_TRUE(host);
  ASSERT_TRUE(host->no_vary_search_expected().has_value());

  // Add a testing PrerenderHost::Observer to the prerender host that we'd like
  // to activate.
  NoVarySearchHintPrerenderHostObserver observer(*host);

  // Start navigation in primary page to kNavigationUrl.
  TestActivationManager primary_page_manager(web_contents(), kNavigationUrl);
  // Start to navigate to kNavigationUrl
  std::unique_ptr<content::TestNavigationObserver> nav_observer =
      test::PrerenderTestHelper::NavigatePrimaryPageAsync(*web_contents_impl(),
                                                          kNavigationUrl);

  // Wait until the navigation is deferred by CommitDeferringCondition.
  ASSERT_TRUE(primary_page_manager.WaitForBeforeChecks());
  primary_page_manager.ResumeActivation();
  ASSERT_FALSE(host->were_headers_received());

  ASSERT_TRUE(host->WaitUntilHeadTimeout().is_positive());

  auto* prerender_web_contents =
      content::WebContents::FromFrameTreeNodeId(host_id);
  content::test::PrerenderHostObserver host_observer(*prerender_web_contents,
                                                     host_id);

  // Advance the prerender http response by sending headers.
  main_prerender_response.WaitForRequest();

  // Advance timer for half the wait until head timeout.
  task_runner->FastForwardBy(host->WaitUntilHeadTimeout() / 2);

  main_prerender_response.Send(
      net::HTTP_OK, /*content_type=*/"text/html", /*content=*/"",
      /*cookies=*/{}, /*extra_headers=*/{"No-Vary-Search: params=(\"a\")"});
  main_prerender_response.Send("Some Content");
  main_prerender_response.Done();

  ASSERT_TRUE(primary_page_manager.WaitForAfterChecks());
  primary_page_manager.ResumeActivation();

  // Wait for the navigation to finish.
  nav_observer->Wait();
  primary_page_manager.WaitForNavigationFinished();
  // Check that the prerender host was activated.
  ASSERT_TRUE(host_observer.was_activated());

  // Ensure the state has been propagated to renderer processes.
  ASSERT_EQ(false, EvalJs(web_contents(), "document.prerendering"));

  // The prerender host should be consumed.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  ASSERT_TRUE(observer.wait_for_headers_start_reason().has_value());
  ASSERT_TRUE(observer.wait_for_headers_finish_reason().has_value());

  EXPECT_EQ(observer.wait_for_headers_start_reason().value(),
            StartedReason::kWithTimeout);
  EXPECT_EQ(observer.wait_for_headers_finish_reason().value(),
            FinishedReason::kNoVarySearchHeaderReceivedAndMatched);

  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.WaitingForHeadersFinishedReason.SpeculationRule",
      FinishedReason::kNoVarySearchHeaderReceivedAndMatched, 1);

  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.MatchableHostCountOnActivation", 1, 1);
}

// Test that the timer is enabled and cleared appropriately when navigating to
// a No-Vary-Search hint matched prerender with timeout waiting for headers.
IN_PROC_BROWSER_TEST_F(
    NoVarySearchPrerenderBrowserTest,
    EagerTimerWorksCorrectlyForHeadersThatArriveAfterTimeout) {
  const std::string kTestingRelativeUrl =
      "/delayed_with_no_vary_search?prerender";
  const std::string kPrerenderingRelativeUrl = kTestingRelativeUrl + "&a=5";
  // Create a HTTP response to control prerendering main-frame navigation.
  net::test_server::ControllableHttpResponse main_prerender_response(
      embedded_test_server(), kPrerenderingRelativeUrl);
  const std::string kNavigationRelativeUrl = kTestingRelativeUrl + "&a=3";
  // Create a HTTP response to control the navigation in main-frame.
  net::test_server::ControllableHttpResponse main_navigation_response(
      embedded_test_server(), kNavigationRelativeUrl);

  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerenderingUrl =
      embedded_test_server()->GetURL(kPrerenderingRelativeUrl);
  const GURL kNavigationUrl =
      embedded_test_server()->GetURL(kNavigationRelativeUrl);

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Inject mock time task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  PrerenderNoVarySearchHintCommitDeferringCondition::
      SetTimerTaskRunnerForTesting(task_runner);

  // Start prerendering `kPrerenderingUrl`.
  content::test::PrerenderHostCreationWaiter host_creation_waiter;
  AddPrerenderAsync(kPrerenderingUrl, R"(params=(\\\"a\\\"))");
  FrameTreeNodeId host_id = host_creation_waiter.Wait();
  auto* host =
      web_contents_impl()->GetPrerenderHostRegistry()->FindNonReservedHostById(
          host_id);
  ASSERT_TRUE(host);
  ASSERT_TRUE(host->no_vary_search_expected().has_value());

  // Add a testing PrerenderHost::Observer to the prerender host that we'd like
  // to activate.
  NoVarySearchHintPrerenderHostObserver observer(*host);

  // Start navigation in primary page to kNavigationUrl.
  TestActivationManager primary_page_manager(web_contents(), kNavigationUrl);
  // Start to navigate to kNavigationUrl
  std::unique_ptr<content::TestNavigationObserver> nav_observer =
      test::PrerenderTestHelper::NavigatePrimaryPageAsync(*web_contents_impl(),
                                                          kNavigationUrl);

  // Wait until the navigation is deferred by CommitDeferringCondition.
  ASSERT_TRUE(primary_page_manager.WaitForBeforeChecks());
  primary_page_manager.ResumeActivation();
  ASSERT_FALSE(host->were_headers_received());

  ASSERT_TRUE(host->WaitUntilHeadTimeout().is_positive());

  auto* prerender_web_contents =
      content::WebContents::FromFrameTreeNodeId(host_id);
  content::test::PrerenderHostObserver host_observer(*prerender_web_contents,
                                                     host_id);

  // Advance the prerender http response by sending headers.
  main_prerender_response.WaitForRequest();

  // Advance timer for twice the wait until head timeout.
  task_runner->FastForwardBy(2 * host->WaitUntilHeadTimeout());

  main_prerender_response.Send(
      net::HTTP_OK, /*content_type=*/"text/html", /*content=*/"",
      /*cookies=*/{}, /*extra_headers=*/{"No-Vary-Search: params=(\"a\")"});
  main_prerender_response.Send("Some Content");
  main_prerender_response.Done();

  ASSERT_TRUE(primary_page_manager.WaitForAfterChecks());

  main_navigation_response.WaitForRequest();
  main_navigation_response.Send("Some Content");
  main_navigation_response.Done();

  // Wait for the navigation to finish.
  nav_observer->Wait();
  primary_page_manager.WaitForNavigationFinished();
  // Check that the prerender host was activated.
  ASSERT_FALSE(host_observer.was_activated());

  // Ensure the state has been propagated to renderer processes.
  ASSERT_EQ(false, EvalJs(web_contents(), "document.prerendering"));

  // The prerender host should be destroyed.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  ASSERT_TRUE(observer.wait_for_headers_start_reason().has_value());
  ASSERT_TRUE(observer.wait_for_headers_finish_reason().has_value());

  EXPECT_EQ(observer.wait_for_headers_start_reason().value(),
            StartedReason::kWithTimeout);
  EXPECT_EQ(observer.wait_for_headers_finish_reason().value(),
            FinishedReason::kTimeoutElapsed);

  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.WaitingForHeadersFinishedReason.SpeculationRule",
      FinishedReason::kTimeoutElapsed, 1);

  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.MatchableHostCountOnActivation", 1, 1);
}

// Helper function to test cases where `no_vary_search_header` that does not
// match the No-Vary-Search hint is served and results in activation mismatch.
void NoVarySearchPrerenderBrowserTest::TestNoVarySearchHeaderFailure(
    const std::string& no_vary_search_header,
    FinishedReason expected_finished_reason) {
  const std::string kTestingRelativeUrl =
      "/delayed_with_no_vary_search?prerender";
  const std::string kPrerenderingRelativeUrl = kTestingRelativeUrl + "&a=5";
  // Create a HTTP response to control prerendering main-frame navigation.
  net::test_server::ControllableHttpResponse main_prerender_response(
      embedded_test_server(), kPrerenderingRelativeUrl);

  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerenderingUrl =
      embedded_test_server()->GetURL(kPrerenderingRelativeUrl);
  const GURL kNavigationUrl =
      embedded_test_server()->GetURL(kTestingRelativeUrl + "&a=3");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Inject mock time task runner to avoid timeout.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  PrerenderNoVarySearchHintCommitDeferringCondition::
      SetTimerTaskRunnerForTesting(task_runner);

  // Start prerendering `kPrerenderingUrl`.
  content::test::PrerenderHostCreationWaiter host_creation_waiter;
  AddPrerenderAsync(kPrerenderingUrl, R"(params=(\\\"a\\\"))");
  FrameTreeNodeId host_id = host_creation_waiter.Wait();
  auto* host =
      web_contents_impl()->GetPrerenderHostRegistry()->FindNonReservedHostById(
          host_id);
  ASSERT_TRUE(host);
  ASSERT_TRUE(host->no_vary_search_expected().has_value());

  // Add a testing PrerenderHost::Observer to the prerender host that we'd like
  // to activate.
  NoVarySearchHintPrerenderHostObserver observer(*host);

  // Start navigation in primary page to kNavigationUrl.
  TestActivationManager primary_page_manager(web_contents(), kNavigationUrl);
  // Start to navigate to kNavigationUrl
  std::unique_ptr<content::TestNavigationObserver> nav_observer =
      test::PrerenderTestHelper::NavigatePrimaryPageAsync(*web_contents_impl(),
                                                          kNavigationUrl);

  // Wait until the navigation is deferred by CommitDeferringCondition.
  ASSERT_TRUE(primary_page_manager.WaitForBeforeChecks());
  primary_page_manager.ResumeActivation();
  ASSERT_FALSE(host->were_headers_received());

  ASSERT_TRUE(host->WaitUntilHeadTimeout().is_positive());

  auto* prerender_web_contents =
      content::WebContents::FromFrameTreeNodeId(host_id);
  content::test::PrerenderHostObserver host_observer(*prerender_web_contents,
                                                     host_id);

  // Advance the prerender http response by sending headers.
  main_prerender_response.WaitForRequest();

  main_prerender_response.Send(net::HTTP_OK, /*content_type=*/"text/html",
                               /*content=*/"",
                               /*cookies=*/{}, {no_vary_search_header});
  main_prerender_response.Send("Some Content");
  main_prerender_response.Done();

  // Wait for the navigation to finish.
  nav_observer->Wait();
  primary_page_manager.WaitForNavigationFinished();

  // Check that the prerender host was not activated as the header was not
  // valid.
  ASSERT_FALSE(host_observer.was_activated());

  ASSERT_TRUE(observer.wait_for_headers_start_reason().has_value());
  ASSERT_TRUE(observer.wait_for_headers_finish_reason().has_value());

  EXPECT_EQ(observer.wait_for_headers_start_reason().value(),
            StartedReason::kWithTimeout);
  EXPECT_EQ(observer.wait_for_headers_finish_reason().value(),
            expected_finished_reason);

  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.WaitingForHeadersFinishedReason.SpeculationRule",
      expected_finished_reason, 1);

  histogram_tester().ExpectTotalCount(
      "Prerender.Experimental.MatchableHostCountOnActivation", 1);
}

// Test that a No-Vary-Search header is malformed.
IN_PROC_BROWSER_TEST_F(NoVarySearchPrerenderBrowserTest,
                       MalformedNoVarySearchHeader) {
  TestNoVarySearchHeaderFailure("No-Vary-Search: malformed(\"a\")",
                                FinishedReason::kNoVarySearchHeaderParseFailed);
}

// Test that a No-Vary-Search header is default value.
IN_PROC_BROWSER_TEST_F(NoVarySearchPrerenderBrowserTest,
                       NoVarySearchHeaderWithDefaultValue) {
  TestNoVarySearchHeaderFailure(
      "No-Vary-Search: params=()",
      FinishedReason::kNoVarySearchHeaderReceivedButDefaultValue);
}

// Test that a No-Vary-Search header is not served.
IN_PROC_BROWSER_TEST_F(NoVarySearchPrerenderBrowserTest, NoNoVarySearchHeader) {
  TestNoVarySearchHeaderFailure("",
                                FinishedReason::kNoVarySearchHeaderNotReceived);
}

// Test that a No-Vary-Search header is received but does not match.
IN_PROC_BROWSER_TEST_F(NoVarySearchPrerenderBrowserTest,
                       UnmatchedNoVarySearchHeader) {
  TestNoVarySearchHeaderFailure(
      "No-Vary-Search: params=(\"different\")",
      FinishedReason::kNoVarySearchHeaderReceivedButNotMatched);
}

// Test that activation is successful when navigating to an inexact URL
// before No-Vary-Search header is back from the server, if the No-Vary-Search
// header is matching when it is received.
IN_PROC_BROWSER_TEST_F(NoVarySearchPrerenderBrowserTest,
                       HintActivationSuccessful) {
  const std::string kTestingRelativeUrl =
      "/delayed_with_no_vary_search?prerender";
  const std::string kPrerenderingRelativeUrl = kTestingRelativeUrl + "&a=5";
  // Create a HTTP response to control prerendering main-frame navigation.
  net::test_server::ControllableHttpResponse main_prerender_response(
      embedded_test_server(), kPrerenderingRelativeUrl);

  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerenderingUrl =
      embedded_test_server()->GetURL(kPrerenderingRelativeUrl);
  const GURL kNavigationUrl =
      embedded_test_server()->GetURL(kTestingRelativeUrl + "&a=3");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prerendering `kPrerenderingUrl`.
  content::test::PrerenderHostCreationWaiter host_creation_waiter;
  AddPrerenderAsync(kPrerenderingUrl, R"(params=(\\\"a\\\"))");
  FrameTreeNodeId host_id = host_creation_waiter.Wait();
  auto* host =
      web_contents_impl()->GetPrerenderHostRegistry()->FindNonReservedHostById(
          host_id);
  ASSERT_TRUE(host);
  ASSERT_TRUE(host->no_vary_search_expected().has_value());

  // Add a PrerenderHost::Observer with default behaviour to increase
  // code coverage.
  PrerenderHost::Observer empty_observer;
  host->AddObserver(&empty_observer);

  NavigationHandleObserver activation_observer(web_contents(), kNavigationUrl);
  // Start navigation in primary page to kNavigationUrl.
  TestActivationManager primary_page_manager(shell()->web_contents(),
                                             kNavigationUrl);
  // Start to navigate to kNavigationUrl
  std::unique_ptr<content::TestNavigationObserver> nav_observer =
      test::PrerenderTestHelper::NavigatePrimaryPageAsync(*web_contents_impl(),
                                                          kNavigationUrl);

  // Wait until the navigation is deferred by CommitDeferringCondition.
  ASSERT_TRUE(primary_page_manager.WaitForBeforeChecks());
  primary_page_manager.ResumeActivation();
  ASSERT_FALSE(host->were_headers_received());

  // Make sure that the prerender host is not a match by IsUrlMatch.
  ASSERT_FALSE(host->IsUrlMatch(kNavigationUrl));
  auto* prerender_web_contents =
      content::WebContents::FromFrameTreeNodeId(host_id);
  content::test::PrerenderHostObserver host_observer(*prerender_web_contents,
                                                     host_id);

  // Check that PrerenderNoVarySearchHintCommitDeferringCondition is deferring
  // the commit.
  NavigationRequest* nav_request =
      web_contents_impl()->GetPrimaryFrameTree().root()->navigation_request();
  EXPECT_TRUE(nav_request->IsCommitDeferringConditionDeferredForTesting());

  // The navigation should not have proceeded past NOT_STARTED because the
  // PrerenderCommitDeferringCondition is deferring it.
  EXPECT_EQ(nav_request->state(), NavigationRequest::NOT_STARTED);

  // Advance the prerender http response by sending headers.
  main_prerender_response.WaitForRequest();
  main_prerender_response.Send(
      net::HTTP_OK, /*content_type=*/"text/html", /*content=*/"",
      /*cookies=*/{}, /*extra_headers=*/{"No-Vary-Search: params=(\"a\")"});
  host_observer.WaitForHeaders();
  ASSERT_TRUE(host->were_headers_received());
  // Make sure that, after receiving headers the prerender host is a match by
  // IsUrlMatch.
  ASSERT_TRUE(host->IsUrlMatch(kNavigationUrl));
  EXPECT_TRUE(nav_request->IsCommitDeferringConditionDeferredForTesting());
  main_prerender_response.Send("Some Content");
  main_prerender_response.Done();

  ASSERT_TRUE(primary_page_manager.WaitForAfterChecks());
  primary_page_manager.ResumeActivation();

  // Wait for the navigation to finish.
  nav_observer->Wait();
  primary_page_manager.WaitForNavigationFinished();
  // Check that the prerender host was activated.
  ASSERT_TRUE(host_observer.was_activated());

  // Ensure the state has been propagated to renderer processes.
  ASSERT_EQ(false, EvalJs(web_contents(), "document.prerendering"));

  // The prerender host should be consumed.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  // Activating the prerendered page should not issue a request.
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kNavigationUrl);

  ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
  ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder().BuildEntry(
      ukm_source_id, PreloadingType::kPrerender,
      PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
      PreloadingTriggeringOutcome::kSuccess,
      PreloadingFailureReason::kUnspecified,
      /*accurate=*/true,
      /*ready_time=*/kMockElapsedTime,
      blink::mojom::SpeculationEagerness::kEager)});

  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.MatchableHostCountOnActivation", 1, 1);
}

// Test that activation is not successful when navigating to an inexact URL
// before No-Vary-Search header is back from the server if the No-Vary-Search
// header is not matching when it is received.
IN_PROC_BROWSER_TEST_F(NoVarySearchPrerenderBrowserTest,
                       HintActivationUnsuccessful) {
  const std::string kTestingRelativeUrl =
      "/delayed_without_no_vary_search?prerender";
  const std::string kPrerenderingRelativeUrl = kTestingRelativeUrl + "&a=5";
  // Create a HTTP response to control prerendering main-frame navigation.
  net::test_server::ControllableHttpResponse main_prerender_response(
      embedded_test_server(), kPrerenderingRelativeUrl);

  const std::string kNavigationRelativeUrl = kTestingRelativeUrl + "&a=3";
  // Create a HTTP response to control main-frame navigation.
  net::test_server::ControllableHttpResponse main_navigation_response(
      embedded_test_server(), kNavigationRelativeUrl);

  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerenderingUrl =
      embedded_test_server()->GetURL(kPrerenderingRelativeUrl);
  const GURL kNavigationUrl =
      embedded_test_server()->GetURL(kNavigationRelativeUrl);

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prerendering `kPrerenderingUrl`.
  content::test::PrerenderHostCreationWaiter host_creation_waiter;
  AddPrerenderAsync(kPrerenderingUrl, R"(params=(\\\"a\\\"))");
  FrameTreeNodeId host_id = host_creation_waiter.Wait();
  auto* host =
      web_contents_impl()->GetPrerenderHostRegistry()->FindNonReservedHostById(
          host_id);
  ASSERT_TRUE(host);
  ASSERT_TRUE(host->no_vary_search_expected().has_value());

  TestActivationManager primary_page_manager(shell()->web_contents(),
                                             kNavigationUrl);
  // Start navigation to kNavigationUrl.
  std::unique_ptr<content::TestNavigationObserver> nav_observer =
      test::PrerenderTestHelper::NavigatePrimaryPageAsync(*web_contents_impl(),
                                                          kNavigationUrl);
  // Wait until the navigation is deferred by CommitDeferringCondition.
  ASSERT_TRUE(primary_page_manager.WaitForBeforeChecks());
  primary_page_manager.ResumeActivation();

  ASSERT_FALSE(host->were_headers_received());
  NavigationRequest* nav_request =
      web_contents_impl()->GetPrimaryFrameTree().root()->navigation_request();
  // Make sure PrerenderHostRegistry selects this prerender as a potential
  // prerender host to activate.
  ASSERT_TRUE(host->IsNoVarySearchHintUrlMatch(kNavigationUrl));
  // Make sure that the prerender host is not a match by IsUrlMatch.
  ASSERT_FALSE(host->IsUrlMatch(kNavigationUrl));

  // Check that PrerenderNoVarySearchHintCommitDeferringCondition is deferring
  // the commit.
  EXPECT_TRUE(nav_request->IsCommitDeferringConditionDeferredForTesting());

  // The navigation should not have proceeded past NOT_STARTED because the
  // PrerenderCommitDeferringCondition is deferring it.
  EXPECT_EQ(nav_request->state(), NavigationRequest::NOT_STARTED);

  auto* prerender_web_contents =
      content::WebContents::FromFrameTreeNodeId(host_id);
  content::test::PrerenderHostObserver host_observer(*prerender_web_contents,
                                                     host_id);
  // Advance the prerender http response by sending headers.
  main_prerender_response.WaitForRequest();
  main_prerender_response.Send(net::HTTP_OK, /*content_type=*/"text/html",
                               /*content=*/"Some Content");
  host_observer.WaitForHeaders();
  ASSERT_TRUE(host->were_headers_received());
  // Make sure that, after receiving headers the prerender host is not a match
  // by IsUrlMatch.
  ASSERT_FALSE(host->IsUrlMatch(kNavigationUrl));
  main_prerender_response.Done();

  main_navigation_response.WaitForRequest();
  main_navigation_response.Send(net::HTTP_OK, /*content_type=*/"text/html",
                                /*content=*/"Other Content");
  main_navigation_response.Done();

  primary_page_manager.WaitForNavigationFinished();
  // Check that the prerender host was not activated.
  ASSERT_FALSE(host_observer.was_activated());

  // Wait for the navigation to finish.
  nav_observer->Wait();
  // The navigation should issue a request.
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kNavigationUrl);

  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.MatchableHostCountOnActivation", 1, 1);
}

// Test that activation is successful when navigating to an exact URL before
// No-Vary-Search header is back from the server.
IN_PROC_BROWSER_TEST_F(NoVarySearchPrerenderBrowserTest,
                       HintActivationSuccessful_ExactUrl) {
  const std::string testing_relative_url =
      "/delayed_with_no_vary_search?prerender";
  const std::string prerendering_relative_url = testing_relative_url + "&a=5";
  // Create a HTTP response to control prerendering main-frame navigation.
  net::test_server::ControllableHttpResponse main_prerender_response(
      embedded_test_server(), prerendering_relative_url);

  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  const GURL prerendering_url =
      embedded_test_server()->GetURL(prerendering_relative_url);
  const GURL navigation_url =
      embedded_test_server()->GetURL(prerendering_relative_url);

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), initial_url);

  // Start prerendering `kPrerenderingUrl`.
  content::test::PrerenderHostCreationWaiter host_creation_waiter;
  AddPrerenderAsync(prerendering_url, R"(params=(\\\"a\\\"))");
  FrameTreeNodeId host_id = host_creation_waiter.Wait();
  auto* host =
      web_contents_impl()->GetPrerenderHostRegistry()->FindNonReservedHostById(
          host_id);
  ASSERT_TRUE(host);
  ASSERT_TRUE(host->no_vary_search_expected().has_value());

  NavigationHandleObserver activation_observer(web_contents(), navigation_url);
  // Start navigation in primary page to kNavigationUrl.
  TestActivationManager primary_page_manager(shell()->web_contents(),
                                             navigation_url);
  // Start to navigate to kNavigationUrl
  std::unique_ptr<content::TestNavigationObserver> nav_observer =
      test::PrerenderTestHelper::NavigatePrimaryPageAsync(*web_contents_impl(),
                                                          navigation_url);

  // Wait until the navigation is deferred by CommitDeferringCondition.
  ASSERT_TRUE(primary_page_manager.WaitForBeforeChecks());
  primary_page_manager.ResumeActivation();
  ASSERT_FALSE(host->were_headers_received());

  // Make sure that the prerender host is a match by IsUrlMatch regardless of
  // the No-Vary-Search header.
  std::optional<UrlMatchType> match_type = host->IsUrlMatch(navigation_url);
  ASSERT_TRUE(match_type.has_value());
  ASSERT_EQ(match_type.value(), UrlMatchType::kExact);
  auto* prerender_web_contents =
      content::WebContents::FromFrameTreeNodeId(host_id);
  content::test::PrerenderHostObserver host_observer(*prerender_web_contents,
                                                     host_id);

  // Check that PrerenderCommitDeferringCondition is deferring the commit.
  NavigationRequest* nav_request =
      web_contents_impl()->GetPrimaryFrameTree().root()->navigation_request();
  EXPECT_TRUE(nav_request->IsCommitDeferringConditionDeferredForTesting());

  // The navigation should not have proceeded past NOT_STARTED because the
  // PrerenderCommitDeferringCondition is deferring it.
  EXPECT_EQ(nav_request->state(), NavigationRequest::NOT_STARTED);

  // Advance the prerender http response by sending headers.
  main_prerender_response.WaitForRequest();
  main_prerender_response.Send(
      net::HTTP_OK, /*content_type=*/"text/html", /*content=*/"",
      /*cookies=*/{}, /*extra_headers=*/{"No-Vary-Search: params=(\"a\")"});
  host_observer.WaitForHeaders();
  ASSERT_TRUE(host->were_headers_received());
  // Make sure that, after receiving headers the prerender host is still a match
  // by IsUrlMatch.
  ASSERT_TRUE(host->IsUrlMatch(navigation_url));
  EXPECT_TRUE(nav_request->IsCommitDeferringConditionDeferredForTesting());
  main_prerender_response.Send("Some Content");
  main_prerender_response.Done();

  ASSERT_TRUE(primary_page_manager.WaitForAfterChecks());
  primary_page_manager.ResumeActivation();

  // Wait for the navigation to finish.
  nav_observer->Wait();
  primary_page_manager.WaitForNavigationFinished();
  // Check that the prerender host was activated.
  ASSERT_TRUE(host_observer.was_activated());
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);

  // PrerenderNoVarySearchHintCommitDeferringCondition was not be involved in
  // prerender activation, so the metric should not be recorded.
  histogram_tester().ExpectTotalCount(
      "Prerender.Experimental.WaitingForHeadersFinishedReason.SpeculationRule",
      0);

  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.MatchableHostCountOnActivation", 1, 1);
}

// Test that activation is successful when 2 matchable PrerenderHosts exist.
IN_PROC_BROWSER_TEST_F(NoVarySearchPrerenderBrowserTest,
                       MultipleMatchableHosts) {
  const std::string testing_relative_url =
      "/delayed_with_no_vary_search?prerender";
  const std::string prerendering_relative_url1 = testing_relative_url + "&a=5";
  const std::string prerendering_relative_url2 = testing_relative_url + "&a=7";
  // Create a HTTP response to control prerendering main-frame navigation.
  net::test_server::ControllableHttpResponse main_prerender_response1(
      embedded_test_server(), prerendering_relative_url1);
  const std::string navigation_relative_url = testing_relative_url + "&a=3";

  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  const GURL prerendering_url1 =
      embedded_test_server()->GetURL(prerendering_relative_url1);
  const GURL prerendering_url2 =
      embedded_test_server()->GetURL(prerendering_relative_url2);
  const GURL navigation_url =
      embedded_test_server()->GetURL(navigation_relative_url);

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), initial_url);

  // Start prerendering `prerendering_url1`.
  test::PrerenderHostCreationWaiter host_creation_waiter1;
  AddPrerenderAsync(prerendering_url1, R"(params=(\\\"a\\\"))");
  FrameTreeNodeId host_id1 = host_creation_waiter1.Wait();
  auto* host1 =
      web_contents_impl()->GetPrerenderHostRegistry()->FindNonReservedHostById(
          host_id1);
  ASSERT_TRUE(host1);
  ASSERT_TRUE(host1->no_vary_search_expected().has_value());
  test::PrerenderHostObserver host_observer1(*web_contents(), host_id1);

  // Start prerendering `prerendering_url2`.
  test::PrerenderHostCreationWaiter host_creation_waiter2;
  AddPrerenderAsync(prerendering_url2, R"(params=(\\\"a\\\"))");
  FrameTreeNodeId host_id2 = host_creation_waiter2.Wait();
  auto* host2 =
      web_contents_impl()->GetPrerenderHostRegistry()->FindNonReservedHostById(
          host_id2);
  ASSERT_TRUE(host2);
  ASSERT_TRUE(host2->no_vary_search_expected().has_value());
  test::PrerenderHostObserver host_observer2(*web_contents(), host_id2);

  NavigationHandleObserver activation_observer(web_contents(), navigation_url);
  // Start navigation in primary page to navigation_url.
  TestActivationManager primary_page_manager(shell()->web_contents(),
                                             navigation_url);
  // Start to navigate to navigation_url.
  std::unique_ptr<TestNavigationObserver> nav_observer =
      test::PrerenderTestHelper::NavigatePrimaryPageAsync(*web_contents(),
                                                          navigation_url);

  // Wait until the navigation is deferred by CommitDeferringCondition.
  ASSERT_TRUE(primary_page_manager.WaitForBeforeChecks());
  primary_page_manager.ResumeActivation();
  ASSERT_FALSE(host1->were_headers_received());

  // The prererender 2 should be destroyed as the prerender 1 is chosen for
  // activation.
  host_observer2.WaitForDestroyed();
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kOtherPrerenderedPageActivated);

  // Check that PrerenderCommitDeferringCondition is deferring the commit.
  NavigationRequest* nav_request =
      web_contents_impl()->GetPrimaryFrameTree().root()->navigation_request();
  EXPECT_TRUE(nav_request->IsCommitDeferringConditionDeferredForTesting());

  // The navigation should not have proceeded past NOT_STARTED because the
  // PrerenderCommitDeferringCondition is deferring it.
  EXPECT_EQ(nav_request->state(), NavigationRequest::NOT_STARTED);

  // Advance the prerender http response 1 by sending headers.
  main_prerender_response1.WaitForRequest();
  main_prerender_response1.Send(
      net::HTTP_OK, /*content_type=*/"text/html", /*content=*/"",
      /*cookies=*/{}, /*extra_headers=*/{"No-Vary-Search: params=(\"a\")"});
  host_observer1.WaitForHeaders();
  ASSERT_TRUE(host1->were_headers_received());
  // Make sure that, after receiving headers the prerender host is still a match
  // by IsUrlMatch.
  ASSERT_TRUE(host1->IsUrlMatch(navigation_url));
  main_prerender_response1.Send("Some Content");
  main_prerender_response1.Done();

  ASSERT_TRUE(primary_page_manager.WaitForAfterChecks());
  primary_page_manager.ResumeActivation();

  // Wait for the navigation to finish.
  nav_observer->Wait();
  primary_page_manager.WaitForNavigationFinished();
  // Check that the prerender host was activated.
  ASSERT_TRUE(host_observer1.was_activated());
  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kActivated, 1);

  histogram_tester().ExpectTotalCount(
      "Prerender.Experimental.WaitingForHeadersFinishedReason.SpeculationRule",
      1);

  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.MatchableHostCountOnActivation", 2, 1);
}

// Tests that the speculationrules No-Vary-Search hint is populated for the
// PrerenderHost.
IN_PROC_BROWSER_TEST_F(NoVarySearchPrerenderBrowserTest, HintIsPopulated) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/no_vary_search_a.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prerendering `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  FrameTreeNodeId host_id =
      AddPrerender(kPrerenderingUrl, R"(params=(\\\"a\\\"))");
  auto* host =
      web_contents_impl()->GetPrerenderHostRegistry()->FindNonReservedHostById(
          host_id);
  ASSERT_TRUE(host);
  ASSERT_TRUE(host->no_vary_search_expected().has_value());
}

// Tests that the speculationrules trigger works in the presence of
// No-Vary-Search for same URL.
IN_PROC_BROWSER_TEST_F(NoVarySearchPrerenderBrowserTest, ExactUrlMatch) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/no_vary_search_a.html?prerender");
  const GURL kNavigationUrl = kPrerenderingUrl;

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prerendering `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  ASSERT_TRUE(host_id);
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  NavigationHandleObserver activation_observer(web_contents(), kNavigationUrl);
  NavigatePrimaryPage(kNavigationUrl);
  // Ensure the state has been propagated to renderer processes.
  ASSERT_EQ(false, EvalJs(web_contents(), "document.prerendering"));

  // The prerender host should be consumed.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  // Activating the prerendered page should not issue a request.
  EXPECT_EQ(GetRequestCount(kNavigationUrl), 1);
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kNavigationUrl);

  ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
  ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder().BuildEntry(
      ukm_source_id, PreloadingType::kPrerender,
      PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
      PreloadingTriggeringOutcome::kSuccess,
      PreloadingFailureReason::kUnspecified,
      /*accurate=*/true,
      /*ready_time=*/kMockElapsedTime,
      blink::mojom::SpeculationEagerness::kEager)});
}

// Tests that the speculationrules trigger works in the presence of
// No-Vary-Search.
IN_PROC_BROWSER_TEST_F(NoVarySearchPrerenderBrowserTest, InexactUrlMatch) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/no_vary_search_a.html?prerender");
  const GURL kNavigationUrl = GetUrl("/no_vary_search_a.html?prerender&a=3");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prerendering `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  ASSERT_TRUE(host_id);
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  NavigationHandleObserver activation_observer(web_contents(), kNavigationUrl);
  NavigatePrimaryPage(kNavigationUrl);
  // Ensure the state has been propagated to renderer processes.
  ASSERT_EQ(false, EvalJs(web_contents(), "document.prerendering"));

  // The prerender host should be consumed.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  EXPECT_FALSE(HasHostForUrl(kNavigationUrl));
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  // Activating the prerendered page should not issue a request.
  EXPECT_EQ(GetRequestCount(kNavigationUrl), 0);
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kNavigationUrl);
  ASSERT_EQ(kNavigationUrl, EvalJs(web_contents(), "window.location.href"));

  // URL match was inexact but should be recorded as accurate.
  ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
  ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder().BuildEntry(
      ukm_source_id, PreloadingType::kPrerender,
      PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
      PreloadingTriggeringOutcome::kSuccess,
      PreloadingFailureReason::kUnspecified,
      /*accurate=*/true,
      /*ready_time=*/kMockElapsedTime,
      blink::mojom::SpeculationEagerness::kEager)});
}

// Tests that the speculationrules trigger works in the presence of
// No-Vary-Search for same URL in the presence of redirection.
IN_PROC_BROWSER_TEST_F(NoVarySearchPrerenderBrowserTest,
                       ExactMatchWithUrlRedirection) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering a URL that causes same-origin redirection.
  const GURL kRedirectedUrl = GetUrl("/no_vary_search_a.html?prerender");
  const GURL kPrerenderingUrl =
      GetUrl("/server-redirect?" + kRedirectedUrl.spec());

  AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl), 1);

  // The prerender host should be registered for the initial request URL, not
  // the redirected URL.
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl));
  EXPECT_FALSE(HasHostForUrl(kRedirectedUrl));

  NavigationHandleObserver activation_observer(web_contents(),
                                               kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);
  // Ensure the state has been propagated to renderer processes.
  ASSERT_EQ(false, EvalJs(web_contents(), "document.prerendering"));

  // The prerender host should be consumed.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  // Activating the prerendered page should not issue a request.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl), 1);

  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kRedirectedUrl);

  ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
  ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder().BuildEntry(
      ukm_source_id, PreloadingType::kPrerender,
      PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
      PreloadingTriggeringOutcome::kSuccess,
      PreloadingFailureReason::kUnspecified,
      /*accurate=*/true,
      /*ready_time=*/kMockElapsedTime,
      blink::mojom::SpeculationEagerness::kEager)});
}

// Tests that the speculationrules trigger works in the presence of
// No-Vary-Search for inexact URL in the presence of redirection.
IN_PROC_BROWSER_TEST_F(NoVarySearchPrerenderBrowserTest,
                       InexactMatchWithUrlRedirection) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering a URL that causes same-origin redirection.
  const GURL kRedirectedUrl = GetUrl("/no_vary_search_a.html?prerender&a=2");
  const GURL kRedirectedUrlWithIgnoredQueryParam =
      GetUrl("/no_vary_search_a.html?prerender&a=3");
  const GURL kPrerenderingUrl =
      GetUrl("/server-redirect?" + kRedirectedUrl.spec());
  const GURL kNavigationUrl =
      GetUrl("/server-redirect?" + kRedirectedUrlWithIgnoredQueryParam.spec());

  AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl), 1);

  // The prerender host should be registered for the initial request URL, not
  // the redirected URL.
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl));
  EXPECT_FALSE(HasHostForUrl(kRedirectedUrl));

  NavigationHandleObserver activation_observer(web_contents(), kNavigationUrl);
  NavigatePrimaryPage(kNavigationUrl);
  // Ensure the state has been propagated to renderer processes.
  ASSERT_EQ(false, EvalJs(web_contents(), "document.prerendering"));

  // The prerender host should be consumed.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  // Activating the prerendered page should not issue a request.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl), 1);
  EXPECT_EQ(GetRequestCount(kNavigationUrl), 0);
  EXPECT_EQ(GetRequestCount(kRedirectedUrlWithIgnoredQueryParam), 0);

  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);
  // Make sure the omnibox URL hasn't been updated to
  // kRedirectedUrlWithIgnoredQueryParam because we've used at navigation
  // the already redirected prerender renderer.
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kRedirectedUrl);
  ASSERT_EQ(kRedirectedUrl, EvalJs(web_contents(), "window.location.href"));

  // URL match was inexact but should be recorded as accurate.
  ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
  ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder().BuildEntry(
      ukm_source_id, PreloadingType::kPrerender,
      PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
      PreloadingTriggeringOutcome::kSuccess,
      PreloadingFailureReason::kUnspecified,
      /*accurate=*/true,
      /*ready_time=*/kMockElapsedTime,
      blink::mojom::SpeculationEagerness::kEager)});
}

// Tests that the speculationrules No-Vary-Search hint is not populated for the
// PrerenderHost if kPrerender2NoVarySearch feature is not enabled.
IN_PROC_BROWSER_TEST_F(DisabledNoVarySearchPrerenderBrowserTest,
                       NoVarySearchHintIsNotPopulated) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/no_vary_search_a.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prerendering `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  FrameTreeNodeId host_id =
      AddPrerender(kPrerenderingUrl, R"(params=(\\\"a\\\"))");
  auto* host =
      web_contents_impl()->GetPrerenderHostRegistry()->FindNonReservedHostById(
          host_id);
  ASSERT_TRUE(host);
  ASSERT_FALSE(host->no_vary_search_expected().has_value());
}

// Tests that the speculationrules trigger works in the presence of
// No-Vary-Search for inexact URL in the presence of main frame navigation.
IN_PROC_BROWSER_TEST_F(NoVarySearchPrerenderBrowserTest,
                       InexactUrlMatchWithMainFrameNavigation) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/no_vary_search_a.html?prerender");
  const GURL kPrerenderingNextUrl = GetUrl("/empty.html?next");
  const GURL kNavigationUrl = GetUrl("/no_vary_search_a.html?prerender&a=3");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prerendering `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  ASSERT_TRUE(host_id);
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  // Navigate the prerendered page to `kPrerenderingNextUrl`.
  NavigatePrerenderedPage(host_id, kPrerenderingNextUrl);
  WaitForPrerenderLoadCompletion(host_id);

  NavigationHandleObserver activation_observer(web_contents(), kNavigationUrl);
  NavigatePrimaryPage(kNavigationUrl);
  // Ensure the state has been propagated to renderer processes.
  ASSERT_EQ(false, EvalJs(web_contents(), "document.prerendering"));

  // The prerender host should be consumed.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  EXPECT_FALSE(HasHostForUrl(kNavigationUrl));
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kPrerenderingNextUrl), 1);

  // Activating the prerendered page should not issue a request.
  EXPECT_EQ(GetRequestCount(kNavigationUrl), 0);
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingNextUrl);
  ASSERT_EQ(kPrerenderingNextUrl,
            EvalJs(web_contents(), "window.location.href"));

  // URL match was inexact but should be recorded as accurate.
  ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
  ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder().BuildEntry(
      ukm_source_id, PreloadingType::kPrerender,
      PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
      PreloadingTriggeringOutcome::kSuccess,
      PreloadingFailureReason::kUnspecified,
      /*accurate=*/true,
      /*ready_time=*/kMockElapsedTime,
      blink::mojom::SpeculationEagerness::kEager)});
}

// Test that activation is unsuccessful when navigating to an inexact URL
// before No-Vary-Search header is back from the server, even if the
// No-Vary-Search header is matching when it is received.
IN_PROC_BROWSER_TEST_F(DisabledNoVarySearchPrerenderBrowserTest,
                       NoVarySearchHintActivationUnsuccessful) {
  const std::string kTestingRelativeUrl =
      "/delayed_with_no_vary_search?prerender";
  const std::string kPrerenderingRelativeUrl = kTestingRelativeUrl + "&a=5";
  // Create a HTTP response to control prerendering main-frame navigation.
  net::test_server::ControllableHttpResponse main_prerender_response(
      embedded_test_server(), kPrerenderingRelativeUrl);

  const std::string kNavigationRelativeUrl = kTestingRelativeUrl + "&a=3";
  // Create a HTTP response to control main-frame navigation.
  net::test_server::ControllableHttpResponse main_navigation_response(
      embedded_test_server(), kNavigationRelativeUrl);

  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerenderingUrl =
      embedded_test_server()->GetURL(kPrerenderingRelativeUrl);
  const GURL kNavigationUrl =
      embedded_test_server()->GetURL(kNavigationRelativeUrl);

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prerendering `kPrerenderingUrl`.
  content::test::PrerenderHostCreationWaiter host_creation_waiter;
  AddPrerenderAsync(kPrerenderingUrl, R"(params=(\\\"a\\\"))");
  FrameTreeNodeId host_id = host_creation_waiter.Wait();
  auto* host =
      web_contents_impl()->GetPrerenderHostRegistry()->FindNonReservedHostById(
          host_id);
  ASSERT_TRUE(host);

  // Start to navigate to kNavigationUrl
  std::unique_ptr<content::TestNavigationObserver> nav_observer =
      test::PrerenderTestHelper::NavigatePrimaryPageAsync(*web_contents_impl(),
                                                          kNavigationUrl);
  auto* prerender_web_contents =
      content::WebContents::FromFrameTreeNodeId(host_id);
  content::test::PrerenderHostObserver host_observer(*prerender_web_contents,
                                                     host_id);

  NavigationRequest* nav_request =
      web_contents_impl()->GetPrimaryFrameTree().root()->navigation_request();
  ASSERT_FALSE(host->were_headers_received());
  // Make sure PrerenderHostRegistry does not select this prerender as a
  // potential prerender host to activate.
  ASSERT_NE(host_id, web_contents_impl()
                         ->GetPrerenderHostRegistry()
                         ->FindPotentialHostToActivate(*nav_request));
  // Make sure that the prerender host is not a match by IsUrlMatch.
  ASSERT_FALSE(host->IsUrlMatch(kNavigationUrl));
  main_prerender_response.WaitForRequest();
  main_prerender_response.Send(
      net::HTTP_OK, /*content_type=*/"text/html", /*content=*/"",
      /*cookies=*/{}, /*extra_headers=*/{"No-Vary-Search: params=(\"a\")"});
  host_observer.WaitForHeaders();
  ASSERT_TRUE(host->were_headers_received());
  // Make sure that, after receiving headers the prerender host is not a
  // match by IsUrlMatch.
  ASSERT_FALSE(host->IsUrlMatch(kNavigationUrl));
  ASSERT_NE(host_id, web_contents_impl()
                         ->GetPrerenderHostRegistry()
                         ->FindPotentialHostToActivate(*nav_request));
  // Advance the main navigation.
  main_navigation_response.WaitForRequest();
  main_navigation_response.Send(
      net::HTTP_OK, /*content_type=*/"text/html", /*content=*/"",
      /*cookies=*/{}, /*extra_headers=*/{"No-Vary-Search: params=(\"a\")"});
  main_navigation_response.Done();
  main_prerender_response.Done();

  // Wait for the navigation to finish.
  nav_observer->Wait();

  // Check that the prerender host was not activated.
  ASSERT_FALSE(host_observer.was_activated());

  // Navigation should issue a request.
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kNavigationUrl);
}

// Tests that the speculationrules trigger works.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, SpeculationRulesPrerender) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prerendering `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  ASSERT_TRUE(host_id);
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  NavigationHandleObserver activation_observer(web_contents(),
                                               kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);
  // Ensure the state has been propagated to renderer processes.
  ASSERT_EQ(false, EvalJs(web_contents(), "document.prerendering"));

  // The prerender host should be consumed.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  // Activating the prerendered page should not issue a request.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);

  {
    // Cross-check that both Preloading_Prediction and Preloading_Attempt UKMs
    // are logged on successful activation for speculation rules prerender.
    ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
    auto attempt_ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName, test::kPreloadingAttemptUkmMetrics);
    auto prediction_ukm_entries =
        test_ukm_recorder()->GetEntries(Preloading_Prediction::kEntryName,
                                        test::kPreloadingPredictionUkmMetrics);
    EXPECT_EQ(prediction_ukm_entries.size(), 1u);
    EXPECT_EQ(attempt_ukm_entries.size(), 1u);

    auto prerender_page_load_ukm_entries =
        test_ukm_recorder()->GetEntriesByName(
            ukm::builders::PrerenderPageLoad::kEntryName);

    // Check that Preloading_Attempt, Preloading_Prediction and
    // PrerenderPageLoad are all associated with the same SourceId.
    // There are three navigations
    // 1) Navigation to initial Url
    // 2) Navigation inside prerender frame tree
    // 3) Prerender activation navigation => navigation_ids_[2].
    // activation_id represents the SourceId for activation navigation. Check
    // that all the UKM events are logged for this SourceId.
    ukm::SourceId activation_id = ToSourceId(navigation_ids_[2]);
    EXPECT_EQ(activation_id, prerender_page_load_ukm_entries.back()->source_id);
    EXPECT_EQ(activation_id, prediction_ukm_entries.back().source_id);
    EXPECT_EQ(activation_id, attempt_ukm_entries.back().source_id);

    ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder().BuildEntry(
        activation_id, PreloadingType::kPrerender,
        PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
        PreloadingTriggeringOutcome::kSuccess,
        PreloadingFailureReason::kUnspecified,
        /*accurate=*/true,
        /*ready_time=*/kMockElapsedTime,
        blink::mojom::SpeculationEagerness::kEager)});

    ExpectPreloadingPredictionUkm({prediction_ukm_entry_builder().BuildEntry(
        ukm_source_id,
        /*confidence=*/100,
        /*accurate_prediction=*/true)});
  }

  // Collect metrics we recorded the renderer processes.
  FetchHistogramsFromChildProcesses();
  histogram_tester().ExpectTotalCount(
      "Prerender.Experimental.ActivationIPCDelay.SpeculationRule", 1u);
}

// Used for running tests that should commonly pass regardless of target hints.
class PrerenderTargetAgnosticBrowserTest
    : public PrerenderBrowserTest,
      public testing::WithParamInterface<std::string> {
 public:
  // Activates a prerendered page for `url` hosted on `prerender_web_contents`.
  void ActivatePrerenderedPage(WebContents& prerender_web_contents,
                               const GURL& url) {
    test::PrerenderHostObserver prerender_observer(prerender_web_contents, url);
    if (GetTargetHint() == "_blank") {
      TestNavigationObserver observer(&prerender_web_contents);
      test::PrerenderTestHelper::OpenNewWindowWithoutOpener(*web_contents(),
                                                            url);
      observer.WaitForNavigationFinished();
    } else {
      test::PrerenderTestHelper::NavigatePrimaryPage(*web_contents(), url);
    }
    ASSERT_TRUE(prerender_observer.was_activated());
  }

 protected:
  std::string GetTargetHint() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         PrerenderTargetAgnosticBrowserTest,
                         testing::Values("_self", "_blank"),
                         [](const testing::TestParamInfo<std::string>& info) {
                           return info.param;
                         });

class AutoSpeculationRulesPrerenderBrowserTest : public PrerenderBrowserTest {
 public:
  AutoSpeculationRulesPrerenderBrowserTest() {
    sub_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kAutoSpeculationRules, {{"config", GetConfig()}});
  }

 protected:
  void SetUp() override {
    ssl_server().RegisterRequestHandler(base::BindLambdaForTesting(
        [&](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url == kInitialUrlPath) {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content_type("text/html");

            // This will trigger the auto speculation rules configured in
            // SetUp().
            response->set_content(
                "<!DOCTYPE html><main data-reactroot></main>");
            return response;
          }
          return nullptr;
        }));

    PrerenderBrowserTest::SetUp();
  }

  PreloadingPredictor PredictorToExpectInUkm() override {
    return content_preloading_predictor::
        kSpeculationRulesFromAutoSpeculationRules;
  }

  GURL GetInitialUrl() const { return GetUrl(kInitialUrlPath); }

  GURL GetPrerenderedUrl() const { return GetUrl(kPrerenderedUrlPath); }

  std::string GetConfig() const {
    // JavaScriptFramework::kReact is 9, and it is detected by the presence of
    // data-reactroot attributes.
    return base::StringPrintf(R"(
    {
      "framework_to_speculation_rules": {
        "9": "{\"prerender\":[{\"source\":\"list\", \"urls\":[\"%s\"]}]}"
      }
    }
    )",
                              kPrerenderedUrlPath);
  }

 private:
  base::test::ScopedFeatureList sub_feature_list_;

  static constexpr char kInitialUrlPath[] = "/start.html";
  static constexpr char kPrerenderedUrlPath[] = "/empty.html?prerender";
};

class AutoSpeculationRulesPrerenderBrowserTestWithHoldback
    : public AutoSpeculationRulesPrerenderBrowserTest {
 public:
  AutoSpeculationRulesPrerenderBrowserTestWithHoldback() {
    sub_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kAutoSpeculationRules,
        {{"config", GetConfig()}, {"holdback", "true"}});
  }

 private:
  base::test::ScopedFeatureList sub_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AutoSpeculationRulesPrerenderBrowserTest, Metrics) {
  const GURL kInitialUrl = GetInitialUrl();
  const GURL kPrerenderingUrl = GetPrerenderedUrl();

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  WaitForPrerenderLoadCompletion(kPrerenderingUrl);
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  NavigationHandleObserver activation_observer(web_contents(),
                                               kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);
  // Ensure the state has been propagated to renderer processes.
  ASSERT_EQ(false, EvalJs(web_contents(), "document.prerendering"));

  // The prerender host should be consumed.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  // Activating the prerendered page should not issue a request.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  // Check UMA final status
  ExpectFinalStatusForSpeculationRuleFromAutoSpeculationRules(
      PrerenderFinalStatus::kActivated);

  // Check UKM metrics, the same as in the
  // PrerenderBrowserTest.SpeculationRulesPrerender test except the predictor is
  // overridden by the AutoSpeculationRulesPrerenderBrowserTest class.
  {
    // Cross-check that both Preloading_Prediction and Preloading_Attempt UKMs
    // are logged on successful activation for speculation rules prerender.
    ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
    auto attempt_ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName, test::kPreloadingAttemptUkmMetrics);
    auto prediction_ukm_entries =
        test_ukm_recorder()->GetEntries(Preloading_Prediction::kEntryName,
                                        test::kPreloadingPredictionUkmMetrics);
    EXPECT_EQ(prediction_ukm_entries.size(), 1u);
    EXPECT_EQ(attempt_ukm_entries.size(), 1u);

    auto prerender_page_load_ukm_entries =
        test_ukm_recorder()->GetEntriesByName(
            ukm::builders::PrerenderPageLoad::kEntryName);

    ukm::SourceId activation_id = ToSourceId(navigation_ids_[2]);
    EXPECT_EQ(activation_id, prerender_page_load_ukm_entries.back()->source_id);
    EXPECT_EQ(activation_id, prediction_ukm_entries.back().source_id);
    EXPECT_EQ(activation_id, attempt_ukm_entries.back().source_id);

    ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder().BuildEntry(
        activation_id, PreloadingType::kPrerender,
        PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
        PreloadingTriggeringOutcome::kSuccess,
        PreloadingFailureReason::kUnspecified,
        /*accurate=*/true,
        /*ready_time=*/kMockElapsedTime,
        blink::mojom::SpeculationEagerness::kEager)});

    ExpectPreloadingPredictionUkm({prediction_ukm_entry_builder().BuildEntry(
        ukm_source_id,
        /*confidence=*/100,
        /*accurate_prediction=*/true)});
  }
}

IN_PROC_BROWSER_TEST_F(AutoSpeculationRulesPrerenderBrowserTestWithHoldback,
                       Metrics) {
  const GURL kInitialUrl = GetInitialUrl();
  const GURL kPrerenderingUrl = GetPrerenderedUrl();

  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Wait for PrerenderHostRegistry to receive the holdback prerender
  // request, and it should be ignored.
  registry_observer.WaitForTrigger(kPrerenderingUrl);
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 0);

  NavigationHandleObserver next_page_observer(web_contents(), kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);

  // No final status for holdbacks.

  // Check UKM metrics: similar to the non-holdback case, except for the
  // holdback status and no prerender page load entries.
  {
    // Cross-check that both Preloading_Prediction and Preloading_Attempt UKMs
    // are logged on successful activation for speculation rules prerender.
    ukm::SourceId ukm_source_id = next_page_observer.next_page_ukm_source_id();
    auto attempt_ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName, test::kPreloadingAttemptUkmMetrics);
    auto prediction_ukm_entries =
        test_ukm_recorder()->GetEntries(Preloading_Prediction::kEntryName,
                                        test::kPreloadingPredictionUkmMetrics);
    EXPECT_EQ(prediction_ukm_entries.size(), 1u);
    EXPECT_EQ(attempt_ukm_entries.size(), 1u);

    auto prerender_page_load_ukm_entries =
        test_ukm_recorder()->GetEntriesByName(
            ukm::builders::PrerenderPageLoad::kEntryName);

    ukm::SourceId next_page_id = ToSourceId(navigation_ids_[1]);
    EXPECT_TRUE(prerender_page_load_ukm_entries.empty());
    EXPECT_EQ(prediction_ukm_entries.back().source_id, next_page_id);
    EXPECT_EQ(attempt_ukm_entries.back().source_id, next_page_id);

    ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder().BuildEntry(
        next_page_id, PreloadingType::kPrerender,
        PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kHoldback,
        PreloadingTriggeringOutcome::kUnspecified,
        PreloadingFailureReason::kUnspecified,
        /*accurate=*/true,
        /*ready_time=*/std::nullopt,
        blink::mojom::SpeculationEagerness::kEager)});

    ExpectPreloadingPredictionUkm({prediction_ukm_entry_builder().BuildEntry(
        ukm_source_id,
        /*confidence=*/100,
        /*accurate_prediction=*/true)});
  }
}

enum class PrerenderingResult { kSuccess, kFailed };
enum class BodySize { kSmall, kLarge };

class PrerenderAndPrefetchBrowserTest
    : public PrerenderBrowserTest,
      public testing::WithParamInterface<
          std::tuple<PrerenderingResult, BodySize, PrefetchReusableForTests>> {
 public:
  // Provides meaningful param names instead of /0, /1, ...
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    auto [prerendering_result, body_size, prefetch_reusable] = info.param;
    std::stringstream params_description;
    switch (prerendering_result) {
      case PrerenderingResult::kSuccess:
        params_description << "PrerenderSucceeded";
        break;
      case PrerenderingResult::kFailed:
        params_description << "PrerenderFailed";
        break;
    }
    switch (body_size) {
      case BodySize::kSmall:
        params_description << "_SmallBody";
        break;
      case BodySize::kLarge:
        params_description << "_LargeBody";
        break;
    }
    switch (prefetch_reusable) {
      case PrefetchReusableForTests::kEnabled:
        params_description << "_PrefetchReusableEnabled";
        break;
      case PrefetchReusableForTests::kDisabled:
        params_description << "_PrefetchReusableDisabled";
        break;
    }
    return params_description.str();
  }

 private:
  void SetUp() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    switch (std::get<PrefetchReusableForTests>(GetParam())) {
      case PrefetchReusableForTests::kDisabled:
        disabled_features.push_back(features::kPrefetchReusable);
        break;
      case PrefetchReusableForTests::kEnabled:
        // Set the limit to the size of `/find_in_long_page.html` - 1, to check
        // that exceeding the limit by 1 byte disallows reuse.
        enabled_features.push_back(
            {features::kPrefetchReusable,
             {{features::kPrefetchReusableBodySizeLimit.name, "112262"}}});
        break;
    }

    sub_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                    disabled_features);
    PrerenderBrowserTest::SetUp();
  }

  void TearDown() override {
    PrerenderBrowserTest::TearDown();
    sub_feature_list_.Reset();
  }

  base::test::ScopedFeatureList sub_feature_list_;
};

IN_PROC_BROWSER_TEST_P(PrerenderAndPrefetchBrowserTest,
                       SpeculationRulesPrefetchThenPrerender) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl =
      std::get<1>(GetParam()) == BodySize::kSmall
          ? GetUrl("/simple_page.html?prerender")
          : GetUrl("/find_in_long_page.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prefetching `kPrerenderingUrl` and wait for its completion.
  PrefetchService* prefetch_service = PrefetchService::GetFromFrameTreeNodeId(
      current_frame_host()->GetFrameTreeNodeId());
  ASSERT_TRUE(prefetch_service);
  base::RunLoop run_loop;
  PrefetchService::SetPrefetchResponseCompletedCallbackForTesting(
      base::BindRepeating(
          [](base::RunLoop* run_loop, const GURL& url,
             base::WeakPtr<PrefetchContainer> prefetch_container) {
            CHECK(prefetch_container);
            CHECK_EQ(prefetch_container->GetURL(), url);
            run_loop->Quit();
          },
          &run_loop, kPrerenderingUrl));
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  AddPrefetchAsync(kPrerenderingUrl);
  run_loop.Run();
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  // Start prerendering `kPrerenderingUrl`.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  ASSERT_TRUE(host_id);
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  switch (std::get<0>(GetParam())) {
    case PrerenderingResult::kSuccess:
      EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl));
      break;
    case PrerenderingResult::kFailed:
      // Cancel prerendered page.
      ASSERT_TRUE(web_contents_impl()->CancelPrerendering(
          FrameTreeNode::GloballyFindByID(host_id),
          PrerenderFinalStatus::kCancelAllHostsForTesting));
      EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
      break;
  }

  // Start main navigation.
  NavigationHandleObserver activation_observer(web_contents(),
                                               kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);
  auto delivery_type =
      EvalJs(web_contents()->GetPrimaryMainFrame(),
             "performance.getEntriesByType('navigation')[0].deliveryType");

  switch (std::get<0>(GetParam())) {
    case PrerenderingResult::kSuccess:
      // Main navigation activates the prerendered page even for the large page.
      ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);
      EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
      EXPECT_EQ(delivery_type, "navigational-prefetch");
      break;
    case PrerenderingResult::kFailed:
      // Main navigation shouldn't activate prerendered page (because it's
      // canceled).
      ExpectFinalStatusForSpeculationRule(
          PrerenderFinalStatus::kCancelAllHostsForTesting);

      if (std::get<1>(GetParam()) == BodySize::kSmall &&
          std::get<2>(GetParam()) == PrefetchReusableForTests::kEnabled) {
        // The prefetched result should be still used for navigation for small
        // body, because it fits within PrefetchDataPipeTee buffer limit.
        EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
        EXPECT_EQ(delivery_type, "navigational-prefetch");
      } else {
        // The prefetched result can't be used for navigation for large body
        // due to PrefetchDataPipeTee buffer limit. A cached response from the
        // HTTP cache is used instead, we still should not see another request.
        EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
        EXPECT_EQ(delivery_type, "cache");
      }
      break;
  }

  switch (std::get<1>(GetParam())) {
    case BodySize::kSmall:
      EXPECT_EQ(GetBodyTextContent(), "Basic html test.");
      break;

    case BodySize::kLarge:
      //  `document.body.textContent.trim().length` for
      //  `/find_in_long_page.html`
      EXPECT_EQ(GetBodyTextContent().size(), 102076u);
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    PrerenderAndPrefetchBrowserTest,
    testing::Combine(testing::Values(PrerenderingResult::kSuccess,
                                     PrerenderingResult::kFailed),
                     testing::Values(BodySize::kSmall, BodySize::kLarge),
                     testing::ValuesIn(PrefetchReusableValuesForTests())),
    PrerenderAndPrefetchBrowserTest::DescribeParams);

// Tests that the speculationrules-triggered prerender would be destroyed after
// its initiator navigates away.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, SpeculationInitiatorNavigateAway) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);

  // Navigate the initiator page to a non-prerendered page. This destroys the
  // prerendered page.
  test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);
  NavigatePrimaryPage(GetUrl("/empty.html?elsewhere"));
  host_observer.WaitForDestroyed();

  // The prerender host should be destroyed.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  // Cross-check that in case where the navigation happens to a different page,
  // we log the correct metrics.
  ukm::SourceId ukm_source_id = PrimaryPageSourceId();
  ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder().BuildEntry(
      ukm_source_id, PreloadingType::kPrerender,
      PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
      PreloadingTriggeringOutcome::kReady,
      PreloadingFailureReason::kUnspecified,
      /*accurate=*/false,
      /*ready_time=*/kMockElapsedTime,
      blink::mojom::SpeculationEagerness::kEager)});
  ExpectPreloadingPredictionUkm({prediction_ukm_entry_builder().BuildEntry(
      ukm_source_id, /*confidence=*/100,
      /*accurate_prediction=*/false)});
}

// Tests that clicking a link can activate a prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, ActivateOnLinkClick) {
  const GURL kInitialUrl = GetUrl("/simple_links.html");
  const GURL kPrerenderingUrl = GetUrl("/title2.html");

  // Navigate to an initial page which has a link to `kPrerenderingUrl`.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl`.
  FrameTreeNodeId prerender_host_id = AddPrerender(kPrerenderingUrl);
  test::PrerenderHostObserver prerender_observer(*web_contents(),
                                                 prerender_host_id);

  // Click the link. It should activate the prerendered page.
  TestNavigationObserver nav_observer(web_contents());
  const std::string kLinkClickScript = R"(
      const link = document.querySelector('#same_site_link');
      link.click();
  )";
  EXPECT_TRUE(ExecJs(web_contents(), kLinkClickScript));
  nav_observer.WaitForNavigationFinished();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
  EXPECT_TRUE(prerender_observer.was_activated());
}

// Tests that clicking a link annotated with "target=_blank" cannot activate a
// prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, ActivateOnLinkClick_TargetBlank) {
  const GURL kInitialUrl = GetUrl("/simple_links.html");
  const GURL kPrerenderingUrl = GetUrl("/title2.html");

  // Navigate to an initial page which has a link to `kPrerenderingUrl`.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl`.
  FrameTreeNodeId prerender_host_id = AddPrerender(kPrerenderingUrl);
  test::PrerenderHostObserver prerender_observer(*web_contents(),
                                                 prerender_host_id);

  // Click the link annotated with "target=_blank". This should not activate the
  // prerendered page.
  TestNavigationObserver nav_observer(kPrerenderingUrl);
  nav_observer.StartWatchingNewWebContents();
  const std::string kLinkClickScript = R"(
      clickSameSiteNewWindowLink();
  )";
  EXPECT_TRUE(ExecJs(web_contents(), kLinkClickScript));
  nav_observer.WaitForNavigationFinished();
  EXPECT_EQ(nav_observer.last_navigation_url(), kPrerenderingUrl);
  EXPECT_FALSE(prerender_observer.was_activated());

  // The navigation occurred in a new WebContents, so the original WebContents
  // should still be showing the initial trigger page.
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);
  // Also, the prerendered page should still be alive.
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl));

  // Navigate to `kPrerenderingUrl` on the original WebContents. This should
  // activate the prerendered page.
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
  EXPECT_TRUE(prerender_observer.was_activated());
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);
}

// Tests that clicking a link annotated with "target=_blank" can activate a
// prerender whose target_hint is "_blank".
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       ActivateOnLinkClick_TargetBlank_WithTargetHintBlank) {
  const GURL kInitialUrl = GetUrl("/simple_links.html");
  const GURL kPrerenderingUrl = GetUrl("/title2.html");

  // Navigate to an initial page which has a link to `kPrerenderingUrl`.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl`.
  FrameTreeNodeId host_id = prerender_helper()->AddPrerender(
      kPrerenderingUrl, /*eagerness=*/std::nullopt, "_blank");
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
  ASSERT_NE(prerender_web_contents, web_contents_impl());
  ExpectWebContentsIsForNewTabPrerendering(*prerender_web_contents);

  // Click the link annotated with "target=_blank". This should activate the
  // prerendered page.
  TestNavigationObserver activation_observer(kPrerenderingUrl);
  activation_observer.WatchExistingWebContents();
  test::PrerenderHostObserver prerender_observer(*prerender_web_contents,
                                                 host_id);
  const std::string kLinkClickScript = R"(
      clickSameSiteNewWindowLink();
  )";
  EXPECT_TRUE(ExecJs(web_contents(), kLinkClickScript));
  activation_observer.WaitForNavigationFinished();
  EXPECT_EQ(prerender_web_contents->GetLastCommittedURL(), kPrerenderingUrl);
  EXPECT_EQ(activation_observer.last_navigation_url(), kPrerenderingUrl);
  EXPECT_TRUE(prerender_observer.was_activated());
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);

  ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
  ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder().BuildEntry(
      ukm_source_id, PreloadingType::kPrerender,
      PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
      PreloadingTriggeringOutcome::kSuccess,
      PreloadingFailureReason::kUnspecified,
      /*accurate=*/true,
      /*ready_time=*/kMockElapsedTime,
      blink::mojom::SpeculationEagerness::kEager)});

  ExpectPreloadingPredictionUkm({prediction_ukm_entry_builder().BuildEntry(
      ukm_source_id,
      /*confidence=*/100,
      /*accurate_prediction=*/true)});

  // The navigation occurred in a new WebContents, so the original WebContents
  // should still be showing the initial trigger page.
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);
}

// Tests that clicking a link annotated with "target=_blank" can activate a
// prerender whose target_hint is "_blank" where the initiator page is in the
// background when the speculation rules were added.
IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    BackgroundedPage_ActivateOnLinkClick_TargetBlank_WithTargetHintBlank) {
  const GURL initial_url = GetUrl("/simple_links.html");
  const GURL prerender_url = GetUrl("/title2.html");
  // Inject mock time task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  PrerenderHostRegistry* registry =
      web_contents_impl()->GetPrerenderHostRegistry();
  registry->SetTaskRunnerForTesting(task_runner);

  // Navigate to an initial page which has a link to `prerender_url`.
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  web_contents()->WasHidden();

  // The timers should be still running.
  ASSERT_TRUE(registry->GetEmbedderTimerForTesting()->IsRunning());
  ASSERT_TRUE(registry->GetSpeculationRulesTimerForTesting()->IsRunning());

  // Start prerendering `prerender_url`.
  FrameTreeNodeId host_id = prerender_helper()->AddPrerender(
      prerender_url, /*eagerness=*/std::nullopt, "_blank");
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
  ASSERT_NE(prerender_web_contents, web_contents_impl());
  ExpectWebContentsIsForNewTabPrerendering(*prerender_web_contents);
  web_contents()->WasShown();

  // The timers should be stopped after WasShown().
  ASSERT_FALSE(registry->GetEmbedderTimerForTesting()->IsRunning());
  ASSERT_FALSE(registry->GetSpeculationRulesTimerForTesting()->IsRunning());
  // Forward the time by
  // PrerenderHostRegistry::kTimeToLiveInBackgroundForSpeculationRules shouldn't
  // affect the prerender.
  task_runner->FastForwardBy(
      PrerenderHostRegistry::kTimeToLiveInBackgroundForSpeculationRules);

  // Click the link annotated with "target=_blank". This should activate the
  // prerendered page.
  TestNavigationObserver activation_observer(prerender_url);
  activation_observer.WatchExistingWebContents();
  test::PrerenderHostObserver prerender_observer(*prerender_web_contents,
                                                 host_id);
  const std::string kLinkClickScript = R"(
      clickSameSiteNewWindowLink();
  )";
  EXPECT_TRUE(ExecJs(web_contents(), kLinkClickScript));
  activation_observer.WaitForNavigationFinished();
  EXPECT_EQ(prerender_web_contents->GetLastCommittedURL(), prerender_url);
  EXPECT_EQ(activation_observer.last_navigation_url(), prerender_url);
  EXPECT_TRUE(prerender_observer.was_activated());
  EXPECT_FALSE(HasHostForUrl(prerender_url));

  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);

  ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
  ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder().BuildEntry(
      ukm_source_id, PreloadingType::kPrerender,
      PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
      PreloadingTriggeringOutcome::kSuccess,
      PreloadingFailureReason::kUnspecified,
      /*accurate=*/true,
      /*ready_time=*/kMockElapsedTime,
      blink::mojom::SpeculationEagerness::kEager)});

  ExpectPreloadingPredictionUkm({prediction_ukm_entry_builder().BuildEntry(
      ukm_source_id,
      /*confidence=*/100,
      /*accurate_prediction=*/true)});

  // The navigation occurred in a new WebContents, so the original WebContents
  // should still be showing the initial trigger page.
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), initial_url);
}

// Tests that the prerendering started by a hidden initiator page will be
// canceled after timeout.
IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    BackgroundedPageTimeout_TargetBlank_WithTargetHintBlank) {
  const GURL initial_url = GetUrl("/simple_links.html");
  const GURL prerender_url = GetUrl("/title2.html");
  // Inject mock time task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  PrerenderHostRegistry* registry =
      web_contents_impl()->GetPrerenderHostRegistry();
  registry->SetTaskRunnerForTesting(task_runner);

  // Navigate to an initial page which has a link to `prerender_url`.
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  web_contents()->WasHidden();

  // The timers should be still running.
  ASSERT_TRUE(registry->GetSpeculationRulesTimerForTesting()->IsRunning());

  // Start prerendering `prerender_url`.
  FrameTreeNodeId host_id = prerender_helper()->AddPrerender(
      prerender_url, /*eagerness=*/std::nullopt, "_blank");
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
  ASSERT_NE(prerender_web_contents, web_contents_impl());
  ExpectWebContentsIsForNewTabPrerendering(*prerender_web_contents);
  test::PrerenderHostObserver prerender_observer(*prerender_web_contents,
                                                 host_id);

  // Expire the timers.
  task_runner->FastForwardBy(
      PrerenderHostRegistry::kTimeToLiveInBackgroundForSpeculationRules);

  // The timers should cancel prerendering.
  prerender_observer.WaitForDestroyed();
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kTimeoutBackgrounded, 1);
}

IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    PrerenderWhenInitiatorInBackground_Queue_Processing_WithTargetHint) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  const GURL prerender_url1 =
      embedded_test_server()->GetURL("/empty.html?prerender1");
  const GURL prerender_url2 =
      embedded_test_server()->GetURL("/empty.html?prerender2");

  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  web_contents()->WasHidden();

  // Insert 2 URLs into the speculation rules at the same time.
  WebContents* prerender_web_contents = nullptr;
  WebContents* prerender2_web_contents = nullptr;

  base::RunLoop run_loop;
  auto creation_subscription = RegisterWebContentsCreationCallback(
      base::BindLambdaForTesting([&](WebContents* web_contents) {
        if (prerender_web_contents == nullptr) {
          prerender_web_contents = web_contents;
        } else {
          prerender2_web_contents = web_contents;
        }
        run_loop.QuitClosure().Run();
      }));
  AddPrerendersAsync({prerender_url1, prerender_url2},
                     /*eagerness=*/std::nullopt,
                     /*target_hint=*/"_blank");
  run_loop.Run();
  ExpectWebContentsIsForNewTabPrerendering(*prerender_web_contents);

  // Check the prerender host is already ready.
  prerender_helper()->WaitForPrerenderLoadCompletion(*prerender_web_contents,
                                                     prerender_url1);
  PrerenderHost* prerender_host =
      static_cast<WebContentsImpl*>(prerender_web_contents)
          ->GetPrerenderHostRegistry()
          ->FindHostByUrlForTesting(prerender_url1);
  auto* preloading_attempt_impl = static_cast<PreloadingAttemptImpl*>(
      prerender_host->preloading_attempt().get());
  EXPECT_EQ(test::PreloadingAttemptAccessor(preloading_attempt_impl)
                .GetTriggeringOutcome(),
            PreloadingTriggeringOutcome::kReady);

  // Currently, prerender_url2 will be cancelled since there is no queue
  // mechanism for prerender-into-new-tab yet.
  // TODO(crbug.com/350785853): Add queue mechanism and
  // update test expectation.
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kTabClosedWithoutUserGesture);
}

// Tests that clicking a link annotated with "target=_blank rel=noopener" cannot
// activate a prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       ActivateOnLinkClick_TargetBlankWithNoopener) {
  const GURL kInitialUrl = GetUrl("/simple_links.html");
  const GURL kPrerenderingUrl = GetUrl("/title2.html");

  // Navigate to an initial page which has a link to `kPrerenderingUrl`.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl`.
  FrameTreeNodeId prerender_host_id = AddPrerender(kPrerenderingUrl);
  test::PrerenderHostObserver prerender_observer(*web_contents(),
                                                 prerender_host_id);

  // Click the link annotated with "target=_blank rel=noopener". This should not
  // activate the prerendered page.
  TestNavigationObserver nav_observer(kPrerenderingUrl);
  nav_observer.StartWatchingNewWebContents();
  const std::string kLinkClickScript = R"(
      clickSameSiteNewWindowWithNoopenerLink();
  )";
  EXPECT_TRUE(ExecJs(web_contents(), kLinkClickScript));
  nav_observer.WaitForNavigationFinished();
  EXPECT_EQ(nav_observer.last_navigation_url(), kPrerenderingUrl);
  EXPECT_FALSE(prerender_observer.was_activated());

  // The navigation occurred in a new WebContents, so the original WebContents
  // should still be showing the initial trigger page.
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);
  // Also, the prerendered page should still be alive.
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl));

  // Navigate to `kPrerenderingUrl` on the original WebContents. This should
  // activate the prerendered page.
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
  EXPECT_TRUE(prerender_observer.was_activated());
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);
}

// Tests that clicking a link annotated with "target=_blank rel=noopener" can
// activate a prerender whose target_hint is "_blank".
IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    ActivateOnLinkClick_TargetBlankWithNoopener_WithTargetHintBlank) {
  const GURL kInitialUrl = GetUrl("/simple_links.html");
  const GURL kPrerenderingUrl = GetUrl("/title2.html");

  // Navigate to an initial page which has a link to `kPrerenderingUrl`.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl`.
  FrameTreeNodeId host_id = prerender_helper()->AddPrerender(
      kPrerenderingUrl, /*eagerness=*/std::nullopt, "_blank");
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
  ASSERT_NE(prerender_web_contents, web_contents_impl());
  ExpectWebContentsIsForNewTabPrerendering(*prerender_web_contents);

  // Click the link annotated with "target=_blank rel=noopener". This should
  // activate the prerendered page.
  TestNavigationObserver activation_observer(kPrerenderingUrl);
  activation_observer.WatchExistingWebContents();
  test::PrerenderHostObserver prerender_observer(*prerender_web_contents,
                                                 host_id);
  const std::string kLinkClickScript = R"(
      clickSameSiteNewWindowWithNoopenerLink();
  )";
  EXPECT_TRUE(ExecJs(web_contents(), kLinkClickScript));
  activation_observer.WaitForNavigationFinished();
  EXPECT_EQ(prerender_web_contents->GetLastCommittedURL(), kPrerenderingUrl);
  EXPECT_EQ(activation_observer.last_navigation_url(), kPrerenderingUrl);
  EXPECT_EQ(prerender_web_contents->GetLastCommittedURL(), kPrerenderingUrl);
  EXPECT_TRUE(prerender_observer.was_activated());
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);

  ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
  ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder().BuildEntry(
      ukm_source_id, PreloadingType::kPrerender,
      PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
      PreloadingTriggeringOutcome::kSuccess,
      PreloadingFailureReason::kUnspecified,
      /*accurate=*/true,
      /*ready_time=*/kMockElapsedTime,
      blink::mojom::SpeculationEagerness::kEager)});

  ExpectPreloadingPredictionUkm({prediction_ukm_entry_builder().BuildEntry(
      ukm_source_id,
      /*confidence=*/100,
      /*accurate_prediction=*/true)});

  // The navigation occurred in a new WebContents, so the original WebContents
  // should still be showing the initial trigger page.
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);
}

// Tests that clicking a link annotated with "target=_blank rel=opener" cannot
// activate a prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       ActivateOnLinkClick_TargetBlankWithOpener) {
  const GURL kInitialUrl = GetUrl("/simple_links.html");
  const GURL kPrerenderingUrl = GetUrl("/title2.html");

  // Navigate to an initial page which has a link to `kPrerenderingUrl`.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl`.
  FrameTreeNodeId prerender_host_id = AddPrerender(kPrerenderingUrl);
  test::PrerenderHostObserver prerender_observer(*web_contents(),
                                                 prerender_host_id);

  // Click the link annotated with "target=_blank rel=opener". This should not
  // activate the prerendered page.
  TestNavigationObserver nav_observer(kPrerenderingUrl);
  nav_observer.StartWatchingNewWebContents();
  const std::string kLinkClickScript = R"(
      clickSameSiteNewWindowWithOpenerLink();
  )";
  EXPECT_TRUE(ExecJs(web_contents(), kLinkClickScript));
  nav_observer.WaitForNavigationFinished();
  EXPECT_EQ(nav_observer.last_navigation_url(), kPrerenderingUrl);
  EXPECT_FALSE(prerender_observer.was_activated());

  // The navigation occurred in a new WebContents, so the original WebContents
  // should still be showing the initial trigger page.
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);
  // Also, the prerendered page should still be alive.
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl));

  // Navigate to `kPrerenderingUrl` on the original WebContents. The page opened
  // with "rel=opener" should prevent it from activating the prerendered page.
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
  EXPECT_FALSE(prerender_observer.was_activated());

  // The prerendered page should be destroyed on activation attempt.
  prerender_observer.WaitForDestroyed();
  EXPECT_TRUE(GetHostForUrl(kPrerenderingUrl).is_null());
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kActivatedWithAuxiliaryBrowsingContexts);
}

// Tests that clicking a link annotated with "target=_blank rel=opener" cannot
// activate a prerender whose target_hint is "_blank".
IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    ActivateOnLinkClick_TargetBlankWithOpener_WithTargetHintBlank) {
  const GURL kInitialUrl = GetUrl("/simple_links.html");
  const GURL kPrerenderingUrl = GetUrl("/title2.html");

  // Navigate to an initial page which has a link to `kPrerenderingUrl`.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl`.
  FrameTreeNodeId host_id = prerender_helper()->AddPrerender(
      kPrerenderingUrl, /*eagerness=*/std::nullopt, "_blank");
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
  ASSERT_NE(prerender_web_contents, web_contents_impl());
  ExpectWebContentsIsForNewTabPrerendering(*prerender_web_contents);

  ukm::SourceId triggering_primary_page_source_id =
      web_contents_impl()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  // Click the link annotated with "target=_blank rel=opener". This should not
  // activate the prerendered page.
  test::PrerenderHostObserver prerender_observer(*prerender_web_contents,
                                                 host_id);
  const std::string kLinkClickScript = R"(
      clickSameSiteNewWindowWithOpenerLink();
  )";
  EXPECT_TRUE(ExecJs(web_contents(), kLinkClickScript));
  // The WebContents pre-created for prerendering should not be used.
  EXPECT_NE(prerender_web_contents->GetLastCommittedURL(), kPrerenderingUrl);
  EXPECT_FALSE(prerender_observer.was_activated());
  // The host should still be available.
  EXPECT_TRUE(HasHostForUrl(*prerender_web_contents, kPrerenderingUrl));

  // The navigation occurred in a new WebContents, so the original WebContents
  // should still be showing the initial trigger page.
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Navigate to `kPrerenderingUrl` on the original WebContents. This should
  // destroy the prerendered page and its WebContents.
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
  EXPECT_FALSE(prerender_observer.was_activated());
  prerender_observer.WaitForDestroyed();
  EXPECT_TRUE(GetHostForUrl(kPrerenderingUrl).is_null());
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kTriggerDestroyed);

  // Wait for UKM recording in PreloadingDataImpl::WebContentsDestroyed() on
  // the destruction of the prerender WebContents.
  // TODO(nhiroki): Wait for that in a more deterministic way instead of
  // RunUntilIdle().
  base::RunLoop().RunUntilIdle();

  // The prerender WebContents doesn't have the primary page that can record UKM
  // on destruction. Instead, it asks the primary page hosted on the primary
  // WebContents to record UKM.
  ExpectPreloadingAttemptPreviousPrimaryPageUkm(
      attempt_previous_ukm_entry_builder().BuildEntry(
          triggering_primary_page_source_id, PreloadingType::kPrerender,
          PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
          PreloadingTriggeringOutcome::kReady,
          PreloadingFailureReason::kUnspecified,
          /*accurate=*/false,
          /*ready_time=*/kMockElapsedTime,
          blink::mojom::SpeculationEagerness::kEager));
  ExpectPreloadingPredictioPreviousPrimaryPageUkm(
      {prediction_previous_ukm_entry_builder().BuildEntry(
          triggering_primary_page_source_id,
          /*confidence=*/100,
          /*accurate=*/false)});
}

// Tests that window.open() annotated with "_blank" and "noopener" can activate
// a prerender whose target_hint is "_blank".
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, ActivateOnWindowOpen) {
  const GURL kInitialUrl = GetUrl("/simple_links.html");
  const GURL kPrerenderingUrl = GetUrl("/title2.html");

  // Navigate to an initial page which has a link to `kPrerenderingUrl`.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl`.
  FrameTreeNodeId host_id = prerender_helper()->AddPrerender(
      kPrerenderingUrl, /*eagerness=*/std::nullopt, "_blank");
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
  ASSERT_NE(prerender_web_contents, web_contents_impl());
  ExpectWebContentsIsForNewTabPrerendering(*prerender_web_contents);

  // Open a new window with "_blank" and "noopener". This should activate the
  // prerendered page.
  TestNavigationObserver activation_observer(kPrerenderingUrl);
  activation_observer.WatchExistingWebContents();
  test::PrerenderHostObserver prerender_observer(*prerender_web_contents,
                                                 host_id);
  const std::string kWindowOpenScript = R"(
      window.open("title2.html", "_blank", "noopener");
  )";
  EXPECT_TRUE(ExecJs(web_contents(), kWindowOpenScript));
  activation_observer.WaitForNavigationFinished();
  EXPECT_EQ(prerender_web_contents->GetLastCommittedURL(), kPrerenderingUrl);
  EXPECT_EQ(activation_observer.last_navigation_url(), kPrerenderingUrl);
  EXPECT_TRUE(prerender_observer.was_activated());
  EXPECT_FALSE(HasHostForUrl(*prerender_web_contents, kPrerenderingUrl));

  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);

  ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
  ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder().BuildEntry(
      ukm_source_id, PreloadingType::kPrerender,
      PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
      PreloadingTriggeringOutcome::kSuccess,
      PreloadingFailureReason::kUnspecified,
      /*accurate=*/true,
      /*ready_time=*/kMockElapsedTime,
      blink::mojom::SpeculationEagerness::kEager)});

  // The navigation occurred in a new WebContents, so the original WebContents
  // should still be showing the initial trigger page.
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);
}

// TODO(crbug.com/40234240): Add more test cases for prerender-in-new-tab:
// - Multiple prerendering requests with the same URL but different target hint.
// - Navigation in a new tab to the prerendering URL multiple times. Only the
//   first navigation should activate the prerendered page.

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, ResponseHeaders) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/set-header?X-Foo: bar");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prerendering `kPrerenderingUrl` and check if `X-Foo` header is
  // observed.
  NavigationHandleObserver observer1(web_contents(), kPrerenderingUrl);
  AddPrerender(kPrerenderingUrl);
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_TRUE(observer1.has_committed());
  EXPECT_EQ("bar", observer1.GetNormalizedResponseHeader("x-foo"));

  // Activate the page and check if `X-Foo` header is observed again.
  NavigationHandleObserver observer2(web_contents(), kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_TRUE(observer2.has_committed());
  EXPECT_EQ("bar", observer2.GetNormalizedResponseHeader("x-foo"));
}

// Tests that cancelling a prerender-into-new-tab trigger by invoking
// CancelHosts on initiator WebContents's PrerenderHostRegistry will
// eventually destruct corresponding PrerenderNewTabHandle and its WebContents
// created for the new-tab trigger.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       NewTabPrerenderCancellationOnInitiatorPHR) {
  const GURL initial_url = GetUrl("/empty.html");
  const GURL prerendering_url = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Start prerendering.
  FrameTreeNodeId host_id = prerender_helper()->AddPrerender(
      prerendering_url, /*eagerness=*/std::nullopt, "_blank");
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
  WebContentsDestroyedWatcher wc_destroyed_watcher(prerender_web_contents);
  test::PrerenderHostObserver host_observer(*prerender_web_contents, host_id);

  // Call CancelHost on initiator WebContents's PrerenderHostRegistry.
  web_contents_impl()->GetPrerenderHostRegistry()->CancelHost(
      host_id, PrerenderFinalStatus::kDestroyed);

  host_observer.WaitForDestroyed();
  // WebContents created for the new-tab trigger will be destroyed.
  wc_destroyed_watcher.Wait();
  EXPECT_FALSE(prerender_helper()->HasNewTabHandle(host_id));
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kDestroyed);
}

// Tests that cancelling a prerender-into-new-tab trigger by invoking
// CancelHosts on PrerenderHostRegistry of WebContents created by new-tab
// triggers will eventually destruct corresponding PrerenderNewTabHandle on
// initiator's PHR and that WebContents created for the new-tab trigger.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       NewTabPrerenderCancellationOnNewTabPHR) {
  const GURL initial_url = GetUrl("/empty.html");
  const GURL prerendering_url = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Start prerendering.
  FrameTreeNodeId host_id = prerender_helper()->AddPrerender(
      prerendering_url, /*eagerness=*/std::nullopt, "_blank");
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
  WebContentsDestroyedWatcher wc_destroyed_watcher(prerender_web_contents);
  test::PrerenderHostObserver host_observer(*prerender_web_contents, host_id);

  // Call CancelHost on WebContents's PrerenderHostRegistry that was created for
  // the new-tab trigger.
  static_cast<WebContentsImpl*>(prerender_web_contents)
      ->GetPrerenderHostRegistry()
      ->CancelHost(host_id, PrerenderFinalStatus::kDestroyed);

  host_observer.WaitForDestroyed();
  // WebContents created for the new-tab trigger will be destroyed.
  wc_destroyed_watcher.Wait();
  EXPECT_FALSE(prerender_helper()->HasNewTabHandle(host_id));
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kDestroyed);
}

// Tests that closing initiator's WebContents will eventually destruct
// corresponding PrerenderNewTabHandle WebContents created for the new-tab
// trigger without crashing.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       NewTabPrerenderCancellationByInitiatorWCClosure) {
  const GURL initial_url = GetUrl("/empty.html");
  const GURL prerendering_url = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Start prerendering.
  FrameTreeNodeId host_id = prerender_helper()->AddPrerender(
      prerendering_url, /*eagerness=*/std::nullopt, "_blank");
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
  WebContentsDestroyedWatcher wc_destroyed_watcher(prerender_web_contents);
  test::PrerenderHostObserver host_observer(*prerender_web_contents, host_id);

  shell()->Close();

  host_observer.WaitForDestroyed();
  // WebContents created for the new-tab trigger will be destroyed.
  wc_destroyed_watcher.Wait();
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kTabClosedWithoutUserGesture);
}

// Tests that prerendering is cancelled if a network request for the
// navigation results in an empty response with 404 status.
IN_PROC_BROWSER_TEST_P(PrerenderTargetAgnosticBrowserTest,
                       PrerenderCancelledOnEmptyBody404) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  // Specify a URL for which we don't have a corresponding file in the data dir.
  const GURL kPrerenderingUrl = GetUrl("/404");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl`.
  test::PrerenderHostCreationWaiter host_creation_waiter;
  prerender_helper()->AddPrerendersAsync(
      {kPrerenderingUrl}, /*eagerness=*/std::nullopt, GetTargetHint());
  FrameTreeNodeId host_id = host_creation_waiter.Wait();
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
  test::PrerenderHostObserver host_observer(*prerender_web_contents, host_id);
  host_observer.WaitForDestroyed();

  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kNavigationBadHttpStatus);
}

// Tests that prerendering is cancelled if a network request for the
// navigation results in an non-empty response with 404 status.
IN_PROC_BROWSER_TEST_P(PrerenderTargetAgnosticBrowserTest,
                       PrerenderCancelledOnNonEmptyBody404) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/page404.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Add prerendering to the 404 error page, then check that it got cancelled.
  test::PrerenderHostCreationWaiter host_creation_waiter;
  prerender_helper()->AddPrerendersAsync(
      {kPrerenderingUrl}, /*eagerness=*/std::nullopt, GetTargetHint());
  FrameTreeNodeId host_id = host_creation_waiter.Wait();
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
  test::PrerenderHostObserver host_observer(*prerender_web_contents, host_id);
  host_observer.WaitForDestroyed();

  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kNavigationBadHttpStatus);
}

// Tests that prerendering is cancelled if a network request for the
// navigation results in an non-empty response with 500 status.
IN_PROC_BROWSER_TEST_P(PrerenderTargetAgnosticBrowserTest,
                       PrerenderCancelledOn500Page) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/page500.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Add prerendering to the 500 error page, then check that it got cancelled.
  test::PrerenderHostCreationWaiter host_creation_waiter;
  prerender_helper()->AddPrerendersAsync(
      {kPrerenderingUrl}, /*eagerness=*/std::nullopt, GetTargetHint());
  FrameTreeNodeId host_id = host_creation_waiter.Wait();
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
  test::PrerenderHostObserver host_observer(*prerender_web_contents, host_id);
  host_observer.WaitForDestroyed();

  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kNavigationBadHttpStatus);
}

IN_PROC_BROWSER_TEST_P(PrerenderTargetAgnosticBrowserTest,
                       PrerenderCancelledOn204Page) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl` that returns 204 response code.
  const GURL kPrerenderingUrl = GetUrl("/echo?status=204");
  test::PrerenderHostCreationWaiter host_creation_waiter;
  prerender_helper()->AddPrerendersAsync(
      {kPrerenderingUrl}, /*eagerness=*/std::nullopt, GetTargetHint());
  FrameTreeNodeId host_id = host_creation_waiter.Wait();
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
  test::PrerenderHostObserver host_observer(*prerender_web_contents, host_id);

  // The prerender should be destroyed.
  host_observer.WaitForDestroyed();

  // Cancellation must have occurred due to bad http status code.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kNavigationBadHttpStatus);
}

IN_PROC_BROWSER_TEST_P(PrerenderTargetAgnosticBrowserTest,
                       PrerenderCancelledOn205Page) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl` that returns 205 response code.
  const GURL kPrerenderingUrl = GetUrl("/echo?status=205");
  test::PrerenderHostCreationWaiter host_creation_waiter;
  prerender_helper()->AddPrerendersAsync(
      {kPrerenderingUrl}, /*eagerness=*/std::nullopt, GetTargetHint());
  FrameTreeNodeId host_id = host_creation_waiter.Wait();
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
  test::PrerenderHostObserver host_observer(*prerender_web_contents, host_id);

  // The prerender should be destroyed.
  host_observer.WaitForDestroyed();

  // Cancellation must have occurred due to bad http status code.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kNavigationBadHttpStatus);
}

namespace {

// Tests that an iframe navigation whose response has either 204 or 205 doesn't
// cancel prerendering.
// This is also a regression test for https://crbug.com/1362818.
void PrerenderBrowserTest::TestPrerenderAllowedOnIframeWithStatusCode(
    OriginType origin_type,
    std::string status_code) {
  // This test is designed for 204 and 205 status codes.
  ASSERT_TRUE(status_code == "204" || status_code == "205");

  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl`.
  const GURL kPrerenderingUrl = GetUrl("/title1.html");
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);

  // Construct an iframe URL whose response has 204/205.
  GURL iframe_url;
  std::string file_path = "/echo?status=" + status_code;
  switch (origin_type) {
    case OriginType::kSameOrigin:
      iframe_url = GetUrl(file_path);
      break;
    case OriginType::kSameSiteCrossOrigin:
      iframe_url = GetSameSiteCrossOriginUrl(file_path);
      break;
    case OriginType::kCrossSite:
      iframe_url = GetCrossSiteUrl(file_path);
      break;
  }

  // Fetch the iframe.
  TestNavigationManager iframe_navigation_manager(web_contents(), iframe_url);
  RenderFrameHost* prerender_rfh = GetPrerenderedMainFrameHost(host_id);
  std::ignore = ExecJs(prerender_rfh, JsReplace(R"(
                const i = document.createElement('iframe');
                i.src = $1;
                document.body.appendChild(i);
             )",
                                                iframe_url.spec()));
  switch (origin_type) {
    case OriginType::kSameOrigin:
      // Wait for the completion of the iframe navigation.
      ASSERT_TRUE(iframe_navigation_manager.WaitForNavigationFinished());
      break;
    case OriginType::kSameSiteCrossOrigin:
    case OriginType::kCrossSite:
      // Cross-origin iframe navigation is deferred in WillStartRequest() before
      // checking the status code.
      ASSERT_TRUE(
          iframe_navigation_manager.WaitForFirstYieldAfterDidStartNavigation());
      auto* request = static_cast<NavigationRequest*>(
          iframe_navigation_manager.GetNavigationHandle());
      EXPECT_TRUE(request->IsDeferredForTesting());
      NavigationThrottleRunner* throttle_runner =
          request->GetNavigationThrottleRunnerForTesting();
      EXPECT_STREQ(
          "PrerenderSubframeNavigationThrottle",
          throttle_runner->GetDeferringThrottle()->GetNameForLogging());
      break;
  }

  // Fetching an iframe whose response has 204/205 status code shouldn't cancel
  // prerendering unlike the mainframe whose response has 204/205 status code.
  // https://wicg.github.io/nav-speculation/prerendering.html#no-bad-navs
  EXPECT_EQ(GetHostForUrl(kPrerenderingUrl), host_id);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderAllowedOnIframe_204_SameOrigin) {
  TestPrerenderAllowedOnIframeWithStatusCode(OriginType::kSameOrigin, "204");
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderAllowedOnIframe_204_SameSiteCrossOrigin) {
  TestPrerenderAllowedOnIframeWithStatusCode(OriginType::kSameSiteCrossOrigin,
                                             "204");
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderAllowedOnIframe_204_CrossSite) {
  TestPrerenderAllowedOnIframeWithStatusCode(OriginType::kCrossSite, "204");
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderAllowedOnIframe_205_SameOrigin) {
  TestPrerenderAllowedOnIframeWithStatusCode(OriginType::kSameOrigin, "205");
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderAllowedOnIframe_205_SameSiteCrossOrigin) {
  TestPrerenderAllowedOnIframeWithStatusCode(OriginType::kSameSiteCrossOrigin,
                                             "205");
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderAllowedOnIframe_205_CrossSite) {
  TestPrerenderAllowedOnIframeWithStatusCode(OriginType::kCrossSite, "205");
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, CancelOnAuthRequested) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl`.
  const GURL kPrerenderingUrl = GetUrl("/auth-basic");
  test::PrerenderHostObserver host_observer(*web_contents_impl(),
                                            kPrerenderingUrl);
  AddPrerenderAsync(kPrerenderingUrl);

  // The prerender should be destroyed.
  host_observer.WaitForDestroyed();
  EXPECT_TRUE(GetHostForUrl(kPrerenderingUrl).is_null());

  // Navigate primary page to flush the metrics.
  const GURL kNavigatedURL = GetUrl("/title2.html");
  ASSERT_TRUE(NavigateToURL(shell(), kNavigatedURL));

  // Cross-check that Preloading.Attempt logs the correct failure reason.
  ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder().BuildEntry(
      PrimaryPageSourceId(), PreloadingType::kPrerender,
      PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
      PreloadingTriggeringOutcome::kFailure,
      ToPreloadingFailureReason(PrerenderFinalStatus::kLoginAuthRequested),
      /*accurate=*/false,
      /*ready_time=*/std::nullopt,
      blink::mojom::SpeculationEagerness::kEager)});

  // Cancellation must have occurred due to authentication request.
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kLoginAuthRequested);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, CancelOnAuthRequestedSubframe) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl`.
  const GURL kPrerenderingUrl = GetUrl("/title1.html");
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);

  // Fetch a subframe that requires authentication.
  const GURL kAuthIFrameUrl = GetUrl("/auth-basic");
  RenderFrameHost* prerender_rfh = GetPrerenderedMainFrameHost(host_id);
  std::ignore =
      ExecJs(prerender_rfh,
             "const i = document.createElement('iframe'); i.src = '" +
                 kAuthIFrameUrl.spec() + "'; document.body.appendChild(i);");

  // The prerender should be destroyed.
  host_observer.WaitForDestroyed();
  EXPECT_TRUE(GetHostForUrl(kPrerenderingUrl).is_null());

  // Cancellation must have occurred due to authentication request.
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kLoginAuthRequested);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, CancelOnAuthRequestedSubResource) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl`.
  const GURL kPrerenderingUrl = GetUrl("/title1.html");
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);

  ASSERT_TRUE(GetHostForUrl(kPrerenderingUrl));

  // Fetch a subresrouce.
  std::string fetch_subresource_script = R"(
        const imgElement = document.createElement('img');
        imgElement.src = '/auth-basic/favicon.gif';
        document.body.appendChild(imgElement);
  )";
  std::ignore =
      ExecJs(GetPrerenderedMainFrameHost(host_id), fetch_subresource_script);

  // The prerender should be destroyed.
  host_observer.WaitForDestroyed();
  EXPECT_TRUE(GetHostForUrl(kPrerenderingUrl).is_null());

  // Cancellation must have occurred due to authentication request.
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kLoginAuthRequested);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       CancelOnSpeculationCandidateRemoved) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering.
  const GURL kPrerenderingUrl = GetUrl("/title2.html");
  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());
  AddPrerenderAsync(kPrerenderingUrl);
  registry_observer.WaitForTrigger(kPrerenderingUrl);
  FrameTreeNodeId host_id = GetHostForUrl(kPrerenderingUrl);
  ASSERT_TRUE(host_id);

  // Remove the rules and check that the prerender is cancelled with an
  // appropriate final status.
  test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);
  ASSERT_TRUE(ExecJs(
      web_contents_impl()->GetPrimaryMainFrame(),
      "document.querySelector('script[type=speculationrules]').remove()"));
  host_observer.WaitForDestroyed();
  EXPECT_TRUE(GetHostForUrl(kPrerenderingUrl).is_null());
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kSpeculationRuleRemoved);
}

// Tests removing speculation rules whose target_hint is "_blank" (i.e.,
// prerender into new tab).
IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    CancelOnSpeculationCandidateRemoved_WithTargetHintBlank) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering.
  const GURL kPrerenderingUrl = GetUrl("/title2.html");
  FrameTreeNodeId host_id = prerender_helper()->AddPrerender(
      kPrerenderingUrl, /*eagerness=*/std::nullopt, "_blank");
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
  ASSERT_NE(prerender_web_contents, web_contents_impl());
  ExpectWebContentsIsForNewTabPrerendering(*prerender_web_contents);

  base::WeakPtr<WebContents> prerender_web_contents_weak =
      prerender_web_contents->GetWeakPtr();

  // Remove the rules and check that the prerender is cancelled with an
  // appropriate final status.
  test::PrerenderHostObserver host_observer(*prerender_web_contents, host_id);
  ASSERT_TRUE(ExecJs(
      web_contents_impl()->GetPrimaryMainFrame(),
      "document.querySelector('script[type=speculationrules]').remove()"));
  host_observer.WaitForDestroyed();
  // During the cancellation, the prerender WebContents should be destroyed.
  EXPECT_FALSE(prerender_web_contents_weak);
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kSpeculationRuleRemoved);

  ukm::SourceId triggering_primary_page_source_id =
      web_contents_impl()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  // The prerender WebContents doesn't have the primary page that can record UKM
  // on destruction. Instead, it asks the primary page hosted on the primary
  // WebContents to record UKM.
  ExpectPreloadingAttemptPreviousPrimaryPageUkm(
      attempt_previous_ukm_entry_builder().BuildEntry(
          triggering_primary_page_source_id, PreloadingType::kPrerender,
          PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
          PreloadingTriggeringOutcome::kReady,
          PreloadingFailureReason::kUnspecified,
          /*accurate=*/false,
          /*ready_time=*/kMockElapsedTime,
          blink::mojom::SpeculationEagerness::kEager));
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DontCancelOnSpeculationUpdateIfStillEligible) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering.
  const GURL kPrerenderingUrl = GetUrl("/title2.html");
  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());
  ASSERT_TRUE(ExecJs(web_contents_impl()->GetPrimaryMainFrame(),
                     JsReplace(
                         R"(
                         let sc = document.createElement('script');
                         sc.type = 'speculationrules';
                         sc.textContent = JSON.stringify({
                           prerender: [
                             {source: "list", urls: [$1]}
                           ]
                         });
                         document.head.appendChild(sc);
                         )",
                         kPrerenderingUrl)));
  registry_observer.WaitForTrigger(kPrerenderingUrl);
  FrameTreeNodeId host_id = GetHostForUrl(kPrerenderingUrl);
  ASSERT_TRUE(host_id);

  ASSERT_TRUE(ExecJs(web_contents_impl()->GetPrimaryMainFrame(),
                     JsReplace(
                         R"(
                         document.querySelector('script[type=speculationrules]')
                             .remove();
                         let sc = document.createElement('script');
                         sc.type = 'speculationrules';
                         sc.textContent = JSON.stringify({
                           prerender: [
                             {source: "list", urls: ["/empty.html", $1]}
                           ]
                         });
                         document.head.appendChild(sc);
                         )",
                         kPrerenderingUrl)));

  // Replace the rules. Even though the original rules are gone, the new ones
  // still permit the prerender so it continues.
  {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
    run_loop.Run();
    ASSERT_TRUE(GetHostForUrl(kPrerenderingUrl));
  }

  // Remove the rules and check that the prerender is cancelled.
  test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);
  ASSERT_TRUE(ExecJs(
      web_contents_impl()->GetPrimaryMainFrame(),
      "document.querySelector('script[type=speculationrules]').remove()"));
  host_observer.WaitForDestroyed();
  EXPECT_TRUE(GetHostForUrl(kPrerenderingUrl).is_null());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       CanStartSecondPrerenderWhenCancellingFirst) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering.
  const GURL kPrerenderingUrl = GetUrl("/title2.html");
  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());
  ASSERT_TRUE(ExecJs(web_contents_impl()->GetPrimaryMainFrame(),
                     JsReplace(
                         R"(
                         let sc = document.createElement('script');
                         sc.type = 'speculationrules';
                         sc.textContent = JSON.stringify({
                           prerender: [
                             {source: "list", urls: [$1]}
                           ]
                         });
                         document.head.appendChild(sc);
                         )",
                         kPrerenderingUrl)));
  registry_observer.WaitForTrigger(kPrerenderingUrl);
  FrameTreeNodeId host_id = GetHostForUrl(kPrerenderingUrl);
  ASSERT_TRUE(host_id);

  // Starting a different prerender still works.
  // (For now, this works unconditionally. In the future this might depend on
  // some other conditions.)
  const GURL kPrerenderingUrl2 = GetUrl("/title3.html");
  test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);
  ASSERT_TRUE(ExecJs(web_contents_impl()->GetPrimaryMainFrame(),
                     JsReplace(
                         R"(
                         document.querySelector('script[type=speculationrules]')
                             .remove();
                         let sc = document.createElement('script');
                         sc.type = 'speculationrules';
                         sc.textContent = JSON.stringify({
                           prerender: [
                             {source: "list", urls: [$1]}
                           ]
                         });
                         document.head.appendChild(sc);
                         )",
                         kPrerenderingUrl2)));

  // The original prerender should be cancelled.
  host_observer.WaitForDestroyed();
  EXPECT_TRUE(GetHostForUrl(kPrerenderingUrl).is_null());

  // And the new one should be discovered.
  registry_observer.WaitForTrigger(kPrerenderingUrl2);
  FrameTreeNodeId second_host_id = GetHostForUrl(kPrerenderingUrl2);
  EXPECT_TRUE(second_host_id);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, RetriggerPrerenderAfterRemoval) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering.
  const GURL kPrerenderingUrl = GetUrl("/title2.html");
  {
    test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());
    ASSERT_TRUE(ExecJs(web_contents_impl()->GetPrimaryMainFrame(),
                       JsReplace(
                           R"(
                          let sc = document.createElement('script');
                          sc.type = 'speculationrules';
                          sc.textContent = JSON.stringify({
                            prerender: [
                              {source: "list", urls: [$1]}
                            ]
                          });
                          document.head.appendChild(sc);
                          )",
                           kPrerenderingUrl)));
    registry_observer.WaitForTrigger(kPrerenderingUrl);
    FrameTreeNodeId host_id = GetHostForUrl(kPrerenderingUrl);
    ASSERT_TRUE(host_id);
    test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);

    // Remove the rules and check that the prerender is cancelled with an
    // appropriate final status.
    ASSERT_TRUE(ExecJs(
        web_contents_impl()->GetPrimaryMainFrame(),
        "document.querySelector('script[type=speculationrules]').remove()"));
    host_observer.WaitForDestroyed();
    EXPECT_TRUE(GetHostForUrl(kPrerenderingUrl).is_null());
  }
  {
    AddPrerender(kPrerenderingUrl);
    FrameTreeNodeId host_id = GetHostForUrl(kPrerenderingUrl);
    EXPECT_TRUE(host_id);
  }
}

// Tests that prerendering triggered by prerendered pages is deferred until
// activation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderChain) {
  // kInitialUrl prerenders kPrerenderChain1, then kPrerenderChain1 prerenders
  // kPrerenderChain2.
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderChain1 =
      GetUrl("/prerender/page_with_trigger_function.html?1");
  const GURL kPrerenderChain2 =
      GetUrl("/prerender/page_with_trigger_function.html?2");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  FrameTreeNodeId host_id = AddPrerender(kPrerenderChain1);

  EXPECT_EQ(GetRequestCount(kPrerenderChain1), 1);
  EXPECT_TRUE(host_id);
  RenderFrameHost* prerender_host = GetPrerenderedMainFrameHost(host_id);
  EXPECT_TRUE(AddTestUtilJS(prerender_host));

  // Add a prerender trigger to the prerendering page.
  EXPECT_TRUE(ExecJs(prerender_host,
                     JsReplace("add_speculation_rules($1)", kPrerenderChain2)));

  // Speculation rules is processed by the idle task runner in Blink. To ensure
  // the speculation candidates has been sent by renderer processes, we should
  // wait until this runner finishes all tasks.
  EXPECT_TRUE(ExecJs(prerender_host, R"(
    const idlePromise = new Promise(resolve => requestIdleCallback(resolve));
    idlePromise;
  )"));

  // Start a navigation request that should not be deferred, and wait it to
  // reach the server. If the prerender request for kPrerenderChain2 is not
  // deferred, the navigation request for kPrerenderChain2 will reach the server
  // earlier than the non-deferred one, so we can wait until the latest request
  // reaches the sever to prove that the prerender request for kPrerenderChain2
  // is deferred.
  EXPECT_TRUE(ExecJs(prerender_host, "add_iframe_async('/title1.html')",
                     EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  WaitForRequest(GetUrl("/title1.html"), 1);

  // The prerender requests were deferred by Mojo capability control, so
  // prerendering pages should not trigger prerendering.
  EXPECT_EQ(GetRequestCount(kPrerenderChain2), 0);
  EXPECT_FALSE(HasHostForUrl(kPrerenderChain2));

  // Activate the prerendering page to grant the deferred prerender requests.
  NavigatePrimaryPage(kPrerenderChain1);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderChain1);

  // The prerendered page was activated. The prerender requests should be
  // processed.
  WaitForPrerenderLoadCompletion(kPrerenderChain2);
  EXPECT_EQ(GetRequestCount(kPrerenderChain2), 1);
  EXPECT_TRUE(HasHostForUrl(kPrerenderChain2));
}

// Tests that sub-frames cannot trigger prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, IgnoreSubFrameInitiatedPrerender) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kSubFrameUrl =
      GetUrl("/prerender/page_with_trigger_function.html");
  const GURL kPrerenderingUrl = GetUrl("/title.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  RenderFrameHostImpl* main_frame_host = current_frame_host();
  EXPECT_TRUE(AddTestUtilJS(main_frame_host));
  EXPECT_EQ("LOADED",
            EvalJs(web_contents(), JsReplace("add_iframe($1)", kSubFrameUrl)));
  RenderFrameHost* child_frame_host = ChildFrameAt(main_frame_host, 0);
  ASSERT_NE(child_frame_host, nullptr);
  ASSERT_EQ(child_frame_host->GetLastCommittedURL(), kSubFrameUrl);

  // Add a prerender trigger to the subframe.
  EXPECT_TRUE(ExecJs(child_frame_host,
                     JsReplace("add_speculation_rules($1)", kPrerenderingUrl)));

  // Speculation rules is processed by the idle task runner in Blink. To ensure
  // the speculation candidates has been sent by renderer processes, we should
  // wait until this runner finishes all tasks.
  EXPECT_TRUE(ExecJs(child_frame_host, R"(
    const idlePromise = new Promise(resolve => requestIdleCallback(resolve));
    idlePromise;
  )"));

  // Start a navigation request that should not be ignored, and wait it to
  // reach the server. If the prerender request is not ignored, the navigation
  // request for kPrerenderingUrl will reach the server earlier than the
  // non-ignored one, so we can wait until the latest request reaches the sever
  // to prove that the prerender request for kPrerenderingUrl is ignored.
  EXPECT_TRUE(ExecJs(main_frame_host, "add_iframe_async('/title1.html')",
                     EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  WaitForRequest(GetUrl("/title1.html"), 1);

  // The prerender requests were ignored by SpeculationHostImpl.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
}

// Regression test for https://crbug.com/1194865.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, CloseOnPrerendering) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prerendering `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  // A prerender host for the URL should be registered.
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl));

  // Should not crash.
  shell()->Close();

  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kTabClosedWithoutUserGesture);
}

namespace {

class RedirectChainObserver : public WebContentsObserver {
 public:
  RedirectChainObserver(WebContents& web_contents, const GURL& url)
      : WebContentsObserver(&web_contents), url_(url) {}
  std::vector<GURL>& redirect_chain() { return redirect_chain_; }

 private:
  void DidFinishNavigation(NavigationHandle* handle) override {
    if (handle->GetURL() != url_)
      return;
    redirect_chain_ = handle->GetRedirectChain();
  }

  const GURL url_;
  std::vector<GURL> redirect_chain_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, SameOriginRedirection) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering a URL that causes same-origin redirection.
  const GURL kRedirectedUrl = GetUrl("/empty.html?prerender");
  const GURL kPrerenderingUrl =
      GetUrl("/server-redirect?" + kRedirectedUrl.spec());
  RedirectChainObserver redirect_chain_observer(*shell()->web_contents(),
                                                kRedirectedUrl);
  AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl), 1);

  ASSERT_EQ(2u, redirect_chain_observer.redirect_chain().size());
  EXPECT_EQ(kPrerenderingUrl, redirect_chain_observer.redirect_chain()[0]);
  EXPECT_EQ(kRedirectedUrl, redirect_chain_observer.redirect_chain()[1]);

  // The prerender host should be registered for the initial request URL, not
  // the redirected URL.
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl));
  EXPECT_FALSE(HasHostForUrl(kRedirectedUrl));

  // Regression test for https://crbug.com/1211274. Make sure that we don't
  // crash when activating a prerendered page which performed a same-origin
  // redirect.
  RedirectChainObserver activation_redirect_chain_observer(
      *shell()->web_contents(), kRedirectedUrl);

  NavigationHandleObserver activation_observer(web_contents(),
                                               kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);
  ASSERT_EQ(1u, activation_redirect_chain_observer.redirect_chain().size());
  EXPECT_EQ(kRedirectedUrl,
            activation_redirect_chain_observer.redirect_chain()[0]);

  // Cross-check that in case redirection when the prerender navigates and user
  // ends up navigating to the redirected URL. accurate_triggering is true.
  ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
  ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder().BuildEntry(
      ukm_source_id, PreloadingType::kPrerender,
      PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
      PreloadingTriggeringOutcome::kSuccess,
      PreloadingFailureReason::kUnspecified,
      /*accurate=*/true,
      /*ready_time=*/kMockElapsedTime,
      blink::mojom::SpeculationEagerness::kEager)});
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, CrossSiteRedirection) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering a URL that causes cross-origin redirection. The
  // cross-origin redirection should fail prerendering.
  const GURL kRedirectedUrl = GetCrossSiteUrl("/empty.html?prerender");
  const GURL kPrerenderingUrl =
      GetUrl("/server-redirect?" + kRedirectedUrl.spec());
  test::PrerenderHostObserver host_observer(*web_contents_impl(),
                                            kPrerenderingUrl);
  AddPrerenderAsync(kPrerenderingUrl);
  host_observer.WaitForDestroyed();
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl), 0);
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  EXPECT_FALSE(HasHostForUrl(kRedirectedUrl));
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kCrossSiteRedirectInInitialNavigation);
}

// Makes sure that activation on navigation for an iframes doesn't happen.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, Activation_iFrame) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  EXPECT_TRUE(AddTestUtilJS(current_frame_host()));

  // Start a prerender.
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);

  // Attempt to activate the prerendered page for an iframe. This should fail
  // and fallback to network request.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ("LOADED", EvalJs(web_contents(),
                             JsReplace("add_iframe($1)", kPrerenderingUrl)));
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 2);

  // Activation shouldn't happen, so the prerender host should not be consumed.
  EXPECT_EQ(GetHostForUrl(kPrerenderingUrl), host_id);
}

// Make sure that the prerendering browsing context has an isolated trivial
// session history. history.length should be limited to 1 in the prerendering
// browsing context.
//
// Explainer:
// https://github.com/jeremyroman/alternate-loading-modes/blob/main/browsing-context.md#session-history
IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    SessionHistoryShouldHaveSingleNavigationEntryInPrerender) {
  // Navigate the primary main frame to an initial page.
  const GURL kInitialUrl = GetUrl("/empty.html?initial");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  TestNavigationHistory(kInitialUrl, /*expected_history_index=*/0,
                        /*expected_history_length=*/1);

  // Navigate the primary main frame to another page so that the initiator
  // page's `history.length` becomes 2. That helps us to distinguish the initial
  // page's session history and the prerendering page's session history. This is
  // not a robust way, but probably good enough in this test.
  const GURL k2ndUrl = GetUrl("/empty.html?2nd");
  ASSERT_TRUE(NavigateToURL(shell(), k2ndUrl));
  TestNavigationHistory(k2ndUrl, /*expected_history_index=*/1,
                        /*expected_history_length=*/2);

  // Start a prerender.
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHost* prerender_frame_host = GetPrerenderedMainFrameHost(host_id);
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  TestNavigationHistory(k2ndUrl, /*expected_history_index=*/1,
                        /*expected_history_length=*/2);
  AssertPrerenderHistoryLength(host_id, prerender_frame_host);

  // From here, we perform several operations which usually append a new entry
  // to the session history, however, all navigations within the prerendering
  // browsing context should be done with replacement in the isolated session
  // history.
  // TODO: Factor out this test into several tests. This test is getting large.

  // Perform history.replaceState() in the prerendered page. Note
  // history.replaceState() doesn't append a new entry anyway. The purpose of
  // testing history.replaceState() here is just for the comparison; pushState()
  // vs replaceState(). Both should have the same behavior in a prerendering
  // browsing context.
  {
    FrameNavigateParamsCapturer capturer(
        FrameTreeNode::From(prerender_frame_host));

    ASSERT_EQ(nullptr, EvalJs(prerender_frame_host,
                              "history.replaceState('state1', null, null)"));

    TestNavigationHistory(k2ndUrl, /*expected_history_index=*/1,
                          /*expected_history_length=*/2);
    AssertPrerenderHistoryLength(host_id, prerender_frame_host);
    EXPECT_EQ("state1", EvalJs(prerender_frame_host, "history.state"));

    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_TRUE(capturer.is_same_document());
    EXPECT_TRUE(capturer.did_replace_entry());
  }

  // Perform history.pushState() in the prerendered page.
  {
    FrameNavigateParamsCapturer capturer(
        FrameTreeNode::From(prerender_frame_host));

    ASSERT_EQ(nullptr, EvalJs(prerender_frame_host,
                              "history.pushState('state2', null, null)"));

    TestNavigationHistory(k2ndUrl, /*expected_history_index=*/1,
                          /*expected_history_length=*/2);
    AssertPrerenderHistoryLength(host_id, prerender_frame_host);
    EXPECT_EQ("state2", EvalJs(prerender_frame_host, "history.state"));

    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_TRUE(capturer.is_same_document());
    EXPECT_TRUE(capturer.did_replace_entry());
  }

  // Do a fragment navigation in the prerendered main frame.
  {
    FrameNavigateParamsCapturer capturer(
        FrameTreeNode::From(prerender_frame_host));

    const GURL kPrerenderingAnchorUrl = GetUrl("/empty.html?prerender#anchor");
    NavigatePrerenderedPage(host_id, kPrerenderingAnchorUrl);
    WaitForPrerenderLoadCompletion(host_id);
    ASSERT_EQ(GetRequestCount(kPrerenderingAnchorUrl), 1);

    TestNavigationHistory(k2ndUrl, /*expected_history_index=*/1,
                          /*expected_history_length=*/2);
    AssertPrerenderHistoryLength(host_id, prerender_frame_host);
    // history.state should be replaced with a fragment navigation.
    EXPECT_EQ(nullptr, EvalJs(prerender_frame_host, "history.state"));

    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_TRUE(capturer.is_same_document());
    EXPECT_TRUE(capturer.did_replace_entry());
  }

  // Add a same-origin iframe to the prerendered page and let it navigate to the
  // different same-origin URL.
  {
    // Add an iframe.
    const GURL kSameOriginSubframeUrl1 =
        GetUrl("/empty.html?same_origin_iframe1");
    EXPECT_TRUE(AddTestUtilJS(prerender_frame_host));
    ASSERT_EQ("LOADED",
              EvalJs(prerender_frame_host,
                     JsReplace("add_iframe($1)", kSameOriginSubframeUrl1)));
    ASSERT_EQ(GetRequestCount(kSameOriginSubframeUrl1), 1);

    auto* child_frame = ChildFrameAt(prerender_frame_host, 0);
    ASSERT_NE(nullptr, child_frame);
    EXPECT_EQ(kSameOriginSubframeUrl1, child_frame->GetLastCommittedURL());

    // Let the added iframe navigate to the different URL.
    {
      FrameNavigateParamsCapturer capturer(FrameTreeNode::From(child_frame));
      const GURL kSameOriginSubframeUrl2 =
          GetUrl("/empty.html?same_origin_iframe2");
      ASSERT_EQ(kSameOriginSubframeUrl2,
                EvalJs(child_frame,
                       JsReplace("location = $1", kSameOriginSubframeUrl2)));
      capturer.Wait();
      child_frame = ChildFrameAt(prerender_frame_host, 0);
      EXPECT_EQ(kSameOriginSubframeUrl2, child_frame->GetLastCommittedURL());
      ASSERT_EQ(GetRequestCount(kSameOriginSubframeUrl2), 1);

      TestNavigationHistory(k2ndUrl, /*expected_history_index=*/1,
                            /*expected_history_length=*/2);
      AssertPrerenderHistoryLength(host_id, prerender_frame_host);
      EXPECT_EQ(nullptr, EvalJs(prerender_frame_host, "history.state"));

      EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());
      EXPECT_FALSE(capturer.is_same_document());
      EXPECT_TRUE(capturer.did_replace_entry());
      EXPECT_TRUE(capturer.is_renderer_initiated());
    }

    // Use WebContents::OpenURL() to let the added iframe navigate.
    {
      FrameNavigateParamsCapturer capturer(FrameTreeNode::From(child_frame));
      const GURL kSameOriginSubframeUrl3 =
          GetUrl("/empty.html?same_origin_iframe3");
      shell()->web_contents()->OpenURL(
          OpenURLParams(kSameOriginSubframeUrl3, Referrer(),
                        child_frame->GetFrameTreeNodeId(),
                        WindowOpenDisposition::CURRENT_TAB,
                        ui::PAGE_TRANSITION_AUTO_SUBFRAME,
                        /*is_renderer_initiated=*/false),
          /*navigation_handle_callback=*/{});
      capturer.Wait();
      child_frame = ChildFrameAt(prerender_frame_host, 0);
      EXPECT_EQ(kSameOriginSubframeUrl3, child_frame->GetLastCommittedURL());
      ASSERT_EQ(GetRequestCount(kSameOriginSubframeUrl3), 1);

      TestNavigationHistory(k2ndUrl, /*expected_history_index=*/1,
                            /*expected_history_length=*/2);
      AssertPrerenderHistoryLength(host_id, prerender_frame_host);
      EXPECT_EQ(nullptr, EvalJs(prerender_frame_host, "history.state"));

      EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());
      EXPECT_FALSE(capturer.is_same_document());
      EXPECT_TRUE(capturer.did_replace_entry());
      EXPECT_FALSE(capturer.is_renderer_initiated());
    }
  }

  // Perform history.back() in the prerendered page, which should be no-op.
  {
    int current_request_count = GetRequestCount(k2ndUrl);
    ASSERT_EQ(nullptr, EvalJs(prerender_frame_host, "history.back()"));
    // Make sure that loading is not happening.
    EXPECT_FALSE(FrameTreeNode::GloballyFindByID(host_id)
                     ->frame_tree()
                     .IsLoadingIncludingInnerFrameTrees());

    TestNavigationHistory(k2ndUrl, /*expected_history_index=*/1,
                          /*expected_history_length=*/2);
    AssertPrerenderHistoryLength(host_id, prerender_frame_host);
    EXPECT_EQ(nullptr, EvalJs(prerender_frame_host, "history.state"));
    EXPECT_EQ(current_request_count, GetRequestCount(k2ndUrl));
  }

  // Perform history.forward() in the prerendered page, which should be no-op.
  {
    int current_request_count = GetRequestCount(k2ndUrl);
    ASSERT_EQ(nullptr, EvalJs(prerender_frame_host, "history.forward()"));
    // Make sure that loading is not happening.
    EXPECT_FALSE(FrameTreeNode::GloballyFindByID(host_id)
                     ->frame_tree()
                     .IsLoadingIncludingInnerFrameTrees());

    TestNavigationHistory(k2ndUrl, /*expected_history_index=*/1,
                          /*expected_history_length=*/2);
    AssertPrerenderHistoryLength(host_id, prerender_frame_host);
    EXPECT_EQ(nullptr, EvalJs(prerender_frame_host, "history.state"));
    EXPECT_EQ(current_request_count, GetRequestCount(k2ndUrl));
  }
}

// Make sure that activation appends the prerendering page's single navigation
// entry to the initiator page's joint session history. We can go back or
// forward after activation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, SessionHistoryAfterActivation) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/empty.html?initial");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  TestNavigationHistory(kInitialUrl, 0, 1);

  // Navigate to another page.
  const GURL k2ndUrl = GetUrl("/empty.html?2nd");
  ASSERT_TRUE(NavigateToURL(shell(), k2ndUrl));
  ASSERT_EQ(GetRequestCount(k2ndUrl), 1);
  TestNavigationHistory(k2ndUrl, 1, 2);

  // Start a prerender.
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHost* prerender_frame_host = GetPrerenderedMainFrameHost(host_id);
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  TestNavigationHistory(k2ndUrl, 1, 2);

  // Call history.pushState(...) in  prerendering.
  ASSERT_EQ(nullptr, EvalJs(prerender_frame_host,
                            "history.pushState('teststate', null, null)"));
  TestNavigationHistory(k2ndUrl, 1, 2);
  AssertPrerenderHistoryLength(host_id, prerender_frame_host);
  EXPECT_EQ("teststate", EvalJs(prerender_frame_host, "history.state"));

  // Activate.
  NavigatePrimaryPage(kPrerenderingUrl);
  // The joint session history becomes [initial, 2nd, <prerender>].
  TestNavigationHistory(kPrerenderingUrl, 2, 3);
  EXPECT_EQ("teststate", EvalJs(web_contents(), "history.state"));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  // Go Back.
  {
    FrameNavigateParamsCapturer capturer(root);
    GoBack();
    // The joint session history becomes [initial, <2nd>, prerender].
    TestNavigationHistory(k2ndUrl, 1, 3);
    EXPECT_EQ(nullptr, EvalJs(web_contents(), "history.state"));

    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_FALSE(capturer.is_same_document());
  }

  // Go Forward.
  {
    FrameNavigateParamsCapturer capturer(root);
    GoForward();
    // The joint session history becomes [initial, 2nd, <prerender>].
    TestNavigationHistory(kPrerenderingUrl, 2, 3);
    EXPECT_EQ("teststate", EvalJs(web_contents(), "history.state"));

    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_FALSE(capturer.is_same_document());
  }
}

class PrerenderOopsifBrowserTest : public PrerenderBrowserTest {
 public:
  PrerenderOopsifBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kIsolateSandboxedIframes,
          {{"grouping", "per-origin"}}}},
        {/* disabled_features */});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test for crbug.com/1470312.
// Prior to the CL that introduced this test, if IsolatedSandboxedIframes are
// enabled, and an isolated frame sends the parent a postMessage, then the
// proxies were attached to the active page and not the prerendered  mainframe.
// These were proxies that were created on demand when processing the
// postMessage. (to ensure the recipient can reply to the sender frame, or to a
// frame that the sender could reach). This led to a CHECK failure in
// ~BrowsingContextInstance(). This test verifies that problem has been
// resolved.
IN_PROC_BROWSER_TEST_F(PrerenderOopsifBrowserTest,
                       OopsifSrcdocSandboxIframeWithPostmessage) {
  // Navigate to an initial page.
  const GURL kInitialUrl =
      GetUrl("/prerender/cross_origin_prerender.html?initial");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  EXPECT_TRUE(AddTestUtilJS(current_frame_host()));

  // Start a prerender.
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/cross_origin_srcdoc_sandboxed_postmessage.html");
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHost* prerender_frame_host = GetPrerenderedMainFrameHost(host_id);
  EXPECT_TRUE(AddTestUtilJS(prerender_frame_host));
  // Create a srcdoc iframe in the prerendered page.
  EXPECT_TRUE(ExecJs(prerender_frame_host, "createSrcdoc();"));
  base::RunLoop().RunUntilIdle();

  // Load another same-origin iframe to ensure loading the srcdoc iframe
  // starts and then it's deferred until activation.
  const GURL kSameOriginSubframeUrl =
      GetUrl("/prerender/cross_origin_prerender.html");
  ASSERT_EQ("LOADED",
            EvalJs(prerender_frame_host,
                   JsReplace("add_iframe($1)", kSameOriginSubframeUrl)));

  // Activate.
  NavigatePrimaryPage(kPrerenderingUrl);
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);

  // Verify postMessage from srcdoc to mainframe completed.
  RenderFrameHostImpl* main_frame =
      static_cast<RenderFrameHostImpl*>(web_contents()->GetPrimaryMainFrame());
  EXPECT_TRUE(ExecJs(
      main_frame,
      "Promise.all([child_response_promise, prerender_handler_promise]);"));

  // OOPSIFs only process-isolate if the parent gets site isolation, which in
  // this case doesn't happen on Android.
  if (AreAllSitesIsolatedForTesting()) {
    RenderFrameHostImpl* sandboxed_render_frame_host =
        main_frame->child_at(0)->current_frame_host();
    EXPECT_TRUE(sandboxed_render_frame_host->GetSiteInstance()
                    ->GetSiteInfo()
                    .is_sandboxed());
    ASSERT_NE(main_frame->GetProcess(),
              sandboxed_render_frame_host->GetProcess());
  }
}

// Makes sure that cross-origin subframe navigations are deferred during
// prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DeferCrossOriginSubframeNavigation) {
  // Navigate to an initial page.
  const GURL kInitialUrl =
      GetUrl("/prerender/cross_origin_prerender.html?initial");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  EXPECT_TRUE(AddTestUtilJS(current_frame_host()));

  // Start a prerender.
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/cross_origin_prerender.html?prerender");
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);

  const GURL kSameOriginSubframeUrl =
      GetUrl("/prerender/cross_origin_prerender.html?same_origin_iframe");
  const GURL kCrossOriginSubframeUrl = GetCrossSiteUrl(
      "/prerender/cross_origin_prerender.html?cross_origin_iframe");

  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  ASSERT_EQ(GetRequestCount(kSameOriginSubframeUrl), 0);
  ASSERT_EQ(GetRequestCount(kCrossOriginSubframeUrl), 0);

  // Add a cross-origin iframe to the prerendering page.
  RenderFrameHost* prerender_frame_host = GetPrerenderedMainFrameHost(host_id);
  EXPECT_TRUE(AddTestUtilJS(prerender_frame_host));

  // Use ExecuteScriptAsync instead of EvalJs as inserted cross-origin iframe
  // navigation would be deferred and script execution does not finish until
  // the activation.
  ExecuteScriptAsync(prerender_frame_host, JsReplace("add_iframe_async($1)",
                                                     kCrossOriginSubframeUrl));
  base::RunLoop().RunUntilIdle();

  // Add a same-origin iframe to the prerendering page.
  ASSERT_EQ("LOADED",
            EvalJs(prerender_frame_host,
                   JsReplace("add_iframe($1)", kSameOriginSubframeUrl)));
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  ASSERT_EQ(GetRequestCount(kSameOriginSubframeUrl), 1);
  ASSERT_EQ(GetRequestCount(kCrossOriginSubframeUrl), 0);

  // Activate.
  NavigatePrimaryPage(kPrerenderingUrl);
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
  ASSERT_EQ("LOADED",
            EvalJs(prerender_frame_host, JsReplace("wait_iframe_async($1)",
                                                   kCrossOriginSubframeUrl)));
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  ASSERT_EQ(GetRequestCount(kSameOriginSubframeUrl), 1);
  EXPECT_EQ(GetRequestCount(kCrossOriginSubframeUrl), 1);

  const char kInitialDocumentPrerenderingScript[] =
      "initial_document_prerendering";
  const char kCurrentDocumentPrerenderingScript[] = "document.prerendering";
  const char kOnprerenderingchangeObservedScript[] =
      "onprerenderingchange_observed";
  const char kActivationStartScript[] =
      "performance.getEntriesByType('navigation')[0].activationStart";
  EXPECT_EQ(true,
            EvalJs(prerender_frame_host, kInitialDocumentPrerenderingScript));
  EXPECT_EQ(false,
            EvalJs(prerender_frame_host, kCurrentDocumentPrerenderingScript));
  EXPECT_EQ(true,
            EvalJs(prerender_frame_host, kOnprerenderingchangeObservedScript));
  EXPECT_NE(0, EvalJs(prerender_frame_host, kActivationStartScript));

  RenderFrameHost* same_origin_render_frame_host = FindRenderFrameHost(
      prerender_frame_host->GetPage(), kSameOriginSubframeUrl);
  CHECK(same_origin_render_frame_host);
  EXPECT_EQ(true, EvalJs(same_origin_render_frame_host,
                         kInitialDocumentPrerenderingScript));
  EXPECT_EQ(false, EvalJs(same_origin_render_frame_host,
                          kCurrentDocumentPrerenderingScript));
  EXPECT_EQ(true, EvalJs(same_origin_render_frame_host,
                         kOnprerenderingchangeObservedScript));
  EXPECT_NE(0, EvalJs(same_origin_render_frame_host, kActivationStartScript));

  RenderFrameHost* cross_origin_render_frame_host = FindRenderFrameHost(
      prerender_frame_host->GetPage(), kCrossOriginSubframeUrl);
  CHECK(cross_origin_render_frame_host);
  EXPECT_EQ(false, EvalJs(cross_origin_render_frame_host,
                          kInitialDocumentPrerenderingScript));
  EXPECT_EQ(false, EvalJs(cross_origin_render_frame_host,
                          kCurrentDocumentPrerenderingScript));
  EXPECT_EQ(false, EvalJs(cross_origin_render_frame_host,
                          kOnprerenderingchangeObservedScript));
  EXPECT_EQ(0, EvalJs(cross_origin_render_frame_host, kActivationStartScript));
}

// Makes sure that subframe navigations are deferred if cross-origin redirects
// are observed in a prerendering page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DeferCrossOriginRedirectsOnSubframeNavigation) {
  // Navigate to an initial page.
  const GURL kInitialUrl =
      GetUrl("/prerender/cross_origin_prerender.html?initial");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/cross_origin_prerender.html?prerender");
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);

  const GURL kCrossOriginSubframeUrl = GetCrossSiteUrl(
      "/prerender/cross_origin_prerender.html?cross_origin_iframe");
  const GURL kServerRedirectSubframeUrl =
      GetUrl("/server-redirect?" + kCrossOriginSubframeUrl.spec());

  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  ASSERT_EQ(GetRequestCount(kServerRedirectSubframeUrl), 0);
  ASSERT_EQ(GetRequestCount(kCrossOriginSubframeUrl), 0);

  // Add an iframe pointing to a server redirect page to the prerendering page.
  RenderFrameHost* prerender_frame_host = GetPrerenderedMainFrameHost(host_id);
  EXPECT_TRUE(AddTestUtilJS(prerender_frame_host));
  // Use ExecuteScriptAsync instead of EvalJs as inserted iframe redirect
  // navigation would be deferred and script execution does not finish until
  // the activation.
  ExecuteScriptAsync(
      prerender_frame_host,
      JsReplace("add_iframe_async($1)", kServerRedirectSubframeUrl));
  WaitForRequest(kServerRedirectSubframeUrl, 1);
  ASSERT_EQ(GetRequestCount(kServerRedirectSubframeUrl), 1);
  ASSERT_EQ(GetRequestCount(kCrossOriginSubframeUrl), 0);

  // Activate.
  NavigatePrimaryPage(kPrerenderingUrl);
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
  ASSERT_EQ("LOADED", EvalJs(prerender_frame_host,
                             JsReplace("wait_iframe_async($1)",
                                       kServerRedirectSubframeUrl)));
  EXPECT_EQ(GetRequestCount(kServerRedirectSubframeUrl), 1);
  EXPECT_EQ(GetRequestCount(kCrossOriginSubframeUrl), 1);

  const char kInitialDocumentPrerenderingScript[] =
      "initial_document_prerendering";
  const char kCurrentDocumentPrerenderingScript[] = "document.prerendering";
  const char kOnprerenderingchangeObservedScript[] =
      "onprerenderingchange_observed";
  EXPECT_EQ(true,
            EvalJs(prerender_frame_host, kInitialDocumentPrerenderingScript));
  EXPECT_EQ(false,
            EvalJs(prerender_frame_host, kCurrentDocumentPrerenderingScript));
  EXPECT_EQ(true,
            EvalJs(prerender_frame_host, kOnprerenderingchangeObservedScript));

  RenderFrameHost* cross_origin_render_frame_host = FindRenderFrameHost(
      prerender_frame_host->GetPage(), kCrossOriginSubframeUrl);
  CHECK(cross_origin_render_frame_host);
  EXPECT_EQ(false, EvalJs(cross_origin_render_frame_host,
                          kInitialDocumentPrerenderingScript));
  EXPECT_EQ(false, EvalJs(cross_origin_render_frame_host,
                          kCurrentDocumentPrerenderingScript));
  EXPECT_EQ(false, EvalJs(cross_origin_render_frame_host,
                          kOnprerenderingchangeObservedScript));
}

// Tests for main frame navigation in a prerendered page.
class PrerenderMainFrameNavigationBrowserTest
    : public testing::WithParamInterface<PreloadingTriggerType>,
      public PrerenderBrowserTest {
 protected:
  enum class NavigationType {
    kSameOrigin,
    kSameSiteCrossOrigin,
    kSameSiteCrossOriginWithOptIn,
    kCrossSite,
  };

  // Runs navigations in the `navigations_types` order and makes sure it ends
  // with `expected_status`.
  void TestMainFrameNavigation(
      const std::vector<NavigationType>& navigation_types,
      PrerenderFinalStatus expected_status) {
    ASSERT_FALSE(navigation_types.empty());
    PreloadingTriggerType trigger_type = GetParam();

    std::vector<GURL> urls;
    for (auto type : navigation_types) {
      urls.push_back(CreateUrl(type));
    }

    const GURL kInitialUrl = GetUrl("/empty.html");
    const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

    // Navigate to an initial page.
    ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

    // Start a prerender.
    FrameTreeNodeId host_id;
    std::unique_ptr<PrerenderHandle> prerender_handle;
    switch (trigger_type) {
      case PreloadingTriggerType::kSpeculationRule:
        host_id = AddPrerender(kPrerenderingUrl);
        break;
      case PreloadingTriggerType::kSpeculationRuleFromIsolatedWorld:
        host_id = AddPrerender(kPrerenderingUrl, /*world_id=*/1);
        break;
      case PreloadingTriggerType::kEmbedder:
        prerender_handle = AddEmbedderTriggeredPrerender(kPrerenderingUrl);
        host_id = static_cast<PrerenderHandleImpl*>(prerender_handle.get())
                      ->frame_tree_node_id_for_testing();
        break;
      case PreloadingTriggerType::kSpeculationRuleFromAutoSpeculationRules:
        FAIL() << "Auto speculation rules does not work with empty.html";
    }
    ASSERT_TRUE(host_id);

    test::PrerenderHostObserver observer(*web_contents_impl(), host_id);

    // Run navigations in the main frame of the prerendered page. Only the last
    // URL of the given navigation URLs will separately be handled later as that
    // could cancel prerendering and never finish.
    for (auto it = urls.begin(); it != urls.end() - 1; ++it) {
      TestNavigationManager navigation_observer(web_contents(), *it);
      NavigatePrerenderedPage(host_id, *it);
      ASSERT_TRUE(navigation_observer.WaitForNavigationFinished());
      EXPECT_TRUE(navigation_observer.was_successful());
    }

    // The last navigation URL. This should cancel prerendering if the
    // expectation is not kActivated.
    const GURL& last_url = urls.back();

    switch (expected_status) {
      case PrerenderFinalStatus::kActivated: {
        // Navigation to the last URL should succeed.
        TestNavigationManager navigation_observer(web_contents(), last_url);
        NavigatePrerenderedPage(host_id, last_url);
        ASSERT_TRUE(navigation_observer.WaitForNavigationFinished());
        EXPECT_TRUE(navigation_observer.was_successful());

        // Activation should succeed.
        switch (trigger_type) {
          case PreloadingTriggerType::kSpeculationRule:
          case PreloadingTriggerType::kSpeculationRuleFromIsolatedWorld:
            NavigatePrimaryPage(kPrerenderingUrl);
            break;
          case PreloadingTriggerType::kEmbedder:
            NavigatePrimaryPageFromAddressBar(kPrerenderingUrl);
            break;
          case PreloadingTriggerType::kSpeculationRuleFromAutoSpeculationRules:
            FAIL() << "Auto speculation rules does not work with empty.html";
        }
        observer.WaitForActivation();
        EXPECT_TRUE(observer.was_activated());
        EXPECT_EQ(web_contents()->GetLastCommittedURL(), last_url);
        break;
      }
      default: {
        // Navigation to the last URL should cancel prerendering.
        NavigatePrerenderedPage(host_id, last_url);
        observer.WaitForDestroyed();
        EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
        break;
      }
    }

    // Verify UMA/UKM records.
    switch (trigger_type) {
      case PreloadingTriggerType::kSpeculationRule:
        ExpectFinalStatusForSpeculationRule(expected_status);
        break;
      case PreloadingTriggerType::kSpeculationRuleFromIsolatedWorld:
        ExpectFinalStatusForSpeculationRuleFromIsolatedWorld(expected_status);
        break;
      case PreloadingTriggerType::kEmbedder:
        ExpectFinalStatusForEmbedder(expected_status);
        break;
      case PreloadingTriggerType::kSpeculationRuleFromAutoSpeculationRules:
        FAIL() << "Auto speculation rules does not work with empty.html";
    }
  }

  // Runs redirections in the `navigations_types` order and makes sure it ends
  // with `expected_status`.
  void TestMainFrameRedirection(
      const std::vector<NavigationType>& redirection_types,
      PrerenderFinalStatus expected_status) {
    ASSERT_FALSE(redirection_types.empty());
    PreloadingTriggerType trigger_type = GetParam();

    // Create a URL that rus a redirection sequence in the order of
    // `redirection_types`. To make the URL, create a final URL from the last
    // element of `redirection_types` and then prefix a rediretion URL by
    // iterating the types in reverse order.
    const GURL final_url = CreateUrl(redirection_types.back());
    GURL url = final_url;
    for (auto it = redirection_types.rbegin() + 1;
         it != redirection_types.rend(); ++it) {
      url = CreateRedirectionUrl(*it, url);
    }

    const GURL kInitialUrl = GetUrl("/empty.html");
    const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

    // Navigate to an initial page.
    ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

    // Start a prerender.
    FrameTreeNodeId host_id;
    std::unique_ptr<PrerenderHandle> prerender_handle;
    switch (trigger_type) {
      case PreloadingTriggerType::kSpeculationRule:
        host_id = AddPrerender(kPrerenderingUrl);
        break;
      case PreloadingTriggerType::kSpeculationRuleFromIsolatedWorld:
        host_id = AddPrerender(kPrerenderingUrl, /*world_id=*/1);
        break;
      case PreloadingTriggerType::kEmbedder:
        prerender_handle = AddEmbedderTriggeredPrerender(kPrerenderingUrl);
        host_id = static_cast<PrerenderHandleImpl*>(prerender_handle.get())
                      ->frame_tree_node_id_for_testing();
        break;
      case PreloadingTriggerType::kSpeculationRuleFromAutoSpeculationRules:
        FAIL() << "Auto speculation rules does not work with empty.html";
    }
    ASSERT_TRUE(host_id);

    test::PrerenderHostObserver observer(*web_contents_impl(), host_id);

    // Run redirections in the main frame of the prerendered page.
    TestNavigationManager navigation_observer(web_contents(), url);
    NavigatePrerenderedPage(host_id, url);
    ASSERT_TRUE(navigation_observer.WaitForNavigationFinished());

    switch (expected_status) {
      case PrerenderFinalStatus::kActivated: {
        // Redirections should succeed.
        EXPECT_TRUE(navigation_observer.was_successful());

        // Activation should succeed.
        switch (trigger_type) {
          case PreloadingTriggerType::kSpeculationRule:
          case PreloadingTriggerType::kSpeculationRuleFromIsolatedWorld:
            NavigatePrimaryPage(kPrerenderingUrl);
            break;
          case PreloadingTriggerType::kEmbedder:
            NavigatePrimaryPageFromAddressBar(kPrerenderingUrl);
            break;
          case PreloadingTriggerType::kSpeculationRuleFromAutoSpeculationRules:
            FAIL() << "Auto speculation rules does not work with empty.html";
        }
        observer.WaitForActivation();
        EXPECT_TRUE(observer.was_activated());
        EXPECT_EQ(web_contents()->GetLastCommittedURL(), final_url);
        break;
      }
      default: {
        // Redirections should cancel prerendering.
        EXPECT_FALSE(navigation_observer.was_successful());
        observer.WaitForDestroyed();
        EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
        break;
      }
    }

    // Verify UMA/UKM records.
    switch (trigger_type) {
      case PreloadingTriggerType::kSpeculationRule:
        ExpectFinalStatusForSpeculationRule(expected_status);
        break;
      case PreloadingTriggerType::kSpeculationRuleFromIsolatedWorld:
        ExpectFinalStatusForSpeculationRuleFromIsolatedWorld(expected_status);
        break;
      case PreloadingTriggerType::kEmbedder:
        ExpectFinalStatusForEmbedder(expected_status);
        break;
      case PreloadingTriggerType::kSpeculationRuleFromAutoSpeculationRules:
        FAIL() << "Auto speculation rules does not work with empty.html";
    }
  }

 private:
  GURL CreateUrl(NavigationType type) {
    static int number_for_prefix = 0;
    std::string prefix = base::NumberToString(number_for_prefix++);
    switch (type) {
      case NavigationType::kSameOrigin:
        return GetUrl("/empty.html?" + prefix);
      case NavigationType::kSameSiteCrossOrigin:
        return GetSameSiteCrossOriginUrl("/empty.html?" + prefix);
      case NavigationType::kSameSiteCrossOriginWithOptIn:
        return GetSameSiteCrossOriginUrl(
            "/prerender/prerender_with_opt_in_header.html?" + prefix);
      case NavigationType::kCrossSite:
        return GetCrossSiteUrl("/empty.html?" + prefix);
    }
  }

  // Creates a URL that redirects to `url_to_redirect`. The origin of the URL is
  // determined by `type`.
  GURL CreateRedirectionUrl(NavigationType type, const GURL& url_to_redirect) {
    switch (type) {
      case NavigationType::kSameOrigin:
        return GetUrl("/server-redirect?" + url_to_redirect.spec());
      case NavigationType::kSameSiteCrossOrigin:
        return GetSameSiteCrossOriginUrl("/server-redirect?" +
                                         url_to_redirect.spec());
      case NavigationType::kSameSiteCrossOriginWithOptIn:
        return GetSameSiteCrossOriginUrl(
            "/server-redirect-credentialed-prerender?" +
            url_to_redirect.spec());
      case NavigationType::kCrossSite:
        return GetCrossSiteUrl("/server-redirect?" + url_to_redirect.spec());
    }
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PrerenderMainFrameNavigationBrowserTest,
    testing::Values(PreloadingTriggerType::kSpeculationRule,
                    PreloadingTriggerType::kSpeculationRuleFromIsolatedWorld,
                    PreloadingTriggerType::kEmbedder),
    [](const testing::TestParamInfo<PreloadingTriggerType>& info) {
      switch (info.param) {
        case PreloadingTriggerType::kSpeculationRule:
          return "SpeculationRule";
        case PreloadingTriggerType::kSpeculationRuleFromIsolatedWorld:
          return "SpeculationRuleFromIsolatedWorld";
        case PreloadingTriggerType::kEmbedder:
          return "Embedder";
        case PreloadingTriggerType::kSpeculationRuleFromAutoSpeculationRules:
          ADD_FAILURE() << "Auto speculation rules does not work with "
                           "TestMainFrameNavigation";
          return "SpeculationRuleFromAutoSpeculationRules";
      }
    });

IN_PROC_BROWSER_TEST_P(PrerenderMainFrameNavigationBrowserTest, SameOrigin) {
  std::vector<NavigationType> navigations = {NavigationType::kSameOrigin};
  TestMainFrameNavigation(navigations, PrerenderFinalStatus::kActivated);
}

IN_PROC_BROWSER_TEST_P(PrerenderMainFrameNavigationBrowserTest,
                       SameSiteCrossOriginWithOptIn) {
  std::vector<NavigationType> navigations = {
      NavigationType::kSameSiteCrossOriginWithOptIn};
  TestMainFrameNavigation(navigations, PrerenderFinalStatus::kActivated);
}

IN_PROC_BROWSER_TEST_P(PrerenderMainFrameNavigationBrowserTest,
                       SameSiteCrossOrigin) {
  std::vector<NavigationType> navigations = {
      NavigationType::kSameSiteCrossOrigin};
  TestMainFrameNavigation(
      navigations,
      PrerenderFinalStatus::
          kSameSiteCrossOriginNavigationNotOptInInMainFrameNavigation);
}

IN_PROC_BROWSER_TEST_P(PrerenderMainFrameNavigationBrowserTest, CrossSite) {
  std::vector<NavigationType> navigations = {NavigationType::kCrossSite};
  TestMainFrameNavigation(
      navigations,
      PrerenderFinalStatus::kCrossSiteNavigationInMainFrameNavigation);
}

IN_PROC_BROWSER_TEST_P(PrerenderMainFrameNavigationBrowserTest,
                       SameSiteCrossOriginWithOptIn_SameOrigin) {
  std::vector<NavigationType> navigations = {
      NavigationType::kSameSiteCrossOriginWithOptIn,
      NavigationType::kSameOrigin};
  TestMainFrameNavigation(navigations, PrerenderFinalStatus::kActivated);
}

IN_PROC_BROWSER_TEST_P(
    PrerenderMainFrameNavigationBrowserTest,
    SameSiteCrossOriginWithOptIn_SameSiteCrossOriginWithOptIn) {
  std::vector<NavigationType> navigations = {
      NavigationType::kSameSiteCrossOriginWithOptIn,
      NavigationType::kSameSiteCrossOriginWithOptIn};
  TestMainFrameNavigation(navigations, PrerenderFinalStatus::kActivated);
}

IN_PROC_BROWSER_TEST_P(PrerenderMainFrameNavigationBrowserTest,
                       SameSiteCrossOriginWithOptIn_SameSiteCrossOrigin) {
  std::vector<NavigationType> navigations = {
      NavigationType::kSameSiteCrossOriginWithOptIn,
      NavigationType::kSameSiteCrossOrigin};
  TestMainFrameNavigation(
      navigations,
      PrerenderFinalStatus::
          kSameSiteCrossOriginNavigationNotOptInInMainFrameNavigation);
}

IN_PROC_BROWSER_TEST_P(PrerenderMainFrameNavigationBrowserTest,
                       SameSiteCrossOrigin_CrossSite) {
  std::vector<NavigationType> navigations = {
      NavigationType::kSameSiteCrossOriginWithOptIn,
      NavigationType::kCrossSite};
  TestMainFrameNavigation(
      navigations,
      PrerenderFinalStatus::kCrossSiteNavigationInMainFrameNavigation);
}

IN_PROC_BROWSER_TEST_P(PrerenderMainFrameNavigationBrowserTest,
                       Redirection_SameOrigin_SameOrigin) {
  std::vector<NavigationType> redirections = {NavigationType::kSameOrigin,
                                              NavigationType::kSameOrigin};
  TestMainFrameRedirection(redirections, PrerenderFinalStatus::kActivated);
}

IN_PROC_BROWSER_TEST_P(PrerenderMainFrameNavigationBrowserTest,
                       Redirection_SameOrigin_SameSiteCrossOriginWithOptIn) {
  std::vector<NavigationType> redirections = {
      NavigationType::kSameOrigin,
      NavigationType::kSameSiteCrossOriginWithOptIn};
  TestMainFrameRedirection(redirections, PrerenderFinalStatus::kActivated);
}

IN_PROC_BROWSER_TEST_P(PrerenderMainFrameNavigationBrowserTest,
                       Redirection_SameOrigin_SameSiteCrossOrigin) {
  std::vector<NavigationType> redirections = {
      NavigationType::kSameOrigin, NavigationType::kSameSiteCrossOrigin};
  TestMainFrameRedirection(
      redirections,
      PrerenderFinalStatus::
          kSameSiteCrossOriginRedirectNotOptInInMainFrameNavigation);
}

IN_PROC_BROWSER_TEST_P(PrerenderMainFrameNavigationBrowserTest,
                       Redirection_SameOrigin_CrossSite) {
  std::vector<NavigationType> redirections = {NavigationType::kSameOrigin,
                                              NavigationType::kCrossSite};
  TestMainFrameRedirection(
      redirections,
      PrerenderFinalStatus::kCrossSiteRedirectInMainFrameNavigation);
}

IN_PROC_BROWSER_TEST_P(PrerenderMainFrameNavigationBrowserTest,
                       Redirection_SameSiteCrossOriginWithOptIn_SameOrigin) {
  std::vector<NavigationType> redirections = {
      NavigationType::kSameSiteCrossOriginWithOptIn,
      NavigationType::kSameOrigin};
  TestMainFrameRedirection(redirections, PrerenderFinalStatus::kActivated);
}

IN_PROC_BROWSER_TEST_P(
    PrerenderMainFrameNavigationBrowserTest,
    Redirection_SameSiteCrossOriginWithOptIn_SameSiteCrossOriginWithOptIn) {
  std::vector<NavigationType> redirections = {
      NavigationType::kSameSiteCrossOriginWithOptIn,
      NavigationType::kSameSiteCrossOriginWithOptIn};
  TestMainFrameRedirection(redirections, PrerenderFinalStatus::kActivated);
}

IN_PROC_BROWSER_TEST_P(
    PrerenderMainFrameNavigationBrowserTest,
    Redirection_SameSiteCrossOriginWithOptIn_SameSiteCrossOrigin) {
  std::vector<NavigationType> redirections = {
      NavigationType::kSameSiteCrossOriginWithOptIn,
      NavigationType::kSameSiteCrossOrigin};
  TestMainFrameRedirection(
      redirections,
      PrerenderFinalStatus::
          kSameSiteCrossOriginRedirectNotOptInInMainFrameNavigation);
}

IN_PROC_BROWSER_TEST_P(PrerenderMainFrameNavigationBrowserTest,
                       Redirection_SameSiteCrossOriginWithOptIn_CrossSite) {
  std::vector<NavigationType> redirections = {
      NavigationType::kSameSiteCrossOriginWithOptIn,
      NavigationType::kCrossSite};
  TestMainFrameRedirection(
      redirections,
      PrerenderFinalStatus::kCrossSiteRedirectInMainFrameNavigation);
}

IN_PROC_BROWSER_TEST_P(
    PrerenderMainFrameNavigationBrowserTest,
    Redirection_SameOrigin_SameSiteCrossOriginWithOptIn_SameOrigin) {
  std::vector<NavigationType> redirections = {
      NavigationType::kSameOrigin,
      NavigationType::kSameSiteCrossOriginWithOptIn,
      NavigationType::kSameOrigin};
  TestMainFrameRedirection(redirections, PrerenderFinalStatus::kActivated);
}

IN_PROC_BROWSER_TEST_P(
    PrerenderMainFrameNavigationBrowserTest,
    Redirection_SameOrigin_SameSiteCrossOriginWithOptIn_SameSiteCrossOriginWithOptIn) {
  std::vector<NavigationType> redirections = {
      NavigationType::kSameOrigin,
      NavigationType::kSameSiteCrossOriginWithOptIn,
      NavigationType::kSameSiteCrossOriginWithOptIn};
  TestMainFrameRedirection(redirections, PrerenderFinalStatus::kActivated);
}

IN_PROC_BROWSER_TEST_P(
    PrerenderMainFrameNavigationBrowserTest,
    Redirection_SameOrigin_SameSiteCrossOriginWithOptIn_SameSiteCrossOrigin) {
  std::vector<NavigationType> redirections = {
      NavigationType::kSameOrigin,
      NavigationType::kSameSiteCrossOriginWithOptIn,
      NavigationType::kSameSiteCrossOrigin};
  TestMainFrameRedirection(
      redirections,
      PrerenderFinalStatus::
          kSameSiteCrossOriginRedirectNotOptInInMainFrameNavigation);
}

IN_PROC_BROWSER_TEST_P(
    PrerenderMainFrameNavigationBrowserTest,
    Redirection_SameOrigin_SameSiteCrossOriginWithOptIn_CrossSite) {
  std::vector<NavigationType> redirections = {
      NavigationType::kSameOrigin,
      NavigationType::kSameSiteCrossOriginWithOptIn,
      NavigationType::kCrossSite};
  TestMainFrameRedirection(
      redirections,
      PrerenderFinalStatus::kCrossSiteRedirectInMainFrameNavigation);
}

IN_PROC_BROWSER_TEST_P(
    PrerenderMainFrameNavigationBrowserTest,
    Redirection_SameSiteCrossOriginWithOptIn_SameOrigin_SameOrigin) {
  std::vector<NavigationType> redirections = {
      NavigationType::kSameSiteCrossOriginWithOptIn,
      NavigationType::kSameOrigin, NavigationType::kSameOrigin};
  TestMainFrameRedirection(redirections, PrerenderFinalStatus::kActivated);
}

IN_PROC_BROWSER_TEST_P(
    PrerenderMainFrameNavigationBrowserTest,
    Redirection_SameSiteCrossOriginWithOptIn_SameOrigin_SameSiteCrossOriginWithOptIn) {
  std::vector<NavigationType> redirections = {
      NavigationType::kSameSiteCrossOriginWithOptIn,
      NavigationType::kSameOrigin,
      NavigationType::kSameSiteCrossOriginWithOptIn};
  TestMainFrameRedirection(redirections, PrerenderFinalStatus::kActivated);
}

IN_PROC_BROWSER_TEST_P(
    PrerenderMainFrameNavigationBrowserTest,
    Redirection_SameSiteCrossOriginWithOptIn_SameOrigin_SameSiteCrossOrigin) {
  std::vector<NavigationType> redirections = {
      NavigationType::kSameSiteCrossOriginWithOptIn,
      NavigationType::kSameOrigin, NavigationType::kSameSiteCrossOrigin};
  TestMainFrameRedirection(
      redirections,
      PrerenderFinalStatus::
          kSameSiteCrossOriginRedirectNotOptInInMainFrameNavigation);
}

IN_PROC_BROWSER_TEST_P(
    PrerenderMainFrameNavigationBrowserTest,
    Redirection_SameSiteCrossOriginWithOptIn_SameOrigin_CrossSite) {
  std::vector<NavigationType> redirections = {
      NavigationType::kSameSiteCrossOriginWithOptIn,
      NavigationType::kSameOrigin, NavigationType::kCrossSite};
  TestMainFrameRedirection(
      redirections,
      PrerenderFinalStatus::kCrossSiteRedirectInMainFrameNavigation);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, MainFrameNavigation_NonHttpUrl) {
  const GURL initial_url = GetUrl("/empty.html");
  const GURL prerendering_url = GetUrl("/empty.html?prerender");
  // Note that local schemes (e.g., data URL) don't work for this test as
  // renderer-initiated navigation to those schemes are blocked by unrelated
  // navigation throttles like BlockedSchemeNavigationThrottle.
  const GURL non_http_url("ftp://example.com/");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), initial_url);

  // Start prerendering.
  FrameTreeNodeId host_id = AddPrerender(prerendering_url);
  ASSERT_TRUE(host_id);

  // Navigation to a non-http(s) URL on a prerendered page should cancel
  // prerendering.
  TestNavigationManager navigation_observer(web_contents(), non_http_url);
  NavigatePrerenderedPage(host_id, non_http_url);
  ASSERT_TRUE(navigation_observer.WaitForNavigationFinished());
  EXPECT_FALSE(navigation_observer.was_successful());
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kInvalidSchemeNavigation);
}

// Regression test for https://crbug.com/1198051
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, MainFrameFragmentNavigation) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl =
      GetUrl("/navigation_controller/hash_anchor_with_iframe.html");
  const GURL kAnchorUrl =
      GetUrl("/navigation_controller/hash_anchor_with_iframe.html#Test");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);

  // Do a fragment navigation.
  NavigatePrerenderedPage(host_id, kAnchorUrl);
  WaitForPrerenderLoadCompletion(host_id);

  RedirectChainObserver redirect_chain_observer(*shell()->web_contents(),
                                                kAnchorUrl);

  // Activate.
  NavigatePrimaryPage(kPrerenderingUrl);
  // Regression test for https://crbug.com/1211274. Make sure that we don't
  // crash when activating a prerendered page which performed a fragment
  // navigation.
  ASSERT_EQ(1u, redirect_chain_observer.redirect_chain().size());
  EXPECT_EQ(kAnchorUrl, redirect_chain_observer.redirect_chain()[0]);

  // Make sure the render is not dead by doing a same page navigation.
  NavigatePrimaryPage(kAnchorUrl);

  // Make sure we did activate the page and issued no network requests
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
}

// Makes sure that activation on navigation for a pop-up window doesn't happen.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, Activation_PopUpWindow) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  EXPECT_TRUE(AddTestUtilJS(current_frame_host()));

  // Start a prerender.
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);

  // Attempt to activate the prerendered page for a pop-up window. This should
  // fail and fallback to network request.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ("LOADED", EvalJs(web_contents(),
                             JsReplace("open_window($1)", kPrerenderingUrl)));
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 2);

  // Activation shouldn't happen, so the prerender host should not be consumed.
  EXPECT_EQ(GetHostForUrl(kPrerenderingUrl), host_id);
}

// Makes sure that activation on navigation for a page that has a pop-up window
// doesn't happen.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, Activation_PageWithPopUpWindow) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  EXPECT_TRUE(AddTestUtilJS(current_frame_host()));

  // Start a prerender.
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender_next");
  AddPrerender(kPrerenderingUrl);
  ASSERT_TRUE(HasHostForUrl(kPrerenderingUrl));

  // Open a pop-up window.
  const GURL kWindowUrl = GetUrl("/empty.html?prerender_window");
  EXPECT_EQ("LOADED",
            EvalJs(web_contents(), JsReplace("open_window($1)", kWindowUrl)));

  // Attempt to activate the prerendered page for the top-level frame. This
  // should fail and fallback to network request because the pop-up window
  // exists.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 2);

  // The prerender host should be canceled.
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kActivatedWithAuxiliaryBrowsingContexts);
}

// This is the same as Activation_PageWithPopUpWindow test but `window.opener`
// will be nullified after it is open. The window loses the communication with
// the opener but it is still treated as an auxiliary context in the browser
// internal, so the activation should fail.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       Activation_PageWithPopUpWindow_OpenerIsNullified) {
  // Navigate to an initial page.
  const GURL initial_url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  EXPECT_TRUE(AddTestUtilJS(current_frame_host()));

  // Start a prerender.
  const GURL prerendering_url = GetUrl("/empty.html?prerender_next");
  AddPrerender(prerendering_url);
  ASSERT_TRUE(HasHostForUrl(prerendering_url));

  // Open a pop-up window that initially has an opener but it is nullified
  // right away.
  const GURL window_url = GetUrl("/empty.html?prerender_window");
  const std::string kOpenWindowAndNullifyScript = R"(
      const win = window.open($1, '_blank');
      win.opener = null;
  )";
  TestNavigationObserver nav_observer(window_url);
  nav_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace(kOpenWindowAndNullifyScript, window_url)));
  nav_observer.WaitForNavigationFinished();

  // Attempt to activate the prerendered page for the top-level frame. This
  // should fail and fallback to network request because the pop-up window
  // exists.
  ASSERT_EQ(GetRequestCount(prerendering_url), 1);
  NavigatePrimaryPage(prerendering_url);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), prerendering_url);
  EXPECT_EQ(GetRequestCount(prerendering_url), 2);

  // The prerender host should be canceled.
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kActivatedWithAuxiliaryBrowsingContexts);
}

// Tests that all RenderFrameHostImpls in the prerendering page know the
// prerendering state.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderIframe) {
  TestHostPrerenderingState(GetUrl("/page_with_iframe.html"));
}

// Blank <iframe> is a special case. Tests that the blank iframe knows the
// prerendering state as well.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderBlankIframe) {
  TestHostPrerenderingState(GetUrl("/page_with_blank_iframe.html"));
}

using PrerenderBrowserDeathTest = PrerenderBrowserTest;

// Tests that an inner WebContents cannot be attached in a prerendered page.
// See https://crbug.com/40191159 for details.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserDeathTest,
                       PrerenderCannotHaveInnerContents) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_blank_iframe.html");
  const GURL kInnerContentsUrl = GetUrl("/empty.html?prerender");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerendered_render_frame_host =
      GetPrerenderedMainFrameHost(host_id);

  EXPECT_CHECK_DEATH({
    CreateAndAttachInnerContents(
        prerendered_render_frame_host->child_at(0)->current_frame_host());
  });
}

// Ensure that whether or not a NavigationRequest is for a prerender activation
// is available in WebContentsObserver::DidStartNavigation.
class IsActivationObserver : public WebContentsObserver {
 public:
  IsActivationObserver(WebContents& web_contents, const GURL& url)
      : WebContentsObserver(&web_contents), url_(url) {}
  bool did_navigate() { return did_navigate_; }
  bool was_activation() { return was_activation_; }

 private:
  void DidStartNavigation(NavigationHandle* handle) override {
    if (handle->GetURL() != url_)
      return;
    did_navigate_ = true;
    was_activation_ = handle->IsPrerenderedPageActivation();
  }

  const GURL url_;
  bool did_navigate_ = false;
  bool was_activation_ = false;
};

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       NavigationRequestIsPrerenderedPageActivation) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  test::PrerenderHostObserver prerender_observer(*shell()->web_contents(),
                                                 kPrerenderingUrl);

  // Navigate to an initial page and start a prerender. Note, AddPrerender will
  // wait until the prerendered page has finished navigating.
  {
    ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
    ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);
    AddPrerender(kPrerenderingUrl);
  }

  IsActivationObserver is_activation_observer(*shell()->web_contents(),
                                              kPrerenderingUrl);

  // Now navigate the primary page to the prerendered URL so that we activate
  // the prerender.
  {
    ASSERT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                       JsReplace("location = $1", kPrerenderingUrl)));
    prerender_observer.WaitForActivation();
  }

  // Ensure that WebContentsObservers see the correct value for
  // IsPrerenderedPageActivation in DidStartNavigation.
  ASSERT_TRUE(is_activation_observer.did_navigate());
  EXPECT_TRUE(is_activation_observer.was_activation());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, ActivationDoesntRunThrottles) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  test::PrerenderHostObserver prerender_observer(*shell()->web_contents(),
                                                 kPrerenderingUrl);

  // Navigate to the initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));

  NavigationThrottle* throttle = nullptr;
  // This will attempt to insert a throttle that DEFERs the navigation at
  // WillStartRequest into all new navigations.
  ShellContentBrowserClient::Get()
      ->set_create_throttles_for_navigation_callback(base::BindLambdaForTesting(
          [&throttle](NavigationHandle* handle)
              -> std::vector<std::unique_ptr<NavigationThrottle>> {
            std::vector<std::unique_ptr<NavigationThrottle>> throttles;

            auto throttle_ptr =
                std::make_unique<TestNavigationThrottle>(handle);
            CHECK(!throttle);
            throttle = throttle_ptr.get();
            throttle_ptr->SetResponse(
                TestNavigationThrottle::WILL_START_REQUEST,
                TestNavigationThrottle::SYNCHRONOUS, NavigationThrottle::DEFER);

            throttles.push_back(std::move(throttle_ptr));
            return throttles;
          }));

  // Start a prerender and ensure that a NavigationThrottle can defer the
  // prerendering navigation. Then resume the navigation so the prerender
  // navigation and load completes.
  {
    TestNavigationManager prerender_manager(shell()->web_contents(),
                                            kPrerenderingUrl);
    AddPrerenderAsync(kPrerenderingUrl);
    ASSERT_TRUE(prerender_manager.WaitForFirstYieldAfterDidStartNavigation());
    ASSERT_NE(throttle, nullptr);

    auto* request =
        NavigationRequest::From(prerender_manager.GetNavigationHandle());
    ASSERT_TRUE(request->IsDeferredForTesting());
    EXPECT_EQ(request->GetDeferringThrottleForTesting(), throttle);
    throttle = nullptr;

    request->GetNavigationThrottleRunnerForTesting()->CallResumeForTesting();
    ASSERT_TRUE(prerender_manager.WaitForNavigationFinished());

    FrameTreeNodeId host_id = GetHostForUrl(kPrerenderingUrl);
    EXPECT_EQ(GetPrerenderedMainFrameHost(host_id)->GetLastCommittedURL(),
              kPrerenderingUrl);
  }

  // Now navigate the primary page to the prerendered URL so that we activate
  // the prerender. The throttle should not have been registered for the
  // activating navigation.
  {
    NavigatePrimaryPage(kPrerenderingUrl);
    prerender_observer.WaitForActivation();
    EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
    EXPECT_EQ(throttle, nullptr);
  }
}

// Ensures that if we attempt to open a URL while prerendering with a window
// disposition other than CURRENT_TAB, we fail.
IN_PROC_BROWSER_TEST_P(PrerenderTargetAgnosticBrowserTest, SuppressOpenURL) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender1");
  const GURL kSecondUrl = GetUrl("/empty.html?prerender2");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prerendering `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  FrameTreeNodeId host_id = prerender_helper()->AddPrerender(
      kPrerenderingUrl, /*eagerness=*/std::nullopt, GetTargetHint());
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
  RenderFrameHost* prerendered_render_frame_host =
      test::PrerenderTestHelper::GetPrerenderedMainFrameHost(
          *prerender_web_contents, host_id);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  OpenURLParams params(kSecondUrl, Referrer(),
                       prerendered_render_frame_host->GetFrameTreeNodeId(),
                       WindowOpenDisposition::NEW_WINDOW,
                       ui::PAGE_TRANSITION_LINK, true);
  params.initiator_origin =
      prerendered_render_frame_host->GetLastCommittedOrigin();
  params.source_render_process_id =
      prerendered_render_frame_host->GetProcess()->GetID();
  params.source_render_frame_id = prerendered_render_frame_host->GetRoutingID();
  auto* new_web_contents = prerender_web_contents->OpenURL(
      params, /*navigation_handle_callback=*/{});
  EXPECT_EQ(nullptr, new_web_contents);
}

// Tests that |RenderFrameHost::ForEachRenderFrameHost| and
// |WebContents::ForEachRenderFrameHost| behave correctly when prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, ForEachRenderFrameHost) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  // All frames are same-origin due to prerendering restrictions for
  // cross-origin.
  const GURL kPrerenderingUrl =
      GetUrl("/cross_site_iframe_factory.html?a.test(a.test(a.test),a.test)");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  RenderFrameHostImpl* initiator_render_frame_host = current_frame_host();

  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerendered_render_frame_host =
      GetPrerenderedMainFrameHost(host_id);
  RenderFrameHostImpl* rfh_sub_1 =
      prerendered_render_frame_host->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_sub_1_1 =
      rfh_sub_1->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_sub_2 =
      prerendered_render_frame_host->child_at(1)->current_frame_host();

  EXPECT_THAT(CollectAllRenderFrameHosts(prerendered_render_frame_host),
              testing::ElementsAre(prerendered_render_frame_host, rfh_sub_1,
                                   rfh_sub_2, rfh_sub_1_1));

  // When iterating over all RenderFrameHosts in a WebContents, we should see
  // the RFHs of both the primary page and the prerendered page.
  EXPECT_THAT(CollectAllRenderFrameHosts(web_contents_impl()),
              testing::UnorderedElementsAre(initiator_render_frame_host,
                                            prerendered_render_frame_host,
                                            rfh_sub_1, rfh_sub_2, rfh_sub_1_1));

  EXPECT_EQ(nullptr, initiator_render_frame_host->GetParentOrOuterDocument());
  EXPECT_EQ(nullptr, prerendered_render_frame_host->GetParentOrOuterDocument());
  EXPECT_EQ(prerendered_render_frame_host,
            rfh_sub_1->GetParentOrOuterDocument());
  EXPECT_EQ(rfh_sub_1, rfh_sub_1_1->GetParentOrOuterDocument());
  EXPECT_EQ(prerendered_render_frame_host,
            rfh_sub_2->GetParentOrOuterDocument());
  EXPECT_EQ(initiator_render_frame_host,
            initiator_render_frame_host->GetOutermostMainFrame());
  EXPECT_EQ(initiator_render_frame_host,
            initiator_render_frame_host->GetOutermostMainFrameOrEmbedder());
  // The outermost document of a prerendered page is the prerendered main
  // RenderFrameHost, not the primary main RenderFrameHost.
  EXPECT_EQ(prerendered_render_frame_host,
            prerendered_render_frame_host->GetOutermostMainFrame());
  EXPECT_EQ(prerendered_render_frame_host, rfh_sub_1->GetOutermostMainFrame());
  EXPECT_EQ(prerendered_render_frame_host,
            rfh_sub_1_1->GetOutermostMainFrame());
  EXPECT_EQ(prerendered_render_frame_host, rfh_sub_2->GetOutermostMainFrame());
  EXPECT_EQ(prerendered_render_frame_host,
            prerendered_render_frame_host->GetOutermostMainFrameOrEmbedder());
  EXPECT_EQ(prerendered_render_frame_host,
            rfh_sub_1->GetOutermostMainFrameOrEmbedder());
  EXPECT_EQ(prerendered_render_frame_host,
            rfh_sub_1_1->GetOutermostMainFrameOrEmbedder());
  EXPECT_EQ(prerendered_render_frame_host,
            rfh_sub_2->GetOutermostMainFrameOrEmbedder());

  // WebContentsImpl::ForEachFrameTree should include prerenders.
  bool visited_prerender_frame_tree = false;
  web_contents_impl()->ForEachFrameTree(
      base::BindLambdaForTesting([&](FrameTree& frame_tree) {
        if (&frame_tree == prerendered_render_frame_host->frame_tree()) {
          visited_prerender_frame_tree = true;
        }
      }));
  EXPECT_TRUE(visited_prerender_frame_tree);
}

// Tests that a prerendering page cannot change the visible URL of the
// corresponding WebContentsImpl instance before activation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, TabVisibleURL) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  ASSERT_EQ(shell()->web_contents()->GetVisibleURL(), kInitialUrl);
  AddPrerender(kPrerenderingUrl);

  // The visible URL should not be modified by the prerendering page.
  EXPECT_EQ(shell()->web_contents()->GetVisibleURL(), kInitialUrl);

  // Activate the prerendered page.
  NavigatePrimaryPage(kPrerenderingUrl);

  // The visible URL should be updated after activation.
  EXPECT_EQ(shell()->web_contents()->GetVisibleURL(), kPrerenderingUrl);
}

// Tests that prerendering will be cancelled if a prerendering page wants to set
// a WebContents-level preferred size.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, CancelOnPreferredSizeChanged) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);

  // Enable PreferredSize mode in the prerendering page. Usually this mode is
  // enabled by extentsions; here we enable it manually. Enabling this mode
  // makes renderers ask the browser to update WebContents-level preferred size,
  // which leads to the cancellation of prerendering.
  RenderFrameHostImpl* prerender_main_frame =
      GetPrerenderedMainFrameHost(host_id);
  prerender_main_frame->GetRenderViewHost()->EnablePreferredSizeMode();

  host_observer.WaitForDestroyed();
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kInactivePageRestriction);
  histogram_tester().ExpectUniqueSample(
      "Prerender.CanceledForInactivePageRestriction.DisallowActivationReason."
      "SpeculationRule",
      DisallowActivationReasonId::kContentsPreferredSizeChanged, 1);
}

// Tests that prerendering cannot request the browser to create a popup widget.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, NoPopupWidget) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/title1.html");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostWrapper prerender_main_frame(
      GetPrerenderedMainFrameHost(host_id));

  std::string create_element_script = R"(
    const widgetElement = document.createElement('input');
    widgetElement.type = 'color';
    widgetElement.id = 'chooser';
    widgetElement.value = '#000000';
    document.body.appendChild(widgetElement);
  )";

  EXPECT_TRUE(ExecJs(prerender_main_frame.get(), create_element_script,
                     EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));

  std::string click_element_script = R"(
    const element = document.getElementById('chooser');
    element.click();
  )";

  // It should be ignored because prerendering page do not have user gestures.
  EXPECT_TRUE(ExecJs(prerender_main_frame.get(), click_element_script));

  // Give the test a chance to fail if the click() is not ignored.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl));
}

// This throttle cancels prerendering on subframe navigation in prerendered
// pages. The subframe navigation itself will keep proceeding.
class TestPrerenderCancellerSubframeNavigationThrottle
    : public NavigationThrottle {
 public:
  explicit TestPrerenderCancellerSubframeNavigationThrottle(
      NavigationHandle* navigation_handle)
      : NavigationThrottle(navigation_handle),
        navigation_request_(NavigationRequest::From(navigation_handle)) {}

  ThrottleCheckResult WillStartRequest() override {
    // Cancel prerendering if this navigation is for subframes in prerendered
    // pages.
    FrameTreeNode* frame_tree_node = navigation_request_->frame_tree_node();
    if (frame_tree_node->frame_tree().is_prerendering() &&
        !frame_tree_node->IsMainFrame()) {
      PrerenderHostRegistry* prerender_host_registry =
          frame_tree_node->current_frame_host()
              ->delegate()
              ->GetPrerenderHostRegistry();
      prerender_host_registry->CancelHost(
          frame_tree_node->frame_tree().root()->frame_tree_node_id(),
          PrerenderFinalStatus::kMaxValue);
    }
    return PROCEED;
  }

  const char* GetNameForLogging() override {
    return "TestPrerenderCancellerSubframeNavigationThrottle";
  }

 private:
  raw_ptr<NavigationRequest> navigation_request_;
};

// Regression test for https://crbug.com/1323309.
// Tests that subframe navigation in prerendered pages starting while
// PrerenderHost is being destroyed should not crash.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       SubframeNavigationWhilePrerenderHostIsBeingDestroyed) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");
  const GURL kCrossOriginSubframeUrl =
      GetCrossSiteUrl("/empty.html?cross_origin_iframe");

  // Navigate to the initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Start a prerender.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  test::PrerenderHostObserver observer(*web_contents_impl(), host_id);

  // Insert TestPrerenderCancellerSubframeNavigationThrottle that cancels
  // prerendering on subframe navigation in a prerendered page. This should run
  // before PrerenderSubframeNavigationThrottle.
  ShellContentBrowserClient::Get()
      ->set_create_throttles_for_navigation_callback(base::BindLambdaForTesting(
          [](NavigationHandle* handle)
              -> std::vector<std::unique_ptr<NavigationThrottle>> {
            std::vector<std::unique_ptr<NavigationThrottle>> throttles;
            throttles.push_back(
                std::make_unique<
                    TestPrerenderCancellerSubframeNavigationThrottle>(handle));
            return throttles;
          }));

  // Use ExecuteScriptAsync instead of EvalJs as inserted cross-origin iframe
  // navigation should be canceled and script execution cannot wait for the
  // completion.
  RenderFrameHost* prerender_frame_host = GetPrerenderedMainFrameHost(host_id);
  EXPECT_TRUE(AddTestUtilJS(prerender_frame_host));
  ExecuteScriptAsync(prerender_frame_host, JsReplace("add_iframe_async($1)",
                                                     kCrossOriginSubframeUrl));

  // Wait for the cancellation triggered by the throttle. The subframe
  // navigation should not crash during the period.
  observer.WaitForDestroyed();
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kMaxValue);
}

class MojoCapabilityControlTestContentBrowserClient
    : public ContentBrowserTestContentBrowserClient,
      public test::MojoCapabilityControlTestHelper {
 public:
  MojoCapabilityControlTestContentBrowserClient() = default;
  ~MojoCapabilityControlTestContentBrowserClient() override = default;
  MojoCapabilityControlTestContentBrowserClient(
      const MojoCapabilityControlTestContentBrowserClient&) = delete;
  MojoCapabilityControlTestContentBrowserClient& operator=(
      const MojoCapabilityControlTestContentBrowserClient&) = delete;

  // ContentBrowserClient implementation.
  void RegisterBrowserInterfaceBindersForFrame(
      RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<RenderFrameHost*>* map) override {
    RegisterTestBrowserInterfaceBindersForFrame(render_frame_host, map);
  }
  void RegisterMojoBinderPoliciesForSameOriginPrerendering(
      MojoBinderPolicyMap& policy_map) override {
    RegisterTestMojoBinderPolicies(policy_map);
  }
};

// Tests that binding requests are handled according to MojoBinderPolicyMap
// during prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, MojoCapabilityControl) {
  MojoCapabilityControlTestContentBrowserClient test_browser_client;

  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_iframe.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start a prerender.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHost* prerendered_render_frame_host =
      GetPrerenderedMainFrameHost(host_id);
  std::vector<RenderFrameHost*> frames =
      CollectAllRenderFrameHosts(prerendered_render_frame_host);

  // A barrier closure to wait until a deferred interface is granted on all
  // frames.
  base::RunLoop run_loop;
  auto barrier_closure =
      base::BarrierClosure(frames.size(), run_loop.QuitClosure());

  mojo::RemoteSet<mojom::TestInterfaceForDefer> defer_remote_set;
  mojo::RemoteSet<mojom::TestInterfaceForGrant> grant_remote_set;
  for (auto* frame : frames) {
    auto* rfhi = static_cast<RenderFrameHostImpl*>(frame);
    EXPECT_TRUE(rfhi->frame_tree()->is_prerendering());
    EXPECT_EQ(rfhi->lifecycle_state(), LifecycleStateImpl::kPrerendering);
    EXPECT_EQ(rfhi->GetLifecycleState(),
              RenderFrameHost::LifecycleState::kPrerendering);

    mojo::Receiver<blink::mojom::BrowserInterfaceBroker>& bib =
        rfhi->browser_interface_broker_receiver_for_testing();
    blink::mojom::BrowserInterfaceBroker* prerender_broker =
        bib.internal_state()->impl();

    // Try to bind a kDefer interface.
    mojo::Remote<mojom::TestInterfaceForDefer> prerender_defer_remote;
    prerender_broker->GetInterface(
        prerender_defer_remote.BindNewPipeAndPassReceiver());
    // The barrier closure will be called after the deferred interface is
    // granted.
    prerender_defer_remote->Ping(barrier_closure);
    defer_remote_set.Add(std::move(prerender_defer_remote));

    // Try to bind a kGrant interface.
    mojo::Remote<mojom::TestInterfaceForGrant> prerender_grant_remote;
    prerender_broker->GetInterface(
        prerender_grant_remote.BindNewPipeAndPassReceiver());
    grant_remote_set.Add(std::move(prerender_grant_remote));
  }
  // Verify that BrowserInterfaceBrokerImpl defers running binders whose
  // policies are kDefer until the prerendered page is activated.
  EXPECT_EQ(test_browser_client.GetDeferReceiverSetSize(), 0U);
  // Verify that BrowserInterfaceBrokerImpl executes kGrant binders immediately.
  EXPECT_EQ(test_browser_client.GetGrantReceiverSetSize(), frames.size());

  // Activate the prerendered page.
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);

  // Wait until the deferred interface is granted on all frames.
  run_loop.Run();
  EXPECT_EQ(test_browser_client.GetDeferReceiverSetSize(), frames.size());
}

// Tests that mojo capability control will cancel prerendering if the main frame
// receives a request for a kCancel interface.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MojoCapabilityControl_CancelMainFrame) {
  MojoCapabilityControlTestContentBrowserClient test_browser_client;

  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_iframe.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start a prerender.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  auto* prerendered_render_frame_host = GetPrerenderedMainFrameHost(host_id);
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker>& bib =
      prerendered_render_frame_host
          ->browser_interface_broker_receiver_for_testing();
  blink::mojom::BrowserInterfaceBroker* prerender_broker =
      bib.internal_state()->impl();

  // Send a kCancel request to cancel prerendering.
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl));
  mojo::Remote<mojom::TestInterfaceForCancel> remote;
  prerender_broker->GetInterface(remote.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kMojoBinderPolicy);
  // `TestInterfaceForCancel` doesn't have a enum value because it is not used
  // in production, so histogram_tester_ should log
  // PrerenderCancelledInterface::kUnkown here.
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCancelledInterface.SpeculationRule",
      PrerenderCancelledInterface::kUnknown, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCancelledUnknownInterface."
      "SpeculationRule",
      InterfaceNameHasher(mojom::TestInterfaceForCancel::Name_), 1);
}

// Tests that mojo capability control will cancel prerendering if child frames
// receive a request for a kCancel interface.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MojoCapabilityControl_CancelIframe) {
  MojoCapabilityControlTestContentBrowserClient test_browser_client;

  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_iframe.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start a prerender.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  auto* main_render_frame_host = GetPrerenderedMainFrameHost(host_id);
  ASSERT_GE(main_render_frame_host->child_count(), 1U);
  RenderFrameHostImpl* child_render_frame_host =
      main_render_frame_host->child_at(0U)->current_frame_host();
  EXPECT_NE(main_render_frame_host->GetLastCommittedURL(),
            child_render_frame_host->GetLastCommittedURL());
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker>& bib =
      child_render_frame_host->browser_interface_broker_receiver_for_testing();
  blink::mojom::BrowserInterfaceBroker* prerender_broker =
      bib.internal_state()->impl();

  // Send a kCancel request to cancel prerendering.
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl));

  mojo::Remote<mojom::TestInterfaceForCancel> remote;
  prerender_broker->GetInterface(remote.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kMojoBinderPolicy);
  // `TestInterfaceForCancel` doesn't have a enum value because it is not used
  // in production, so histogram_tester_ should log
  // PrerenderCancelledInterface::kUnkown here.
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCancelledInterface.SpeculationRule",
      PrerenderCancelledInterface::kUnknown, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCancelledUnknownInterface."
      "SpeculationRule",
      InterfaceNameHasher(mojom::TestInterfaceForCancel::Name_), 1);
}

// Tests that mojo capability control will crash the prerender if the browser
// process receives a kUnexpected interface.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MojoCapabilityControl_HandleUnexpected) {
  MojoCapabilityControlTestContentBrowserClient test_browser_client;

  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender1");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Set up the error handler for bad mojo messages.
  std::string bad_message_error;
  mojo::SetDefaultProcessErrorHandler(
      base::BindLambdaForTesting([&](const std::string& error) {
        EXPECT_FALSE(error.empty());
        EXPECT_TRUE(bad_message_error.empty());
        bad_message_error = error;
      }));

  // Start a prerender.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  auto* main_render_frame_host = GetPrerenderedMainFrameHost(host_id);

  // Rebind a receiver for testing.
  // mojo::ReportBadMessage must be called within the stack frame derived from
  // mojo IPC calls, so this browser test should call the
  // remote<blink::mojom::BrowserInterfaceBroker>::GetInterface() to test
  // unexpected interfaces. But its remote end is in renderer processes and
  // inaccessible, so the test code has to create another BrowserInterfaceBroker
  // pipe and rebind the receiver end so as to send the request from the remote.
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker>& bib =
      main_render_frame_host->browser_interface_broker_receiver_for_testing();
  auto broker_receiver_of_previous_document = bib.Unbind();
  ASSERT_TRUE(broker_receiver_of_previous_document);
  mojo::Remote<blink::mojom::BrowserInterfaceBroker> remote_broker;
  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> fake_receiver =
      remote_broker.BindNewPipeAndPassReceiver();
  main_render_frame_host->BindBrowserInterfaceBrokerReceiver(
      std::move(fake_receiver));

  // Send a kUnexpected request.
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl));
  mojo::Remote<mojom::TestInterfaceForUnexpected> remote;
  remote_broker->GetInterface(remote.BindNewPipeAndPassReceiver());
  remote_broker.FlushForTesting();
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  EXPECT_EQ(bad_message_error,
            "MBPA_BAD_INTERFACE: content.mojom.TestInterfaceForUnexpected");
}

// Regression test for https://crbug.com/1268714 and https://crbug.com/1424250.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, MojoCapabilityControl_LoosenMode) {
  MojoCapabilityControlTestContentBrowserClient test_browser_client;

  // Some Android bots run with the site isolation disabled and behave
  // differently on cross-origin iframe creation in a prerendered page. More
  // specifically, when the site isolation is disabled, cross-site iframe will
  // not create a speculative RenderFrameHost, and it results in test failures.
  // To avoid it, this test explicitly runs with the site isolation enabled.
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  GURL initial_url = GetUrl("/empty.html");
  GURL prerendering_url =
      GetUrl("/cross_site_iframe_factory.html?a.test(a.test,a.test)");
  GURL cross_origin_iframe_url = GetCrossSiteUrl("/title1.html");

  // 1. Navigate to an initial page and prerender a page.
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  FrameTreeNodeId host_id = AddPrerender(prerendering_url);
  RenderFrameHostImpl* prerendered_render_frame_host =
      GetPrerenderedMainFrameHost(host_id);

  // 2. Let the first iframe navigate to a cross-origin url. It will create a
  // speculative RFH and the navigation will be deferred.
  TestNavigationManager subframe_navigation_manager(web_contents(),
                                                    cross_origin_iframe_url);
  std::string js = R"(
    const frame = document.getElementById($1);
    frame.contentWindow.location.href = $2;
  )";
  EXPECT_TRUE(ExecJs(prerendered_render_frame_host,
                     JsReplace(js, "child-0", cross_origin_iframe_url.spec())));

  // 3. Wait until the navigation to `cross_origin_iframe_url` is deferred by
  // NavigationThrottle.
  ASSERT_TRUE(
      subframe_navigation_manager.WaitForFirstYieldAfterDidStartNavigation());
  FrameTreeNode* child_ftn =
      FrameTreeNode::GloballyFindByID(host_id)->child_at(0);
  NavigationRequest* child_navigation = child_ftn->navigation_request();
  ASSERT_NE(child_navigation, nullptr);
  ASSERT_TRUE(child_navigation->IsDeferredForTesting());

  // 4. Collect all RenderFrameHosts in the frame tree.
  std::vector<RenderFrameHostImpl*> all_prerender_frames;
  size_t count_speculative = 0;
  prerendered_render_frame_host->ForEachRenderFrameHostIncludingSpeculative(
      [&](RenderFrameHostImpl* rfh) {
        all_prerender_frames.push_back(rfh);
        count_speculative +=
            (rfh->lifecycle_state() == LifecycleStateImpl::kSpeculative);
      });
  // With feature DeferSpeculativeRFHCreation, the speculative RFH won't be
  // created when the navigation starts.
  if (base::FeatureList::IsEnabled(features::kDeferSpeculativeRFHCreation)) {
    ASSERT_EQ(all_prerender_frames.size(), 3u);
    ASSERT_EQ(count_speculative, 0u);
  } else {
    ASSERT_EQ(all_prerender_frames.size(), 4u);
    ASSERT_EQ(count_speculative, 1u);
  }

  // 5. Renderers attempt to build Mojo connections for kDefer and kGrant
  // interfaces during prerendering. This part simulates them.

  // A barrier closure to wait until a deferred interface is granted on all
  // frames.
  base::RunLoop run_loop;
  auto barrier_closure =
      base::BarrierClosure(all_prerender_frames.size(), run_loop.QuitClosure());

  // Iterate all the frames to bind interfaces.
  mojo::RemoteSet<mojom::TestInterfaceForDefer> defer_remote_set;
  mojo::RemoteSet<mojom::TestInterfaceForGrant> grant_remote_set;
  for (auto* rfhi : all_prerender_frames) {
    mojo::Receiver<blink::mojom::BrowserInterfaceBroker>& bib =
        rfhi->browser_interface_broker_receiver_for_testing();
    blink::mojom::BrowserInterfaceBroker* prerender_broker =
        bib.internal_state()->impl();

    // Try to bind a kDefer interface.
    mojo::Remote<mojom::TestInterfaceForDefer> prerender_defer_remote;
    prerender_broker->GetInterface(
        prerender_defer_remote.BindNewPipeAndPassReceiver());
    // The barrier closure will be called after the deferred interface is
    // granted.
    prerender_defer_remote->Ping(barrier_closure);
    defer_remote_set.Add(std::move(prerender_defer_remote));

    // Try to bind a kGrant interface.
    mojo::Remote<mojom::TestInterfaceForGrant> prerender_grant_remote;
    prerender_broker->GetInterface(
        prerender_grant_remote.BindNewPipeAndPassReceiver());
    grant_remote_set.Add(std::move(prerender_grant_remote));
  }

  // Verify that BrowserInterfaceBrokerImpl defers running binders whose
  // policies are kDefer until the prerendered page is activated.
  EXPECT_EQ(test_browser_client.GetDeferReceiverSetSize(), 0U);
  // Verify that BrowserInterfaceBrokerImpl executes kGrant binders immediately.
  EXPECT_EQ(test_browser_client.GetGrantReceiverSetSize(),
            all_prerender_frames.size());

  // 6. Activate the prerendered page and listen to the DidFinishNavigation
  // event, to ensure the Activate IPC is sent.
  TestActivationManager prerendered_activation_navigation(web_contents(),
                                                          prerendering_url);
  ASSERT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                     JsReplace("location = $1", prerendering_url)));
  prerendered_activation_navigation.WaitForNavigationFinished();
  EXPECT_TRUE(prerendered_activation_navigation.was_activated());

  // Make sure all the deferred interfaces are granted after activation. This is
  // a regression test for https://crbug.com/1424250.
  run_loop.Run();
  EXPECT_EQ(test_browser_client.GetDeferReceiverSetSize(),
            all_prerender_frames.size());

  // 7. Renderers attempt to build Mojo connections for kCancel interfaces.
  // This part simulates some subframe documents start sending kCancel
  // interfaces after they know about the activation. It tests the regression
  // situation caught by https://crbug.com/1268714. If some RenderFrameHostImpls
  // are not informed of the activation, this test will crash.
  for (auto* rfhi : all_prerender_frames) {
    mojo::Receiver<blink::mojom::BrowserInterfaceBroker>& bib =
        rfhi->browser_interface_broker_receiver_for_testing();
    blink::mojom::BrowserInterfaceBroker* prerender_broker =
        bib.internal_state()->impl();

    // Send a kCancel request to the browser. This test should not crash.
    mojo::Remote<mojom::TestInterfaceForCancel> remote;
    prerender_broker->GetInterface(remote.BindNewPipeAndPassReceiver());
    remote.FlushForTesting();
  }
}

// Test that prerenders triggered by speculation rules are canceled when a
// background timeout timer is fired.
void PrerenderBrowserTest::TestCancelPrerendersWhenTimeout(
    std::vector<Visibility> visibility_transitions) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderUrl1 = GetUrl("/empty.html?prerender1");
  const GURL kPrerenderUrl2 = GetUrl("/empty.html?prerender2");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  AddPrerender(kPrerenderUrl1);
  AddPrerender(kPrerenderUrl2);

  test::PrerenderHostObserver prerender_observer(*web_contents_impl(),
                                                 kPrerenderUrl1);

  PrerenderHostRegistry* registry =
      web_contents_impl()->GetPrerenderHostRegistry();

  // The timers should not start yet when the prerendered page is in the
  // foreground.
  ASSERT_FALSE(registry->GetEmbedderTimerForTesting()->IsRunning());
  ASSERT_FALSE(registry->GetSpeculationRulesTimerForTesting()->IsRunning());

  // Inject mock time task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  registry->SetTaskRunnerForTesting(task_runner);

  // Changing the visibility state starts/stops the timeout timer.
  for (Visibility visibility : visibility_transitions) {
    switch (visibility) {
      case Visibility::HIDDEN:
        web_contents()->WasHidden();
        ASSERT_TRUE(registry->GetEmbedderTimerForTesting()->IsRunning());
        ASSERT_TRUE(
            registry->GetSpeculationRulesTimerForTesting()->IsRunning());
        break;
      case Visibility::OCCLUDED:
        web_contents()->WasOccluded();
        ASSERT_TRUE(registry->GetEmbedderTimerForTesting()->IsRunning());
        ASSERT_TRUE(
            registry->GetSpeculationRulesTimerForTesting()->IsRunning());
        break;
      case Visibility::VISIBLE:
        web_contents()->WasShown();
        ASSERT_FALSE(registry->GetEmbedderTimerForTesting()->IsRunning());
        ASSERT_FALSE(
            registry->GetSpeculationRulesTimerForTesting()->IsRunning());
        break;
    }
  }

  // The remaining part of this test assumes the timers are running.
  ASSERT_TRUE(registry->GetEmbedderTimerForTesting()->IsRunning());
  ASSERT_TRUE(registry->GetSpeculationRulesTimerForTesting()->IsRunning());

  // Expire the timers.
  task_runner->FastForwardBy(
      PrerenderHostRegistry::kTimeToLiveInBackgroundForSpeculationRules);
  ASSERT_FALSE(registry->GetEmbedderTimerForTesting()->IsRunning());
  ASSERT_FALSE(registry->GetSpeculationRulesTimerForTesting()->IsRunning());

  // The timers should cancel prerendering.
  prerender_observer.WaitForDestroyed();
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kTimeoutBackgrounded, 2);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       CancelPrerendersWhenTimeout_Hidden) {
  // The timeout timers should start on the hidden state.
  TestCancelPrerendersWhenTimeout({Visibility::HIDDEN});
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       CancelPrerendersWhenTimeout_Occluded) {
  // The timeout timers should start on the occluded state.
  TestCancelPrerendersWhenTimeout({Visibility::OCCLUDED});
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       CancelPrerendersWhenTimeout_OccludedHidden) {
  // The timeout timers should start on the occluded state and then keep running
  // on the hidden state.
  TestCancelPrerendersWhenTimeout({Visibility::OCCLUDED, Visibility::HIDDEN});
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       CancelPrerendersWhenTimeout_OccludedVisibleHidden) {
  // The timeout timers should start on the occluded state, stop on the visible
  // state, and then restart on the hidden state.
  TestCancelPrerendersWhenTimeout(
      {Visibility::OCCLUDED, Visibility::VISIBLE, Visibility::HIDDEN});
}

// Test that a PrerenderHost triggered by embedder is canceled when it times out
// in the background.
void PrerenderBrowserTest::TestCancelOnlyEmbedderTriggeredPrerenderWhenTimeout(
    std::vector<Visibility> visibility_transitions) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderUrl1 = GetUrl("/empty.html?prerender1");
  const GURL kPrerenderUrl2 = GetUrl("/empty.html?prerender2");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering by speculation rules.
  AddPrerender(kPrerenderUrl1);

  test::PrerenderHostObserver host_observer(*web_contents_impl(),
                                            kPrerenderUrl2);
  // Start prerendering by embedder.
  std::unique_ptr<PrerenderHandle> prerender_handle =
      AddEmbedderTriggeredPrerenderAsync(kPrerenderUrl2);

  PrerenderHostRegistry* registry =
      web_contents_impl()->GetPrerenderHostRegistry();

  // The timers should not start yet when the prerendered page is in the
  // foreground.
  ASSERT_FALSE(registry->GetEmbedderTimerForTesting()->IsRunning());
  ASSERT_FALSE(registry->GetSpeculationRulesTimerForTesting()->IsRunning());

  // Inject mock time task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  registry->SetTaskRunnerForTesting(task_runner);

  // Changing the visibility state starts/stops the timeout timer.
  for (Visibility visibility : visibility_transitions) {
    switch (visibility) {
      case Visibility::HIDDEN:
        web_contents()->WasHidden();
        ASSERT_TRUE(registry->GetEmbedderTimerForTesting()->IsRunning());
        ASSERT_TRUE(
            registry->GetSpeculationRulesTimerForTesting()->IsRunning());
        break;
      case Visibility::OCCLUDED:
        web_contents()->WasOccluded();
        ASSERT_TRUE(registry->GetEmbedderTimerForTesting()->IsRunning());
        ASSERT_TRUE(
            registry->GetSpeculationRulesTimerForTesting()->IsRunning());
        break;
      case Visibility::VISIBLE:
        web_contents()->WasShown();
        ASSERT_FALSE(registry->GetEmbedderTimerForTesting()->IsRunning());
        ASSERT_FALSE(
            registry->GetSpeculationRulesTimerForTesting()->IsRunning());
        break;
    }
  }

  // The remaining part of this test assumes the timers are running.
  ASSERT_TRUE(registry->GetEmbedderTimerForTesting()->IsRunning());
  ASSERT_TRUE(registry->GetSpeculationRulesTimerForTesting()->IsRunning());

  // PrerenderHost triggered by embedder should be destroyed and PrerenderHost
  // triggered by speculation rules should be alive, since the timeout value
  // differs depending on the trigger type.
  ASSERT_GT(PrerenderHostRegistry::kTimeToLiveInBackgroundForSpeculationRules,
            PrerenderHostRegistry::kTimeToLiveInBackgroundForEmbedder);
  task_runner->FastForwardBy(
      PrerenderHostRegistry::kTimeToLiveInBackgroundForEmbedder);

  host_observer.WaitForDestroyed();

  // The timer for speculation rules is still running and PrerenderHost for
  // speculation rules is alive.
  ASSERT_FALSE(registry->GetEmbedderTimerForTesting()->IsRunning());
  ASSERT_TRUE(registry->GetSpeculationRulesTimerForTesting()->IsRunning());
  EXPECT_TRUE(GetHostForUrl(kPrerenderUrl1));

  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kTimeoutBackgrounded, 1);
  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kTimeoutBackgrounded, 0);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       CancelOnlyEmbedderTriggeredPrerenderWhenTimeout_Hidden) {
  // The timeout timers should start on the hidden state.
  TestCancelOnlyEmbedderTriggeredPrerenderWhenTimeout({Visibility::HIDDEN});
}

IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    CancelOnlyEmbedderTriggeredPrerenderWhenTimeout_Occluded) {
  // The timeout timers should start on the occluded state.
  TestCancelOnlyEmbedderTriggeredPrerenderWhenTimeout({Visibility::OCCLUDED});
}

IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    CancelOnlyEmbedderTriggeredPrerenderWhenTimeout_OccludedHidden) {
  // The timeout timers should start on the occluded state and then keep running
  // on the hidden state.
  TestCancelOnlyEmbedderTriggeredPrerenderWhenTimeout(
      {Visibility::OCCLUDED, Visibility::HIDDEN});
}

IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    CancelOnlyEmbedderTriggeredPrerenderWhenTimeout_OccludedVisibleHidden) {
  // The timeout timers should start on the occluded state, stop on the visible
  // state, and then restart on the hidden state.
  TestCancelOnlyEmbedderTriggeredPrerenderWhenTimeout(
      {Visibility::OCCLUDED, Visibility::VISIBLE, Visibility::HIDDEN});
}

// Test that the timers for PrerenderHost timeout is reset when the
// hidden/occluded page gets visible.
void PrerenderBrowserTest::TestTimerResetWhenPageGoBackToForeground(
    Visibility visibility) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderUrl = GetUrl("/empty.html?prerender");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  AddPrerender(kPrerenderUrl);

  PrerenderHostRegistry* registry =
      web_contents_impl()->GetPrerenderHostRegistry();

  // The timers should not start yet when the prerendered page is in the
  // foreground.
  ASSERT_FALSE(registry->GetEmbedderTimerForTesting()->IsRunning());
  ASSERT_FALSE(registry->GetSpeculationRulesTimerForTesting()->IsRunning());

  // Changing the visibility state to HIDDEN/OCCLUDED will not stop prerendering
  // immediately, but start the timers.
  switch (visibility) {
    case Visibility::HIDDEN:
      web_contents()->WasHidden();
      break;
    case Visibility::OCCLUDED:
      web_contents()->WasOccluded();
      break;
    case Visibility::VISIBLE:
      ASSERT_TRUE(false);
      break;
  }

  // The remaining part of this test assumes the timers are running.
  ASSERT_TRUE(registry->GetEmbedderTimerForTesting()->IsRunning());
  ASSERT_TRUE(registry->GetSpeculationRulesTimerForTesting()->IsRunning());

  // The timers should be reset when the HIDDEN/OCCLUDED page goes back to the
  // foreground.
  web_contents()->WasShown();
  ASSERT_FALSE(registry->GetEmbedderTimerForTesting()->IsRunning());
  ASSERT_FALSE(registry->GetSpeculationRulesTimerForTesting()->IsRunning());

  // Activate the prerendered page.
  test::PrerenderHostObserver prerender_observer(*web_contents(),
                                                 GetHostForUrl(kPrerenderUrl));
  NavigatePrimaryPage(kPrerenderUrl);
  prerender_observer.WaitForActivation();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderUrl);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kActivated, 1);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       TimerResetWhenPageGoBackToForeground_Hidden) {
  TestTimerResetWhenPageGoBackToForeground(Visibility::HIDDEN);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       TimerResetWhenPageGoBackToForeground_Occluded) {
  TestTimerResetWhenPageGoBackToForeground(Visibility::OCCLUDED);
}

// Test that a PrerenderHost in a triggered by speculation rules with
// "target=_blank" are canceled when it times out in the background .
void PrerenderBrowserTest::TestCancelPrerenderWithTargetBlankWhenTimeout(
    Visibility visibility) {
  const GURL kInitialUrl = GetUrl("/simple_links.html");
  const GURL kPrerenderUrl = GetUrl("/title2.html");

  // Navigate to an initial page which has a link to `kPrerenderUrl`.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderUrl`.
  FrameTreeNodeId host_id = prerender_helper()->AddPrerender(
      kPrerenderUrl, /*eagerness=*/std::nullopt, "_blank");
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
  ASSERT_NE(prerender_web_contents, web_contents_impl());
  ExpectWebContentsIsForNewTabPrerendering(*prerender_web_contents);

  PrerenderHostRegistry* registry =
      web_contents_impl()->GetPrerenderHostRegistry();

  // The timers should not start yet when the prerendered page is in the
  // foreground.
  ASSERT_FALSE(registry->GetEmbedderTimerForTesting()->IsRunning());
  ASSERT_FALSE(registry->GetSpeculationRulesTimerForTesting()->IsRunning());

  // Inject mock time task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  registry->SetTaskRunnerForTesting(task_runner);

  test::PrerenderHostObserver prerender_observer(*prerender_web_contents,
                                                 host_id);

  // Changing the visibility state to HIDDEN/OCCLUDED will not stop prerendering
  // immediately, but start the timers.
  switch (visibility) {
    case Visibility::HIDDEN:
      web_contents()->WasHidden();
      break;
    case Visibility::OCCLUDED:
      web_contents()->WasOccluded();
      break;
    case Visibility::VISIBLE:
      ASSERT_TRUE(false);
      break;
  }

  // The remaining part of this test assumes the timers are running.
  ASSERT_TRUE(registry->GetEmbedderTimerForTesting()->IsRunning());
  ASSERT_TRUE(registry->GetSpeculationRulesTimerForTesting()->IsRunning());

  // Expire the timers.
  task_runner->FastForwardBy(
      PrerenderHostRegistry::kTimeToLiveInBackgroundForSpeculationRules);
  ASSERT_FALSE(registry->GetEmbedderTimerForTesting()->IsRunning());
  ASSERT_FALSE(registry->GetSpeculationRulesTimerForTesting()->IsRunning());

  prerender_observer.WaitForDestroyed();
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kTimeoutBackgrounded, 1);

  // The navigation occurred in a new WebContents, so the original WebContents
  // should still be showing the initial trigger page.
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       CancelPrerenderWithTargetBlankWhenTimeout_Hidden) {
  TestCancelPrerenderWithTargetBlankWhenTimeout(Visibility::HIDDEN);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       CancelPrerenderWithTargetBlankWhenTimeout_Occluded) {
  TestCancelPrerenderWithTargetBlankWhenTimeout(Visibility::OCCLUDED);
}

enum class SSLPrerenderTestErrorBlockType { kClientCertRequested, kCertError };

std::string SSLPrerenderTestErrorBlockTypeToString(
    const testing::TestParamInfo<SSLPrerenderTestErrorBlockType>& info) {
  switch (info.param) {
    case SSLPrerenderTestErrorBlockType::kClientCertRequested:
      return "ClientCertRequested";
    case SSLPrerenderTestErrorBlockType::kCertError:
      return "CertError";
  }
}

class SSLPrerenderBrowserTest
    : public testing::WithParamInterface<SSLPrerenderTestErrorBlockType>,
      public PrerenderBrowserTest {
 protected:
  void RequireClientCertsOrSendExpiredCerts() {
    net::SSLServerConfig ssl_config;
    switch (GetParam()) {
      case SSLPrerenderTestErrorBlockType::kClientCertRequested:
        ssl_config.client_cert_type =
            net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
        ResetSSLConfig(net::test_server::EmbeddedTestServer::CERT_TEST_NAMES,
                       ssl_config);
        break;
      case SSLPrerenderTestErrorBlockType::kCertError:
        ResetSSLConfig(net::test_server::EmbeddedTestServer::CERT_EXPIRED,
                       ssl_config);
        break;
    }
  }
  PrerenderFinalStatus GetExpectedFinalStatus() {
    switch (GetParam()) {
      case SSLPrerenderTestErrorBlockType::kClientCertRequested:
        return PrerenderFinalStatus::kClientCertRequested;
      case SSLPrerenderTestErrorBlockType::kCertError:
        return PrerenderFinalStatus::kSslCertificateError;
    }
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SSLPrerenderBrowserTest,
    testing::Values(SSLPrerenderTestErrorBlockType::kClientCertRequested,
                    SSLPrerenderTestErrorBlockType::kCertError),
    SSLPrerenderTestErrorBlockTypeToString);

// For a prerendering navigation request, if the server requires a client
// certificate or responds to the request with an invalid certificate, the
// prernedering should be canceled.
IN_PROC_BROWSER_TEST_P(SSLPrerenderBrowserTest,
                       CertificateValidation_Navigation) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Reset the server's config.
  RequireClientCertsOrSendExpiredCerts();

  const GURL kPrerenderingUrl = GetUrl("/title1.html");

  // Start prerendering `kPrerenderingUrl`.
  test::PrerenderHostObserver host_observer(*web_contents(), kPrerenderingUrl);
  prerender_helper()->AddPrerenderAsync(kPrerenderingUrl);

  // The prerender should be destroyed.
  host_observer.WaitForDestroyed();
  EXPECT_TRUE(prerender_helper()->GetHostForUrl(kPrerenderingUrl).is_null());
  ExpectFinalStatusForSpeculationRule(GetExpectedFinalStatus());
}

// For a prerendering subresource request, if the server requires a client
// certificate or responds to the request with an invalid certificate, the
// prernedering should be canceled.
IN_PROC_BROWSER_TEST_P(SSLPrerenderBrowserTest,
                       CertificateValidation_Subresource) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl`.
  const GURL kPrerenderingUrl = GetUrl("/title1.html");
  FrameTreeNodeId host_id = prerender_helper()->AddPrerender(kPrerenderingUrl);
  test::PrerenderHostObserver host_observer(*web_contents(), host_id);

  // Reset the server's config.
  RequireClientCertsOrSendExpiredCerts();

  ASSERT_TRUE(prerender_helper()->GetHostForUrl(kPrerenderingUrl));

  // Fetch a subresrouce.
  std::string fetch_subresource_script = R"(
        const imgElement = document.createElement('img');
        imgElement.src = '/load_image/image.png';
        document.body.appendChild(imgElement);
  )";
  std::ignore = ExecJs(prerender_helper()->GetPrerenderedMainFrameHost(host_id),
                       fetch_subresource_script);

  // The prerender should be destroyed.
  host_observer.WaitForDestroyed();
  EXPECT_TRUE(prerender_helper()->GetHostForUrl(kPrerenderingUrl).is_null());
  ExpectFinalStatusForSpeculationRule(GetExpectedFinalStatus());
}

// Tests that prerendering will be cancelled if the server asks for client
// certificates or responds with an expired certificate, even if the main
// resource request is intercepted and sent by a service worker.
IN_PROC_BROWSER_TEST_P(SSLPrerenderBrowserTest,
                       CertificateValidation_SWMainResource) {
  // Register a service worker that intercepts resource requests.
  const GURL kInitialUrl = GetUrl("/workers/service_worker_setup.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  EXPECT_EQ("ok", EvalJs(web_contents(), "setup();"));

  // Reset the server's config.
  RequireClientCertsOrSendExpiredCerts();

  const GURL kPrerenderingUrl = GetUrl("/workers/simple.html?intercept");
  test::PrerenderHostObserver host_observer(*web_contents(), kPrerenderingUrl);
  prerender_helper()->AddPrerenderAsync(kPrerenderingUrl);

  // The prerender should be destroyed.
  host_observer.WaitForDestroyed();
  EXPECT_TRUE(prerender_helper()->GetHostForUrl(kPrerenderingUrl).is_null());

  // For the kCertError case, StoragePartitionImpl cannot locate any
  // WebContents. So, the certificate error does not cause any UI changes; it
  // just cancels the url request, and leads to the cancellation of
  // prerendering with kNavigationRequestNetworkError.
  ExpectFinalStatusForSpeculationRule(
      GetParam() == SSLPrerenderTestErrorBlockType::kClientCertRequested
          ? PrerenderFinalStatus::kClientCertRequested
          : PrerenderFinalStatus::kNavigationRequestNetworkError);
}

// Tests that prerendering will be cancelled if the server asks for client
// certificates or responds with an expired certificate, even if the subresource
// request is intercepted by a service worker.
IN_PROC_BROWSER_TEST_P(SSLPrerenderBrowserTest,
                       CertificateValidation_SWSubResource) {
  // Skip the test when the block type is kCertError. With the type, this test
  // times out due to https://crbug.com/1311887.
  // TODO(crbug.com/40220378): Enable the test with kCertError.
  if (GetParam() == SSLPrerenderTestErrorBlockType::kCertError)
    return;

  // Load an initial page and register a service worker that intercepts
  // resources requests.
  const GURL kInitialUrl = GetUrl("/workers/service_worker_setup.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  EXPECT_EQ("ok", EvalJs(current_frame_host(), "setup();"));

  // Prerender a page.
  const GURL kPrerenderingUrl = GetUrl("/workers/empty.html");
  FrameTreeNodeId host_id = prerender_helper()->AddPrerender(kPrerenderingUrl);
  test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  RequireClientCertsOrSendExpiredCerts();

  // Try to fetch a sub resource through the registered service worker. The
  // server should ask for a client certificate or respond with an expired
  // certificate, which leads to the cancellation of prerendering.
  std::string resource_url = GetUrl("/workers/empty.js?intercept").spec();
  std::ignore = ExecJs(prerender_helper()->GetPrerenderedMainFrameHost(host_id),
                       JsReplace("fetch($1);", resource_url));

  // Check the prerender was destroyed.
  host_observer.WaitForDestroyed();
  EXPECT_TRUE(prerender_helper()->GetHostForUrl(kPrerenderingUrl).is_null());
  ExpectFinalStatusForSpeculationRule(GetExpectedFinalStatus());
}

// Tests for feature restrictions in prerendered pages =========================

// Tests that window.open() in a prerendering page fails.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, FeatureRestriction_WindowOpen) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerendering");
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  auto* prerender_frame = GetPrerenderedMainFrameHost(host_id);
  EXPECT_TRUE(AddTestUtilJS(prerender_frame));

  // Attempt to open a window in the prerendered page. This should fail.
  const GURL kWindowOpenUrl = GetUrl("/empty.html?prerender");

  EXPECT_EQ("FAILED", EvalJs(prerender_frame,
                             JsReplace("open_window($1)", kWindowOpenUrl)));
  EXPECT_EQ(GetRequestCount(kWindowOpenUrl), 0);

  // Opening a window shouldn't cancel prerendering.
  EXPECT_EQ(GetHostForUrl(kPrerenderingUrl), host_id);
}

IN_PROC_BROWSER_TEST_P(PrerenderTargetAgnosticBrowserTest,
                       RenderFrameHostLifecycleState) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  EXPECT_EQ(current_frame_host()->lifecycle_state(),
            LifecycleStateImpl::kActive);

  // Start a prerender.
  FrameTreeNodeId host_id = prerender_helper()->AddPrerender(
      kPrerenderingUrl, /*eagerness=*/std::nullopt, GetTargetHint());
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);

  // Open an iframe in the prerendered page.
  auto* rfh_a = static_cast<RenderFrameHostImpl*>(
      test::PrerenderTestHelper::GetPrerenderedMainFrameHost(
          *prerender_web_contents, host_id));
  EXPECT_TRUE(AddTestUtilJS(rfh_a));
  EXPECT_EQ("LOADED",
            EvalJs(rfh_a, JsReplace("add_iframe($1)",
                                    GetUrl("/empty.html?prerender"))));
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();

  // Both rfh_a and rfh_b lifecycle state's should be kPrerendering.
  EXPECT_EQ(LifecycleStateImpl::kPrerendering, rfh_a->lifecycle_state());
  EXPECT_EQ(LifecycleStateImpl::kPrerendering, rfh_b->lifecycle_state());
  EXPECT_FALSE(rfh_a->IsInPrimaryMainFrame());
  EXPECT_FALSE(rfh_b->IsInPrimaryMainFrame());

  // Activate the prerendered page.
  ActivatePrerenderedPage(*prerender_web_contents, kPrerenderingUrl);

  // Both rfh_a and rfh_b lifecycle state's should be kActive after activation.
  EXPECT_EQ(LifecycleStateImpl::kActive, rfh_a->lifecycle_state());
  EXPECT_EQ(LifecycleStateImpl::kActive, rfh_b->lifecycle_state());
  EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());
  EXPECT_FALSE(rfh_b->IsInPrimaryMainFrame());

  // "Navigation.TimeToActivatePrerender.SpeculationRule" histogram should be
  // recorded on every prerender activation.
  histogram_tester().ExpectTotalCount(
      "Navigation.TimeToActivatePrerender.SpeculationRule", 1u);
}

// Test that prerender activation is deferred and resumed after the ongoing
// (in-flight) main-frame navigation in the prerendering frame tree commits.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       SupportActivationWithOngoingMainFrameNavigation) {
  // Create a HTTP response to control prerendering main-frame navigation.
  net::test_server::ControllableHttpResponse main_document_response(
      embedded_test_server(), "/main_document");

  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerenderingUrl =
      embedded_test_server()->GetURL("/main_document");

  // Navigate to an initial page in primary frame tree.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender, and navigate to a page that doesn't commit navigation.
  {
    test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());
    AddPrerenderAsync(kPrerenderingUrl);
    registry_observer.WaitForTrigger(kPrerenderingUrl);
    EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl));
  }

  FrameTreeNodeId host_id = GetHostForUrl(kPrerenderingUrl);
  test::PrerenderHostObserver prerender_observer(*web_contents_impl(), host_id);
  EXPECT_FALSE(prerender_observer.was_activated());

  // Defer the activation until the ongoing main-frame navigation in prerender
  // frame tree commits.
  {
    // Start navigation in primary page to kPrerenderingUrl.
    TestActivationManager primary_page_manager(shell()->web_contents(),
                                               kPrerenderingUrl);
    ASSERT_TRUE(ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
                       JsReplace("location = $1", kPrerenderingUrl)));

    NavigationRequest* request =
        web_contents_impl()->GetPrimaryFrameTree().root()->navigation_request();

    // Wait until the navigation is deferred by CommitDeferringCondition.
    ASSERT_TRUE(primary_page_manager.WaitForBeforeChecks());
    primary_page_manager.ResumeActivation();

    // TODO(bokan): This could be any CommitDeferringCondition, we should have
    // a way to pause on a specific CommitDeferringCondition.
    EXPECT_TRUE(request->IsCommitDeferringConditionDeferredForTesting());

    // The navigation should not have proceeded past NOT_STARTED because the
    // PrerenderCommitDeferringCondition is deferring it.
    EXPECT_EQ(request->state(), NavigationRequest::NOT_STARTED);

    // Complete the prerender response and finish ongoing prerender main frame
    // navigation.
    main_document_response.WaitForRequest();
    main_document_response.Send(net::HTTP_OK, "main_document");
    main_document_response.Done();

    // The URL should still point to the kInitialUrl until the activation is
    // completed.
    EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), kInitialUrl);

    // Make sure that the prerender was not activated yet.
    EXPECT_FALSE(prerender_observer.was_activated());

    primary_page_manager.WaitForNavigationFinished();
    prerender_observer.WaitForActivation();
  }

  // Prerender should be activated and the URL should point to kPrerenderingUrl.
  {
    EXPECT_TRUE(prerender_observer.was_activated());
    EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
    EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
  }

  // "Navigation.Prerender.ActivationCommitDeferTime" histogram should be
  // recorded as PrerenderCommitDeferringCondition defers the navigation.
  histogram_tester().ExpectTotalCount(
      "Navigation.Prerender.ActivationCommitDeferTime.SpeculationRule", 1u);
}

// TODO(crbug.com/40170624): Now the File System Access API is not
// supported on Android. Enable this browser test after
// https://crbug.com/1011535 is fixed.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_DeferPrivateOriginFileSystem DISABLED_DeferPrivateOriginFileSystem
#else
#define MAYBE_DeferPrivateOriginFileSystem DeferPrivateOriginFileSystem
#endif

// Tests that access to the origin private file system via the File System
// Access API is deferred until activating the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MAYBE_DeferPrivateOriginFileSystem) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/restriction_file_system.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Make a prerendered page.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  auto* prerender_render_frame_host = GetPrerenderedMainFrameHost(host_id);

  EXPECT_EQ(
      true,
      ExecJs(prerender_render_frame_host, "accessOriginPrivateFileSystem();",
             EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE |
                 EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  // Run a event loop so the page can fail the test.
  EXPECT_TRUE(ExecJs(prerender_render_frame_host, "runLoop();"));

  // Activate the page.
  NavigatePrimaryPage(kPrerenderingUrl);

  // Wait for the completion of `accessOriginPrivateFileSystem`.
  EXPECT_EQ(true, EvalJs(prerender_render_frame_host, "result;"));
  // Check the event sequence seen in the prerendered page.
  EvalJsResult results = EvalJs(prerender_render_frame_host, "eventsSeen");
  std::vector<std::string> eventsSeen;
  base::Value resultsList = results.ExtractList();
  for (auto& result : resultsList.GetList())
    eventsSeen.push_back(result.GetString());
  EXPECT_THAT(eventsSeen,
              testing::ElementsAreArray(
                  {"accessOriginPrivateFileSystem (prerendering: true)",
                   "prerenderingchange (prerendering: false)",
                   "getDirectory (prerendering: false)"}));
}

// Tests that DocumentUserData object is not cleared on activating a
// prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, DocumentUserData) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start a prerender.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  auto* prerender_render_frame_host = GetPrerenderedMainFrameHost(host_id);

  // Get the DocumentData associated with prerender RenderFrameHost.
  DocumentData::CreateForCurrentDocument(prerender_render_frame_host);
  base::WeakPtr<DocumentData> data =
      DocumentData::GetForCurrentDocument(prerender_render_frame_host)
          ->GetWeakPtr();
  EXPECT_TRUE(data);

  // Activate the prerendered page.
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);

  // The prerender host should be consumed.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  // DocumentData associated with document shouldn't have been cleared on
  // activating prerendered page.
  base::WeakPtr<DocumentData> data_after_activation =
      DocumentData::GetForCurrentDocument(current_frame_host())->GetWeakPtr();
  EXPECT_TRUE(data_after_activation);

  // Both the instances of DocumentData before and after activation should point
  // to the same object and make sure they aren't null.
  EXPECT_EQ(data_after_activation.get(), data.get());
}

// Tests that executing the GamepadMonitor API on a prerendering before
// navigating to the prerendered page causes cancel prerendering.
// This test cannot be a web test because web tests handles the GamepadMonitor
// interface on the renderer side. See GamepadController::Install().
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, GamepadMonitorCancelPrerendering) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Make a prerendered page.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  auto* prerender_render_frame_host = GetPrerenderedMainFrameHost(host_id);

  // Executing `navigator.getGamepads()` to start binding the GamepadMonitor
  // interface.
  std::ignore = EvalJs(prerender_render_frame_host, "navigator.getGamepads()",
                       EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE);
  // Verify Mojo capability control cancels prerendering.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kMojoBinderPolicy);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCancelledInterface.SpeculationRule",
      PrerenderCancelledInterface::kGamepadMonitor, 1);
}

// TODO(crbug.com/40178939) LaCrOS binds the HidManager interface, which
// might be required by Gamepad Service, in a different way. Disable this test
// before figuring out how to set the test context correctly.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// Tests that requesting to bind the GamepadMonitor interface after the
// prerenderingchange event dispatched does not cancel prerendering.
// This test cannot be a web test because web tests handles the GamepadMonitor
// interface on the renderer side. See GamepadController::Install().
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, GamepadMonitorAfterNavigation) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/prerender/restriction-gamepad.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Make a prerendered page.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  AddPrerender(kPrerenderingUrl);
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  // Activate the prerendered page to dispatch the prerenderingchange event and
  // run the Gamepad API in the event.
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
  // Wait for the completion of the prerenderingchange event to make sure the
  // API is called.
  EXPECT_EQ(true, EvalJs(shell()->web_contents(), "prerenderingChanged"));
  // The API call shouldn't discard the prerendered page and shouldn't restart
  // navigation.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

// Tests that accessing the clipboard via the execCommand API fails because the
// page does not has any user activation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, ClipboardByExecCommandFail) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Make a prerendered page.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  auto* prerender_render_frame_host = GetPrerenderedMainFrameHost(host_id);

  // Access the clipboard and fail.
  EXPECT_EQ(false,
            EvalJs(prerender_render_frame_host, "document.execCommand('copy');",
                   EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_EQ(false, EvalJs(prerender_render_frame_host,
                          "document.execCommand('paste');",
                          EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

void LoadAndWaitForPrerenderDestroyed(test::PrerenderTestHelper* helper,
                                      const GURL prerendering_url,
                                      const std::string& target_hint) {
  test::PrerenderHostCreationWaiter host_creation_waiter;
  helper->AddPrerendersAsync({prerendering_url}, /*eagerness=*/std::nullopt,
                             target_hint);
  FrameTreeNodeId host_id = host_creation_waiter.Wait();
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
  test::PrerenderHostObserver host_observer(*prerender_web_contents, host_id);
  host_observer.WaitForDestroyed();
  if (target_hint == "_blank") {
    EXPECT_FALSE(helper->HasNewTabHandle(host_id));
  } else {
    EXPECT_TRUE(helper->GetHostForUrl(*prerender_web_contents, prerendering_url)
                    .is_null());
  }
}

#if BUILDFLAG(ENABLE_PPAPI)
// Tests that we will cancel the prerendering if the prerendering page attempts
// to use plugins.
//
// TODO(crbug.com/40180674): This does not cover embedders that override
// `ContentRendererClient::OverrideCreatePlugin()` (such as for Chrome's PDF
// viewer), as cancellation depends on the renderer attempting to bind
// `content::mojom::PepperHost`.
IN_PROC_BROWSER_TEST_P(PrerenderTargetAgnosticBrowserTest,
                       PluginsCancelPrerendering) {
  const GURL kInitialUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  LoadAndWaitForPrerenderDestroyed(
      prerender_helper(), GetUrl("/prerender/page-with-embedded-plugin.html"),
      GetTargetHint());
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kMojoBinderPolicy);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCancelledInterface.SpeculationRule",
      PrerenderCancelledInterface::kUnknown, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCancelledUnknownInterface."
      "SpeculationRule",
      InterfaceNameHasher(mojom::PepperHost::Name_), 1);

  LoadAndWaitForPrerenderDestroyed(
      prerender_helper(), GetUrl("/prerender/page-with-object-plugin.html"),
      GetTargetHint());
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kMojoBinderPolicy, 2);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCancelledInterface.SpeculationRule",
      PrerenderCancelledInterface::kUnknown, 2);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCancelledUnknownInterface."
      "SpeculationRule",
      InterfaceNameHasher(mojom::PepperHost::Name_), 2);

  // Run JavaScript code to inject a new iframe to load a page, and see if it
  // correctly runs and results in making a navigation request in the iframe. If
  // the initiator is still working normally after prerendering cancellation,
  // this request should arrive.
  RenderFrameHostImpl* main_frame_host = current_frame_host();
  EXPECT_TRUE(AddTestUtilJS(main_frame_host));
  EXPECT_TRUE(ExecJs(main_frame_host, "add_iframe_async('/title1.html')",
                     EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  WaitForRequest(GetUrl("/title1.html"), 1);
}
#endif  // BUILDFLAG(ENABLE_PPAPI)

#if BUILDFLAG(IS_ANDROID)
// On Android the Notification constructor throws an exception regardless of
// whether the page is being prerendered.
// Tests that we will get the exception from the prerendering if the
// prerendering page attempts to use notification.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, NotificationConstructorAndroid) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Make a prerendered page.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  auto* prerender_render_frame_host = GetPrerenderedMainFrameHost(host_id);

  // Create the Notification and fail.
  EXPECT_EQ(false, EvalJs(prerender_render_frame_host, R"(
    (() => {
      try { new Notification('My Notification'); return true;
      } catch(e) { return false; }
    })();
  )"));
}
#endif  // BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/40184233): Make a WPT when we have a stable way to wait
// cancellation runs.
IN_PROC_BROWSER_TEST_P(PrerenderTargetAgnosticBrowserTest, DownloadByScript) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerendering");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Make a prerendered page.
  test::PrerenderHostCreationWaiter host_creation_waiter;
  prerender_helper()->AddPrerendersAsync(
      {kPrerenderingUrl}, /*eagerness=*/std::nullopt, GetTargetHint());
  FrameTreeNodeId host_id = host_creation_waiter.Wait();
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
  test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
      *prerender_web_contents, kPrerenderingUrl);

  test::PrerenderHostObserver host_observer(*prerender_web_contents, host_id);
  auto* prerender_host = test::PrerenderTestHelper::GetPrerenderedMainFrameHost(
      *prerender_web_contents, host_id);
  const std::string js_string = R"(
      document.body.innerHTML =
          "<a id='target' download='download-link' href='cache.txt'>here</a>";
      document.getElementById('target').click();
  )";
  ExecuteScriptAsync(prerender_host, js_string);

  host_observer.WaitForDestroyed();
  if (GetTargetHint() == "_blank") {
    EXPECT_FALSE(prerender_helper()->HasNewTabHandle(host_id));
  } else {
    EXPECT_TRUE(prerender_helper()
                    ->GetHostForUrl(*prerender_web_contents, kPrerenderingUrl)
                    .is_null());
  }

  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kDownload);
}

IN_PROC_BROWSER_TEST_P(PrerenderTargetAgnosticBrowserTest,
                       DownloadInMainFrame) {
  const GURL kInitialUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // TODO(crbug.com/40184233): Make a WPT for the content-disposition WPT test.
  const GURL kDownloadUrl =
      GetUrl("/set-header?Content-Disposition: attachment");

  LoadAndWaitForPrerenderDestroyed(prerender_helper(), kDownloadUrl,
                                   GetTargetHint());

  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kDownload);
}

IN_PROC_BROWSER_TEST_P(PrerenderTargetAgnosticBrowserTest, DownloadInSubframe) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerendering");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Make a prerendered page.
  test::PrerenderHostCreationWaiter host_creation_waiter;
  prerender_helper()->AddPrerendersAsync(
      {kPrerenderingUrl}, /*eagerness=*/std::nullopt, GetTargetHint());
  FrameTreeNodeId host_id = host_creation_waiter.Wait();
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
  test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
      *prerender_web_contents, kPrerenderingUrl);

  test::PrerenderHostObserver host_observer(*prerender_web_contents, host_id);
  auto* prerender_host = test::PrerenderTestHelper::GetPrerenderedMainFrameHost(
      *prerender_web_contents, host_id);
  EXPECT_TRUE(AddTestUtilJS(prerender_host));

  // TODO(crbug.com/40184233): Make a WPT for the content-disposition WPT test.
  const GURL kDownloadUrl =
      GetUrl("/set-header?Content-Disposition: attachment");
  ExecuteScriptAsync(prerender_host,
                     JsReplace("add_iframe_async($1)", kDownloadUrl));

  host_observer.WaitForDestroyed();
  if (GetTargetHint() == "_blank") {
    EXPECT_FALSE(prerender_helper()->HasNewTabHandle(host_id));
  } else {
    EXPECT_TRUE(prerender_helper()
                    ->GetHostForUrl(*prerender_web_contents, kPrerenderingUrl)
                    .is_null());
  }

  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kDownload);
}

// The viewport meta tag is only enabled on Android.
#if BUILDFLAG(IS_ANDROID)
namespace {

// Used to observe the viewport change in the WebContents.
class TestViewportWebContentsObserver : public WebContentsObserver {
 public:
  TestViewportWebContentsObserver(WebContents* web_contents,
                                  blink::mojom::ViewportFit wanted_value)
      : WebContentsObserver(web_contents), wanted_value_(wanted_value) {}

  TestViewportWebContentsObserver(const TestViewportWebContentsObserver&) =
      delete;
  TestViewportWebContentsObserver& operator=(
      const TestViewportWebContentsObserver&) = delete;

  // WebContentsObserver implementation.
  void ViewportFitChanged(blink::mojom::ViewportFit value) override {
    value_ = value;
    if (waiting_for_wanted_value_ && value == wanted_value_) {
      std::move(waiting_for_wanted_value_).Run();
    }
  }

  void WaitForWantedValue() {
    if (value_.has_value() && value_.value() == wanted_value_) {
      return;
    }
    base::RunLoop loop;
    waiting_for_wanted_value_ = loop.QuitClosure();
    loop.Run();
  }

 private:
  base::OnceClosure waiting_for_wanted_value_;
  std::optional<blink::mojom::ViewportFit> value_;
  const blink::mojom::ViewportFit wanted_value_;
};

}  // namespace

// Tests that the viewport-fit property works well on prerendering page:
// * The property in prerendering page shouldn't affect the primary page.
// * After activating the prerendered page, WebContents's viewport property can
//   be updated.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, ViewportFit) {
  const GURL kInitialUrl = GetUrl("/prerender/viewport.html");
  const GURL kPrerenderingUrl = GetUrl("/prerender/viewport.html?prerendering");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  RenderFrameHostImpl* prerender_rfh = GetPrerenderedMainFrameHost(host_id);
  RenderFrameHostImpl* primary_rfh = web_contents_impl()->GetPrimaryMainFrame();

  {
    // Set viewport-fit property in the primary page and the prerendering page.
    // Prerendering shouldn't be cancelled, nor should its property affect the
    // corresponding WebContents's property.
    TestViewportWebContentsObserver observer(web_contents_impl(),
                                             blink::mojom::ViewportFit::kCover);
    EXPECT_TRUE(ExecJs(prerender_rfh, "setViewportFit('contain')"));
    EXPECT_TRUE(ExecJs(primary_rfh, "setViewportFit('cover')"));
    web_contents_impl()->FullscreenStateChanged(
        primary_rfh, true, blink::mojom::FullscreenOptions::New());
    observer.WaitForWantedValue();
  }
  {
    // After the prerendering page is activated, the WebContents's property
    // should be updated.
    TestViewportWebContentsObserver observer(
        web_contents_impl(), blink::mojom::ViewportFit::kContain);
    prerender_helper()->NavigatePrimaryPage(kPrerenderingUrl);
    web_contents_impl()->FullscreenStateChanged(
        prerender_rfh, true, blink::mojom::FullscreenOptions::New());
    observer.WaitForWantedValue();
  }
  EXPECT_TRUE(host_observer.was_activated());
}
#endif  // BUILDFLAG(IS_ANDROID)

// End: Tests for feature restrictions in prerendered pages ====================

// Tests prerendering for low-end devices.
class PrerenderLowMemoryBrowserTest : public PrerenderBrowserTest {
 public:
  PrerenderLowMemoryBrowserTest() {
    // Set the value of memory threshold more than the physical memory.  The
    // test will expect that prerendering does not occur.
    std::string memory_threshold =
        base::NumberToString(base::SysInfo::AmountOfPhysicalMemoryMB() + 1);
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kPrerender2MemoryControls,
          {{blink::features::kPrerender2MemoryThresholdParamName,
            memory_threshold}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that prerendering doesn't run for low-end devices.
IN_PROC_BROWSER_TEST_F(PrerenderLowMemoryBrowserTest, NoPrerender) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  // Attempt to prerender.
  test::PrerenderHostRegistryObserver observer(*web_contents_impl());
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  AddPrerenderAsync(kPrerenderingUrl);
  observer.WaitForTrigger(kPrerenderingUrl);

  // It should fail.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kLowEndDevice);

  // Navigate primary page to flush the metrics.
  NavigatePrimaryPage(kPrerenderingUrl);
  // Cross-check that in case of low memory the eligibility reason points to
  // kLowMemory.
  ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder().BuildEntry(
      PrimaryPageSourceId(), PreloadingType::kPrerender,
      PreloadingEligibility::kLowMemory, PreloadingHoldbackStatus::kUnspecified,
      PreloadingTriggeringOutcome::kUnspecified,
      PreloadingFailureReason::kUnspecified,
      /*accurate=*/true,
      /*ready_time=*/std::nullopt,
      blink::mojom::SpeculationEagerness::kEager)});
}

class PrerenderSequentialPrerenderingBrowserTest : public PrerenderBrowserTest {
 public:
  PrerenderSequentialPrerenderingBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrerender2NewLimitAndScheduler,
          {{"max_num_of_running_speculation_rules_eager_prerenders",
            base::NumberToString(MaxNumOfRunningPrerenders())}}},
         {features::kPrerender2EmbedderBlockedHosts,
          {{"embedder_blocked_hosts", "a.test,b.test,c.test"}}}},
        {});
  }

  int MaxNumOfRunningPrerenders() const { return 4; }

 protected:
  void TestSequentialPrerenderingVisibilityStateTransition(
      Visibility initial_visibility,
      Visibility background_visibility);

 private:
  base::test::ScopedFeatureList feature_list_;
};

namespace {

// Records all the navigation start and finish events until the navigation to
// `target_url` finished.
class SequentialPrerenderObserver : public WebContentsObserver {
 public:
  enum class EventType {
    kStart,
    kFinish,
  };

  SequentialPrerenderObserver(WebContents& web_contents, const GURL& target_url)
      : WebContentsObserver(&web_contents), target_url_(target_url) {}

  const std::vector<std::pair<GURL, EventType>>& events_sequence() const {
    return events_sequence_;
  }

  void WaitForTargetNavigationFinished() {
    if (target_navigation_finished_) {
      return;
    }
    base::RunLoop loop;
    quit_closure_ = loop.QuitClosure();
    loop.Run();
  }

 private:
  void DidStartNavigation(NavigationHandle* handle) override {
    events_sequence_.emplace_back(handle->GetURL(), EventType::kStart);
  }

  void DidFinishNavigation(NavigationHandle* handle) override {
    events_sequence_.emplace_back(handle->GetURL(), EventType::kFinish);
    if (handle->GetURL() != target_url_) {
      return;
    }
    target_navigation_finished_ = true;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  const GURL target_url_;
  base::OnceClosure quit_closure_;
  bool target_navigation_finished_ = false;

  std::vector<std::pair<GURL, EventType>> events_sequence_;
};

}  // namespace

// Tests that multiple prerenderings should be enqueued and the pending request
// starts right after the previous prerender calls DidFinishNavigation.
IN_PROC_BROWSER_TEST_F(PrerenderSequentialPrerenderingBrowserTest,
                       SequentialPrerendering) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  std::vector<GURL> prerender_urls;
  for (int i = 0; i < 3; i++) {
    prerender_urls.push_back(
        GetUrl("/empty.html?prerender" + base::NumberToString(i)));
  }

  SequentialPrerenderObserver observer(*web_contents(), prerender_urls[2]);

  // Insert 3 URLs into the speculation rules at the same time.
  AddPrerendersAsync(prerender_urls);

  // Wait for DidFinishNavigation on the last URL.
  observer.WaitForTargetNavigationFinished();

  // Check if all the prerender requests are handled sequentially.
  std::vector<std::pair<GURL, SequentialPrerenderObserver::EventType>>
      expected_sequence = {
          {prerender_urls[0], SequentialPrerenderObserver::EventType::kStart},
          {prerender_urls[1], SequentialPrerenderObserver::EventType::kStart},
          {prerender_urls[0], SequentialPrerenderObserver::EventType::kFinish},
          {prerender_urls[2], SequentialPrerenderObserver::EventType::kStart},
          {prerender_urls[1], SequentialPrerenderObserver::EventType::kFinish},
          {prerender_urls[2], SequentialPrerenderObserver::EventType::kFinish},
      };
  EXPECT_EQ(observer.events_sequence(), expected_sequence);

  // Make sure if the activation succeeds and other prerender hosts are
  // destroyed.
  std::vector<std::unique_ptr<test::PrerenderHostObserver>> prerender_observers;
  for (int i = 0; i < 3; i++) {
    prerender_observers.push_back(std::make_unique<test::PrerenderHostObserver>(
        *web_contents(), GetHostForUrl(prerender_urls[i])));
  }
  NavigatePrimaryPage(prerender_urls[1]);
  prerender_observers[0]->WaitForDestroyed();
  prerender_observers[1]->WaitForActivation();
  prerender_observers[2]->WaitForDestroyed();

  EXPECT_TRUE(prerender_observers[1]->was_activated());
  EXPECT_FALSE(HasHostForUrl(prerender_urls[1]));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), prerender_urls[1]);
  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kActivated, 1);
  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kOtherPrerenderedPageActivated, 2);
}

// Tests that a cancelled request in the pending queue is skipped and the next
// prerender starts.
IN_PROC_BROWSER_TEST_F(PrerenderSequentialPrerenderingBrowserTest,
                       SkipCancelledPrerenderAndStartNextPrerender) {
  net::test_server::ControllableHttpResponse response1(
      embedded_test_server(), "/empty.html?prerender1");
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerender1 =
      embedded_test_server()->GetURL("/empty.html?prerender1");
  const GURL kPrerender2 =
      embedded_test_server()->GetURL("/empty.html?prerender2");
  const GURL kPrerender3 =
      embedded_test_server()->GetURL("/empty.html?prerender3");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());

  // Insert 3 URLs into the speculation rules at the same time. The first
  // prerender should start immediately, and the other two requests enqueued.
  AddPrerendersAsync({kPrerender1, kPrerender2, kPrerender3});

  registry_observer.WaitForTrigger(kPrerender3);
  test::PrerenderHostObserver prerender3_observer(*web_contents(),
                                                  GetHostForUrl(kPrerender3));

  // Stop the first prerendering initial navigation.
  response1.WaitForRequest();

  // Cancel the second prerender, and this cancellation shouldn't prevent the
  // incoming third prerender from starting.
  web_contents_impl()->GetPrerenderHostRegistry()->CancelHost(
      GetHostForUrl(kPrerender2), PrerenderFinalStatus::kDestroyed);

  // Resume the first prerender. The second one doesn't send
  // request as the host has been already destroyed.
  response1.Send(net::HTTP_OK, "");
  response1.Done();

  // Wait for the third prerender completes its initial navigation.
  WaitForPrerenderLoadCompletion(kPrerender3);

  // Activate the third prerender and it should succeed.
  NavigatePrimaryPage(kPrerender3);
  prerender3_observer.WaitForActivation();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerender3);
  EXPECT_TRUE(prerender3_observer.was_activated());

  // The first prerender is destroyed during activation.
  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kOtherPrerenderedPageActivated, 1);

  // The second prerender is destroyed directly.
  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kDestroyed, 1);

  // The third prerender is successfully activated.
  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kActivated, 1);
}

// Test to make sure that the completion of iframe navigation in a prerendering
// page doesn't start another pending prerender request.
IN_PROC_BROWSER_TEST_F(
    PrerenderSequentialPrerenderingBrowserTest,
    IframeNavigationFinishDontDisruptPrerenderNavigationFinish) {
  net::test_server::ControllableHttpResponse response2(
      embedded_test_server(), "/empty.html?prerender2");
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerender1 =
      embedded_test_server()->GetURL("/empty.html?prerender1");
  const GURL kPrerender2 =
      embedded_test_server()->GetURL("/empty.html?prerender2");
  const GURL kPrerender3 =
      embedded_test_server()->GetURL("/empty.html?prerender3");
  const GURL kIframeUrl = embedded_test_server()->GetURL("/empty.html?iframe");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Insert 3 URLs into the speculation rules at the same time. The first
  // prerender should start immediately.
  AddPrerendersAsync({kPrerender1, kPrerender2, kPrerender3});

  // Stop the second prerendering initial navigation.
  response2.WaitForRequest();

  WaitForPrerenderLoadCompletion(kPrerender1);
  FrameTreeNodeId host_id = GetHostForUrl(kPrerender1);
  ASSERT_TRUE(host_id);

  // Insert an iframe into the first prerender's main frame host.
  RenderFrameHost* prerender_frame_host = GetPrerenderedMainFrameHost(host_id);
  EXPECT_TRUE(AddTestUtilJS(prerender_frame_host));
  EXPECT_EQ("LOADED", EvalJs(prerender_frame_host,
                             JsReplace("add_iframe($1)", kIframeUrl)));
  RenderFrameHost* child_frame_host = ChildFrameAt(prerender_frame_host, 0);
  ASSERT_NE(child_frame_host, nullptr);
  ASSERT_EQ(child_frame_host->GetLastCommittedURL(), kIframeUrl);

  // Confirm that the third prerender doesn't start even if the iframe
  // navigation within the prerendered main frame has finished.
  PrerenderHost* prerender3_host =
      web_contents_impl()->GetPrerenderHostRegistry()->FindHostByUrlForTesting(
          kPrerender3);
  EXPECT_FALSE(prerender3_host->GetInitialNavigationId().has_value());
}

// Tests that if PrerenderHostRegistry is attempting to activate a pending
// prerender host, it will be successfully canceled with the final status of
// `kActivatedBeforeStarted`.
IN_PROC_BROWSER_TEST_F(PrerenderSequentialPrerenderingBrowserTest,
                       ActivateBeforePrerenderStarts) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/empty.html?prerender1");
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerender1 =
      embedded_test_server()->GetURL("/empty.html?prerender1");
  const GURL kPrerender2 =
      embedded_test_server()->GetURL("/empty.html?prerender2");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());

  // Insert 2 URLs into the speculation rules at the same time.
  AddPrerendersAsync({kPrerender1, kPrerender2});

  registry_observer.WaitForTrigger(kPrerender2);
  test::PrerenderHostObserver prerender2_observer(*web_contents(),
                                                  GetHostForUrl(kPrerender2));

  // Stop the first prerendering initial navigation.
  response.WaitForRequest();

  // Activate the page with pending prerender.
  NavigatePrimaryPage(kPrerender2);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerender2);
  EXPECT_FALSE(prerender2_observer.was_activated());

  // The first prerender was destroyed by SpeculationHostImpl.
  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kTriggerDestroyed, 1);
  // The second prerender is destroyed since activation navigation is requested
  // while it's still pending.
  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kActivatedBeforeStarted, 1);

  ukm::SourceId ukm_source_id = PrimaryPageSourceId();
  ExpectPreloadingAttemptUkm({
      attempt_ukm_entry_builder().BuildEntry(
          ukm_source_id, PreloadingType::kPrerender,
          PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
          PreloadingTriggeringOutcome::kRunning,
          PreloadingFailureReason::kUnspecified,
          /*accurate=*/false,
          /*ready_time=*/std::nullopt,
          blink::mojom::SpeculationEagerness::kEager),
      attempt_ukm_entry_builder().BuildEntry(
          ukm_source_id, PreloadingType::kPrerender,
          PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
          PreloadingTriggeringOutcome::kTriggeredButPending,
          PreloadingFailureReason::kUnspecified,
          /*accurate=*/true,
          /*ready_time=*/std::nullopt,
          blink::mojom::SpeculationEagerness::kEager),
  });
}

// Test that if the 5 URLs are specified in the speculation rule while only 4
// prerenders are allowed, the 5th prerender should be cancelled.
IN_PROC_BROWSER_TEST_F(PrerenderSequentialPrerenderingBrowserTest,
                       ExceedTheRequestNumberLimit) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/empty.html?prerender1");
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");

  std::vector<GURL> prerender_urls;

  ASSERT_EQ(MaxNumOfRunningPrerenders(), 4);
  for (int i = 0; i < MaxNumOfRunningPrerenders() + 1; i++) {
    prerender_urls.push_back(embedded_test_server()->GetURL(
        "/empty.html?prerender" + base::NumberToString(i)));
  }

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());

  // Insert 5 URLs into the speculation rules at the same time.
  AddPrerendersAsync(prerender_urls);

  // Stop the first prerendering initial navigation.
  response.WaitForRequest();

  // Wait for the last prerender request will be triggered.
  registry_observer.WaitForTrigger(prerender_urls.back());

  // The last prerender is destroyed since the number of prerender requests
  // from speculation rules exceeds its limit of 4.
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kMaxNumOfRunningEagerPrerendersExceeded, 1);
}

// Test that the requests from embedder are handled immediately regardless of
// the requests from speculation rules.
IN_PROC_BROWSER_TEST_F(PrerenderSequentialPrerenderingBrowserTest,
                       EmbedderPrerenderHandledImmediately) {
  net::test_server::ControllableHttpResponse prerender1_response(
      embedded_test_server(), "/empty.html?prerender1");
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerender1 =
      embedded_test_server()->GetURL("/empty.html?prerender1");
  const GURL kPrerender2 =
      embedded_test_server()->GetURL("/empty.html?prerender2");
  const GURL kEmbedderPrerender =
      embedded_test_server()->GetURL("/empty.html?embedder");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Insert 2 URLs into the speculation rules at the same time.
  AddPrerendersAsync({kPrerender1, kPrerender2});

  // Stop the first prerender's initial navigation.
  prerender1_response.WaitForRequest();

  // Start prerendering by embedder triggered prerendering; this should start
  // immediately instead of being enqueued.
  std::unique_ptr<PrerenderHandle> prerender_handle =
      AddEmbedderTriggeredPrerender(kEmbedderPrerender);
  EXPECT_TRUE(prerender_handle);

  // Confirm that embedder triggered prerender does not affect the pending
  // prerender triggered by speculation rules.
  PrerenderHost* prerender2_host =
      web_contents_impl()->GetPrerenderHostRegistry()->FindHostByUrlForTesting(
          kPrerender2);
  EXPECT_FALSE(prerender2_host->GetInitialNavigationId());

  // Also confirm the remaining request triggered by speculation rules can
  // resume if the first prerender finish its navigation, to make sure the
  // prioritized embedder request doesn't break conditions of other requests.
  prerender1_response.Send(net::HTTP_OK, "");
  prerender1_response.Done();
  WaitForPrerenderLoadCompletion(kPrerender2);
  EXPECT_TRUE(HasHostForUrl(kPrerender2));

  // Activate the embedder triggered prerender.
  test::PrerenderHostObserver embedder_observer(
      *web_contents(), GetHostForUrl(kEmbedderPrerender));
  shell()->web_contents()->OpenURL(
      OpenURLParams(
          kEmbedderPrerender, Referrer(), WindowOpenDisposition::CURRENT_TAB,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});

  embedder_observer.WaitForActivation();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kEmbedderPrerender);
  EXPECT_TRUE(embedder_observer.was_activated());
}

// Test that hosts in the embedder blocklist are not prerendered.
IN_PROC_BROWSER_TEST_F(PrerenderSequentialPrerenderingBrowserTest,
                       EmbedderHostBlocklisted) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  // b.test was added to embedder_blocked_hosts in the test setup.
  const GURL kEmbedderPrerender =
      embedded_test_server()->GetURL("b.test", "/empty.html?embedder");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(shell()->web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prerendering by embedder triggered prerendering. This should be
  // blocked because b.test is in embedder_blocked_hosts.
  std::unique_ptr<PrerenderHandle> prerender_handle =
      AddEmbedderTriggeredPrerenderAsync(kEmbedderPrerender);

  EXPECT_FALSE(prerender_handle);
  EXPECT_TRUE(GetHostForUrl(kEmbedderPrerender).is_null());
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kEmbedderHostDisallowed, 1);
}

// Tests that if the running prerender is cancelled by
// PrerenderHostRegistry::CancelHost(), the next pending prerender starts its
// navigation.
IN_PROC_BROWSER_TEST_F(PrerenderSequentialPrerenderingBrowserTest,
                       RunningHostCancellationStartPendingPrerender) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/empty.html?prerender1");
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerender1 =
      embedded_test_server()->GetURL("/empty.html?prerender1");
  const GURL kPrerender2 =
      embedded_test_server()->GetURL("/empty.html?prerender2");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());

  // Insert 2 URLs into the speculation rules at the same time.
  AddPrerendersAsync({kPrerender1, kPrerender2});

  registry_observer.WaitForTrigger(kPrerender2);
  test::PrerenderHostObserver prerender2_observer(*web_contents(),
                                                  GetHostForUrl(kPrerender2));

  // Stop the first prerendering initial navigation.
  response.WaitForRequest();

  // Cancel the running prerender. The next pending prerender should start upon
  // this cancellation.
  web_contents_impl()->GetPrerenderHostRegistry()->CancelHost(
      GetHostForUrl(kPrerender1), PrerenderFinalStatus::kDestroyed);
  WaitForPrerenderLoadCompletion(kPrerender2);

  // Activate the page with the prerender that was pending.
  NavigatePrimaryPage(kPrerender2);
  prerender2_observer.WaitForActivation();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerender2);
  EXPECT_TRUE(prerender2_observer.was_activated());

  // The first prerender should be manually destroyed.
  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kDestroyed, 1);
  // The second prerender should be successfully activated.
  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kActivated, 1);
}

// Tests that if the running prerender is cancelled by
// PrerenderHostRegistry::CancelHosts(), the next pending prerender
// starts its navigation.
IN_PROC_BROWSER_TEST_F(PrerenderSequentialPrerenderingBrowserTest,
                       SpeculationRulesUpdateStartPendingPrerender) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/empty.html?prerender1");
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerender1 =
      embedded_test_server()->GetURL("/empty.html?prerender1");
  const GURL kPrerender2 =
      embedded_test_server()->GetURL("/empty.html?prerender2");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());

  // Insert 2 URLs into the speculation rules in order. The prerender for
  // `kPrerender1` should start first.
  std::string script = R"(
                        let sc = document.createElement('script');
                        sc.type = 'speculationrules';
                        sc.id = $1;
                        sc.textContent = JSON.stringify({
                          prerender: [
                            {source: "list", urls: [$2]}
                          ]
                        });
                        document.head.appendChild(sc);
                        )";
  ASSERT_TRUE(ExecJs(web_contents_impl()->GetPrimaryMainFrame(),
                     JsReplace(script, "prerender1", kPrerender1)));
  ASSERT_TRUE(ExecJs(web_contents_impl()->GetPrimaryMainFrame(),
                     JsReplace(script, "prerender2", kPrerender2)));

  registry_observer.WaitForTrigger(kPrerender2);
  test::PrerenderHostObserver prerender2_observer(*web_contents(),
                                                  GetHostForUrl(kPrerender2));

  // Stop the first prerendering initial navigation.
  response.WaitForRequest();

  // Delete the first speculation rule. This speculation rules removal invokes
  // the PrerenderHostRegistry::CancelHosts(), and the next pending
  // prerender should start upon the cancellation.
  ASSERT_TRUE(ExecJs(web_contents_impl()->GetPrimaryMainFrame(),
                     "document.querySelector('#prerender1').remove()"));
  WaitForPrerenderLoadCompletion(kPrerender2);

  // Activate the page with the prerender that was pending.
  NavigatePrimaryPage(kPrerender2);
  prerender2_observer.WaitForActivation();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerender2);
  EXPECT_TRUE(prerender2_observer.was_activated());

  // The first prerender should be cancelled by the trigger.
  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kSpeculationRuleRemoved, 1);
  // The second prerender should be successfully activated.
  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kActivated, 1);
}

// Test that a pending prerender should have the
// `PreloadingTriggeringOutcome::kTriggeredButPending`.
IN_PROC_BROWSER_TEST_F(PrerenderSequentialPrerenderingBrowserTest,
                       PreloadingTriggeringOutcomeForPendingPrerender) {
  net::test_server::ControllableHttpResponse response1(
      embedded_test_server(), "/empty.html?prerender1");
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerender1 =
      embedded_test_server()->GetURL("/empty.html?prerender1");
  const GURL kPrerender2 =
      embedded_test_server()->GetURL("/empty.html?prerender2");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());

  // Insert 2 URLs into the speculation rules at the same time.
  AddPrerendersAsync({kPrerender1, kPrerender2});
  registry_observer.WaitForTrigger(kPrerender2);

  // Stop the first prerendering initial navigation.
  response1.WaitForRequest();

  // The pending host should have
  // `PreloadingTriggeringOutcome::kTriggeredButPending`.
  PrerenderHost* prerender2_host =
      web_contents_impl()->GetPrerenderHostRegistry()->FindHostByUrlForTesting(
          kPrerender2);
  auto* preloading_attempt_impl = static_cast<PreloadingAttemptImpl*>(
      prerender2_host->preloading_attempt().get());
  EXPECT_EQ(test::PreloadingAttemptAccessor(preloading_attempt_impl)
                .GetTriggeringOutcome(),
            PreloadingTriggeringOutcome::kTriggeredButPending);

  NavigationHandleObserver activation_observer(web_contents(), kPrerender1);
  test::PrerenderHostObserver prerender1_observer(*web_contents(),
                                                  GetHostForUrl(kPrerender1));

  // Defer the activation until the ongoing initial navigation in prerender
  // frame tree commits.
  TestActivationManager primary_page_manager(shell()->web_contents(),
                                             kPrerender1);
  ASSERT_TRUE(ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
                     JsReplace("location = $1", kPrerender1)));

  NavigationRequest* request =
      web_contents_impl()->GetPrimaryFrameTree().root()->navigation_request();

  // Wait until the activation navigation is deferred by
  // CommitDeferringCondition.
  ASSERT_TRUE(primary_page_manager.WaitForBeforeChecks());
  primary_page_manager.ResumeActivation();

  // Confirm that the activation navigation is deferred.
  EXPECT_TRUE(request->IsCommitDeferringConditionDeferredForTesting());

  // Complete the first prerender response and finish its initial navigation.
  response1.Send(net::HTTP_OK, "");
  response1.Done();

  primary_page_manager.WaitForNavigationFinished();
  prerender1_observer.WaitForActivation();

  // The prerender1 should succeed in activation and have kSuccess outcome. The
  // prerender2 should start right after the activation but get destroyed by the
  // change of the primary page soon, so it should result in the kRunning
  // outcome.
  ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
  ExpectPreloadingAttemptUkm({
      attempt_ukm_entry_builder().BuildEntry(
          ukm_source_id, PreloadingType::kPrerender,
          PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
          PreloadingTriggeringOutcome::kSuccess,
          PreloadingFailureReason::kUnspecified,
          /*accurate=*/true,
          /*ready_time=*/kMockElapsedTime,
          blink::mojom::SpeculationEagerness::kEager),
      attempt_ukm_entry_builder().BuildEntry(
          ukm_source_id, PreloadingType::kPrerender,
          PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
          PreloadingTriggeringOutcome::kTriggeredButPending,
          PreloadingFailureReason::kUnspecified,
          /*accurate=*/false,
          /*ready_time=*/std::nullopt,
          blink::mojom::SpeculationEagerness::kEager),
  });
}

// Test that when the running prerender is destroyed due to the activation of
// another already prerendered page, other pending prerender's outcome is
// recorded as `kTriggeredButPending`.
IN_PROC_BROWSER_TEST_F(
    PrerenderSequentialPrerenderingBrowserTest,
    PreloadingTriggeringOutcomeForStartingPrerenderBeforeDestruction) {
  net::test_server::ControllableHttpResponse response2(
      embedded_test_server(), "/empty.html?prerender2");
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerender1 =
      embedded_test_server()->GetURL("/empty.html?prerender1");
  const GURL kPrerender2 =
      embedded_test_server()->GetURL("/empty.html?prerender2");
  const GURL kPrerender3 =
      embedded_test_server()->GetURL("/empty.html?prerender3");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());

  // Insert 3 URLs into the speculation rules at the same time.
  AddPrerendersAsync({kPrerender1, kPrerender2, kPrerender3});
  registry_observer.WaitForTrigger(kPrerender3);
  test::PrerenderHostObserver prerender1_observer(*web_contents(),
                                                  GetHostForUrl(kPrerender1));

  // Stop the second prerendering initial navigation.
  response2.WaitForRequest();

  NavigationHandleObserver activation_observer(web_contents(), kPrerender1);

  // Activate prerender1. The trigger should destroy all the other prerender
  // hosts.
  NavigatePrimaryPage(kPrerender1);
  prerender1_observer.WaitForActivation();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerender1);
  EXPECT_TRUE(prerender1_observer.was_activated());

  ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
  ExpectPreloadingAttemptUkm({
      attempt_ukm_entry_builder().BuildEntry(
          ukm_source_id, PreloadingType::kPrerender,
          PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
          PreloadingTriggeringOutcome::kSuccess,
          PreloadingFailureReason::kUnspecified,
          /*accurate=*/true,
          /*ready_time=*/kMockElapsedTime,
          blink::mojom::SpeculationEagerness::kEager),
      attempt_ukm_entry_builder().BuildEntry(
          ukm_source_id, PreloadingType::kPrerender,
          PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
          PreloadingTriggeringOutcome::kRunning,
          PreloadingFailureReason::kUnspecified,
          /*accurate=*/false,
          /*ready_time=*/std::nullopt,
          blink::mojom::SpeculationEagerness::kEager),
      attempt_ukm_entry_builder().BuildEntry(
          ukm_source_id, PreloadingType::kPrerender,
          PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
          PreloadingTriggeringOutcome::kTriggeredButPending,
          PreloadingFailureReason::kUnspecified,
          /*accurate=*/false,
          /*ready_time=*/std::nullopt,
          blink::mojom::SpeculationEagerness::kEager),
  });
}

// Test that all the prerender hosts except the one to be activated are
// cancelled regardless of their status right after the PrerenderHostRegistry
// receives the activation request.
IN_PROC_BROWSER_TEST_F(PrerenderSequentialPrerenderingBrowserTest,
                       CancelAllPrerenderUponActivationRequestArrival) {
  net::test_server::ControllableHttpResponse response3(
      embedded_test_server(), "/empty.html?prerender3");
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");

  ASSERT_EQ(MaxNumOfRunningPrerenders(), 4);
  std::vector<GURL> prerender_urls;
  for (int i = 1; i <= MaxNumOfRunningPrerenders(); i++) {
    prerender_urls.push_back(embedded_test_server()->GetURL(
        "/empty.html?prerender" + base::NumberToString(i)));
  }

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());

  // Insert 4 URLs into the speculation rules at the same time.
  AddPrerendersAsync(prerender_urls);
  registry_observer.WaitForTrigger(prerender_urls[3]);

  // Stop the third prerendering initial navigation.
  response3.WaitForRequest();

  NavigationHandleObserver activation_observer(web_contents(),
                                               prerender_urls[0]);
  test::PrerenderHostObserver prerender1_observer(
      *web_contents(), GetHostForUrl(prerender_urls[0]));

  // Defer the activation of the first prerender.
  TestActivationManager primary_page_manager(shell()->web_contents(),
                                             prerender_urls[0]);
  ASSERT_TRUE(ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
                     JsReplace("location = $1", prerender_urls[0])));

  ASSERT_TRUE(primary_page_manager.WaitForBeforeChecks());
  NavigationRequest* request =
      web_contents_impl()->GetPrimaryFrameTree().root()->navigation_request();
  ASSERT_EQ(request->GetURL(), prerender_urls[0]);

  // Confirm that all the other prerender hosts are successfully cancelled.
  for (auto& url : prerender_urls) {
    if (url == prerender_urls[0])
      continue;
    EXPECT_TRUE(GetHostForUrl(url).is_null());
  }

  // Resume the activation.
  primary_page_manager.ResumeActivation();
  prerender1_observer.WaitForActivation();

  // When the PrerenderHostRegistry received the activation request, the status
  // of each prerender host is:
  // 1. Ready for activation,
  // 2. Ready for activation,
  // 3. Running,
  // 4. Pending.
  // We activated the first prerender, so all the other prerender hosts should
  // be cancelled with each corresponding status.
  ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
  ExpectPreloadingAttemptUkm({
      attempt_ukm_entry_builder().BuildEntry(
          ukm_source_id, PreloadingType::kPrerender,
          PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
          PreloadingTriggeringOutcome::kSuccess,
          PreloadingFailureReason::kUnspecified,
          /*accurate=*/true,
          /*ready_time=*/kMockElapsedTime,
          blink::mojom::SpeculationEagerness::kEager),
      attempt_ukm_entry_builder().BuildEntry(
          ukm_source_id, PreloadingType::kPrerender,
          PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
          PreloadingTriggeringOutcome::kReady,
          PreloadingFailureReason::kUnspecified,
          /*accurate=*/false,
          /*ready_time=*/kMockElapsedTime,
          blink::mojom::SpeculationEagerness::kEager),
      attempt_ukm_entry_builder().BuildEntry(
          ukm_source_id, PreloadingType::kPrerender,
          PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
          PreloadingTriggeringOutcome::kRunning,
          PreloadingFailureReason::kUnspecified,
          /*accurate=*/false,
          /*ready_time=*/std::nullopt,
          blink::mojom::SpeculationEagerness::kEager),
      attempt_ukm_entry_builder().BuildEntry(
          ukm_source_id, PreloadingType::kPrerender,
          PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
          PreloadingTriggeringOutcome::kTriggeredButPending,
          PreloadingFailureReason::kUnspecified,
          /*accurate=*/false,
          /*ready_time=*/std::nullopt,
          blink::mojom::SpeculationEagerness::kEager),
  });
}

// Tests that prerendering in a new tab multiple times and activating one of
// them succeed.
IN_PROC_BROWSER_TEST_F(PrerenderSequentialPrerenderingBrowserTest,
                       MultipleNewTabPrerendering) {
  GURL initial_url = GetUrl("/simple_links.html");
  std::vector<GURL> prerendering_urls = {GetUrl("/title2.html"),
                                         GetUrl("/title2.html?2"),
                                         GetUrl("/title2.html?3")};

  // Navigate to an initial page which has a link to `prerendering_urls[0]`.
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Start prerendering.
  std::vector<FrameTreeNodeId> prerender_host_ids;
  std::vector<WebContents*> prerender_web_contents_list;
  for (const GURL& prerendering_url : prerendering_urls) {
    FrameTreeNodeId host_id = prerender_helper()->AddPrerender(
        prerendering_url, /*eagerness=*/std::nullopt, "_blank");

    EXPECT_FALSE(base::Contains(prerender_host_ids, host_id));
    prerender_host_ids.push_back(host_id);

    // Make sure that prerendering in a new tab creates new WebContentsImpl, not
    // reuse existing WebContentsImpl.
    auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
    ASSERT_TRUE(prerender_web_contents);
    EXPECT_NE(prerender_web_contents, web_contents_impl());
    ExpectWebContentsIsForNewTabPrerendering(*prerender_web_contents);
    EXPECT_FALSE(
        base::Contains(prerender_web_contents_list, prerender_web_contents));
    prerender_web_contents_list.push_back(prerender_web_contents);
  }

  // Click the link to prerendering_urls[0]. This should activate
  // prerender_host_ids[0].
  test::PrerenderHostObserver prerender_observer(
      *prerender_web_contents_list[0], prerender_host_ids[0]);
  const std::string kLinkClickScript = R"(
      clickSameSiteNewWindowLink();
  )";
  EXPECT_TRUE(ExecJs(web_contents(), kLinkClickScript));
  prerender_observer.WaitForActivation();
  EXPECT_EQ(prerender_web_contents_list[0]->GetLastCommittedURL(),
            prerendering_urls[0]);
  EXPECT_TRUE(prerender_observer.was_activated());

  // prerendering_urls[0] was consumed for activation, but others were not.
  EXPECT_FALSE(
      HasHostForUrl(*prerender_web_contents_list[0], prerendering_urls[0]));
  EXPECT_TRUE(
      HasHostForUrl(*prerender_web_contents_list[1], prerendering_urls[1]));
  EXPECT_TRUE(
      HasHostForUrl(*prerender_web_contents_list[2], prerendering_urls[2]));

  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);

  // The navigation occurred in a new WebContents, so the original WebContents
  // should still be showing the initial trigger page.
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), initial_url);
}

// Test that the prerender request is handled and stored regardless of the
// initial visibility of the current tab, and when the current tab goes
// background (in the cases where HIDDEN or OCCLUDED is specified by
// `next_visibility`) then the prerender sequence is terminated, and when
// the current tab gets visible then we start the next prerender if we have some
// pending prerender hosts. Note that if the initial visibility is background,
// there is still one prerender allowed to be running.
void PrerenderSequentialPrerenderingBrowserTest::
    TestSequentialPrerenderingVisibilityStateTransition(
        Visibility initial_visibility,
        Visibility next_visibility) {
  net::test_server::ControllableHttpResponse response1(
      embedded_test_server(), "/empty.html?prerender1");
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  const GURL prerender_url1 =
      embedded_test_server()->GetURL("/empty.html?prerender1");
  const GURL prerender_url2 =
      embedded_test_server()->GetURL("/empty.html?prerender2");

  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Set the initial visibility.
  switch (initial_visibility) {
    case Visibility::VISIBLE:
      web_contents()->WasShown();
      break;
    case Visibility::HIDDEN:
      web_contents()->WasHidden();
      break;
    case Visibility::OCCLUDED:
      web_contents()->WasOccluded();
      break;
  }

  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());

  // Insert 2 URLs into the speculation rules at the same time.
  AddPrerendersAsync({prerender_url1, prerender_url2});
  registry_observer.WaitForTrigger(prerender_url2);

  test::PrerenderHostObserver prerender2_observer(
      *web_contents(), GetHostForUrl(prerender_url2));

  // Stop the first prerendering initial navigation.
  response1.WaitForRequest();

  // Change the visibility status to HIDDEN/OCCLUDED.
  switch (next_visibility) {
    case Visibility::HIDDEN:
      web_contents()->WasHidden();
      break;
    case Visibility::OCCLUDED:
      web_contents()->WasOccluded();
      break;
    case Visibility::VISIBLE:
      // The timing of `next_visibility`=Visibility::VISIBLE is delayed until a
      // later point.
      break;
  }

  // Complete the first prerender response and finish its initial navigation.
  // This shouldn't start the pending prerender.
  response1.Send(net::HTTP_OK, "");
  response1.Done();
  WaitForPrerenderLoadCompletion(prerender_url1);

  // Check the prerender host is already ready.
  PrerenderHost* prerender_host =
      web_contents_impl()->GetPrerenderHostRegistry()->FindHostByUrlForTesting(
          prerender_url1);
  auto* preloading_attempt_impl = static_cast<PreloadingAttemptImpl*>(
      prerender_host->preloading_attempt().get());
  EXPECT_EQ(test::PreloadingAttemptAccessor(preloading_attempt_impl)
                .GetTriggeringOutcome(),
            PreloadingTriggeringOutcome::kReady);

  // Check the next prerender host is still pending.
  PrerenderHost* prerender2_host =
      web_contents_impl()->GetPrerenderHostRegistry()->FindHostByUrlForTesting(
          prerender_url2);
  auto* preloading_attempt_impl2 = static_cast<PreloadingAttemptImpl*>(
      prerender2_host->preloading_attempt().get());
  EXPECT_EQ(test::PreloadingAttemptAccessor(preloading_attempt_impl2)
                .GetTriggeringOutcome(),
            PreloadingTriggeringOutcome::kTriggeredButPending);

  // The hidden/occluded page gets back to the foreground. The next pending
  // prerender should start. The case of `next_visibility`=Visibility::VISIBLE
  // is delayed until now.
  web_contents()->WasShown();
  WaitForPrerenderLoadCompletion(prerender_url2);

  // Check the next prerender host is already ready.
  auto* preloading_attempt_impl2_2 = static_cast<PreloadingAttemptImpl*>(
      prerender2_host->preloading_attempt().get());
  EXPECT_EQ(test::PreloadingAttemptAccessor(preloading_attempt_impl2_2)
                .GetTriggeringOutcome(),
            PreloadingTriggeringOutcome::kReady);

  // Activate the second prerender.
  NavigatePrimaryPage(prerender_url2);
  prerender2_observer.WaitForActivation();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), prerender_url2);
  EXPECT_TRUE(prerender2_observer.was_activated());
}

IN_PROC_BROWSER_TEST_F(PrerenderSequentialPrerenderingBrowserTest,
                       PrerenderInBackground_InitialyVisible_Hidden) {
  TestSequentialPrerenderingVisibilityStateTransition(Visibility::VISIBLE,
                                                      Visibility::HIDDEN);
}

IN_PROC_BROWSER_TEST_F(PrerenderSequentialPrerenderingBrowserTest,
                       PrerenderInBackground_InitialyVisible_Occluded) {
  TestSequentialPrerenderingVisibilityStateTransition(Visibility::VISIBLE,
                                                      Visibility::OCCLUDED);
}

IN_PROC_BROWSER_TEST_F(PrerenderSequentialPrerenderingBrowserTest,
                       PrerenderInBackground_InitialyOccluded_Hidden) {
  TestSequentialPrerenderingVisibilityStateTransition(Visibility::OCCLUDED,
                                                      Visibility::HIDDEN);
}

IN_PROC_BROWSER_TEST_F(PrerenderSequentialPrerenderingBrowserTest,
                       PrerenderInBackground_InitialyOccluded_Occluded) {
  TestSequentialPrerenderingVisibilityStateTransition(Visibility::OCCLUDED,
                                                      Visibility::OCCLUDED);
}

IN_PROC_BROWSER_TEST_F(PrerenderSequentialPrerenderingBrowserTest,
                       PrerenderInBackground_InitialyHidden_Hidden) {
  TestSequentialPrerenderingVisibilityStateTransition(Visibility::HIDDEN,
                                                      Visibility::HIDDEN);
}

IN_PROC_BROWSER_TEST_F(PrerenderSequentialPrerenderingBrowserTest,
                       PrerenderInBackground_InitialyHidden_Occluded) {
  TestSequentialPrerenderingVisibilityStateTransition(Visibility::HIDDEN,
                                                      Visibility::OCCLUDED);
}

IN_PROC_BROWSER_TEST_F(PrerenderSequentialPrerenderingBrowserTest,
                       PrerenderInBackground_InitialyHidden_Visible) {
  TestSequentialPrerenderingVisibilityStateTransition(Visibility::HIDDEN,
                                                      Visibility::VISIBLE);
}

IN_PROC_BROWSER_TEST_F(PrerenderSequentialPrerenderingBrowserTest,
                       PrerenderWhenInitiatorInBackground_Queue_Processing) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  const GURL prerender_url1 =
      embedded_test_server()->GetURL("/empty.html?prerender1");
  const GURL prerender_url2 =
      embedded_test_server()->GetURL("/empty.html?prerender2");

  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  web_contents()->WasHidden();

  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());

  // Insert 2 URLs into the speculation rules at the same time.
  AddPrerendersAsync({prerender_url1, prerender_url2});
  registry_observer.WaitForTrigger(prerender_url2);
  WaitForPrerenderLoadCompletion(prerender_url1);

  // Check the prerender host is already ready.
  PrerenderHost* prerender_host =
      web_contents_impl()->GetPrerenderHostRegistry()->FindHostByUrlForTesting(
          prerender_url1);
  auto* preloading_attempt_impl = static_cast<PreloadingAttemptImpl*>(
      prerender_host->preloading_attempt().get());
  EXPECT_EQ(test::PreloadingAttemptAccessor(preloading_attempt_impl)
                .GetTriggeringOutcome(),
            PreloadingTriggeringOutcome::kReady);

  // Check the next prerender host is still pending.
  PrerenderHost* prerender2_host =
      web_contents_impl()->GetPrerenderHostRegistry()->FindHostByUrlForTesting(
          prerender_url2);
  auto* preloading_attempt_impl2 = static_cast<PreloadingAttemptImpl*>(
      prerender2_host->preloading_attempt().get());
  EXPECT_EQ(test::PreloadingAttemptAccessor(preloading_attempt_impl2)
                .GetTriggeringOutcome(),
            PreloadingTriggeringOutcome::kTriggeredButPending);

  // Test if prerender_url1 is cancelled, the prerender host of prerender_url2
  // should be processed.
  web_contents_impl()->GetPrerenderHostRegistry()->CancelHost(
      GetHostForUrl(prerender_url1), PrerenderFinalStatus::kDestroyed);
  WaitForPrerenderLoadCompletion(prerender_url2);

  // Check the next prerender host is already ready.
  test::PrerenderHostObserver prerender2_observer(
      *web_contents(), GetHostForUrl(prerender_url2));
  auto* preloading_attempt_impl2_2 = static_cast<PreloadingAttemptImpl*>(
      prerender2_host->preloading_attempt().get());
  EXPECT_EQ(test::PreloadingAttemptAccessor(preloading_attempt_impl2_2)
                .GetTriggeringOutcome(),
            PreloadingTriggeringOutcome::kReady);

  // Activate the second prerender.
  web_contents()->WasShown();
  NavigatePrimaryPage(prerender_url2);
  prerender2_observer.WaitForActivation();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), prerender_url2);
  EXPECT_TRUE(prerender2_observer.was_activated());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       IsInactiveAndDisallowActivationCancelsPrerendering) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prerendering `kPrerenderingUrl`.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  auto* prerender_render_frame_host = GetPrerenderedMainFrameHost(host_id);

  // Invoke IsInactiveAndDisallowActivation for the prerendered document.
  EXPECT_EQ(prerender_render_frame_host->lifecycle_state(),
            RenderFrameHostImpl::LifecycleStateImpl::kPrerendering);
  EXPECT_TRUE(prerender_render_frame_host->IsInactiveAndDisallowActivation(
      DisallowActivationReasonId::kForTesting));

  // The prerender host for the URL should be destroyed as
  // RenderFrameHost::IsInactiveAndDisallowActivation cancels prerendering in
  // LifecycleStateImpl::kPrerendering state.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  // Cancelling the prerendering disables the activation. The navigation
  // should issue a request again.
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 2);
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kInactivePageRestriction);
  histogram_tester().ExpectUniqueSample(
      "Prerender.CanceledForInactivePageRestriction.DisallowActivationReason."
      "SpeculationRule",
      DisallowActivationReasonId::kForTesting, 1);
}

// Make sure input events are routed to the primary FrameTree not the prerender
// one. See https://crbug.com/1197136
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, InputRoutedToPrimaryFrameTree) {
  const GURL kInitialUrl = GetUrl("/prerender/simple_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  WaitForPrerenderLoadCompletion(kPrerenderingUrl);

  // Touch / click the link and wait for the navigation to complete.
  TestNavigationObserver navigation_observer(web_contents());
  SyntheticTapGestureParams params;
  params.gesture_source_type = mojom::GestureSourceType::kTouchInput;
  params.position = GetCenterCoordinatesOfElementWithId(web_contents(), "link");
  web_contents_impl()->GetRenderViewHost()->GetWidget()->QueueSyntheticGesture(
      std::make_unique<SyntheticTapGesture>(params), base::DoNothing());
  navigation_observer.Wait();

  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, VisibilityWhilePrerendering) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(shell()->web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prerendering `kPrerenderingUrl`.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  auto* prerendered_render_frame_host = GetPrerenderedMainFrameHost(host_id);

  // The visibility state must be "hidden" while prerendering.
  auto* rvh = static_cast<RenderViewHostImpl*>(
      prerendered_render_frame_host->GetRenderViewHost());
  EXPECT_EQ(rvh->GetPageLifecycleStateManager()
                ->CalculatePageLifecycleState()
                ->visibility,
            PageVisibilityState::kHidden);
  EXPECT_EQ(prerendered_render_frame_host->GetVisibilityState(),
            PageVisibilityState::kHidden);

  // Activate prerendering page.
  NavigatePrimaryPage(kPrerenderingUrl);

  // The visibility state should be "visible" after activation.
  EXPECT_EQ(rvh->GetPageLifecycleStateManager()
                ->CalculatePageLifecycleState()
                ->visibility,
            PageVisibilityState::kVisible);
  EXPECT_EQ(prerendered_render_frame_host->GetVisibilityState(),
            PageVisibilityState::kVisible);
}

// Tests that prerendering doesn't affect WebContents::GetTitle().
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, TitleWhilePrerendering) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/simple_page.html");
  const std::u16string kInitialTitle(u"title");
  const std::u16string kPrerenderingTitle(u"OK");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     JsReplace("document.title = $1", kInitialTitle)));
  EXPECT_EQ(shell()->web_contents()->GetTitle(), kInitialTitle);

  // Start a prerender to `kPrerenderUrl` that has title `kPrerenderingTitle`.
  ASSERT_TRUE(AddPrerender(kPrerenderingUrl));

  // Make sure that WebContents::GetTitle() returns the current title from the
  // primary page.
  EXPECT_EQ(shell()->web_contents()->GetTitle(), kInitialTitle);

  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
  // The title should be updated with the activated page.
  EXPECT_EQ(shell()->web_contents()->GetTitle(), kPrerenderingTitle);
}

// Tests that WebContentsObserver::TitleWasSet is not dispatched when title is
// set during prerendering, but is later dispatched after activation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, TitleWasSetWithPrerendering) {
  const GURL kInitialUrl = GetUrl("/title2.html");
  const GURL kPrerenderingUrlWithTitle = GetUrl("/simple_page.html");
  const GURL kPrerenderingUrlWithoutTitle = GetUrl("/title1.html");
  const std::u16string kInitialTitle(u"Title Of Awesomeness");
  const std::u16string kPrerenderingTitle(u"OK");

  // Navigate to an initial page; TitleWasSet should be called when page sets
  // its title.
  {
    testing::NiceMock<MockWebContentsObserver> mock_observer(
        shell()->web_contents());
    EXPECT_CALL(mock_observer, TitleWasSet(testing::_));
    ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
    EXPECT_EQ(shell()->web_contents()->GetTitle(), kInitialTitle);
  }

  // Prerender a page; TitleWasSet should not be called despite the page setting
  // a title.
  {
    testing::NiceMock<MockWebContentsObserver> mock_observer(
        shell()->web_contents());
    EXPECT_CALL(mock_observer, TitleWasSet(testing::_)).Times(0);
    ASSERT_TRUE(AddPrerender(kPrerenderingUrlWithTitle));
  }

  // Activate prerendered page; TitleWasSet should now be called.
  {
    testing::NiceMock<MockWebContentsObserver> mock_observer(
        shell()->web_contents());
    EXPECT_CALL(mock_observer, TitleWasSet(testing::_))
        .WillOnce(testing::Invoke([kPrerenderingTitle](NavigationEntry* entry) {
          EXPECT_EQ(entry->GetTitleForDisplay(), kPrerenderingTitle);
        }));
    NavigatePrimaryPage(kPrerenderingUrlWithTitle);
  }

  // Prerender a page without a title and then activate it; TitleWasSet should
  // not be called.
  {
    testing::NiceMock<MockWebContentsObserver> mock_observer(
        shell()->web_contents());
    EXPECT_CALL(mock_observer, TitleWasSet(testing::_)).Times(0);
    ASSERT_TRUE(AddPrerender(kPrerenderingUrlWithoutTitle));
    NavigatePrimaryPage(kPrerenderingUrlWithoutTitle);
  }
}

// Test that the prerender request from embedder to non-HTTP(S) scheme URL
// should fail because `PrerenderNavigationThrottle` discards the request. This
// is a regression test for https://crbug.com/1361210.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, EmbedderPrerenderToNonHttpUrl) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderUrl = GURL("file://example.txt");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(shell()->web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prerendering by embedder triggered prerendering.
  std::unique_ptr<PrerenderHandle> prerender_handle =
      AddEmbedderTriggeredPrerenderAsync(kPrerenderUrl);

  // Both the creation of PrerenderHandle and PrerenderHost should fail.
  EXPECT_FALSE(prerender_handle);
  EXPECT_TRUE(GetHostForUrl(kPrerenderUrl).is_null());
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kInvalidSchemeNavigation, 1);
}

// Ensures WebContents::OpenURL targeting a frame in a prerendered host will
// successfully navigate that frame.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, OpenURLInPrerenderingFrame) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_blank_iframe.html");
  const GURL kNewIframeUrl = GetUrl("/simple_page.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(shell()->web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prerendering `kPrerenderingUrl`.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  auto* prerendered_render_frame_host = GetPrerenderedMainFrameHost(host_id);
  auto* child_frame = ChildFrameAt(prerendered_render_frame_host, 0);
  ASSERT_TRUE(child_frame);

  // Navigate the iframe's FrameTreeNode in the prerendering frame tree. This
  // should successfully navigate.
  TestNavigationManager iframe_observer(shell()->web_contents(), kNewIframeUrl);
  shell()->web_contents()->OpenURL(
      OpenURLParams(
          kNewIframeUrl, Referrer(), child_frame->GetFrameTreeNodeId(),
          WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_AUTO_SUBFRAME,
          /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});
  ASSERT_TRUE(iframe_observer.WaitForNavigationFinished());
  EXPECT_TRUE(iframe_observer.was_committed());
  EXPECT_TRUE(iframe_observer.was_successful());
  EXPECT_EQ(child_frame->GetLastCommittedURL(), kNewIframeUrl);
}

// Ensure that WebContentsObserver::DidFailLoad is not invoked and cancels
// prerendering when invoked on the prerendering main frame.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, DidFailLoadCancelsPrerendering) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_iframe.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Initialize a MockWebContentsObserver and ensure that DidFailLoad is not
  // invoked inside prerender frame tree.
  testing::NiceMock<MockWebContentsObserver> observer(shell()->web_contents());
  EXPECT_CALL(observer, DidFailLoad(testing::_, testing::_, testing::_))
      .Times(0);

  // Start a prerender.
  FrameTreeNodeId prerender_host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerender_frame_host =
      GetPrerenderedMainFrameHost(prerender_host_id);

  // Trigger DidFailLoad, this should cancel prerendering.
  prerender_frame_host->DidFailLoadWithError(kPrerenderingUrl, net::ERR_FAILED);

  // The prerender host for the URL should be deleted as DidFailLoad cancels
  // prerendering.
  test::PrerenderHostObserver prerender_observer(*web_contents(),
                                                 kPrerenderingUrl);
  TestNavigationManager navigation_observer(shell()->web_contents(),
                                            kPrerenderingUrl);
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  // Now navigate the primary page to the prerendered URL. Cancelling the
  // prerender disables the activation due to DidFailLoad.
  ASSERT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                     JsReplace("location = $1", kPrerenderingUrl)));
  ASSERT_TRUE(navigation_observer.WaitForNavigationFinished());
  EXPECT_FALSE(prerender_observer.was_activated());

  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kDidFailLoad);
}

class DidFailLoadWebContentsObserver : public WebContentsObserver {
 public:
  explicit DidFailLoadWebContentsObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  bool WasDidFailLoadCalled() { return was_did_fail_load_called_; }
  int GetErrorCode() const { return error_code_; }
  const GURL& GetUrl() const { return url_; }

 private:
  void DidFailLoad(RenderFrameHost* rfh,
                   const GURL& url,
                   int error_code) override {
    was_did_fail_load_called_ = true;
    url_ = url;
    error_code_ = error_code;

    EXPECT_FALSE(rfh->IsErrorDocument());
    EXPECT_TRUE(rfh->IsInLifecycleState(
        RenderFrameHost::LifecycleState::kPrerendering));
  }

  bool was_did_fail_load_called_ = false;
  int error_code_ = net::OK;
  GURL url_;
};

// Ensure that RenderFrameHost::DidFailLoad on subframes don't cancel
// prerendering. This happens when JavaScript calls `window.stop()` in a
// frame, for instance.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DidFailLoadSubframesDoesNotCancelPrerendering) {
  DidFailLoadWebContentsObserver observer(web_contents());

  TestHostPrerenderingState(GetUrl("/page_with_stop_iframe.html"));

  EXPECT_TRUE(observer.WasDidFailLoadCalled());
  EXPECT_EQ(net::ERR_ABORTED, observer.GetErrorCode());
  EXPECT_EQ(GetUrl("/stop.html"), observer.GetUrl());
}

// Ensure that RenderFrameHost::DidFailLoad on the main frame cancels
// prerendering. This happens when JavaScript calls `window.stop()` in the
// main frame, for instance.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DidFailLoadMainFrameCancelsPrerendering) {
  DidFailLoadWebContentsObserver observer(web_contents());

  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/stop.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender and wait until it is canceled.
  test::PrerenderHostObserver host_observer(*web_contents_impl(),
                                            kPrerenderingUrl);
  AddPrerenderAsync(kPrerenderingUrl);
  host_observer.WaitForDestroyed();

  // DidFailLoad callback should not be called.
  EXPECT_FALSE(observer.WasDidFailLoadCalled());

  // Prerendering should be canceled for kDidFailLoad.
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kDidFailLoad);
}

// Ensures WebContents::OpenURL with a cross-origin URL targeting a frame in a
// prerendered host will successfully navigate that frame, though it should be
// deferred until activation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       OpenURLCrossOriginInPrerenderingFrame) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_blank_iframe.html");
  const GURL kNewIframeUrl = GetCrossSiteUrl("/simple_page.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(shell()->web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prerendering `kPrerenderingUrl`.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  auto* prerendered_render_frame_host = GetPrerenderedMainFrameHost(host_id);
  auto* child_frame = ChildFrameAt(prerendered_render_frame_host, 0);
  ASSERT_TRUE(child_frame);

  TestNavigationManager iframe_observer(shell()->web_contents(), kNewIframeUrl);

  // Navigate the iframe's FrameTreeNode in the prerendering frame tree. This
  // should successfully navigate but the navigation will be deferred until the
  // prerendering page is activated.
  {
    shell()->web_contents()->OpenURL(
        OpenURLParams(kNewIframeUrl, Referrer(),
                      child_frame->GetFrameTreeNodeId(),
                      WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_AUTO_SUBFRAME,
                      /*is_renderer_initiated=*/false),
        /*navigation_handle_callback=*/{});
    ASSERT_TRUE(iframe_observer.WaitForFirstYieldAfterDidStartNavigation());
    NavigationRequest* request =
        static_cast<NavigationRequest*>(iframe_observer.GetNavigationHandle());
    EXPECT_EQ(request->state(), NavigationRequest::WILL_START_REQUEST);
    EXPECT_TRUE(request->IsDeferredForTesting());
  }

  // Now navigate the primary page to the prerendered URL so that we activate
  // the prerender.
  {
    test::PrerenderHostObserver prerender_observer(*web_contents(),
                                                   kPrerenderingUrl);
    ASSERT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                       JsReplace("location = $1", kPrerenderingUrl)));
    prerender_observer.WaitForActivation();
  }

  // Now that we're activated, the iframe navigation should be able to finish.
  // Ensure the navigation completes in the iframe.
  {
    ASSERT_TRUE(iframe_observer.WaitForNavigationFinished());
    child_frame = ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
    ASSERT_TRUE(child_frame);
    EXPECT_EQ(child_frame->GetLastCommittedURL(), kNewIframeUrl);
  }
}

// Test that the main frame navigation after the initial prerender navigation
// when the activation has already started doesn't cancel an ongoing
// prerendering.
// Testing steps:
// 1. prerender navigation starts/finishes
// 2. activation starts and suspends on CommitDeferringCondition
// 3. navigation in the prerendered page starts
// 4. navigation in the prerendered page finishes
// 5. activation resumes/finishes
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MainFrameNavigationDuringActivation) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?1");
  const GURL kPrerenderingUrl2 = GetUrl("/empty.html?2");
  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  FrameTreeNodeId prerender_host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerendered_rfh =
      GetPrerenderedMainFrameHost(prerender_host_id);
  test::PrerenderHostObserver prerender_observer(*web_contents(),
                                                 prerender_host_id);
  auto* prerender_ftn = prerendered_rfh->frame_tree_node();
  EXPECT_FALSE(prerender_ftn->HasNavigation());

  // Start an activation navigation for the prerender and pause it before it
  // completes.
  TestActivationManager activation_observer(shell()->web_contents(),
                                            kPrerenderingUrl);
  {
    ASSERT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                       JsReplace("location = $1", kPrerenderingUrl)));

    // Pause the activation before it's committed.
    EXPECT_TRUE(activation_observer.WaitForBeforeChecks());
    EXPECT_TRUE(activation_observer.GetNavigationHandle()
                    ->IsCommitDeferringConditionDeferredForTesting());
    EXPECT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);
  }

  // Make a navigation in the prerendered page. This navigation should succeed.
  TestNavigationManager navigation_observer(web_contents(), kPrerenderingUrl2);
  NavigatePrerenderedPage(prerender_host_id, kPrerenderingUrl2);
  ASSERT_TRUE(navigation_observer.WaitForNavigationFinished());
  EXPECT_TRUE(navigation_observer.was_successful());

  // Verify that all RenderFrameHostImpls are the prerendering state.
  EXPECT_TRUE(prerender_helper()->VerifyPrerenderingState(kPrerenderingUrl));

  // The activation isn't cancelled because there is no ongoing navigation.
  activation_observer.ResumeActivation();

  // Wait for the completion of the navigation. This should be the prerendered
  // page activation.
  activation_observer.WaitForNavigationFinished();

  // The prerender host should have been consumed since the activation was
  // completed.
  EXPECT_FALSE(
      web_contents_impl()->GetPrerenderHostRegistry()->FindNonReservedHostById(
          prerender_host_id));
  EXPECT_FALSE(
      web_contents_impl()->GetPrerenderHostRegistry()->HasReservedHost());

  EXPECT_TRUE(activation_observer.was_activated());
  EXPECT_TRUE(activation_observer.was_successful());
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), kPrerenderingUrl2);
}

// Test that a main frame navigation after the initial prerender navigation
// doesn't cancel an ongoing prerendering. The main frame navigation runs
// concurrent with the activation.
// 1. prerender navigation starts/finishes
// 2. activation starts and suspends on CommitDeferringCondition
// 3. navigation in the prerendered page starts
// 4. activation resumes
// 5. navigation in the prerendered page finishes
// 6. activation finishes
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MainFrameNavigationConcurrentWithActivation) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?1");
  const GURL kPrerenderingUrl2 = GetUrl("/empty.html?2");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  FrameTreeNodeId prerender_host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerendered_rfh =
      GetPrerenderedMainFrameHost(prerender_host_id);
  test::PrerenderHostObserver prerender_observer(*web_contents(),
                                                 prerender_host_id);
  auto* prerender_ftn = prerendered_rfh->frame_tree_node();
  EXPECT_FALSE(prerender_ftn->HasNavigation());

  // Start an activation navigation for the prerender and pause it before it
  // completes.
  TestActivationManager activation_observer(shell()->web_contents(),
                                            kPrerenderingUrl);
  {
    ASSERT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                       JsReplace("location = $1", kPrerenderingUrl)));

    // Pause the activation before it's committed.
    EXPECT_TRUE(activation_observer.WaitForBeforeChecks());
    EXPECT_TRUE(activation_observer.GetNavigationHandle()
                    ->IsCommitDeferringConditionDeferredForTesting());
    EXPECT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);
  }

  // Make a navigation in the prerendered page. This navigation should succeed.
  TestNavigationManager navigation_observer(web_contents(), kPrerenderingUrl2);
  NavigatePrerenderedPage(prerender_host_id, kPrerenderingUrl2);

  // Resume an activation navigation before completing the navigation in the
  // prerendered page. The activation isn't cancelled because
  // PrerenderCommitDeferringCondition defers the activation until the ongoing
  // main frame navigation is completed.
  activation_observer.ResumeActivation();

  // Wait for the completion of the navigation in the prerendered page.
  ASSERT_TRUE(navigation_observer.WaitForNavigationFinished());
  EXPECT_TRUE(navigation_observer.was_successful());

  // Verify that all RenderFrameHostImpls are the prerendering state.
  EXPECT_TRUE(prerender_helper()->VerifyPrerenderingState(kPrerenderingUrl));

  // Wait for the completion of the navigation. This should be the prerendered
  // page activation.
  activation_observer.WaitForNavigationFinished();

  // The prerender host should have been consumed since the activation was
  // completed.
  EXPECT_FALSE(
      web_contents_impl()->GetPrerenderHostRegistry()->FindNonReservedHostById(
          prerender_host_id));
  EXPECT_FALSE(
      web_contents_impl()->GetPrerenderHostRegistry()->HasReservedHost());

  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl2);
  EXPECT_TRUE(activation_observer.was_activated());
  EXPECT_TRUE(activation_observer.was_successful());
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), kPrerenderingUrl2);
}

// Test that a main frame navigation after the initial prerender navigation and
// the activation is resumed cancels prerendering. This is the edge case that
// PrerenderCommitDeferringCondition posts a task to resume activation
// (https://source.chromium.org/chromium/chromium/src/+/main:content/browser/preloading/prerender/prerender_commit_deferring_condition.cc;l=105-106;drc=86ba45ef0be48fc81656da31dd4952857963485c)
// and a main frame navigation starts before activation is completed.
// 1. prerender navigation starts/finishes
// 2. activation starts and suspends on CommitDeferringCondition
// 3. navigation in the prerendered page starts
// 4. activation resumes
// 5. navigation in the prerendered page finishes
// 6. another navigation in the prerendered page starts but the server never
//    respond to the navigation
// 7. activation is canceled
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MainFrameNavigationAfterActivationIsResumed) {
  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url != "/empty.html?3") {
          return nullptr;
        }
        return std::make_unique<net::test_server::HungResponse>();
      }));
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerenderingUrl = embedded_test_server()->GetURL("/empty.html?1");
  const GURL kPrerenderingUrl2 =
      embedded_test_server()->GetURL("/empty.html?2");
  // The server returns a HungResponse to the request to kPrerenderingUrl3,
  // which doesn't actually respond until the server is destroyed.
  const GURL kPrerenderingUrl3 =
      embedded_test_server()->GetURL("/empty.html?3");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  FrameTreeNodeId prerender_host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerendered_rfh =
      GetPrerenderedMainFrameHost(prerender_host_id);
  test::PrerenderHostObserver prerender_observer(*web_contents(),
                                                 prerender_host_id);
  auto* prerender_ftn = prerendered_rfh->frame_tree_node();
  EXPECT_FALSE(prerender_ftn->HasNavigation());

  TestActivationManager activation_observer(shell()->web_contents(),
                                            kPrerenderingUrl);

  // Set a callback that will be called after the last commit deferring
  // condition is executed. The callback starts a main frame navigation in a
  // prerendered page after activation is resumed.
  activation_observer.SetCallbackCalledAfterActivationIsReady(base::BindOnce(
      &PrerenderBrowserTest::NavigatePrerenderedPage, base::Unretained(this),
      prerender_host_id, kPrerenderingUrl3));

  // Start an activation.
  {
    ASSERT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                       JsReplace("location = $1", kPrerenderingUrl)));

    // Pause the activation before it's committed.
    EXPECT_TRUE(activation_observer.WaitForBeforeChecks());
    EXPECT_TRUE(activation_observer.GetNavigationHandle()
                    ->IsCommitDeferringConditionDeferredForTesting());
    EXPECT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);
  }

  // Start a main frame navigation in a prerendered page. It defers the
  // activation commit.
  TestNavigationManager navigation_observer(web_contents(), kPrerenderingUrl2);
  NavigatePrerenderedPage(prerender_host_id, kPrerenderingUrl2);

  // Verify that all RenderFrameHostImpls are the prerendering state.
  EXPECT_TRUE(prerender_helper()->VerifyPrerenderingState(kPrerenderingUrl));

  // Resume an activation navigation before completing the navigation in the
  // prerendered page. The activation isn't cancelled because
  // PrerenderCommitDeferringCondition defers the activation until the ongoing
  // main frame navigation is completed.
  activation_observer.ResumeActivation();

  // Wait for the completion of the navigation in the prerendered page.
  ASSERT_TRUE(navigation_observer.WaitForNavigationFinished());
  EXPECT_TRUE(navigation_observer.was_successful());

  // Wait for the completion of the navigation. This shouldn't be the
  // prerendered page activation.
  activation_observer.WaitForNavigationFinished();

  // The prerender host should have been abandoned.
  EXPECT_FALSE(
      web_contents_impl()->GetPrerenderHostRegistry()->FindNonReservedHostById(
          prerender_host_id));
  EXPECT_FALSE(
      web_contents_impl()->GetPrerenderHostRegistry()->HasReservedHost());

  EXPECT_FALSE(activation_observer.was_activated());
  EXPECT_TRUE(activation_observer.was_successful());
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), kPrerenderingUrl);

  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kActivatedDuringMainFrameNavigation);
}

// Test the following scenario: a prerender initial navigation is pending and an
// activation navigation is deferred due to that, and then if prerender is
// canceled, the activation navigation will fall back to a normal navigation
// with no crash and hang.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       CancelPrerenderWhenDeferringActivationNavigation) {
  const char prerendering_url_c[] = "/empty.html?prerender";
  net::test_server::ControllableHttpResponse response_for_initial_navigation(
      embedded_test_server(), prerendering_url_c);
  net::test_server::ControllableHttpResponse response_for_activation_navigation(
      embedded_test_server(), prerendering_url_c);

  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  const GURL prerendering_url =
      embedded_test_server()->GetURL(prerendering_url_c);

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Start prerendering.
  content::test::PrerenderHostCreationWaiter host_creation_waiter;
  AddPrerenderAsync(prerendering_url);
  FrameTreeNodeId host_id = host_creation_waiter.Wait();

  response_for_initial_navigation.WaitForRequest();
  // Not sending the response so that the prerender initial navigation will be
  // pending.

  test::PrerenderHostObserver prerender_observer(*web_contents(), host_id);
  TestActivationManager activation_observer(web_contents(), prerendering_url);

  // Start prerender activation. This will be deferred because initial
  // navigation is not finished.
  test::PrerenderTestHelper::NavigatePrimaryPageAsync(*web_contents_impl(),
                                                      prerendering_url);
  NavigationRequest* request =
      web_contents_impl()->GetPrimaryFrameTree().root()->navigation_request();
  ASSERT_TRUE(activation_observer.WaitForBeforeChecks());
  activation_observer.ResumeActivation();
  ASSERT_TRUE(request->IsCommitDeferringConditionDeferredForTesting());
  ASSERT_EQ(request->state(), NavigationRequest::NOT_STARTED);

  // Cancel prerendering.
  CancelPrerenderedPage(host_id);
  prerender_observer.WaitForDestroyed();

  // Activation navigation will fall back to normal navigation.
  response_for_activation_navigation.WaitForRequest();
  response_for_activation_navigation.Send(net::HTTP_OK, "");
  response_for_activation_navigation.Done();
  activation_observer.WaitForNavigationFinished();
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), prerendering_url);
  EXPECT_FALSE(activation_observer.was_activated());
}

// Test that WebContentsObserver::DidFinishLoad is not invoked when the page
// gets loaded while prerendering but it is deferred and invoked on prerender
// activation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DidFinishLoadInvokedAfterActivation) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/simple_page.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Initialize a MockWebContentsObserver and ensure that DidFinishLoad is not
  // invoked while prerendering.
  testing::NiceMock<MockWebContentsObserver> observer(shell()->web_contents());
  EXPECT_CALL(observer, DidFinishLoad(testing::_, testing::_)).Times(0);

  // Start a prerender.
  FrameTreeNodeId prerender_host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerender_frame_host =
      GetPrerenderedMainFrameHost(prerender_host_id);
  EXPECT_EQ(0u, prerender_frame_host->child_count());

  // Verify and clear all expectations on the mock observer before setting new
  // ones.
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::InSequence s;

  // Activate the prerendered page. This should result in invoking DidFinishLoad
  // once for root RenderFrameHost `prerender_frame_host`.
  {
    // Verify that DidFinishNavigation is invoked before DidFinishLoad on
    // activation.
    EXPECT_CALL(observer, DidFinishNavigation(testing::_));

    EXPECT_CALL(observer,
                DidFinishLoad(prerender_frame_host, kPrerenderingUrl));
  }
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
}

// Test that WebContentsObserver::DidFinishLoad is not invoked when the page
// gets loaded while prerendering but it is deferred and invoked on prerender
// activation for both main and sub-frames.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DidFinishLoadInvokedAfterActivationWithSubframes) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_iframe.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Initialize a MockWebContentsObserver and ensure that DidFinishLoad is not
  // invoked while prerendering.
  testing::NiceMock<MockWebContentsObserver> observer(shell()->web_contents());
  testing::InSequence s;
  EXPECT_CALL(observer, DidFinishLoad(testing::_, testing::_)).Times(0);

  // Start a prerender.
  FrameTreeNodeId prerender_host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerender_main_frame_host =
      GetPrerenderedMainFrameHost(prerender_host_id);
  RenderFrameHost* child_frame = ChildFrameAt(prerender_main_frame_host, 0);
  EXPECT_EQ(1u, prerender_main_frame_host->child_count());

  // Verify and clear all expectations on the mock observer before setting new
  // ones.
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Activate the prerendered page. This should result in invoking DidFinishLoad
  // twice once for root and once for child RenderFrameHosts.
  {
    // Verify that DidFinishNavigation is invoked before DidFinishLoad.
    EXPECT_CALL(observer, DidFinishNavigation(testing::_));

    EXPECT_CALL(observer,
                DidFinishLoad(prerender_main_frame_host, kPrerenderingUrl));

    EXPECT_CALL(observer,
                DidFinishLoad(child_frame, child_frame->GetLastCommittedURL()));
  }
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
}

// Test that WebContentsObserver::DOMContentLoaded is not invoked while
// prerendering but it is deferred and invoked on prerender activation for both
// main and sub-frames.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DOMContentLoadedInvokedAfterActivationWithSubframes) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_iframe.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Initialize a MockWebContentsObserver and ensure that DOMContentLoaded is
  // not invoked while prerendering.
  testing::NiceMock<MockWebContentsObserver> observer(shell()->web_contents());
  EXPECT_CALL(observer, DOMContentLoaded(testing::_)).Times(0);

  // Start a prerender.
  FrameTreeNodeId prerender_host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerender_main_frame_host =
      GetPrerenderedMainFrameHost(prerender_host_id);
  RenderFrameHost* child_frame = ChildFrameAt(prerender_main_frame_host, 0);
  EXPECT_EQ(prerender_main_frame_host->child_count(), 1u);
  ASSERT_TRUE(prerender_host_id);

  // Verify and clear all expectations on the mock observer before setting new
  // ones.
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::InSequence s;

  // Activate the prerendered page. This should result in invoking
  // DOMContentLoaded twice once for root and once for child RenderFrameHost.
  {
    // Verify that DidFinishNavigation is invoked before DOMContentLoaded on
    // activation.
    EXPECT_CALL(observer, DidFinishNavigation(testing::_));

    EXPECT_CALL(observer, DOMContentLoaded(prerender_main_frame_host)).Times(1);

    EXPECT_CALL(observer, DOMContentLoaded(child_frame)).Times(1);
  }
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
}

// Test that WebContentsObserver::DocumentOnLoadCompletedInPrimaryMainFrame is
// not invoked when the page gets loaded while prerendering but it is deferred
// and invoked on prerender activation.
IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    DocumentOnLoadCompletedInPrimaryMainFrameInvokedAfterActivation) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_iframe.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Initialize a MockWebContentsObserver and ensure that
  // DocumentOnLoadCompletedInPrimaryMainFrame is not invoked while
  // prerendering.
  testing::NiceMock<MockWebContentsObserver> observer(shell()->web_contents());
  EXPECT_CALL(observer, DocumentOnLoadCompletedInPrimaryMainFrame()).Times(0);

  // Start a prerender.
  FrameTreeNodeId prerender_host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerender_frame_host =
      GetPrerenderedMainFrameHost(prerender_host_id);
  EXPECT_EQ(prerender_frame_host->child_count(), 1u);
  ASSERT_TRUE(prerender_host_id);

  // Verify and clear all expectations on the mock observer before setting new
  // ones.
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::InSequence s;

  // Activate the prerendered page. This should result in invoking
  // DocumentOnLoadCompletedInPrimaryMainFrame only for main RenderFrameHost.
  {
    // Verify that DidFinishNavigation is invoked before
    // DocumentOnLoadCompletedInPrimaryMainFrame on activation.
    EXPECT_CALL(observer, DidFinishNavigation(testing::_));

    EXPECT_CALL(observer, DocumentOnLoadCompletedInPrimaryMainFrame()).Times(1);
  }
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
}

// Test that WebContentsObserver::PrimaryMainDocumentElementAvailable is not
// invoked when the page gets loaded while prerendering but it is deferred and
// invoked on prerender activation.
IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    PrimaryMainDocumentElementAvailableInvokedAfterActivation) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_iframe.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Initialize a MockWebContentsObserver and ensure that
  // PrimaryMainDocumentElementAvailable is not invoked while prerendering.
  testing::NiceMock<MockWebContentsObserver> observer(shell()->web_contents());
  EXPECT_CALL(observer, PrimaryMainDocumentElementAvailable()).Times(0);

  // AddPrerender() below waits until WebContentsObserver::DidStopLoading() is
  // called and RenderFrameHostImpl::PrimaryMainDocumentElementAvailable() call
  // is expected before it returns.
  FrameTreeNodeId prerender_host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerender_frame_host =
      GetPrerenderedMainFrameHost(prerender_host_id);
  EXPECT_EQ(prerender_frame_host->child_count(), 1u);
  ASSERT_TRUE(prerender_host_id);

  // Verify and clear all expectations on the mock observer before setting new
  // ones.
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::InSequence s;

  // Activate the prerendered page. This should result in invoking
  // PrimaryMainDocumentElementAvailable only for main RenderFrameHost.
  // Verify that DidFinishNavigation is invoked before
  // PrimaryMainDocumentElementAvailable on activation.
  EXPECT_CALL(observer, DidFinishNavigation(testing::_));

  EXPECT_CALL(observer, PrimaryMainDocumentElementAvailable()).Times(1);
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
}

// Test that WebContentsObserver::LoadProgressChanged is not invoked when the
// page gets loaded while prerendering but is invoked on prerender activation.
// Check that LoadProgressChanged is only called once for
// blink::kFinalLoadProgress if the prerender page completes loading on
// activation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       LoadProgressChangedInvokedOnActivation) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/simple_page.html");

  web_contents_impl()->set_minimum_delay_between_loading_updates_for_testing(
      base::Milliseconds(0));

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Initialize a MockWebContentsObserver and ensure that LoadProgressChanged is
  // not invoked while prerendering.
  testing::NiceMock<MockWebContentsObserver> observer(shell()->web_contents());
  testing::InSequence s;
  EXPECT_CALL(observer, LoadProgressChanged(testing::_)).Times(0);

  // Start a prerender.
  FrameTreeNodeId prerender_host_id = AddPrerender(kPrerenderingUrl);
  ASSERT_TRUE(prerender_host_id);
  RenderFrameHostImpl* prerender_frame_host =
      GetPrerenderedMainFrameHost(prerender_host_id);

  // Verify and clear all expectations on the mock observer before setting new
  // ones.
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Activate the prerendered page. This should result in invoking
  // LoadProgressChanged for the following cases:
  {
    // 1) During DidStartLoading LoadProgressChanged is invoked with
    // kInitialLoadProgress value.
    EXPECT_CALL(observer, LoadProgressChanged(blink::kInitialLoadProgress));

    // Verify that DidFinishNavigation is invoked before final load progress
    // notification.
    EXPECT_CALL(observer, DidFinishNavigation(testing::_));

    // 2) During DidStopLoading LoadProgressChanged is invoked with
    // kFinalLoadProgress.
    EXPECT_CALL(observer, LoadProgressChanged(blink::kFinalLoadProgress))
        .Times(1);
  }

  // Set the prerender load progress value to blink::kFinalLoadProgress, this
  // should result in invoking LoadProgressChanged(blink::kFinalLoadProgress)
  // only once on activation during call to DidStopLoading.
  prerender_frame_host->GetPage().set_load_progress(blink::kFinalLoadProgress);
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
}

// Test the dispatch order of various load events on prerender activation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, OrderingOfDifferentLoadEvents) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/simple_page.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Initialize a MockWebContentsObserver to check order of different load
  // events.
  testing::NiceMock<MockWebContentsObserver> observer(shell()->web_contents());

  // Start a prerender.
  FrameTreeNodeId prerender_host_id = AddPrerender(kPrerenderingUrl);
  ASSERT_TRUE(prerender_host_id);

  // Verify and clear all expectations on the mock observer before setting new
  // ones.
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::InSequence s;

  // Activate the prerendered page. This should result in invoking various
  // WebContentsObserver events in the following order.
  {
    EXPECT_CALL(observer, DidStartLoading()).Times(1);

    // Verify that DidFinishNavigation is invoked before any finish load events
    // are dispatched.
    EXPECT_CALL(observer, DidFinishNavigation(testing::_)).Times(1);

    EXPECT_CALL(observer, DOMContentLoaded(testing::_)).Times(1);

    EXPECT_CALL(observer, DocumentOnLoadCompletedInPrimaryMainFrame()).Times(1);

    EXPECT_CALL(observer, DidFinishLoad(testing::_, testing::_)).Times(1);

    EXPECT_CALL(observer, DidStopLoading()).Times(1);
  }
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
}

// Tests that cross-origin subframe navigations in a prerendered page are
// deferred even if they start after the a navigation starts that will
// attempt to activate the prerendered page.
//
// Regression test for https://crbug.com/1190262.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       CrossOriginSubframeNavigationDuringActivation) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_blank_iframe.html");
  const GURL kCrossOriginUrl = GetCrossSiteUrl("/simple_page.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(shell()->web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prerendering `kPrerenderingUrl`.
  FrameTreeNodeId prerender_host_id;
  RenderFrameHost* prerender_main_frame = nullptr;
  {
    prerender_host_id = AddPrerender(kPrerenderingUrl);
    prerender_main_frame = GetPrerenderedMainFrameHost(prerender_host_id);
    RenderFrameHost* child_frame = ChildFrameAt(prerender_main_frame, 0);
    ASSERT_TRUE(child_frame);
  }

  // Start an activation navigation for the prerender. Pause activation before
  // it completes.
  test::PrerenderHostObserver prerender_observer(*web_contents(),
                                                 kPrerenderingUrl);
  TestActivationManager activation_observer(shell()->web_contents(),
                                            kPrerenderingUrl);
  {
    ASSERT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                       JsReplace("location = $1", kPrerenderingUrl)));

    EXPECT_TRUE(activation_observer.WaitForBeforeChecks());
    EXPECT_TRUE(activation_observer.GetNavigationHandle()
                    ->IsCommitDeferringConditionDeferredForTesting());
    EXPECT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);
  }

  // Start a cross-origin subframe navigation in the prerendered page. It
  // should be deferred.
  std::string kNavigateScript = R"(
    document.querySelector('iframe').src = $1;
  )";
  TestNavigationManager iframe_nav_observer(shell()->web_contents(),
                                            kCrossOriginUrl);
  ASSERT_TRUE(ExecJs(prerender_main_frame,
                     JsReplace(kNavigateScript, kCrossOriginUrl)));

  ASSERT_TRUE(iframe_nav_observer.WaitForFirstYieldAfterDidStartNavigation());

  // The PrerenderSubframeNavigationThrottle should defer it until activation.
  auto* child_ftn =
      FrameTreeNode::GloballyFindByID(prerender_host_id)->child_at(0);
  auto* child_navigation = child_ftn->navigation_request();
  ASSERT_NE(child_navigation, nullptr);
  EXPECT_TRUE(child_navigation->IsDeferredForTesting());

  // Allow the activation navigation to complete.
  activation_observer.WaitForNavigationFinished();
  EXPECT_TRUE(activation_observer.was_activated());

  // The iframe navigation should finish.
  ASSERT_TRUE(iframe_nav_observer.WaitForNavigationFinished());
  EXPECT_EQ(ChildFrameAt(prerender_main_frame, 0)->GetLastCommittedURL(),
            kCrossOriginUrl);
}

// Tests WebContents::OpenURL to a frame in a prerendered page when a
// navigation that will attempt to activate the page has already started. The
// subframe navigation should succeed.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       OpenURLInSubframeDuringActivation) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_blank_iframe.html");
  const GURL kNewIframeUrl = GetUrl("/simple_page.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(shell()->web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prerendering `kPrerenderingUrl`.
  FrameTreeNodeId prerender_host_id;
  RenderFrameHost* child_frame = nullptr;
  {
    prerender_host_id = AddPrerender(kPrerenderingUrl);
    auto* prerendered_render_frame_host =
        GetPrerenderedMainFrameHost(prerender_host_id);
    child_frame = ChildFrameAt(prerendered_render_frame_host, 0);
    ASSERT_TRUE(child_frame);
  }

  // Start an activation navigation for the prerender and pause before it
  // completes.
  test::PrerenderHostObserver prerender_observer(*web_contents(),
                                                 kPrerenderingUrl);
  TestActivationManager activation_observer(shell()->web_contents(),
                                            kPrerenderingUrl);
  {
    ASSERT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                       JsReplace("location = $1", kPrerenderingUrl)));

    EXPECT_TRUE(activation_observer.WaitForBeforeChecks());
    EXPECT_TRUE(activation_observer.GetNavigationHandle()
                    ->IsCommitDeferringConditionDeferredForTesting());
    EXPECT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);
  }

  // Use the OpenURL API to navigate the iframe in the prerendering frame tree.
  // This navigation should succeed.
  {
    TestNavigationManager iframe_observer(shell()->web_contents(),
                                          kNewIframeUrl);
    shell()->web_contents()->OpenURL(
        OpenURLParams(kNewIframeUrl, Referrer(),
                      child_frame->GetFrameTreeNodeId(),
                      WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_AUTO_SUBFRAME,
                      /*is_renderer_initiated=*/false),
        /*navigation_handle_callback=*/{});
    ASSERT_TRUE(iframe_observer.WaitForNavigationFinished());
    EXPECT_EQ(child_frame->GetLastCommittedURL(), kNewIframeUrl);
  }

  // Allow the activation navigation to complete.
  activation_observer.WaitForNavigationFinished();
  EXPECT_TRUE(activation_observer.was_activated());
}

// Tests that loading=lazy doesn't prevent image load in a prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, LazyLoading) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/prerender/image_loading_lazy.html");
  const GURL kImageUrl = GetUrl("/blank.jpg");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(shell()->web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prerendering `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  // A request for the image in the prerendered page shouldn't be prevented by
  // loading=lazy.
  EXPECT_EQ(GetRequestCount(kImageUrl), 1);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       SessionStorageAfterBackNavigation_NoProcessReuse) {
  // When BackForwardCache feature is enabled, this test doesn't work, because
  // this test is checking the behavior of a new renderer process which is
  // created for a back forward navigation from a prerendered page.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  const GURL kInitialUrl = GetUrl("/prerender/session_storage.html");
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/session_storage.html?prerendering=");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  std::unique_ptr<RenderProcessHostWatcher> process_host_watcher =
      std::make_unique<RenderProcessHostWatcher>(
          current_frame_host()->GetProcess(),
          RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);

  AddPrerender(kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);

  EXPECT_EQ("initial", EvalJs(current_frame_host(),
                              "window.sessionKeysInPrerenderingchange")
                           .ExtractString());
  EXPECT_EQ(
      "activated, initial",
      EvalJs(current_frame_host(), "getSessionStorageKeys()").ExtractString());

  // Speculative fix for the test flakiness (crbug.com/1216038), which may be
  // caused by the delayed async IPC of Session Storage (StorageArea.Put()).
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     "new Promise(resolve => requestIdleCallback(resolve));"));

  // Make sure that the initial renderer process is destroyed. So that the
  // initial renderer process will not be reused after the back forward
  // navigation below.
  process_host_watcher->Wait();

  // Navigate back to the initial page.
  TestNavigationObserver observer(shell()->web_contents());
  shell()->GoBackOrForward(-1);
  observer.Wait();
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), kInitialUrl);

  EXPECT_EQ(
      "activated, initial",
      EvalJs(current_frame_host(), "getSessionStorageKeys()").ExtractString());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       SessionStorageAfterBackNavigation_KeepInitialProcess) {
  const GURL kInitialUrl = GetUrl("/prerender/session_storage.html");
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/session_storage.html?prerendering=");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  RenderProcessHostImpl* initial_process_host =
      static_cast<RenderProcessHostImpl*>(current_frame_host()->GetProcess());
  // Increment the keep alive ref count of the renderer process to keep it alive
  // so it is reused on the back navigation below. The test checks that the
  // session storage state changed in the activated page is correctly propagated
  // after a back navigation that uses an existing renderer process.
  initial_process_host->IncrementKeepAliveRefCount(0);

  AddPrerender(kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);

  EXPECT_EQ("initial", EvalJs(current_frame_host(),
                              "window.sessionKeysInPrerenderingchange")
                           .ExtractString());
  EXPECT_EQ(
      "activated, initial",
      EvalJs(current_frame_host(), "getSessionStorageKeys()").ExtractString());

  // Speculative fix for the test flakiness (crbug.com/1216038), which may be
  // caused by the delayed async IPC of Session Storage (StorageArea.Put()).
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     "new Promise(resolve => requestIdleCallback(resolve));"));

  // Navigate back to the initial page.
  TestNavigationObserver observer(shell()->web_contents());
  shell()->GoBackOrForward(-1);
  observer.Wait();
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), kInitialUrl);

  EXPECT_EQ(
      "activated, initial",
      EvalJs(current_frame_host(), "getSessionStorageKeys()").ExtractString());
}

// Tests that prerender whose target_hint is "_blank" is using the same session
// storage across prerender navigations, and the initiator doesn't share the
// same storage.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       SessionStorage_TargetBlank_WithTargetHintBlank) {
  const GURL kInitialUrl = GetUrl("/prerender/session_storage.html");
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/session_storage.html?prerendering=");

  // Navigate to an initial page which has a link to `kPrerenderingUrl`.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl`.
  FrameTreeNodeId host_id = prerender_helper()->AddPrerender(
      kPrerenderingUrl, /*eagerness=*/std::nullopt, "_blank");
  auto* prerender_web_contents =
      static_cast<WebContentsImpl*>(WebContents::FromFrameTreeNodeId(host_id));
  ExpectWebContentsIsForNewTabPrerendering(*prerender_web_contents);
  auto* initiator_web_contents = web_contents_impl();
  ASSERT_NE(prerender_web_contents, initiator_web_contents);

  std::string prerender_session_storage_id_before_activation =
      FrameTreeNode::GloballyFindByID(host_id)
          ->frame_tree()
          .controller()
          .GetSessionStorageNamespace(prerender_web_contents->GetSiteInstance()
                                          ->GetStoragePartitionConfig())
          ->id();
  EXPECT_EQ(
      "prerendering",
      EvalJs(content::test::PrerenderTestHelper::GetPrerenderedMainFrameHost(
                 *prerender_web_contents, host_id),
             "getSessionStorageKeys()")
          .ExtractString());

  // Click the link annotated with "target=_blank". This should activate the
  // prerendered page.
  TestNavigationObserver activation_observer(kPrerenderingUrl);
  activation_observer.WatchExistingWebContents();
  test::PrerenderHostObserver prerender_observer(*prerender_web_contents,
                                                 host_id);
  const std::string kLinkClickScript = R"(
      clickSameSiteNewWindowLink();
  )";
  EXPECT_TRUE(ExecJs(web_contents(), kLinkClickScript));
  activation_observer.WaitForNavigationFinished();
  EXPECT_EQ(prerender_web_contents->GetLastCommittedURL(), kPrerenderingUrl);
  EXPECT_EQ(activation_observer.last_navigation_url(), kPrerenderingUrl);
  EXPECT_TRUE(prerender_observer.was_activated());
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  // The initiator's session storage is expected to be different from the
  // prerender's.
  EXPECT_EQ("initial", EvalJs(initiator_web_contents->GetPrimaryMainFrame(),
                              "getSessionStorageKeys()")
                           .ExtractString());
  EXPECT_EQ("activated", EvalJs(prerender_web_contents->GetPrimaryMainFrame(),
                                "getSessionStorageKeys()")
                             .ExtractString());

  // The prerender session storage is expected to be the same across prerender
  // activation.
  EXPECT_EQ(
      prerender_session_storage_id_before_activation,
      prerender_web_contents->GetPrimaryFrameTree()
          .controller()
          .GetSessionStorageNamespace(prerender_web_contents->GetSiteInstance()
                                          ->GetStoragePartitionConfig())
          ->id());
  EXPECT_NE(
      prerender_session_storage_id_before_activation,
      initiator_web_contents->GetPrimaryFrameTree()
          .controller()
          .GetSessionStorageNamespace(prerender_web_contents->GetSiteInstance()
                                          ->GetStoragePartitionConfig())
          ->id());

  // The navigation occurred in a new WebContents, so the original WebContents
  // should still be showing the initial trigger page.
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);
}

// Test if the host is abandoned when the renderer page crashes.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, AbandonIfRendererProcessCrashes) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);

  // Crash the relevant renderer.
  {
    test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);
    RenderProcessHost* process =
        GetPrerenderedMainFrameHost(host_id)->GetProcess();
    ScopedAllowRendererCrashes allow_renderer_crashes(process);
    process->ForceCrash();
    host_observer.WaitForDestroyed();
  }
  ExpectFinalStatusForSpeculationRule(
#if BUILDFLAG(IS_ANDROID)
      PrerenderFinalStatus::kRendererProcessKilled);
#else
      PrerenderFinalStatus::kRendererProcessCrashed);
#endif  // BUILDFLAG(IS_ANDROID)
}

// Test if the host is abandoned when the renderer page is killed.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, AbandonIfRendererProcessIsKilled) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);

  // Shut down the relevant renderer.
  {
    test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);
    RenderProcessHost* process =
        GetPrerenderedMainFrameHost(host_id)->GetProcess();
    ScopedAllowRendererCrashes allow_renderer_crashes(process);
    EXPECT_TRUE(process->Shutdown(0));
    host_observer.WaitForDestroyed();
  }

  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kRendererProcessKilled);
}

// Test if the host is abandoned when the primary main page that triggers a
// prerendering is killed.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       AbandonIfPrimaryMainFrameRendererProcessIsKilled) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);

  // Shut down the current renderer.
  {
    test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);
    RenderProcessHost* process = current_frame_host()->GetProcess();
    ScopedAllowRendererCrashes allow_renderer_crashes(process);
    EXPECT_TRUE(process->Shutdown(0));
    host_observer.WaitForDestroyed();
  }

  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kPrimaryMainFrameRendererProcessKilled);
}

class PrerenderBackForwardCacheBrowserTest : public PrerenderBrowserTest {
 public:
  PrerenderBackForwardCacheBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{::features::kBackForwardCache, {}},
         {kBackForwardCacheNoTimeEviction, {}}},
        // Allow BackForwardCache for all devices regardless of their memory.
        {::features::kBackForwardCacheMemoryControls});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrerenderBackForwardCacheBrowserTest,
                       SessionStorageAfterBackNavigation) {
  const GURL kInitialUrl = GetUrl("/prerender/session_storage.html");
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/session_storage.html?prerendering=");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  RenderFrameHostWrapper main_frame(
      shell()->web_contents()->GetPrimaryMainFrame());

  AddPrerender(kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);

  EXPECT_EQ("initial", EvalJs(current_frame_host(),
                              "window.sessionKeysInPrerenderingchange")
                           .ExtractString());
  EXPECT_EQ(
      "activated, initial",
      EvalJs(current_frame_host(), "getSessionStorageKeys()").ExtractString());

  // Speculative fix for the test flakiness (crbug.com/1216038), which may be
  // caused by the delayed async IPC of Session Storage (StorageArea.Put()).
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     "new Promise(resolve => requestIdleCallback(resolve));"));

  // Navigate back to the initial page.
  shell()->GoBackOrForward(-1);
  WaitForLoadStop(shell()->web_contents());

  // Expect the navigation to be served from the back-forward cache to verify
  // the test is testing what is intended.
  ASSERT_EQ(shell()->web_contents()->GetPrimaryMainFrame(), main_frame.get());

  EXPECT_EQ(
      "activated, initial",
      EvalJs(current_frame_host(), "getSessionStorageKeys()").ExtractString());
}

#if !BUILDFLAG(IS_ANDROID)
// The out-of-process StorageService is not implemented on Android. Also as
// commented below, test_api->CrashNow() won't work on x86 and x86_64 Android.

class PrerenderRestartStorageServiceBrowserTest : public PrerenderBrowserTest {
 public:
  PrerenderRestartStorageServiceBrowserTest() = default;

 protected:
  void CrashStorageServiceAndWaitForRestart() {
    mojo::Remote<storage::mojom::StorageService>& service =
        StoragePartitionImpl::GetStorageServiceForTesting();
    base::RunLoop loop;
    service.set_disconnect_handler(base::BindLambdaForTesting([&] {
      loop.Quit();
      service.reset();
    }));
    mojo::Remote<storage::mojom::TestApi> test_api;
    StoragePartitionImpl::GetStorageServiceForTesting()->BindTestApi(
        test_api.BindNewPipeAndPassReceiver().PassPipe());
    // On x86 and x86_64 Android, base::ImmediateCrash() macro used in
    // CrashNow() does not seem to work as expected. (See
    // https://crbug.com/1211655)
    test_api->CrashNow();
    loop.Run();
  }
};

IN_PROC_BROWSER_TEST_F(PrerenderRestartStorageServiceBrowserTest,
                       RestartStorageServiceBeforePrerendering) {
  const GURL kInitialUrl = GetUrl("/prerender/session_storage.html");
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/session_storage.html?prerendering=");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  CrashStorageServiceAndWaitForRestart();

  EXPECT_EQ(
      "initial",
      EvalJs(current_frame_host(), "getSessionStorageKeys()").ExtractString());

  AddPrerender(kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);

  EXPECT_EQ("initial", EvalJs(current_frame_host(),
                              "window.sessionKeysInPrerenderingchange")
                           .ExtractString());
  EXPECT_EQ(
      "activated, initial",
      EvalJs(current_frame_host(), "getSessionStorageKeys()").ExtractString());
}

IN_PROC_BROWSER_TEST_F(PrerenderRestartStorageServiceBrowserTest,
                       RestartStorageServiceWhilePrerendering) {
  const GURL kInitialUrl = GetUrl("/prerender/session_storage.html");
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/session_storage.html?prerendering=");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);

  CrashStorageServiceAndWaitForRestart();

  EXPECT_EQ(
      "initial",
      EvalJs(current_frame_host(), "getSessionStorageKeys()").ExtractString());
  EXPECT_EQ(
      "initial, prerendering",
      EvalJs(GetPrerenderedMainFrameHost(host_id), "getSessionStorageKeys()")
          .ExtractString());

  NavigatePrimaryPage(kPrerenderingUrl);

  EXPECT_EQ("initial", EvalJs(current_frame_host(),
                              "window.sessionKeysInPrerenderingchange")
                           .ExtractString());
  EXPECT_EQ(
      "activated, initial",
      EvalJs(current_frame_host(), "getSessionStorageKeys()").ExtractString());
}
#endif

// Make sure that we can deal with the speculative RFH that is created during
// the activation navigation.
// TODO(crbug.com/40174053): We should try to avoid creating the
// speculative RFH (redirects allowing). Once that is done we should either
// change this test (if redirects allowed) or remove it completely.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, SpeculationRulesScript) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  // A prerender host for the URL should be registered.
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl));

  // Activate the prerendered page.
  // The test passes if we don't crash while cleaning up speculative render
  // frame host.
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);

  // The prerender host should be consumed.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  // Activating the prerendered page should not issue a request.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
}

class PrerenderEagernessBrowserTest : public PrerenderBrowserTest {
 public:
  void SetUp() override {
#if !BUILDFLAG(IS_ANDROID)
    PrerenderBrowserTest::SetUp();
#else
    // TODO(crbug.com/40269669): Add the implementation of pointer interaction
    // on Android to the function below.
    GTEST_SKIP();
#endif  // BUILDFLAG(IS_ANDROID)
  }

  void InsertAnchor(const GURL url) {
    const std::string script = R"(
      const anchor = document.createElement('a');
      anchor.href = $1;
      anchor.text = $1;
      document.body.appendChild(anchor);
    )";
    ASSERT_TRUE(ExecJs(web_contents(), JsReplace(script, url.spec())));
  }

  void ResetPointerPosition() {
#if !BUILDFLAG(IS_ANDROID)
    InputEventAckWaiter waiter(
        web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost(),
        blink::WebInputEvent::Type::kMouseMove);
    SimulateMouseEvent(web_contents(), blink::WebMouseEvent::Type::kMouseMove,
                       blink::WebMouseEvent::Button::kNoButton,
                       gfx::Point(0, 0));
    waiter.Wait();
#else
    // TODO(crbug.com/40269669): Simulate |WebGestureEvent| to make this
    // function work for Android.
#endif  // !BUILDFLAG(IS_ANDROID)
  }

  void PointerHoverToAnchor(const GURL& url) {
    ResetPointerPosition();

#if !BUILDFLAG(IS_ANDROID)
    const auto point = CalculateCenterPointOfAnchorElement(url);
    InputEventAckWaiter waiter(
        web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost(),
        blink::WebInputEvent::Type::kMouseMove);
    SimulateMouseEvent(web_contents(), blink::WebMouseEvent::Type::kMouseMove,
                       blink::WebMouseEvent::Button::kNoButton, point);
    waiter.Wait();
#else
    // TODO(crbug.com/40269669): Simulate |WebGestureEvent| to make this
    // function work for Android.
#endif  // !BUILDFLAG(IS_ANDROID)
  }

  void PointerDownToAnchor(const GURL& url) {
    ResetPointerPosition();

#if !BUILDFLAG(IS_ANDROID)
    const auto point = CalculateCenterPointOfAnchorElement(url);
    InputEventAckWaiter waiter(
        web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost(),
        blink::WebInputEvent::Type::kMouseDown);
    SimulateMouseEventForClick(blink::WebMouseEvent::Type::kMouseDown,
                               blink::WebMouseEvent::Button::kLeft, point);
    waiter.Wait();
#else
    // TODO(crbug.com/40269669): Simulate |WebGestureEvent| to make this
    // function work for Android.
#endif  // !BUILDFLAG(IS_ANDROID)
  }

  void PointerUpToAnchor(const GURL& url) {
#if !BUILDFLAG(IS_ANDROID)
    const auto point = CalculateCenterPointOfAnchorElement(url);
    InputEventAckWaiter waiter(
        web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost(),
        blink::WebInputEvent::Type::kMouseUp);
    SimulateMouseEventForClick(blink::WebMouseEvent::Type::kMouseUp,
                               blink::WebMouseEvent::Button::kLeft, point);
    waiter.Wait();
#else
    // TODO(crbug.com/40269669): Simulate |WebGestureEvent| to make this
    // function work for Android.
#endif  // !BUILDFLAG(IS_ANDROID)
  }

  void ClickAnchor(const GURL url) {
    PointerDownToAnchor(url);
    PointerUpToAnchor(url);
  }

 private:
  void SimulateMouseEventForClick(blink::WebInputEvent::Type type,
                                  blink::WebMouseEvent::Button button,
                                  const gfx::Point& point) {
    auto* web_contents_impl = static_cast<WebContentsImpl*>(web_contents());
    auto* rwhvb = static_cast<RenderWidgetHostViewBase*>(
        web_contents()->GetRenderWidgetHostView());
    blink::WebMouseEvent mouse_event(type, 0, ui::EventTimeForNow());
    mouse_event.button = button;
    mouse_event.SetPositionInWidget(point.x(), point.y());
    // Mac needs positionInScreen for events to plugins.
    gfx::Rect offset = web_contents()->GetContainerBounds();
    mouse_event.SetPositionInScreen(point.x() + offset.x(),
                                    point.y() + offset.y());
    mouse_event.click_count = 1;

    web_contents_impl->GetInputEventRouter()->RouteMouseEvent(
        rwhvb, &mouse_event, ui::LatencyInfo());
  }

  gfx::Point CalculateCenterPointOfAnchorElement(const GURL& url) {
    const std::string script_get_x = R"(
      const anchor = document.querySelector('a[href=$1]');
      const bounds = anchor.getBoundingClientRect();
      Math.floor(bounds.left + bounds.width / 2);
    )";

    const std::string script_get_y = R"(
      const anchor = document.querySelector('a[href=$1]');
      const bounds = anchor.getBoundingClientRect();
      Math.floor(bounds.top + bounds.height / 2);
    )";

    const float x = EvalJs(web_contents(), JsReplace(script_get_x, url.spec()))
                        .ExtractDouble();
    const float y = EvalJs(web_contents(), JsReplace(script_get_y, url.spec()))
                        .ExtractDouble();

    return gfx::ToFlooredPoint(gfx::PointF(x, y));
  }
};

namespace {

class PreloadingDeciderObserverForPrerenderTesting
    : public PreloadingDeciderObserverForTesting {
 public:
  explicit PreloadingDeciderObserverForPrerenderTesting(
      RenderFrameHostImpl* rfh)
      : rfh_(rfh) {
    auto* preloading_decider =
        PreloadingDecider::GetOrCreateForCurrentDocument(rfh_);
    old_observer_ = preloading_decider->SetObserverForTesting(this);
    events_called_.fill(false);
  }
  ~PreloadingDeciderObserverForPrerenderTesting() override {
    auto* preloading_decider =
        PreloadingDecider::GetOrCreateForCurrentDocument(rfh_);
    EXPECT_EQ(this, preloading_decider->SetObserverForTesting(old_observer_));
  }

  void UpdateSpeculationCandidates(
      const std::vector<blink::mojom::SpeculationCandidatePtr>& candidates)
      override {
    OnEventCalled(Events::kUpdateSpeculationCandidates);
  }

  void OnPointerHover(const GURL& url) override {
    OnEventCalled(Events::kOnPointerHover);
  }

  void OnPointerDown(const GURL& url) override {
    OnEventCalled(Events::kOnPointerDown);
  }

  void WaitUpdateSpeculationCandidates() {
    WaitEvent(Events::kUpdateSpeculationCandidates);
  }

  void WaitOnPointerHover() { WaitEvent(Events::kOnPointerHover); }

  void WaitOnPointerDown() { WaitEvent(Events::kOnPointerDown); }

 private:
  enum Events {
    kUpdateSpeculationCandidates = 0,
    kOnPointerHover,
    kOnPointerDown,
    kMaxValue = kOnPointerDown,
  };

  void WaitEvent(Events event) {
    if (events_called_[event]) {
      return;
    }
    ASSERT_FALSE(quit_closures_[event]);
    base::RunLoop loop;
    quit_closures_[event] = loop.QuitClosure();
    loop.Run();
  }

  void OnEventCalled(Events event) {
    events_called_[event] = true;
    if (quit_closures_[event]) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(quit_closures_[event]));
    }
  }

  raw_ptr<RenderFrameHostImpl> rfh_;
  raw_ptr<PreloadingDeciderObserverForTesting> old_observer_;

  std::array<base::OnceClosure, Events::kMaxValue + 1> quit_closures_;
  std::array<bool, Events::kMaxValue + 1> events_called_;
};

}  // namespace

// Tests speculation rules prerendering where the eagerness is "eager".
// The default eagerness of list rules is "eager", so its behavior should be
// same to normal speculation rules prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderEagernessBrowserTest, kEager) {
  const GURL initial_url = GetUrl("/empty.html");
  const GURL prerendering_url = GetUrl("/empty.html?prerender");

  // Navigate to an initial page, insert an anchor to the prerender page.
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  InsertAnchor(prerendering_url);

  RenderFrameHostImpl* rfh = current_frame_host();
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(rfh);

  // Add speculation rules with the eagerness.
  // When the eagerness is "eager", speculation candidates will never be kept in
  // the |on_standby_candidates_| on |PreloadingDecider|, and |PrerenderHost|
  // will be created immediately.
  AddPrerenderWithEagernessAsync(prerendering_url,
                                 blink::mojom::SpeculationEagerness::kEager);
  WaitForPrerenderLoadCompletion(prerendering_url);
  EXPECT_TRUE(HasHostForUrl(prerendering_url));
  EXPECT_FALSE(preloading_decider->IsOnStandByForTesting(
      prerendering_url, blink::mojom::SpeculationAction::kPrerender));

  // Activate the prerendered page by clicking the anchor.
  FrameTreeNodeId host_id = GetHostForUrl(prerendering_url);
  test::PrerenderHostObserver prerender_observer(*web_contents(), host_id);
  PointerDownToAnchor(prerendering_url);
  PointerUpToAnchor(prerendering_url);
  prerender_observer.WaitForActivation();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), prerendering_url);
  EXPECT_TRUE(prerender_observer.was_activated());
}

// Tests speculation rules prerendering where the eagerness is "moderate".
IN_PROC_BROWSER_TEST_F(PrerenderEagernessBrowserTest, kModerate) {
  const GURL initial_url = GetUrl("/empty.html");
  const GURL prerendering_url = GetUrl("/empty.html?prerender");

  // Navigate to an initial page, insert an anchor to the prerender page.
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  InsertAnchor(prerendering_url);

  RenderFrameHostImpl* rfh = current_frame_host();
  PreloadingDeciderObserverForPrerenderTesting preloading_decider_observer(rfh);
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(rfh);

  // Add speculation rules with the eagerness.
  // When the eagerness is not "eager", speculation candidates will be kept in
  // the |on_standby_candidates_| on |PreloadingDecider|. |PrerenderHost| will
  // not be created at this time, waiting for user interaction(pointer hovering
  // for the "moderate").
  AddPrerenderWithEagernessAsync(prerendering_url,
                                 blink::mojom::SpeculationEagerness::kModerate);
  preloading_decider_observer.WaitUpdateSpeculationCandidates();
  EXPECT_FALSE(HasHostForUrl(prerendering_url));
  EXPECT_TRUE(preloading_decider->IsOnStandByForTesting(
      prerendering_url, blink::mojom::SpeculationAction::kPrerender));

  // Hover the anchor of the prerendering page. When eagerness is "moderate",
  // this interaction invokes the creation of |PrerenderHost|.
  PointerHoverToAnchor(prerendering_url);
  preloading_decider_observer.WaitOnPointerHover();
  WaitForPrerenderLoadCompletion(prerendering_url);
  EXPECT_TRUE(HasHostForUrl(prerendering_url));
  EXPECT_FALSE(preloading_decider->IsOnStandByForTesting(
      prerendering_url, blink::mojom::SpeculationAction::kPrerender));

  // Activate the prerendered page by clicking the anchor.
  FrameTreeNodeId host_id = GetHostForUrl(prerendering_url);
  test::PrerenderHostObserver prerender_observer(*web_contents(), host_id);
  PointerDownToAnchor(prerendering_url);
  PointerUpToAnchor(prerendering_url);
  prerender_observer.WaitForActivation();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), prerendering_url);
  EXPECT_TRUE(prerender_observer.was_activated());
}

// Tests speculation rules prerendering where the eagerness is "conservative".
IN_PROC_BROWSER_TEST_F(PrerenderEagernessBrowserTest, kConservative) {
  const GURL initial_url = GetUrl("/empty.html");
  const GURL prerendering_url = GetUrl("/empty.html?prerender");

  // Navigate to an initial page, insert an anchor to the prerender page.
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  InsertAnchor(prerendering_url);

  RenderFrameHostImpl* rfh = current_frame_host();
  PreloadingDeciderObserverForPrerenderTesting preloading_decider_observer(rfh);
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(rfh);

  // Add speculation rules with the eagerness.
  // When the eagerness is not "eager", speculation candidates will be kept in
  // the |on_standby_candidates_| on |PreloadingDecider|. |PrerenderHost| will
  // not be created at this time, waiting for user interaction(pointer clicking
  // for the "conservative").
  AddPrerenderWithEagernessAsync(
      prerendering_url, blink::mojom::SpeculationEagerness::kConservative);
  preloading_decider_observer.WaitUpdateSpeculationCandidates();
  EXPECT_FALSE(HasHostForUrl(prerendering_url));
  EXPECT_TRUE(preloading_decider->IsOnStandByForTesting(
      prerendering_url, blink::mojom::SpeculationAction::kPrerender));

  // Click the anchor of the prerendering page. When eagerness is
  // "conservative", PointerDown interaction invokes the creation of
  // |PrerenderHost| and this host will be activated on the navigation triggered
  // by the series of actions(PointerDown, PointerUp) on clicking.
  PointerDownToAnchor(prerendering_url);
  preloading_decider_observer.WaitOnPointerDown();
  WaitForPrerenderLoadCompletion(prerendering_url);
  EXPECT_TRUE(HasHostForUrl(prerendering_url));
  EXPECT_FALSE(preloading_decider->IsOnStandByForTesting(
      prerendering_url, blink::mojom::SpeculationAction::kPrerender));

  FrameTreeNodeId host_id = GetHostForUrl(prerendering_url);
  test::PrerenderHostObserver prerender_observer(*web_contents(), host_id);
  PointerUpToAnchor(prerendering_url);
  prerender_observer.WaitForActivation();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), prerendering_url);
  EXPECT_TRUE(prerender_observer.was_activated());
}

// TODO(crbug.com/40275452): These tests are turned off on Fuchsia and iOS
// tentatively because pointer simulation on them doesn't work properly on this
// test.
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
// Tests the metrics
// Prerender.Experimental.ReceivedPrerendersPerPrimaryPageChangedCount2
// correctly records the number of prerenders by each category per primary page
// changed.
IN_PROC_BROWSER_TEST_F(PrerenderEagernessBrowserTest,
                       ReceivedPrerendersPerPrimaryPageChangedCount) {
  auto GetAllSamples = [&](const std::string& eagerness_category) {
    return histogram_tester().GetAllSamples(
        "Prerender.Experimental.ReceivedPrerendersPerPrimaryPageChangedCount2."
        "SpeculationRule." +
        eagerness_category);
  };

  // Navigate to an initial page,
  const GURL initial_url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Nothing should have been recoreded yet.
  EXPECT_THAT(GetAllSamples("Total"), testing::IsEmpty());

  // Start one eager prerender.
  const GURL prerendering_url = GetUrl("/empty.html?prerender");
  AddPrerender(prerendering_url);

  // Navigate to the another url.
  // Expect that the categories "Total" and "Egaer" record 1 and others record
  // 0, as there was one eager prerender of the previous page.
  const GURL next_url = GetUrl("/empty.html?next");
  ASSERT_TRUE(NavigateToURL(shell(), next_url));
  EXPECT_THAT(GetAllSamples("Conservative"),
              base::BucketsAre(base::Bucket(0, 1)));
  EXPECT_THAT(GetAllSamples("Moderate"), base::BucketsAre(base::Bucket(0, 1)));
  EXPECT_THAT(GetAllSamples("NonEager"), base::BucketsAre(base::Bucket(0, 1)));
  EXPECT_THAT(GetAllSamples("Eager"), base::BucketsAre(base::Bucket(1, 1)));
  EXPECT_THAT(GetAllSamples("Total"), base::BucketsAre(base::Bucket(1, 1)));

  // Next, try to trigger followings:
  // a) 4 prerenders whose eagerness is eager
  // b) 2 prerenders whose eagerness is moderate
  // c) 1 prerenders whose eagerness is conservative
  // Then, try to activate the one of the URL(choosing conservative one).

  // a)
  for (int i = 0; i < 4; ++i) {
    GURL prerendering_url_eager =
        GetUrl("/empty.html?prerender_eager_" + base::NumberToString(i));
    AddPrerender(prerendering_url_eager);
  }

  // b)
  for (int i = 0; i < 2; ++i) {
    GURL prerendering_url_moderate =
        GetUrl("/empty.html?prerender_moderate_" + base::NumberToString(i));
    InsertAnchor(prerendering_url_moderate);
    AddPrerenderWithEagernessAsync(
        prerendering_url_moderate,
        blink::mojom::SpeculationEagerness::kModerate);
    PointerHoverToAnchor(prerendering_url_moderate);
    WaitForPrerenderLoadCompletion(prerendering_url_moderate);
  }

  // c)
  const GURL prerendering_url_conservative =
      GetUrl("/empty.html?prerender_conservative");
  InsertAnchor(prerendering_url_conservative);
  AddPrerenderWithEagernessAsync(
      prerendering_url_conservative,
      blink::mojom::SpeculationEagerness::kConservative);

  // Try to trigger and activate.
  TestActivationManager activation_manager(web_contents(),
                                           prerendering_url_conservative);
  ClickAnchor(prerendering_url_conservative);
  activation_manager.WaitForNavigationFinished();
  ASSERT_EQ(web_contents()->GetLastCommittedURL(),
            prerendering_url_conservative);
  ASSERT_TRUE(activation_manager.was_activated());

  // Expect following results:
  // - For each eagerness category, the number of prerenders triggered with that
  // eagerness is recorded.
  // - The sum of moderate and conservative prerenders is recorded to
  // "NonEager" (2 + 1 = 3).
  // - Total eligible numbers of prerenders is recorded to "Total" (4 + 2 + 1 =
  // 7).
  EXPECT_THAT(GetAllSamples("Eager"),
              base::BucketsAre(base::Bucket(1, 1), base::Bucket(4, 1)));
  EXPECT_THAT(GetAllSamples("Moderate"),
              base::BucketsAre(base::Bucket(0, 1), base::Bucket(2, 1)));
  EXPECT_THAT(GetAllSamples("Conservative"),
              base::BucketsAre(base::Bucket(0, 1), base::Bucket(1, 1)));
  EXPECT_THAT(GetAllSamples("NonEager"),
              base::BucketsAre(base::Bucket(0, 1), base::Bucket(3, 1)));
  EXPECT_THAT(GetAllSamples("Total"),
              base::BucketsAre(base::Bucket(1, 1), base::Bucket(7, 1)));
}

IN_PROC_BROWSER_TEST_F(PrerenderEagernessBrowserTest,
                       NonEagerPrerendersCanBeRetriggeredAfterTimeout) {
  // Navigate to an initial page.
  const GURL initial_url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Add a non-eager speculation rule.
  const GURL prerendering_url = GetUrl("/empty.html?prerender");
  InsertAnchor(prerendering_url);
  AddPrerenderWithEagernessAsync(prerendering_url,
                                 blink::mojom::SpeculationEagerness::kModerate);

  // Start prerendering.
  test::PrerenderHostCreationWaiter host_creation_waiter_a;
  PointerHoverToAnchor(prerendering_url);
  FrameTreeNodeId host_id_a = host_creation_waiter_a.Wait();
  test::PrerenderHostObserver prerender_observer_a(*web_contents_impl(),
                                                   host_id_a);

  PrerenderHostRegistry* prerender_host_registry =
      web_contents_impl()->GetPrerenderHostRegistry();
  ASSERT_FALSE(prerender_host_registry->GetSpeculationRulesTimerForTesting()
                   ->IsRunning());

  // Inject mock time task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  prerender_host_registry->SetTaskRunnerForTesting(task_runner);

  // Change the visibility to hidden and advance the timer. Prerendered page is
  // cancelced by timeout.
  web_contents()->WasHidden();
  ASSERT_TRUE(prerender_host_registry->GetSpeculationRulesTimerForTesting()
                  ->IsRunning());
  task_runner->FastForwardBy(
      PrerenderHostRegistry::kTimeToLiveInBackgroundForSpeculationRules);
  ASSERT_FALSE(prerender_host_registry->GetSpeculationRulesTimerForTesting()
                   ->IsRunning());
  prerender_observer_a.WaitForDestroyed();
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kTimeoutBackgrounded, 1);

  // Changing the visibility to shown.
  web_contents()->WasShown();

  // Start prerendering again.
  test::PrerenderHostCreationWaiter host_creation_waiter_b;
  PointerHoverToAnchor(prerendering_url);
  FrameTreeNodeId host_id_b = host_creation_waiter_b.Wait();
  test::PrerenderHostObserver prerender_observer_b(*web_contents(), host_id_b);

  NavigatePrimaryPage(prerendering_url);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), prerendering_url);
  EXPECT_TRUE(prerender_observer_b.was_activated());
}

class PrerenderNewLimitAndSchedulerBrowserTest
    : public PrerenderEagernessBrowserTest,
      public testing::WithParamInterface<std::string> {
 public:
  PrerenderNewLimitAndSchedulerBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrerender2NewLimitAndScheduler,
          {{"max_num_of_running_speculation_rules_eager_prerenders",
            base::NumberToString(
                MaxNumOfRunningSpeculationRulesEagerPrerenders())},
           {"max_num_of_running_speculation_rules_non_eager_prerenders",
            base::NumberToString(
                MaxNumOfRunningSpeculationRulesNonEagerPrerenders())},
           {"max_num_of_running_embedder_prerenders",
            base::NumberToString(MaxNumOfRunningEmbedderPrerenders())}}}},
        {});
  }
  int MaxNumOfRunningSpeculationRulesEagerPrerenders() { return 2; }
  int MaxNumOfRunningSpeculationRulesNonEagerPrerenders() { return 1; }
  int MaxNumOfRunningEmbedderPrerenders() { return 2; }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         PrerenderNewLimitAndSchedulerBrowserTest,
                         testing::Values("_self", "_blank"),
                         [](const testing::TestParamInfo<std::string>& info) {
                           return info.param;
                         });

IN_PROC_BROWSER_TEST_P(PrerenderNewLimitAndSchedulerBrowserTest,
                       ResetForNonEagerPrerener) {
  const GURL initial_url = GetUrl("/empty.html");
  std::vector<GURL> prerendering_urls;
  std::vector<base::WeakPtr<WebContents>> prerender_web_contents_list;

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Add moderate prerenders as many times as limit + 1 and trigger all of them
  // by hovering their links. All prerenders should be started at the time of
  // hovering, and the oldest started prerender should be canceled and removed
  // from the registry for the limit after the last prerender is started.
  int num_of_attempts = MaxNumOfRunningSpeculationRulesNonEagerPrerenders() + 1;
  for (int i = 0; i < num_of_attempts; i++) {
    PreloadingDeciderObserverForPrerenderTesting preloading_decider_observer(
        current_frame_host());
    const GURL prerendering_url =
        GetUrl("/empty.html?prerender" + base::ToString(i));
    prerendering_urls.push_back(prerendering_url);
    InsertAnchor(prerendering_url);
    AddPrerendersAsync({prerendering_url},
                       blink::mojom::SpeculationEagerness::kModerate,
                       GetParam());
    preloading_decider_observer.WaitUpdateSpeculationCandidates();

    test::PrerenderHostCreationWaiter host_creation_waiter;
    PointerHoverToAnchor(prerendering_url);
    FrameTreeNodeId host_id = host_creation_waiter.Wait();
    auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
    prerender_web_contents_list.push_back(prerender_web_contents->GetWeakPtr());
    test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
        *prerender_web_contents, prerendering_url);
  }

  for (int i = 0; i < num_of_attempts; i++) {
    bool host_existing_in_registry =
        prerender_web_contents_list[i] &&
        HasHostForUrl(*prerender_web_contents_list[i], prerendering_urls[i]);
    if (i == 0) {
      // The first (= oldest) prerender is removed since the (limit + 1)-th
      // prerender was started.
      ASSERT_FALSE(host_existing_in_registry);
    } else {
      ASSERT_TRUE(host_existing_in_registry);
    }
  }

  // Hover the first link again. This should be retriggered.
  const auto& prerendering_url_first = prerendering_urls[0];
  test::PrerenderHostCreationWaiter host_creation_waiter;
  PointerHoverToAnchor(prerendering_url_first);
  FrameTreeNodeId host_id = host_creation_waiter.Wait();
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
  prerender_web_contents_list[0] = prerender_web_contents->GetWeakPtr();
  test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
      *prerender_web_contents, prerendering_url_first);

  // The oldest prerender in registry at this point should be removed due to the
  // limit.
  for (int i = 0; i < num_of_attempts; i++) {
    bool host_existing_in_registry =
        prerender_web_contents_list[i] &&
        HasHostForUrl(*prerender_web_contents_list[i], prerendering_urls[i]);
    if (i == 1) {
      EXPECT_FALSE(host_existing_in_registry);
    } else {
      EXPECT_TRUE(host_existing_in_registry);
    }
  }
}
#endif  // !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)

class PrerenderWithBackForwardCacheBrowserTest
    : public PrerenderBrowserTest,
      public testing::WithParamInterface<BackForwardCacheType> {
 public:
  PrerenderWithBackForwardCacheBrowserTest() {
    switch (GetParam()) {
      case BackForwardCacheType::kDisabled:
        feature_list_.InitAndDisableFeature(::features::kBackForwardCache);
        break;
      case BackForwardCacheType::kEnabled:
        feature_list_.InitWithFeaturesAndParameters(
            GetDefaultEnabledBackForwardCacheFeaturesForTesting(
                /*ignore_outstanding_network_request=*/false),
            GetDefaultDisabledBackForwardCacheFeaturesForTesting());
        break;
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         PrerenderWithBackForwardCacheBrowserTest,
                         testing::Values(BackForwardCacheType::kDisabled,
                                         BackForwardCacheType::kEnabled),
                         ToString);

// Tests that history navigation works after activation. This runs with variaous
// BFCache configurations that may modify behavior of history navigation.
// This is a regression test for https://crbug.com/1201914.
IN_PROC_BROWSER_TEST_P(PrerenderWithBackForwardCacheBrowserTest,
                       HistoryNavigationAfterActivation) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  RenderFrameHostImpl* initial_frame_host = current_frame_host();
  blink::LocalFrameToken initial_frame_token =
      initial_frame_host->GetFrameToken();

  // When the BFCache is disabled, activation will destroy the initial frame
  // host. This observer will be used for confirming it.
  RenderFrameDeletedObserver delete_observer(initial_frame_host);

  // Make and activate a prerendered page.
  AddPrerender(kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);

  // Check if the initial page is in the BFCache.
  switch (GetParam()) {
    case BackForwardCacheType::kDisabled:
      EXPECT_NE(current_frame_host(), initial_frame_host);
      // The initial frame host should be deleted after activation because it is
      // not cached in the BFCache.
      delete_observer.WaitUntilDeleted();
      break;
    case BackForwardCacheType::kEnabled:
      // Same-origin prerender activation should allow the initial page to be
      // cached in the BFCache.
      ASSERT_TRUE(IsBackForwardCacheEnabled());
      EXPECT_TRUE(initial_frame_host->IsInBackForwardCache());
      break;
  }

  // Navigate back to the initial page.
  TestNavigationObserver observer(web_contents());
  shell()->GoBackOrForward(-1);
  observer.Wait();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Check if the back navigation is served from the BFCache.
  switch (GetParam()) {
    case BackForwardCacheType::kDisabled:
      // The frame host should be created again.
      EXPECT_NE(current_frame_host()->GetFrameToken(), initial_frame_token);
      break;
    case BackForwardCacheType::kEnabled:
      // The frame host should be restored.
      EXPECT_EQ(current_frame_host()->GetFrameToken(), initial_frame_token);
      EXPECT_FALSE(initial_frame_host->IsInBackForwardCache());
      break;
  }
}

// Tests that a trigger page destroys a prerendered page when it navigates
// forward and goes into the back/forward cache.
IN_PROC_BROWSER_TEST_P(PrerenderWithBackForwardCacheBrowserTest,
                       CancelOnAfterTriggerIsStoredInBackForwardCache_Forward) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kNextUrl = GetUrl("/empty.html?next");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  RenderFrameHostImplWrapper initial_frame_host(current_frame_host());

  // Make a prerendered page from the initial page.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  test::PrerenderHostObserver prerender_observer(*web_contents_impl(), host_id);

  // Navigate the initial page to a non-prerendered page.
  ASSERT_TRUE(NavigateToURL(shell(), kNextUrl));

  // Check if the initial page is in the back/forward cache.
  switch (GetParam()) {
    case BackForwardCacheType::kDisabled:
      // The BFCache is disabled, so the initial page is not in the
      // back/forward cache.
      if (ShouldCreateNewHostForAllFrames()) {
        ASSERT_TRUE(initial_frame_host.WaitUntilRenderFrameDeleted());
      } else {
        ASSERT_FALSE(initial_frame_host->IsInBackForwardCache());
      }
      break;
    case BackForwardCacheType::kEnabled:
      // The back/forward cache is enabled, so the initial page is in the
      // back/forward cache after the same-origin navigation.
      ASSERT_TRUE(IsBackForwardCacheEnabled());
      ASSERT_TRUE(initial_frame_host->IsInBackForwardCache());
      break;
  }

  // The navigation should destroy the prerendered page regardless of if the
  // initial page was in the back/forward cache.
  prerender_observer.WaitForDestroyed();
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kTriggerDestroyed);
}

// Tests that a trigger page destroys a prerendered page when it navigates back
// and goes into the BFCache.
IN_PROC_BROWSER_TEST_P(PrerenderWithBackForwardCacheBrowserTest,
                       CancelOnAfterTriggerIsStoredInBackForwardCache_Back) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kNextUrl = GetUrl("/empty.html?next");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Navigate to a next page.
  ASSERT_TRUE(NavigateToURL(shell(), kNextUrl));
  RenderFrameHostImplWrapper next_frame_host(current_frame_host());

  // Make a prerendered page from the next page.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  test::PrerenderHostObserver prerender_observer(*web_contents_impl(), host_id);

  // Navigate back to the initial page.
  TestNavigationObserver navigation_observer(web_contents());
  shell()->GoBackOrForward(-1);
  navigation_observer.Wait();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Check if the next page is in the back/forward cache.
  switch (GetParam()) {
    case BackForwardCacheType::kDisabled:
      // The back/forward cache is disabled, so the next page is not in the
      // back/forward cache.
      if (ShouldCreateNewHostForAllFrames()) {
        ASSERT_TRUE(next_frame_host.WaitUntilRenderFrameDeleted());
      } else {
        ASSERT_FALSE(next_frame_host->IsInBackForwardCache());
      }
      break;
    case BackForwardCacheType::kEnabled:
      // The back/forward cache is enabled, so the next page is in the
      // back/forward cache after the same-origin back navigation.
      ASSERT_TRUE(IsBackForwardCacheEnabled());
      ASSERT_TRUE(next_frame_host->IsInBackForwardCache());
      break;
  }

  // The navigation should destroy the prerendered page regardless of if the
  // next page was in the back/forward cache.
  prerender_observer.WaitForDestroyed();
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kTriggerDestroyed);
}

class PrerenderBackForwardCacheRestorationBrowserTest
    : public PrerenderEagernessBrowserTest,
      public BackForwardCacheMetricsTestMatcher,
      public testing::WithParamInterface<blink::mojom::SpeculationEagerness> {
 public:
  PrerenderBackForwardCacheRestorationBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            /*ignore_outstanding_network_request=*/false),
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }

 protected:
  blink::mojom::SpeculationEagerness GetSpeculationEagerness() const {
    return GetParam();
  }

  // implementation for `BackForwardCacheMetricsTestMatcher`
  const ukm::TestAutoSetUkmRecorder& ukm_recorder() override {
    return *PrerenderBrowserTest::test_ukm_recorder();
  }

  // implementation for `BackForwardCacheMetricsTestMatcher`
  const base::HistogramTester& histogram_tester() override {
    return PrerenderBrowserTest::histogram_tester();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PrerenderBackForwardCacheRestorationBrowserTest,
    testing::Values(blink::mojom::SpeculationEagerness::kEager,
                    blink::mojom::SpeculationEagerness::kModerate,
                    blink::mojom::SpeculationEagerness::kConservative),
    [](const testing::TestParamInfo<blink::mojom::SpeculationEagerness>& info) {
      return base::ToString(info.param);
    });

// Test whether speculation rules prerendering is processed again on pages
// restored from BFCache via forward navigation.
// When the eagerness is kEager(default), speculation rules prerendering will no
// longer be processed after restoration.
// For non-eager cases (kModerate, kConservative), candidates are stored between
// restoration unless they were triggered by user action (This test scenario
// reproduces only this case). However, once after processed by user action,
// then they will not be processed again until they are retriggered
// (crbug.com/1449163 for more information).
IN_PROC_BROWSER_TEST_P(PrerenderBackForwardCacheRestorationBrowserTest,
                       RestoredViaForwardNavigation) {
  const GURL initial_url = GetUrl("/empty.html");
  const GURL next_url = GetUrl("/empty.html?next");
  const GURL prerendering_url = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Navigate to a next page.
  ASSERT_TRUE(NavigateToURL(shell(), next_url));
  RenderFrameHostImpl* rfh_next = current_frame_host();
  InsertAnchor(prerendering_url);

  PreloadingDeciderObserverForPrerenderTesting preloading_decider_observer(
      rfh_next);
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(rfh_next);

  // Add speculation rules and wait to be loaded.
  AddPrerenderWithEagernessAsync(prerendering_url, GetSpeculationEagerness());
  if (GetSpeculationEagerness() == blink::mojom::SpeculationEagerness::kEager) {
    WaitForPrerenderLoadCompletion(prerendering_url);
    ASSERT_TRUE(HasHostForUrl(prerendering_url));
  } else {
    preloading_decider_observer.WaitUpdateSpeculationCandidates();
    ASSERT_TRUE(preloading_decider->IsOnStandByForTesting(
        prerendering_url, blink::mojom::SpeculationAction::kPrerender));
  }

  // Navigate backward to the initial page. The next page should be stored to
  // the BFCache.
  GoBack();
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), initial_url);
  ExpectRestored(FROM_HERE);
  ASSERT_TRUE(rfh_next->IsInBackForwardCache());

  // Navigate forward. The next page should be restored from the BFCache.
  GoForward();
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), next_url);
  ExpectRestored(FROM_HERE);

  if (GetSpeculationEagerness() == blink::mojom::SpeculationEagerness::kEager) {
    // Prerendering will be processed by retriggering.
    WaitForPrerenderLoadCompletion(prerendering_url);
    FrameTreeNodeId host_id_retriggered = GetHostForUrl(prerendering_url);

    test::PrerenderHostObserver prerender_observer(*web_contents(),
                                                   host_id_retriggered);

    // Activate the prerendered page.
    NavigatePrimaryPage(prerendering_url);
    prerender_observer.WaitForActivation();
    ASSERT_EQ(web_contents()->GetLastCommittedURL(), prerendering_url);
    EXPECT_TRUE(prerender_observer.was_activated());
  } else {
    ASSERT_FALSE(HasHostForUrl(prerendering_url));

    // |on_standby_candidates_| holds the non-eager candidates if the candidates
    // were not processed by user interaction.
    EXPECT_TRUE(preloading_decider->IsOnStandByForTesting(
        prerendering_url, blink::mojom::SpeculationAction::kPrerender));

    // Trigger and activate the non-eager prerender.
    {
      TestActivationManager activation_manager(web_contents(),
                                               prerendering_url);
      ClickAnchor(prerendering_url);
      activation_manager.WaitForNavigationFinished();
      ASSERT_EQ(web_contents()->GetLastCommittedURL(), prerendering_url);
      ASSERT_TRUE(activation_manager.was_activated());
    }
  }
}

// Test whether speculation rules prerendering is processed again on pages
// restored from BFCache via backward navigation.
// When the eagerness is kEager(default), speculation rules prerendering will no
// longer be processed after restoration.
// For non-eager cases (kModerate, kConservative), candidates are stored between
// restoration unless they were triggered by user action. However, once after
// processed by user action, then they will not be processed again until they
// are retriggered (crbug.com/1449163 for more information).
IN_PROC_BROWSER_TEST_P(PrerenderBackForwardCacheRestorationBrowserTest,
                       RestoredViaBackwardNavigation) {
  const GURL initial_url = GetUrl("/empty.html");
  const GURL prerendering_url_a = GetUrl("/empty.html?prerender_a");
  const GURL prerendering_url_b = GetUrl("/empty.html?prerender_b");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  RenderFrameHostImpl* rfh_initial = current_frame_host();
  InsertAnchor(prerendering_url_a);
  InsertAnchor(prerendering_url_b);

  if (GetSpeculationEagerness() == blink::mojom::SpeculationEagerness::kEager) {
    // Add speculation rules and wait to be loaded.
    AddPrerenderWithEagernessAsync(prerendering_url_a,
                                   GetSpeculationEagerness());
    AddPrerenderWithEagernessAsync(prerendering_url_b,
                                   GetSpeculationEagerness());
    WaitForPrerenderLoadCompletion(prerendering_url_a);
    WaitForPrerenderLoadCompletion(prerendering_url_b);

    FrameTreeNodeId host_id_a = GetHostForUrl(prerendering_url_a);
    test::PrerenderHostObserver prerender_observer_a(*web_contents(),
                                                     host_id_a);

    // Activate the page A. The initial page should be stored to the BFCache.
    NavigatePrimaryPage(prerendering_url_a);
    prerender_observer_a.WaitForActivation();
    ASSERT_EQ(web_contents()->GetLastCommittedURL(), prerendering_url_a);
    ASSERT_TRUE(prerender_observer_a.was_activated());
    ASSERT_TRUE(rfh_initial->IsInBackForwardCache());

    // Navigate backward. The initial page should be restored from the BFCache.
    GoBack();
    ASSERT_EQ(web_contents()->GetLastCommittedURL(), initial_url);
    ExpectRestored(FROM_HERE);

    // Prerendering for both the page A and the page B will be processed by
    // retriggering.
    WaitForPrerenderLoadCompletion(prerendering_url_a);
    WaitForPrerenderLoadCompletion(prerendering_url_b);
    FrameTreeNodeId host_id_a_retriggered = GetHostForUrl(prerendering_url_a);

    test::PrerenderHostObserver prerender_observer_a_retriggered(
        *web_contents(), host_id_a_retriggered);

    // Activate the page A again.
    NavigatePrimaryPage(prerendering_url_a);
    prerender_observer_a_retriggered.WaitForActivation();
    ASSERT_EQ(web_contents()->GetLastCommittedURL(), prerendering_url_a);
    EXPECT_TRUE(prerender_observer_a_retriggered.was_activated());
  } else {
    auto* preloading_decider =
        PreloadingDecider::GetOrCreateForCurrentDocument(rfh_initial);

    // Add speculation rules and wait to be loaded.
    // TODO(taiyo): modify |PreloadingDeciderObserverForPrerenderTesting| to
    // enable observing for URLs.
    {
      PreloadingDeciderObserverForPrerenderTesting preloading_decider_observer(
          rfh_initial);
      AddPrerenderWithEagernessAsync(prerendering_url_a,
                                     GetSpeculationEagerness());
      preloading_decider_observer.WaitUpdateSpeculationCandidates();
    }
    {
      PreloadingDeciderObserverForPrerenderTesting preloading_decider_observer(
          rfh_initial);
      AddPrerenderWithEagernessAsync(prerendering_url_b,
                                     GetSpeculationEagerness());
      preloading_decider_observer.WaitUpdateSpeculationCandidates();
    }
    ASSERT_TRUE(preloading_decider->IsOnStandByForTesting(
        prerendering_url_a, blink::mojom::SpeculationAction::kPrerender));
    ASSERT_TRUE(preloading_decider->IsOnStandByForTesting(
        prerendering_url_b, blink::mojom::SpeculationAction::kPrerender));

    // Activate the page A. The initial page should be stored to the BFCache.
    {
      TestActivationManager activation_manager(web_contents(),
                                               prerendering_url_a);
      ClickAnchor(prerendering_url_a);
      activation_manager.WaitForNavigationFinished();
      ASSERT_EQ(web_contents()->GetLastCommittedURL(), prerendering_url_a);
      ASSERT_TRUE(activation_manager.was_activated());
      ASSERT_TRUE(rfh_initial->IsInBackForwardCache());
    }

    // Navigate backward. The initial page should be restored from the BFCache.
    GoBack();
    ASSERT_EQ(web_contents()->GetLastCommittedURL(), initial_url);
    ExpectRestored(FROM_HERE);

    // |on_standby_candidates_| holds the non-eager candidates if the candidates
    // were not processed by user interaction so that the the Page B's candidate
    // should be in the |on_standby_candidates_|.
    EXPECT_TRUE(preloading_decider->IsOnStandByForTesting(
        prerendering_url_b, blink::mojom::SpeculationAction::kPrerender));

    // TODO(crbug.com/40273826): In the current implementation, non-eager
    // candidates that are once processed by user interaction will no
    // longer be stored in |on_standby_candidates_| when retriggered (more
    // specifically, |UpdateSpeculationCandidates| is (re)invoked) and instead
    // |PrerenderHost| will be created immediately, as with eager candidates.
    // See crbug description for more details.
    {
      WaitForPrerenderLoadCompletion(prerendering_url_a);
      EXPECT_TRUE(HasHostForUrl(prerendering_url_a));
      EXPECT_FALSE(preloading_decider->IsOnStandByForTesting(
          prerendering_url_a, blink::mojom::SpeculationAction::kPrerender));
    }

    ASSERT_FALSE(HasHostForUrl(prerendering_url_b));

    // Trigger and activate the Page A again.
    {
      TestActivationManager activation_manager(web_contents(),
                                               prerendering_url_a);
      ClickAnchor(prerendering_url_a);
      activation_manager.WaitForNavigationFinished();
      ASSERT_EQ(web_contents()->GetLastCommittedURL(), prerendering_url_a);
      ASSERT_TRUE(activation_manager.was_activated());
    }
  }
}

// Tests that PrerenderHostRegistry can hold up to two prerendering for the
// prerender embedders it receives.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, StartByEmbeddersMultipleTimes) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kFirstPrerenderingUrl = GetUrl("/empty.html?prerender1");
  const GURL kSecondPrerenderingUrl = GetUrl("/empty.html?prerender2");
  const GURL kThirdPrerenderingUrl = GetUrl("/empty.html?prerender3");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  // Start prerendering by embedder triggered prerendering; this should be
  // trigger successfully.
  std::unique_ptr<PrerenderHandle> prerender_handle1 =
      AddEmbedderTriggeredPrerenderAsync(kFirstPrerenderingUrl);
  EXPECT_TRUE(prerender_handle1);

  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kMaxNumOfRunningEagerPrerendersExceeded, 0);

  // Start prerendering by embedder triggered prerendering; this should be
  // trigger successfully.
  std::unique_ptr<PrerenderHandle> prerender_handle2 =
      AddEmbedderTriggeredPrerenderAsync(kSecondPrerenderingUrl);
  EXPECT_TRUE(prerender_handle2);

  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kMaxNumOfRunningEmbedderPrerendersExceeded, 0);

  // Start prerendering by embedder triggered prerendering; this should hit the
  // limit.
  std::unique_ptr<PrerenderHandle> prerender_handle3 =
      AddEmbedderTriggeredPrerenderAsync(kThirdPrerenderingUrl);
  EXPECT_FALSE(prerender_handle3);

  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kMaxNumOfRunningEmbedderPrerendersExceeded, 1);
}

// Tests that PrerenderHostRegistry can hold up to two prerendering for the
// prerender speculation rule and prerender embedders in total.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       StartByEmbeddersAndSpeculationRulesMultipleTimes) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kSpeculationRulesPrerenderingUrl =
      GetUrl("/empty.html?prerender1");
  const GURL kEmbedderPrerenderingUrl1 = GetUrl("/empty.html?prerender2");
  const GURL kEmbedderPrerenderingUrl2 = GetUrl("/empty.html?prerender3");
  const GURL kEmbedderPrerenderingUrl3 = GetUrl("/empty.html?prerender4");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  // Add a prerender speculation rule; this should be triggered successfully.
  AddPrerender(kSpeculationRulesPrerenderingUrl);

  // Add the first prerender speculation rule; it should trigger prerendering
  // successfully.
  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kMaxNumOfRunningEagerPrerendersExceeded, 0);

  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kMaxNumOfRunningEmbedderPrerendersExceeded, 0);

  // Start the first embedder triggered prerendering; this should be triggered
  // successfully.
  std::unique_ptr<PrerenderHandle> prerender_handle1 =
      AddEmbedderTriggeredPrerenderAsync(kEmbedderPrerenderingUrl1);
  EXPECT_TRUE(prerender_handle1);

  // Start the second embedder triggered prerendering; this should be triggered
  // successfully.
  std::unique_ptr<PrerenderHandle> prerender_handle2 =
      AddEmbedderTriggeredPrerenderAsync(kEmbedderPrerenderingUrl2);
  EXPECT_TRUE(prerender_handle2);

  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kMaxNumOfRunningEmbedderPrerendersExceeded, 0);

  // Start the third embedder triggered prerendering; this should hit the limit.
  std::unique_ptr<PrerenderHandle> prerender_handle3 =
      AddEmbedderTriggeredPrerenderAsync(kEmbedderPrerenderingUrl3);
  EXPECT_FALSE(prerender_handle3);

  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kMaxNumOfRunningEmbedderPrerendersExceeded, 1);

  // Cancel the second embedder triggered prerendering and start a new one;
  // this should succeed as one of the prerenders is freed.
  prerender_handle2.reset();
  prerender_handle3 =
      AddEmbedderTriggeredPrerenderAsync(kEmbedderPrerenderingUrl3);
  EXPECT_TRUE(prerender_handle3);
}

class MultiplePrerendersBrowserTest : public PrerenderBrowserTest {
 public:
  MultiplePrerendersBrowserTest() {
    base::test::FeatureRefAndParams memory_controls{
        blink::features::kPrerender2MemoryControls,
        // A value 100 allows prerenderings regardless of the current memory
        // usage.
        {{"acceptable_percent_of_system_memory", "100"},
         // Allow prerendering on low-end trybot devices so that prerendering
         // can run on any bots.
         {"memory_threshold_in_mb", "0"}}};

    feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrerender2NewLimitAndScheduler,
          {{"max_num_of_running_speculation_rules_eager_prerenders",
            base::NumberToString(MaxNumOfRunningPrerenders())}}},
         memory_controls},
        {});
  }

  int MaxNumOfRunningPrerenders() const { return 4; }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class MultiplePrerendersWithLimitedMemoryBrowserTest
    : public MultiplePrerendersBrowserTest {
 public:
  MultiplePrerendersWithLimitedMemoryBrowserTest() {
    base::test::FeatureRefAndParams memory_controls{
        blink::features::kPrerender2MemoryControls,
        // A value 0 doesn't allow any prerendering.
        {{"acceptable_percent_of_system_memory", "0"},
         // Allow prerendering on low-end trybot devices so that prerendering
         // can run on any bots.
         {"memory_threshold_in_mb", "0"}}};

    feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrerender2NewLimitAndScheduler,
          {{"max_num_of_running_speculation_rules_eager_prerenders",
            base::NumberToString(MaxNumOfRunningPrerenders())}}},
         memory_controls},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that moderate-level memory pressure doesn't cancel prerendering on
// trigger.
IN_PROC_BROWSER_TEST_F(MultiplePrerendersBrowserTest,
                       MemoryPressureOnTrigger_Moderate) {
  GURL initial_url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Emulate moderate-level memory pressure state.
  FakeMemoryPressureMonitor memory_pressure_monitor(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  ASSERT_EQ(base::MemoryPressureMonitor::Get()->GetCurrentPressureLevel(),
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  // Triggering prerendering should not be canceled due to the moderate level
  // memory pressure.
  GURL prerender_url = GetUrl("/empty.html?prerender");
  AddPrerender(prerender_url);
  EXPECT_TRUE(HasHostForUrl(prerender_url));

  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kMemoryPressureOnTrigger, 0);
}

// Tests that critical-level memory pressure cancels prerendering on trigger.
IN_PROC_BROWSER_TEST_F(MultiplePrerendersBrowserTest,
                       MemoryPressureOnTrigger_Critical) {
  GURL initial_url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Emulate critical-level memory pressure state.
  FakeMemoryPressureMonitor memory_pressure_monitor(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  ASSERT_EQ(base::MemoryPressureMonitor::Get()->GetCurrentPressureLevel(),
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

  // Triggering prerendering should be canceled due to the critical level memory
  // pressure.
  GURL prerender_url = GetUrl("/empty.html?prerender");
  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());
  AddPrerenderAsync(prerender_url);
  registry_observer.WaitForTrigger(prerender_url);
  EXPECT_FALSE(HasHostForUrl(prerender_url));

  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kMemoryPressureOnTrigger, 1);
}

// Tests that moderate-level memory pressure doesn't cancel prerendering after
// triggered.
IN_PROC_BROWSER_TEST_F(MultiplePrerendersBrowserTest,
                       MemoryPressureAfterTriggered_Moderate) {
  GURL initial_url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  std::vector<GURL> prerender_urls = {
      GetUrl("/empty.html?prerender0"),
      GetUrl("/empty.html?prerender1"),
      GetUrl("/empty.html?prerender2"),
  };

  for (const GURL& prerender_url : prerender_urls) {
    AddPrerender(prerender_url);
  }

  // Emulate moderate-level memory pressure event. This shouldn't cancel
  // prerendering.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  // Run the message loop to give a chance to unexpectedly cancel prerendering
  // due to some bug.
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kMemoryPressureAfterTriggered, 0);
}

// Tests that critical-level memory pressure cancels prerendering after
// triggered.
IN_PROC_BROWSER_TEST_F(MultiplePrerendersBrowserTest,
                       MemoryPressureAfterTriggered_Critical) {
  GURL initial_url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  std::vector<GURL> prerender_urls = {
      GetUrl("/empty.html?prerender0"),
      GetUrl("/empty.html?prerender1"),
      GetUrl("/empty.html?prerender2"),
  };

  std::vector<std::unique_ptr<test::PrerenderHostObserver>> observers;
  for (const GURL& prerender_url : prerender_urls) {
    FrameTreeNodeId host_id = AddPrerender(prerender_url);
    observers.push_back(std::make_unique<test::PrerenderHostObserver>(
        *web_contents(), host_id));
  }

  // Emulate critical-level memory pressure event. This should cancel
  // prerendering.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  for (auto& observer : observers) {
    observer->WaitForDestroyed();
  }
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kMemoryPressureAfterTriggered,
      prerender_urls.size());
}

// Tests that PrerenderHostRegistry only starts prerender speculation rules
// up to `max_num_of_running_speculation_rules` defined by a Finch param.
IN_PROC_BROWSER_TEST_F(MultiplePrerendersBrowserTest,
                       AddSpeculationRulesMultipleTimes) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  for (int i = 0; i < MaxNumOfRunningPrerenders(); i++) {
    GURL prerendering_url =
        GetUrl("/empty.html?prerender" + base::NumberToString(i));

    // Add a prerender speculation rule; it should trigger prerendering
    // successfully.
    AddPrerender(prerendering_url);
  }

  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());

  const GURL kExceededPrerenderingUrl =
      GetUrl("/empty.html?exceeded-prerender");
  // Add a new prerender speculation rule. Since PrerenderHostRegistry limits
  // the number of running prerenders to `max_num_of_running_speculation_rules`
  // defined by a Finch param, this rule should not be applied.
  AddPrerenderAsync(kExceededPrerenderingUrl);
  registry_observer.WaitForTrigger(kExceededPrerenderingUrl);
  EXPECT_FALSE(HasHostForUrl(kExceededPrerenderingUrl));

  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kMaxNumOfRunningEagerPrerendersExceeded);

  const GURL kEmbedderTriggeredPrerenderingUrl =
      GetUrl("/empty.html?embedder-triggered-prerender");
  // Start an embedder triggered prerendering; this should be triggered
  // successfully because its limitation is independent from speculation rules.
  std::unique_ptr<PrerenderHandle> prerender_handle =
      AddEmbedderTriggeredPrerenderAsync(kEmbedderTriggeredPrerenderingUrl);
  EXPECT_TRUE(prerender_handle);
}

// Tests that PrerenderHostRegistry can start prerendering when the DevTools is
// open even if the acceptable percent of the system memory is set to 0.
IN_PROC_BROWSER_TEST_F(MultiplePrerendersWithLimitedMemoryBrowserTest,
                       DevToolsOverride) {
  GURL initial_url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Emulating Devtools attached to test the memory restriction override. Retain
  // the returned host until the test finishes to avoid DevTools termination.
  scoped_refptr<DevToolsAgentHost> dev_tools_agent_host =
      content::DevToolsAgentHost::GetOrCreateFor(web_contents());
  ASSERT_TRUE(dev_tools_agent_host);

  std::vector<GURL> urls = {
      GetUrl("/empty.html?prerender0"),
      GetUrl("/empty.html?prerender1"),
      GetUrl("/empty.html?prerender2"),
  };

  for (const GURL& url : urls) {
    AddPrerender(url);
  }

  // Prerender attempts shouldn't be cancelled for the memory limit.
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kMemoryLimitExceeded, 0);

  // Activate one of the prerendered pages. This should cancel the other
  // prerendered as kTriggerDestroyed.
  NavigatePrimaryPage(urls[0]);
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), urls[0]);
  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kActivated, 1);
  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kOtherPrerenderedPageActivated, 2);
}

// Tests that cross-site urls cannot be prerendered.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, SkipCrossSitePrerender) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetCrossSiteUrl("/empty.html?crossorigin");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());

  url::Origin initiator_origin = url::Origin::Create(kInitialUrl);
  url::Origin prerender_origin = url::Origin::Create(kPrerenderingUrl);

  // Add a cross-origin prerender rule.
  AddPrerenderAsync(kPrerenderingUrl);

  // Wait for PrerenderHostRegistry to receive the cross-origin prerender
  // request, and it should be ignored.
  registry_observer.WaitForTrigger(kPrerenderingUrl);
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kCrossSiteNavigationInInitialNavigation);

  // Cross-check that in case of cross-origin navigation the eligibility
  // reason points to kCrossOrigin.
  ASSERT_TRUE(NavigateToURL(shell(), kPrerenderingUrl));
  ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder().BuildEntry(
      PrimaryPageSourceId(), PreloadingType::kPrerender,
      PreloadingEligibility::kCrossOrigin,
      PreloadingHoldbackStatus::kUnspecified,
      PreloadingTriggeringOutcome::kUnspecified,
      PreloadingFailureReason::kUnspecified,
      /*accurate=*/true,
      /*ready_time=*/std::nullopt,
      blink::mojom::SpeculationEagerness::kEager)});
}

// Tests that same-site cross-origin navigation by speculation rules is not
// allowed with the feature enabled but without opt-in.
IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    SameSiteCrossOriginNavigationSpeculationRulesWithoutOptInHeader) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl =
      GetSameSiteCrossOriginUrl("/empty.html?samesitecrossorigin");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Add a same-site cross-origin prerender rule.
  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());
  AddPrerenderAsync(kPrerenderingUrl);
  // Wait for PrerenderHostRegistry to receive the same-site cross-origin
  // prerender request, but it will be ignored because the opt-in header is
  // missing.
  registry_observer.WaitForTrigger(kPrerenderingUrl);
  // Navigate to the prerendering URL. This should result in regular navigation,
  // not prerender activation.
  NavigatePrimaryPage(kPrerenderingUrl);

  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::
          kSameSiteCrossOriginNavigationNotOptInInInitialNavigation);
}

// Tests that same-site cross-origin redirection by speculation rules with the
// feature enabled but without opt-in.
IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    SameSiteCrossOriginRedirectionSpeculationRulesWithoutOptInHeader) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering a URL that causes cross-origin redirection. The
  // cross-origin redirection should fail prerendering without an opt-in header.
  const GURL kRedirectedUrl =
      GetSameSiteCrossOriginUrl("/empty.html?samesitecrossorigin");
  const GURL kPrerenderingUrl =
      GetUrl("/server-redirect?" + kRedirectedUrl.spec());
  test::PrerenderHostObserver host_observer(*web_contents_impl(),
                                            kPrerenderingUrl);
  AddPrerenderAsync(kPrerenderingUrl);
  host_observer.WaitForDestroyed();
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl), 1);
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  EXPECT_FALSE(HasHostForUrl(kRedirectedUrl));
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::
          kSameSiteCrossOriginRedirectNotOptInInInitialNavigation);
}

// Tests that same-site cross-origin redirection with credentialed prerender by
// speculation rules with the feature enabled but the redirected page without
// opt-in. This test verifies a case which is a.test -> a.test (credentialed
// prerender) -> b.a.test (no credentialed prerender).
IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    SameSiteCrossOriginCredentialedPrerenderRedirectionSpeculationRulesWithoutOptInHeader) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering a URL that causes cross-origin redirection. The
  // cross-origin redirection should fail prerendering without an opt-in header.
  const GURL kRedirectedUrl =
      GetSameSiteCrossOriginUrl("/empty.html?samesitecrossorigin");
  const GURL kPrerenderingUrl = GetUrl(
      "/server-redirect-credentialed-prerender?" + kRedirectedUrl.spec());
  test::PrerenderHostObserver host_observer(*web_contents_impl(),
                                            kPrerenderingUrl);
  AddPrerenderAsync(kPrerenderingUrl);
  host_observer.WaitForDestroyed();
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl), 1);
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  EXPECT_FALSE(HasHostForUrl(kRedirectedUrl));
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::
          kSameSiteCrossOriginRedirectNotOptInInInitialNavigation);
}

// Tests that same-site cross-origin redirection with credentialed prerender by
// speculation rules with the feature enabled but the redirected page without
// opt-in. This test verifies a case which is a.test -> b.a.test (credentialed
// prerender) -> b.a.test (no credentialed prerender)
IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    SameSiteCrossOriginCredentialedPrerenderRedirectionSpeculationRulesWithoutOptInHeader2) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering a URL that causes cross-origin redirection. The
  // cross-origin redirection should fail prerendering without an opt-in header.
  const GURL kRedirectedUrl =
      GetSameSiteCrossOriginUrl("/empty.html?samesitecrossorigin");
  const GURL kPrerenderingUrl = GetSameSiteCrossOriginUrl(
      "/server-redirect-credentialed-prerender?" + kRedirectedUrl.spec());
  test::PrerenderHostObserver host_observer(*web_contents_impl(),
                                            kPrerenderingUrl);
  AddPrerenderAsync(kPrerenderingUrl);
  host_observer.WaitForDestroyed();
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl), 1);
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  EXPECT_FALSE(HasHostForUrl(kRedirectedUrl));
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::
          kSameSiteCrossOriginRedirectNotOptInInInitialNavigation);
}

// Tests that same-site cross-origin navigation redirecting back to same-origin
// without opt-in.
IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    SameSiteCrossOriginNavigationBackToSameOriginWithoutOptInHeader) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering a URL that causes same-site cross-origin navigation and
  // redirects back to the same-origin. This should not fail even without
  // same-site cross-origin opt-in header.
  const GURL kRedirectedUrl = GetUrl("/empty.html?samesitecrossorigin");
  const GURL kPrerenderingUrl =
      GetSameSiteCrossOriginUrl("/server-redirect?" + kRedirectedUrl.spec());
  test::PrerenderHostObserver host_observer(*web_contents_impl(),
                                            kPrerenderingUrl);

  RedirectChainObserver redirect_chain_observer(*shell()->web_contents(),
                                                kRedirectedUrl);
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  ASSERT_EQ(2u, redirect_chain_observer.redirect_chain().size());
  EXPECT_EQ(kPrerenderingUrl, redirect_chain_observer.redirect_chain()[0]);
  EXPECT_EQ(kRedirectedUrl, redirect_chain_observer.redirect_chain()[1]);

  NavigatePrimaryPage(kPrerenderingUrl);
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kRedirectedUrl);
  // Activating the prerendered page should not issue a request.
  EXPECT_EQ(GetRequestCount(kRedirectedUrl), 1);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);
}

// Tests that cross-origin redirection in multiple redirections by speculation
// rules should be canceled.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       CrossSiteMultipleRedirectionSpeculationRules) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering a URL that causes cross-origin redirection. The
  // cross-origin redirection should fail prerendering without an opt-in header.
  const GURL kRedirectedUrl = GetSameSiteCrossOriginUrl(
      "/prerender/prerender_with_opt_in_header.html?prerender");
  const GURL kRedirectedUrl2 =
      GetCrossSiteUrl("/server-redirect?" + kRedirectedUrl.spec());
  const GURL kPrerenderingUrl =
      GetUrl("/server-redirect?" + kRedirectedUrl2.spec());
  test::PrerenderHostObserver host_observer(*web_contents_impl(),
                                            kPrerenderingUrl);
  AddPrerenderAsync(kPrerenderingUrl);
  host_observer.WaitForDestroyed();

  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl), 0);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl2), 0);
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  EXPECT_FALSE(HasHostForUrl(kRedirectedUrl));
  EXPECT_FALSE(HasHostForUrl(kRedirectedUrl2));
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kCrossSiteRedirectInInitialNavigation);
}

// Tests that same-site cross-origin navigation by speculation rules can be
// prerendered with the feature enabled.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       CheckSameSiteCrossOriginSpeculationRulesPrerender) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl =
      GetSameSiteCrossOriginUrl("/prerender/prerender_with_opt_in_header.html");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Add a same-site cross-origin prerender rule.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  NavigatePrimaryPage(kPrerenderingUrl);
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
  // Activating the prerendered page should not issue a request.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);
}

// Tests that same-site cross-origin redirection by speculation rules is
// allowed.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       SameSiteCrossOriginSpeculationRulesRedirection) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering a URL that causes same-origin redirection.
  const GURL kRedirectedUrl = GetSameSiteCrossOriginUrl(
      "/prerender/prerender_with_opt_in_header.html?prerender");
  const GURL kPrerenderingUrl =
      GetSameSiteCrossOriginUrl("/server-redirect?" + kRedirectedUrl.spec());
  RedirectChainObserver redirect_chain_observer(*shell()->web_contents(),
                                                kRedirectedUrl);
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  ASSERT_EQ(GetRequestCount(kRedirectedUrl), 0);
  AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl), 1);

  ASSERT_EQ(2u, redirect_chain_observer.redirect_chain().size());
  EXPECT_EQ(kPrerenderingUrl, redirect_chain_observer.redirect_chain()[0]);
  EXPECT_EQ(kRedirectedUrl, redirect_chain_observer.redirect_chain()[1]);

  // The prerender host should be registered for the initial request URL, not
  // the redirected URL.
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl));
  EXPECT_FALSE(HasHostForUrl(kRedirectedUrl));

  RedirectChainObserver activation_redirect_chain_observer(
      *shell()->web_contents(), kRedirectedUrl);
  NavigationHandleObserver activation_observer(web_contents(),
                                               kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);
  ASSERT_EQ(1u, activation_redirect_chain_observer.redirect_chain().size());
  EXPECT_EQ(kRedirectedUrl,
            activation_redirect_chain_observer.redirect_chain()[0]);

  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kRedirectedUrl);
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);
  // Activating the prerendered page should not issue a request.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl), 1);

  // Cross-check that in case redirection when the prerender navigates and
  // user ends up navigating to the redirected URL. accurate_triggering is
  // true.
  ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
  ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder().BuildEntry(
      ukm_source_id, PreloadingType::kPrerender,
      PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
      PreloadingTriggeringOutcome::kSuccess,
      PreloadingFailureReason::kUnspecified,
      /*accurate=*/true,
      /*ready_time=*/kMockElapsedTime,
      blink::mojom::SpeculationEagerness::kEager)});
}

// Tests that multiple same-site cross-origin redirections by speculation rules
// is allowed, and only the terminal one is checked for the opt in header.
IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    SameSiteCrossOriginSpeculationRulesMultipleRedirections) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering a URL that causes same-origin redirection.
  const GURL kRedirectedUrl = GetSameSiteCrossOriginUrl(
      "/prerender/prerender_with_opt_in_header.html?prerender");
  const GURL kRedirectedUrl2 =
      GetSameSiteCrossOriginUrl("/server-redirect?" + kRedirectedUrl.spec());
  const GURL kPrerenderingUrl =
      GetSameSiteCrossOriginUrl("/server-redirect?" + kRedirectedUrl2.spec());
  RedirectChainObserver redirect_chain_observer(*shell()->web_contents(),
                                                kRedirectedUrl);
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  ASSERT_EQ(GetRequestCount(kRedirectedUrl), 0);
  ASSERT_EQ(GetRequestCount(kRedirectedUrl2), 0);
  AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl), 1);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl2), 1);

  ASSERT_EQ(3u, redirect_chain_observer.redirect_chain().size());
  EXPECT_EQ(kPrerenderingUrl, redirect_chain_observer.redirect_chain()[0]);
  EXPECT_EQ(kRedirectedUrl2, redirect_chain_observer.redirect_chain()[1]);
  EXPECT_EQ(kRedirectedUrl, redirect_chain_observer.redirect_chain()[2]);

  // The prerender host should be registered for the initial request URL, not
  // the redirected URL.
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl));
  EXPECT_FALSE(HasHostForUrl(kRedirectedUrl));
  EXPECT_FALSE(HasHostForUrl(kRedirectedUrl2));

  RedirectChainObserver activation_redirect_chain_observer(
      *shell()->web_contents(), kRedirectedUrl);
  NavigationHandleObserver activation_observer(web_contents(),
                                               kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);
  ASSERT_EQ(1u, activation_redirect_chain_observer.redirect_chain().size());
  EXPECT_EQ(kRedirectedUrl,
            activation_redirect_chain_observer.redirect_chain()[0]);

  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kRedirectedUrl);
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);
  // Activating the prerendered page should not issue a request.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl), 1);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl2), 1);

  // Cross-check that in case redirection when the prerender navigates and user
  // ends up navigating to the redirected URL. accurate_triggering is true.
  ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
  ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder().BuildEntry(
      ukm_source_id, PreloadingType::kPrerender,
      PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
      PreloadingTriggeringOutcome::kSuccess,
      PreloadingFailureReason::kUnspecified,
      /*accurate=*/true,
      /*ready_time=*/kMockElapsedTime,
      blink::mojom::SpeculationEagerness::kEager)});
}

void PrerenderBrowserTest::TestEmbedderTriggerWithUnsupportedScheme(
    const GURL& prerendering_url) {
  const GURL initial_url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  ASSERT_FALSE(prerendering_url.SchemeIsHTTPOrHTTPS());

  auto* preloading_data =
      PreloadingData::GetOrCreateForWebContents(web_contents_impl());
  PreloadingPredictor preloading_predictor(100, "Embedder");
  PreloadingURLMatchCallback same_url_matcher =
      PreloadingData::GetSameURLMatcher(prerendering_url);
  PreloadingAttempt* preloading_attempt = preloading_data->AddPreloadingAttempt(
      preloading_predictor, PreloadingType::kPrerender,
      std::move(same_url_matcher),
      /*planned_max_preloading_type=*/std::nullopt,
      web_contents_impl()->GetPrimaryMainFrame()->GetPageUkmSourceId());

  // Start prerendering by embedder triggered prerendering.
  std::unique_ptr<PrerenderHandle> prerender_handle =
      AddEmbedderTriggeredPrerenderAsync(prerendering_url, preloading_attempt);
  EXPECT_FALSE(prerender_handle);

  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kInvalidSchemeNavigation, 1);

  // Navigate primary page to flush the metrics.
  const GURL navigated_url = GetUrl("/empty.html?navigated");
  ASSERT_TRUE(NavigateToURL(shell(), navigated_url));

  auto attempt_ukm_entry_builder =
      std::make_unique<test::PreloadingAttemptUkmEntryBuilder>(
          preloading_predictor);
  ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder->BuildEntry(
      PrimaryPageSourceId(), PreloadingType::kPrerender,
      PreloadingEligibility::kHttpOrHttpsOnly,
      PreloadingHoldbackStatus::kUnspecified,
      PreloadingTriggeringOutcome::kUnspecified,
      PreloadingFailureReason::kUnspecified,
      /*accurate=*/false,
      /*ready_time=*/std::nullopt,
      /*eagerness=*/std::nullopt)});
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       EmbedderTrigger_UnsupportedScheme_ViewSource) {
  const GURL prerendering_url =
      GURL("view-source:" + GetUrl("/empty.html?prerender").spec());
  TestEmbedderTriggerWithUnsupportedScheme(prerendering_url);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       EmbedderTrigger_UnsupportedScheme_DataUrl) {
  // The content is "<h1>Hello, World!</h1>".
  const GURL prerendering_url(
      "data:text/html,%3Ch1%3EHello%2C%20World%21%3C%2Fh1%3E");
  TestEmbedderTriggerWithUnsupportedScheme(prerendering_url);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       EmbedderTrigger_SameOriginRedirection) {
  const GURL initial_url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  const GURL redirected_url_node_2 = GetUrl("/empty.html?prerender");
  const GURL redirected_url_node_1 =
      GetUrl("/server-redirect?" + redirected_url_node_2.spec());
  const GURL prerender_initial_url =
      GetUrl("/server-redirect?" + redirected_url_node_1.spec());

  RedirectChainObserver redirect_chain_observer(*shell()->web_contents(),
                                                redirected_url_node_2);

  // Start prerendering by embedder triggered prerendering.
  std::unique_ptr<PrerenderHandle> prerender_handle =
      AddEmbedderTriggeredPrerender(prerender_initial_url);
  ASSERT_EQ(3u, redirect_chain_observer.redirect_chain().size());

  // Prerender is not canceled.
  EXPECT_TRUE(HasHostForUrl(prerender_initial_url));

  // Regression test for https://crbug.com/1211274. Make sure that we don't
  // crash when activating a prerendered page which performed a same-origin
  // redirect.
  RedirectChainObserver activation_redirect_chain_observer(
      *shell()->web_contents(), redirected_url_node_2);
  test::PrerenderHostObserver prerender_observer(*web_contents_impl(),
                                                 prerender_initial_url);
  shell()->web_contents()->OpenURL(
      OpenURLParams(
          prerender_initial_url, Referrer(), WindowOpenDisposition::CURRENT_TAB,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});

  prerender_observer.WaitForActivation();
  ASSERT_EQ(1u, activation_redirect_chain_observer.redirect_chain().size());
  EXPECT_EQ(redirected_url_node_2,
            activation_redirect_chain_observer.redirect_chain()[0]);
}

// If there is a cross-origin url in the redirection chain, tests prerender
// should be canceled.
IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    EmbedderTrigger_CancelIfCrossOriginUrlInRedirectionChain) {
  GURL initial_url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Prerendering a url that will be redirected to same_origin_redirected_url
  // and then cross_origin_redirected_url.
  GURL cross_origin_redirected_url = GetCrossSiteUrl("/empty.html");
  GURL same_origin_redirected_url =
      GetUrl("/server-redirect?" + cross_origin_redirected_url.spec());
  GURL prerendering_initial_url =
      GetUrl("/server-redirect?" + same_origin_redirected_url.spec());

  RedirectChainObserver redirect_chain_observer(*shell()->web_contents(),
                                                cross_origin_redirected_url);

  // Start prerendering by embedder triggered prerendering.
  std::unique_ptr<PrerenderHandle> prerender_handle =
      AddEmbedderTriggeredPrerender(prerendering_initial_url);

  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kCrossSiteRedirectInInitialNavigation, 1);
  EXPECT_FALSE(HasHostForUrl(prerendering_initial_url));
}

std::unique_ptr<PrerenderHandle>
PrerenderEmbedderTriggeredCrossOriginRedirectionPage(
    WebContentsImpl& web_contents,
    const GURL& prerendering_url,
    const GURL& cross_origin_url) {
  EXPECT_FALSE(url::IsSameOriginWith(prerendering_url, cross_origin_url));
  RedirectChainObserver redirect_chain_observer{web_contents, cross_origin_url};

  // Start prerendering by embedder triggered prerendering.
  std::unique_ptr<PrerenderHandle> prerender_handle =
      web_contents.StartPrerendering(
          prerendering_url, PreloadingTriggerType::kEmbedder,
          "EmbedderSuffixForTest",
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          /*should_warm_up_compositor=*/false,
          PreloadingHoldbackStatus::kUnspecified,
          /*preloading_attempt=*/nullptr, /*url_match_predicate=*/{},
          /*prerender_navigation_handle_callback=*/{});
  EXPECT_TRUE(prerender_handle);
  test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(web_contents,
                                                            prerendering_url);
  EXPECT_EQ(2u, redirect_chain_observer.redirect_chain().size());
  return prerender_handle;
}

namespace {

class FrameDisplayStateChangedObserver : public WebContentsObserver {
 public:
  explicit FrameDisplayStateChangedObserver(RenderFrameHost& host)
      : WebContentsObserver(WebContents::FromRenderFrameHost(&host)),
        target_host_(&host) {}

  void WaitForFrameDisplayStateChanged() {
    if (changed_count_ > 0) {
      changed_count_--;
    } else {
      base::RunLoop loop;
      callback_ = loop.QuitClosure();
      loop.Run();
    }
  }

  void FrameDisplayStateChanged(RenderFrameHost* host,
                                bool is_display_none) override {
    if (host == target_host_) {
      if (callback_)
        std::move(callback_).Run();
      else
        changed_count_++;
    }
  }

  int changed_count_ = 0;
  const raw_ptr<RenderFrameHost> target_host_;
  base::OnceClosure callback_;
};

}  // namespace

// Tests that FrameOwnerProperties are in sync after activation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, FrameOwnerPropertiesDisplayNone) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/doc-with-display-none-iframe.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  EXPECT_TRUE(AddTestUtilJS(current_frame_host()));

  // Start prerendering a document with a display:none iframe.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHost* prerender_frame_host = GetPrerenderedMainFrameHost(host_id);
  EXPECT_TRUE(ExecJs(prerender_frame_host, "loaded;"));

  // The iframe is at "/empty.html". It should be display none.
  RenderFrameHost* iframe_host = FindRenderFrameHost(
      prerender_frame_host->GetPage(), GetUrl("/empty.html"));
  EXPECT_FALSE(prerender_frame_host->IsFrameDisplayNone());
  EXPECT_TRUE(iframe_host->IsFrameDisplayNone());

  // Activate.
  NavigatePrimaryPage(kPrerenderingUrl);
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);

  // The frames should still have the same display properties.
  EXPECT_FALSE(prerender_frame_host->IsFrameDisplayNone());
  EXPECT_TRUE(iframe_host->IsFrameDisplayNone());

  // Change the display properties.
  FrameDisplayStateChangedObserver obs(*iframe_host);
  EXPECT_TRUE(
      ExecJs(prerender_frame_host,
             "document.querySelector('iframe').style = 'display: block;'"));
  obs.WaitForFrameDisplayStateChanged();

  EXPECT_FALSE(prerender_frame_host->IsFrameDisplayNone());
  EXPECT_FALSE(iframe_host->IsFrameDisplayNone());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, TriggeredPrerenderUkm) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // PrerenderPageLoad metric should not be recorded yet.
  EXPECT_EQ(0u,
            ukm_recorder
                .GetEntriesByName(ukm::builders::PrerenderPageLoad::kEntryName)
                .size());

  // Start a prerender.
  ASSERT_TRUE(AddPrerender(kPrerenderingUrl));

  // PrerenderPageLoad:TriggeredPrerender is recorded for the initiator page
  // load.
  const std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>
      entries = ukm_recorder.GetEntriesByName(
          ukm::builders::PrerenderPageLoad::kEntryName);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId(),
            entries.front()->source_id);
  ukm_recorder.ExpectEntryMetric(
      entries.front(),
      ukm::builders::PrerenderPageLoad::kTriggeredPrerenderName, 1);
}

// Tests that background color in a prerendered page does not affect
// the primary page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, ColorSchemeDarkInNonPrimaryPage) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/color-scheme-dark.html");

  // Expect initial page background color to be white.
  BackgroundColorChangeWaiter empty_page_background_waiter(web_contents());

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  // Wait for the page background to change to white.
  empty_page_background_waiter.Wait();

  {
    // Now set up a mock observer for BackgroundColorChanged, to test if the
    // mocked observer executes BackgroundColorChanged for the prerendered page.
    testing::NiceMock<MockWebContentsObserver> background_color_observer(
        web_contents());
    EXPECT_CALL(background_color_observer, OnBackgroundColorChanged())
        .Times(Exactly(0));

    AddPrerender(kPrerenderingUrl);
  }

  BackgroundColorChangeWaiter prerendered_page_background_waiter(
      web_contents());
  // Now set up a mock observer for BackgroundColorChanged, to test if the
  // mocked observer executes BackgroundColorChanged when activating the
  // prerendered page.
  testing::NiceMock<MockWebContentsObserver> background_color_observer(
      web_contents());
  EXPECT_CALL(background_color_observer, OnBackgroundColorChanged())
      .Times(Exactly(1));
  NavigatePrimaryPage(kPrerenderingUrl);
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
  // Wait for the page background to change.
  prerendered_page_background_waiter.Wait();
}

// TODO(b/335786567): Flaky on win-asan.
#if (BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER))
#define MAYBE_ThemeColorSchemeChangeInNonPrimaryPage \
  DISABLED_ThemeColorSchemeChangeInNonPrimaryPage
#else
#define MAYBE_ThemeColorSchemeChangeInNonPrimaryPage \
  ThemeColorSchemeChangeInNonPrimaryPage
#endif
// Tests that theme color in a prerendered page does not affect
// the primary page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MAYBE_ThemeColorSchemeChangeInNonPrimaryPage) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/theme_color.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  {
    // Now set up a mock observer for DidChangeThemeColor, to test if the
    // mocked observer executes DidChangeThemeColor for the prerendered page.
    testing::NiceMock<MockWebContentsObserver> theme_color_observer(
        web_contents());
    EXPECT_CALL(theme_color_observer, DidChangeThemeColor()).Times(Exactly(0));

    AddPrerender(kPrerenderingUrl);
  }

  ThemeChangeWaiter theme_change_waiter(web_contents());
  testing::NiceMock<MockWebContentsObserver> theme_color_observer(
      web_contents());
  EXPECT_CALL(theme_color_observer, DidChangeThemeColor()).Times(Exactly(1));

  NavigatePrimaryPage(kPrerenderingUrl);
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
  theme_change_waiter.Wait();
}

// Tests that text autosizer works per page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       TextAutosizerInfoChangeInNonPrimaryPage) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/title1.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  RenderFrameHostImpl* primary_frame_host = current_frame_host();

  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerender_frame_host =
      GetPrerenderedMainFrameHost(host_id);

  // Update the autosizer page info in the prerendering page.
  blink::mojom::TextAutosizerPageInfo prerender_page_info(
      /*main_frame_width=*/320,
      /*main_frame_layout_width=*/480,
      /*device_scale_adjustment=*/1.f);
  prerender_frame_host->TextAutosizerPageInfoChanged(
      prerender_page_info.Clone());

  // Only the prerendering page's autosizer info should be updated.
  EXPECT_TRUE(prerender_page_info.Equals(
      prerender_frame_host->GetPage().text_autosizer_page_info()));
  EXPECT_FALSE(prerender_page_info.Equals(
      primary_frame_host->GetPage().text_autosizer_page_info()));

  // After being activated, the prerendered page becomes the primary page, so
  // the page info of the primary page should equal `prerender_page_info`.
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_TRUE(prerender_page_info.Equals(
      current_frame_host()->GetPage().text_autosizer_page_info()));
}

// Check that the prerendered page window.name is maintained after activation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       VerifyFrameNameMaintainedAfterActivation) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/title1.html");

  // 1. Load initiator page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // 2. Load prerender.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerendered_render_frame_host =
      GetPrerenderedMainFrameHost(host_id);

  // 3. Set window.name.
  ASSERT_TRUE(
      ExecJs(prerendered_render_frame_host, "window.name = 'prerender_page'"));

  EXPECT_EQ(prerendered_render_frame_host->GetFrameName(), "prerender_page");
  EXPECT_EQ(current_frame_host()->GetFrameName(), "");

  // 4. Activate prerender.
  test::PrerenderHostObserver host_observer(*web_contents(), kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_TRUE(host_observer.was_activated());
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);

  // 5. Ensure that the window.name is preserved.
  EXPECT_EQ(current_frame_host()->GetFrameName(), "prerender_page");
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, ActivateWhileReloadingSubframe) {
  const char kSubframePath[] = "/title1.html";
  net::test_server::ControllableHttpResponse first_response(
      embedded_test_server(), kSubframePath);
  net::test_server::ControllableHttpResponse second_response(
      embedded_test_server(), kSubframePath);

  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerenderingUrl =
      embedded_test_server()->GetURL("/page_with_iframe.html");
  const GURL kSubframeUrl = embedded_test_server()->GetURL(kSubframePath);

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());
  AddPrerenderAsync(kPrerenderingUrl);

  // Handle a response for the subframe main resource.
  first_response.WaitForRequest();
  first_response.Send(net::HTTP_OK, "");
  first_response.Done();

  // Now we can wait for the prerendering navigation finishes.
  registry_observer.WaitForTrigger(kPrerenderingUrl);
  FrameTreeNodeId host_id = GetHostForUrl(kPrerenderingUrl);
  WaitForPrerenderLoadCompletion(host_id);

  RenderFrameHostImpl* prerender_rfh = GetPrerenderedMainFrameHost(host_id);
  RenderFrameHostImpl* child_rfh =
      prerender_rfh->child_at(0)->current_frame_host();
  EXPECT_EQ(child_rfh->GetLastCommittedURL(), kSubframeUrl);

  // Reload the iframe.
  EXPECT_TRUE(ExecJs(child_rfh, "window.location.reload();"));
  second_response.WaitForRequest();
  // Do not finish the second response to execute activation during the reload.

  // Ensure that activation works even while the iframe is under the reload.
  TestNavigationObserver nav_observer(web_contents());
  EXPECT_TRUE(
      ExecJs(web_contents(), JsReplace("location = $1", kPrerenderingUrl)));
  second_response.Send(net::HTTP_OK, "");
  second_response.Done();
  nav_observer.WaitForNavigationFinished();
}

// Check that the inactive RFH shouldn't update UserActivation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, DoNotUpdateUserActivationState) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/title1.html");

  // 1. Load initiator page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // 2. Load prerender.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerendered_rfh = GetPrerenderedMainFrameHost(host_id);

  EXPECT_FALSE(
      current_frame_host()->frame_tree_node()->HasStickyUserActivation());
  EXPECT_FALSE(prerendered_rfh->frame_tree_node()->HasStickyUserActivation());

  // 3. Try to set the user activation bits to the prerendered RFH.
  prerendered_rfh->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);
  EXPECT_FALSE(prerendered_rfh->frame_tree_node()->HasStickyUserActivation());
  EXPECT_FALSE(prerendered_rfh->HasTransientUserActivation());

  EXPECT_FALSE(
      current_frame_host()->frame_tree_node()->HasStickyUserActivation());
  EXPECT_FALSE(
      current_frame_host()->frame_tree_node()->HasTransientUserActivation());

  // 4. Set the user activation bits to the primary RFH.
  current_frame_host()->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);
  EXPECT_TRUE(
      current_frame_host()->frame_tree_node()->HasStickyUserActivation());

  EXPECT_FALSE(prerendered_rfh->frame_tree_node()->HasStickyUserActivation());
}

// Tests that prerendering is cancelled when a mixed content subframe is
// detected.
IN_PROC_BROWSER_TEST_P(PrerenderTargetAgnosticBrowserTest, MixedContent) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerendering");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Make a prerendered page.
  FrameTreeNodeId host_id = prerender_helper()->AddPrerender(
      kPrerenderingUrl, /*eagerness=*/std::nullopt, GetTargetHint());
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
  auto* prerendered_rfh =
      test::PrerenderTestHelper::GetPrerenderedMainFrameHost(
          *prerender_web_contents, host_id);
  CHECK(prerendered_rfh);
  EXPECT_TRUE(AddTestUtilJS(prerendered_rfh));

  test::PrerenderHostObserver host_observer(*prerender_web_contents, host_id);

  // Make a mixed content iframe.
  std::ignore =
      ExecJs(prerendered_rfh,
             "add_iframe_async('http://a.test/empty.html?prerendering')",
             EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES);

  host_observer.WaitForDestroyed();
  if (GetTargetHint() == "_blank") {
    EXPECT_FALSE(prerender_helper()->HasNewTabHandle(host_id));
  } else {
    EXPECT_TRUE(prerender_helper()
                    ->GetHostForUrl(*prerender_web_contents, kPrerenderingUrl)
                    .is_null());
  }

  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kMixedContent);
}

// Check that the Content-Security-Policy set via HTTP header applies after the
// activation. This test verifies that that the web sandbox flags value is none.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       ActivatePageWithCspHeaderFrameSrc) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl =
      GetUrl("/set-header?Content-Security-Policy: frame-src 'none'");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerendered_render_frame_host =
      GetPrerenderedMainFrameHost(host_id);

  // Check that CSP was set on the prerendered page prior to activation.
  {
    const std::vector<network::mojom::ContentSecurityPolicyPtr>& root_csp_pre =
        prerendered_render_frame_host->policy_container_host()
            ->policies()
            .content_security_policies;
    EXPECT_EQ(1u, root_csp_pre.size());
    EXPECT_EQ("frame-src 'none'", root_csp_pre[0]->header->header_value);
    EXPECT_EQ(prerendered_render_frame_host->active_sandbox_flags(),
              network::mojom::WebSandboxFlags::kNone);
  }

  test::PrerenderHostObserver host_observer(*web_contents(), kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_TRUE(host_observer.was_activated());
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);

  // Check that CSP was set on the prerendered page after activation.
  {
    const std::vector<network::mojom::ContentSecurityPolicyPtr>& root_csp_post =
        current_frame_host()
            ->policy_container_host()
            ->policies()
            .content_security_policies;
    EXPECT_EQ(1u, root_csp_post.size());
    EXPECT_EQ("frame-src 'none'", root_csp_post[0]->header->header_value);
    EXPECT_EQ(current_frame_host()->active_sandbox_flags(),
              network::mojom::WebSandboxFlags::kNone);
    EXPECT_EQ(static_cast<WebContentsImpl*>(web_contents())
                  ->GetPrimaryFrameTree()
                  .root()
                  ->active_sandbox_flags(),
              network::mojom::WebSandboxFlags::kNone);
  }
}

// Check that the Content-Security-Policy set via HTTP header applies after the
// activation. This test verifies that that the web sandbox flags value is set
// to allow scripts.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       ActivatePageWithCspHeaderSandboxFlags) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl =
      GetUrl("/set-header?Content-Security-Policy: sandbox allow-scripts");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerendered_render_frame_host =
      GetPrerenderedMainFrameHost(host_id);

  // Check that CSP was set on the prerendered page prior to activation.
  {
    const std::vector<network::mojom::ContentSecurityPolicyPtr>& root_csp_pre =
        prerendered_render_frame_host->policy_container_host()
            ->policies()
            .content_security_policies;
    EXPECT_EQ(1u, root_csp_pre.size());
    EXPECT_EQ("sandbox allow-scripts", root_csp_pre[0]->header->header_value);
    EXPECT_EQ(prerendered_render_frame_host->active_sandbox_flags(),
              network::mojom::WebSandboxFlags::kAll &
                  ~network::mojom::WebSandboxFlags::kScripts &
                  ~network::mojom::WebSandboxFlags::kAutomaticFeatures);
  }

  test::PrerenderHostObserver host_observer(*web_contents(), kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_TRUE(host_observer.was_activated());
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);

  // Check that CSP was set on the prerendered page after activation.
  {
    const std::vector<network::mojom::ContentSecurityPolicyPtr>& root_csp_post =
        current_frame_host()
            ->policy_container_host()
            ->policies()
            .content_security_policies;
    EXPECT_EQ(1u, root_csp_post.size());
    EXPECT_EQ("sandbox allow-scripts", root_csp_post[0]->header->header_value);
    EXPECT_EQ(current_frame_host()->active_sandbox_flags(),
              network::mojom::WebSandboxFlags::kAll &
                  ~network::mojom::WebSandboxFlags::kScripts &
                  ~network::mojom::WebSandboxFlags::kAutomaticFeatures);
    EXPECT_EQ(static_cast<WebContentsImpl*>(web_contents())
                  ->GetPrimaryFrameTree()
                  .root()
                  ->active_sandbox_flags(),
              network::mojom::WebSandboxFlags::kAll &
                  ~network::mojom::WebSandboxFlags::kScripts &
                  ~network::mojom::WebSandboxFlags::kAutomaticFeatures);
  }
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, VerifyPrerenderProcessVisibility) {
  // Navigate the primary main frame to an initial page.
  const GURL kInitialUrl = GetUrl("/empty.html?initial");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHost* prerender_frame_host = GetPrerenderedMainFrameHost(host_id);
  RenderProcessHost* prerender_process_host =
      prerender_frame_host->GetProcess();
  ASSERT_NE(prerender_frame_host, nullptr);
  // Ensure that a prerender process is backgrounded. This will put prerender
  // processes in lower priority compared to other active processes. (See
  // https://crbug.com/1211665)
  EXPECT_TRUE(prerender_process_host->GetPriority() ==
              base::Process::Priority::kBestEffort);

  // Activate the prerendered page.
  test::PrerenderHostObserver host_observer(*web_contents(), kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_TRUE(host_observer.was_activated());
  // Expect the change in the ChildProcessLauncherPriority to increase priority.
  EXPECT_NE(prerender_process_host->GetPriority(),
            base::Process::Priority::kBestEffort);
}

class PrerenderPurposePrefetchBrowserTest : public PrerenderBrowserTest {
 public:
  PrerenderPurposePrefetchBrowserTest() = default;
  ~PrerenderPurposePrefetchBrowserTest() override = default;

  void SetUp() override {
    ssl_server().RegisterRequestHandler(
        base::BindRepeating(&HandleCorsRequest));
    PrerenderBrowserTest::SetUp();
  }

  static std::unique_ptr<net::test_server::HttpResponse> HandleCorsRequest(
      const net::test_server::HttpRequest& request) {
    // The "Purpose: prefetch" header shouldn't cause CORS preflights.
    EXPECT_NE(request.method_string, "OPTIONS");

    // Ignore if the request is not cross origin.
    //
    // Note: Checking the origin of `request.GetURL()` doesn't work here because
    // the host part of the URL is translated (e.g., "a.test" to "127.0.0.1")
    // based on the host resolver rule before this point.
    if (request.relative_url.find("cors") == std::string::npos)
      return nullptr;

    // Serves a fake response with the ACAO header.
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    response->set_code(net::HTTP_OK);
    response->set_content("");
    response->set_content_type("text/plain");
    return response;
  }

  bool TestPurposePrefetchHeader(const GURL& url) {
    net::test_server::HttpRequest::HeaderMap headers = GetRequestHeaders(url);
    auto it = headers.find("Purpose");
    if (it == headers.end()) {
      return false;
    }
    EXPECT_EQ("prefetch", it->second);

    it = headers.find("Sec-Purpose");
    if (it == headers.end()) {
      return false;
    }
    EXPECT_EQ("prefetch;prerender", it->second);
    return true;
  }
};

// Tests that a request for the initial prerender navigation has the
// "Purpose: prefetch" header.
// TODO(nhiroki): Move this test to WPT.
IN_PROC_BROWSER_TEST_F(PrerenderPurposePrefetchBrowserTest, InitialNavigation) {
  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("/empty.html")));

  // Start prerendering.
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");
  AddPrerender(kPrerenderingUrl);

  // The prerender request should have the header.
  EXPECT_TRUE(TestPurposePrefetchHeader(kPrerenderingUrl));
}

// Tests that a redirected request for the initial prerender navigation has the
// "Purpose: prefetch" header.
// TODO(nhiroki): Move this test to WPT.
IN_PROC_BROWSER_TEST_F(PrerenderPurposePrefetchBrowserTest,
                       RedirectionOnInitialNavigation) {
  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("/empty.html")));

  // Start prerendering a URL that causes same-origin redirection.
  const GURL kRedirectedUrl = GetUrl("/empty.html?prerender");
  const GURL kPrerenderingUrl =
      GetUrl("/server-redirect?" + kRedirectedUrl.spec());
  RedirectChainObserver redirect_chain_observer(*shell()->web_contents(),
                                                kRedirectedUrl);
  AddPrerender(kPrerenderingUrl);
  ASSERT_EQ(2u, redirect_chain_observer.redirect_chain().size());
  EXPECT_EQ(kPrerenderingUrl, redirect_chain_observer.redirect_chain()[0]);
  EXPECT_EQ(kRedirectedUrl, redirect_chain_observer.redirect_chain()[1]);

  // Both the initial request and the redirected request should have the
  // "Purpose: prefetch" header.
  EXPECT_TRUE(TestPurposePrefetchHeader(kPrerenderingUrl));
  EXPECT_TRUE(TestPurposePrefetchHeader(kRedirectedUrl));
}

// Tests that requests from a prerendered page have the "Purpose: prefetch"
// header.
// TODO(nhiroki): Move this test to WPT.
IN_PROC_BROWSER_TEST_F(PrerenderPurposePrefetchBrowserTest, ResourceRequests) {
  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("/empty.html")));

  // Start prerendering.
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/purpose_prefetch_header.html");
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostWrapper prerender_main_frame(
      GetPrerenderedMainFrameHost(host_id));

  // The prerender request should have the "Purpose: prefetch" header.
  TestPurposePrefetchHeader(kPrerenderingUrl);

  // Issue iframe and subresource requests in the prerendered page.
  EXPECT_TRUE(ExecJs(prerender_main_frame.get(), "run('before');",
                     EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Requests from the prerendered page should have the header.
  EXPECT_TRUE(TestPurposePrefetchHeader(
      GetUrl("/prerender/purpose_prefetch_header_iframe.html?before")));
  EXPECT_TRUE(
      TestPurposePrefetchHeader(GetUrl("/prerender/missing.jpg?before")));
  EXPECT_TRUE(
      TestPurposePrefetchHeader(GetUrl("/prerender/missing.txt?before")));
  EXPECT_TRUE(TestPurposePrefetchHeader(GetUrl("/empty.html?before")));
  EXPECT_TRUE(TestPurposePrefetchHeader(
      GetUrl("/prerender/iframe-missing.jpg?before")));
  EXPECT_TRUE(TestPurposePrefetchHeader(
      GetUrl("/prerender/iframe-missing.txt?before")));

  // Issue a cross-origin subresource request in the prerendered page. The
  // request should have the header.
  GURL cross_origin_url1 =
      GetCrossSiteUrl("/prerender/cors-missing.txt?before");
  EXPECT_TRUE(ExecJs(prerender_main_frame.get(),
                     "request('" + cross_origin_url1.spec() + "');",
                     EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(TestPurposePrefetchHeader(cross_origin_url1));

  // Activate the prerendered page.
  test::PrerenderHostObserver host_observer(*web_contents(), kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_TRUE(host_observer.was_activated());

  // Issue iframe and subresource requests in the activated page.
  EXPECT_TRUE(ExecJs(prerender_main_frame.get(), "run('after');",
                     EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Requests from the activated page should not have the header.
  EXPECT_FALSE(TestPurposePrefetchHeader(
      GetUrl("/prerender/purpose_prefetch_header_iframe.html?after")));
  EXPECT_FALSE(
      TestPurposePrefetchHeader(GetUrl("/prerender/missing.jpg?after")));
  EXPECT_FALSE(
      TestPurposePrefetchHeader(GetUrl("/prerender/missing.txt?after")));
  EXPECT_FALSE(TestPurposePrefetchHeader(GetUrl("/empty.html?after")));
  EXPECT_FALSE(
      TestPurposePrefetchHeader(GetUrl("/prerender/iframe-missing.jpg?after")));
  EXPECT_FALSE(
      TestPurposePrefetchHeader(GetUrl("/prerender/iframe-missing.txt?after")));

  // Issue a cross-origin subresource request in the activated page. The request
  // should not have the header.
  GURL cross_origin_url2 = GetCrossSiteUrl("/prerender/cors-missing.txt?after");
  EXPECT_TRUE(ExecJs(prerender_main_frame.get(),
                     "request('" + cross_origin_url2.spec() + "');",
                     EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_FALSE(TestPurposePrefetchHeader(cross_origin_url2));
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, EnterFullscreen) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerendering");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  auto* prerendered_rfh = GetPrerenderedMainFrameHost(host_id);

  // We should disallow to enter Fullscreen by the inactive RFH.
  prerendered_rfh->EnterFullscreen(
      blink::mojom::FullscreenOptions::New(),
      base::BindOnce([](bool value) { EXPECT_FALSE(value); }));
  EXPECT_FALSE(web_contents_impl()->IsFullscreen());
}

namespace {
class TestJavaScriptDialogManager : public JavaScriptDialogManager,
                                    public WebContentsDelegate {
 public:
  TestJavaScriptDialogManager() = default;
  ~TestJavaScriptDialogManager() override = default;

  // WebContentsDelegate overrides
  JavaScriptDialogManager* GetJavaScriptDialogManager(
      WebContents* source) override {
    return this;
  }

  // JavaScriptDialogManager overrides
  void RunJavaScriptDialog(WebContents* web_contents,
                           RenderFrameHost* render_frame_host,
                           JavaScriptDialogType dialog_type,
                           const std::u16string& message_text,
                           const std::u16string& default_prompt_text,
                           DialogClosedCallback callback,
                           bool* did_suppress_message) override {}
  void RunBeforeUnloadDialog(WebContents* web_contents,
                             RenderFrameHost* render_frame_host,
                             bool is_reload,
                             DialogClosedCallback callback) override {}
  void CancelDialogs(WebContents* web_contents, bool reset_state) override {
    cancel_dialogs_called_ = true;
  }

  bool cancel_dialogs_called() { return cancel_dialogs_called_; }

 private:
  bool cancel_dialogs_called_ = false;
};

class PrerenderWithRenderDocumentBrowserTest : public PrerenderBrowserTest {
 public:
  PrerenderWithRenderDocumentBrowserTest() {
    InitAndEnableRenderDocumentFeature(
        &feature_list_,
        GetRenderDocumentLevelName(RenderDocumentLevel::kSubframe));
  }
  ~PrerenderWithRenderDocumentBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};
}  // namespace

IN_PROC_BROWSER_TEST_F(
    PrerenderWithRenderDocumentBrowserTest,
    ModalDialogShouldNotBeDismissedAfterPrerenderSubframeNavigation) {
  const GURL kPrerenderingUrl = GetUrl("/title1.html");
  const GURL kSubframeUrl1 = GetUrl("/empty.html");
  const GURL kSubframeUrl2 = GetUrl("/title2.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("/empty.html")));

  // Start prerendering.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHost* prerender_rfh = GetPrerenderedMainFrameHost(host_id);
  CHECK(prerender_rfh);
  AddTestUtilJS(prerender_rfh);

  // Add subframe in prerendering page.
  ASSERT_TRUE(
      ExecJs(prerender_rfh, JsReplace("add_iframe($1)", kSubframeUrl1)));

  // Setup test dialog manager and create dialog.
  TestJavaScriptDialogManager dialog_manager;
  web_contents_impl()->SetDelegate(&dialog_manager);
  web_contents_impl()->RunJavaScriptDialog(
      web_contents_impl()->GetPrimaryMainFrame(), u"", u"",
      JAVASCRIPT_DIALOG_TYPE_ALERT, false, base::NullCallback());

  // Navigate subframe (with render document enabled, this should cause a RFH
  // swap).
  TestNavigationManager subframe_nav_manager(web_contents(), kSubframeUrl2);
  ASSERT_TRUE(ExecJs(
      prerender_rfh,
      JsReplace("document.querySelector('iframe').src = $1", kSubframeUrl2)));
  ASSERT_TRUE(subframe_nav_manager.WaitForNavigationFinished());

  // We should not dismiss dialogs when the prerender's subframe navigates and
  // swaps its RFH.
  EXPECT_FALSE(dialog_manager.cancel_dialogs_called());

  // Clean up test dialog manager.
  web_contents_impl()->SetDelegate(nullptr);
  web_contents_impl()->SetJavaScriptDialogManagerForTesting(nullptr);
}

// Tests that NavigationHandle::GetNavigatingFrameType() returns the correct
// type in prerendering and after activation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, NavigationHandleFrameType) {
  {
    const GURL kInitialUrl = GetUrl("/empty.html");
    DidFinishNavigationObserver observer(
        web_contents(),
        base::BindLambdaForTesting([](NavigationHandle* navigation_handle) {
          EXPECT_TRUE(navigation_handle->IsInPrimaryMainFrame());
          CHECK_EQ(navigation_handle->GetNavigatingFrameType(),
                   FrameType::kPrimaryMainFrame);
        }));
    // Navigate to an initial page.
    ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  }

  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");
  {
    DidFinishNavigationObserver observer(
        web_contents(),
        base::BindLambdaForTesting([](NavigationHandle* navigation_handle) {
          EXPECT_TRUE(navigation_handle->IsInPrerenderedMainFrame());
          CHECK_EQ(navigation_handle->GetNavigatingFrameType(),
                   FrameType::kPrerenderMainFrame);
        }));
    // Start prerendering.
    AddPrerender(kPrerenderingUrl);
  }

  {
    DidFinishNavigationObserver observer(
        web_contents(),
        base::BindLambdaForTesting([](NavigationHandle* navigation_handle) {
          EXPECT_TRUE(navigation_handle->IsInPrimaryMainFrame());
          EXPECT_TRUE(navigation_handle->IsPrerenderedPageActivation());
          CHECK_EQ(navigation_handle->GetNavigatingFrameType(),
                   FrameType::kPrimaryMainFrame);
        }));
    NavigatePrimaryPage(kPrerenderingUrl);
  }
}

// Tests that NavigationHandle::IsRendererInitiated() returns RendererInitiated
// = true correctly.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       NavigationHandleIsRendererInitiatedTrue) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");
  {
    DidFinishNavigationObserver observer(
        web_contents(),
        base::BindLambdaForTesting([](NavigationHandle* navigation_handle) {
          EXPECT_TRUE(navigation_handle->IsInPrerenderedMainFrame());
          EXPECT_TRUE(navigation_handle->IsRendererInitiated());
        }));
    // Start prerendering.
    AddPrerender(kPrerenderingUrl);
  }
  NavigatePrimaryPage(kPrerenderingUrl);
}

// Tests that FrameTreeNode::has_received_user_gesture_before_nav_ is not set on
// the prerendered main frame or the activated main frame when the primary main
// frame doesn't have it.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       HasReceivedUserGestureBeforeNavigation) {
  // Navigate to an initial page.
  const GURL initial_url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // The primary main frame doesn't have the
  // has_received_user_gesture_before_nav bit.
  ASSERT_FALSE(current_frame_host()
                   ->frame_tree_node()
                   ->has_received_user_gesture_before_nav());

  // Start prerendering.
  const GURL prerendering_url = GetUrl("/empty.html?prerender");
  FrameTreeNodeId host_id = AddPrerender(prerendering_url);
  RenderFrameHostImpl* prerendered_render_frame_host =
      GetPrerenderedMainFrameHost(host_id);

  // The prerendered main frame should not have the bit.
  EXPECT_FALSE(prerendered_render_frame_host->frame_tree_node()
                   ->has_received_user_gesture_before_nav());

  // Activate the prerendered page.
  test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  NavigatePrimaryPage(prerendering_url);
  ASSERT_TRUE(host_observer.was_activated());

  // The activated main frame should not have the bit.
  EXPECT_FALSE(current_frame_host()
                   ->frame_tree_node()
                   ->has_received_user_gesture_before_nav());
}

// Tests that FrameTreeNode::has_received_user_gesture_before_nav_ is not
// propagated from the primary main frame to the prerendered main frame but it
// is propagated to the activated main frame.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       HasReceivedUserGestureBeforeNavigation_Propagation) {
  // Navigate to an initial page.
  const GURL initial_url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Set the has_received_user_gesture_before_nav bit on the primary main frame.
  current_frame_host()->HadStickyUserActivationBeforeNavigationChanged(true);
  ASSERT_TRUE(current_frame_host()
                  ->frame_tree_node()
                  ->has_received_user_gesture_before_nav());

  // Start prerendering.
  const GURL prerendering_url = GetUrl("/empty.html?prerender");
  FrameTreeNodeId host_id = AddPrerender(prerendering_url);
  RenderFrameHostImpl* prerendered_render_frame_host =
      GetPrerenderedMainFrameHost(host_id);

  // The prerendered main frame should not have the bit.
  EXPECT_FALSE(prerendered_render_frame_host->frame_tree_node()
                   ->has_received_user_gesture_before_nav());

  // Activate the prerendered page.
  test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  NavigatePrimaryPage(prerendering_url);
  ASSERT_TRUE(host_observer.was_activated());

  // The activated main frame should have the bit.
  EXPECT_TRUE(current_frame_host()
                  ->frame_tree_node()
                  ->has_received_user_gesture_before_nav());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       CancelPrerenderWhenIsOverridingUserAgentDiffers) {
  const std::string user_agent_override = "foo";

  // Navigate to an initial page.
  const GURL initial_url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Enable user agent override for future navigations.
  UserAgentInjector injector(shell()->web_contents(), user_agent_override);

  const GURL prerendering_url = GetUrl("/empty.html?prerender");

  // Start prerendering.
  const FrameTreeNodeId host_id = AddPrerender(prerendering_url);

  RenderFrameHostImpl* prerender_rfh =
      static_cast<RenderFrameHostImpl*>(GetPrerenderedMainFrameHost(host_id));
  EXPECT_EQ(user_agent_override, EvalJs(prerender_rfh, "navigator.userAgent"));

  // Stop overriding user agent from now on.
  injector.set_is_overriding_user_agent(false);

  // Activate the prerendered page.
  test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  NavigatePrimaryPage(prerendering_url);
  host_observer.WaitForDestroyed();

  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kActivationNavigationParameterMismatch);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.ActivationHeadersMismatch.SpeculationRule",
      -511888193, 1);
}

class PrerenderSpeculationRulesHoldbackBrowserTest
    : public PrerenderBrowserTest {
 public:
  PrerenderSpeculationRulesHoldbackBrowserTest() {
    prerender_helper()->SetHoldback(
        PreloadingType::kPrerender,
        content_preloading_predictor::kSpeculationRules, true);
  }
  ~PrerenderSpeculationRulesHoldbackBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(PrerenderSpeculationRulesHoldbackBrowserTest,
                       PrerenderHoldbackTest) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl` this should fail as we are in
  // holdback.
  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());
  AddPrerenderAsync(kPrerenderingUrl);

  // Wait for PrerenderHostRegistry to receive the holdback prerender
  // request, and it should be ignored.
  registry_observer.WaitForTrigger(kPrerenderingUrl);
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  NavigationHandleObserver activation_observer(web_contents(),
                                               kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);

  // Cross-check that PreloadingHoldbackStatus is correctly set.
  ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
  ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder().BuildEntry(
      ukm_source_id, PreloadingType::kPrerender,
      PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kHoldback,
      PreloadingTriggeringOutcome::kUnspecified,
      PreloadingFailureReason::kUnspecified,
      /*accurate=*/true,
      /*ready_time=*/std::nullopt,
      blink::mojom::SpeculationEagerness::kEager)});
}

class PrerenderFencedFrameBrowserTest : public PrerenderBrowserTest {
 public:
  PrerenderFencedFrameBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kFencedFrames, {}},
         {::features::kPrivacySandboxAdsAPIsOverride, {}},
         {blink::features::kFencedFramesAPIChanges, {}},
         {blink::features::kFencedFramesDefaultMode, {}}},
        {/* disabled_features */});
  }
  ~PrerenderFencedFrameBrowserTest() override = default;

  void SetUp() override {
    ssl_server().RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest,
        "/fenced-frame-with-speculation-rules",
        base::BindRepeating(HandleFencedFrameWithSpeculationRulesRequest)));
    ssl_server().RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest,
        "/fenced-frame-with-speculation-rules-header",
        base::BindRepeating(
            HandleFencedFrameWithSpeculationRulesHeaderRequest)));
    ssl_server().RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/prerender.json",
        base::BindRepeating(HandlePrerenderJsonRequest)));
    PrerenderBrowserTest::SetUp();
  }

  static std::unique_ptr<net::test_server::HttpResponse>
  HandleFencedFrameWithSpeculationRulesRequest(
      const net::test_server::HttpRequest& request) {
    constexpr char kSpeculationRule[] = R"({
      <!doctype html>
      <script type="speculationrules">
      {
        "prerender":[
          {"source": "list", "urls": ["/empty.html"]}
        ]
      }
      </script>
    })";

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->AddCustomHeader("Supports-Loading-Mode", "fenced-frame");
    http_response->set_content_type("text/html");
    http_response->set_content(kSpeculationRule);
    return http_response;
  }

  static std::unique_ptr<net::test_server::HttpResponse>
  HandleFencedFrameWithSpeculationRulesHeaderRequest(
      const net::test_server::HttpRequest& request) {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->AddCustomHeader("Supports-Loading-Mode", "fenced-frame");
    http_response->AddCustomHeader("Speculation-Rules", "\"/prerender.json\"");
    http_response->set_content_type("text/html");
    http_response->set_content("<!doctype html>nothing");
    return http_response;
  }

  static std::unique_ptr<net::test_server::HttpResponse>
  HandlePrerenderJsonRequest(const net::test_server::HttpRequest& request) {
    constexpr char kSpeculationRule[] = R"(
      {
        "prerender":[
          {"source": "list", "urls": ["/empty.html"]}
        ]
      }
    )";

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("application/speculationrules+json");
    http_response->set_content(kSpeculationRule);
    return http_response;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that creating a fenced frame in a prerendered page is deferred until
// activation.
IN_PROC_BROWSER_TEST_F(PrerenderFencedFrameBrowserTest,
                       CreateFencedFrameInPrerenderedPage) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");
  const GURL kFencedFrameUrl = GetUrl("/title1.html");
  constexpr char kAddFencedFrameScript[] = R"({
    const fenced_frame = document.createElement('fencedframe');
    fenced_frame.config = new FencedFrameConfig($1);
    document.body.appendChild(fenced_frame);
  })";

  const int kNumNavigations = 3;
  TestNavigationObserver nav_observer(web_contents(), kNumNavigations);

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  auto* prerendered_rfh = GetPrerenderedMainFrameHost(host_id);
  EXPECT_EQ(kPrerenderingUrl, nav_observer.last_navigation_url());
  EXPECT_TRUE(ExecJs(prerendered_rfh,
                     JsReplace(kAddFencedFrameScript, kFencedFrameUrl)));
  // Since we've deferred creating the fenced frame delegate, we should see no
  // child frames.
  size_t child_frame_count = 0;
  prerendered_rfh->ForEachRenderFrameHost([&](RenderFrameHostImpl* rfh) {
    if (rfh != prerendered_rfh)
      child_frame_count++;
  });
  EXPECT_EQ(0lu, child_frame_count);

  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(kPrerenderingUrl, nav_observer.last_navigation_url());
  nav_observer.Wait();
  EXPECT_EQ(kFencedFrameUrl, nav_observer.last_navigation_url());
}

// Test that prerendering triggered by fenced frames with speculation rules is
// blocked.
IN_PROC_BROWSER_TEST_F(PrerenderFencedFrameBrowserTest,
                       PrerenderFromFencedFrame_SpeculationRules) {
  const GURL initial_url = GetUrl("/empty.html");
  const GURL fenced_frame_url = GetUrl("/fenced-frame-with-speculation-rules");
  constexpr char kAddFencedFrameScript[] = R"({
    const fenced_frame = document.createElement('fencedframe');
    fenced_frame.config = new FencedFrameConfig($1);
    document.body.appendChild(fenced_frame);
  })";

  // Prerendering triggered by fenced frames will be blocked. To detect it, we
  // need to wait its failure by monitoring a console error.
  const char* console_pattern =
      "The SpeculationRules API does not support prerendering in fenced "
      "frames.";
  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(console_pattern);

  // Start prerendering from fenced frames.
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  RenderFrameHostImpl* primary_rfh = web_contents_impl()->GetPrimaryMainFrame();
  EXPECT_TRUE(
      ExecJs(primary_rfh, JsReplace(kAddFencedFrameScript, fenced_frame_url)));

  ASSERT_TRUE(console_observer.Wait());

  histogram_tester().ExpectTotalCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule", 0);
}

// Test that prerendering triggered by fenced frames with speculation rules
// header is blocked.
IN_PROC_BROWSER_TEST_F(PrerenderFencedFrameBrowserTest,
                       PrerenderFromFencedFrame_LinkSpeculationRules) {
  const GURL initial_url = GetUrl("/empty.html");
  const GURL fenced_frame_url =
      GetUrl("/fenced-frame-with-speculation-rules-header");
  constexpr char kAddFencedFrameScript[] = R"({
    const fenced_frame = document.createElement('fencedframe');
    fenced_frame.config = new FencedFrameConfig($1);
    document.body.appendChild(fenced_frame);
  })";

  // Prerendering triggered by fenced frames will be blocked. To detect it, we
  // need to wait its failure by monitoring a console error.
  const char* console_pattern =
      "The SpeculationRules API does not support prerendering in fenced "
      "frames.";
  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(console_pattern);

  // Start prerendering from fenced frames.
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  RenderFrameHostImpl* primary_rfh = web_contents_impl()->GetPrimaryMainFrame();
  EXPECT_TRUE(
      ExecJs(primary_rfh, JsReplace(kAddFencedFrameScript, fenced_frame_url)));

  ASSERT_TRUE(console_observer.Wait());

  histogram_tester().ExpectTotalCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule", 0);
}

namespace {

class PrerenderWithSiteIsolationDisabledBrowserTest
    : public PrerenderBrowserTest {
 public:
  PrerenderWithSiteIsolationDisabledBrowserTest() = default;
  ~PrerenderWithSiteIsolationDisabledBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PrerenderBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableSiteIsolation);
  }
};

}  // namespace

// This test sets up a scenario where we swap SiteInstances during a prerender
// page's first navigation. Full site isolation is disabled for this test, but
// we dynamically isolate "b.test". The max process count is also set to 1.
//
// We initially start off with navigating the primary main frame to b.test,
// which will be assigned to a process P1.
//
// P1 ----- b.test
//
// We then add an a.test iframe, which will be assigned to a different process
// P2. This is because P1 currently hosts content from b.test, and b.test has
// been configured to require isolation from other sites.
//
// P1 ------ b.test
// P2 ------ a.test
//
// We then start prerendering b.test. This happens in two steps. In the first
// step we initialize the FrameTree and create an empty main frame that hasn't
// been navigated. This empty main frame has an empty SiteInstance (prerenders
// use an empty SiteInfo for this currently) which is assigned to P2 (in normal
// circumstances, it would be assigned to a new process but because we're above
// the process limit, it tries to reuse an existing process, and P2 is eligible
// as it currently only has the a.test iframe and a.test does not need to be
// isolated).
//
// P1 ------ b.test
// P2 ------ a.test, <empty prerender>
//
// In the second step, we navigate the prerender main frame to the prerender
// url, which is b.test. Now b.test is configured to be in an isolated process,
// so we can't reuse the current SiteInstance (as it is assigned to P1 which has
// content from a.test), and have to move it to a new process (and therefore
// have to swap the SiteInstance).
//
// P1 ------ b.test (primary), b.test (prerender)
// P2 ------ a.test
IN_PROC_BROWSER_TEST_F(PrerenderWithSiteIsolationDisabledBrowserTest,
                       ForceSiteInstanceSwapForInitialPrerenderNavigation) {
  if (AreAllSitesIsolatedForTesting()) {
    LOG(ERROR) << "Site Isolation should be disabled for this test.";
    return;
  }

  // Set max renderer process count to force process reuse and prevent
  // prerendering pages from getting dedicated processes by default.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  const GURL kInitialUrl =
      ssl_server().GetURL("isolated.b.test", "/empty.html");
  const GURL kIframeUrl = ssl_server().GetURL("a.test", "/empty.html");
  const GURL kPrerenderingUrl =
      ssl_server().GetURL("isolated.b.test", "/title1.html");

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddFutureIsolatedOrigins(
      {url::Origin::Create(kInitialUrl)},
      ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);

  // Navigate primary main frame to b.test. It will be assigned to a process
  // that is locked to b.test.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Add an a.test iframe, which will be loaded in a new process that isn't
  // locked.
  ASSERT_TRUE(AddTestUtilJS(current_frame_host()));
  ASSERT_TRUE(
      ExecJs(current_frame_host(), JsReplace("add_iframe($1)", kIframeUrl)));
  RenderFrameHostImplWrapper iframe(
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(current_frame_host(), 0)));
  ASSERT_NE(current_frame_host()->GetProcess(), iframe->GetProcess());

  // Prerender b.test. The initial empty document will be assigned to
  // the same process as the a.test iframe, but on navigation to b.test, it
  // can no longer use the same process, and the SiteInstance will have to be
  // changed in order to assign the document to a different process.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImplWrapper prerender_rfh(
      GetPrerenderedMainFrameHost(host_id));
  EXPECT_EQ(prerender_rfh->lifecycle_state(),
            LifecycleStateImpl::kPrerendering);
  EXPECT_EQ(prerender_rfh->GetProcess(), current_frame_host()->GetProcess());
}

class PrerenderClientHintsBrowserTest : public PrerenderBrowserTest {
 public:
  PrerenderClientHintsBrowserTest() = default;
  ~PrerenderClientHintsBrowserTest() override = default;

  void SetUp() override {
    ssl_server().RegisterRequestHandler(base::BindRepeating(&HandleRequest));
    PrerenderBrowserTest::SetUp();
  }

  static std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url.find("acceptch") == std::string::npos)
      return nullptr;

    // Serve a response indicating clients to provide full version of UA.
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    if (request.relative_url.find("full-version") != std::string::npos) {
      response->AddCustomHeader("Accept-CH", "sec-ch-ua-full-version");
    } else if (request.relative_url.find("bitness") != std::string::npos) {
      response->AddCustomHeader("Accept-CH", "sec-ch-ua-bitness");
    } else if (request.relative_url.find("viewport-width") !=
               std::string::npos) {
      response->AddCustomHeader("Accept-CH", "viewport-width");
      response->AddCustomHeader("Accept-CH", "sec-ch-viewport-width");
    } else if (request.relative_url.find("viewport-height") !=
               std::string::npos) {
      // Don't need to add "viewport-height" as it is not defined in the specs.
      response->AddCustomHeader("Accept-CH", "sec-ch-viewport-height");
    } else if (request.relative_url.find("no-value") != std::string::npos) {
      response->AddCustomHeader("Accept-CH", "");
    }
    response->set_code(net::HTTP_OK);
    if (request.relative_url.find("iframe") != std::string::npos) {
      response->set_content(R"(
        <html><head><title>iframe test</title></head>
        <body>
        <iframe src="title1.html" id="test"></iframe>
        </body></html>
      )");
      response->set_content_type("text/html");
    } else if (request.relative_url.find("image") != std::string::npos) {
      response->set_content(R"(
        <html>
        <head></head>
        <body>
          <img src="./blank.jpg"/>
          <p>This page has an image. Yay for images!
        </body>
        </html>
      )");
      response->set_content_type("text/html");

    } else {
      response->set_content("");
      response->set_content_type("text/plain");
    }
    return response;
  }

 protected:
  bool HasRequestHeader(const GURL& url, const std::string& key) {
    net::test_server::HttpRequest::HeaderMap headers = GetRequestHeaders(url);
    return headers.find(key) != headers.end();
  }
};

IN_PROC_BROWSER_TEST_F(PrerenderClientHintsBrowserTest,
                       PrerenderResponseChangesClientHintsLocally) {
  MockClientHintsControllerDelegate client_hints_controller_delegate(
      GetShellUserAgentMetadata());
  ShellContentBrowserClient::Get()
      ->browser_context()
      ->set_client_hints_controller_delegate(&client_hints_controller_delegate);

  // Navigate to an initial page.
  GURL url = GetUrl("/empty.html?acceptch-bitness");
  ASSERT_TRUE(NavigateToURL(shell(), url));

  // Start prerendering.
  GURL prerender_url = GetUrl("/iframe.html?acceptch-full-version");
  FrameTreeNodeId host_id = AddPrerender(prerender_url);

  // The main frame request does not contain sec-ch-ua-full-version, because it
  // is using the global setting at this moment. sec-ch-ua-bitness should be
  // contained as well, because it is a global setting and applies to all
  // navigations.
  EXPECT_TRUE(HasRequestHeader(prerender_url, "sec-ch-ua-bitness"));
  EXPECT_FALSE(HasRequestHeader(prerender_url, "sec-ch-ua-full-version"));

  // The subframe prerender navigation requests should contain
  // sec-ch-ua-full-version, as the main frame navigation request changed the
  // client hints setting.
  GURL prerender_iframe_url = GetUrl("/title1.html");
  WaitForRequest(prerender_iframe_url, 1);
  EXPECT_TRUE(HasRequestHeader(prerender_iframe_url, "sec-ch-ua-full-version"));
  EXPECT_TRUE(HasRequestHeader(prerender_iframe_url, "sec-ch-ua-bitness"));

  test::PrerenderHostObserver prerender_observer(*web_contents_impl(), host_id);
  NavigatePrimaryPage(prerender_url);

  // The prerendered page should be activated successfully. The settings on the
  // prerendered page should not apply to the primary navigation before
  // activation, so at this point the navigation request is using the global
  // setting, which is the same as the prerender initial navigation.
  prerender_observer.WaitForActivation();

  GURL real_navigate_url = GetUrl("/empty.html?real");
  NavigatePrimaryPage(real_navigate_url);

  // The request headers should contain sec-ch-ua-full-version, because the
  // prerender local setting was propagated to the global setting. The final
  // setting is the union set of global setting and local setting.
  EXPECT_TRUE(HasRequestHeader(real_navigate_url, "sec-ch-ua-full-version"));
  EXPECT_TRUE(HasRequestHeader(prerender_iframe_url, "sec-ch-ua-bitness"));
}

IN_PROC_BROWSER_TEST_F(PrerenderClientHintsBrowserTest,
                       ChangesToClientHintsAreDiscardIfNoActivation) {
  MockClientHintsControllerDelegate client_hints_controller_delegate(
      GetShellUserAgentMetadata());
  ShellContentBrowserClient::Get()
      ->browser_context()
      ->set_client_hints_controller_delegate(&client_hints_controller_delegate);

  // Navigate to an initial page.
  GURL url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), url));

  // Start prerendering.
  GURL prerender_url = GetUrl("/empty.html?acceptch");
  GURL real_navigate_url = GetUrl("/empty.html?real");

  FrameTreeNodeId host_id = AddPrerender(prerender_url);
  test::PrerenderHostObserver prerender_observer(*web_contents_impl(), host_id);
  NavigatePrimaryPage(real_navigate_url);

  // The request headers should not contain sec-ch-ua-full-version, because no
  // primary pages indicate to do so and the prerender local setting has been
  // discarded.
  EXPECT_FALSE(HasRequestHeader(real_navigate_url, "sec-ch-ua-full-version"));
  GURL real_navigate_url_2 = GetUrl("/empty.html?real2");
  NavigatePrimaryPage(real_navigate_url_2);

  // The request headers should not contain sec-ch-ua-full-version, because no
  // primary pages indicate to do so and the prerender local setting has been
  // discarded.
  EXPECT_FALSE(HasRequestHeader(real_navigate_url_2, "sec-ch-ua-full-version"));
}

IN_PROC_BROWSER_TEST_F(PrerenderClientHintsBrowserTest,
                       PrimaryResponsesDoNotResetPrenderSettings) {
  MockClientHintsControllerDelegate client_hints_controller_delegate(
      GetShellUserAgentMetadata());
  ShellContentBrowserClient::Get()
      ->browser_context()
      ->set_client_hints_controller_delegate(&client_hints_controller_delegate);

  // Navigate to an initial page.
  GURL url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), url));

  // Start prerendering.
  GURL prerender_url = GetUrl("/iframe.html?acceptch-full-version");
  FrameTreeNodeId host_id = AddPrerender(prerender_url);

  // The main frame request does not contain sec-ch-ua-full-version, because it
  // is using the global setting at this moment.
  EXPECT_FALSE(HasRequestHeader(prerender_url, "sec-ch-ua-full-version"));

  // The subframe prerender navigation requests should contain
  // sec-ch-ua-full-version, as the main frame navigation request changed the
  // client hints setting.
  GURL prerender_iframe_url = GetUrl("/title1.html");
  WaitForRequest(prerender_iframe_url, 1);
  EXPECT_TRUE(HasRequestHeader(prerender_iframe_url, "sec-ch-ua-full-version"));

  // Open a new tab, and the new page clears all settings.
  GURL new_tab_url = GetUrl("/image.html?acceptch-no-value");
  OpenURLParams params(
      new_tab_url, Referrer(), WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false);
  auto* new_web_contents =
      web_contents_impl()->OpenURL(params, /*navigation_handle_callback=*/{});
  ASSERT_NE(nullptr, new_web_contents);
  GURL new_tab_image_url = GetUrl("/blank.jpg");
  WaitForRequest(new_tab_image_url, 1);
  EXPECT_FALSE(HasRequestHeader(new_tab_url, "sec-ch-ua-full-version"));

  test::PrerenderHostObserver prerender_observer(*web_contents_impl(), host_id);
  NavigatePrimaryPage(prerender_url);

  // The prerendered page should be activated successfully.
  prerender_observer.WaitForActivation();

  GURL real_navigate_url = GetUrl("/empty.html?real");
  NavigatePrimaryPage(real_navigate_url);

  // The request headers should contain sec-ch-ua-full-version, because the
  // prerender local setting was propagated to the global setting.
  EXPECT_TRUE(HasRequestHeader(real_navigate_url, "sec-ch-ua-full-version"));
}

// Test that changes on the viewport width of the initiator page between when to
// trigger prerendering and when to activate don't fail activation params match.
IN_PROC_BROWSER_TEST_F(PrerenderClientHintsBrowserTest, ViewPort_Width) {
  MockClientHintsControllerDelegate client_hints_controller_delegate(
      GetShellUserAgentMetadata());
  ShellContentBrowserClient::Get()
      ->browser_context()
      ->set_client_hints_controller_delegate(&client_hints_controller_delegate);

  // Set the initial window size.
  web_contents_impl()->Resize(gfx::Rect(10, 20));

  // Navigate to an initial page.
  GURL url = GetUrl("/empty.html?acceptch-viewport-width");
  ASSERT_TRUE(NavigateToURL(shell(), url));

  // Start prerendering. This won't have the "(sec-ch-)viewport-width" headers
  // as the width is 0 due to the lack of a cached/known viewport size.
  GURL prerender_url = GetUrl("/iframe.html?acceptch");
  FrameTreeNodeId host_id = AddPrerender(prerender_url);
  EXPECT_FALSE(HasRequestHeader(prerender_url, "viewport-width"));
  EXPECT_FALSE(HasRequestHeader(prerender_url, "sec-ch-viewport-width"));

  // Resize the window.
  web_contents_impl()->Resize(gfx::Rect(30, 40));

  // Activation should also not have the "(sec-ch-)viewport-width" headers.
  test::PrerenderHostObserver prerender_observer(*web_contents_impl(), host_id);
  NavigatePrimaryPage(prerender_url);
  prerender_observer.WaitForActivation();
  EXPECT_FALSE(HasRequestHeader(prerender_url, "viewport-width"));
  EXPECT_FALSE(HasRequestHeader(prerender_url, "sec-ch-viewport-width"));
}

// Test that changes on the viewport height of the initiator page between when
// to trigger prerendering and when to activate don't fail activation params
// match.
IN_PROC_BROWSER_TEST_F(PrerenderClientHintsBrowserTest, ViewPort_Height) {
  MockClientHintsControllerDelegate client_hints_controller_delegate(
      GetShellUserAgentMetadata());
  ShellContentBrowserClient::Get()
      ->browser_context()
      ->set_client_hints_controller_delegate(&client_hints_controller_delegate);

  // Set the initial window size.
  web_contents_impl()->Resize(gfx::Rect(10, 20));

  // Navigate to an initial page.
  GURL url = GetUrl("/empty.html?acceptch-viewport-height");
  ASSERT_TRUE(NavigateToURL(shell(), url));

  // Start prerendering. This won't have the "sec-ch-viewport-height" header
  // as the height is 0 due to the lack of a cached/known viewport size.
  GURL prerender_url = GetUrl("/iframe.html?acceptch");
  FrameTreeNodeId host_id = AddPrerender(prerender_url);
  EXPECT_FALSE(HasRequestHeader(prerender_url, "sec-ch-viewport-height"));

  // Resize the window.
  web_contents_impl()->Resize(gfx::Rect(30, 40));

  // Activation should also not have the "sec-ch-viewport-height" header.
  test::PrerenderHostObserver prerender_observer(*web_contents_impl(), host_id);
  NavigatePrimaryPage(prerender_url);
  prerender_observer.WaitForActivation();
  EXPECT_FALSE(HasRequestHeader(prerender_url, "sec-ch-viewport-height"));
}

void CheckExpectedCrossOriginMetrics(
    const base::HistogramTester& histogram_tester,
    PrerenderCrossOriginRedirectionMismatch mismatch_type,
    std::optional<PrerenderCrossOriginRedirectionProtocolChange>
        protocol_change) {
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kCrossSiteRedirectInInitialNavigation, 1);
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCrossOriginRedirectionMismatch.Embedder_"
      "EmbedderSuffixForTest",
      mismatch_type, 1);
  if (protocol_change.has_value()) {
    histogram_tester.ExpectUniqueSample(
        "Prerender.Experimental.CrossOriginRedirectionProtocolChange.Embedder_"
        "EmbedderSuffixForTest",
        protocol_change.value(), 1);
  }
}

// Tests PrerenderCrossOriginRedirectionMismatch.kSchemeHostPortMismatch was
// recorded when a prerendering navigaton was redireted to another origin with
// different scheme, host and port.
IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    EmbedderTrigger_CrossOriginRedirection_SchemeHostPortMismatch) {
  base::HistogramTester histogram_tester;
  embedded_test_server()->AddDefaultHandlers(GetTestDataFilePath());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL initial_url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // The redirected_url's origin completely differs from the prerendering one.
  GURL redirected_url = embedded_test_server()->GetURL("b.test", "/empty.html");
  GURL prerendering_url = GetUrl("/server-redirect?" + redirected_url.spec());
  ASSERT_NE(prerendering_url.scheme(), redirected_url.scheme());
  ASSERT_NE(prerendering_url.host(), redirected_url.host());
  ASSERT_NE(prerendering_url.port(), redirected_url.port());

  PrerenderEmbedderTriggeredCrossOriginRedirectionPage(
      *web_contents_impl(), prerendering_url, redirected_url);
  CheckExpectedCrossOriginMetrics(
      histogram_tester,
      PrerenderCrossOriginRedirectionMismatch::kSchemeHostPortMismatch,
      /*protocol_change=*/std::nullopt);
}

// Tests a prerendering navigaton goes with HTTP protocol, and being redirected
// to upgrade its protocol to HTTPS.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       EmbedderTrigger_CrossOriginRedirection_ProtocolUpgrade) {
  base::HistogramTester histogram_tester;
  embedded_test_server()->AddDefaultHandlers(GetTestDataFilePath());
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL initial_url = embedded_test_server()->GetURL("a.test", "/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Redirect to another url with protocol upgraded.
  GURL redirected_url = ssl_server().GetURL("a.test", "/empty.html");
  GURL prerendering_url = embedded_test_server()->GetURL(
      "a.test", "/server-redirect?" + redirected_url.spec());
  ASSERT_NE(prerendering_url.scheme(), redirected_url.scheme());
  ASSERT_NE(prerendering_url.port(), redirected_url.port());
  ASSERT_EQ(prerendering_url.scheme(), url::kHttpScheme);
  ASSERT_EQ(redirected_url.scheme(), url::kHttpsScheme);

  PrerenderEmbedderTriggeredCrossOriginRedirectionPage(
      *web_contents_impl(), prerendering_url, redirected_url);
  CheckExpectedCrossOriginMetrics(
      histogram_tester,
      PrerenderCrossOriginRedirectionMismatch::kSchemePortMismatch,
      PrerenderCrossOriginRedirectionProtocolChange::kHttpProtocolUpgrade);
}

// Similar to
// CancelEmbedderTriggeredPrerenderingCrossOriginRedirection_ProtocolUpgrade,
// tests a prerendering navigaton goes with HTTPS protocol, and being redirected
// to upgrade its protocol to HTTPS.
IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    EmbedderTrigger_CrossOriginRedirection_ProtocolDowngrade) {
  base::HistogramTester histogram_tester;
  GURL initial_url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  GURL::Replacements downgrade_protocol;
  downgrade_protocol.SetSchemeStr(url::kHttpScheme);
  std::string port_str(base::NumberToString(ssl_server().port() + 1));
  downgrade_protocol.SetPortStr(port_str);
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Redirect to another url with protocol upgraded.
  GURL redirected_url =
      GetUrl("/empty.html").ReplaceComponents(downgrade_protocol);
  GURL prerendering_url = GetUrl("/server-redirect?" + redirected_url.spec());
  ASSERT_NE(prerendering_url.scheme(), redirected_url.scheme());
  ASSERT_NE(prerendering_url.port(), redirected_url.port());
  ASSERT_EQ(prerendering_url.scheme(), url::kHttpsScheme);
  ASSERT_EQ(redirected_url.scheme(), "http");

  PrerenderEmbedderTriggeredCrossOriginRedirectionPage(
      *web_contents_impl(), prerendering_url, redirected_url);
  CheckExpectedCrossOriginMetrics(
      histogram_tester,
      PrerenderCrossOriginRedirectionMismatch::kSchemePortMismatch,
      PrerenderCrossOriginRedirectionProtocolChange::kHttpProtocolDowngrade);
}

// Tests that embedder triggered prerender can be redirected to the subdomain
// because they are same-site.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       EmbedderTrigger_CrossOriginRedirection_ToSubdomain) {
  GURL initial_url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  GURL::Replacements set_host;
  set_host.SetHostStr("www.a.test");

  GURL redirected_url = GetUrl("/empty.html").ReplaceComponents(set_host);
  GURL prerendering_url = GetUrl("/server-redirect?" + redirected_url.spec());

  std::unique_ptr<PrerenderHandle> prerender_handle =
      PrerenderEmbedderTriggeredCrossOriginRedirectionPage(
          *web_contents_impl(), prerendering_url, redirected_url);
  test::PrerenderHostObserver prerender_observer(*web_contents_impl(),
                                                 prerendering_url);
  shell()->web_contents()->OpenURL(
      OpenURLParams(
          prerendering_url, Referrer(), WindowOpenDisposition::CURRENT_TAB,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});
  prerender_observer.WaitForActivation();
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kActivated, 1);
}

// Tests that embedder triggered prerender can be redirected to the same site.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       EmbedderTrigger_CrossOriginRedirection_FromSubdomain) {
  GURL initial_url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  GURL::Replacements set_host;
  set_host.SetHostStr("www.a.test");

  GURL redirected_url = GetUrl("/empty.html");
  GURL prerendering_url = GetUrl("/server-redirect?" + redirected_url.spec())
                              .ReplaceComponents(set_host);
  std::unique_ptr<PrerenderHandle> prerender_handle =
      PrerenderEmbedderTriggeredCrossOriginRedirectionPage(
          *web_contents_impl(), prerendering_url, redirected_url);
  test::PrerenderHostObserver prerender_observer(*web_contents_impl(),
                                                 prerendering_url);
  shell()->web_contents()->OpenURL(
      OpenURLParams(
          prerendering_url, Referrer(), WindowOpenDisposition::CURRENT_TAB,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});
  prerender_observer.WaitForActivation();
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kActivated, 1);
}

// Tests PrerenderCrossOriginRedirectionMismatch.kHostMismatch is recorded
// when the prerendering navigation is redirected to a different domain.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       EmbedderTrigger_CrossOriginRedirection_DifferentDomain) {
  base::HistogramTester histogram_tester;
  GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  GURL kRedirectedUrl = GetCrossSiteUrl("/empty.html?prerender");
  GURL kPrerenderingUrl = GetUrl("/server-redirect?" + kRedirectedUrl.spec());
  PrerenderEmbedderTriggeredCrossOriginRedirectionPage(
      *web_contents_impl(), kPrerenderingUrl, kRedirectedUrl);
  CheckExpectedCrossOriginMetrics(
      histogram_tester, PrerenderCrossOriginRedirectionMismatch::kHostMismatch,
      /*protocol_change=*/std::nullopt);
}

// Tests that prerender works with accessibility.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderWithAccessibilityEnabled) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_iframe.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Enable accessibility.
  ScopedAccessibilityModeOverride inner_scoped_accessibility_mode(
      shell()->web_contents(), ui::kAXModeComplete);

  // Start prerendering `kPrerenderingUrl`, which has an iframe attached.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  ASSERT_TRUE(host_id);
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  test::PrerenderHostObserver prerender_observer(*web_contents_impl(),
                                                 kPrerenderingUrl);

  NavigatePrimaryPage(kPrerenderingUrl);

  prerender_observer.WaitForActivation();
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);
}

class UpdateTargetURLDelegate : public WebContentsDelegate {
 public:
  explicit UpdateTargetURLDelegate(WebContents* web_contents) {
    web_contents->SetDelegate(this);
  }

  UpdateTargetURLDelegate(const UpdateTargetURLDelegate&) = delete;
  UpdateTargetURLDelegate& operator=(const UpdateTargetURLDelegate&) = delete;

  bool is_updated_target_url() { return is_updated_target_url_; }

 private:
  void UpdateTargetURL(WebContents* source, const GURL& url) override {
    is_updated_target_url_ = true;
  }

  bool is_updated_target_url_ = false;
};

// Tests that text autosizer works per page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, FocusChangeInPrerenderedPage) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/simple_links.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerender_frame_host =
      GetPrerenderedMainFrameHost(host_id);

  UpdateTargetURLDelegate delegate(shell()->web_contents());

  // No crash.
  EXPECT_TRUE(ExecJs(prerender_frame_host,
                     "document.getElementById('same_site_link').focus();"));

  // The prerendered page should not update the target url.
  EXPECT_FALSE(delegate.is_updated_target_url());
}

// Tests that an unused RenderWidgetHost (that is owned by a RenderViewHostImpl)
// created by a prerendering FrameTree points to the primary frame tree after
// activation. Regression test for crbug.com/1324149.
IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    UnusedRenderWidgetHostFrameTreePointerUpdatedOnActivation) {
  // Since the render view host won't be created until the response is received
  // if feature DeferSpeculativeRFHCreation is on. The test is no longer valid
  // for this case.
  if (base::FeatureList::IsEnabled(features::kDeferSpeculativeRFHCreation)) {
    return;
  }

  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  EXPECT_TRUE(AddTestUtilJS(current_frame_host()));

  // Start a prerender.
  const GURL kPrerenderingUrl = GetUrl("/title2.html");
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);

  // Add a cross-origin iframe to the prerendering page.
  const GURL kCrossOriginSubframeUrl = GetCrossSiteUrl("/title2.html");
  RenderFrameHostImpl* prerender_rfh = GetPrerenderedMainFrameHost(host_id);
  EXPECT_TRUE(AddTestUtilJS(prerender_rfh));
  EXPECT_TRUE(ExecJs(prerender_rfh, JsReplace("add_iframe_async($1)",
                                              kCrossOriginSubframeUrl)));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(prerender_rfh->child_count(), 1u);
  FrameTreeNode* iframe = prerender_rfh->child_at(0);
  // The cross-origin navigation in the iframe will be throttled, but not before
  // creating a out-of-process speculative RFH (which would also result in an
  // RVH created for the subframe speculatively).
  ASSERT_TRUE(iframe->render_manager()->speculative_frame_host());
  RenderViewHostImpl* render_view_host =
      iframe->render_manager()->speculative_frame_host()->render_view_host();

  // Activate.
  NavigatePrimaryPage(kPrerenderingUrl);
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
  // Wait for iframe to finish navigating.
  ASSERT_EQ("LOADED",
            EvalJs(prerender_rfh, JsReplace("wait_iframe_async($1)",
                                            kCrossOriginSubframeUrl)));
  // This asserts that the current RenderViewHost was created before activation
  // (to make sure we're testing the right thing).
  EXPECT_EQ(render_view_host, iframe->current_frame_host()->render_view_host());

  // The unused RenderWidgetHost should point to the primary FrameTree now.
  RenderWidgetHostImpl* render_widget_host = render_view_host->GetWidget();
  EXPECT_NE(render_widget_host,
            iframe->current_frame_host()->GetRenderWidgetHost());
  EXPECT_EQ(render_widget_host->frame_tree(),
            current_frame_host()->frame_tree());

  // Navigate the primary main frame to the same origin as |iframe|; this
  // should reuse |render_view_host|, and as a result |render_widget_host| will
  // be used. If the |render_widget_host| points to the wrong frame_tree, this
  // will result in a segfault (reproducing crbug.com/1324149) when we try to
  // focus the new page's view.
  const GURL kCrossOriginUrl = GetCrossSiteUrl("/title1.html");
  DisableProactiveBrowsingInstanceSwapFor(current_frame_host());
  NavigatePrimaryPage(kCrossOriginUrl);
  ASSERT_EQ(current_frame_host()->render_view_host(), render_view_host);
  ASSERT_EQ(current_frame_host()->GetRenderWidgetHost(), render_widget_host);
}

// Tests that window.close() can cancel speculation rules.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, WindowClosedSpeculationRules) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerenderingUrl = embedded_test_server()->GetURL("/title1.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  AddPrerender(kPrerenderingUrl);

  FrameTreeNodeId host_id = GetHostForUrl(kPrerenderingUrl);
  WaitForPrerenderLoadCompletion(host_id);

  test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  RenderFrameHostImpl* prerender_rfh = GetPrerenderedMainFrameHost(host_id);
  ASSERT_TRUE(ExecJs(prerender_rfh, "window.close()"));
  host_observer.WaitForDestroyed();
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kWindowClosed);

  // The initiator page should not be closed by window.closed().
  EXPECT_TRUE(ExecJs(web_contents(), ""));
}

// Tests that window.close() can cancel speculation rules whose target_hint is
// "_blank" (i.e., prerender into new tab).
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       WindowClosedSpeculationRules_WithTargetHintBlank) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerenderingUrl = embedded_test_server()->GetURL("/title1.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  FrameTreeNodeId host_id = prerender_helper()->AddPrerender(
      kPrerenderingUrl, /*eagerness=*/std::nullopt, "_blank");
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
  ASSERT_NE(prerender_web_contents, web_contents_impl());
  ExpectWebContentsIsForNewTabPrerendering(*prerender_web_contents);

  test::PrerenderHostObserver host_observer(*prerender_web_contents, host_id);
  RenderFrameHost* prerender_rfh =
      test::PrerenderTestHelper::GetPrerenderedMainFrameHost(
          *prerender_web_contents, host_id);
  ASSERT_TRUE(ExecJs(prerender_rfh, "window.close()"));
  host_observer.WaitForDestroyed();
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kWindowClosed);

  // The initiator page should not be closed by window.closed().
  EXPECT_TRUE(ExecJs(web_contents(), ""));
}

// Tests that Prerender is suppressed by slow network.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, SlowNetwork) {
  // Emulate slow network.
  MockClientHintsControllerDelegate client_hints_controller_delegate(
      GetShellUserAgentMetadata());
  ShellContentBrowserClient::Get()
      ->browser_context()
      ->set_client_hints_controller_delegate(&client_hints_controller_delegate);
  network::NetworkQualityTracker& network_quality_tracker =
      *client_hints_controller_delegate.GetNetworkQualityTracker();
  base::TimeDelta http_rtt =
      base::Milliseconds(1) +
      ::features::kSuppressesPrerenderingOnSlowNetworkThreshold.Get();
  network_quality_tracker.ReportRTTsAndThroughputForTesting(
      http_rtt, network_quality_tracker.GetDownstreamThroughputKbps());

  ASSERT_TRUE(embedded_test_server()->Start());

  GURL initial_url = GetUrl("/empty.html");
  GURL prerendering_url = GetUrl("/empty.html?prerender");

  // Attempt to prerender.
  test::PrerenderHostRegistryObserver observer(*web_contents_impl());
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  AddPrerenderAsync(prerendering_url);
  observer.WaitForTrigger(prerendering_url);

  // It should fail.
  EXPECT_FALSE(HasHostForUrl(prerendering_url));
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kSlowNetwork);

  // Navigate primary page to flush the metrics.
  NavigatePrimaryPage(prerendering_url);
  // Cross-check that the eligibility reason points to kSlowNetwork on slow
  // network.
  ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder().BuildEntry(
      PrimaryPageSourceId(), PreloadingType::kPrerender,
      PreloadingEligibility::kSlowNetwork,
      PreloadingHoldbackStatus::kUnspecified,
      PreloadingTriggeringOutcome::kUnspecified,
      PreloadingFailureReason::kUnspecified,
      /*accurate=*/true,
      /*ready_time=*/std::nullopt,
      blink::mojom::SpeculationEagerness::kEager)});
}

class V8OptimizerContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  explicit V8OptimizerContentBrowserClient(bool disable) : disable_(disable) {}
  ~V8OptimizerContentBrowserClient() override = default;

  bool AreV8OptimizationsDisabledForSite(BrowserContext* browser_context,
                                         const GURL& site_url) override {
    return disable_;
  }

 public:
  const bool disable_ = false;
};

// Previously, prerendering a page that had the COOP crashed when the V8
// optimizer was disabled by the site settings. To prevent it, in the current
// implementation, prerendering is tentatively disabled when the V8 optimizer is
// disabled. This is the regression test for the issue. See
// https://crbug.com/40076091 for details.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderCOOPWithoutV8Optimizer) {
  // Disable the V8 optimizer.
  V8OptimizerContentBrowserClient test_browser_client(/*disable=*/true);

  // Attempt to prerender the page that has the COOP.
  const GURL initial_url = GetUrl("/empty.html");
  const GURL prerendering_url =
      GetUrl("/set-header?Cross-Origin-Opener-Policy: same-origin");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), initial_url);

  // Start prerendering. This should fail as the optimizer is disabled and not
  // crash.
  test::PrerenderHostRegistryObserver observer(*web_contents());
  AddPrerenderAsync(prerendering_url);
  observer.WaitForTrigger(prerendering_url);
  EXPECT_FALSE(HasHostForUrl(prerendering_url));
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kV8OptimizerDisabled);

  // Navigate the primary page to flush the Ukm.
  NavigatePrimaryPage(prerendering_url);
  ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder().BuildEntry(
      PrimaryPageSourceId(), PreloadingType::kPrerender,
      PreloadingEligibility::kV8OptimizerDisabled,
      PreloadingHoldbackStatus::kUnspecified,
      PreloadingTriggeringOutcome::kUnspecified,
      PreloadingFailureReason::kUnspecified,
      /*accurate=*/true,
      /*ready_time=*/std::nullopt,
      blink::mojom::SpeculationEagerness::kEager)});
}

// See the comment on PrerenderCOOPWithoutV8Optimizer test for details. This
// test ensures that prerendering is disabled regardless of whether the target
// page has the COOP, when the V8 optimizer is disabled.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderNonCOOPWithoutV8Optimizer) {
  // Disable the V8 optimizer.
  V8OptimizerContentBrowserClient test_browser_client(/*disable=*/true);

  const GURL initial_url = GetUrl("/empty.html");
  const GURL prerendering_url = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), initial_url);

  // Start prerendering. This should fail as the optimizer is disabled and not
  // crash.
  test::PrerenderHostRegistryObserver observer(*web_contents());
  AddPrerenderAsync(prerendering_url);
  observer.WaitForTrigger(prerendering_url);
  EXPECT_FALSE(HasHostForUrl(prerendering_url));
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kV8OptimizerDisabled);

  // Navigate the primary page to flush the Ukm.
  NavigatePrimaryPage(prerendering_url);
  ExpectPreloadingAttemptUkm({attempt_ukm_entry_builder().BuildEntry(
      PrimaryPageSourceId(), PreloadingType::kPrerender,
      PreloadingEligibility::kV8OptimizerDisabled,
      PreloadingHoldbackStatus::kUnspecified,
      PreloadingTriggeringOutcome::kUnspecified,
      PreloadingFailureReason::kUnspecified,
      /*accurate=*/true,
      /*ready_time=*/std::nullopt,
      blink::mojom::SpeculationEagerness::kEager)});
}

// See the comment on PrerenderCOOPWithoutV8Optimizer test for details. This
// test ensures that prerendering a page that has the COOP succeeds when the V8
// optimizer is enabled.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderCOOPWithV8Optimizer) {
  // Enable the V8 optimizer.
  V8OptimizerContentBrowserClient test_browser_client(/*disable=*/false);

  const GURL initial_url = GetUrl("/empty.html");
  const GURL prerendering_url =
      GetUrl("/set-header?Cross-Origin-Opener-Policy: same-origin");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), initial_url);

  // Start prerendering a page that has the COOP.
  FrameTreeNodeId host_id = AddPrerender(prerendering_url);
  ASSERT_TRUE(host_id);

  // Activate the prerendered page.
  NavigatePrimaryPage(prerendering_url);
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);
}

// Many of these tests navigate away from a page and then test whether the back
// navigation entry can be prerendered. This is parameterized on whether the
// navigation away from the original page is browser or renderer initiated.
class PrerenderSessionHistoryBrowserTest
    : public PrerenderBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "FromBrowser" : "FromRenderer";
  }

  // Navigate `web_contents` to `url`. The test parameterization determines
  // whether to do a browser initiated navigation or a renderer initiated
  // navigation.
  void NavigateAway(WebContentsImpl* web_contents, const GURL& url) {
    const bool from_browser = GetParam();
    if (from_browser) {
      ASSERT_TRUE(NavigateToURL(web_contents, url));
    } else {
      ASSERT_TRUE(NavigateToURLFromRenderer(web_contents, url));
    }
  }

  // Tests in this fixture generally begin by setting up a back navigation
  // entry, with `url1` being the back navigation entry and `url2` being the
  // last committed entry.
  void PerformInitialNavigations(WebContentsImpl* web_contents,
                                 const GURL& url1,
                                 const GURL& url2) {
    ASSERT_TRUE(NavigateToURL(web_contents, url1));
    NavigateAway(web_contents, url2);
  }

  void PredictBackNavigation(WebContentsImpl* web_contents) {
    PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();
    // For testing convenience, pretend that the mouse back button is the
    // predictor.
    const auto predictor = content_preloading_predictor::kMouseBackButton;

    registry->BackNavigationLikely(predictor);

    WaitForHttpCacheQueryCompletion(web_contents);
  }

  void PerformBackNavigation(WebContentsImpl* web_contents) {
    NavigationControllerImpl& controller = web_contents->GetController();
    ASSERT_TRUE(controller.CanGoBack());
    TestNavigationObserver back_observer(web_contents);
    controller.GoBack();
    back_observer.Wait();
  }

  void WaitForHttpCacheQueryCompletion(WebContentsImpl* web_contents) {
    PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();
    EXPECT_TRUE(base::test::RunUntil(
        [&]() { return !registry->HasOngoingHttpCacheQueryForTesting(); }));
  }

  void ClearBackForwardCache(WebContentsImpl* web_contents) {
    web_contents->GetController().GetBackForwardCache().Flush();
  }

  void ClearAllCaches(WebContentsImpl* web_contents) {
    BrowsingDataRemover* cache_remover =
        web_contents->GetBrowserContext()->GetBrowsingDataRemover();
    BrowsingDataRemoverCompletionObserver cache_clear_completion_observer(
        cache_remover);
    cache_remover->RemoveAndReply(
        base::Time::Min(), base::Time::Max(),
        BrowsingDataRemover::DATA_TYPE_CACHE,
        BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        &cache_clear_completion_observer);
    cache_clear_completion_observer.BlockUntilCompletion();
  }

  void ExpectAttemptUkm(ukm::TestUkmRecorder& ukm_recorder,
                        bool accurate,
                        PreloadingEligibility eligibility,
                        ukm::SourceId source_id) {
    std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> attempts =
        ukm_recorder.GetEntries(ukm::builders::Preloading_Attempt::kEntryName,
                                test::kPreloadingAttemptUkmMetrics);
    ASSERT_EQ(attempts.size(), 1u);

    const auto predictor = content_preloading_predictor::kMouseBackButton;
    const PreloadingHoldbackStatus holdback_status =
        eligibility == PreloadingEligibility::kEligible
            ? PreloadingHoldbackStatus::kAllowed
            : PreloadingHoldbackStatus::kUnspecified;
    const PreloadingTriggeringOutcome triggering_outcome =
        eligibility == PreloadingEligibility::kEligible
            ? PreloadingTriggeringOutcome::kNoOp
            : PreloadingTriggeringOutcome::kUnspecified;

    test::PreloadingAttemptUkmEntryBuilder entry_builder(predictor);
    ukm::TestUkmRecorder::HumanReadableUkmEntry expected_entry =
        entry_builder.BuildEntry(
            source_id, PreloadingType::kPrerender, eligibility, holdback_status,
            triggering_outcome, PreloadingFailureReason::kUnspecified,
            accurate);

    EXPECT_EQ(attempts[0], expected_entry)
        << test::ActualVsExpectedUkmEntryToString(attempts[0], expected_entry);
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         PrerenderSessionHistoryBrowserTest,
                         testing::Bool(),
                         PrerenderSessionHistoryBrowserTest::DescribeParams);

// Other tests in `PrerenderSessionHistoryBrowserTest` explicitly trigger the
// prediction and the navigation. For this test, we actually simulate the back
// button press events.
IN_PROC_BROWSER_TEST_P(PrerenderSessionHistoryBrowserTest,
                       BackButtonNavigation) {
  const GURL url1 = GetUrl("/title1.html");
  const GURL url2 = GetCrossSiteUrl("/title2.html");
  PerformInitialNavigations(web_contents_impl(), url1, url2);

  ClearBackForwardCache(web_contents_impl());

  base::HistogramTester histogram_tester;

  NavigationControllerImpl& controller = web_contents_impl()->GetController();
  ASSERT_TRUE(controller.CanGoBack());
  TestNavigationObserver back_observer(web_contents_impl());
  InputEventAckWaiter mouse_down_waiter(
      web_contents_impl()->GetPrimaryMainFrame()->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kMouseDown);
  const gfx::Point click_location(50, 50);
  SimulateMouseEvent(web_contents_impl(),
                     blink::WebInputEvent::Type::kMouseDown,
                     blink::WebMouseEvent::Button::kBack, click_location);
  // The mouse up triggers the navigation. We wait until after the cache query
  // to send the mouse up to ensure the navigation happens after the browser
  // decides whether to prerender.
  mouse_down_waiter.Wait();
  WaitForHttpCacheQueryCompletion(web_contents_impl());
  SimulateMouseEvent(web_contents_impl(), blink::WebInputEvent::Type::kMouseUp,
                     blink::WebMouseEvent::Button::kBack, click_location);
  back_observer.Wait();

  histogram_tester.ExpectUniqueSample(
      "Preloading.PrerenderBackNavigationEligibility.MouseBackButton",
      PrerenderBackNavigationEligibility::kEligible, 1);
}

IN_PROC_BROWSER_TEST_P(PrerenderSessionHistoryBrowserTest,
                       PredictionForEligibleBackNavigation) {
  const GURL url1 = GetUrl("/title1.html");
  const GURL url2 = GetCrossSiteUrl("/title2.html");
  PerformInitialNavigations(web_contents_impl(), url1, url2);

  ClearBackForwardCache(web_contents_impl());

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  PredictBackNavigation(web_contents_impl());
  PerformBackNavigation(web_contents_impl());

  ukm::SourceId source_id =
      web_contents_impl()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  histogram_tester.ExpectUniqueSample(
      "Preloading.PrerenderBackNavigationEligibility.MouseBackButton",
      PrerenderBackNavigationEligibility::kEligible, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.MouseBackButton.Precision",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.MouseBackButton.Recall",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Prerender.Attempt.MouseBackButton.Precision",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Prerender.Attempt.MouseBackButton.Recall",
      PredictorConfusionMatrix::kTruePositive, 1);
  ExpectAttemptUkm(ukm_recorder, true, PreloadingEligibility::kEligible,
                   source_id);
}

IN_PROC_BROWSER_TEST_P(PrerenderSessionHistoryBrowserTest,
                       NoPredictionDueToBfcache) {
  if (!BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    GTEST_SKIP()
        << "This test assumes the back navigation is restoring from bfcache.";
  }

  const GURL url1 = GetUrl("/title1.html");
  const GURL url2 = GetCrossSiteUrl("/title2.html");
  PerformInitialNavigations(web_contents_impl(), url1, url2);

  base::HistogramTester histogram_tester;

  PredictBackNavigation(web_contents_impl());
  PerformBackNavigation(web_contents_impl());

  histogram_tester.ExpectUniqueSample(
      "Preloading.PrerenderBackNavigationEligibility.MouseBackButton",
      PrerenderBackNavigationEligibility::kBfcacheEntryExists, 1);
  histogram_tester.ExpectTotalCount(
      "Preloading.Predictor.MouseBackButton.Precision", 0);
  histogram_tester.ExpectTotalCount(
      "Preloading.Predictor.MouseBackButton.Recall", 0);
  histogram_tester.ExpectTotalCount(
      "Preloading.Prerender.Attempt.MouseBackButton.Precision", 0);
  histogram_tester.ExpectTotalCount(
      "Preloading.Prerender.Attempt.MouseBackButton.Recall", 0);
}

IN_PROC_BROWSER_TEST_P(PrerenderSessionHistoryBrowserTest,
                       RendererNavigationAfterBackPrediction) {
  const GURL url1 = GetUrl("/title1.html");
  const GURL url2 = GetCrossSiteUrl("/title2.html");
  const GURL url3 = GetCrossSiteUrl("/title3.html");
  PerformInitialNavigations(web_contents_impl(), url1, url2);

  ClearBackForwardCache(web_contents_impl());

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  PredictBackNavigation(web_contents_impl());
  TestNavigationObserver nav_observer(web_contents_impl());
  ASSERT_TRUE(ExecJs(web_contents_impl(), JsReplace("location = $1;", url3)));
  nav_observer.Wait();

  ukm::SourceId source_id =
      web_contents_impl()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  histogram_tester.ExpectUniqueSample(
      "Preloading.PrerenderBackNavigationEligibility.MouseBackButton",
      PrerenderBackNavigationEligibility::kEligible, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.MouseBackButton.Precision",
      PredictorConfusionMatrix::kFalsePositive, 1);
  // A renderer navigation is not a false negative for this predictor.
  histogram_tester.ExpectTotalCount(
      "Preloading.Predictor.MouseBackButton.Recall", 0);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Prerender.Attempt.MouseBackButton.Precision",
      PredictorConfusionMatrix::kFalsePositive, 1);
  histogram_tester.ExpectTotalCount(
      "Preloading.Prerender.Attempt.MouseBackButton.Recall", 0);
  ExpectAttemptUkm(ukm_recorder, false, PreloadingEligibility::kEligible,
                   source_id);
}

IN_PROC_BROWSER_TEST_P(PrerenderSessionHistoryBrowserTest,
                       NotEligibleForSameDocument) {
  const GURL url1 = GetUrl("/title1.html");
  const GURL url2 = GetUrl("/title1.html#same");
  PerformInitialNavigations(web_contents_impl(), url1, url2);

  base::HistogramTester histogram_tester;

  PredictBackNavigation(web_contents_impl());
  PerformBackNavigation(web_contents_impl());

  histogram_tester.ExpectUniqueSample(
      "Preloading.PrerenderBackNavigationEligibility.MouseBackButton",
      PrerenderBackNavigationEligibility::kTargetIsSameDocument, 1);
  histogram_tester.ExpectTotalCount(
      "Preloading.Predictor.MouseBackButton.Precision", 0);
  histogram_tester.ExpectTotalCount(
      "Preloading.Predictor.MouseBackButton.Recall", 0);
  histogram_tester.ExpectTotalCount(
      "Preloading.Prerender.Attempt.MouseBackButton.Precision", 0);
  histogram_tester.ExpectTotalCount(
      "Preloading.Prerender.Attempt.MouseBackButton.Recall", 0);
}

IN_PROC_BROWSER_TEST_P(PrerenderSessionHistoryBrowserTest,
                       NotEligibleForSameSite) {
  const GURL url1 = GetUrl("/title1.html");
  const GURL url2 = GetSameSiteCrossOriginUrl("/title2.html");
  PerformInitialNavigations(web_contents_impl(), url1, url2);

  ClearBackForwardCache(web_contents_impl());

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  PredictBackNavigation(web_contents_impl());
  PerformBackNavigation(web_contents_impl());

  ukm::SourceId source_id =
      web_contents_impl()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  histogram_tester.ExpectUniqueSample(
      "Preloading.PrerenderBackNavigationEligibility.MouseBackButton",
      PrerenderBackNavigationEligibility::kTargetIsSameSite, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.MouseBackButton.Precision",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.MouseBackButton.Recall",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Prerender.Attempt.MouseBackButton.Precision",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Prerender.Attempt.MouseBackButton.Recall",
      PredictorConfusionMatrix::kTruePositive, 1);
  ExpectAttemptUkm(ukm_recorder, true,
                   ToPreloadingEligibility(
                       PrerenderBackNavigationEligibility::kTargetIsSameSite),
                   source_id);
}

IN_PROC_BROWSER_TEST_P(PrerenderSessionHistoryBrowserTest,
                       NotEligibleForUncached) {
  const GURL url1 = GetUrl("/title1.html");
  const GURL url2 = GetCrossSiteUrl("/title2.html");
  PerformInitialNavigations(web_contents_impl(), url1, url2);

  // Ensure `url1` is not served from the HTTP cache or bfcache.
  ClearAllCaches(web_contents_impl());

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  PredictBackNavigation(web_contents_impl());
  PerformBackNavigation(web_contents_impl());

  ukm::SourceId source_id =
      web_contents_impl()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  histogram_tester.ExpectUniqueSample(
      "Preloading.PrerenderBackNavigationEligibility.MouseBackButton",
      PrerenderBackNavigationEligibility::kNoHttpCacheEntry, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.MouseBackButton.Precision",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.MouseBackButton.Recall",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Prerender.Attempt.MouseBackButton.Precision",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Prerender.Attempt.MouseBackButton.Recall",
      PredictorConfusionMatrix::kTruePositive, 1);
  ExpectAttemptUkm(ukm_recorder, true,
                   ToPreloadingEligibility(
                       PrerenderBackNavigationEligibility::kNoHttpCacheEntry),
                   source_id);
}

IN_PROC_BROWSER_TEST_P(PrerenderSessionHistoryBrowserTest,
                       NotEligibleForPostMethod) {
  const GURL url1 = GetUrl("/form_that_posts_to_echoall.html");
  const GURL url2 = GetUrl("/echoall");
  const GURL url3 = GetCrossSiteUrl("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), url1));

  TestNavigationObserver form_post_observer(web_contents_impl());
  ASSERT_TRUE(
      ExecJs(web_contents_impl(), "document.getElementById('form').submit();"));
  form_post_observer.Wait();
  ASSERT_EQ(url2, web_contents_impl()->GetLastCommittedURL());
  ASSERT_TRUE(web_contents_impl()
                  ->GetController()
                  .GetLastCommittedEntry()
                  ->GetHasPostData());

  NavigateAway(web_contents_impl(), url3);

  ClearBackForwardCache(web_contents_impl());

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  PredictBackNavigation(web_contents_impl());
  PerformBackNavigation(web_contents_impl());

  ukm::SourceId source_id =
      web_contents_impl()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  histogram_tester.ExpectUniqueSample(
      "Preloading.PrerenderBackNavigationEligibility.MouseBackButton",
      PrerenderBackNavigationEligibility::kMethodNotGet, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.MouseBackButton.Precision",
      PredictorConfusionMatrix::kTruePositive, 1);
  // A POST navigation is not a false negative for this predictor.
  histogram_tester.ExpectTotalCount(
      "Preloading.Predictor.MouseBackButton.Recall", 0);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Prerender.Attempt.MouseBackButton.Precision",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectTotalCount(
      "Preloading.Prerender.Attempt.MouseBackButton.Recall", 0);
  ExpectAttemptUkm(ukm_recorder, true,
                   ToPreloadingEligibility(
                       PrerenderBackNavigationEligibility::kMethodNotGet),
                   source_id);
}

IN_PROC_BROWSER_TEST_P(PrerenderSessionHistoryBrowserTest,
                       NotEligibleForFailedNavigation) {
  const GURL url1 = GetUrl("/page404.html");
  const GURL url2 = GetCrossSiteUrl("/title1.html");
  PerformInitialNavigations(web_contents_impl(), url1, url2);

  ClearBackForwardCache(web_contents_impl());

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  PredictBackNavigation(web_contents_impl());
  PerformBackNavigation(web_contents_impl());

  ukm::SourceId source_id =
      web_contents_impl()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  histogram_tester.ExpectUniqueSample(
      "Preloading.PrerenderBackNavigationEligibility.MouseBackButton",
      PrerenderBackNavigationEligibility::kTargetIsFailedNavigation, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.MouseBackButton.Precision",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.MouseBackButton.Recall",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Prerender.Attempt.MouseBackButton.Precision",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Prerender.Attempt.MouseBackButton.Recall",
      PredictorConfusionMatrix::kTruePositive, 1);
  ExpectAttemptUkm(
      ukm_recorder, true,
      ToPreloadingEligibility(
          PrerenderBackNavigationEligibility::kTargetIsFailedNavigation),
      source_id);
}

IN_PROC_BROWSER_TEST_P(PrerenderSessionHistoryBrowserTest,
                       NotEligibleForNonHttpScheme) {
  const GURL url1 = GURL("data:text/html,test");
  const GURL url2 = GetUrl("/title1.html");
  PerformInitialNavigations(web_contents_impl(), url1, url2);

  ClearBackForwardCache(web_contents_impl());

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  PredictBackNavigation(web_contents_impl());
  PerformBackNavigation(web_contents_impl());

  ukm::SourceId source_id =
      web_contents_impl()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  histogram_tester.ExpectUniqueSample(
      "Preloading.PrerenderBackNavigationEligibility.MouseBackButton",
      PrerenderBackNavigationEligibility::kTargetIsNonHttp, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.MouseBackButton.Precision",
      PredictorConfusionMatrix::kTruePositive, 1);
  // A navigation to a data URL is not a false negative for this predictor.
  histogram_tester.ExpectTotalCount(
      "Preloading.Predictor.MouseBackButton.Recall", 0);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Prerender.Attempt.MouseBackButton.Precision",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectTotalCount(
      "Preloading.Prerender.Attempt.MouseBackButton.Recall", 0);
  ExpectAttemptUkm(ukm_recorder, true,
                   ToPreloadingEligibility(
                       PrerenderBackNavigationEligibility::kTargetIsNonHttp),
                   source_id);
}

// Returns whether the two given windows can script each other.
// Assumes `opener` has a variable named `newWindow` which refers to `openee`.
bool IsScriptable(WebContentsImpl* opener, WebContentsImpl* openee) {
  // Have `opener` set a property such that `openee` can read it.
  const std::string kPropName = "mrPostman";
  const std::string kPropValue = "a property for me";

  if (EvalJs(opener, JsReplace(R"((() => {
                                 let result = '';
                                 try {
                                   newWindow[$1] = $2;
                                   result = newWindow[$1] || '';
                                 } catch {}
                                 return result;
                               })();)",
                               kPropName, kPropValue))
          .ExtractString() != kPropValue) {
    return false;
  }

  return EvalJs(openee, JsReplace("window[$1] || '';", kPropName))
             .ExtractString() == kPropValue;
}

IN_PROC_BROWSER_TEST_P(PrerenderSessionHistoryBrowserTest,
                       NotEligibleForRelatedActiveContents) {
  const GURL url1 = GetUrl("/title1.html");
  const GURL url2 = GetCrossSiteUrl("/title2.html");

  ASSERT_TRUE(NavigateToURL(shell(), url1));
  RenderFrameHostImplWrapper opener_rfh(current_frame_host());
  EXPECT_EQ(1u, opener_rfh->GetSiteInstance()->GetRelatedActiveContentsCount());

  ShellAddedObserver shell_observer;
  EXPECT_TRUE(
      ExecJs(shell(), JsReplace("window.newWindow = window.open($1);", url1)));
  Shell* popup = shell_observer.GetShell();
  WebContentsImpl* popup_contents =
      static_cast<WebContentsImpl*>(popup->web_contents());
  EXPECT_TRUE(WaitForLoadStop(popup_contents));
  EXPECT_TRUE(IsScriptable(web_contents_impl(), popup_contents));
  EXPECT_EQ(2u, opener_rfh->GetSiteInstance()->GetRelatedActiveContentsCount());

  NavigateAway(popup_contents, url2);
  ClearBackForwardCache(popup_contents);
  EXPECT_FALSE(IsScriptable(web_contents_impl(), popup_contents));
  RenderFrameHostImplWrapper cross_site_popup_rfh(
      popup_contents->GetPrimaryMainFrame());
  // Whether the SiteInstance changes depends on the process model. The default
  // SiteInstance could be in use.
  if (cross_site_popup_rfh->GetSiteInstance() ==
      opener_rfh->GetSiteInstance()) {
    EXPECT_EQ(2u,
              opener_rfh->GetSiteInstance()->GetRelatedActiveContentsCount());
  } else if (cross_site_popup_rfh->GetSiteInstance()->IsRelatedSiteInstance(
                 opener_rfh->GetSiteInstance())) {
    EXPECT_EQ(2u,
              opener_rfh->GetSiteInstance()->GetRelatedActiveContentsCount());
  } else {
    // `NavigateAway` may have swapped BrowsingInstances depending on test
    // parameterization.
    EXPECT_EQ(1u,
              opener_rfh->GetSiteInstance()->GetRelatedActiveContentsCount());
  }

  // `opener_rfh` is active and is using the same BrowsingInstance as the
  // popup's back navigation entry, so it would not be safe to prerender for
  // that entry.
  SiteInstanceImpl* target_site_instance =
      popup_contents->GetController().GetEntryAtOffset(-1)->site_instance();
  EXPECT_TRUE(opener_rfh->GetSiteInstance()->IsRelatedSiteInstance(
      target_site_instance));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  PredictBackNavigation(popup_contents);
  PerformBackNavigation(popup_contents);

  EXPECT_TRUE(IsScriptable(web_contents_impl(), popup_contents));
  EXPECT_EQ(2u, opener_rfh->GetSiteInstance()->GetRelatedActiveContentsCount());

  ukm::SourceId source_id =
      popup_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();

  histogram_tester.ExpectUniqueSample(
      "Preloading.PrerenderBackNavigationEligibility.MouseBackButton",
      PrerenderBackNavigationEligibility::kRelatedActiveContents, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.MouseBackButton.Precision",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.MouseBackButton.Recall",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Prerender.Attempt.MouseBackButton.Precision",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Prerender.Attempt.MouseBackButton.Recall",
      PredictorConfusionMatrix::kTruePositive, 1);
  ExpectAttemptUkm(
      ukm_recorder, true,
      ToPreloadingEligibility(
          PrerenderBackNavigationEligibility::kRelatedActiveContents),
      source_id);
}

IN_PROC_BROWSER_TEST_P(PrerenderSessionHistoryBrowserTest,
                       PredictAfterOpeneeDestroyed) {
  const GURL url1 = GetUrl("/title1.html");
  const GURL url2 = GetCrossSiteUrl("/title2.html");
  ASSERT_TRUE(NavigateToURL(shell(), url1));
  RenderFrameHostImplWrapper opener_rfh(current_frame_host());

  ShellAddedObserver shell_observer;
  EXPECT_TRUE(
      ExecJs(shell(), JsReplace("window.newWindow = window.open($1);", url1)));
  Shell* popup = shell_observer.GetShell();
  WebContentsImpl* popup_contents =
      static_cast<WebContentsImpl*>(popup->web_contents());
  EXPECT_TRUE(WaitForLoadStop(popup_contents));
  EXPECT_EQ(2u, opener_rfh->GetSiteInstance()->GetRelatedActiveContentsCount());

  NavigateAway(web_contents_impl(), url2);
  ClearBackForwardCache(web_contents_impl());

  WebContentsDestroyedWatcher close_popup_waiter(popup_contents);
  popup_contents->ClosePage();
  close_popup_waiter.Wait();

  // Unlike `NotEligibleForRelatedActiveContents`, there's no longer another
  // WebContents which is sharing the BrowsingInstance of the back navigation
  // entry, so it would be safe to prerender.

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  PredictBackNavigation(web_contents_impl());
  PerformBackNavigation(web_contents_impl());

  ukm::SourceId source_id =
      web_contents_impl()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  histogram_tester.ExpectUniqueSample(
      "Preloading.PrerenderBackNavigationEligibility.MouseBackButton",
      PrerenderBackNavigationEligibility::kEligible, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.MouseBackButton.Precision",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.MouseBackButton.Recall",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Prerender.Attempt.MouseBackButton.Precision",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Prerender.Attempt.MouseBackButton.Recall",
      PredictorConfusionMatrix::kTruePositive, 1);
  ExpectAttemptUkm(ukm_recorder, true, PreloadingEligibility::kEligible,
                   source_id);
}

IN_PROC_BROWSER_TEST_P(PrerenderSessionHistoryBrowserTest,
                       BackNavigationOfCloneWebContents) {
  const GURL url1 = GetUrl("/title1.html");
  const GURL url2 = GetCrossSiteUrl("/title2.html");
  PerformInitialNavigations(web_contents_impl(), url1, url2);

  // Whether the navigation from `url1` to `url2` swapped BrowsingInstances
  // depends on test parameterization and additional configuration options that
  // are not particularly relevant for the intended scope of this test. So we'll
  // just handle both possibilities as part of this test.
  SiteInstanceImpl* prev_site_instance = web_contents_impl()
                                             ->GetController()
                                             .GetEntryAtOffset(-1)
                                             ->site_instance();
  const bool original_navs_swapped_browsing_instance =
      !web_contents_impl()->GetSiteInstance()->IsRelatedSiteInstance(
          prev_site_instance);

  FakeWebContentsDelegate clone_delegate;
  std::unique_ptr<WebContents> new_web_contents_owned =
      web_contents_impl()->Clone();
  WebContentsImpl* new_web_contents =
      static_cast<WebContentsImpl*>(new_web_contents_owned.get());
  new_web_contents->SetDelegate(&clone_delegate);
  TestNavigationObserver clone_load_observer(new_web_contents);
  new_web_contents->GetController().LoadIfNecessary();
  clone_load_observer.Wait();

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  PredictBackNavigation(new_web_contents);
  PerformBackNavigation(new_web_contents);

  const PrerenderBackNavigationEligibility expected_eligibility =
      original_navs_swapped_browsing_instance
          ? PrerenderBackNavigationEligibility::kEligible
          : PrerenderBackNavigationEligibility::kRelatedActiveContents;
  ukm::SourceId source_id =
      new_web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();

  histogram_tester.ExpectUniqueSample(
      "Preloading.PrerenderBackNavigationEligibility.MouseBackButton",
      expected_eligibility, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.MouseBackButton.Precision",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.MouseBackButton.Recall",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Prerender.Attempt.MouseBackButton.Precision",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Prerender.Attempt.MouseBackButton.Recall",
      PredictorConfusionMatrix::kTruePositive, 1);
  ExpectAttemptUkm(ukm_recorder, true,
                   ToPreloadingEligibility(expected_eligibility), source_id);
}

IN_PROC_BROWSER_TEST_P(
    PrerenderSessionHistoryBrowserTest,
    BackNavigationOfClonedWebContentsWithOriginalAtTargetEntry) {
  const GURL url1 = GetUrl("/title1.html");
  const GURL url2 = GetCrossSiteUrl("/title2.html");
  PerformInitialNavigations(web_contents_impl(), url1, url2);

  FakeWebContentsDelegate clone_delegate;
  std::unique_ptr<WebContents> new_web_contents_owned =
      web_contents_impl()->Clone();
  WebContentsImpl* new_web_contents =
      static_cast<WebContentsImpl*>(new_web_contents_owned.get());
  new_web_contents->SetDelegate(&clone_delegate);
  TestNavigationObserver clone_load_observer(new_web_contents);
  new_web_contents->GetController().LoadIfNecessary();
  clone_load_observer.Wait();

  PerformBackNavigation(web_contents_impl());

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  PredictBackNavigation(new_web_contents);
  PerformBackNavigation(new_web_contents);

  ukm::SourceId source_id =
      new_web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();

  histogram_tester.ExpectUniqueSample(
      "Preloading.PrerenderBackNavigationEligibility.MouseBackButton",
      PrerenderBackNavigationEligibility::kRelatedActiveContents, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.MouseBackButton.Precision",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.MouseBackButton.Recall",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Prerender.Attempt.MouseBackButton.Precision",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Prerender.Attempt.MouseBackButton.Recall",
      PredictorConfusionMatrix::kTruePositive, 1);
  ExpectAttemptUkm(
      ukm_recorder, true,
      ToPreloadingEligibility(
          PrerenderBackNavigationEligibility::kRelatedActiveContents),
      source_id);
}

// PrerenderHosts created through speculation rules are not suitable for use in
// session history navigations. In particular, the SiteInstances would be
// mismatched.
IN_PROC_BROWSER_TEST_P(
    PrerenderSessionHistoryBrowserTest,
    BackButtonNavigationDoesNotUseSpeculationRulePrerenders) {
  const GURL url1 = GetUrl("/title1.html");
  const GURL url2 = GetUrl("/title2.html");
  PerformInitialNavigations(web_contents_impl(), url1, url2);
  ClearBackForwardCache(web_contents_impl());

  FrameTreeNodeId host_id = AddPrerender(url1);
  test::PrerenderHostObserver prerender_observer(*web_contents(), host_id);

  PerformBackNavigation(web_contents_impl());

  EXPECT_FALSE(prerender_observer.was_activated());
}

class PrerenderWarmUpCompositorBrowserTest
    : public PrerenderBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool, std::string>> {
 public:
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    auto [warm_up_compositor, prerender2_warm_up_compositor,
          prerender2_warm_up_compositor_trigger_point] = info.param;
    std::stringstream params_description;
    params_description << "kWarmUpCompositor";
    params_description << (warm_up_compositor ? "Enabled" : "Disabled");
    params_description << "_kPrerender2WarmUpCompositor";
    params_description << (prerender2_warm_up_compositor ? "Enabled"
                                                         : "Disabled");
    params_description << "_" << prerender2_warm_up_compositor_trigger_point;
    return params_description.str();
  }

  PrerenderWarmUpCompositorBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    auto [warm_up_compositor, prerender2_warm_up_compositor,
          prerender2_warm_up_compositor_trigger_point] = GetParam();
    if (warm_up_compositor) {
      enabled_features.push_back({features::kWarmUpCompositor, {}});
    } else {
      disabled_features.push_back(features::kWarmUpCompositor);
    }

    if (prerender2_warm_up_compositor) {
      enabled_features.push_back(
          {blink::features::kPrerender2WarmUpCompositor,
           {{"trigger_point", prerender2_warm_up_compositor_trigger_point}}});
    } else {
      disabled_features.push_back(blink::features::kPrerender2WarmUpCompositor);
    }

    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PrerenderWarmUpCompositorBrowserTest,
    // Flips `kWarmUpCompositor` (cc) and `kPrerender2WarmUpCompositor`(blink)
    // and puts the value of `kPrerender2WarmUpCompositorTriggerPoint` (if
    // latter flag is enabled).
    testing::Values(
        std::make_tuple(true, true, "did_commit_load"),
        std::make_tuple(true, true, "did_dispatch_dom_content_loaded_event"),
        std::make_tuple(true, true, "did_finish_load"),
        // `kWarmUpCompositor` controls the independent cc internal feature of
        // warming up, and `kPrerender2WarmUpCompositor` manages the trigger
        // point of that feature for prerender case. Therefore, warming up on
        // prerender should not performed in the first place unless both flags
        // are enabled.
        std::make_tuple(true, false, ""),
        std::make_tuple(false, true, "did_commit_load"),
        std::make_tuple(false, true, "did_dispatch_dom_content_loaded_event"),
        std::make_tuple(false, true, "did_finish_load")),
    PrerenderWarmUpCompositorBrowserTest::DescribeParams);

// Test that the prerendering page does not crash when enabling compositor
// warming up features.
// TODO(crbug.com/41496019): Check whether the warming up is actually happening.
IN_PROC_BROWSER_TEST_P(PrerenderWarmUpCompositorBrowserTest,
                       WarmingUpCCDoesntInvokeCrashes) {
  const GURL initial_url = GetUrl("/empty.html");
  const GURL prerendering_url = GetUrl("/empty.html?prerender");

  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  auto prerender_handle = AddEmbedderTriggeredPrerender(
      prerendering_url, /*preloading_attempt=*/nullptr,
      /*should_warm_up_compositor=*/true);
  FrameTreeNodeId prerender_host_id =
      static_cast<PrerenderHandleImpl*>(prerender_handle.get())
          ->frame_tree_node_id_for_testing();
  test::PrerenderHostObserver prerender_observer(*web_contents(),
                                                 prerender_host_id);

  NavigatePrimaryPageFromAddressBar(prerendering_url);
  prerender_observer.WaitForActivation();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), prerendering_url);
  EXPECT_TRUE(prerender_observer.was_activated());
}

}  // namespace content
