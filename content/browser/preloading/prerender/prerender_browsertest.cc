// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
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
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/thread_annotations.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/services/storage/public/mojom/storage_service.mojom.h"
#include "components/services/storage/public/mojom/test_api.test-mojom.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/file_system_access/file_system_chooser_test_helpers.h"
#include "content/browser/portal/portal.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_attempt_impl.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/preloading/prerender/prerender_metrics.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/input/synthetic_tap_gesture.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigation_type.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/background_color_change_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/mock_client_hints_controller_delegate.h"
#include "content/public/test/mock_web_contents_observer.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/theme_change_waiter.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/mock_commit_deferring_condition.h"
#include "content/test/portal/portal_activated_observer.h"
#include "content/test/portal/portal_created_observer.h"
#include "content/test/render_document_feature.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_mojo_binder_policy_applier_unittest.mojom.h"
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
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/loader_constants.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "third_party/blink/public/mojom/page/display_cutout.mojom.h"
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
using ukm::builders::Preloading_Prediction;
static const auto kMockElapsedTime =
    base::ScopedMockElapsedTimersForTest::kMockElapsedTime;

// Tests the params of WebContentsImpl that contains a prerendered page for a
// new tab navigation.
void ExpectWebContentsIsForNewTabPrerendering(WebContentsImpl& web_contents) {
  // The primary page shows the initial blank page.
  EXPECT_TRUE(web_contents.GetLastCommittedURL().is_empty());

  // The prerendering WebContents should not have an opener to avoid cross-page
  // scripting during prerendering.
  EXPECT_FALSE(web_contents.HasOpener());

  // The prerendering WebContents should be hidden until prerender activation.
  EXPECT_TRUE(web_contents.IsHidden());
}

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
    feature_list_.InitAndEnableFeature(blink::features::kPrerender2InNewTab);
  }
  ~PrerenderBrowserTest() override = default;

  void SetUp() override {
    prerender_helper_->SetUp(&ssl_server_);
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    host_resolver()->AddRule("*", "127.0.0.1");
    attempt_ukm_entry_builder_ =
        std::make_unique<test::PreloadingAttemptUkmEntryBuilder>(
            content_preloading_predictor::kSpeculationRules);
    prediction_ukm_entry_builder_ =
        std::make_unique<test::PreloadingPredictionUkmEntryBuilder>(
            content_preloading_predictor::kSpeculationRules);
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

  // Waits until the request count for `url` reaches `count`.
  void WaitForRequest(const GURL& url, int count) {
    prerender_helper_->WaitForRequest(url, count);
  }

  int AddPrerender(const GURL& prerendering_url) {
    return prerender_helper_->AddPrerender(prerendering_url);
  }

  void AddPrerenderAsync(const GURL& prerendering_url) {
    prerender_helper_->AddPrerenderAsync(prerendering_url);
  }

  void AddMultiplePrerenderAsync(const std::vector<GURL>& prerendering_urls) {
    prerender_helper_->AddMultiplePrerenderAsync(prerendering_urls);
  }

  void AddPrerenderWithTargetHintAsync(const GURL& prerendering_url,
                                       const std::string& target_hint) {
    prerender_helper_->AddPrerenderWithTargetHintAsync(prerendering_url,
                                                       target_hint);
  }

  bool AddTestUtilJS(RenderFrameHost* host) {
    bool success = false;
    std::string js = R"(
        const script = document.createElement("script");
        script.addEventListener('load', () => {
          window.domAutomationController.send(true);
        });
        script.addEventListener('error', () => {
          window.domAutomationController.send(false);
        });
        script.src = "/prerender/test_utils.js";
        document.body.appendChild(script);
    )";
    EXPECT_TRUE(ExecuteScriptAndExtractBool(host, js, &success));
    return success;
  }

  void NavigatePrimaryPage(const GURL& url) {
    prerender_helper_->NavigatePrimaryPage(url);
  }

  int GetHostForUrl(const GURL& url) {
    return prerender_helper_->GetHostForUrl(url);
  }

  RenderFrameHostImpl* GetPrerenderedMainFrameHost(int host_id) {
    return static_cast<RenderFrameHostImpl*>(
        prerender_helper_->GetPrerenderedMainFrameHost(host_id));
  }

  void NavigatePrerenderedPage(int host_id, const GURL& url) {
    return prerender_helper_->NavigatePrerenderedPage(host_id, url);
  }

  bool HasHostForUrl(const GURL& url) {
    int host_id = GetHostForUrl(url);
    return host_id != RenderFrameHost::kNoFrameTreeNodeId;
  }

  void WaitForPrerenderLoadCompleted(int host_id) {
    prerender_helper_->WaitForPrerenderLoadCompletion(host_id);
  }

  void WaitForPrerenderLoadCompletion(const GURL& url) {
    prerender_helper_->WaitForPrerenderLoadCompletion(url);
  }

  GURL GetUrl(const std::string& path) {
    return ssl_server_.GetURL("a.test", path);
  }

  GURL GetCrossSiteUrl(const std::string& path) {
    return ssl_server_.GetURL("b.test", path);
  }

  GURL GetUrlForSameSiteCrossOriginTest(const std::string& path) {
    return ssl_server().GetURL("a.a.test", path);
  }

  GURL GetSameSiteCrossOriginUrl(const std::string& path) {
    return ssl_server().GetURL("b.a.test", path);
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

  const test::PreloadingPredictionUkmEntryBuilder&
  prediction_ukm_entry_builder() {
    return *prediction_ukm_entry_builder_;
  }

  void TestHostPrerenderingState(const GURL& prerender_url) {
    const GURL kInitialUrl = GetUrl("/empty.html");

    // Navigate to an initial page.
    ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

    // The initial page should not be in prerendered state.
    RenderFrameHostImpl* initiator_render_frame_host = current_frame_host();
    EXPECT_EQ(initiator_render_frame_host->frame_tree()->type(),
              FrameTree::Type::kPrimary);
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
    // Useful for testing CSP:prefetch-src
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
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

  void AssertPrerenderHistoryLength(int host_id,
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

  void ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus status) {
    // Check FinalStatus in UMA.
    histogram_tester().ExpectUniqueSample(
        "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
        status, 1);

    // Check all entries in UKM to make sure that the recorded FinalStatus is
    // equal to `status`. At least one entry should exist.
    bool final_status_entry_found = false;
    const auto entries = ukm_recorder_->GetEntriesByName(
        ukm::builders::PrerenderPageLoad::kEntryName);
    for (const auto* entry : entries) {
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

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

  // Stores all the navigation_ids for all navigations. This is used to check
  // that we record UKMs for correct SourceIds.
  std::vector<int64_t> navigation_ids_;

 protected:
  net::test_server::EmbeddedTestServer& ssl_server() { return ssl_server_; }

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
  std::unique_ptr<test::PreloadingPredictionUkmEntryBuilder>
      prediction_ukm_entry_builder_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<base::ScopedMockElapsedTimersForTest> scoped_test_timer_;
};
}  // namespace

// Tests that the speculationrules trigger works.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, SpeculationRulesPrerender) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prerendering `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  int host_id = AddPrerender(kPrerenderingUrl);
  ASSERT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  NavigationHandleObserver activation_observer(web_contents(),
                                               kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);

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

    UkmEntry attempt_expected_entry = attempt_ukm_entry_builder().BuildEntry(
        activation_id, PreloadingType::kPrerender,
        PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
        PreloadingTriggeringOutcome::kSuccess,
        PreloadingFailureReason::kUnspecified,
        /*accurate=*/true,
        /*ready_time=*/kMockElapsedTime);

    UkmEntry prediction_expected_entry =
        prediction_ukm_entry_builder().BuildEntry(ukm_source_id,
                                                  /*confidence=*/100,
                                                  /*accurate_prediction=*/true);

    EXPECT_EQ(attempt_ukm_entries[0], attempt_expected_entry)
        << test::ActualVsExpectedUkmEntryToString(attempt_ukm_entries[0],
                                                  attempt_expected_entry);
    EXPECT_EQ(prediction_ukm_entries[0], prediction_expected_entry)
        << test::ActualVsExpectedUkmEntryToString(prediction_ukm_entries[1],
                                                  prediction_expected_entry);
  }
}

// Tests that the speculationrules-triggered prerender would be destroyed after
// its initiator navigates away.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, SpeculationInitiatorNavigateAway) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  int host_id = AddPrerender(kPrerenderingUrl);

  // Navigate the initiator page to a non-prerendered page. This destroys the
  // prerendered page.
  test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);
  NavigatePrimaryPage(GetUrl("/empty.html?elsewhere"));
  host_observer.WaitForDestroyed();

  // The prerender host should be destroyed.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  {
    // Cross-check that in case where the navigation happens to a different
    // page, we log the correct metrics.
    ukm::SourceId ukm_source_id = PrimaryPageSourceId();
    auto attempt_ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName, test::kPreloadingAttemptUkmMetrics);
    auto prediction_ukm_entries =
        test_ukm_recorder()->GetEntries(Preloading_Prediction::kEntryName,
                                        test::kPreloadingPredictionUkmMetrics);
    EXPECT_EQ(prediction_ukm_entries.size(), 1u);
    EXPECT_EQ(attempt_ukm_entries.size(), 1u);

    UkmEntry attempt_expected_entry = attempt_ukm_entry_builder().BuildEntry(
        ukm_source_id, PreloadingType::kPrerender,
        PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
        PreloadingTriggeringOutcome::kReady,
        PreloadingFailureReason::kUnspecified,
        /*accurate=*/false,
        /*ready_time=*/kMockElapsedTime);

    UkmEntry prediction_expected_entry =
        prediction_ukm_entry_builder().BuildEntry(
            ukm_source_id, /*confidence=*/100,
            /*accurate_prediction=*/false);

    EXPECT_EQ(attempt_ukm_entries[0], attempt_expected_entry)
        << test::ActualVsExpectedUkmEntryToString(attempt_ukm_entries[0],
                                                  attempt_expected_entry);
    EXPECT_EQ(prediction_ukm_entries[0], prediction_expected_entry)
        << test::ActualVsExpectedUkmEntryToString(prediction_ukm_entries[1],
                                                  prediction_expected_entry);
  }
}

// Tests that clicking a link can activate a prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, ActivateOnLinkClick) {
  const GURL kInitialUrl = GetUrl("/simple_links.html");
  const GURL kPrerenderingUrl = GetUrl("/title2.html");

  // Navigate to an initial page which has a link to `kPrerenderingUrl`.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl`.
  int prerender_host_id = AddPrerender(kPrerenderingUrl);
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
  int prerender_host_id = AddPrerender(kPrerenderingUrl);
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
  TestNavigationObserver nav_observer(kPrerenderingUrl);
  nav_observer.StartWatchingNewWebContents();
  AddPrerenderWithTargetHintAsync(kPrerenderingUrl, "_blank");
  nav_observer.WaitForNavigationFinished();
  EXPECT_EQ(nav_observer.last_navigation_url(), kPrerenderingUrl);

  PrerenderHost* prerender_host =
      web_contents_impl()->GetPrerenderHostRegistry()->FindHostByUrlForTesting(
          kPrerenderingUrl);
  ASSERT_TRUE(prerender_host);
  auto* prerender_web_contents = WebContentsImpl::FromFrameTreeNode(
      prerender_host->GetPrerenderFrameTree().root());
  ASSERT_NE(prerender_web_contents, web_contents_impl());
  ExpectWebContentsIsForNewTabPrerendering(*prerender_web_contents);

  // Click the link annotated with "target=_blank". This should activate the
  // prerendered page.
  test::PrerenderHostObserver prerender_observer(
      *prerender_web_contents, prerender_host->frame_tree_node_id());
  const std::string kLinkClickScript = R"(
      clickSameSiteNewWindowLink();
  )";
  EXPECT_TRUE(ExecJs(web_contents(), kLinkClickScript));
  EXPECT_EQ(prerender_web_contents->GetLastCommittedURL(), kPrerenderingUrl);
  EXPECT_TRUE(prerender_observer.was_activated());
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);

  // The navigation occurred in a new WebContents, so the original WebContents
  // should still be showing the initial trigger page.
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);
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
  int prerender_host_id = AddPrerender(kPrerenderingUrl);
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
  TestNavigationObserver nav_observer(kPrerenderingUrl);
  nav_observer.StartWatchingNewWebContents();
  AddPrerenderWithTargetHintAsync(kPrerenderingUrl, "_blank");
  nav_observer.WaitForNavigationFinished();
  EXPECT_EQ(nav_observer.last_navigation_url(), kPrerenderingUrl);

  PrerenderHost* prerender_host =
      web_contents_impl()->GetPrerenderHostRegistry()->FindHostByUrlForTesting(
          kPrerenderingUrl);
  ASSERT_TRUE(prerender_host);
  auto* prerender_web_contents = WebContentsImpl::FromFrameTreeNode(
      prerender_host->GetPrerenderFrameTree().root());
  ASSERT_NE(prerender_web_contents, web_contents_impl());
  ExpectWebContentsIsForNewTabPrerendering(*prerender_web_contents);

  // Click the link annotated with "target=_blank rel=noopener". This should
  // activate the prerendered page.
  test::PrerenderHostObserver prerender_observer(
      *prerender_web_contents, prerender_host->frame_tree_node_id());
  const std::string kLinkClickScript = R"(
      clickSameSiteNewWindowWithNoopenerLink();
  )";
  EXPECT_TRUE(ExecJs(web_contents(), kLinkClickScript));
  EXPECT_EQ(prerender_web_contents->GetLastCommittedURL(), kPrerenderingUrl);
  EXPECT_TRUE(prerender_observer.was_activated());
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);

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
  int prerender_host_id = AddPrerender(kPrerenderingUrl);
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

  // The prerendered page should be destroyed as the trigger page navigated
  // away.
  prerender_observer.WaitForDestroyed();
  EXPECT_EQ(GetHostForUrl(kPrerenderingUrl),
            RenderFrameHost::kNoFrameTreeNodeId);
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kTriggerDestroyed);
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
  TestNavigationObserver nav_observer(kPrerenderingUrl);
  nav_observer.StartWatchingNewWebContents();
  AddPrerenderWithTargetHintAsync(kPrerenderingUrl, "_blank");
  nav_observer.WaitForNavigationFinished();
  EXPECT_EQ(nav_observer.last_navigation_url(), kPrerenderingUrl);

  PrerenderHost* prerender_host =
      web_contents_impl()->GetPrerenderHostRegistry()->FindHostByUrlForTesting(
          kPrerenderingUrl);
  ASSERT_TRUE(prerender_host);
  auto* prerender_web_contents = WebContentsImpl::FromFrameTreeNode(
      prerender_host->GetPrerenderFrameTree().root());
  ASSERT_NE(prerender_web_contents, web_contents_impl());
  ExpectWebContentsIsForNewTabPrerendering(*prerender_web_contents);

  // Click the link annotated with "target=_blank rel=opener". This should not
  // activate the prerendered page.
  test::PrerenderHostObserver prerender_observer(
      *prerender_web_contents, prerender_host->frame_tree_node_id());
  const std::string kLinkClickScript = R"(
      clickSameSiteNewWindowWithOpenerLink();
  )";
  EXPECT_TRUE(ExecJs(web_contents(), kLinkClickScript));
  // The WebContents pre-created for prerendering should not be used.
  EXPECT_NE(prerender_web_contents->GetLastCommittedURL(), kPrerenderingUrl);
  EXPECT_FALSE(prerender_observer.was_activated());
  // The host should still be available.
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl));

  // The navigation occurred in a new WebContents, so the original WebContents
  // should still be showing the initial trigger page.
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);
}

// TODO(crbug.com/1350676): Add more test cases for prerender-in-new-tab:
// - Multiple prerendering requests with the same URL but different target hint.
// - Navigation in a new tab to the prerendering URL multiple times. Only the
//   first navigation should activate the prerendered page.
// - Cancellation of prerender-in-new-tab (e.g., removing speculation rules,
//   calling disallowed features in a prerendered page).
// - Behavior of PrerenderNewTabHandle::WebContentsDelegateImpl.

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

// Tests that prerendering is cancelled if a network request for the
// navigation results in an empty response with 404 status.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderCancelledOnEmptyBody404) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  // Specify a URL for which we don't have a corresponding file in the data dir.
  const GURL kPrerenderingUrl = GetUrl("/404");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl`.
  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());
  AddPrerenderAsync(kPrerenderingUrl);
  registry_observer.WaitForTrigger(kPrerenderingUrl);
  int host_id = GetHostForUrl(kPrerenderingUrl);
  test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);
  host_observer.WaitForDestroyed();
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kNavigationBadHttpStatus);
}

// Tests that prerendering is cancelled if a network request for the
// navigation results in an non-empty response with 404 status.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderCancelledOnNonEmptyBody404) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/page404.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Add prerendering to the 404 error page, then check that it got cancelled.
  test::PrerenderHostObserver host_observer(*web_contents_impl(),
                                            kPrerenderingUrl);
  AddPrerenderAsync(kPrerenderingUrl);
  host_observer.WaitForDestroyed();
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kNavigationBadHttpStatus);
}

// Tests that prerendering is cancelled if a network request for the
// navigation results in an non-empty response with 500 status.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderCancelledOn500Page) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/page500.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Add prerendering to the 500 error page, then check that it got cancelled.
  test::PrerenderHostObserver host_observer(*web_contents_impl(),
                                            kPrerenderingUrl);
  AddPrerenderAsync(kPrerenderingUrl);
  host_observer.WaitForDestroyed();
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kNavigationBadHttpStatus);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderCancelledOn204Page) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl` that returns 204 response code.
  const GURL kPrerenderingUrl = GetUrl("/echo?status=204");
  test::PrerenderHostObserver host_observer(*web_contents_impl(),
                                            kPrerenderingUrl);
  AddPrerenderAsync(kPrerenderingUrl);

  // The prerender should be destroyed.
  host_observer.WaitForDestroyed();
  EXPECT_EQ(GetHostForUrl(kPrerenderingUrl),
            RenderFrameHost::kNoFrameTreeNodeId);

  // Cancellation must have occurred due to bad http status code.
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kNavigationBadHttpStatus);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderCancelledOn205Page) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl` that returns 205 response code.
  const GURL kPrerenderingUrl = GetUrl("/echo?status=205");
  test::PrerenderHostObserver host_observer(*web_contents_impl(),
                                            kPrerenderingUrl);
  AddPrerenderAsync(kPrerenderingUrl);

  // The prerender should be destroyed.
  host_observer.WaitForDestroyed();
  EXPECT_EQ(GetHostForUrl(kPrerenderingUrl),
            RenderFrameHost::kNoFrameTreeNodeId);

  // Cancellation must have occurred due to bad http status code.
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
  int host_id = AddPrerender(kPrerenderingUrl);
  test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);
  WaitForPrerenderLoadCompletion(kPrerenderingUrl);

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
  EXPECT_EQ(GetHostForUrl(kPrerenderingUrl),
            RenderFrameHost::kNoFrameTreeNodeId);

  // Navigate primary page to flush the metrics.
  const GURL kNavigatedURL = GetUrl("/title2.html");
  ASSERT_TRUE(NavigateToURL(shell(), kNavigatedURL));

  {
    // Cross-check that Preloading.Attempt logs the correct failure reason.
    ukm::SourceId ukm_source_id = PrimaryPageSourceId();
    auto attempt_ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName, test::kPreloadingAttemptUkmMetrics);

    UkmEntry attempt_expected_entry = attempt_ukm_entry_builder().BuildEntry(
        ukm_source_id, PreloadingType::kPrerender,
        PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
        PreloadingTriggeringOutcome::kFailure,
        ToPreloadingFailureReason(PrerenderFinalStatus::kLoginAuthRequested),
        /*accurate=*/false);

    EXPECT_EQ(attempt_ukm_entries[0], attempt_expected_entry)
        << test::ActualVsExpectedUkmEntryToString(attempt_ukm_entries[0],
                                                  attempt_expected_entry);
  }

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
  int host_id = AddPrerender(kPrerenderingUrl);
  test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);
  WaitForPrerenderLoadCompletion(kPrerenderingUrl);

  // Fetch a subframe that requires authentication.
  const GURL kAuthIFrameUrl = GetUrl("/auth-basic");
  RenderFrameHost* prerender_rfh = GetPrerenderedMainFrameHost(host_id);
  std::ignore =
      ExecJs(prerender_rfh,
             "const i = document.createElement('iframe'); i.src = '" +
                 kAuthIFrameUrl.spec() + "'; document.body.appendChild(i);");

  // The prerender should be destroyed.
  host_observer.WaitForDestroyed();
  EXPECT_EQ(GetHostForUrl(kPrerenderingUrl),
            RenderFrameHost::kNoFrameTreeNodeId);

  // Cancellation must have occurred due to authentication request.
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kLoginAuthRequested);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, CancelOnAuthRequestedSubResource) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());

  // Start prerendering `kPrerenderingUrl`.
  const GURL kPrerenderingUrl = GetUrl("/title1.html");
  int host_id = AddPrerender(kPrerenderingUrl);
  test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);
  WaitForPrerenderLoadCompletion(kPrerenderingUrl);

  ASSERT_NE(GetHostForUrl(kPrerenderingUrl),
            RenderFrameHost::kNoFrameTreeNodeId);

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
  EXPECT_EQ(GetHostForUrl(kPrerenderingUrl),
            RenderFrameHost::kNoFrameTreeNodeId);

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
  int host_id = GetHostForUrl(kPrerenderingUrl);
  ASSERT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);

  // Remove the rules and check that the prerender is cancelled with an
  // appropriate final status.
  test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);
  ASSERT_TRUE(ExecJs(
      web_contents_impl()->GetPrimaryMainFrame(),
      "document.querySelector('script[type=speculationrules]').remove()"));
  host_observer.WaitForDestroyed();
  EXPECT_EQ(GetHostForUrl(kPrerenderingUrl),
            RenderFrameHost::kNoFrameTreeNodeId);
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kTriggerDestroyed);
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
  int host_id = GetHostForUrl(kPrerenderingUrl);
  ASSERT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);

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
    ASSERT_NE(GetHostForUrl(kPrerenderingUrl),
              RenderFrameHost::kNoFrameTreeNodeId);
  }

  // Remove the rules and check that the prerender is cancelled.
  test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);
  ASSERT_TRUE(ExecJs(
      web_contents_impl()->GetPrimaryMainFrame(),
      "document.querySelector('script[type=speculationrules]').remove()"));
  host_observer.WaitForDestroyed();
  EXPECT_EQ(GetHostForUrl(kPrerenderingUrl),
            RenderFrameHost::kNoFrameTreeNodeId);
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
  int host_id = GetHostForUrl(kPrerenderingUrl);
  ASSERT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);

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
  EXPECT_EQ(GetHostForUrl(kPrerenderingUrl),
            RenderFrameHost::kNoFrameTreeNodeId);

  // And the new one should be discovered.
  registry_observer.WaitForTrigger(kPrerenderingUrl2);
  int second_host_id = GetHostForUrl(kPrerenderingUrl2);
  EXPECT_NE(second_host_id, RenderFrameHost::kNoFrameTreeNodeId);
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
    int host_id = GetHostForUrl(kPrerenderingUrl);
    ASSERT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);
    test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);

    // Remove the rules and check that the prerender is cancelled with an
    // appropriate final status.
    ASSERT_TRUE(ExecJs(
        web_contents_impl()->GetPrimaryMainFrame(),
        "document.querySelector('script[type=speculationrules]').remove()"));
    host_observer.WaitForDestroyed();
    EXPECT_EQ(GetHostForUrl(kPrerenderingUrl),
              RenderFrameHost::kNoFrameTreeNodeId);
  }
  {
    AddPrerender(kPrerenderingUrl);
    int host_id = GetHostForUrl(kPrerenderingUrl);
    EXPECT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);
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
  int host_id = AddPrerender(kPrerenderChain1);

  EXPECT_EQ(GetRequestCount(kPrerenderChain1), 1);
  EXPECT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);
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

  {
    // Cross-check that in case redirection when the prerender navigates and
    // user ends up navigating to the redirected URL. accurate_triggering is
    // true.
    ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
    auto attempt_ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName, test::kPreloadingAttemptUkmMetrics);

    UkmEntry attempt_expected_entry = attempt_ukm_entry_builder().BuildEntry(
        ukm_source_id, PreloadingType::kPrerender,
        PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
        PreloadingTriggeringOutcome::kSuccess,
        PreloadingFailureReason::kUnspecified,
        /*accurate=*/true,
        /*ready_time=*/kMockElapsedTime);

    EXPECT_EQ(attempt_ukm_entries[0], attempt_expected_entry)
        << test::ActualVsExpectedUkmEntryToString(attempt_ukm_entries[0],
                                                  attempt_expected_entry);
  }
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
  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());
  test::PrerenderHostObserver host_observer(*web_contents_impl(),
                                            kPrerenderingUrl);
  AddPrerenderAsync(kPrerenderingUrl);
  host_observer.WaitForDestroyed();
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl), 0);
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  EXPECT_FALSE(HasHostForUrl(kRedirectedUrl));
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kCrossSiteRedirect);
}

// Makes sure that activation on navigation for an iframes doesn't happen.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, Activation_iFrame) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  EXPECT_TRUE(AddTestUtilJS(current_frame_host()));

  // Start a prerender.
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");
  int host_id = AddPrerender(kPrerenderingUrl);

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
  int host_id = AddPrerender(kPrerenderingUrl);
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
    WaitForPrerenderLoadCompleted(host_id);
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
      shell()->web_contents()->OpenURL(OpenURLParams(
          kSameOriginSubframeUrl3, Referrer(),
          child_frame->GetFrameTreeNodeId(), WindowOpenDisposition::CURRENT_TAB,
          ui::PAGE_TRANSITION_AUTO_SUBFRAME,
          /*is_renderer_initiated=*/false));
      capturer.Wait();

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
  int host_id = AddPrerender(kPrerenderingUrl);
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
  int host_id = AddPrerender(kPrerenderingUrl);

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
  DCHECK(same_origin_render_frame_host);
  EXPECT_EQ(true, EvalJs(same_origin_render_frame_host,
                         kInitialDocumentPrerenderingScript));
  EXPECT_EQ(false, EvalJs(same_origin_render_frame_host,
                          kCurrentDocumentPrerenderingScript));
  EXPECT_EQ(true, EvalJs(same_origin_render_frame_host,
                         kOnprerenderingchangeObservedScript));
  EXPECT_NE(0, EvalJs(same_origin_render_frame_host, kActivationStartScript));

  RenderFrameHost* cross_origin_render_frame_host = FindRenderFrameHost(
      prerender_frame_host->GetPage(), kCrossOriginSubframeUrl);
  DCHECK(cross_origin_render_frame_host);
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
  int host_id = AddPrerender(kPrerenderingUrl);

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
  DCHECK(cross_origin_render_frame_host);
  EXPECT_EQ(false, EvalJs(cross_origin_render_frame_host,
                          kInitialDocumentPrerenderingScript));
  EXPECT_EQ(false, EvalJs(cross_origin_render_frame_host,
                          kCurrentDocumentPrerenderingScript));
  EXPECT_EQ(false, EvalJs(cross_origin_render_frame_host,
                          kOnprerenderingchangeObservedScript));
}

// Test main frame navigation in prerendering page cancels the prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MainFrameNavigationCancelsPrerendering) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");
  const GURL kHungUrl = GetUrl("/hung");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  int host_id = AddPrerender(kPrerenderingUrl);

  // Start a navigation in the prerender frame tree that will cancel the
  // initiator's prerendering.
  test::PrerenderHostObserver observer(*web_contents_impl(), host_id);

  NavigatePrerenderedPage(host_id, kHungUrl);

  observer.WaitForDestroyed();
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kMainFrameNavigation);
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
  int host_id = AddPrerender(kPrerenderingUrl);
  WaitForPrerenderLoadCompleted(host_id);

  // Do a fragment navigation.
  NavigatePrerenderedPage(host_id, kAnchorUrl);
  WaitForPrerenderLoadCompleted(host_id);

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
  int host_id = AddPrerender(kPrerenderingUrl);

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

  // Activation shouldn't happen, so the prerender host should not be consumed.
  // However, we don't check the existence of the prerender host here unlike
  // other activation tests because navigating the frame that triggered
  // prerendering abandons the prerendered page regardless of activation.
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

// Tests that an inner WebContents can be attached in a prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, ActivatePageWithInnerContents) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_blank_iframe.html");
  const GURL kInnerContentsUrl = GetUrl("/empty.html?prerender");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  int host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerendered_render_frame_host =
      GetPrerenderedMainFrameHost(host_id);
  WebContentsImpl* inner_contents =
      static_cast<WebContentsImpl*>(CreateAndAttachInnerContents(
          prerendered_render_frame_host->child_at(0)->current_frame_host()));
  ASSERT_TRUE(NavigateToURLFromRenderer(inner_contents, kInnerContentsUrl));

  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kInnerContentsUrl), 1);
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
            DCHECK(!throttle);
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

    int host_id = GetHostForUrl(kPrerenderingUrl);
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
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, SuppressOpenURL) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender1");
  const GURL kSecondUrl = GetUrl("/empty.html?prerender2");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prerendering `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  int host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerendered_render_frame_host =
      GetPrerenderedMainFrameHost(host_id);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  auto* web_contents =
      WebContents::FromRenderFrameHost(prerendered_render_frame_host);
  OpenURLParams params(kSecondUrl, Referrer(),
                       prerendered_render_frame_host->GetFrameTreeNodeId(),
                       WindowOpenDisposition::NEW_WINDOW,
                       ui::PAGE_TRANSITION_LINK, true);
  params.initiator_origin =
      prerendered_render_frame_host->GetLastCommittedOrigin();
  params.source_render_process_id =
      prerendered_render_frame_host->GetProcess()->GetID();
  params.source_render_frame_id = prerendered_render_frame_host->GetRoutingID();
  auto* new_web_contents = web_contents->OpenURL(params);
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

  int host_id = AddPrerender(kPrerenderingUrl);
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

  int host_id = AddPrerender(kPrerenderingUrl);
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
  int host_id = AddPrerender(kPrerenderingUrl);
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
  int host_id = AddPrerender(kPrerenderingUrl);
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
    : public TestContentBrowserClient,
      mojom::TestInterfaceForDefer,
      mojom::TestInterfaceForGrant,
      mojom::TestInterfaceForCancel,
      mojom::TestInterfaceForUnexpected {
 public:
  void RegisterBrowserInterfaceBindersForFrame(
      RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<RenderFrameHost*>* map) override {
    map->Add<mojom::TestInterfaceForDefer>(base::BindRepeating(
        &MojoCapabilityControlTestContentBrowserClient::BindDeferInterface,
        base::Unretained(this)));
    map->Add<mojom::TestInterfaceForGrant>(base::BindRepeating(
        &MojoCapabilityControlTestContentBrowserClient::BindGrantInterface,
        base::Unretained(this)));
    map->Add<mojom::TestInterfaceForCancel>(base::BindRepeating(
        &MojoCapabilityControlTestContentBrowserClient::BindCancelInterface,
        base::Unretained(this)));
    map->Add<mojom::TestInterfaceForUnexpected>(base::BindRepeating(
        &MojoCapabilityControlTestContentBrowserClient::BindUnexpectedInterface,
        base::Unretained(this)));
  }

  void RegisterMojoBinderPoliciesForSameOriginPrerendering(
      MojoBinderPolicyMap& policy_map) override {
    policy_map.SetNonAssociatedPolicy<mojom::TestInterfaceForGrant>(
        MojoBinderNonAssociatedPolicy::kGrant);
    policy_map.SetNonAssociatedPolicy<mojom::TestInterfaceForCancel>(
        MojoBinderNonAssociatedPolicy::kCancel);
    policy_map.SetNonAssociatedPolicy<mojom::TestInterfaceForUnexpected>(
        MojoBinderNonAssociatedPolicy::kUnexpected);
  }

  void BindDeferInterface(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::TestInterfaceForDefer> receiver) {
    defer_receiver_set_.Add(this, std::move(receiver));
  }

  void BindGrantInterface(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::TestInterfaceForGrant> receiver) {
    grant_receiver_set_.Add(this, std::move(receiver));
  }

  void BindCancelInterface(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::TestInterfaceForCancel> receiver) {
    cancel_receiver_set_.Add(this, std::move(receiver));
  }

  void BindUnexpectedInterface(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::TestInterfaceForUnexpected> receiver) {
    unexpected_receiver_.Bind(std::move(receiver));
  }

  // mojom::TestInterfaceForDefer implementation.
  void Ping(PingCallback callback) override { std::move(callback).Run(); }

  size_t GetDeferReceiverSetSize() { return defer_receiver_set_.size(); }

  size_t GetGrantReceiverSetSize() { return grant_receiver_set_.size(); }

 private:
  mojo::ReceiverSet<mojom::TestInterfaceForDefer> defer_receiver_set_;
  mojo::ReceiverSet<mojom::TestInterfaceForGrant> grant_receiver_set_;
  mojo::ReceiverSet<mojom::TestInterfaceForCancel> cancel_receiver_set_;
  mojo::Receiver<mojom::TestInterfaceForUnexpected> unexpected_receiver_{this};
};

// Tests that binding requests are handled according to MojoBinderPolicyMap
// during prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, MojoCapabilityControl) {
  MojoCapabilityControlTestContentBrowserClient test_browser_client;
  auto* old_browser_client = SetBrowserClientForTesting(&test_browser_client);

  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_iframe.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start a prerender.
  int host_id = AddPrerender(kPrerenderingUrl);
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

  SetBrowserClientForTesting(old_browser_client);
}

// Tests that mojo capability control will cancel prerendering if the main frame
// receives a request for a kCancel interface.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MojoCapabilityControl_CancelMainFrame) {
  MojoCapabilityControlTestContentBrowserClient test_browser_client;
  auto* old_browser_client = SetBrowserClientForTesting(&test_browser_client);

  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_iframe.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start a prerender.
  int host_id = AddPrerender(kPrerenderingUrl);
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
  SetBrowserClientForTesting(old_browser_client);
}

// Tests that mojo capability control will cancel prerendering if child frames
// receive a request for a kCancel interface.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MojoCapabilityControl_CancelIframe) {
  MojoCapabilityControlTestContentBrowserClient test_browser_client;
  auto* old_browser_client = SetBrowserClientForTesting(&test_browser_client);

  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_iframe.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start a prerender.
  int host_id = AddPrerender(kPrerenderingUrl);
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

  SetBrowserClientForTesting(old_browser_client);
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
  auto* old_browser_client = SetBrowserClientForTesting(&test_browser_client);

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
  int host_id = AddPrerender(kPrerenderingUrl);
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

  SetBrowserClientForTesting(old_browser_client);
}

// Regression test for https://crbug.com/1268714.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, MojoCapabilityControl_LoosenMode) {
  MojoCapabilityControlTestContentBrowserClient test_browser_client;
  auto* old_browser_client = SetBrowserClientForTesting(&test_browser_client);
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  GURL initial_url = GetUrl("/empty.html");
  GURL prerendering_url =
      GetUrl("/cross_site_iframe_factory.html?a.test(a.test,a.test)");
  GURL cross_origin_iframe_url = GetCrossSiteUrl("/title1.html");

  // 1. Navigate to an initial page and prerender a page.
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  int host_id = AddPrerender(prerendering_url);
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
  ASSERT_EQ(all_prerender_frames.size(), 4u);
  ASSERT_EQ(count_speculative, 1u);

  // 5. Activate the prerendered page and listen to the DidFinishNavigation
  // event, to ensure the Activate IPC is sent.
  TestActivationManager prerendered_activation_navigation(web_contents(),
                                                          prerendering_url);
  ASSERT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                     JsReplace("location = $1", prerendering_url)));
  prerendered_activation_navigation.WaitForNavigationFinished();
  EXPECT_TRUE(prerendered_activation_navigation.was_activated());

  // 6. Renderers attempt to build Mojo connections for kCancel interfaces.
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
  SetBrowserClientForTesting(old_browser_client);
}

// Test that a PrerenderHost triggered by speculation rules is canceled when
// it times out in the background.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, CancelPrerenderWhenTimeout) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderUrl = GetUrl("/empty.html?prerender");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  int host_id = AddPrerender(kPrerenderUrl);
  test::PrerenderHostObserver prerender_observer(*web_contents_impl(), host_id);

  PrerenderHostRegistry* registry =
      web_contents_impl()->GetPrerenderHostRegistry();

  // The timers should not start yet when the prerendered page is in the
  // foreground.
  ASSERT_FALSE(registry->GetEmbedderTimerForTesting()->IsRunning());
  ASSERT_FALSE(registry->GetSpeculationRulesTimerForTesting()->IsRunning());

  // Inject mock time task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  registry->SetTaskRunnerForTesting(task_runner);

  // Changing the visibility state to HIDDEN will not stop prerendering
  // immediately, but start the timers.
  web_contents()->WasHidden();
  ASSERT_TRUE(registry->GetEmbedderTimerForTesting()->IsRunning());
  ASSERT_TRUE(registry->GetSpeculationRulesTimerForTesting()->IsRunning());

  task_runner->FastForwardBy(
      PrerenderHostRegistry::kTimeToLiveInBackgroundForSpeculationRules);

  prerender_observer.WaitForDestroyed();
  ASSERT_FALSE(registry->GetEmbedderTimerForTesting()->IsRunning());
  ASSERT_FALSE(registry->GetSpeculationRulesTimerForTesting()->IsRunning());
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kTimeoutBackgrounded, 1);
}

// Test that multiple PrerenderHosts triggered by speculation rules are canceled
// when it times out in the background.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       CancelMultiplePrerendersWhenTimeout) {
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

  // Changing the visibility state to HIDDEN will not stop prerendering
  // immediately, but start the timers.
  web_contents()->WasHidden();
  ASSERT_TRUE(registry->GetEmbedderTimerForTesting()->IsRunning());
  ASSERT_TRUE(registry->GetSpeculationRulesTimerForTesting()->IsRunning());

  task_runner->FastForwardBy(
      PrerenderHostRegistry::kTimeToLiveInBackgroundForSpeculationRules);

  prerender_observer.WaitForDestroyed();
  ASSERT_FALSE(registry->GetEmbedderTimerForTesting()->IsRunning());
  ASSERT_FALSE(registry->GetSpeculationRulesTimerForTesting()->IsRunning());
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kTimeoutBackgrounded, 2);
}

// Test that a PrerenderHost triggered by embedder is canceled when it times out
// in the background.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       CancelOnlyEmbedderTriggeredPrerenderWhenTimeout) {
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
      web_contents_impl()->StartPrerendering(
          kPrerenderUrl2, PrerenderTriggerType::kEmbedder,
          "EmbedderSuffixForTest",
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          nullptr);

  PrerenderHostRegistry* registry =
      web_contents_impl()->GetPrerenderHostRegistry();

  // The timers should not start yet when the prerendered page is in the
  // foreground.
  ASSERT_FALSE(registry->GetEmbedderTimerForTesting()->IsRunning());
  ASSERT_FALSE(registry->GetSpeculationRulesTimerForTesting()->IsRunning());

  // Inject mock time task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  registry->SetTaskRunnerForTesting(task_runner);

  // Changing the visibility state to HIDDEN will not stop prerendering
  // immediately, but start the timers.
  web_contents()->WasHidden();
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
  EXPECT_NE(GetHostForUrl(kPrerenderUrl1), RenderFrameHost::kNoFrameTreeNodeId);

  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kTimeoutBackgrounded, 1);
  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kTimeoutBackgrounded, 0);
}

// Test that the timers for PrerenderHost timeout is reset when the tab gets
// visible.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       TimerResetWhenHiddenPageGoBackToForeground) {
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

  // Changing the visibility state to HIDDEN will not stop prerendering
  // immediately, but start the timers.
  web_contents()->WasHidden();
  ASSERT_TRUE(registry->GetEmbedderTimerForTesting()->IsRunning());
  ASSERT_TRUE(registry->GetSpeculationRulesTimerForTesting()->IsRunning());

  // The timers should be reset when the hidden page goes back to the
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

// Test that a PrerenderHost in a triggered by speculation rules with
// "target=_blank" are canceled when it times out in the background.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       CancelPrerenderWithTargetBlankWhenTimeout) {
  const GURL kInitialUrl = GetUrl("/simple_links.html");
  const GURL kPrerenderUrl = GetUrl("/title2.html");

  // Navigate to an initial page which has a link to `kPrerenderUrl`.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderUrl`.
  TestNavigationObserver nav_observer(kPrerenderUrl);
  nav_observer.StartWatchingNewWebContents();
  AddPrerenderWithTargetHintAsync(kPrerenderUrl, "_blank");
  nav_observer.WaitForNavigationFinished();
  EXPECT_EQ(nav_observer.last_navigation_url(), kPrerenderUrl);

  PrerenderHost* prerender_host =
      web_contents_impl()->GetPrerenderHostRegistry()->FindHostByUrlForTesting(
          kPrerenderUrl);
  ASSERT_TRUE(prerender_host);
  auto* prerender_web_contents = WebContentsImpl::FromFrameTreeNode(
      prerender_host->GetPrerenderFrameTree().root());
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

  test::PrerenderHostObserver prerender_observer(*web_contents_impl(),
                                                 kPrerenderUrl);

  // Changing the visibility state to HIDDEN will not stop prerendering
  // immediately, but start the timers.
  web_contents()->WasHidden();
  ASSERT_TRUE(registry->GetEmbedderTimerForTesting()->IsRunning());
  ASSERT_TRUE(registry->GetSpeculationRulesTimerForTesting()->IsRunning());

  task_runner->FastForwardBy(
      PrerenderHostRegistry::kTimeToLiveInBackgroundForSpeculationRules);

  prerender_observer.WaitForDestroyed();
  ASSERT_FALSE(registry->GetEmbedderTimerForTesting()->IsRunning());
  ASSERT_FALSE(registry->GetSpeculationRulesTimerForTesting()->IsRunning());
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kTimeoutBackgrounded, 1);

  // The navigation occurred in a new WebContents, so the original WebContents
  // should still be showing the initial trigger page.
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);
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
  EXPECT_EQ(prerender_helper()->GetHostForUrl(kPrerenderingUrl),
            RenderFrameHost::kNoFrameTreeNodeId);
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
  test::PrerenderHostRegistryObserver registry_observer(*web_contents());

  // Start prerendering `kPrerenderingUrl`.
  const GURL kPrerenderingUrl = GetUrl("/title1.html");
  int host_id = prerender_helper()->AddPrerender(kPrerenderingUrl);
  test::PrerenderHostObserver host_observer(*web_contents(), host_id);

  // Reset the server's config.
  RequireClientCertsOrSendExpiredCerts();

  ASSERT_NE(prerender_helper()->GetHostForUrl(kPrerenderingUrl),
            RenderFrameHost::kNoFrameTreeNodeId);

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
  EXPECT_EQ(prerender_helper()->GetHostForUrl(kPrerenderingUrl),
            RenderFrameHost::kNoFrameTreeNodeId);
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
  EXPECT_EQ(prerender_helper()->GetHostForUrl(kPrerenderingUrl),
            RenderFrameHost::kNoFrameTreeNodeId);

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
  // TODO(https://crbug.com/1311887): Enable the test with kCertError.
  if (GetParam() == SSLPrerenderTestErrorBlockType::kCertError)
    return;

  // Load an initial page and register a service worker that intercepts
  // resources requests.
  const GURL kInitialUrl = GetUrl("/workers/service_worker_setup.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  EXPECT_EQ("ok", EvalJs(current_frame_host(), "setup();"));

  // Prerender a page.
  const GURL kPrerenderingUrl = GetUrl("/workers/empty.html");
  int host_id = prerender_helper()->AddPrerender(kPrerenderingUrl);
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
  EXPECT_EQ(prerender_helper()->GetHostForUrl(kPrerenderingUrl),
            RenderFrameHost::kNoFrameTreeNodeId);
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
  int host_id = AddPrerender(kPrerenderingUrl);
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

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, RenderFrameHostLifecycleState) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  EXPECT_EQ(current_frame_host()->lifecycle_state(),
            LifecycleStateImpl::kActive);

  // Start a prerender.
  int host_id = AddPrerender(kPrerenderingUrl);

  // Open an iframe in the prerendered page.
  RenderFrameHostImpl* rfh_a = GetPrerenderedMainFrameHost(host_id);
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
  NavigatePrimaryPage(kPrerenderingUrl);

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

  int host_id = GetHostForUrl(kPrerenderingUrl);
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
      "Navigation.Prerender.ActivationCommitDeferTime", 1u);
}

// Tests that prerendering is gated behind CSP:prefetch-src
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, CSPPrefetchSrc) {
  GURL initial_url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  const std::string kCSPScript = R"(
    const meta = document.createElement('meta');
    meta.httpEquiv = "Content-Security-Policy";
    meta.content = "prefetch-src https://a.test:*/title1.html";
    document.getElementsByTagName('head')[0].appendChild(meta);
  )";

  // Add CSP:prefetch-src */title1.html
  EXPECT_TRUE(ExecJs(current_frame_host(), kCSPScript));

  const char* kConsolePattern =
      "Refused to prefetch content from "
      "'https://a.test:*/*.html' because it violates the "
      "following Content Security Policy directive: \"prefetch-src "
      "https://a.test:*/title1.html\"*";

  // Check what happens when a prerendering is blocked:
  {
    GURL disallowed_url = GetUrl("/title2.html");
    WebContentsConsoleObserver console_observer(web_contents_impl());
    console_observer.SetPattern(kConsolePattern);

    // Prerender will fail. Then FindHostByUrlForTesting() should return null.
    test::PrerenderHostRegistryObserver observer(*web_contents_impl());
    AddPrerenderAsync(disallowed_url);
    observer.WaitForTrigger(disallowed_url);
    int host_id = GetHostForUrl(disallowed_url);
    test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);
    ASSERT_TRUE(console_observer.Wait());
    EXPECT_EQ(1u, console_observer.messages().size());
    EXPECT_EQ(GetRequestCount(disallowed_url), 0);
    host_observer.WaitForDestroyed();
    ExpectFinalStatusForSpeculationRule(
        PrerenderFinalStatus::kNavigationRequestBlockedByCsp);
  }

  EXPECT_TRUE(ExecJs(current_frame_host(), kCSPScript));

  // Check what happens when prerendering isn't blocked.
  {
    WebContentsConsoleObserver console_observer(web_contents_impl());
    console_observer.SetPattern(kConsolePattern);
    GURL kAllowedUrl = GetUrl("/title1.html");
    AddPrerender(kAllowedUrl);
    EXPECT_EQ(0u, console_observer.messages().size());
    EXPECT_EQ(GetRequestCount(kAllowedUrl), 1);
  }
}

// Tests that prerendering is gated behind CSP:default-src.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, CSPDefaultSrc) {
  GURL initial_url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  std::string kCSPScript = R"(
    const meta = document.createElement('meta');
    meta.httpEquiv = "Content-Security-Policy";
    meta.content =
        "default-src https://a.test:*/title1.html; script-src 'unsafe-inline'";
    document.getElementsByTagName('head')[0].appendChild(meta);
  )";

  // Add CSP:prefetch-src */title1.html
  EXPECT_TRUE(ExecJs(current_frame_host(), kCSPScript));

  const char* kConsolePattern =
      "Refused to prefetch content from "
      "'https://a.test:*/*.html' because it violates the "
      "following Content Security Policy directive: \"default-src "
      "https://a.test:*/title1.html\"*";

  // Check what happens when a prerendering is blocked:
  {
    GURL disallowed_url = GetUrl("/title2.html");
    WebContentsConsoleObserver console_observer(web_contents_impl());
    console_observer.SetPattern(kConsolePattern);
    test::PrerenderHostRegistryObserver observer(*web_contents_impl());
    test::PrerenderHostObserver host_observer(*web_contents_impl(),
                                              disallowed_url);
    AddPrerenderAsync(disallowed_url);
    ASSERT_TRUE(console_observer.Wait());
    EXPECT_EQ(1u, console_observer.messages().size());
    EXPECT_EQ(GetRequestCount(disallowed_url), 0);
    host_observer.WaitForDestroyed();
    ExpectFinalStatusForSpeculationRule(
        PrerenderFinalStatus::kNavigationRequestBlockedByCsp);
  }

  EXPECT_TRUE(ExecJs(current_frame_host(), kCSPScript));

  // Check what happens when prerendering isn't blocked.
  {
    WebContentsConsoleObserver console_observer(web_contents_impl());
    console_observer.SetPattern(kConsolePattern);
    GURL kAllowedUrl = GetUrl("/title1.html");
    AddPrerender(kAllowedUrl);
    EXPECT_EQ(0u, console_observer.messages().size());
    EXPECT_EQ(GetRequestCount(kAllowedUrl), 1);
  }
}

// TODO(https://crbug.com/1182032): Now the File System Access API is not
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
  int host_id = AddPrerender(kPrerenderingUrl);
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
  int host_id = AddPrerender(kPrerenderingUrl);
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
  int host_id = AddPrerender(kPrerenderingUrl);
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

// TODO(https://crbug.com/1201980) LaCrOS binds the HidManager interface, which
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
  int host_id = AddPrerender(kPrerenderingUrl);
  auto* prerender_render_frame_host = GetPrerenderedMainFrameHost(host_id);

  // Access the clipboard and fail.
  EXPECT_EQ(false,
            EvalJs(prerender_render_frame_host, "document.execCommand('copy');",
                   EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_EQ(false, EvalJs(prerender_render_frame_host,
                          "document.execCommand('paste');",
                          EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

void LoadAndWaitForPrerenderDestroyed(WebContents* const web_contents,
                                      const GURL prerendering_url,
                                      test::PrerenderTestHelper* helper) {
  test::PrerenderHostObserver host_observer(*web_contents, prerendering_url);
  helper->AddPrerenderAsync(prerendering_url);
  host_observer.WaitForDestroyed();
  EXPECT_EQ(helper->GetHostForUrl(prerendering_url),
            RenderFrameHost::kNoFrameTreeNodeId);
}

#if BUILDFLAG(ENABLE_PPAPI)
// Tests that we will cancel the prerendering if the prerendering page attempts
// to use plugins.
//
// TODO(crbug.com/1205920): This does not cover embedders that override
// `ContentRendererClient::OverrideCreatePlugin()` (such as for Chrome's PDF
// viewer), as cancellation depends on the renderer attempting to bind
// `content::mojom::PepperHost`.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PluginsCancelPrerendering) {
  const GURL kInitialUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  LoadAndWaitForPrerenderDestroyed(
      web_contents(), GetUrl("/prerender/page-with-embedded-plugin.html"),
      prerender_helper());
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kMojoBinderPolicy);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCancelledInterface.SpeculationRule",
      PrerenderCancelledInterface::kUnknown, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCancelledUnknownInterface."
      "SpeculationRule",
      InterfaceNameHasher(mojom::PepperHost::Name_), 1);

  LoadAndWaitForPrerenderDestroyed(
      web_contents(), GetUrl("/prerender/page-with-object-plugin.html"),
      prerender_helper());
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
  int host_id = AddPrerender(kPrerenderingUrl);
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

// TODO(crbug.com/1215073): Make a WPT when we have a stable way to wait
// cancellation runs.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, DownloadByScript) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerendering");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Make a prerendered page.
  int host_id = AddPrerender(kPrerenderingUrl);
  auto* prerender_host = GetPrerenderedMainFrameHost(host_id);
  test::PrerenderHostObserver host_observer(*web_contents(), host_id);

  const std::string js_string = R"(
      document.body.innerHTML =
          "<a id='target' download='download-link' href='cache.txt'>here</a>";
      document.getElementById('target').click();
  )";
  ExecuteScriptAsync(prerender_host, js_string);

  host_observer.WaitForDestroyed();
  EXPECT_EQ(prerender_helper()->GetHostForUrl(kPrerenderingUrl),
            RenderFrameHost::kNoFrameTreeNodeId);

  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kDownload);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, DownloadInMainFrame) {
  const GURL kInitialUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // TODO(crbug.com/1215073): Make a WPT for the content-disposition WPT test.
  const GURL kDownloadUrl =
      GetUrl("/set-header?Content-Disposition: attachment");

  LoadAndWaitForPrerenderDestroyed(web_contents(), kDownloadUrl,
                                   prerender_helper());

  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kDownload);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, DownloadInSubframe) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerendering");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Make a prerendered page.
  int host_id = AddPrerender(kPrerenderingUrl);
  auto* prerender_host = GetPrerenderedMainFrameHost(host_id);
  EXPECT_TRUE(AddTestUtilJS(prerender_host));

  // TODO(crbug.com/1215073): Make a WPT for the content-disposition WPT test.
  const GURL kDownloadUrl =
      GetUrl("/set-header?Content-Disposition: attachment");
  ExecuteScriptAsync(prerender_host,
                     JsReplace("add_iframe_async($1)", kDownloadUrl));

  test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  host_observer.WaitForDestroyed();
  EXPECT_EQ(GetHostForUrl(kPrerenderingUrl),
            RenderFrameHost::kNoFrameTreeNodeId);

  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kDownload);
}

// Tests that requesting audio output devices from prerendering documents result
// in cancellation of prerendering. Prerender2 decides to cancel prerendering
// here, because browser cannot defer this request as the renderer's main thread
// blocks while it waits for the response.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, RequestAudioOutputDevice) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/title1.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Make a prerendered page.
  int host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerender_rfh = GetPrerenderedMainFrameHost(host_id);
  test::PrerenderHostObserver host_observer(*web_contents(), host_id);

  // Create a output audio context which request audio output devices.
  // Prerendering should be cancelled in this case.
  // Whether using the EXECUTE_SCRIPT_NO_USER_GESTURE flag or not does not
  // affect the test result. The purpose of using it is to simulate real
  // scenarios since prerendering pages cannot have user gestures.
  std::ignore = ExecJs(prerender_rfh, "const context = new AudioContext();",
                       EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE);
  host_observer.WaitForDestroyed();
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kAudioOutputDeviceRequested);
}

// Tests that an activated page is allowed to request output devices.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       RequestAudioOutputDeviceAfterActivation) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/title1.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  int host_id = AddPrerender(kPrerenderingUrl);
  test::PrerenderHostObserver host_observer(*web_contents(), host_id);

  // After being activated, the document can play audio and it should work as a
  // normal document.
  prerender_helper()->NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_TRUE(host_observer.was_activated());
  std::string audio_script = R"(
      const context = new AudioContext();
      const osc = new OscillatorNode(context);
      osc.connect(context.destination);
      osc.start();
  )";
  EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(), audio_script));
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
  absl::optional<blink::mojom::ViewportFit> value_;
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
  int host_id = AddPrerender(kPrerenderingUrl);
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
  {
    // Cross-check that in case of low memory the eligibility reason points to
    // kLowMemory.
    ukm::SourceId ukm_source_id = PrimaryPageSourceId();
    auto attempt_ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName, test::kPreloadingAttemptUkmMetrics);

    UkmEntry attempt_expected_entry = attempt_ukm_entry_builder().BuildEntry(
        ukm_source_id, PreloadingType::kPrerender,
        PreloadingEligibility::kLowMemory,
        PreloadingHoldbackStatus::kUnspecified,
        PreloadingTriggeringOutcome::kUnspecified,
        PreloadingFailureReason::kUnspecified,
        /*accurate=*/true);

    EXPECT_EQ(attempt_ukm_entries[0], attempt_expected_entry)
        << test::ActualVsExpectedUkmEntryToString(attempt_ukm_entries[0],
                                                  attempt_expected_entry);
  }
}

class PrerenderSequentialPrerenderingBrowserTest : public PrerenderBrowserTest {
 public:
  PrerenderSequentialPrerenderingBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kPrerender2,
          {{"max_num_of_running_speculation_rules",
            base::NumberToString(MaxNumOfRunningPrerenders())},
           {"embedder_blocked_hosts", "a.test,b.test,c.test"}}},
         {blink::features::kPrerender2SequentialPrerendering, {}}},
        {});
  }

  int MaxNumOfRunningPrerenders() const { return 4; }

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
  AddMultiplePrerenderAsync(prerender_urls);

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
      PrerenderFinalStatus::kTriggerDestroyed, 2);
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
  AddMultiplePrerenderAsync({kPrerender1, kPrerender2, kPrerender3});

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

  // The first prerender is destroyed by SpeculationHostImpl.
  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kTriggerDestroyed, 1);

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
  AddMultiplePrerenderAsync({kPrerender1, kPrerender2, kPrerender3});

  // Stop the second prerendering initial navigation.
  response2.WaitForRequest();

  WaitForPrerenderLoadCompletion(kPrerender1);
  int host_id = GetHostForUrl(kPrerender1);
  ASSERT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);

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
  AddMultiplePrerenderAsync({kPrerender1, kPrerender2});

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

  {
    ukm::SourceId ukm_source_id = PrimaryPageSourceId();

    auto ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName, test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(ukm_entries.size(), 2u);

    std::vector<UkmEntry> expected_entries = {
        attempt_ukm_entry_builder().BuildEntry(
            ukm_source_id, PreloadingType::kPrerender,
            PreloadingEligibility::kEligible,
            PreloadingHoldbackStatus::kAllowed,
            PreloadingTriggeringOutcome::kRunning,
            PreloadingFailureReason::kUnspecified,
            /*accurate=*/false),
        attempt_ukm_entry_builder().BuildEntry(
            ukm_source_id, PreloadingType::kPrerender,
            PreloadingEligibility::kEligible,
            PreloadingHoldbackStatus::kAllowed,
            PreloadingTriggeringOutcome::kTriggeredButPending,
            PreloadingFailureReason::kUnspecified,
            /*accurate=*/true),
    };

    EXPECT_THAT(ukm_entries,
                testing::UnorderedElementsAreArray(expected_entries))
        << test::ActualVsExpectedUkmEntriesToString(ukm_entries,
                                                    expected_entries);
  }
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
  AddMultiplePrerenderAsync(prerender_urls);

  // Stop the first prerendering initial navigation.
  response.WaitForRequest();

  // Wait for the last prerender request will be triggered.
  registry_observer.WaitForTrigger(prerender_urls.back());

  // The last prerender is destroyed since the number of prerender requests
  // from speculation rules exceeds its limit of 4.
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded, 1);
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
  AddMultiplePrerenderAsync({kPrerender1, kPrerender2});

  // Stop the first prerender's initial navigation.
  prerender1_response.WaitForRequest();

  // Start prerendering by embedder triggered prerendering; this should start
  // immediately instead of being enqueued.
  std::unique_ptr<PrerenderHandle> prerender_handle =
      web_contents_impl()->StartPrerendering(
          kEmbedderPrerender, PrerenderTriggerType::kEmbedder,
          "EmbedderSuffixForTest",
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          nullptr);

  EXPECT_TRUE(prerender_handle);
  WaitForPrerenderLoadCompletion(kEmbedderPrerender);

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
  shell()->web_contents()->OpenURL(OpenURLParams(
      kEmbedderPrerender, Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      /*is_renderer_initiated=*/false));

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
      web_contents_impl()->StartPrerendering(
          kEmbedderPrerender, PrerenderTriggerType::kEmbedder,
          "EmbedderSuffixForTest",
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          nullptr);

  EXPECT_FALSE(prerender_handle);
  EXPECT_EQ(GetHostForUrl(kEmbedderPrerender),
            RenderFrameHost::kNoFrameTreeNodeId);
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
  AddMultiplePrerenderAsync({kPrerender1, kPrerender2});

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
      PrerenderFinalStatus::kTriggerDestroyed, 1);
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
  AddMultiplePrerenderAsync({kPrerender1, kPrerender2});
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
  {
    ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();

    auto ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName, test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(ukm_entries.size(), 2u);

    std::vector<UkmEntry> expected_entries = {
        attempt_ukm_entry_builder().BuildEntry(
            ukm_source_id, PreloadingType::kPrerender,
            PreloadingEligibility::kEligible,
            PreloadingHoldbackStatus::kAllowed,
            PreloadingTriggeringOutcome::kSuccess,
            PreloadingFailureReason::kUnspecified,
            /*accurate=*/true,
            /*ready_time=*/kMockElapsedTime),
        attempt_ukm_entry_builder().BuildEntry(
            ukm_source_id, PreloadingType::kPrerender,
            PreloadingEligibility::kEligible,
            PreloadingHoldbackStatus::kAllowed,
            PreloadingTriggeringOutcome::kTriggeredButPending,
            PreloadingFailureReason::kUnspecified,
            /*accurate=*/false),
    };

    EXPECT_THAT(ukm_entries,
                testing::UnorderedElementsAreArray(expected_entries))
        << test::ActualVsExpectedUkmEntriesToString(ukm_entries,
                                                    expected_entries);
  }
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
  AddMultiplePrerenderAsync({kPrerender1, kPrerender2, kPrerender3});
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

  {
    ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();

    auto ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName, test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(ukm_entries.size(), 3u);

    std::vector<UkmEntry> expected_entries = {
        attempt_ukm_entry_builder().BuildEntry(
            ukm_source_id, PreloadingType::kPrerender,
            PreloadingEligibility::kEligible,
            PreloadingHoldbackStatus::kAllowed,
            PreloadingTriggeringOutcome::kSuccess,
            PreloadingFailureReason::kUnspecified,
            /*accurate=*/true,
            /*ready_time=*/kMockElapsedTime),
        attempt_ukm_entry_builder().BuildEntry(
            ukm_source_id, PreloadingType::kPrerender,
            PreloadingEligibility::kEligible,
            PreloadingHoldbackStatus::kAllowed,
            PreloadingTriggeringOutcome::kRunning,
            PreloadingFailureReason::kUnspecified,
            /*accurate=*/false),
        attempt_ukm_entry_builder().BuildEntry(
            ukm_source_id, PreloadingType::kPrerender,
            PreloadingEligibility::kEligible,
            PreloadingHoldbackStatus::kAllowed,
            PreloadingTriggeringOutcome::kTriggeredButPending,
            PreloadingFailureReason::kUnspecified,
            /*accurate=*/false),
    };

    EXPECT_THAT(ukm_entries,
                testing::UnorderedElementsAreArray(expected_entries))
        << test::ActualVsExpectedUkmEntriesToString(ukm_entries,
                                                    expected_entries);
  }
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
  AddMultiplePrerenderAsync(prerender_urls);
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
    EXPECT_EQ(GetHostForUrl(url), RenderFrameHost::kNoFrameTreeNodeId);
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
  {
    ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();

    auto ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName, test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(ukm_entries.size(), 4u);

    std::vector<UkmEntry> expected_entries = {
        attempt_ukm_entry_builder().BuildEntry(
            ukm_source_id, PreloadingType::kPrerender,
            PreloadingEligibility::kEligible,
            PreloadingHoldbackStatus::kAllowed,
            PreloadingTriggeringOutcome::kSuccess,
            PreloadingFailureReason::kUnspecified,
            /*accurate=*/true,
            /*ready_time=*/kMockElapsedTime),
        attempt_ukm_entry_builder().BuildEntry(
            ukm_source_id, PreloadingType::kPrerender,
            PreloadingEligibility::kEligible,
            PreloadingHoldbackStatus::kAllowed,
            PreloadingTriggeringOutcome::kReady,
            PreloadingFailureReason::kUnspecified,
            /*accurate=*/false,
            /*ready_time=*/kMockElapsedTime),
        attempt_ukm_entry_builder().BuildEntry(
            ukm_source_id, PreloadingType::kPrerender,
            PreloadingEligibility::kEligible,
            PreloadingHoldbackStatus::kAllowed,
            PreloadingTriggeringOutcome::kRunning,
            PreloadingFailureReason::kUnspecified,
            /*accurate=*/false),
        attempt_ukm_entry_builder().BuildEntry(
            ukm_source_id, PreloadingType::kPrerender,
            PreloadingEligibility::kEligible,
            PreloadingHoldbackStatus::kAllowed,
            PreloadingTriggeringOutcome::kTriggeredButPending,
            PreloadingFailureReason::kUnspecified,
            /*accurate=*/false),
    };

    EXPECT_THAT(ukm_entries,
                testing::UnorderedElementsAreArray(expected_entries))
        << test::ActualVsExpectedUkmEntriesToString(ukm_entries,
                                                    expected_entries);
  }
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
  TestNavigationObserver nav_observer(prerendering_urls[2]);
  nav_observer.StartWatchingNewWebContents();
  for (const GURL& prerendering_url : prerendering_urls) {
    AddPrerenderWithTargetHintAsync(prerendering_url, "_blank");
  }
  nav_observer.WaitForNavigationFinished();
  EXPECT_EQ(nav_observer.last_navigation_url(), prerendering_urls[2]);

  // Make sure that prerendering in a new tab creates new PrerenderHost and
  // new WebContentsImpl every time.
  std::vector<PrerenderHost*> prerender_hosts;
  std::vector<WebContentsImpl*> prerender_web_contents_impls;
  for (const GURL& prerendering_url : prerendering_urls) {
    PrerenderHost* prerender_host =
        web_contents_impl()
            ->GetPrerenderHostRegistry()
            ->FindHostByUrlForTesting(prerendering_url);
    ASSERT_TRUE(prerender_host);
    EXPECT_FALSE(base::Contains(prerender_hosts, prerender_host));
    prerender_hosts.push_back(prerender_host);

    auto* prerender_web_contents_impl = WebContentsImpl::FromFrameTreeNode(
        prerender_host->GetPrerenderFrameTree().root());
    ASSERT_TRUE(prerender_web_contents_impl);
    EXPECT_NE(prerender_web_contents_impl, web_contents_impl());
    ExpectWebContentsIsForNewTabPrerendering(*prerender_web_contents_impl);

    // Prerendering in a new tab should create a new WebContentsImpl, not reuse
    // existing WebContentsImpl.
    EXPECT_FALSE(base::Contains(prerender_web_contents_impls,
                                prerender_web_contents_impl));
    prerender_web_contents_impls.push_back(prerender_web_contents_impl);
  }
  ASSERT_EQ(prerender_hosts.size(), prerendering_urls.size());
  ASSERT_EQ(prerender_web_contents_impls.size(), prerendering_urls.size());

  // Click the link to prerendering_urls[0]. This should activate
  // prerender_hosts[0].
  test::PrerenderHostObserver prerender_observer(
      *prerender_web_contents_impls[0],
      prerender_hosts[0]->frame_tree_node_id());
  const std::string kLinkClickScript = R"(
      clickSameSiteNewWindowLink();
  )";
  EXPECT_TRUE(ExecJs(web_contents(), kLinkClickScript));
  prerender_observer.WaitForActivation();
  EXPECT_EQ(prerender_web_contents_impls[0]->GetLastCommittedURL(),
            prerendering_urls[0]);
  EXPECT_TRUE(prerender_observer.was_activated());

  // prerender_hosts[0] was consumed for activation, but others were not.
  EXPECT_FALSE(HasHostForUrl(prerendering_urls[0]));
  EXPECT_TRUE(HasHostForUrl(prerendering_urls[1]));
  EXPECT_TRUE(HasHostForUrl(prerendering_urls[2]));

  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kActivated);

  // The navigation occurred in a new WebContents, so the original WebContents
  // should still be showing the initial trigger page.
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), initial_url);
}

// TODO(crbug.com/1356907): Remove this and merge it to
// PrerenderSequentialPrerenderingBrowserTest once kPrerender2InBackground is
// enabled by default.
class SequentialPrerenderInBackgroundBrowserTest
    : public PrerenderSequentialPrerenderingBrowserTest {
 public:
  SequentialPrerenderInBackgroundBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {// Enable to run prerenderings in the background.
         blink::features::kPrerender2InBackground},
        // Disable the memory requirement of Prerender2 so the test can run on
        // any bot.
        {blink::features::kPrerender2MemoryControls});
  }

  ~SequentialPrerenderInBackgroundBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that when the current tab gets hidden then the prerender sequence is
// terminated, and when the current tab gets visible then we start the next
// prerender if we have some pending prerender hosts.
IN_PROC_BROWSER_TEST_F(SequentialPrerenderInBackgroundBrowserTest,
                       SequentialPrerenderingInBackground) {
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
  AddMultiplePrerenderAsync({kPrerender1, kPrerender2});
  registry_observer.WaitForTrigger(kPrerender2);

  test::PrerenderHostObserver prerender2_observer(*web_contents(),
                                                  GetHostForUrl(kPrerender2));

  // Stop the first prerendering initial navigation.
  response1.WaitForRequest();

  // Change the visibility status to HIDDEN.
  web_contents()->WasHidden();

  // Complete the first prerender response and finish its initial navigation.
  // This shouldn't start the pending prerender.
  response1.Send(net::HTTP_OK, "");
  response1.Done();
  WaitForPrerenderLoadCompletion(kPrerender1);

  // Check the next prerender host is still pending.
  PrerenderHost* prerender2_host =
      web_contents_impl()->GetPrerenderHostRegistry()->FindHostByUrlForTesting(
          kPrerender2);
  auto* preloading_attempt_impl = static_cast<PreloadingAttemptImpl*>(
      prerender2_host->preloading_attempt().get());
  EXPECT_EQ(test::PreloadingAttemptAccessor(preloading_attempt_impl)
                .GetTriggeringOutcome(),
            PreloadingTriggeringOutcome::kTriggeredButPending);

  // The hidden page gets back to the foreground. The next pending prerender
  // should start.
  web_contents()->WasShown();
  WaitForPrerenderLoadCompletion(kPrerender2);

  // Activate the second prerender.
  NavigatePrimaryPage(kPrerender2);
  prerender2_observer.WaitForActivation();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerender2);
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
  int host_id = AddPrerender(kPrerenderingUrl);
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
  int host_id = AddPrerender(kPrerenderingUrl);
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
  ASSERT_NE(AddPrerender(kPrerenderingUrl),
            RenderFrameHost::kNoFrameTreeNodeId);

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
    ASSERT_NE(AddPrerender(kPrerenderingUrlWithTitle),
              RenderFrameHost::kNoFrameTreeNodeId);
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
    ASSERT_NE(AddPrerender(kPrerenderingUrlWithoutTitle),
              RenderFrameHost::kNoFrameTreeNodeId);
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
      web_contents_impl()->StartPrerendering(
          kPrerenderUrl, PrerenderTriggerType::kEmbedder,
          "EmbedderSuffixForTest",
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          nullptr);

  // Both the creation of PrerenderHandle and PrerenderHost should fail.
  EXPECT_FALSE(prerender_handle);
  EXPECT_EQ(GetHostForUrl(kPrerenderUrl), RenderFrameHost::kNoFrameTreeNodeId);
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
  int host_id = AddPrerender(kPrerenderingUrl);
  auto* prerendered_render_frame_host = GetPrerenderedMainFrameHost(host_id);
  auto* child_frame = ChildFrameAt(prerendered_render_frame_host, 0);
  ASSERT_TRUE(child_frame);

  // Navigate the iframe's FrameTreeNode in the prerendering frame tree. This
  // should successfully navigate.
  TestNavigationManager iframe_observer(shell()->web_contents(), kNewIframeUrl);
  shell()->web_contents()->OpenURL(OpenURLParams(
      kNewIframeUrl, Referrer(), child_frame->GetFrameTreeNodeId(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_AUTO_SUBFRAME,
      /*is_renderer_initiated=*/false));
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
  int prerender_host_id = AddPrerender(kPrerenderingUrl);
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
  int host_id = AddPrerender(kPrerenderingUrl);
  auto* prerendered_render_frame_host = GetPrerenderedMainFrameHost(host_id);
  auto* child_frame = ChildFrameAt(prerendered_render_frame_host, 0);
  ASSERT_TRUE(child_frame);

  TestNavigationManager iframe_observer(shell()->web_contents(), kNewIframeUrl);

  // Navigate the iframe's FrameTreeNode in the prerendering frame tree. This
  // should successfully navigate but the navigation will be deferred until the
  // prerendering page is activated.
  {
    shell()->web_contents()->OpenURL(OpenURLParams(
        kNewIframeUrl, Referrer(), child_frame->GetFrameTreeNodeId(),
        WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_AUTO_SUBFRAME,
        /*is_renderer_initiated=*/false));
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

// Test starting a main frame navigation after the initial
// prerender navigation when activation has already started.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MainFrameNavigationDuringActivation) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?1");
  const GURL kPrerenderingUrl2 = GetUrl("/empty.html?2");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  int prerender_host_id = AddPrerender(kPrerenderingUrl);
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

  // Make a navigation in the prerendered page. This navigation should
  // be cancelled by PrerenderNavigationThrottle.
  TestNavigationManager bad_nav_observer(web_contents(), kPrerenderingUrl2);
  NavigatePrerenderedPage(prerender_host_id, kPrerenderingUrl2);
  ASSERT_TRUE(bad_nav_observer.WaitForNavigationFinished());
  EXPECT_FALSE(bad_nav_observer.was_successful());

  // PrerenderNavigationThrottle also cancels the activation and then starts
  // regular navigation.
  activation_observer.ResumeActivation();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // The prerender host should have been abandoned.
  EXPECT_FALSE(
      web_contents_impl()->GetPrerenderHostRegistry()->FindNonReservedHostById(
          prerender_host_id));
  EXPECT_FALSE(
      web_contents_impl()->GetPrerenderHostRegistry()->FindReservedHostById(
          prerender_host_id));
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kMainFrameNavigation);

  // Wait for completion of the navigation. This shouldn't be the prerendered
  // page activation.
  activation_observer.WaitForNavigationFinished();
  EXPECT_FALSE(activation_observer.was_activated());
  EXPECT_TRUE(activation_observer.was_successful());
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), kPrerenderingUrl);
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
  int prerender_host_id = AddPrerender(kPrerenderingUrl);
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
  int prerender_host_id = AddPrerender(kPrerenderingUrl);
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
  int prerender_host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerender_main_frame_host =
      GetPrerenderedMainFrameHost(prerender_host_id);
  RenderFrameHost* child_frame = ChildFrameAt(prerender_main_frame_host, 0);
  EXPECT_EQ(prerender_main_frame_host->child_count(), 1u);
  ASSERT_NE(prerender_host_id, RenderFrameHost::kNoFrameTreeNodeId);

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
  int prerender_host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerender_frame_host =
      GetPrerenderedMainFrameHost(prerender_host_id);
  EXPECT_EQ(prerender_frame_host->child_count(), 1u);
  ASSERT_NE(prerender_host_id, RenderFrameHost::kNoFrameTreeNodeId);

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
  int prerender_host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerender_frame_host =
      GetPrerenderedMainFrameHost(prerender_host_id);
  EXPECT_EQ(prerender_frame_host->child_count(), 1u);
  ASSERT_NE(prerender_host_id, RenderFrameHost::kNoFrameTreeNodeId);

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
  int prerender_host_id = AddPrerender(kPrerenderingUrl);
  ASSERT_NE(prerender_host_id, RenderFrameHost::kNoFrameTreeNodeId);
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
  int prerender_host_id = AddPrerender(kPrerenderingUrl);
  ASSERT_NE(prerender_host_id, RenderFrameHost::kNoFrameTreeNodeId);

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
  int prerender_host_id = RenderFrameHost::kNoFrameTreeNodeId;
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
  int prerender_host_id = RenderFrameHost::kNoFrameTreeNodeId;
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
    shell()->web_contents()->OpenURL(OpenURLParams(
        kNewIframeUrl, Referrer(), child_frame->GetFrameTreeNodeId(),
        WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_AUTO_SUBFRAME,
        /*is_renderer_initiated=*/false));
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

// Test if the host is abandoned when the renderer page crashes.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, AbandonIfRendererProcessCrashes) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  int host_id = AddPrerender(kPrerenderingUrl);

  // Crash the relevant renderer.
  {
    test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);
    RenderProcessHost* process =
        GetPrerenderedMainFrameHost(host_id)->GetProcess();
    ScopedAllowRendererCrashes allow_renderer_crashes(process);
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_X86_FAMILY)
    // On x86 and x86_64 Android, base::ImmediateCrash() macro used in
    // ChildProcessHostImpl::CrashHungProcess() called from ForceCrash()
    // does not seem to work as expected. (See https://crbug.com/1211655)
    // We have no other ForceCrash() call sites on other than Linux and CrOS.
    // In this test, we call Shutdown(content::RESULT_CODE_HUNG) instead as
    // HungRenderDialogView does so on other platforms than Linux and CrOS.
    process->Shutdown(RESULT_CODE_HUNG);
#else
    // On Android, ForceCrash results in TERMINATION_STATUS_NORMAL_TERMINATION.
    // On other platforms, it does in TERMINATION_STATUS_PROCESS_CRASHED.
    process->ForceCrash();
#endif
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
  int host_id = AddPrerender(kPrerenderingUrl);

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
  int host_id = AddPrerender(kPrerenderingUrl);

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
        {{features::kBackForwardCache, {}},
         {kBackForwardCacheNoTimeEviction, {}}},
        // Allow BackForwardCache for all devices regardless of their memory.
        {features::kBackForwardCacheMemoryControls});
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

  int host_id = AddPrerender(kPrerenderingUrl);

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

class PrerenderWithProactiveBrowsingInstanceSwap : public PrerenderBrowserTest {
 public:
  PrerenderWithProactiveBrowsingInstanceSwap() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{features::kProactivelySwapBrowsingInstance,
                               {{"level", "SameSite"}}}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class PrerenderSameSiteCrossOriginBrowserTest : public PrerenderBrowserTest {
 public:
  PrerenderSameSiteCrossOriginBrowserTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kSameSiteCrossOriginForSpeculationRulesPrerender);
  }

  void SetUp() override {
    ssl_server().RegisterRequestHandler(
        base::BindRepeating(&net::test_server::HandlePrefixedRequest,
                            "/server-redirect-credentialed-prerender",
                            base::BindRepeating(HandleCredentialedRequest)));
    PrerenderBrowserTest::SetUp();
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

  GURL GetUrl(const std::string& path) {
    return ssl_server().GetURL("a.a.test", path);
  }

  GURL GetCrossSiteUrl(const std::string& path) {
    return ssl_server().GetURL("a.b.test", path);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Make sure that we can deal with the speculative RFH that is created during
// the activation navigation.
// TODO(https://crbug.com/1190197): We should try to avoid creating the
// speculative RFH (redirects allowing). Once that is done we should either
// change this test (if redirects allowed) or remove it completely.
IN_PROC_BROWSER_TEST_F(PrerenderWithProactiveBrowsingInstanceSwap,
                       SpeculationRulesScript) {
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

class PrerenderWithBackForwardCacheBrowserTest
    : public PrerenderBrowserTest,
      public testing::WithParamInterface<BackForwardCacheType> {
 public:
  PrerenderWithBackForwardCacheBrowserTest() {
    // Allow the BFCache for all devices regardless of their memory.
    std::vector<base::test::FeatureRef> disabled_features{
        features::kBackForwardCacheMemoryControls};

    switch (GetParam()) {
      case BackForwardCacheType::kDisabled:
        feature_list_.InitAndDisableFeature(features::kBackForwardCache);
        break;
      case BackForwardCacheType::kEnabled:
        feature_list_.InitWithFeaturesAndParameters(
            {{features::kBackForwardCache, {{}}},
             {features::kBackForwardCacheTimeToLiveControl,
              {{"time_to_live_seconds", "3600"}}}},
            disabled_features);
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
  RenderFrameHostImpl* initial_frame_host = current_frame_host();

  // Make a prerendered page from the initial page.
  int host_id = AddPrerender(kPrerenderingUrl);
  test::PrerenderHostObserver prerender_observer(*web_contents_impl(), host_id);

  // Navigate the initial page to a non-prerendered page.
  ASSERT_TRUE(NavigateToURL(shell(), kNextUrl));

  // Check if the initial page is in the back/forward cache.
  switch (GetParam()) {
    case BackForwardCacheType::kDisabled:
      // The BFCache is disabled, so the initial page is not in the
      // back/forward cache.
      ASSERT_FALSE(initial_frame_host->IsInBackForwardCache());
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
  RenderFrameHostImpl* next_frame_host = current_frame_host();

  // Make a prerendered page from the next page.
  int host_id = AddPrerender(kPrerenderingUrl);
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
      ASSERT_FALSE(next_frame_host->IsInBackForwardCache());
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
      web_contents_impl()->StartPrerendering(
          kFirstPrerenderingUrl, PrerenderTriggerType::kEmbedder,
          "EmbedderSuffixForTest",
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          nullptr);
  EXPECT_TRUE(prerender_handle1);

  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded, 0);

  // Start prerendering by embedder triggered prerendering; this should be
  // trigger successfully.
  std::unique_ptr<PrerenderHandle> prerender_handle2 =
      web_contents_impl()->StartPrerendering(
          kSecondPrerenderingUrl, PrerenderTriggerType::kEmbedder,
          "EmbedderSuffixForTest",
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          nullptr);
  EXPECT_TRUE(prerender_handle2);

  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded, 0);

  // Start prerendering by embedder triggered prerendering; this should hit the
  // limit.
  std::unique_ptr<PrerenderHandle> prerender_handle3 =
      web_contents_impl()->StartPrerendering(
          kThirdPrerenderingUrl, PrerenderTriggerType::kEmbedder,
          "EmbedderSuffixForTest",
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          nullptr);
  EXPECT_FALSE(prerender_handle3);

  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded, 1);
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
      PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded, 0);

  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded, 0);

  // Start the first embedder triggered prerendering; this should be triggered
  // successfully.
  std::unique_ptr<PrerenderHandle> prerender_handle1 =
      web_contents_impl()->StartPrerendering(
          kEmbedderPrerenderingUrl1, PrerenderTriggerType::kEmbedder,
          "EmbedderSuffixForTest",
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          nullptr);
  EXPECT_TRUE(prerender_handle1);

  // Start the second embedder triggered prerendering; this should be triggered
  // successfully.
  std::unique_ptr<PrerenderHandle> prerender_handle2 =
      web_contents_impl()->StartPrerendering(
          kEmbedderPrerenderingUrl2, PrerenderTriggerType::kEmbedder,
          "EmbedderSuffixForTest",
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          nullptr);
  EXPECT_TRUE(prerender_handle2);

  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded, 0);

  // Start the third embedder triggered prerendering; this should hit the limit.
  std::unique_ptr<PrerenderHandle> prerender_handle3 =
      web_contents_impl()->StartPrerendering(
          kEmbedderPrerenderingUrl3, PrerenderTriggerType::kEmbedder,
          "EmbedderSuffixForTest",
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          nullptr);
  EXPECT_FALSE(prerender_handle3);

  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded, 1);

  // Cancel the second embedder triggered prerendering and start a new one;
  // this should succeed as one of the prerenders is freed.
  prerender_handle2.reset();
  prerender_handle3 = web_contents_impl()->StartPrerendering(
      kEmbedderPrerenderingUrl3, PrerenderTriggerType::kEmbedder,
      "EmbedderSuffixForTest",
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      nullptr);
  EXPECT_TRUE(prerender_handle3);
}

class MultiplePrerendersBrowserTest : public PrerenderBrowserTest {
 public:
  MultiplePrerendersBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kPrerender2,
          {{"max_num_of_running_speculation_rules",
            base::NumberToString(MaxNumOfRunningPrerenders())}}},
         {blink::features::kPrerender2MemoryControls,
          // A value 100 allows prerenderings regardless of the current memory
          // usage.
          {{"acceptable_percent_of_system_memory", "100"},
           // Allow prerendering on low-end trybot devices so that prerendering
           // can run on any bots.
           {"memory_threshold_in_mb", "0"}}}},
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
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kPrerender2,
          {{"max_num_of_running_speculation_rules",
            base::NumberToString(MaxNumOfRunningPrerenders())}}},
         {blink::features::kPrerender2MemoryControls,
          // A value 0 doesn't allow any prerendering.
          {{"acceptable_percent_of_system_memory", "0"},
           // Allow prerendering on low-end trybot devices so that prerendering
           // can run on any bots.
           {"memory_threshold_in_mb", "0"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

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
      PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded);

  const GURL kEmbedderTriggeredPrerenderingUrl =
      GetUrl("/empty.html?embedder-triggered-prerender");
  // Start an embedder triggered prerendering; this should be triggered
  // successfully because its limitation is independent from speculation rules.
  std::unique_ptr<PrerenderHandle> prerender_handle =
      web_contents_impl()->StartPrerendering(
          kEmbedderTriggeredPrerenderingUrl, PrerenderTriggerType::kEmbedder,
          "EmbedderSuffixForTest",
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          nullptr);
  EXPECT_TRUE(prerender_handle);

  // Navigate to `kInitialUrl` on the primary page. This should make
  // SpeculationHostImpl destructed.
  NavigatePrimaryPage(kInitialUrl);
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // The metric is recorded after SpeculationHostImpl is destructed or a primary
  // page is updated.
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.CancellationPercentageByExcessiveMemoryUsage."
      "SpeculationRule",
      0, 1);
}

// Tests that PrerenderHostRegistry can't start any prerenderings if the
// acceptable percent of the system memory is set to 0.
IN_PROC_BROWSER_TEST_F(MultiplePrerendersWithLimitedMemoryBrowserTest,
                       AddSpeculationRulesMultipleTimes) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  for (int i = 0; i < MaxNumOfRunningPrerenders(); i++) {
    const GURL kPrerenderingUrl =
        GetUrl("/empty.html?prerender" + base::NumberToString(i));
    test::PrerenderHostObserver host_observer(*web_contents(),
                                              kPrerenderingUrl);

    // Add a prerender speculation rule; it should be destroyed due to the
    // limited memory resource.
    AddPrerenderAsync(kPrerenderingUrl);
    host_observer.WaitForDestroyed();
  }

  int count_of_memory_limit_exceeded = histogram_tester().GetBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kMemoryLimitExceeded);
  // For an unknown reason, requesting the private memory footprint can fail.
  // This test allows the failure of getting the memory dump.
  int count_of_no_memory_dump = histogram_tester().GetBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kFailToGetMemoryUsage);
  EXPECT_EQ(MaxNumOfRunningPrerenders(),
            count_of_memory_limit_exceeded + count_of_no_memory_dump);

  const GURL kEmbedderTriggeredPrerenderingUrl =
      GetUrl("/empty.html?embedder-triggered-prerender");
  // Start an embedder triggered prerendering; this should be triggered
  // successfully because its limitation is independent from speculation rules.
  std::unique_ptr<PrerenderHandle> prerender_handle =
      web_contents_impl()->StartPrerendering(
          kEmbedderTriggeredPrerenderingUrl, PrerenderTriggerType::kEmbedder,
          "EmbedderSuffixForTest",
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          nullptr);
  EXPECT_TRUE(prerender_handle);

  // Navigate to `kInitialUrl` on the primary page. This should make
  // SpeculationHostImpl destructed.
  NavigatePrimaryPage(kInitialUrl);
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // The metric is recorded after SpeculationHostImpl is destructed or a primary
  // page is updated.
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.CancellationPercentageByExcessiveMemoryUsage."
      "SpeculationRule",
      count_of_memory_limit_exceeded * 100 / MaxNumOfRunningPrerenders(), 1);
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
      PrerenderFinalStatus::kCrossSiteNavigation);

  ASSERT_TRUE(NavigateToURL(shell(), kPrerenderingUrl));
  {
    // Cross-check that in case of cross-origin navigation the eligibility
    // reason points to kCrossOrigin.
    ukm::SourceId ukm_source_id = PrimaryPageSourceId();
    auto attempt_ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName, test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(attempt_ukm_entries.size(), 1u);

    UkmEntry attempt_expected_entry = attempt_ukm_entry_builder().BuildEntry(
        ukm_source_id, PreloadingType::kPrerender,
        PreloadingEligibility::kCrossOrigin,
        PreloadingHoldbackStatus::kUnspecified,
        PreloadingTriggeringOutcome::kUnspecified,
        PreloadingFailureReason::kUnspecified,
        /*accurate=*/true);

    EXPECT_EQ(attempt_ukm_entries[0], attempt_expected_entry)
        << test::ActualVsExpectedUkmEntryToString(attempt_ukm_entries[0],
                                                  attempt_expected_entry);
  }
}

// Tests that same-site cross-origin navigation by speculation rules is not
// allowed with the feature enabled but without opt-in.
IN_PROC_BROWSER_TEST_F(
    PrerenderSameSiteCrossOriginBrowserTest,
    SameSiteCrossOriginNavigationSpeculationRulesWithoutOptInHeader) {
  ASSERT_TRUE(blink::features::
                  IsSameSiteCrossOriginForSpeculationRulesPrerender2Enabled());
  const GURL kInitialUrl = GetUrlForSameSiteCrossOriginTest("/empty.html");
  const GURL kPrerenderingUrl =
      GetSameSiteCrossOriginUrl("/empty.html?samesitecrossorigin");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Add a same-site cross-origin prerender rule.
  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());
  AddPrerenderAsync(kPrerenderingUrl);
  // Wait for PrerenderHostRegistry to receive the cross-origin prerender
  // request, but it will be ignored because the flag
  // SameSiteCrossOriginForSpeculationRulesPrerender2 is enabled without an
  // opt-in header.
  registry_observer.WaitForTrigger(kPrerenderingUrl);
  // Navigate to the prerendered page and this should trigger cancellation
  // because of a lack of the opt in header.
  NavigatePrimaryPage(kPrerenderingUrl);

  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kSameSiteCrossOriginNavigationNotOptIn);
}

// Tests that same-site cross-origin redirection by speculation rules with the
// feature enabled but without opt-in.
IN_PROC_BROWSER_TEST_F(
    PrerenderSameSiteCrossOriginBrowserTest,
    SameSiteCrossOriginRedirectionSpeculationRulesWithoutOptInHeader) {
  ASSERT_TRUE(blink::features::
                  IsSameSiteCrossOriginForSpeculationRulesPrerender2Enabled());
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrlForSameSiteCrossOriginTest("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering a URL that causes cross-origin redirection. The
  // cross-origin redirection should fail prerendering without an opt-in header.
  const GURL kRedirectedUrl =
      GetSameSiteCrossOriginUrl("/empty.html?samesitecrossorigin");
  const GURL kPrerenderingUrl = GetUrlForSameSiteCrossOriginTest(
      "/server-redirect?" + kRedirectedUrl.spec());
  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());
  test::PrerenderHostObserver host_observer(*web_contents_impl(),
                                            kPrerenderingUrl);
  AddPrerenderAsync(kPrerenderingUrl);
  host_observer.WaitForDestroyed();
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl), 1);
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  EXPECT_FALSE(HasHostForUrl(kRedirectedUrl));
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kSameSiteCrossOriginRedirectNotOptIn);
}

// Tests that same-site cross-origin redirection with credentialed prerender by
// speculation rules with the feature enabled but the redirected page without
// opt-in. This test verifies a case which is a.a.test -> a.a.test (credentialed
// prerender) -> b.a.test (no credentialed prerender).
IN_PROC_BROWSER_TEST_F(
    PrerenderSameSiteCrossOriginBrowserTest,
    SameSiteCrossOriginCredentialedPrerenderRedirectionSpeculationRulesWithoutOptInHeader) {
  ASSERT_TRUE(blink::features::
                  IsSameSiteCrossOriginForSpeculationRulesPrerender2Enabled());
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrlForSameSiteCrossOriginTest("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering a URL that causes cross-origin redirection. The
  // cross-origin redirection should fail prerendering without an opt-in header.
  const GURL kRedirectedUrl =
      GetSameSiteCrossOriginUrl("/empty.html?samesitecrossorigin");
  const GURL kPrerenderingUrl = GetUrlForSameSiteCrossOriginTest(
      "/server-redirect-credentialed-prerender?" + kRedirectedUrl.spec());
  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());
  test::PrerenderHostObserver host_observer(*web_contents_impl(),
                                            kPrerenderingUrl);
  AddPrerenderAsync(kPrerenderingUrl);
  host_observer.WaitForDestroyed();
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl), 1);
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  EXPECT_FALSE(HasHostForUrl(kRedirectedUrl));
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kSameSiteCrossOriginRedirectNotOptIn);
}

// Tests that same-site cross-origin redirection with credentialed prerender by
// speculation rules with the feature enabled but the redirected page without
// opt-in. This test verifies a case which is a.a.test -> b.a.test (credentialed
// prerender) -> b.a.test (no credentialed prerender)
IN_PROC_BROWSER_TEST_F(
    PrerenderSameSiteCrossOriginBrowserTest,
    SameSiteCrossOriginCredentialedPrerenderRedirectionSpeculationRulesWithoutOptInHeader2) {
  ASSERT_TRUE(blink::features::
                  IsSameSiteCrossOriginForSpeculationRulesPrerender2Enabled());
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrlForSameSiteCrossOriginTest("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering a URL that causes cross-origin redirection. The
  // cross-origin redirection should fail prerendering without an opt-in header.
  const GURL kRedirectedUrl =
      GetSameSiteCrossOriginUrl("/empty.html?samesitecrossorigin");
  const GURL kPrerenderingUrl = GetSameSiteCrossOriginUrl(
      "/server-redirect-credentialed-prerender?" + kRedirectedUrl.spec());
  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());
  test::PrerenderHostObserver host_observer(*web_contents_impl(),
                                            kPrerenderingUrl);
  AddPrerenderAsync(kPrerenderingUrl);
  host_observer.WaitForDestroyed();
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl), 1);
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  EXPECT_FALSE(HasHostForUrl(kRedirectedUrl));
  ExpectFinalStatusForSpeculationRule(
      PrerenderFinalStatus::kSameSiteCrossOriginRedirectNotOptIn);
}

// Tests that same-site cross-origin navigation redirecting back to same-origin
// without opt-in.
IN_PROC_BROWSER_TEST_F(
    PrerenderSameSiteCrossOriginBrowserTest,
    SameSiteCrossOriginNavigationBackToSameOriginWithoutOptInHeader) {
  ASSERT_TRUE(blink::features::
                  IsSameSiteCrossOriginForSpeculationRulesPrerender2Enabled());
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrlForSameSiteCrossOriginTest("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering a URL that causes cross-origin navigation and redirects
  // back to the same-origin. This should not fail even without same-site
  // cross-origin opt-in header.
  const GURL kRedirectedUrl =
      GetUrlForSameSiteCrossOriginTest("/empty.html?samesitecrossorigin");
  const GURL kPrerenderingUrl =
      GetSameSiteCrossOriginUrl("/server-redirect?" + kRedirectedUrl.spec());
  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());
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
IN_PROC_BROWSER_TEST_F(PrerenderSameSiteCrossOriginBrowserTest,
                       CrossSiteMultipleRedirectionSpeculationRules) {
  ASSERT_TRUE(blink::features::
                  IsSameSiteCrossOriginForSpeculationRulesPrerender2Enabled());
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrlForSameSiteCrossOriginTest("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering a URL that causes cross-origin redirection. The
  // cross-origin redirection should fail prerendering without an opt-in header.
  const GURL kRedirectedUrl = GetSameSiteCrossOriginUrl(
      "/prerender/prerender_with_opt_in_header.html?prerender");
  const GURL kRedirectedUrl2 =
      GetCrossSiteUrl("/server-redirect?" + kRedirectedUrl.spec());
  const GURL kPrerenderingUrl = GetUrlForSameSiteCrossOriginTest(
      "/server-redirect?" + kRedirectedUrl2.spec());
  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());
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
  ExpectFinalStatusForSpeculationRule(PrerenderFinalStatus::kCrossSiteRedirect);
}

// Tests that same-site cross-origin navigation by speculation rules can be
// prerendered with the feature enabled.
IN_PROC_BROWSER_TEST_F(PrerenderSameSiteCrossOriginBrowserTest,
                       CheckSameSiteCrossOriginSpeculationRulesPrerender) {
  ASSERT_TRUE(blink::features::
                  IsSameSiteCrossOriginForSpeculationRulesPrerender2Enabled());
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
IN_PROC_BROWSER_TEST_F(PrerenderSameSiteCrossOriginBrowserTest,
                       SameSiteCrossOriginSpeculationRulesRedirection) {
  ASSERT_TRUE(blink::features::
                  IsSameSiteCrossOriginForSpeculationRulesPrerender2Enabled());
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

  {
    // Cross-check that in case redirection when the prerender navigates and
    // user ends up navigating to the redirected URL. accurate_triggering is
    // true.
    ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
    auto attempt_ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName, test::kPreloadingAttemptUkmMetrics);

    UkmEntry attempt_expected_entry = attempt_ukm_entry_builder().BuildEntry(
        ukm_source_id, PreloadingType::kPrerender,
        PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
        PreloadingTriggeringOutcome::kSuccess,
        PreloadingFailureReason::kUnspecified,
        /*accurate=*/true,
        /*ready_time=*/kMockElapsedTime);

    EXPECT_EQ(attempt_ukm_entries[0], attempt_expected_entry)
        << test::ActualVsExpectedUkmEntryToString(attempt_ukm_entries[0],
                                                  attempt_expected_entry);
  }
}

// Tests that multiple same-site cross-origin redirections by speculation rules
// is allowed, and only the terminal one is checked for the opt in header.
IN_PROC_BROWSER_TEST_F(
    PrerenderSameSiteCrossOriginBrowserTest,
    SameSiteCrossOriginSpeculationRulesMultipleRedirections) {
  ASSERT_TRUE(blink::features::
                  IsSameSiteCrossOriginForSpeculationRulesPrerender2Enabled());
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

  {
    // Cross-check that in case redirection when the prerender navigates and
    // user ends up navigating to the redirected URL. accurate_triggering is
    // true.
    ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
    auto attempt_ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName, test::kPreloadingAttemptUkmMetrics);

    UkmEntry attempt_expected_entry = attempt_ukm_entry_builder().BuildEntry(
        ukm_source_id, PreloadingType::kPrerender,
        PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
        PreloadingTriggeringOutcome::kSuccess,
        PreloadingFailureReason::kUnspecified,
        /*accurate=*/true,
        /*ready_time=*/kMockElapsedTime);

    EXPECT_EQ(attempt_ukm_entries[0], attempt_expected_entry)
        << test::ActualVsExpectedUkmEntryToString(attempt_ukm_entries[0],
                                                  attempt_expected_entry);
  }
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
      web_contents_impl()->StartPrerendering(
          prerender_initial_url, PrerenderTriggerType::kEmbedder,
          "EmbedderSuffixForTest",
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          nullptr);
  EXPECT_TRUE(prerender_handle);
  test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
      *shell()->web_contents(), prerender_initial_url);
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
  shell()->web_contents()->OpenURL(OpenURLParams(
      prerender_initial_url, Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      /*is_renderer_initiated=*/false));

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
      web_contents_impl()->StartPrerendering(
          prerendering_initial_url, PrerenderTriggerType::kEmbedder,
          "EmbedderSuffixForTest",
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          nullptr);
  EXPECT_TRUE(prerender_handle);
  test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
      *shell()->web_contents(), prerendering_initial_url);

  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kCrossSiteRedirect, 1);
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
          prerendering_url, PrerenderTriggerType::kEmbedder,
          "EmbedderSuffixForTest",
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          nullptr);
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
  int host_id = AddPrerender(kPrerenderingUrl);
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
  ASSERT_NE(AddPrerender(kPrerenderingUrl),
            RenderFrameHost::kNoFrameTreeNodeId);

  // PrerenderPageLoad:TriggeredPrerender is recorded for the initiator page
  // load.
  const std::vector<const ukm::mojom::UkmEntry*> entries =
      ukm_recorder.GetEntriesByName(
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

// Tests that theme color in a prerendered page does not affect
// the primary page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       ThemeColorSchemeChangeInNonPrimaryPage) {
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

  int host_id = AddPrerender(kPrerenderingUrl);
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
  int host_id = AddPrerender(kPrerenderingUrl);
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
  int host_id = GetHostForUrl(kPrerenderingUrl);
  WaitForPrerenderLoadCompleted(host_id);

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
  int host_id = AddPrerender(kPrerenderingUrl);
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
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, MixedContent) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerendering");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Make a prerendered page.
  int host_id = AddPrerender(kPrerenderingUrl);
  auto* prerendered_rfh = GetPrerenderedMainFrameHost(host_id);
  DCHECK(prerendered_rfh);
  EXPECT_TRUE(AddTestUtilJS(prerendered_rfh));

  test::PrerenderHostObserver host_observer(*web_contents(), host_id);

  // Make a mixed content iframe.
  std::ignore =
      ExecJs(prerendered_rfh,
             "add_iframe_async('http://a.test/empty.html?prerendering')",
             EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES);

  host_observer.WaitForDestroyed();
  EXPECT_EQ(prerender_helper()->GetHostForUrl(kPrerenderingUrl),
            RenderFrameHost::kNoFrameTreeNodeId);

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

  int host_id = AddPrerender(kPrerenderingUrl);
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

  int host_id = AddPrerender(kPrerenderingUrl);
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
  int host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHost* prerender_frame_host = GetPrerenderedMainFrameHost(host_id);
  RenderProcessHost* prerender_process_host =
      prerender_frame_host->GetProcess();
  ASSERT_NE(prerender_frame_host, nullptr);
  // Ensure that a prerender process is invisible in
  // ChildProcessLauncherPriority. This will put prerender processes in lower
  // priority compared to other active processes. (See
  // https://crbug.com/1211665)
  EXPECT_TRUE(prerender_process_host->IsProcessBackgrounded());

  // Activate the prerendered page.
  test::PrerenderHostObserver host_observer(*web_contents(), kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_TRUE(host_observer.was_activated());
  // Expect the change in the ChildProcessLauncherPriority to become visible.
  EXPECT_FALSE(prerender_process_host->IsProcessBackgrounded());
}

// Test that the prerendered page uses own UKM source id during navigation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, GetPageUkmSourceId) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  NavigationHandleObserver handle_observer(web_contents(), kPrerenderingUrl);
  int host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerender_rfh = GetPrerenderedMainFrameHost(host_id);

  ukm::SourceId nav_request_id = handle_observer.next_page_ukm_source_id();
  // Ensure that the prerendered page uses own UKM source id in navigation, not
  // from the primary main frame.
  EXPECT_NE(current_frame_host()->GetPageUkmSourceId(), nav_request_id);
  EXPECT_EQ(prerender_rfh->GetPageUkmSourceId(), nav_request_id);

  // Activate the prerendered page and check that the
  // RenderFrameHost::GetPageUkmSourceId and
  // NavigationHandle::GetNextPageUkmSourceId() for prerender activation
  // navigation are different.
  test::PrerenderHostObserver host_observer(*web_contents(), kPrerenderingUrl);
  NavigationHandleObserver activation_observer(web_contents(),
                                               kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_TRUE(host_observer.was_activated());

  ukm::SourceId activation_nav_request_id =
      activation_observer.next_page_ukm_source_id();
  EXPECT_EQ(web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId(),
            prerender_rfh->GetPageUkmSourceId());
  EXPECT_NE(prerender_rfh->GetPageUkmSourceId(), activation_nav_request_id);
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
  int host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostWrapper prerender_main_frame(
      GetPrerenderedMainFrameHost(host_id));

  // The prerender request should have the "Purpose: prefetch" header.
  TestPurposePrefetchHeader(kPrerenderingUrl);

  // Issue iframe and subresource requests in the prerendered page.
  EXPECT_TRUE(ExecJs(prerender_main_frame.get(), "run('before');",
                     EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Requests from the prerenderered page should have the header.
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
  int host_id = AddPrerender(kPrerenderingUrl);
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
  int host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHost* prerender_rfh = GetPrerenderedMainFrameHost(host_id);
  DCHECK(prerender_rfh);
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
          DCHECK_EQ(navigation_handle->GetNavigatingFrameType(),
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
          DCHECK_EQ(navigation_handle->GetNavigatingFrameType(),
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
          DCHECK_EQ(navigation_handle->GetNavigatingFrameType(),
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
  int host_id = AddPrerender(prerendering_url);
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
  int host_id = AddPrerender(prerendering_url);
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

// TODO(https://crbug.com/1408911): This test is flaky on Mac bots.
#if BUILDFLAG(IS_MAC)
#define MAYBE_CancelPrerenderWhenIsOverridingUserAgentDiffers \
  DISABLED_CancelPrerenderWhenIsOverridingUserAgentDiffers
#else
#define MAYBE_CancelPrerenderWhenIsOverridingUserAgentDiffers \
  CancelPrerenderWhenIsOverridingUserAgentDiffers
#endif
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MAYBE_CancelPrerenderWhenIsOverridingUserAgentDiffers) {
  const std::string user_agent_override = "foo";

  // Navigate to an initial page.
  const GURL initial_url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Enable user agent override for future navigations.
  UserAgentInjector injector(shell()->web_contents(), user_agent_override);

  const GURL prerendering_url = GetUrl("/empty.html?prerender");

  // Start prerendering.
  const int host_id = AddPrerender(prerendering_url);

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

class PrerenderPreloaderHoldbackBrowserTest : public PrerenderBrowserTest {
 public:
  PrerenderPreloaderHoldbackBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kPrerender2Holdback);
  }
  ~PrerenderPreloaderHoldbackBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrerenderPreloaderHoldbackBrowserTest,
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
  {
    // Cross-check that PreloadingHoldbackStatus is correctly set.
    ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
    auto attempt_ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName, test::kPreloadingAttemptUkmMetrics);

    UkmEntry attempt_expected_entry = attempt_ukm_entry_builder().BuildEntry(
        ukm_source_id, PreloadingType::kPrerender,
        PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kHoldback,
        PreloadingTriggeringOutcome::kUnspecified,
        PreloadingFailureReason::kUnspecified,
        /*accurate=*/true);

    EXPECT_EQ(attempt_ukm_entries[0], attempt_expected_entry)
        << test::ActualVsExpectedUkmEntryToString(attempt_ukm_entries[0],
                                                  attempt_expected_entry);
  }
}

class PrerenderFencedFrameBrowserTest : public PrerenderBrowserTest {
 public:
  PrerenderFencedFrameBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kFencedFrames, {}},
         {features::kPrivacySandboxAdsAPIsOverride, {}}},
        {/* disabled_features */});
  }
  ~PrerenderFencedFrameBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrerenderFencedFrameBrowserTest,
                       PrerenderFencedFrameBrowserTest) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");
  const GURL kFencedFrameUrl = GetUrl("/title1.html");
  constexpr char kAddFencedFrameScript[] = R"({
    const fenced_frame = document.createElement('fencedframe');
    fenced_frame.src = $1;
    document.body.appendChild(fenced_frame);
  })";

  const int kNumNavigations = 3;
  TestNavigationObserver nav_observer(web_contents(), kNumNavigations);

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  int host_id = AddPrerender(kPrerenderingUrl);
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

namespace {

class PrerenderPortalBrowserTest : public PrerenderBrowserTest {
 public:
  PrerenderPortalBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kPortals},
        /*disabled_features=*/{});
  }
  ~PrerenderPortalBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

// Test that the Prerender skips Portal element in a prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderPortalBrowserTest, DeferPortalForPrerender) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/title1.html");
  const GURL kPortalUrl = GetUrl("/title2.html");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  int host_id = AddPrerender(kPrerenderingUrl);

  // Adds a Portal to the prerendered page.
  auto* prerendered_rfh = GetPrerenderedMainFrameHost(host_id);
  ASSERT_TRUE(
      ExecJs(prerendered_rfh,
             JsReplace("{"
                       "  let portal = document.createElement('portal');"
                       "  portal.src = $1;"
                       "  document.body.appendChild(portal);"
                       "}",
                       kPortalUrl)));

  // Since we defer the Portal creation, we expect no child frames.
  ASSERT_EQ(prerendered_rfh->GetPortals().size(), 0UL);

  PortalCreatedObserver portal_created_observer(prerendered_rfh);
  // Activates the prerender page. The Portal is created.
  NavigatePrimaryPage(kPrerenderingUrl);

  auto* primary_main_rfh =
      static_cast<RenderFrameHostImpl*>(web_contents()->GetPrimaryMainFrame());
  ASSERT_EQ(primary_main_rfh, prerendered_rfh);

  Portal* portal = portal_created_observer.WaitUntilPortalCreated();
  ASSERT_NE(nullptr, portal);
  auto* portal_contents = portal->GetPortalContents();
  TestNavigationObserver portal_wc_observer(portal_contents, 1);
  portal_wc_observer.Wait();
  ASSERT_EQ(kPortalUrl, portal_wc_observer.last_navigation_url());

  // Since the Portal is now created after navigation to the prerender page, we
  // expect one child frame.
  ASSERT_EQ(prerendered_rfh->GetPortals().size(), 1UL);

  // The rest of this test asserts the correct behavior of the Portal.

  // Ensures the portal is NOT the current tab
  ASSERT_NE(nullptr, portal_contents);
  ASSERT_NE(portal_contents, web_contents());

  // Installs the adoption script to Portal's frame.
  auto* portal_frame = portal_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(ExecJs(portal_frame,
                     "window.addEventListener('portalactivate', e => { "
                     "  const portal = e.adoptPredecessor(document); "
                     "  document.body.appendChild(portal); "
                     "});"));

  // Activates the portal. We expect successful activation of the portal, and
  // the previous WebContents is adopted in a new portal.
  PortalActivatedObserver activated_observer(portal);
  PortalCreatedObserver adoption_observer(portal_frame);
  ExecuteScriptAsync(primary_main_rfh,
                     "document.querySelector('portal').activate();");
  ASSERT_EQ(blink::mojom::PortalActivateResult::kPredecessorWasAdopted,
            activated_observer.WaitForActivateResult());
  adoption_observer.WaitUntilPortalCreated();
}

// Tests that the portal is deleted when the embedding page is prerendered.
IN_PROC_BROWSER_TEST_F(PrerenderPortalBrowserTest, DeleteDeferredPortal) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/title1.html");
  const GURL kPortalUrl = GetUrl("/title2.html");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  int host_id = AddPrerender(kPrerenderingUrl);

  // Adds a Portal to the prerendered page.
  auto* prerendered_rfh = GetPrerenderedMainFrameHost(host_id);
  ASSERT_TRUE(
      ExecJs(prerendered_rfh,
             JsReplace("{"
                       "  let portal = document.createElement('portal');"
                       "  portal.src = $1;"
                       "  document.body.appendChild(portal);"
                       "}",
                       kPortalUrl)));

  // Since we defer the Portal creation, we expect no child frames.
  ASSERT_EQ(prerendered_rfh->GetPortals().size(), 0UL);

  // Deletes the portal before navigating to the prerendered page. This will
  // result in the |HTMLPortalElement| being disconnected.
  ASSERT_TRUE(
      ExecJs(prerendered_rfh,
             "document.body.removeChild(document.querySelector('portal'));"));

  // Activates the prerender page.
  NavigatePrimaryPage(kPrerenderingUrl);

  auto* primary_main_rfh =
      static_cast<RenderFrameHostImpl*>(web_contents()->GetPrimaryMainFrame());
  ASSERT_EQ(primary_main_rfh, prerendered_rfh);

  // The activated prerender page shall have no child.
  ASSERT_EQ(prerendered_rfh->GetPortals().size(), 0UL);
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
  int host_id = AddPrerender(kPrerenderingUrl);
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
  int host_id = AddPrerender(prerender_url);
  WaitForPrerenderLoadCompleted(host_id);

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

  int host_id = AddPrerender(prerender_url);
  test::PrerenderHostObserver prerender_observer(*web_contents_impl(), host_id);
  WaitForPrerenderLoadCompleted(host_id);
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
  int host_id = AddPrerender(prerender_url);
  WaitForPrerenderLoadCompleted(host_id);

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
  auto* new_web_contents = web_contents_impl()->OpenURL(params);
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

void CheckExpectedCrossOriginMetrics(
    const base::HistogramTester& histogram_tester,
    PrerenderCrossOriginRedirectionMismatch mismatch_type,
    absl::optional<PrerenderCrossOriginRedirectionProtocolChange>
        protocol_change,
    absl::optional<PrerenderCrossOriginRedirectionDomain> domain_change) {
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kCrossSiteRedirect, 1);
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
  if (domain_change.has_value()) {
    histogram_tester.ExpectUniqueSample(
        "Prerender.Experimental.CrossOriginRedirectionDomain.Embedder_"
        "EmbedderSuffixForTest",
        domain_change.value(), 1);
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
      /*protocol_change=*/absl::nullopt, /*domain_change=*/absl::nullopt);
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
      PrerenderCrossOriginRedirectionProtocolChange::kHttpProtocolUpgrade,
      /*domain_change=*/absl::nullopt);
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
      PrerenderCrossOriginRedirectionProtocolChange::kHttpProtocolDowngrade,
      /*domain_change=*/absl::nullopt);
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
  shell()->web_contents()->OpenURL(OpenURLParams(
      prerendering_url, Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      /*is_renderer_initiated=*/false));
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
  shell()->web_contents()->OpenURL(OpenURLParams(
      prerendering_url, Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      /*is_renderer_initiated=*/false));
  prerender_observer.WaitForActivation();
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kActivated, 1);
}

namespace {

class EnforceDisableSameSiteRedirectionForEmbedderTriggeredPrerenderBrowserTest
    : public PrerenderBrowserTest {
 public:
  EnforceDisableSameSiteRedirectionForEmbedderTriggeredPrerenderBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            blink::features::
                kSameSiteRedirectionForEmbedderTriggeredPrerender});
  }
  ~EnforceDisableSameSiteRedirectionForEmbedderTriggeredPrerenderBrowserTest()
      override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that embedder triggered prerender can be redirected to the same
// site.
IN_PROC_BROWSER_TEST_F(
    EnforceDisableSameSiteRedirectionForEmbedderTriggeredPrerenderBrowserTest,
    EmbedderTrigger_CrossOriginRedirection_FromSubdomain) {
  GURL initial_url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  GURL::Replacements set_host;
  set_host.SetHostStr("www.a.test");

  GURL redirected_url = GetUrl("/empty.html");
  GURL prerendering_url = GetUrl("/server-redirect?" + redirected_url.spec())
                              .ReplaceComponents(set_host);
  test::PrerenderHostObserver prerender_observer(*web_contents_impl(),
                                                 prerendering_url);
  std::unique_ptr<PrerenderHandle> prerender_handle =
      PrerenderEmbedderTriggeredCrossOriginRedirectionPage(
          *web_contents_impl(), prerendering_url, redirected_url);

  prerender_observer.WaitForDestroyed();
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderFinalStatus::kSameSiteCrossOriginRedirect, 1);
}

}  // namespace

// Tests PrerenderCrossOriginRedirectionMismatch.kHostMismatch and
// PrerenderCrossOriginRedirectionDomain.kCrossDomain are recorded
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
      /*protocol_change=*/absl::nullopt,
      PrerenderCrossOriginRedirectionDomain::kCrossDomain);
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
  EnableAccessibilityForWebContents(shell()->web_contents());

  // Start prerendering `kPrerenderingUrl`, which has an iframe attached.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  int host_id = AddPrerender(kPrerenderingUrl);
  ASSERT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);
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

  int host_id = AddPrerender(kPrerenderingUrl);
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
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  EXPECT_TRUE(AddTestUtilJS(current_frame_host()));

  // Start a prerender.
  const GURL kPrerenderingUrl = GetUrl("/title2.html");
  int host_id = AddPrerender(kPrerenderingUrl);

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

}  // namespace content
