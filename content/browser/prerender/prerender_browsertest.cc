// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <tuple>

#include "base/barrier_closure.h"
#include "base/base_switches.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/services/storage/public/mojom/storage_service.mojom.h"
#include "components/services/storage/public/mojom/test_api.test-mojom.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/file_system_access/file_system_chooser_test_helpers.h"
#include "content/browser/prerender/prerender_host.h"
#include "content/browser/prerender/prerender_host_registry.h"
#include "content/browser/prerender/prerender_metrics.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/input/synthetic_tap_gesture.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_type.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/background_color_change_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/mock_web_contents_observer.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/theme_change_waiter.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/mock_commit_deferring_condition.h"
#include "content/test/render_document_feature.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_mojo_binder_policy_applier_unittest.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
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
  kEnabledCrossSiteOnly,
  kEnabledWithSameSite,
};

std::string ToString(const testing::TestParamInfo<BackForwardCacheType>& info) {
  switch (info.param) {
    case BackForwardCacheType::kDisabled:
      return "Disabled";
    case BackForwardCacheType::kEnabledCrossSiteOnly:
      return "Enabled";
    case BackForwardCacheType::kEnabledWithSameSite:
      return "EnabledWithSameSite";
  }
}

int32_t InterfaceNameHasher(const std::string& interface_name) {
  return static_cast<int32_t>(base::HashMetricNameAs32Bits(interface_name));
}

RenderFrameHost* FindRenderFrameHost(Page& page, const GURL& url) {
  return content::FrameMatchingPredicate(
      page, base::BindRepeating(&content::FrameHasSourceUrl, url));
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

  friend class content::DocumentUserData<DocumentData>;

  base::WeakPtrFactory<DocumentData> weak_ptr_factory_{this};

  DOCUMENT_USER_DATA_KEY_DECL();
};

DOCUMENT_USER_DATA_KEY_IMPL(DocumentData);

class PrerenderBrowserTest : public ContentBrowserTest {
 public:
  using LifecycleStateImpl = RenderFrameHostImpl::LifecycleStateImpl;

  PrerenderBrowserTest() {
    prerender_helper_ =
        std::make_unique<test::PrerenderTestHelper>(base::BindRepeating(
            &PrerenderBrowserTest::web_contents, base::Unretained(this)));
  }
  ~PrerenderBrowserTest() override = default;

  void SetUp() override {
    prerender_helper_->SetUp(&ssl_server_);
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    host_resolver()->AddRule("*", "127.0.0.1");
    ssl_server_.AddDefaultHandlers(GetTestDataFilePath());
    ssl_server_.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(ssl_server_.Start());

    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
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

  GURL GetCrossOriginUrl(const std::string& path) {
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
    return web_contents_impl()->GetMainFrame();
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
        base::BindRepeating([](content::RenderFrameHostImpl* rfhi) {
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
        }));
  }

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
                     ->controller()
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

  void ExpectFinalStatusForSpeculationRule(PrerenderHost::FinalStatus status) {
    // Check FinalStatus in UMA.
    histogram_tester_.ExpectUniqueSample(
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

 protected:
  net::test_server::EmbeddedTestServer& ssl_server() { return ssl_server_; }

 private:
  net::test_server::EmbeddedTestServer ssl_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};

  std::unique_ptr<test::PrerenderTestHelper> prerender_helper_;

  base::HistogramTester histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
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

  NavigatePrimaryPage(kPrerenderingUrl);

  // The prerender host should be consumed.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  // Activating the prerendered page should not issue a request.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  ExpectFinalStatusForSpeculationRule(PrerenderHost::FinalStatus::kActivated);
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
  base::HistogramTester histogram_tester;

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
      PrerenderHost::FinalStatus::kNavigationBadHttpStatus);
}

// Tests that prerendering is cancelled if a network request for the
// navigation results in an non-empty response with 404 status.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderCancelledOnNonEmptyBody404) {
  base::HistogramTester histogram_tester;

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
      PrerenderHost::FinalStatus::kNavigationBadHttpStatus);
}

// Tests that prerendering is cancelled if a network request for the
// navigation results in an non-empty response with 500 status.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderCancelledOn500Page) {
  base::HistogramTester histogram_tester;

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
      PrerenderHost::FinalStatus::kNavigationBadHttpStatus);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderCancelledOn204Page) {
  base::HistogramTester histogram_tester;

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
      PrerenderHost::FinalStatus::kNavigationBadHttpStatus);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderCancelledOn205Page) {
  base::HistogramTester histogram_tester;

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
      PrerenderHost::FinalStatus::kNavigationBadHttpStatus);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderAllowedOn204Iframe) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering `kPrerenderingUrl`.
  const GURL kPrerenderingUrl = GetUrl("/title1.html");
  int host_id = AddPrerender(kPrerenderingUrl);
  test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);
  WaitForPrerenderLoadCompletion(kPrerenderingUrl);

  // Fetch a subframe that responses 204 status code.
  const GURL kIFrameUrl = GetUrl("/echo?status=204");
  RenderFrameHost* prerender_rfh = GetPrerenderedMainFrameHost(host_id);
  std::ignore =
      ExecJs(prerender_rfh,
             "const i = document.createElement('iframe'); i.src = '" +
                 kIFrameUrl.spec() + "'; document.body.appendChild(i);");

  // Fetching a subframe that response 204 status code shouldn't cancel
  // prerendering unlike the mainframe that response 204 status code.
  // https://wicg.github.io/nav-speculation/prerendering.html#no-bad-navs
  EXPECT_EQ(GetHostForUrl(kPrerenderingUrl), host_id);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, CancelOnAuthRequested) {
  base::HistogramTester histogram_tester;

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

  // Cancellation must have occurred due to authentication request.
  ExpectFinalStatusForSpeculationRule(
      PrerenderHost::FinalStatus::kLoginAuthRequested);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, CancelOnAuthRequestedSubframe) {
  base::HistogramTester histogram_tester;

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
      PrerenderHost::FinalStatus::kLoginAuthRequested);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, CancelOnAuthRequestedSubResource) {
  base::HistogramTester histogram_tester;

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
      PrerenderHost::FinalStatus::kLoginAuthRequested);
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
  NavigatePrimaryPage(kPrerenderingUrl);
  ASSERT_EQ(1u, activation_redirect_chain_observer.redirect_chain().size());
  EXPECT_EQ(kRedirectedUrl,
            activation_redirect_chain_observer.redirect_chain()[0]);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, CrossOriginRedirection) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering a URL that causes cross-origin redirection. The
  // cross-origin redirection should fail prerendering.
  const GURL kRedirectedUrl = GetCrossOriginUrl("/empty.html?prerender");
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
  ExpectFinalStatusForSpeculationRule(
      PrerenderHost::FinalStatus::kCrossOriginRedirect);
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
    EXPECT_FALSE(
        FrameTreeNode::GloballyFindByID(host_id)->frame_tree()->IsLoading());

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
    EXPECT_FALSE(
        FrameTreeNode::GloballyFindByID(host_id)->frame_tree()->IsLoading());

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
  const GURL kCrossOriginSubframeUrl = GetCrossOriginUrl(
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

  const GURL kCrossOriginSubframeUrl = GetCrossOriginUrl(
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
  base::HistogramTester histogram_tester;
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
      PrerenderHost::FinalStatus::kMainFrameNavigation);
}

// Regression test for https://crbug.com/1198051
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, MainFrameFragmentNavigation) {
  base::HistogramTester histogram_tester;
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
    ASSERT_TRUE(ExecJs(web_contents()->GetMainFrame(),
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
  content::ShellContentBrowserClient::Get()
      ->set_create_throttles_for_navigation_callback(base::BindLambdaForTesting(
          [&throttle](content::NavigationHandle* handle)
              -> std::vector<std::unique_ptr<content::NavigationThrottle>> {
            std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;

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
    prerender_manager.WaitForFirstYieldAfterDidStartNavigation();
    ASSERT_NE(throttle, nullptr);

    auto* request =
        NavigationRequest::From(prerender_manager.GetNavigationHandle());
    ASSERT_TRUE(request->IsDeferredForTesting());
    EXPECT_EQ(request->GetDeferringThrottleForTesting(), throttle);
    throttle = nullptr;

    request->GetNavigationThrottleRunnerForTesting()->CallResumeForTesting();
    prerender_manager.WaitForNavigationFinished();

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
      base::BindLambdaForTesting([&](FrameTree* frame_tree) {
        if (frame_tree == prerendered_render_frame_host->frame_tree()) {
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
  base::HistogramTester histogram_tester;

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
  ExpectFinalStatusForSpeculationRule(PrerenderHost::FinalStatus::kDestroyed);
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
      mojo::PendingReceiver<content::mojom::TestInterfaceForDefer> receiver) {
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
  base::HistogramTester histogram_tester;

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
  ExpectFinalStatusForSpeculationRule(
      PrerenderHost::FinalStatus::kMojoBinderPolicy);
  // `TestInterfaceForCancel` doesn't have a enum value because it is not used
  // in production, so histogram_tester should log
  // PrerenderCancelledInterface::kUnkown here.
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCancelledInterface.SpeculationRule",
      PrerenderCancelledInterface::kUnknown, 1);
  histogram_tester.ExpectUniqueSample(
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
  base::HistogramTester histogram_tester;

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
  ExpectFinalStatusForSpeculationRule(
      PrerenderHost::FinalStatus::kMojoBinderPolicy);
  // `TestInterfaceForCancel` doesn't have a enum value because it is not used
  // in production, so histogram_tester should log
  // PrerenderCancelledInterface::kUnkown here.
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCancelledInterface.SpeculationRule",
      PrerenderCancelledInterface::kUnknown, 1);
  histogram_tester.ExpectUniqueSample(
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
  GURL cross_origin_iframe_url = GetCrossOriginUrl("/title1.html");

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
  subframe_navigation_manager.WaitForFirstYieldAfterDidStartNavigation();
  FrameTreeNode* child_ftn =
      FrameTreeNode::GloballyFindByID(host_id)->child_at(0);
  NavigationRequest* child_navigation = child_ftn->navigation_request();
  ASSERT_NE(child_navigation, nullptr);
  ASSERT_TRUE(child_navigation->IsDeferredForTesting());

  // 4. Collect all RenderFrameHosts in the frame tree.
  std::vector<RenderFrameHostImpl*> all_prerender_frames;
  size_t count_speculative = 0;
  prerendered_render_frame_host->ForEachRenderFrameHostIncludingSpeculative(
      base::BindLambdaForTesting([&](RenderFrameHostImpl* rfh) {
        all_prerender_frames.push_back(rfh);
        count_speculative +=
            (rfh->lifecycle_state() == LifecycleStateImpl::kSpeculative);
      }));
  ASSERT_EQ(all_prerender_frames.size(), 4u);
  ASSERT_EQ(count_speculative, 1u);

  // 5. Activate the prerendered page and listen to the DidFinishNavigation
  // event, to ensure the Activate IPC is sent.
  TestActivationManager prerendered_activation_navigation(web_contents(),
                                                          prerendering_url);
  ASSERT_TRUE(ExecJs(web_contents()->GetMainFrame(),
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
  PrerenderHost::FinalStatus GetExpectedFinalStatus() {
    switch (GetParam()) {
      case SSLPrerenderTestErrorBlockType::kClientCertRequested:
        return PrerenderHost::FinalStatus::kClientCertRequested;
      case SSLPrerenderTestErrorBlockType::kCertError:
        return PrerenderHost::FinalStatus::kSslCertificateError;
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
  base::HistogramTester histogram_tester;

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
  base::HistogramTester histogram_tester;

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
            content::RenderFrameHost::kNoFrameTreeNodeId);

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
  base::HistogramTester histogram_tester;

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
          ? PrerenderHost::FinalStatus::kClientCertRequested
          : PrerenderHost::FinalStatus::kNavigationRequestNetworkError);
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

  base::HistogramTester histogram_tester;

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

// TODO(https://crbug.com/1132746): Test canceling prerendering when its
// initiator is no longer interested in prerending this page.

// TODO(https://crbug.com/1132746): Test prerendering for auth error, etc.

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
  base::HistogramTester histogram_tester;
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
  histogram_tester.ExpectTotalCount(
      "Navigation.TimeToActivatePrerender.SpeculationRule", 1u);
}

// Test that prerender activation is deferred and resumed after the ongoing
// (in-flight) main-frame navigation in the prerendering frame tree commits.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       SupportActivationWithOngoingMainFrameNavigation) {
  base::HistogramTester histogram_tester;

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
    ASSERT_TRUE(ExecJs(shell()->web_contents()->GetMainFrame(),
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
  histogram_tester.ExpectTotalCount(
      "Navigation.Prerender.ActivationCommitDeferTime", 1u);
}

// Tests that prerendering is gated behind CSP:prefetch-src
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, CSPPrefetchSrc) {
  base::HistogramTester histogram_tester;

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
    console_observer.Wait();
    EXPECT_EQ(1u, console_observer.messages().size());
    EXPECT_EQ(GetRequestCount(disallowed_url), 0);
    host_observer.WaitForDestroyed();
    ExpectFinalStatusForSpeculationRule(
        PrerenderHost::FinalStatus::kNavigationRequestBlockedByCsp);
  }

  // TODO(https://crbug.com/1215031): Remove this reload after fixing the issue.
  // Now a document cannot trigger prerendering twice, even if the first started
  // one is canceled. So we have to reload the initiator page to get a new
  // document instance.
  ReloadBlockUntilNavigationsComplete(shell(), 1);
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
  base::HistogramTester histogram_tester;

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
    console_observer.Wait();
    EXPECT_EQ(1u, console_observer.messages().size());
    EXPECT_EQ(GetRequestCount(disallowed_url), 0);
    host_observer.WaitForDestroyed();
    ExpectFinalStatusForSpeculationRule(
        PrerenderHost::FinalStatus::kNavigationRequestBlockedByCsp);
  }

  // TODO(https://crbug.com/1215031): Remove this reload after fixing the issue.
  // Now a document cannot trigger prerendering twice, even if the first started
  // one is canceled. So we have to reload the initiator page to get a new
  // document instance.
  ReloadBlockUntilNavigationsComplete(shell(), 1);
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
  for (auto& result : resultsList.GetListDeprecated())
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
  base::HistogramTester histogram_tester;
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
  ExpectFinalStatusForSpeculationRule(
      PrerenderHost::FinalStatus::kMojoBinderPolicy);
  histogram_tester.ExpectUniqueSample(
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

#if BUILDFLAG(ENABLE_PLUGINS)
// Tests that we will cancel the prerendering if the prerendering page attempts
// to use plugins.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PluginsCancelPrerendering) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  LoadAndWaitForPrerenderDestroyed(
      web_contents(), GetUrl("/prerender/page-with-embedded-plugin.html"),
      prerender_helper());
  ExpectFinalStatusForSpeculationRule(
      PrerenderHost::FinalStatus::kMojoBinderPolicy);
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCancelledInterface.SpeculationRule",
      PrerenderCancelledInterface::kUnknown, 1);
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCancelledUnknownInterface."
      "SpeculationRule",
      InterfaceNameHasher(mojom::PepperHost::Name_), 1);

  // TODO(https://crbug.com/1215031): Remove this reload after fixing the issue.
  // Now a document cannot trigger prerendering twice, even if the first started
  // one is canceled. So we have to reload the initiator page to get a new
  // document instance.
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  LoadAndWaitForPrerenderDestroyed(
      web_contents(), GetUrl("/prerender/page-with-object-plugin.html"),
      prerender_helper());
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderHost::FinalStatus::kMojoBinderPolicy, 2);
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCancelledInterface.SpeculationRule",
      PrerenderCancelledInterface::kUnknown, 2);
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCancelledUnknownInterface."
      "SpeculationRule",
      InterfaceNameHasher(mojom::PepperHost::Name_), 2);
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

// This is a browser test and cannot be upstreamed to WPT because it diverges
// from the spec by cancelling prerendering in the Notification constructor,
// whereas the spec says to defer upon use requestPermission().
#if BUILDFLAG(IS_ANDROID)
// On Android the Notification constructor throws an exception regardless of
// whether the page is being prerendered.
// Tests that we will get the exception from the prerendering if the
// prerendering page attempts to use notification.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, NotificationConstructorAndroid) {
  base::HistogramTester histogram_tester;
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
#else
// On non-Android the Notification constructor is supported and can be used to
// show a notification, but if used during prerendering it cancels prerendering.
// Tests that we will cancel the prerendering if the prerendering page attempts
// to use notification.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, NotificationConstructor) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  LoadAndWaitForPrerenderDestroyed(web_contents(),
                                   GetUrl("/prerender/notification.html"),
                                   prerender_helper());

  ExpectFinalStatusForSpeculationRule(
      PrerenderHost::FinalStatus::kMojoBinderPolicy);
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCancelledInterface.SpeculationRule",
      PrerenderCancelledInterface::kNotificationService, 1);
}
#endif  // BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/1215073): Make a WPT when we have a stable way to wait
// cancellation runs.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, DownloadByScript) {
  base::HistogramTester histogram_tester;
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

  ExpectFinalStatusForSpeculationRule(PrerenderHost::FinalStatus::kDownload);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, DownloadInMainFrame) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // TODO(crbug.com/1215073): Make a WPT for the content-disposition WPT test.
  const GURL kDownloadUrl =
      GetUrl("/set-header?Content-Disposition: attachment");

  LoadAndWaitForPrerenderDestroyed(web_contents(), kDownloadUrl,
                                   prerender_helper());

  ExpectFinalStatusForSpeculationRule(PrerenderHost::FinalStatus::kDownload);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, DownloadInSubframe) {
  base::HistogramTester histogram_tester;
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

  ExpectFinalStatusForSpeculationRule(PrerenderHost::FinalStatus::kDownload);
}

// Tests that requesting audio output devices from prerendering documents result
// in cancellation of prerendering. Prerender2 decides to cancel prerendering
// here, because browser cannot defer this request as the renderer's main thread
// blocks while it waits for the response.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, RequestAudioOutputDevice) {
  base::HistogramTester histogram_tester;
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
      PrerenderHost::FinalStatus::kAudioOutputDeviceRequested);
}

// Tests that an activated page is allowed to request output devices.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       RequestAudioOutputDeviceAfterActivation) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/title1.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  int host_id = AddPrerender(kPrerenderingUrl);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);

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
  EXPECT_TRUE(ExecJs(web_contents()->GetMainFrame(), audio_script));
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
  RenderFrameHostImpl* primary_rfh = web_contents_impl()->GetMainFrame();

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
        {{blink::features::kPrerender2, {}},
         {blink::features::kPrerender2MemoryControls,
          {{blink::features::kPrerender2MemoryThresholdParamName,
            memory_threshold}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that prerendering doesn't run for low-end devices.
IN_PROC_BROWSER_TEST_F(PrerenderLowMemoryBrowserTest, NoPrerender) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  // Attempt to prerender.
  test::PrerenderHostRegistryObserver observer(*web_contents_impl());
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  AddPrerenderAsync(kPrerenderingUrl);
  observer.WaitForTrigger(kPrerenderingUrl);

  // It should fail.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  ExpectFinalStatusForSpeculationRule(
      PrerenderHost::FinalStatus::kLowEndDevice);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       IsInactiveAndDisallowActivationCancelsPrerendering) {
  base::HistogramTester histogram_tester;
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
  ExpectFinalStatusForSpeculationRule(PrerenderHost::FinalStatus::kDestroyed);
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
  params.gesture_source_type = content::mojom::GestureSourceType::kTouchInput;
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
  iframe_observer.WaitForNavigationFinished();
  EXPECT_TRUE(iframe_observer.was_committed());
  EXPECT_TRUE(iframe_observer.was_successful());
  EXPECT_EQ(child_frame->GetLastCommittedURL(), kNewIframeUrl);
}

// Ensure that WebContentsObserver::DidFailLoad is not invoked and cancels
// prerendering when invoked inside prerender frame tree.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, DidFailLoadCancelsPrerendering) {
  base::HistogramTester histogram_tester;
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
  ASSERT_TRUE(ExecJs(web_contents()->GetMainFrame(),
                     JsReplace("location = $1", kPrerenderingUrl)));
  navigation_observer.WaitForNavigationFinished();
  EXPECT_FALSE(prerender_observer.was_activated());

  ExpectFinalStatusForSpeculationRule(PrerenderHost::FinalStatus::kDidFailLoad);
}

// Ensures WebContents::OpenURL with a cross-origin URL targeting a frame in a
// prerendered host will successfully navigate that frame, though it should be
// deferred until activation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       OpenURLCrossOriginInPrerenderingFrame) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_blank_iframe.html");
  const GURL kNewIframeUrl = GetCrossOriginUrl("/simple_page.html");

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
    iframe_observer.WaitForFirstYieldAfterDidStartNavigation();
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
    ASSERT_TRUE(ExecJs(web_contents()->GetMainFrame(),
                       JsReplace("location = $1", kPrerenderingUrl)));
    prerender_observer.WaitForActivation();
  }

  // Now that we're activated, the iframe navigation should be able to finish.
  // Ensure the navigation completes in the iframe.
  {
    iframe_observer.WaitForNavigationFinished();
    child_frame = ChildFrameAt(web_contents()->GetMainFrame(), 0);
    ASSERT_TRUE(child_frame);
    EXPECT_EQ(child_frame->GetLastCommittedURL(), kNewIframeUrl);
  }
}

// Test starting a main frame navigation after the initial
// prerender navigation when activation has already started.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MainFrameNavigationDuringActivation) {
  base::HistogramTester histogram_tester;
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
    ASSERT_TRUE(ExecJs(web_contents()->GetMainFrame(),
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
  bad_nav_observer.WaitForNavigationFinished();
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
      PrerenderHost::FinalStatus::kMainFrameNavigation);

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
  const GURL kCrossOriginUrl = GetCrossOriginUrl("/simple_page.html");

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
    ASSERT_TRUE(ExecJs(web_contents()->GetMainFrame(),
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

  iframe_nav_observer.WaitForFirstYieldAfterDidStartNavigation();

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
  iframe_nav_observer.WaitForNavigationFinished();
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
    ASSERT_TRUE(ExecJs(web_contents()->GetMainFrame(),
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
    iframe_observer.WaitForNavigationFinished();
    EXPECT_EQ(child_frame->GetLastCommittedURL(), kNewIframeUrl);
  }

  // Allow the activation navigation to complete.
  activation_observer.WaitForNavigationFinished();
  EXPECT_TRUE(activation_observer.was_activated());
}

class ScopedDataSaverTestContentBrowserClient
    : public TestContentBrowserClient {
 public:
  ScopedDataSaverTestContentBrowserClient()
      : old_client(SetBrowserClientForTesting(this)) {}
  ~ScopedDataSaverTestContentBrowserClient() override {
    SetBrowserClientForTesting(old_client);
  }

  // ContentBrowserClient overrides:
  bool IsDataSaverEnabled(BrowserContext* context) override { return true; }

  void OverrideWebkitPrefs(WebContents* web_contents,
                           blink::web_pref::WebPreferences* prefs) override {
    prefs->data_saver_enabled = true;
  }

 private:
  raw_ptr<ContentBrowserClient> old_client;
};

// Tests that the data saver doesn't prevent image load in a prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, DataSaver) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/prerender/image.html");
  const GURL kImageUrl = GetUrl("/blank.jpg");

  // Enable data saver.
  ScopedDataSaverTestContentBrowserClient scoped_content_browser_client;
  shell()->web_contents()->OnWebPreferencesChanged();

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(shell()->web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Start prerendering `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  // A request for the image in the prerendered page shouldn't be prevented by
  // the data saver.
  EXPECT_EQ(GetRequestCount(kImageUrl), 1);
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
  content::TestNavigationObserver observer(shell()->web_contents());
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
  content::TestNavigationObserver observer(shell()->web_contents());
  shell()->GoBackOrForward(-1);
  observer.Wait();
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), kInitialUrl);

  EXPECT_EQ(
      "activated, initial",
      EvalJs(current_frame_host(), "getSessionStorageKeys()").ExtractString());
}

// Test if the host is abandoned when the renderer page crashes.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, AbandonIfRendererProcessCrashes) {
  base::HistogramTester histogram_tester;
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
    // On x86 and x86_64 Android, IMMEDIATE_CRASH() macro used in
    // ChildProcessHostImpl::CrashHungProcess() called from ForceCrash()
    // does not seem to work as expected. (See https://crbug.com/1211655)
    // We have no other ForceCrash() call sites on other than Linux and CrOS.
    // In this test, we call Shutdown(content::RESULT_CODE_HUNG) instead as
    // HungRenderDialogView does so on other platforms than Linux and CrOS.
    process->Shutdown(content::RESULT_CODE_HUNG);
#else
    // On Android, ForceCrash results in TERMINATION_STATUS_NORMAL_TERMINATION.
    // On other platforms, it does in TERMINATION_STATUS_PROCESS_CRASHED.
    process->ForceCrash();
#endif
    host_observer.WaitForDestroyed();
  }

  ExpectFinalStatusForSpeculationRule(
#if BUILDFLAG(IS_ANDROID)
      PrerenderHost::FinalStatus::kRendererProcessKilled);
#else
      PrerenderHost::FinalStatus::kRendererProcessCrashed);
#endif  // BUILDFLAG(IS_ANDROID)
}

// Test if the host is abandoned when the renderer page is killed.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, AbandonIfRendererProcessIsKilled) {
  base::HistogramTester histogram_tester;
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
      PrerenderHost::FinalStatus::kRendererProcessKilled);
}

class PrerenderBackForwardCacheBrowserTest : public PrerenderBrowserTest {
 public:
  PrerenderBackForwardCacheBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kBackForwardCache, {{"enable_same_site", "true"}}},
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

  RenderFrameHostWrapper main_frame(shell()->web_contents()->GetMainFrame());

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
  ASSERT_EQ(shell()->web_contents()->GetMainFrame(), main_frame.get());

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
    // On x86 and x86_64 Android, IMMEDIATE_CRASH() macro used in CrashNow()
    // does not seem to work as expected. (See https://crbug.com/1211655)
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
    // Set up the common params for the BFCache.
    base::FieldTrialParams feature_params;
    feature_params["TimeToLiveInBackForwardCacheInSeconds"] = "3600";

    // Allow the BFCache for all devices regardless of their memory.
    std::vector<base::Feature> disabled_features{
        features::kBackForwardCacheMemoryControls};

    switch (GetParam()) {
      case BackForwardCacheType::kDisabled:
        feature_list_.InitAndDisableFeature(features::kBackForwardCache);
        break;
      case BackForwardCacheType::kEnabledCrossSiteOnly:
        feature_params["enable_same_site"] = "false";
        feature_list_.InitWithFeaturesAndParameters(
            {{features::kBackForwardCache, feature_params}}, disabled_features);
        break;
      case BackForwardCacheType::kEnabledWithSameSite:
        feature_list_.InitWithFeaturesAndParameters(
            {{features::kBackForwardCache, feature_params}}, disabled_features);
        break;
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PrerenderWithBackForwardCacheBrowserTest,
    testing::Values(BackForwardCacheType::kDisabled,
                    BackForwardCacheType::kEnabledCrossSiteOnly,
                    BackForwardCacheType::kEnabledWithSameSite),
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
    case BackForwardCacheType::kEnabledCrossSiteOnly:
      // Same-origin prerender activation should allow the initial page to be
      // cached in the BFCache even if BFCache for same-site navigations is not
      // enabled. This is because prerender activation always swaps
      // BrowsingInstance, making the previous page cacheable, unlike regular
      // same-origin navigation.
      ASSERT_FALSE(IsSameSiteBackForwardCacheEnabled());
      EXPECT_TRUE(initial_frame_host->IsInBackForwardCache());
      break;
    case BackForwardCacheType::kEnabledWithSameSite:
      // Same-origin prerender activation should allow the initial page to be
      // cached in the BFCache.
      ASSERT_TRUE(IsSameSiteBackForwardCacheEnabled());
      EXPECT_TRUE(initial_frame_host->IsInBackForwardCache());
      break;
  }

  // Navigate back to the initial page.
  content::TestNavigationObserver observer(web_contents());
  shell()->GoBackOrForward(-1);
  observer.Wait();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Check if the back navigation is served from the BFCache.
  switch (GetParam()) {
    case BackForwardCacheType::kDisabled:
      // The frame host should be created again.
      EXPECT_NE(current_frame_host()->GetFrameToken(), initial_frame_token);
      break;
    case BackForwardCacheType::kEnabledCrossSiteOnly:
    case BackForwardCacheType::kEnabledWithSameSite:
      // The frame host should be restored.
      EXPECT_EQ(current_frame_host()->GetFrameToken(), initial_frame_token);
      EXPECT_FALSE(initial_frame_host->IsInBackForwardCache());
      break;
  }
}

// Tests that a trigger page destroys a prerendered page when it navigates
// forward and goes into the BFCache.
IN_PROC_BROWSER_TEST_P(PrerenderWithBackForwardCacheBrowserTest,
                       CancelOnAfterTriggerIsStoredInBackForwardCache_Forward) {
  base::HistogramTester histogram_tester;
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

  // Check if the initial page is in the BFCache.
  switch (GetParam()) {
    case BackForwardCacheType::kDisabled:
      // The BFCache is disabled, so the initial page is not in the BFCache.
      ASSERT_FALSE(initial_frame_host->IsInBackForwardCache());
      break;
    case BackForwardCacheType::kEnabledCrossSiteOnly:
      // The BFCache is enabled but the same-site BFCache is disabled. The
      // navigation was same-origin, so the initial page is not in the BFCache.
      ASSERT_FALSE(IsSameSiteBackForwardCacheEnabled());
      ASSERT_FALSE(initial_frame_host->IsInBackForwardCache());
      break;
    case BackForwardCacheType::kEnabledWithSameSite:
      // The same-site BFCache is enabled, so the initial page is in the BFCache
      // after the same-origin navigation.
      ASSERT_TRUE(IsSameSiteBackForwardCacheEnabled());
      ASSERT_TRUE(initial_frame_host->IsInBackForwardCache());
      break;
  }

  // The navigation should destroy the prerendered page regardless of if the
  // initial page was in the BFCache.
  prerender_observer.WaitForDestroyed();
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  ExpectFinalStatusForSpeculationRule(
      PrerenderHost::FinalStatus::kTriggerDestroyed);
}

// Tests that a trigger page destroys a prerendered page when it navigates back
// and goes into the BFCache.
IN_PROC_BROWSER_TEST_P(PrerenderWithBackForwardCacheBrowserTest,
                       CancelOnAfterTriggerIsStoredInBackForwardCache_Back) {
  base::HistogramTester histogram_tester;
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
  content::TestNavigationObserver navigation_observer(web_contents());
  shell()->GoBackOrForward(-1);
  navigation_observer.Wait();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Check if the next page is in the BFCache.
  switch (GetParam()) {
    case BackForwardCacheType::kDisabled:
      // The BFCache is disabled, so the next page is not in the BFCache.
      ASSERT_FALSE(next_frame_host->IsInBackForwardCache());
      break;
    case BackForwardCacheType::kEnabledCrossSiteOnly:
      // The BFCache is enabled but the same-site BFCache is disabled. The back
      // navigation was same-origin, so the next page is not in the BFCache.
      ASSERT_FALSE(IsSameSiteBackForwardCacheEnabled());
      ASSERT_FALSE(next_frame_host->IsInBackForwardCache());
      break;
    case BackForwardCacheType::kEnabledWithSameSite:
      // The same-site BFCache is enabled, so the next page is in the BFCache
      // after the same-origin back navigation.
      ASSERT_TRUE(IsSameSiteBackForwardCacheEnabled());
      ASSERT_TRUE(next_frame_host->IsInBackForwardCache());
      break;
  }

  // The navigation should destroy the prerendered page regardless of if the
  // next page was in the BFCache.
  prerender_observer.WaitForDestroyed();
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  ExpectFinalStatusForSpeculationRule(
      PrerenderHost::FinalStatus::kTriggerDestroyed);
}

// Tests that PrerenderHostRegistry only starts prerendering for the first
// prerender speculation rule it receives.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, AddSpeculationRulesMultipleTimes) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kFirstPrerenderingUrl = GetUrl("/empty.html?prerender1");
  const GURL kSecondPrerenderingUrl = GetUrl("/empty.html?prerender2");

  // Add the first prerender speculation rule; it should trigger prerendering
  // successfully.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  AddPrerender(kFirstPrerenderingUrl);
  histogram_tester.ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderHost::FinalStatus::kMaxNumOfRunningPrerendersExceeded, 0);

  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());

  // Add a new prerender speculation rule. Since PrerenderHostRegistry limits
  // the number of running prerenders to one, this rule should not be applied.
  AddPrerenderAsync(kSecondPrerenderingUrl);
  registry_observer.WaitForTrigger(kSecondPrerenderingUrl);
  EXPECT_FALSE(HasHostForUrl(kSecondPrerenderingUrl));
  ExpectFinalStatusForSpeculationRule(
      PrerenderHost::FinalStatus::kMaxNumOfRunningPrerendersExceeded);
}

// Tests that PrerenderHostRegistry can hold up to two prerendering for the
// prerender embedders it receives.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, StartByEmbeddersMultipleTimes) {
  base::HistogramTester histogram_tester;
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
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  EXPECT_TRUE(prerender_handle1);

  histogram_tester.ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderHost::FinalStatus::kMaxNumOfRunningPrerendersExceeded, 0);

  // Start prerendering by embedder triggered prerendering; this should be
  // trigger successfully.
  std::unique_ptr<PrerenderHandle> prerender_handle2 =
      web_contents_impl()->StartPrerendering(
          kSecondPrerenderingUrl, PrerenderTriggerType::kEmbedder,
          "EmbedderSuffixForTest",
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  EXPECT_TRUE(prerender_handle2);

  histogram_tester.ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderHost::FinalStatus::kMaxNumOfRunningPrerendersExceeded, 0);

  // Start prerendering by embedder triggered prerendering; this should hit the
  // limit.
  std::unique_ptr<PrerenderHandle> prerender_handle3 =
      web_contents_impl()->StartPrerendering(
          kThirdPrerenderingUrl, PrerenderTriggerType::kEmbedder,
          "EmbedderSuffixForTest",
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  EXPECT_FALSE(prerender_handle3);

  histogram_tester.ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderHost::FinalStatus::kMaxNumOfRunningPrerendersExceeded, 1);
}

// Tests that PrerenderHostRegistry can hold up to two prerendering for the
// prerender speculation rule and prerender embedders in total.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       StartByEmbeddersAndSpeculationRulesMultipleTimes) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kFirstPrerenderingUrl = GetUrl("/empty.html?prerender1");
  const GURL kSecondPrerenderingUrl = GetUrl("/empty.html?prerender2");
  const GURL kThirdPrerenderingUrl = GetUrl("/empty.html?prerender3");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  // Add the first prerender speculation rule; it should trigger prerendering
  // successfully.
  AddPrerender(kFirstPrerenderingUrl);
  histogram_tester.ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderHost::FinalStatus::kMaxNumOfRunningPrerendersExceeded, 0);

  histogram_tester.ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderHost::FinalStatus::kMaxNumOfRunningPrerendersExceeded, 0);

  // Start prerendering by embedder triggered prerendering; this should be
  // trigger successfully.
  std::unique_ptr<PrerenderHandle> prerender_handle2 =
      web_contents_impl()->StartPrerendering(
          kSecondPrerenderingUrl, PrerenderTriggerType::kEmbedder,
          "EmbedderSuffixForTest",
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  EXPECT_TRUE(prerender_handle2);

  histogram_tester.ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderHost::FinalStatus::kMaxNumOfRunningPrerendersExceeded, 0);

  // Start prerendering by embedder triggered prerendering; this should hit the
  // limit.
  std::unique_ptr<PrerenderHandle> prerender_handle3 =
      web_contents_impl()->StartPrerendering(
          kThirdPrerenderingUrl, PrerenderTriggerType::kEmbedder,
          "EmbedderSuffixForTest",
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  EXPECT_FALSE(prerender_handle3);

  histogram_tester.ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderHost::FinalStatus::kMaxNumOfRunningPrerendersExceeded, 1);

  // Start prerendering by embedder triggered prerendering; this should be
  // trigger successfully as one of the prerenders is freed.
  prerender_handle2.reset();
  prerender_handle3 = web_contents_impl()->StartPrerendering(
      kThirdPrerenderingUrl, PrerenderTriggerType::kEmbedder,
      "EmbedderSuffixForTest",
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  EXPECT_TRUE(prerender_handle3);
}

// Tests that cross-origin urls cannot be prerendered.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, SkipCrossOriginPrerender) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetCrossOriginUrl("/empty.html?crossorigin");

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());

  // Add a cross-origin prerender.
  AddPrerenderAsync(kPrerenderingUrl);

  // Wait for PrerenderHostRegistry to receive the cross-origin prerender
  // request, and it should be ignored.
  registry_observer.WaitForTrigger(kPrerenderingUrl);
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  ExpectFinalStatusForSpeculationRule(
      PrerenderHost::FinalStatus::kCrossOriginNavigation);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       EmbedderTrigger_SameOriginRedirection) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  const GURL kRedirectedUrl = GetUrl("/empty.html?prerender");
  const GURL kPrerenderingUrl =
      GetUrl("/server-redirect?" + kRedirectedUrl.spec());

  RedirectChainObserver redirect_chain_observer(*shell()->web_contents(),
                                                kRedirectedUrl);

  // Start prerendering by embedder triggered prerendering.
  std::unique_ptr<PrerenderHandle> prerender_handle =
      web_contents_impl()->StartPrerendering(
          kPrerenderingUrl, PrerenderTriggerType::kEmbedder,
          "EmbedderSuffixForTest",
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  EXPECT_TRUE(prerender_handle);
  test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
      *shell()->web_contents(), kPrerenderingUrl);
  ASSERT_EQ(2u, redirect_chain_observer.redirect_chain().size());

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "EmbedderSuffixForTest",
      PrerenderHost::FinalStatus::kEmbedderTriggeredAndSameOriginRedirected, 1);
}

void PrerenderEmbedderTriggeredCrossOriginRedirectionPage(
    WebContentsImpl& web_contents,
    const GURL& prerendering_url,
    const GURL& cross_origin_url) {
  RedirectChainObserver redirect_chain_observer{web_contents, cross_origin_url};

  // Start prerendering by embedder triggered prerendering.
  std::unique_ptr<PrerenderHandle> prerender_handle =
      web_contents.StartPrerendering(
          prerendering_url, PrerenderTriggerType::kEmbedder,
          "EmbedderSuffixForTest",
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  EXPECT_TRUE(prerender_handle);
  test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(web_contents,
                                                            prerendering_url);

  ASSERT_EQ(2u, redirect_chain_observer.redirect_chain().size());
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
  EXPECT_EQ(web_contents()->GetMainFrame()->GetPageUkmSourceId(),
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
  content::BackgroundColorChangeWaiter empty_page_background_waiter(
      web_contents());

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

  content::BackgroundColorChangeWaiter prerendered_page_background_waiter(
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

  content::ThemeChangeWaiter theme_change_waiter(web_contents());
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
  blink::mojom::TextAutosizerPageInfo primary_page_info =
      primary_frame_host->GetPage().text_autosizer_page_info();

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
  base::HistogramTester histogram_tester;
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

  ExpectFinalStatusForSpeculationRule(
      PrerenderHost::FinalStatus::kMixedContent);
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
      GetCrossOriginUrl("/prerender/cors-missing.txt?before");
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
  GURL cross_origin_url2 =
      GetCrossOriginUrl("/prerender/cors-missing.txt?after");
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
      web_contents_impl()->GetMainFrame(), u"", u"",
      JAVASCRIPT_DIALOG_TYPE_ALERT, false, base::NullCallback());

  // Navigate subframe (with render document enabled, this should cause a RFH
  // swap).
  TestNavigationManager subframe_nav_manager(web_contents(), kSubframeUrl2);
  ASSERT_TRUE(ExecJs(
      prerender_rfh,
      JsReplace("document.querySelector('iframe').src = $1", kSubframeUrl2)));
  subframe_nav_manager.WaitForNavigationFinished();

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
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
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
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  NavigatePrimaryPage(prerendering_url);
  ASSERT_TRUE(host_observer.was_activated());

  // The activated main frame should have the bit.
  EXPECT_TRUE(current_frame_host()
                  ->frame_tree_node()
                  ->has_received_user_gesture_before_nav());
}

class PrerenderFencedFrameBrowserTest
    : public PrerenderBrowserTest,
      public testing::WithParamInterface<bool /* shadow_dom_fenced_frames */> {
 public:
  PrerenderFencedFrameBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kFencedFrames,
          {{"implementation_type", GetParam() ? "shadow_dom" : "mparch"}}},
         {features::kPrivacySandboxAdsAPIsOverride, {}}},
        {/* disabled_features */});
  }
  ~PrerenderFencedFrameBrowserTest() override = default;

  bool IsShadowDomImpl() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(PrerenderFencedFrameBrowserTest,
                       PrerenderFencedFrameBrowserTest) {
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");
  const GURL kFencedFrameUrl = GetUrl("/title1.html");
  constexpr char kAddFencedFrameScript[] = R"({
    const fenced_frame = document.createElement('fencedframe');
    fenced_frame.src = $1;
    document.body.appendChild(fenced_frame);
  })";

  // We see a navigation to about:blank for Shadow DOM, but not MPArch, so we
  // need to account for another navigation with that implementation.
  const int kNumNavigations = IsShadowDomImpl() ? 4 : 3;
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
  prerendered_rfh->ForEachRenderFrameHost(
      base::BindLambdaForTesting([&](RenderFrameHostImpl* rfh) {
        if (rfh != prerendered_rfh)
          child_frame_count++;
      }));
  EXPECT_EQ(0lu, child_frame_count);

  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(kPrerenderingUrl, nav_observer.last_navigation_url());
  nav_observer.Wait();
  EXPECT_EQ(kFencedFrameUrl, nav_observer.last_navigation_url());
}

INSTANTIATE_TEST_SUITE_P(PrerenderFencedFrameBrowserTest,
                         PrerenderFencedFrameBrowserTest,
                         testing::Bool());

}  // namespace content
