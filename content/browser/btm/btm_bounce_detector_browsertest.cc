// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/btm/btm_bounce_detector.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/content_settings/core/common/features.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/browser/btm/btm_browsertest_utils.h"
#include "content/browser/btm/btm_service_impl.h"
#include "content/browser/btm/btm_storage.h"
#include "content/browser/btm/btm_test_utils.h"
#include "content/browser/btm/btm_utils.h"
#include "content/browser/tpcd_heuristics/opener_heuristic_tab_helper.h"
#include "content/browser/tpcd_heuristics/redirect_heuristic_tab_helper.h"
#include "content/common/features.h"
#include "content/public/browser/attribution_data_model.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/btm_redirect_info.h"
#include "content/public/browser/btm_service.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/interest_group_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/btm_service_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/render_frame_host_test_support.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/site_for_cookies.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/test_data_directory.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/network/public/cpp/features.h"
#include "services/network/test/trust_token_request_handler.h"
#include "services/network/test/trust_token_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "content/public/browser/scoped_authenticator_environment_for_testing.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device_factory.h"
#endif  // !BUILDFLAG(IS_ANDROID)

using base::Bucket;
using testing::Contains;
using testing::ElementsAre;
using testing::Eq;
using testing::Gt;
using testing::IsEmpty;
using testing::Pair;

namespace content {

namespace {

using AttributionData = std::set<AttributionDataModel::DataKey>;
using blink::mojom::StorageTypeAccessed;

// Returns a simplified URL representation for ease of comparison in tests.
// Just host+path.
std::string FormatURL(const GURL& url) {
  return base::StrCat({url.host(), url.path()});
}

void AppendRedirect(std::vector<std::string>* redirects,
                    const BtmRedirectInfo& redirect,
                    const BtmRedirectChainInfo& chain,
                    size_t redirect_index) {
  redirects->push_back(base::StringPrintf(
      "[%zu/%zu] %s -> %s (%s) -> %s", redirect_index + 1, chain.length,
      FormatURL(chain.initial_url).c_str(),
      FormatURL(redirect.redirector_url).c_str(),
      std::string(BtmDataAccessTypeToString(redirect.access_type)).c_str(),
      FormatURL(chain.final_url).c_str()));
}

void AppendRedirects(std::vector<std::string>* vec,
                     std::vector<BtmRedirectInfoPtr> redirects,
                     BtmRedirectChainInfoPtr chain) {
  size_t redirect_index = chain->length - redirects.size();
  for (const auto& redirect : redirects) {
    AppendRedirect(vec, *redirect, *chain, redirect_index);
    redirect_index++;
  }
}

void AppendSitesInReport(std::vector<std::string>* reports,
                         const std::set<std::string>& sites) {
  reports->push_back(base::JoinString(
      std::vector<std::string_view>(sites.begin(), sites.end()), ", "));
}

std::vector<url::Origin> GetOrigins(const AttributionData& data) {
  std::vector<url::Origin> origins;
  std::ranges::transform(data, std::back_inserter(origins),
                         &AttributionDataModel::DataKey::reporting_origin);
  return origins;
}

bool ContainsWrite(BtmDataAccessType access) {
  using enum BtmDataAccessType;
  return access == kWrite || access == kReadWrite;
}

// Waits for BTM to know that a cookie was written by a redirect at
// `redirect_url`, which must be the last redirect that was performed in the
// currently-in-progress redirect chain.
testing::AssertionResult WaitForRedirectCookieWrite(WebContents* web_contents,
                                                    const GURL& redirect_url) {
  RedirectChainDetector* detector =
      RedirectChainDetector::FromWebContents(web_contents);

  if (detector->CommittedRedirectContext().GetRedirectChainLength() == 0) {
    return testing::AssertionFailure() << "No redirects detected";
  }

  // Make sure the last redirect was at the expected URL.
  const BtmRedirectInfo& redirect =
      detector->CommittedRedirectContext()
          [detector->CommittedRedirectContext().size() - 1];
  if (redirect.redirector_url != redirect_url) {
    return testing::AssertionFailure()
           << "Expected redirect at " << redirect_url << "; found "
           << redirect.redirector_url;
  }

  if (!ContainsWrite(redirect.access_type)) {
    // We haven't been notified about the cookie write from the redirect yet.
    // Wait for it to ensure the bounce is considered stateful.
    URLCookieAccessObserver(web_contents, redirect_url,
                            CookieOperation::kChange)
        .Wait();
  }

  // Return success if the cookie write was detected.
  if (ContainsWrite(redirect.access_type)) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure() << "Still no cookie write detected";
  }
}

}  // namespace

// Keeps a log of DidStartNavigation, OnCookiesAccessed, and DidFinishNavigation
// executions.
class WCOCallbackLogger : public WebContentsObserver,
                          public WebContentsUserData<WCOCallbackLogger>,
                          public SharedWorkerService::Observer,
                          public DedicatedWorkerService::Observer {
 public:
  WCOCallbackLogger(const WCOCallbackLogger&) = delete;
  WCOCallbackLogger& operator=(const WCOCallbackLogger&) = delete;

  const std::vector<std::string>& log() const { return log_; }

 private:
  explicit WCOCallbackLogger(WebContents* web_contents);
  // So WebContentsUserData::CreateForWebContents() can call the constructor.
  friend class WebContentsUserData<WCOCallbackLogger>;

  // Start WebContentsObserver overrides:
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void OnCookiesAccessed(RenderFrameHost* render_frame_host,
                         const CookieAccessDetails& details) override;
  void OnCookiesAccessed(NavigationHandle* navigation_handle,
                         const CookieAccessDetails& details) override;
  void NotifyStorageAccessed(RenderFrameHost* render_frame_host,
                             StorageTypeAccessed storage_type,
                             bool blocked) override;
  void OnServiceWorkerAccessed(RenderFrameHost* render_frame_host,
                               const GURL& scope,
                               AllowServiceWorkerResult allowed) override;
  void OnServiceWorkerAccessed(NavigationHandle* navigation_handle,
                               const GURL& scope,
                               AllowServiceWorkerResult allowed) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void WebAuthnAssertionRequestSucceeded(
      RenderFrameHost* render_frame_host) override;
  // End WebContentsObserver overrides.

  // Start SharedWorkerService.Observer overrides:
  void OnClientAdded(const blink::SharedWorkerToken& token,
                     GlobalRenderFrameHostId render_frame_host_id) override;
  void OnWorkerCreated(const blink::SharedWorkerToken& token,
                       int worker_process_id,
                       const url::Origin& security_origin,
                       const base::UnguessableToken& dev_tools_token) override {
  }
  void OnBeforeWorkerDestroyed(const blink::SharedWorkerToken& token) override {
  }
  void OnClientRemoved(const blink::SharedWorkerToken& token,
                       GlobalRenderFrameHostId render_frame_host_id) override {}
  using SharedWorkerService::Observer::OnFinalResponseURLDetermined;
  // End SharedWorkerService.Observer overrides.

  // Start DedicatedWorkerService.Observer overrides:
  void OnWorkerCreated(const blink::DedicatedWorkerToken& worker_token,
                       int worker_process_id,
                       const url::Origin& security_origin,
                       DedicatedWorkerCreator creator) override;
  void OnBeforeWorkerDestroyed(const blink::DedicatedWorkerToken& worker_token,
                               DedicatedWorkerCreator creator) override {}
  void OnFinalResponseURLDetermined(
      const blink::DedicatedWorkerToken& worker_token,
      const GURL& url) override {}
  // End DedicatedWorkerService.Observer overrides.

  std::vector<std::string> log_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WCOCallbackLogger::WCOCallbackLogger(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      WebContentsUserData<WCOCallbackLogger>(*web_contents) {}

void WCOCallbackLogger::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  log_.push_back(
      base::StringPrintf("DidStartNavigation(%s)",
                         FormatURL(navigation_handle->GetURL()).c_str()));
}

void WCOCallbackLogger::OnCookiesAccessed(RenderFrameHost* render_frame_host,
                                          const CookieAccessDetails& details) {
  // Callbacks for favicons are ignored only in testing logs because their
  // ordering is variable and would cause flakiness
  if (details.url.GetPath() == "/favicon.ico") {
    return;
  }

  log_.push_back(base::StringPrintf(
      "OnCookiesAccessed(RenderFrameHost, %s: %s)",
      details.type == CookieOperation::kChange ? "Change" : "Read",
      FormatURL(details.url).c_str()));
}

void WCOCallbackLogger::OnCookiesAccessed(NavigationHandle* navigation_handle,
                                          const CookieAccessDetails& details) {
  log_.push_back(base::StringPrintf(
      "OnCookiesAccessed(NavigationHandle, %s: %s)",
      details.type == CookieOperation::kChange ? "Change" : "Read",
      FormatURL(details.url).c_str()));
}

void WCOCallbackLogger::OnServiceWorkerAccessed(
    RenderFrameHost* render_frame_host,
    const GURL& scope,
    AllowServiceWorkerResult allowed) {
  log_.push_back(
      base::StringPrintf("OnServiceWorkerAccessed(RenderFrameHost: %s)",
                         FormatURL(scope).c_str()));
}

void WCOCallbackLogger::OnServiceWorkerAccessed(
    NavigationHandle* navigation_handle,
    const GURL& scope,
    AllowServiceWorkerResult allowed) {
  log_.push_back(
      base::StringPrintf("OnServiceWorkerAccessed(NavigationHandle: %s)",
                         FormatURL(scope).c_str()));
}

void WCOCallbackLogger::OnClientAdded(
    const blink::SharedWorkerToken& token,
    GlobalRenderFrameHostId render_frame_host_id) {
  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_frame_host_id);
  GURL scope;
  if (render_frame_host) {
    scope = GetFirstPartyURL(*render_frame_host);
  }

  log_.push_back(base::StringPrintf("OnSharedWorkerClientAdded(%s)",
                                    FormatURL(scope).c_str()));
}

void WCOCallbackLogger::OnWorkerCreated(
    const blink::DedicatedWorkerToken& worker_token,
    int worker_process_id,
    const url::Origin& security_origin,
    DedicatedWorkerCreator creator) {
  const GlobalRenderFrameHostId& render_frame_host_id =
      std::get<GlobalRenderFrameHostId>(creator);
  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_frame_host_id);
  GURL scope;
  if (render_frame_host) {
    scope = GetFirstPartyURL(*render_frame_host);
  }

  log_.push_back(base::StringPrintf("OnDedicatedWorkerCreated(%s)",
                                    FormatURL(scope).c_str()));
}

void WCOCallbackLogger::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!IsInPrimaryPage(*navigation_handle)) {
    return;
  }

  log_.push_back(
      base::StringPrintf("DidFinishNavigation(%s)",
                         FormatURL(navigation_handle->GetURL()).c_str()));
}

void WCOCallbackLogger::WebAuthnAssertionRequestSucceeded(
    RenderFrameHost* render_frame_host) {
  log_.push_back(base::StringPrintf(
      "WebAuthnAssertionRequestSucceeded(%s)",
      FormatURL(render_frame_host->GetLastCommittedURL()).c_str()));
}

void WCOCallbackLogger::NotifyStorageAccessed(
    RenderFrameHost* render_frame_host,
    StorageTypeAccessed storage_type,
    bool blocked) {
  log_.push_back(base::StringPrintf(
      "NotifyStorageAccessed(%s: %s)", base::ToString(storage_type).c_str(),
      FormatURL(render_frame_host->GetLastCommittedURL()).c_str()));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WCOCallbackLogger);

class BtmBounceDetectorBrowserTest : public ContentBrowserTest {
 protected:
  BtmBounceDetectorBrowserTest()
      : prerender_test_helper_(base::BindRepeating(
            &BtmBounceDetectorBrowserTest::GetActiveWebContents,
            base::Unretained(this))) {
    enabled_features_.push_back(
        {network::features::kSkipTpcdMitigationsForAds,
         {{"SkipTpcdMitigationsForAdsHeuristics", "true"}}});
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features_,
                                                       disabled_features_);
    ContentBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Prevents flakiness by handling clicks even before content is drawn.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  void SetUpOnMainThread() override {
    browser_client_ =
        std::make_unique<ContentBrowserTestTpcBlockingBrowserClient>();
    prerender_test_helper_.RegisterServerRequestMonitor(embedded_test_server());
    net::test_server::RegisterDefaultHandlers(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("*", "127.0.0.1");

    // Set third-party cookies to be blocked by default. If they're not blocked
    // by default, BTM will not run.
    browser_client().SetBlockThirdPartyCookiesByDefault(true);
    WebContents* web_contents = GetActiveWebContents();
    ASSERT_FALSE(btm::Are3PcsGenerallyEnabled(web_contents->GetBrowserContext(),
                                              web_contents));

    SetUpBtmWebContentsObserver();
  }

  void SetUpBtmWebContentsObserver() {
    web_contents_observer_ =
        BtmWebContentsObserver::FromWebContents(GetActiveWebContents());
    CHECK(web_contents_observer_);
  }

  WebContents* GetActiveWebContents() { return shell()->web_contents(); }

  void StartAppendingRedirectsTo(std::vector<std::string>* redirects) {
    GetRedirectChainHelper()->SetRedirectChainHandlerForTesting(
        base::BindRepeating(&AppendRedirects, redirects));
  }

  void StartAppendingReportsTo(std::vector<std::string>* reports) {
    web_contents_observer_->SetIssueReportingCallbackForTesting(
        base::BindRepeating(&AppendSitesInReport, reports));
  }

  // Perform a browser-based navigation to terminate the current redirect chain.
  // (NOTE: tests using WCOCallbackLogger must call this *after* checking the
  // log, since this navigation will be logged.)
  //
  // By default (when `wait`=true) this waits for the BtmService to tell
  // observers that the redirect chain was handled. But some tests override
  // the handling flow so that chains don't reach the service (and so observers
  // are never notified). Such tests should pass `wait`=false.
  void EndRedirectChain(bool wait = true) {
    WebContents* web_contents = GetActiveWebContents();
    BtmService* btm_service =
        BtmService::Get(web_contents->GetBrowserContext());
    GURL expected_url = web_contents->GetLastCommittedURL();

    BtmRedirectChainObserver chain_observer(btm_service, expected_url);
    // Performing a browser-based navigation terminates the current redirect
    // chain.
    ASSERT_TRUE(NavigateToURL(
        web_contents,
        embedded_test_server()->GetURL("endthechain.test", "/title1.html")));
    if (wait) {
      chain_observer.Wait();
    }
  }

  auto* fenced_frame_test_helper() { return &fenced_frame_test_helper_; }
  auto* prerender_test_helper() { return &prerender_test_helper_; }

  RenderFrameHost* GetIFrame() {
    WebContents* web_contents = GetActiveWebContents();
    return ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  }

  RenderFrameHost* GetNestedIFrame() { return ChildFrameAt(GetIFrame(), 0); }

  RedirectChainDetector* GetRedirectChainHelper() {
    return RedirectChainDetector::FromWebContents(GetActiveWebContents());
  }

  void NavigateNestedIFrameTo(RenderFrameHost* parent_frame,
                              const std::string& iframe_id,
                              const GURL& url) {
    TestNavigationObserver load_observer(GetActiveWebContents());
    std::string script = base::StringPrintf(
        "var iframe = document.getElementById('%s');iframe.src='%s';",
        iframe_id.c_str(), url.spec().c_str());
    ASSERT_TRUE(ExecJs(parent_frame, script, EXECUTE_SCRIPT_NO_USER_GESTURE));
    load_observer.Wait();
  }

  void AccessCHIPSViaJSIn(RenderFrameHost* frame) {
    FrameCookieAccessObserver observer(GetActiveWebContents(), frame,
                                       CookieOperation::kChange);
    ASSERT_TRUE(ExecJs(frame,
                       "document.cookie = '__Host-foo=bar;"
                       "SameSite=None;Secure;Path=/;Partitioned';",
                       EXECUTE_SCRIPT_NO_USER_GESTURE));
    observer.Wait();
  }

  void SimulateMouseClick() {
    SimulateMouseClickAndWait(GetActiveWebContents());
  }

  void SimulateCookieWrite() {
    WebContents* web_contents = GetActiveWebContents();
    RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
    URLCookieAccessObserver cookie_observer(
        web_contents, frame->GetLastCommittedURL(), CookieOperation::kChange);
    ASSERT_TRUE(ExecJs(frame, "document.cookie = 'foo=bar';",
                       EXECUTE_SCRIPT_NO_USER_GESTURE));
    cookie_observer.Wait();
  }

  TpcBlockingBrowserClient& browser_client() { return browser_client_->impl(); }

  const base::FilePath kContentTestDataDir = GetTestDataFilePath();

  std::vector<base::test::FeatureRefAndParams> enabled_features_;
  std::vector<base::test::FeatureRef> disabled_features_;
  raw_ptr<BtmWebContentsObserver, AcrossTasksDanglingUntriaged>
      web_contents_observer_ = nullptr;

 private:
  test::PrerenderTestHelper prerender_test_helper_;
  test::FencedFrameTestHelper fenced_frame_test_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ContentBrowserTestTpcBlockingBrowserClient> browser_client_;
};

IN_PROC_BROWSER_TEST_F(
    BtmBounceDetectorBrowserTest,
    // TODO(crbug.com/40924446): Re-enable this test
    DISABLED_AttributeSameSiteIframesCookieClientAccessTo1P) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      embedded_test_server()->GetURL("a.test", "/page_with_blank_iframe.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL iframe_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(
      NavigateIframeToURL(GetActiveWebContents(), "test_iframe", iframe_url));

  AccessCookieViaJSIn(GetActiveWebContents(), GetIFrame());

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(redirects,
              ElementsAre(("[1/1] blank -> a.test/page_with_blank_iframe.html "
                           "(Write) -> d.test/title1.html")));
}

// TODO(crbug.com/40276415): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_AttributeSameSiteIframesCookieServerAccessTo1P \
  DISABLED_AttributeSameSiteIframesCookieServerAccessTo1P
#else
#define MAYBE_AttributeSameSiteIframesCookieServerAccessTo1P \
  AttributeSameSiteIframesCookieServerAccessTo1P
#endif
IN_PROC_BROWSER_TEST_F(BtmBounceDetectorBrowserTest,
                       MAYBE_AttributeSameSiteIframesCookieServerAccessTo1P) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(kContentTestDataDir);
  ASSERT_TRUE(https_server.Start());

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      embedded_test_server()->GetURL("a.test", "/page_with_blank_iframe.html");
  browser_client().AllowThirdPartyCookiesOnSite(
      embedded_test_server()->GetURL("a.test", "/"));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL iframe_url =
      https_server.GetURL("a.test", "/set-cookie?foo=bar;SameSite=None;Secure");
  ASSERT_TRUE(
      NavigateIframeToURL(GetActiveWebContents(), "test_iframe", iframe_url));

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(redirects,
              ElementsAre(("[1/1] blank -> a.test/page_with_blank_iframe.html "
                           "(Write) -> d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_F(BtmBounceDetectorBrowserTest,
                       Attribute3PIframesCHIPSClientAccessTo1P) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(kContentTestDataDir);
  ASSERT_TRUE(https_server.Start());

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      embedded_test_server()->GetURL("a.test", "/page_with_blank_iframe.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL iframe_url = https_server.GetURL("b.test", "/title1.html");
  ASSERT_TRUE(
      NavigateIframeToURL(GetActiveWebContents(), "test_iframe", iframe_url));

  AccessCHIPSViaJSIn(GetIFrame());

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  std::string access_type =
      base::FeatureList::IsEnabled(network::features::kGetCookiesOnSet)
          ? "ReadWrite"
          : "Write";
  EXPECT_THAT(redirects,
              ElementsAre(base::StringPrintf(
                  "[1/1] blank -> a.test/page_with_blank_iframe.html "
                  "(%s) -> d.test/title1.html",
                  access_type)));
}

IN_PROC_BROWSER_TEST_F(BtmBounceDetectorBrowserTest,
                       // TODO(crbug.com/40287072): Re-enable this test
                       DISABLED_Attribute3PIframesCHIPSServerAccessTo1P) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(kContentTestDataDir);
  ASSERT_TRUE(https_server.Start());

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      https_server.GetURL("a.test", "/page_with_blank_iframe.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL iframe_url =
      https_server.GetURL("b.test",
                          "/set-cookie?__Host-foo=bar;SameSite=None;"
                          "Secure;Path=/;Partitioned");
  ASSERT_TRUE(
      NavigateIframeToURL(GetActiveWebContents(), "test_iframe", iframe_url));

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(redirects,
              ElementsAre(("[1/1] blank -> a.test/page_with_blank_iframe.html "
                           "(Write) -> d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_F(
    BtmBounceDetectorBrowserTest,
    // TODO(crbug.com/40287072): Re-enable this test
    DISABLED_AttributeSameSiteNestedIframesCookieClientAccessTo1P) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      embedded_test_server()->GetURL("a.test", "/page_with_blank_iframe.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL iframe_url =
      embedded_test_server()->GetURL("a.test", "/page_with_blank_iframe.html");
  ASSERT_TRUE(
      NavigateIframeToURL(GetActiveWebContents(), "test_iframe", iframe_url));

  const GURL nested_iframe_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  NavigateNestedIFrameTo(GetIFrame(), "test_iframe", nested_iframe_url);

  AccessCookieViaJSIn(GetActiveWebContents(), GetNestedIFrame());

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(redirects,
              ElementsAre(("[1/1] blank -> a.test/page_with_blank_iframe.html "
                           "(Write) -> d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_F(
    BtmBounceDetectorBrowserTest,
    // TODO(crbug.com/40287072): Re-enable this test
    DISABLED_AttributeSameSiteNestedIframesCookieServerAccessTo1P) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(kContentTestDataDir);
  ASSERT_TRUE(https_server.Start());

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      embedded_test_server()->GetURL("a.test", "/page_with_blank_iframe.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL iframe_url =
      embedded_test_server()->GetURL("a.test", "/page_with_blank_iframe.html");
  ASSERT_TRUE(
      NavigateIframeToURL(GetActiveWebContents(), "test_iframe", iframe_url));

  const GURL nested_iframe_url =
      https_server.GetURL("a.test", "/set-cookie?foo=bar;SameSite=None;Secure");
  NavigateNestedIFrameTo(GetIFrame(), "test_iframe", nested_iframe_url);

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(redirects,
              ElementsAre(("[1/1] blank -> a.test/page_with_blank_iframe.html "
                           "(Write) -> d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_F(BtmBounceDetectorBrowserTest,
                       Attribute3PNestedIframesCHIPSClientAccessTo1P) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(kContentTestDataDir);
  ASSERT_TRUE(https_server.Start());

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      embedded_test_server()->GetURL("a.test", "/page_with_blank_iframe.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL iframe_url =
      embedded_test_server()->GetURL("b.test", "/page_with_blank_iframe.html");
  ASSERT_TRUE(
      NavigateIframeToURL(GetActiveWebContents(), "test_iframe", iframe_url));

  const GURL nested_iframe_url = https_server.GetURL("c.test", "/title1.html");
  NavigateNestedIFrameTo(GetIFrame(), "test_iframe", nested_iframe_url);

  AccessCHIPSViaJSIn(GetNestedIFrame());

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  std::string access_type =
      base::FeatureList::IsEnabled(network::features::kGetCookiesOnSet)
          ? "ReadWrite"
          : "Write";
  EXPECT_THAT(redirects,
              ElementsAre(base::StringPrintf(
                  "[1/1] blank -> a.test/page_with_blank_iframe.html "
                  "(%s) -> d.test/title1.html",
                  access_type)));
}

IN_PROC_BROWSER_TEST_F(BtmBounceDetectorBrowserTest,
                       Attribute3PNestedIframesCHIPSServerAccessTo1P) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(kContentTestDataDir);
  ASSERT_TRUE(https_server.Start());

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      embedded_test_server()->GetURL("a.test", "/page_with_blank_iframe.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL iframe_url =
      embedded_test_server()->GetURL("b.test", "/page_with_blank_iframe.html");
  ASSERT_TRUE(
      NavigateIframeToURL(GetActiveWebContents(), "test_iframe", iframe_url));

  const GURL nested_iframe_url = https_server.GetURL(
      "a.test",
      "/set-cookie?__Host-foo=bar;SameSite=None;Secure;Path=/;Partitioned");
  NavigateNestedIFrameTo(GetIFrame(), "test_iframe", nested_iframe_url);

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(redirects,
              ElementsAre(("[1/1] blank -> a.test/page_with_blank_iframe.html "
                           "(Write) -> d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_F(BtmBounceDetectorBrowserTest,
                       Attribute3PSubResourceCHIPSClientAccessTo1P) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(kContentTestDataDir);
  ASSERT_TRUE(https_server.Start());

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  // This block represents a navigation sequence with a CHIP access (write). It
  // might as well be happening in a separate tab from the navigation block
  // below that does the CHIP's read via subresource request.
  {
    const GURL primary_main_frame_url = embedded_test_server()->GetURL(
        "a.test", "/page_with_blank_iframe.html");
    ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

    const GURL iframe_url = embedded_test_server()->GetURL(
        "b.test", "/page_with_blank_iframe.html");
    ASSERT_TRUE(
        NavigateIframeToURL(GetActiveWebContents(), "test_iframe", iframe_url));

    const GURL nested_iframe_url =
        https_server.GetURL("c.test", "/title1.html");
    NavigateNestedIFrameTo(GetIFrame(), "test_iframe", nested_iframe_url);

    AccessCHIPSViaJSIn(GetNestedIFrame());
  }

  const GURL primary_main_frame_url =
      embedded_test_server()->GetURL("a.test", "/page_with_blank_iframe.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  GURL image_url = https_server.GetURL("c.test", "/favicon/icon.png");
  CreateImageAndWaitForCookieAccess(GetActiveWebContents(), image_url);

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());

  EXPECT_THAT(
      redirects,
      ElementsAre(
          ("[1/1] a.test/page_with_blank_iframe.html -> "
           "a.test/page_with_blank_iframe.html (Read) -> d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_F(BtmBounceDetectorBrowserTest,
                       DiscardFencedFrameCookieClientAccess) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL fenced_frame_url =
      embedded_test_server()->GetURL("a.test", "/fenced_frames/title1.html");
  RenderFrameHostWrapper fenced_frame(
      fenced_frame_test_helper()->CreateFencedFrame(
          GetActiveWebContents()->GetPrimaryMainFrame(), fenced_frame_url));
  EXPECT_FALSE(fenced_frame.IsDestroyed());

  AccessCookieViaJSIn(GetActiveWebContents(), fenced_frame.get());

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(
      redirects,
      ElementsAre(
          ("[1/1] blank -> a.test/title1.html (None) -> d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_F(BtmBounceDetectorBrowserTest,
                       DiscardFencedFrameCookieServerAccess) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL fenced_frame_url = embedded_test_server()->GetURL(
      "a.test", "/fenced_frames/set_cookie_header.html");
  URLCookieAccessObserver observer(GetActiveWebContents(), fenced_frame_url,
                                   CookieOperation::kChange);
  RenderFrameHostWrapper fenced_frame(
      fenced_frame_test_helper()->CreateFencedFrame(
          GetActiveWebContents()->GetPrimaryMainFrame(), fenced_frame_url));
  EXPECT_FALSE(fenced_frame.IsDestroyed());
  observer.Wait();

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(
      redirects,
      ElementsAre(
          "[1/1] blank -> a.test/title1.html (None) -> d.test/title1.html"));
}

// TODO(crbug.com/40917101): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_DiscardPrerenderedPageCookieClientAccess \
  DISABLED_DiscardPrerenderedPageCookieClientAccess
#else
#define MAYBE_DiscardPrerenderedPageCookieClientAccess \
  DiscardPrerenderedPageCookieClientAccess
#endif
IN_PROC_BROWSER_TEST_F(BtmBounceDetectorBrowserTest,
                       MAYBE_DiscardPrerenderedPageCookieClientAccess) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL prerendering_url =
      embedded_test_server()->GetURL("a.test", "/title2.html");
  const FrameTreeNodeId host_id =
      prerender_test_helper()->AddPrerender(prerendering_url);
  prerender_test_helper()->WaitForPrerenderLoadCompletion(prerendering_url);
  test::PrerenderHostObserver observer(*GetActiveWebContents(), host_id);
  EXPECT_FALSE(observer.was_activated());
  RenderFrameHost* prerender_frame =
      prerender_test_helper()->GetPrerenderedMainFrameHost(host_id);
  EXPECT_NE(prerender_frame, nullptr);

  AccessCookieViaJSIn(GetActiveWebContents(), prerender_frame);

  prerender_test_helper()->CancelPrerenderedPage(host_id);
  observer.WaitForDestroyed();

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(
      redirects,
      ElementsAre(
          "[1/1] blank -> a.test/title1.html (None) -> d.test/title1.html"));
}

// TODO(crbug.com/40269306): flaky test.
IN_PROC_BROWSER_TEST_F(BtmBounceDetectorBrowserTest,
                       DISABLED_DiscardPrerenderedPageCookieServerAccess) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL prerendering_url =
      embedded_test_server()->GetURL("a.test", "/set_cookie_header.html");
  URLCookieAccessObserver observer(GetActiveWebContents(), prerendering_url,
                                   CookieOperation::kChange);
  const FrameTreeNodeId host_id =
      prerender_test_helper()->AddPrerender(prerendering_url);
  prerender_test_helper()->WaitForPrerenderLoadCompletion(prerendering_url);
  observer.Wait();

  test::PrerenderHostObserver prerender_observer(*GetActiveWebContents(),
                                                 host_id);
  EXPECT_FALSE(prerender_observer.was_activated());
  prerender_test_helper()->CancelPrerenderedPage(host_id);
  prerender_observer.WaitForDestroyed();

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  // From the time the cookie was set by the prerendering page and now, the
  // primary main page might have accessed (read) the cookie (when sending a
  // request for a favicon after prerendering page already accessed (Write) the
  // cookie). To prevent flakiness we check for any such access and test for the
  // expected outcome accordingly.
  // TODO(crbug.com/40269100): Investigate whether Prerendering pages
  // (same-site) can be use for evasion.
  const std::string expected_access_type =
      observer.CookieAccessedInPrimaryPage() ? "Read" : "None";

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(redirects,
              ElementsAre(("[1/1] blank -> a.test/title1.html (" +
                           expected_access_type + ") -> d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_F(BtmBounceDetectorBrowserTest,
                       DetectStatefulBounce_ClientRedirect_SiteDataAccess) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  // Navigate to the initial page, a.test.
  ASSERT_TRUE(
      NavigateToURL(GetActiveWebContents(),
                    embedded_test_server()->GetURL("a.test", "/title1.html")));

  // Navigate with a click (not considered to be redirect) to b.test.
  ASSERT_TRUE(NavigateToURLFromRenderer(
      GetActiveWebContents(),
      embedded_test_server()->GetURL("b.test", "/title1.html")));

  EXPECT_TRUE(AccessStorage(GetActiveWebContents()->GetPrimaryMainFrame(),
                            StorageTypeAccessed::kLocalStorage));

  // Navigate without a click (considered a client-redirect) to c.test.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(),
      embedded_test_server()->GetURL("c.test", "/title1.html")));

  EndRedirectChain(/*wait=*/false);

  EXPECT_THAT(redirects,
              ElementsAre("[1/1] a.test/title1.html -> b.test/title1.html "
                          "(Write) -> c.test/title1.html"));
}

// The timing of WCO::OnCookiesAccessed() execution is unpredictable for
// redirects. Sometimes it's called before WCO::DidRedirectNavigation(), and
// sometimes after. Therefore BtmBounceDetector needs to know when it's safe to
// judge an HTTP redirect as stateful (accessing cookies) or not. This test
// tries to verify that OnCookiesAccessed() is always called before
// DidFinishNavigation(), so that BtmBounceDetector can safely perform that
// judgement in DidFinishNavigation().
//
// This test also verifies that OnCookiesAccessed() is called for URLs in the
// same order that they're visited (and that for redirects that both read and
// write cookies, OnCookiesAccessed() is called with kRead before it's called
// with kChange, although BtmBounceDetector doesn't depend on that anymore.)
//
// If either assumption is incorrect, this test will be flaky. On 2022-04-27 I
// (rtarpine) ran this test 1000 times in 40 parallel jobs with no failures, so
// it seems robust.
IN_PROC_BROWSER_TEST_F(BtmBounceDetectorBrowserTest,
                       AllCookieCallbacksBeforeNavigationFinished) {
  GURL redirect_url = embedded_test_server()->GetURL(
      "a.test",
      "/cross-site/b.test/cross-site-with-cookie/c.test/cross-site-with-cookie/"
      "d.test/set-cookie?name=value");
  GURL final_url =
      embedded_test_server()->GetURL("d.test", "/set-cookie?name=value");
  WebContents* web_contents = GetActiveWebContents();

  // Set cookies on all 4 test domains
  ASSERT_TRUE(NavigateToSetCookie(web_contents, embedded_test_server(),
                                  "a.test",
                                  /*is_secure_cookie_set=*/false,
                                  /*is_ad_tagged=*/false));
  ASSERT_TRUE(NavigateToSetCookie(web_contents, embedded_test_server(),
                                  "b.test",
                                  /*is_secure_cookie_set=*/false,
                                  /*is_ad_tagged=*/false));
  ASSERT_TRUE(NavigateToSetCookie(web_contents, embedded_test_server(),
                                  "c.test",
                                  /*is_secure_cookie_set=*/false,
                                  /*is_ad_tagged=*/false));
  ASSERT_TRUE(NavigateToSetCookie(web_contents, embedded_test_server(),
                                  "d.test",
                                  /*is_secure_cookie_set=*/false,
                                  /*is_ad_tagged=*/false));

  // Start logging WebContentsObserver callbacks.
  WCOCallbackLogger::CreateForWebContents(web_contents);
  auto* logger = WCOCallbackLogger::FromWebContents(web_contents);

  // Visit the redirect.
  URLCookieAccessObserver observer(web_contents, final_url,
                                   CookieOperation::kChange);
  ASSERT_TRUE(NavigateToURL(web_contents, redirect_url, final_url));
  observer.Wait();

  // Verify that the 7 OnCookiesAccessed() executions are called in order, and
  // all between DidStartNavigation() and DidFinishNavigation().
  //
  // Note: according to web_contents_observer.h, sometimes cookie reads/writes
  // from navigations may cause the RenderFrameHost* overload of
  // OnCookiesAccessed to be called instead. We haven't seen that yet, and this
  // test will intentionally fail if it happens so that we'll notice.
  EXPECT_THAT(
      logger->log(),
      testing::ContainerEq(std::vector<std::string>(
          {("DidStartNavigation(a.test/cross-site/b.test/"
            "cross-site-with-cookie/"
            "c.test/cross-site-with-cookie/d.test/set-cookie)"),
           ("OnCookiesAccessed(NavigationHandle, Read: "
            "a.test/cross-site/b.test/cross-site-with-cookie/c.test/"
            "cross-site-with-cookie/d.test/set-cookie)"),
           ("OnCookiesAccessed(NavigationHandle, Read: "
            "b.test/cross-site-with-cookie/c.test/cross-site-with-cookie/"
            "d.test/"
            "set-cookie)"),
           ("OnCookiesAccessed(NavigationHandle, Change: "
            "b.test/cross-site-with-cookie/c.test/cross-site-with-cookie/"
            "d.test/"
            "set-cookie)"),
           ("OnCookiesAccessed(NavigationHandle, Read: "
            "c.test/cross-site-with-cookie/d.test/set-cookie)"),
           ("OnCookiesAccessed(NavigationHandle, Change: "
            "c.test/cross-site-with-cookie/d.test/set-cookie)"),
           "OnCookiesAccessed(NavigationHandle, Read: d.test/set-cookie)",
           "OnCookiesAccessed(NavigationHandle, Change: d.test/set-cookie)",
           "DidFinishNavigation(d.test/set-cookie)"})));
}

// An EmbeddedTestServer request handler for
// /cross-site-with-samesite-none-cookie URLs. Like /cross-site-with-cookie, but
// the cookie has additional Secure and SameSite=None attributes.
std::unique_ptr<net::test_server::HttpResponse>
HandleCrossSiteSameSiteNoneCookieRedirect(
    net::EmbeddedTestServer* server,
    const net::test_server::HttpRequest& request) {
  const std::string prefix = "/cross-site-with-samesite-none-cookie";
  if (!net::test_server::ShouldHandle(request, prefix)) {
    return nullptr;
  }

  std::string dest_all = base::UnescapeBinaryURLComponent(
      request.relative_url.substr(prefix.size() + 1));

  std::string dest;
  size_t delimiter = dest_all.find("/");
  if (delimiter != std::string::npos) {
    dest = base::StringPrintf(
        "//%s:%hu/%s", dest_all.substr(0, delimiter).c_str(), server->port(),
        dest_all.substr(delimiter + 1).c_str());
  }

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", dest);
  http_response->AddCustomHeader("Set-Cookie",
                                 "server-redirect=true; Secure; SameSite=None");
  http_response->set_content_type("text/html");
  http_response->set_content(base::StringPrintf(
      "<html><head></head><body>Redirecting to %s</body></html>",
      dest.c_str()));
  return http_response;
}

// Ignore iframes because their state will be partitioned under the top-level
// site anyway.
IN_PROC_BROWSER_TEST_F(BtmBounceDetectorBrowserTest,
                       IgnoreServerRedirectsInIframes) {
  // We host the iframe content on an HTTPS server, because for it to write a
  // cookie, the cookie needs to be SameSite=None and Secure.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(kContentTestDataDir);
  https_server.RegisterDefaultHandler(base::BindRepeating(
      &HandleCrossSiteSameSiteNoneCookieRedirect, &https_server));
  ASSERT_TRUE(https_server.Start());

  const GURL root_url =
      embedded_test_server()->GetURL("a.test", "/page_with_blank_iframe.html");
  const GURL redirect_url = https_server.GetURL(
      "b.test", "/cross-site-with-samesite-none-cookie/c.test/title1.html");
  const std::string iframe_id = "test_iframe";
  WebContents* web_contents = GetActiveWebContents();

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  ASSERT_TRUE(NavigateToURL(web_contents, root_url));
  ASSERT_TRUE(NavigateIframeToURL(web_contents, iframe_id, redirect_url));
  EndRedirectChain(/*wait=*/false);

  // b.test had a stateful redirect, but because it was in an iframe, we ignored
  // it.
  EXPECT_THAT(redirects, IsEmpty());
}

// This test verifies that sites in a redirect chain with previous user
// interaction are not reported in the resulting issue when a navigation
// finishes.
IN_PROC_BROWSER_TEST_F(BtmBounceDetectorBrowserTest,
                       ReportRedirectorsInChain_OmitSitesWithInteraction) {
  WebContents* web_contents = GetActiveWebContents();

  std::vector<std::string> reports;
  StartAppendingReportsTo(&reports);

  // Record user activation on d.test.
  GURL url = embedded_test_server()->GetURL("d.test", "/title1.html");

  ASSERT_TRUE(NavigateToURL(web_contents, url));
  SimulateMouseClick();

  // Verify interaction was recorded for d.test, before proceeding.
  std::optional<StateValue> state =
      GetBtmState(GetBtmService(web_contents), url);
  ASSERT_TRUE(state.has_value());
  ASSERT_TRUE(state->user_activation_times.has_value());

  // Visit initial page on a.test.
  ASSERT_TRUE(NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));

  // Navigate with a click (not a redirect) to b.test, which statefully
  // S-redirects to c.test and write a cookie on c.test.
  ASSERT_TRUE(NavigateToURLFromRenderer(
      web_contents,
      embedded_test_server()->GetURL(
          "b.test", "/cross-site-with-cookie/c.test/title1.html"),
      embedded_test_server()->GetURL("c.test", "/title1.html")));
  AccessCookieViaJSIn(web_contents, web_contents->GetPrimaryMainFrame());

  // Navigate without a click (i.e. by C-redirecting) to d.test and write a
  // cookie on d.test:
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, embedded_test_server()->GetURL("d.test", "/title1.html")));
  AccessCookieViaJSIn(web_contents, web_contents->GetPrimaryMainFrame());

  // Navigate without a click (i.e. by C-redirecting) to e.test, which
  // statelessly S-redirects to f.test, which statefully S-redirects to g.test.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents,
      embedded_test_server()->GetURL(
          "e.test",
          "/cross-site/f.test/cross-site-with-cookie/g.test/"
          "title1.html"),
      embedded_test_server()->GetURL("g.test", "/title1.html")));
  EndRedirectChain();
  WaitOnStorage(GetBtmService(web_contents));

  // Verify that d.test is not reported (because it had previous user
  // interaction), but the rest of the chain is reported.
  EXPECT_THAT(reports, ElementsAre(("b.test"), ("c.test"), ("e.test, f.test")));
}

// This test verifies that a third-party cookie access doesn't cause a client
// bounce to be considered stateful.
IN_PROC_BROWSER_TEST_F(
    BtmBounceDetectorBrowserTest,
    DetectStatefulRedirect_Client_IgnoreThirdPartySubresource) {
  // We host the image on an HTTPS server, because for it to read a third-party
  // cookie, it needs to be SameSite=None and Secure.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(kContentTestDataDir);
  https_server.RegisterDefaultHandler(base::BindRepeating(
      &HandleCrossSiteSameSiteNoneCookieRedirect, &https_server));
  ASSERT_TRUE(https_server.Start());

  GURL initial_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL bounce_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL final_url = embedded_test_server()->GetURL("c.test", "/title1.html");
  GURL image_url = https_server.GetURL("d.test", "/favicon/icon.png");
  WebContents* web_contents = GetActiveWebContents();

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  // Start logging WebContentsObserver callbacks.
  WCOCallbackLogger::CreateForWebContents(web_contents);
  auto* logger = WCOCallbackLogger::FromWebContents(web_contents);

  // Set SameSite=None cookie on d.test.
  ASSERT_TRUE(NavigateToURL(
      web_contents, https_server.GetURL(
                        "d.test", "/set-cookie?foo=bar;Secure;SameSite=None")));

  // Visit initial page
  ASSERT_TRUE(NavigateToURL(web_contents, initial_url));
  // Navigate with a click (not a redirect).
  ASSERT_TRUE(NavigateToURLFromRenderer(web_contents, bounce_url));

  // Cause a third-party cookie read.
  CreateImageAndWaitForCookieAccess(web_contents, image_url);
  // Navigate without a click (i.e. by redirecting).
  ASSERT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(web_contents, final_url));

  EXPECT_THAT(logger->log(),
              ElementsAre(
                  // Set cookie on d.test
                  ("DidStartNavigation(d.test/set-cookie)"),
                  ("OnCookiesAccessed(NavigationHandle, "
                   "Change: d.test/set-cookie)"),
                  ("DidFinishNavigation(d.test/set-cookie)"),
                  // Visit a.test
                  ("DidStartNavigation(a.test/title1.html)"),
                  ("DidFinishNavigation(a.test/title1.html)"),
                  // Bounce on b.test (reading third-party d.test cookie)
                  ("DidStartNavigation(b.test/title1.html)"),
                  ("DidFinishNavigation(b.test/title1.html)"),
                  ("OnCookiesAccessed(RenderFrameHost, "
                   "Read: d.test/favicon/icon.png)"),
                  // Land on c.test
                  ("DidStartNavigation(c.test/title1.html)"),
                  ("DidFinishNavigation(c.test/title1.html)")));
  EndRedirectChain(/*wait=*/false);

  // b.test is a bounce, but not stateful.
  EXPECT_THAT(redirects, ElementsAre("[1/1] a.test/title1.html"
                                     " -> b.test/title1.html (None)"
                                     " -> c.test/title1.html"));
}

// This test verifies that a same-site cookie access DOES cause a client
// bounce to be considered stateful.
IN_PROC_BROWSER_TEST_F(BtmBounceDetectorBrowserTest,
                       DetectStatefulRedirect_Client_FirstPartySubresource) {
  GURL initial_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL bounce_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL final_url = embedded_test_server()->GetURL("c.test", "/title1.html");
  GURL image_url =
      embedded_test_server()->GetURL("sub.b.test", "/favicon/icon.png");
  WebContents* web_contents = GetActiveWebContents();

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  // Start logging WebContentsObserver callbacks.
  WCOCallbackLogger::CreateForWebContents(web_contents);
  auto* logger = WCOCallbackLogger::FromWebContents(web_contents);

  // Set cookie on sub.b.test.
  ASSERT_TRUE(NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("sub.b.test", "/set-cookie?foo=bar")));

  // Visit initial page
  ASSERT_TRUE(NavigateToURL(web_contents, initial_url));
  // Navigate with a click (not a redirect).
  ASSERT_TRUE(NavigateToURLFromRenderer(web_contents, bounce_url));

  // Cause a same-site cookie read.
  CreateImageAndWaitForCookieAccess(web_contents, image_url);
  // Navigate without a click (i.e. by redirecting).
  ASSERT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(web_contents, final_url));

  EXPECT_THAT(logger->log(),
              ElementsAre(
                  // Set cookie on sub.b.test
                  ("DidStartNavigation(sub.b.test/set-cookie)"),
                  ("OnCookiesAccessed(NavigationHandle, "
                   "Change: sub.b.test/set-cookie)"),
                  ("DidFinishNavigation(sub.b.test/set-cookie)"),
                  // Visit a.test
                  ("DidStartNavigation(a.test/title1.html)"),
                  ("DidFinishNavigation(a.test/title1.html)"),
                  // Bounce on b.test (reading same-site sub.b.test cookie)
                  ("DidStartNavigation(b.test/title1.html)"),
                  ("DidFinishNavigation(b.test/title1.html)"),
                  ("OnCookiesAccessed(RenderFrameHost, "
                   "Read: sub.b.test/favicon/icon.png)"),
                  // Land on c.test
                  ("DidStartNavigation(c.test/title1.html)"),
                  ("DidFinishNavigation(c.test/title1.html)")));
  EndRedirectChain(/*wait=*/false);

  // b.test IS considered a stateful bounce, even though the cookie was read by
  // an image hosted on sub.b.test.
  EXPECT_THAT(redirects,
              ElementsAre(("[1/1] a.test/title1.html -> b.test/title1.html "
                           "(Read) -> c.test/title1.html")));
}

// This test verifies that consecutive redirect chains are combined into one.
IN_PROC_BROWSER_TEST_F(BtmBounceDetectorBrowserTest,
                       DetectStatefulRedirect_ServerClientClientServer) {
  WebContents* web_contents = GetActiveWebContents();

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  // Visit initial page on a.test
  ASSERT_TRUE(NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));

  // Navigate with a click (not a redirect) to b.test, which S-redirects to
  // c.test
  ASSERT_TRUE(NavigateToURLFromRenderer(
      web_contents,
      embedded_test_server()->GetURL("b.test",
                                     "/cross-site/c.test/title1.html"),
      embedded_test_server()->GetURL("c.test", "/title1.html")));

  // Navigate without a click (i.e. by C-redirecting) to d.test
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, embedded_test_server()->GetURL("d.test", "/title1.html")));

  // Navigate without a click (i.e. by C-redirecting) to e.test, which
  // S-redirects to f.test
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents,
      embedded_test_server()->GetURL("e.test",
                                     "/cross-site/f.test/title1.html"),
      embedded_test_server()->GetURL("f.test", "/title1.html")));
  EndRedirectChain(/*wait=*/false);

  EXPECT_THAT(
      redirects,
      ElementsAre(("[1/4] a.test/title1.html -> "
                   "b.test/cross-site/c.test/title1.html (None) -> "
                   "f.test/title1.html"),
                  ("[2/4] a.test/title1.html -> c.test/title1.html (None) -> "
                   "f.test/title1.html"),
                  ("[3/4] a.test/title1.html -> d.test/title1.html (None) -> "
                   "f.test/title1.html"),
                  ("[4/4] a.test/title1.html -> "
                   "e.test/cross-site/f.test/title1.html (None) -> "
                   "f.test/title1.html")));
}

IN_PROC_BROWSER_TEST_F(BtmBounceDetectorBrowserTest,
                       DetectStatefulRedirect_ClosingTabEndsChain) {
  WebContents* web_contents = GetActiveWebContents();

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  // Visit initial page on a.test
  ASSERT_TRUE(NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));

  // Navigate with a click (not a redirect) to b.test, which S-redirects to
  // c.test
  ASSERT_TRUE(NavigateToURLFromRenderer(
      web_contents,
      embedded_test_server()->GetURL("b.test",
                                     "/cross-site/c.test/title1.html"),
      embedded_test_server()->GetURL("c.test", "/title1.html")));

  EXPECT_THAT(redirects, IsEmpty());

  CloseTab(web_contents);

  EXPECT_THAT(redirects,
              ElementsAre(("[1/1] a.test/title1.html -> "
                           "b.test/cross-site/c.test/title1.html (None) -> "
                           "c.test/title1.html")));
}

// Verifies server redirects that occur while opening a link in a new tab are
// properly detected.
IN_PROC_BROWSER_TEST_F(BtmBounceDetectorBrowserTest,
                       OpenServerRedirectURLInNewTab) {
  WebContents* original_tab = GetActiveWebContents();
  GURL original_tab_url(
      embedded_test_server()->GetURL("a.test", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(original_tab, original_tab_url));

  // Open a server-redirecting link in a new tab.
  GURL new_tab_url(embedded_test_server()->GetURL(
      "b.test", "/cross-site-with-cookie/c.test/title1.html"));
  ASSERT_OK_AND_ASSIGN(WebContents * new_tab,
                       OpenInNewTab(original_tab, new_tab_url));

  // Verify the tab is different from the original and at the correct URL.
  EXPECT_NE(new_tab, original_tab);
  ASSERT_EQ(new_tab->GetLastCommittedURL(),
            embedded_test_server()->GetURL("c.test", "/title1.html"));

  ASSERT_TRUE(WaitForRedirectCookieWrite(new_tab, new_tab_url));

  std::vector<std::string> redirects;
  RedirectChainDetector* tab_web_contents_observer =
      RedirectChainDetector::FromWebContents(new_tab);
  tab_web_contents_observer->SetRedirectChainHandlerForTesting(
      base::BindRepeating(&AppendRedirects, &redirects));

  WebContentsDestroyedWatcher watcher(new_tab);
  new_tab->Close();
  watcher.Wait();

  EXPECT_THAT(redirects,
              ElementsAre((
                  "[1/1] a.test/ -> " /* Note: the URL's path is lost here. */
                  "b.test/cross-site-with-cookie/c.test/title1.html (Write) -> "
                  "c.test/title1.html")));
}

// Verifies client redirects that occur while opening a link in a new tab are
// properly detected.
IN_PROC_BROWSER_TEST_F(BtmBounceDetectorBrowserTest,
                       OpenClientRedirectURLInNewTab) {
  WebContents* original_tab = GetActiveWebContents();
  GURL original_tab_url(
      embedded_test_server()->GetURL("a.test", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(original_tab, original_tab_url));

  // Open link in a new tab.
  GURL new_tab_url(embedded_test_server()->GetURL("b.test", "/title1.html"));
  ASSERT_OK_AND_ASSIGN(WebContents * new_tab,
                       OpenInNewTab(original_tab, new_tab_url));

  // Verify the tab is different from the original and at the correct URL.
  EXPECT_NE(original_tab, new_tab);
  ASSERT_EQ(new_tab_url, new_tab->GetLastCommittedURL());

  std::vector<std::string> redirects;
  RedirectChainDetector* tab_web_contents_observer =
      RedirectChainDetector::FromWebContents(new_tab);
  tab_web_contents_observer->SetRedirectChainHandlerForTesting(
      base::BindRepeating(&AppendRedirects, &redirects));

  // Navigate without a click (i.e. by C-redirecting) to c.test.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      new_tab, embedded_test_server()->GetURL("c.test", "/title1.html")));
  WebContentsDestroyedWatcher watcher(new_tab);
  new_tab->Close();
  watcher.Wait();

  EXPECT_THAT(
      redirects,
      ElementsAre(("[1/1] a.test/ -> " /* Note: the URL's path is lost here. */
                   "b.test/title1.html (None) -> "
                   "c.test/title1.html")));
}

// Verifies the start URL of a redirect chain started by opening a link in a new
// tab is handled correctly, when that start page has an opaque origin.
IN_PROC_BROWSER_TEST_F(BtmBounceDetectorBrowserTest,
                       OpenRedirectURLInNewTab_OpaqueOriginInitiator) {
  WebContents* original_tab = GetActiveWebContents();
  GURL original_tab_url("data:text/html,<html></html>");
  ASSERT_TRUE(NavigateToURL(original_tab, original_tab_url));

  // Open a server-redirecting link in a new tab.
  GURL new_tab_url(embedded_test_server()->GetURL(
      "b.test", "/cross-site-with-cookie/c.test/title1.html"));
  ASSERT_OK_AND_ASSIGN(WebContents * new_tab,
                       OpenInNewTab(original_tab, new_tab_url));

  // Verify the tab is different from the original and at the correct URL.
  EXPECT_NE(new_tab, original_tab);
  ASSERT_EQ(new_tab->GetLastCommittedURL(),
            embedded_test_server()->GetURL("c.test", "/title1.html"));

  ASSERT_TRUE(WaitForRedirectCookieWrite(new_tab, new_tab_url));

  std::vector<std::string> redirects;
  RedirectChainDetector::FromWebContents(new_tab)
      ->SetRedirectChainHandlerForTesting(
          base::BindRepeating(&AppendRedirects, &redirects));

  WebContentsDestroyedWatcher watcher(new_tab);
  new_tab->Close();
  watcher.Wait();

  EXPECT_THAT(redirects,
              ElementsAre((
                  "[1/1] blank -> "
                  "b.test/cross-site-with-cookie/c.test/title1.html (Write) -> "
                  "c.test/title1.html")));
}

class RedirectHeuristicBrowserTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void PreRunTestOnMainThread() override {
    ContentBrowserTest::PreRunTestOnMainThread();
    ukm::InitializeSourceUrlRecorderForWebContents(GetActiveWebContents());
    browser_client_.emplace();
  }

  WebContents* GetActiveWebContents() { return shell()->web_contents(); }

  // Perform a browser-based navigation to terminate the current redirect chain.
  void EndRedirectChain() {
    ASSERT_TRUE(NavigateToURL(
        GetActiveWebContents(),
        embedded_test_server()->GetURL("endthechain.test", "/title1.html")));
  }

  void SimulateMouseClick() {
    SimulateMouseClickAndWait(GetActiveWebContents());
  }

  void SimulateWebAuthnAssertion() {
    WebAuthnAssertionRequestSucceeded(
        GetActiveWebContents()->GetPrimaryMainFrame());
  }

  TpcBlockingBrowserClient& browser_client() { return browser_client_->impl(); }

 private:
  std::optional<ContentBrowserTestTpcBlockingBrowserClient> browser_client_;
};

// Tests the conditions for recording RedirectHeuristic_CookieAccess2 and
// RedirectHeuristic_CookieAccessThirdParty2 UKM events.
// TODO(crbug.com/369920781): Flaky
IN_PROC_BROWSER_TEST_F(RedirectHeuristicBrowserTest,
                       DISABLED_RecordsRedirectHeuristicCookieAccessEvent) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  WebContents* web_contents = GetActiveWebContents();

  // We host the "image" on an HTTPS server, because for it to write a
  // cookie, the cookie needs to be SameSite=None and Secure.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(GetTestDataFilePath());
  ASSERT_TRUE(https_server.Start());

  GURL initial_url = embedded_test_server()->GetURL("a.test", "/title1.html");

  GURL tracker_url_pre_target_redirect =
      embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL image_url_pre_target_redirect =
      https_server.GetURL("sub.b.test", "/favicon/icon.png");

  GURL target_url = embedded_test_server()->GetURL("d.test", "/title1.html");
  GURL target_image_url =
      https_server.GetURL("sub.d.test", "/favicon/icon.png");

  GURL tracker_url_post_target_redirect =
      embedded_test_server()->GetURL("c.test", "/title1.html");
  GURL image_url_post_target_redirect =
      https_server.GetURL("sub.c.test", "/favicon/icon.png");

  GURL final_url = embedded_test_server()->GetURL("f.test", "/title1.html");

  browser_client().AllowThirdPartyCookiesOnSite(target_url);

  // Set cookies on image URLs.
  ASSERT_TRUE(NavigateToSetCookie(web_contents, &https_server, "sub.b.test",
                                  /*is_secure_cookie_set=*/true,
                                  /*is_ad_tagged=*/false));
  ASSERT_TRUE(NavigateToSetCookie(web_contents, &https_server, "sub.c.test",
                                  /*is_secure_cookie_set=*/true,
                                  /*is_ad_tagged=*/false));
  ASSERT_TRUE(NavigateToSetCookie(web_contents, &https_server, "sub.d.test",
                                  /*is_secure_cookie_set=*/true,
                                  /*is_ad_tagged=*/false));

  // Visit initial page.
  ASSERT_TRUE(NavigateToURL(web_contents, initial_url));
  // Redirect to tracking URL.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, tracker_url_pre_target_redirect));

  // Redirect to target URL.
  ASSERT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(web_contents, target_url));
  // Read a cookie from the tracking URL.
  CreateImageAndWaitForCookieAccess(web_contents,
                                    image_url_pre_target_redirect);
  // Read a cookie from the second tracking URL.
  CreateImageAndWaitForCookieAccess(web_contents,
                                    image_url_post_target_redirect);
  // Read a cookie from an image with the same domain as the target URL.
  CreateImageAndWaitForCookieAccess(web_contents, target_image_url);

  // Redirect to second tracking URL. (This has no effect since the cookie
  // accesses already happened.)
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, tracker_url_post_target_redirect));
  // Redirect to final URL.
  ASSERT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(web_contents, final_url));

  EndRedirectChain();

  std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
      ukm_first_party_entries =
          ukm_recorder.GetEntries("RedirectHeuristic.CookieAccess2", {});

  // Expect one UKM entry.

  // Include the cookies read where a tracking site read cookies while embedded
  // on a site later in the redirect chain.

  // Exclude the cookies reads where:
  // - The tracking site did not appear in the prior redirect chain.
  // - The tracking and target sites had the same domain.
  ASSERT_EQ(1u, ukm_first_party_entries.size());
  EXPECT_THAT(
      ukm_recorder.GetSourceForSourceId(ukm_first_party_entries[0].source_id)
          ->url(),
      Eq(target_url));

  // Expect one corresponding UKM entry for CookieAccessThirdParty.
  std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
      ukm_third_party_entries = ukm_recorder.GetEntries(
          "RedirectHeuristic.CookieAccessThirdParty2", {});
  ASSERT_EQ(1u, ukm_third_party_entries.size());
  EXPECT_THAT(
      ukm_recorder.GetSourceForSourceId(ukm_third_party_entries[0].source_id)
          ->url(),
      Eq(tracker_url_pre_target_redirect));
}

// Tests setting different metrics for the RedirectHeuristic_CookieAccess2 UKM
// event.
// TODO(crbug.com/40934961): Flaky on multiple platforms.
IN_PROC_BROWSER_TEST_F(RedirectHeuristicBrowserTest,
                       DISABLED_RedirectHeuristicCookieAccessEvent_AllMetrics) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  WebContents* web_contents = GetActiveWebContents();

  // We host the "image" on an HTTPS server, because for it to write a
  // cookie, the cookie needs to be SameSite=None and Secure.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(GetTestDataFilePath());
  ASSERT_TRUE(https_server.Start());

  GURL final_url = embedded_test_server()->GetURL("a.test", "/title1.html");

  GURL tracker_url_with_user_activation_interaction =
      embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL image_url_with_user_activation_interaction =
      https_server.GetURL("sub.b.test", "/favicon/icon.png");

  GURL tracker_url_in_iframe =
      embedded_test_server()->GetURL("c.test", "/title1.html");
  GURL image_url_in_iframe =
      https_server.GetURL("sub.c.test", "/favicon/icon.png");

  GURL tracker_url_with_authentication_interaction =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  GURL image_url_with_authentication_interaction =
      https_server.GetURL("sub.d.test", "/favicon/icon.png");

  GURL target_url_3pc_allowed =
      embedded_test_server()->GetURL("e.test", "/title1.html");
  GURL target_url_3pc_blocked =
      embedded_test_server()->GetURL("f.test", "/page_with_blank_iframe.html");

  browser_client().AllowThirdPartyCookiesOnSite(target_url_3pc_allowed);
  browser_client().BlockThirdPartyCookiesOnSite(target_url_3pc_blocked);

  // Set cookies on image URLs.
  ASSERT_TRUE(NavigateToSetCookie(web_contents, &https_server, "sub.b.test",
                                  /*is_secure_cookie_set=*/true,
                                  /*is_ad_tagged=*/true));
  ASSERT_TRUE(NavigateToSetCookie(web_contents, &https_server, "sub.c.test",
                                  /*is_secure_cookie_set=*/true,
                                  /*is_ad_tagged=*/false));
  ASSERT_TRUE(NavigateToSetCookie(web_contents, &https_server, "sub.d.test",
                                  /*is_secure_cookie_set=*/true,
                                  /*is_ad_tagged=*/false));

  // Start on `tracker_url_with_user_activation_interaction` and record a
  // current user activation interaction.
  ASSERT_TRUE(NavigateToURL(web_contents,
                            tracker_url_with_user_activation_interaction));
  SimulateMouseClick();

  // Redirect to on `tracker_url_with_authentication_interaction` and record a
  // current authentication interaction.
  ASSERT_TRUE(
      NavigateToURL(web_contents, tracker_url_with_authentication_interaction));
  SimulateWebAuthnAssertion();

  // Redirect to one of the target URLs, to set DoesFirstPartyPrecedeThirdParty.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, target_url_3pc_blocked));
  // Redirect to all tracking URLs.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, tracker_url_in_iframe));
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, tracker_url_with_user_activation_interaction));
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, tracker_url_with_authentication_interaction));

  // Redirect to target URL with cookies allowed.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, target_url_3pc_allowed));
  // Read a cookie from the tracking URL with user activation interaction.
  CreateImageAndWaitForCookieAccess(
      web_contents,
      https_server.GetURL("sub.b.test", "/favicon/icon.png?isad=1"));

  // Read a cookie from the tracking URL with authentication interaction.
  CreateImageAndWaitForCookieAccess(web_contents,
                                    image_url_with_authentication_interaction);

  // Redirect to target URL with cookies blocked.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, target_url_3pc_blocked));
  // Open an iframe of the tracking URL on the target URL.
  ASSERT_TRUE(NavigateIframeToURL(web_contents,
                                  /*iframe_id=*/"test_iframe",
                                  image_url_in_iframe));
  // Read a cookie from the tracking URL in an iframe on the target page.
  CreateImageAndWaitForCookieAccess(web_contents, image_url_in_iframe);

  // Redirect to final URL.
  ASSERT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(web_contents, final_url));

  EndRedirectChain();

  std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> ukm_entries =
      ukm_recorder.GetEntries(
          "RedirectHeuristic.CookieAccess2",
          {"AccessId", "AccessAllowed", "IsAdTagged",
           "HoursSinceLastInteraction", "MillisecondsSinceRedirect",
           "OpenerHasSameSiteIframe", "SitesPassedCount",
           "DoesFirstPartyPrecedeThirdParty", "IsCurrentInteraction",
           "InteractionType"});

  // Expect UKM entries from all three cookie accesses.
  ASSERT_EQ(3u, ukm_entries.size());

  // Expect reasonable delays between the redirect and cookie access.
  for (const auto& entry : ukm_entries) {
    EXPECT_GT(entry.metrics.at("MillisecondsSinceRedirect"), 0);
    EXPECT_LT(entry.metrics.at("MillisecondsSinceRedirect"), 1000);
  }

  // The first cookie access was from a tracking site with a user activation
  // interaction within the last hour, on a site with 3PC access allowed.

  // 2 site were passed: tracker_url_with_user_activation_interaction ->
  // tracker_url_with_authentication_interaction -> target_url_3pc_allowed
  auto access_id_1 = ukm_entries[0].metrics.at("AccessId");
  EXPECT_THAT(
      ukm_recorder.GetSourceForSourceId(ukm_entries[0].source_id)->url(),
      Eq(target_url_3pc_allowed));
  EXPECT_EQ(ukm_entries[0].metrics.at("AccessAllowed"), true);
  EXPECT_EQ(ukm_entries[0].metrics.at("IsAdTagged"),
            static_cast<int32_t>(OptionalBool::kTrue));
  EXPECT_EQ(ukm_entries[0].metrics.at("HoursSinceLastInteraction"), 0);
  EXPECT_EQ(ukm_entries[0].metrics.at("OpenerHasSameSiteIframe"),
            static_cast<int32_t>(OptionalBool::kFalse));
  EXPECT_EQ(ukm_entries[0].metrics.at("SitesPassedCount"), 2);
  EXPECT_EQ(ukm_entries[0].metrics.at("DoesFirstPartyPrecedeThirdParty"),
            false);
  EXPECT_EQ(ukm_entries[0].metrics.at("IsCurrentInteraction"), 1);
  EXPECT_EQ(ukm_entries[0].metrics.at("InteractionType"),
            static_cast<int32_t>(BtmInteractionType::UserActivation));

  // The second cookie access was from a tracking site with an authentication
  // within the last hour, on a site with 3PC access allowed.

  // 1 site was passed: tracker_url_with_authentication_interaction ->
  // target_url_3pc_allowed
  auto access_id_2 = ukm_entries[1].metrics.at("AccessId");
  EXPECT_THAT(
      ukm_recorder.GetSourceForSourceId(ukm_entries[1].source_id)->url(),
      Eq(target_url_3pc_allowed));
  EXPECT_EQ(ukm_entries[1].metrics.at("AccessAllowed"), true);
  EXPECT_EQ(ukm_entries[0].metrics.at("IsAdTagged"),
            static_cast<int32_t>(OptionalBool::kFalse));
  EXPECT_EQ(ukm_entries[1].metrics.at("HoursSinceLastInteraction"), 0);
  EXPECT_EQ(ukm_entries[1].metrics.at("OpenerHasSameSiteIframe"),
            static_cast<int32_t>(OptionalBool::kFalse));
  EXPECT_EQ(ukm_entries[1].metrics.at("SitesPassedCount"), 1);
  EXPECT_EQ(ukm_entries[1].metrics.at("DoesFirstPartyPrecedeThirdParty"),
            false);
  EXPECT_EQ(ukm_entries[1].metrics.at("IsCurrentInteraction"), 1);
  EXPECT_EQ(ukm_entries[1].metrics.at("InteractionType"),
            static_cast<int32_t>(BtmInteractionType::Authentication));

  // The third cookie access was from a tracking site in an iframe of the
  // target, on a site with 3PC access blocked.

  // 4 sites were passed: tracker_url_in_iframe ->
  // tracker_url_with_user_activation_interaction
  // -> tracker_url_with_authentication_interaction -> target_url_3pc_allowed ->
  // target_url_3pc_blocked
  auto access_id_3 = ukm_entries[2].metrics.at("AccessId");
  EXPECT_THAT(
      ukm_recorder.GetSourceForSourceId(ukm_entries[2].source_id)->url(),
      Eq(target_url_3pc_blocked));
  EXPECT_EQ(ukm_entries[2].metrics.at("AccessAllowed"), false);
  EXPECT_EQ(ukm_entries[2].metrics.at("IsAdTagged"),
            static_cast<int32_t>(OptionalBool::kFalse));
  EXPECT_EQ(ukm_entries[2].metrics.at("HoursSinceLastInteraction"), -1);
  EXPECT_EQ(ukm_entries[2].metrics.at("OpenerHasSameSiteIframe"),
            static_cast<int32_t>(OptionalBool::kTrue));
  EXPECT_EQ(ukm_entries[2].metrics.at("SitesPassedCount"), 4);
  EXPECT_EQ(ukm_entries[2].metrics.at("DoesFirstPartyPrecedeThirdParty"), true);
  EXPECT_EQ(ukm_entries[2].metrics.at("IsCurrentInteraction"), 0);
  EXPECT_EQ(ukm_entries[2].metrics.at("InteractionType"),
            static_cast<int32_t>(BtmInteractionType::NoInteraction));

  // Verify there are 3 corresponding CookieAccessThirdParty entries with
  // matching access IDs.
  std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
      ukm_third_party_entries = ukm_recorder.GetEntries(
          "RedirectHeuristic.CookieAccessThirdParty2", {"AccessId"});
  ASSERT_EQ(3u, ukm_third_party_entries.size());

  EXPECT_THAT(
      ukm_recorder.GetSourceForSourceId(ukm_third_party_entries[0].source_id)
          ->url(),
      Eq(tracker_url_with_user_activation_interaction));
  EXPECT_EQ(ukm_third_party_entries[0].metrics.at("AccessId"), access_id_1);

  EXPECT_THAT(
      ukm_recorder.GetSourceForSourceId(ukm_third_party_entries[1].source_id)
          ->url(),
      Eq(tracker_url_with_authentication_interaction));
  EXPECT_EQ(ukm_third_party_entries[1].metrics.at("AccessId"), access_id_2);

  EXPECT_THAT(
      ukm_recorder.GetSourceForSourceId(ukm_third_party_entries[2].source_id)
          ->url(),
      Eq(tracker_url_in_iframe));
  EXPECT_EQ(ukm_third_party_entries[2].metrics.at("AccessId"), access_id_3);
}

struct RedirectHeuristicFlags {
  bool write_redirect_grants = false;
  bool require_aba_flow = true;
  bool require_current_interaction = true;
  bool user_activation_interaction = true;
};

// chrome/browser/ui/browser.h (for changing profile prefs) is not available on
// Android.
#if !BUILDFLAG(IS_ANDROID)
class RedirectHeuristicGrantTest
    : public RedirectHeuristicBrowserTest,
      public testing::WithParamInterface<RedirectHeuristicFlags> {
 public:
  RedirectHeuristicGrantTest() {
    std::string grant_time_string =
        GetParam().write_redirect_grants ? "60s" : "0s";
    std::string require_aba_flow_string =
        base::ToString(GetParam().require_aba_flow);
    std::string require_current_interaction_string =
        base::ToString(GetParam().require_current_interaction);

    enabled_features_.push_back(
        {content_settings::features::kTpcdHeuristicsGrants,
         {{"TpcdReadHeuristicsGrants", "true"},
          {"TpcdWriteRedirectHeuristicGrants", grant_time_string},
          {"TpcdRedirectHeuristicRequireABAFlow", require_aba_flow_string},
          {"TpcdRedirectHeuristicRequireCurrentInteraction",
           require_current_interaction_string}}});
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features_,
                                                       disabled_features_);
    RedirectHeuristicBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Prevents flakiness by handling clicks even before content is drawn.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  void SetUpOnMainThread() override {
    RedirectHeuristicBrowserTest::SetUpOnMainThread();

    browser_client_.emplace();
    browser_client().SetBlockThirdPartyCookiesByDefault(true);
    WebContents* web_contents = GetActiveWebContents();
    ASSERT_FALSE(btm::Are3PcsGenerallyEnabled(web_contents->GetBrowserContext(),
                                              web_contents));
  }

  TpcBlockingBrowserClient& browser_client() { return browser_client_->impl(); }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::vector<base::test::FeatureRefAndParams> enabled_features_;
  std::vector<base::test::FeatureRef> disabled_features_;

 private:
  std::optional<ContentBrowserTestTpcBlockingBrowserClient> browser_client_;
};

IN_PROC_BROWSER_TEST_P(RedirectHeuristicGrantTest,
                       CreatesRedirectHeuristicGrantsWithSatisfyingURL) {
  WebContents* web_contents = GetActiveWebContents();

  // Initialize first party URL and two trackers.
  GURL first_party_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL aba_current_interaction_url =
      embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL no_interaction_url =
      embedded_test_server()->GetURL("c.test", "/title1.html");

  // Start on `first_party_url`.
  ASSERT_TRUE(NavigateToURL(web_contents, first_party_url));

  // Navigate to `aba_current_interaction_url` and record a current interaction.
  ASSERT_TRUE(NavigateToURL(web_contents, aba_current_interaction_url));
  SimulateMouseClick();

  // Redirect through `first_party_url`, `aba_current_interaction_url`, and
  // `no_interaction_url` before committing and ending on `first_party_url`.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                          first_party_url));
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, aba_current_interaction_url));
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                          no_interaction_url));
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                          first_party_url));
  EndRedirectChain();

  // Wait on async tasks for the grants to be created.
  WaitOnStorage(GetBtmService(web_contents));

  // Expect some cookie grants on `first_party_url` based on flags and criteria.
  EXPECT_EQ(browser_client().IsFullCookieAccessAllowed(
                web_contents->GetBrowserContext(), web_contents,
                aba_current_interaction_url,
                blink::StorageKey::CreateFirstParty(
                    url::Origin::Create(first_party_url)),
                /*overrides=*/{}),
            GetParam().write_redirect_grants);

  EXPECT_FALSE(browser_client().IsFullCookieAccessAllowed(
      web_contents->GetBrowserContext(), web_contents, no_interaction_url,
      blink::StorageKey::CreateFirstParty(url::Origin::Create(first_party_url)),
      /*overrides=*/{}));
}

IN_PROC_BROWSER_TEST_P(
    RedirectHeuristicGrantTest,
    CreatesRedirectHeuristicGrantsWithPartiallySatisfyingURL) {
  WebContents* web_contents = GetActiveWebContents();

  // Initialize first party URL and two trackers.
  GURL first_party_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL aba_past_interaction_url =
      embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL no_aba_current_interaction_url =
      embedded_test_server()->GetURL("c.test", "/title1.html");

  // Record a past interaction on `aba_past_interaction_url`.
  ASSERT_TRUE(NavigateToURL(web_contents, aba_past_interaction_url));
  SimulateMouseClick();

  // Start redirect chain on `no_aba_current_interaction_url` and record a
  // current interaction.
  ASSERT_TRUE(NavigateToURL(web_contents, no_aba_current_interaction_url));
  SimulateMouseClick();

  // Redirect through `no_aba_current_interaction_url`, `first_party_url`, and
  // `aba_past_interaction_url` before committing and ending on
  // `first_party_url`.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, no_aba_current_interaction_url));
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                          first_party_url));
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, aba_past_interaction_url));
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                          first_party_url));
  EndRedirectChain();

  // Wait on async tasks for the grants to be created.
  WaitOnStorage(GetBtmService(web_contents));

  // Expect some cookie grants on `first_party_url` based on flags and criteria.
  EXPECT_EQ(browser_client().IsFullCookieAccessAllowed(
                web_contents->GetBrowserContext(), web_contents,
                aba_past_interaction_url,
                blink::StorageKey::CreateFirstParty(
                    url::Origin::Create(first_party_url)),
                /*overrides=*/{}),
            GetParam().write_redirect_grants &&
                !GetParam().require_current_interaction);
  EXPECT_EQ(browser_client().IsFullCookieAccessAllowed(
                web_contents->GetBrowserContext(), web_contents,
                no_aba_current_interaction_url,
                blink::StorageKey::CreateFirstParty(
                    url::Origin::Create(first_party_url)),
                /*overrides=*/{}),
            GetParam().write_redirect_grants && !GetParam().require_aba_flow);
}

IN_PROC_BROWSER_TEST_P(RedirectHeuristicGrantTest,
                       CreatesRedirectHeuristicGrantsWithWebAuthnInteractions) {
  WebContents* web_contents = GetActiveWebContents();

  // Initialize first party URL and two trackers.
  GURL first_party_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL past_interaction_url =
      embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL current_interaction_url =
      embedded_test_server()->GetURL("c.test", "/title1.html");

  // Record a past web authentication interaction on `past_interaction_url`.
  ASSERT_TRUE(NavigateToURL(web_contents, past_interaction_url));
  SimulateWebAuthnAssertion();

  // Start redirect chain on `first_party_url` with an interaction that simulate
  // a user starting the authentication process
  ASSERT_TRUE(NavigateToURL(web_contents, first_party_url));
  SimulateMouseClick();

  // Navigate through 'past_interaction_url', 'current_interaction_url' with a
  // web authentication interaction, and back to 'first_party_url'
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, past_interaction_url));
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, current_interaction_url));
  SimulateWebAuthnAssertion();
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                          first_party_url));
  EndRedirectChain();

  // Wait on async tasks for the grants to be created.
  WaitOnStorage(GetBtmService(web_contents));

  // Expect some cookie grants on `first_party_url` based on flags and criteria.
  EXPECT_EQ(
      browser_client().IsFullCookieAccessAllowed(
          web_contents->GetBrowserContext(), web_contents, past_interaction_url,
          blink::StorageKey::CreateFirstParty(
              url::Origin::Create(first_party_url)),
          /*overrides=*/{}),
      GetParam().write_redirect_grants &&
          !GetParam().require_current_interaction);
  EXPECT_EQ(browser_client().IsFullCookieAccessAllowed(
                web_contents->GetBrowserContext(), web_contents,
                current_interaction_url,
                blink::StorageKey::CreateFirstParty(
                    url::Origin::Create(first_party_url)),
                /*overrides=*/{}),
            GetParam().write_redirect_grants && !GetParam().require_aba_flow);
}

IN_PROC_BROWSER_TEST_F(BtmBounceDetectorBrowserTest,
                       RedirectInfoHttpStatusPersistence) {
  WebContents* const web_contents = GetActiveWebContents();

  // The "final" URL will not have any server redirects.
  GURL final_url = embedded_test_server()->GetURL("/echo");
  // The "302" and "303" URLs will have a server redirect to the final URL,
  // giving a 302 and 303 HTTP response code status, respectively.
  GURL redirect_303 = embedded_test_server()->GetURL("/server-redirect-303?" +
                                                     final_url.spec());
  GURL redirect_302 = embedded_test_server()->GetURL("/server-redirect-302?" +
                                                     final_url.spec());
  // The "301" URL will give a 301 response code and redirect to the "302" URL.
  GURL redirect_301 = embedded_test_server()->GetURL("/server-redirect-301?" +
                                                     redirect_302.spec());

  // Navigate to a URL that will give a 301 redirect to another URL that will
  // give a 302 redirect, before settling on a third URL.
  ASSERT_TRUE(NavigateToURL(web_contents, redirect_301, final_url));

  // Do client redirect to a URL that gives a 303 redirect.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, redirect_303, final_url));

  RedirectChainDetector* wco =
      RedirectChainDetector::FromWebContents(web_contents);
  const BtmRedirectContext& context = wco->CommittedRedirectContext();

  ASSERT_EQ(context.size(), 4u);

  EXPECT_EQ(context[0].response_code, 301);
  EXPECT_EQ(context[1].response_code, 302);
  // The client redirect does not have an explicit HTTP response status.
  EXPECT_EQ(context[2].response_code, 0);
  EXPECT_EQ(context[3].response_code, 303);
}

const RedirectHeuristicFlags kRedirectHeuristicTestCases[] = {
    {
        .write_redirect_grants = false,
    },
    {
        .write_redirect_grants = true,
        .require_aba_flow = true,
        .require_current_interaction = true,
    },
    {
        .write_redirect_grants = true,
        .require_aba_flow = false,
        .require_current_interaction = true,
    },
    {
        .write_redirect_grants = true,
        .require_aba_flow = true,
        .require_current_interaction = false,
    },
    {
        .write_redirect_grants = true,
        .require_aba_flow = false,
        .require_current_interaction = false,
        .user_activation_interaction = false,
    },
};

INSTANTIATE_TEST_SUITE_P(All,
                         RedirectHeuristicGrantTest,
                         ::testing::ValuesIn(kRedirectHeuristicTestCases));
#endif  // !BUILDFLAG(IS_ANDROID)

class BtmSiteDataAccessDetectorTest
    : public BtmBounceDetectorBrowserTest,
      public testing::WithParamInterface<StorageTypeAccessed> {
 public:
  BtmSiteDataAccessDetectorTest(const BtmSiteDataAccessDetectorTest&) = delete;
  BtmSiteDataAccessDetectorTest& operator=(
      const BtmSiteDataAccessDetectorTest&) = delete;

  BtmSiteDataAccessDetectorTest() = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server().SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    embedded_https_test_server().AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(embedded_https_test_server().Start());
    SetUpBtmWebContentsObserver();
  }
};

IN_PROC_BROWSER_TEST_P(BtmSiteDataAccessDetectorTest,
                       DetectSiteDataAccess_Storages) {
  // Start logging `WebContentsObserver` callbacks.
  WCOCallbackLogger::CreateForWebContents(GetActiveWebContents());
  auto* logger = WCOCallbackLogger::FromWebContents(GetActiveWebContents());

  ASSERT_TRUE(NavigateToURL(
      GetActiveWebContents(),
      embedded_https_test_server().GetURL("a.test", "/title1.html")));

  ASSERT_TRUE(
      AccessStorage(GetActiveWebContents()->GetPrimaryMainFrame(), GetParam()));

  ASSERT_THAT(
      logger->log(),
      testing::ContainerEq(std::vector<std::string>(
          {"DidStartNavigation(a.test/title1.html)",
           "DidFinishNavigation(a.test/title1.html)",
           base::StringPrintf("NotifyStorageAccessed(%s: a.test/title1.html)",
                              base::ToString(GetParam()).c_str())})));
}

IN_PROC_BROWSER_TEST_P(BtmSiteDataAccessDetectorTest,
                       AttributeSameSiteIframesSiteDataAccessTo1P) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url = embedded_https_test_server().GetURL(
      "a.test", "/page_with_blank_iframe.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL iframe_url =
      embedded_https_test_server().GetURL("a.test", "/title1.html");
  ASSERT_TRUE(
      NavigateIframeToURL(GetActiveWebContents(), "test_iframe", iframe_url));

  EXPECT_TRUE(AccessStorage(GetIFrame(), GetParam()));

  const GURL primary_main_frame_final_url =
      embedded_https_test_server().GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(redirects,
              ElementsAre(("[1/1] blank -> a.test/page_with_blank_iframe.html "
                           "(Write) -> d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_P(BtmSiteDataAccessDetectorTest,
                       AttributeSameSiteNestedIframesSiteDataAccessTo1P) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url = embedded_https_test_server().GetURL(
      "a.test", "/page_with_blank_iframe.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL iframe_url = embedded_https_test_server().GetURL(
      "a.test", "/page_with_blank_iframe.html");
  ASSERT_TRUE(
      NavigateIframeToURL(GetActiveWebContents(), "test_iframe", iframe_url));

  const GURL nested_iframe_url =
      embedded_https_test_server().GetURL("a.test", "/title1.html");
  NavigateNestedIFrameTo(GetIFrame(), "test_iframe", nested_iframe_url);

  EXPECT_TRUE(AccessStorage(GetNestedIFrame(), GetParam()));

  const GURL primary_main_frame_final_url =
      embedded_https_test_server().GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(redirects,
              ElementsAre(("[1/1] blank -> a.test/page_with_blank_iframe.html "
                           "(Write) -> d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_P(BtmSiteDataAccessDetectorTest,
                       DiscardFencedFrameCookieClientAccess) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      embedded_https_test_server().GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL fenced_frame_url = embedded_https_test_server().GetURL(
      "a.test", "/fenced_frames/title0.html");
  std::unique_ptr<RenderFrameHostWrapper> fenced_frame =
      std::make_unique<RenderFrameHostWrapper>(
          fenced_frame_test_helper()->CreateFencedFrame(
              GetActiveWebContents()->GetPrimaryMainFrame(), fenced_frame_url));
  EXPECT_NE(fenced_frame, nullptr);

  EXPECT_TRUE(AccessStorage(fenced_frame->get(), GetParam()));

  const GURL primary_main_frame_final_url =
      embedded_https_test_server().GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(
      redirects,
      ElementsAre(
          ("[1/1] blank -> a.test/title1.html (None) -> d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_P(BtmSiteDataAccessDetectorTest,
                       DiscardPrerenderedPageCookieClientAccess) {
  // Prerendering pages do not have access to `StorageTypeAccessed::kFileSystem`
  // until activation (AKA becoming the primary page, whose test case is already
  // covered).
  if (GetParam() == StorageTypeAccessed::kFileSystem) {
    GTEST_SKIP();
  }

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      embedded_https_test_server().GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL prerendering_url =
      embedded_https_test_server().GetURL("a.test", "/title2.html");
  const FrameTreeNodeId host_id =
      prerender_test_helper()->AddPrerender(prerendering_url);
  prerender_test_helper()->WaitForPrerenderLoadCompletion(prerendering_url);
  test::PrerenderHostObserver observer(*GetActiveWebContents(), host_id);
  EXPECT_FALSE(observer.was_activated());
  RenderFrameHost* prerender_frame =
      prerender_test_helper()->GetPrerenderedMainFrameHost(host_id);
  EXPECT_NE(prerender_frame, nullptr);

  EXPECT_TRUE(AccessStorage(prerender_frame, GetParam()));

  prerender_test_helper()->CancelPrerenderedPage(host_id);
  observer.WaitForDestroyed();

  const GURL primary_main_frame_final_url =
      embedded_https_test_server().GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(
      redirects,
      ElementsAre(
          ("[1/1] blank -> a.test/title1.html (None) -> d.test/title1.html")));
}

// WeLocks accesses aren't monitored by the `PageSpecificContentSettings` as
// they are not persistent.
// TODO(crbug.com/40269763): Remove `StorageTypeAccessed::kFileSystem` once
// deprecation is complete.
INSTANTIATE_TEST_SUITE_P(All,
                         BtmSiteDataAccessDetectorTest,
                         ::testing::Values(StorageTypeAccessed::kLocalStorage,
                                           StorageTypeAccessed::kSessionStorage,
                                           StorageTypeAccessed::kCacheStorage,
                                           StorageTypeAccessed::kFileSystem,
                                           StorageTypeAccessed::kIndexedDB));

// WebAuthn tests do not work on Android because there is no current way to
// install a virtual authenticator.
// NOTE: Manual testing was performed to ensure this implementation works as
// expected on Android platform.
// TODO(crbug.com/40269763): Implement automated testing once the infrastructure
// permits it (Requires mocking the Android Platform Authenticator i.e. GMS
// Core).
#if !BUILDFLAG(IS_ANDROID)
// Some refs for this test fixture:
// clang-format off
// - https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/webauthn/chrome_webauthn_browsertest.cc;drc=c4061a03f240338b42a5b84c98b1a11b62a97a9a
// - https://source.chromium.org/chromium/chromium/src/+/main:content/browser/webauth/webauth_browsertest.cc;drc=e8e4ad9096841fae7c55cea1b7d278c58f6160ff
// - https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/payments/secure_payment_confirmation_authenticator_browsertest.cc;drc=edea5c45c08d151afe67276f08a2ee13814563e1
// clang-format on
class BtmWebAuthnBrowserTest : public ContentBrowserTest {
 public:
  BtmWebAuthnBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  BtmWebAuthnBrowserTest(const BtmWebAuthnBrowserTest&) = delete;
  BtmWebAuthnBrowserTest& operator=(const BtmWebAuthnBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    mock_cert_verifier_.SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    // Allowlist all certs for the HTTPS server.
    mock_cert_verifier()->set_default_result(net::OK);

    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.ServeFilesFromSourceDirectory(GetTestDataFilePath());
    https_server_.RegisterDefaultHandler(base::BindRepeating(
        &HandleCrossSiteSameSiteNoneCookieRedirect, &https_server_));
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(https_server_.Start());

    auto virtual_device_factory =
        std::make_unique<device::test::VirtualFidoDeviceFactory>();

    virtual_device_factory->mutable_state()->InjectResidentKey(
        std::vector<uint8_t>{1, 2, 3, 4}, authn_hostname,
        std::vector<uint8_t>{5, 6, 7, 8}, "Foo", "Foo Bar");

    device::VirtualCtap2Device::Config config;
    config.resident_key_support = true;
    virtual_device_factory->SetCtap2Config(std::move(config));

    auth_env_ = std::make_unique<ScopedAuthenticatorEnvironmentForTesting>(
        std::move(virtual_device_factory));

    web_contents_observer_ =
        BtmWebContentsObserver::FromWebContents(GetActiveWebContents());
    CHECK(web_contents_observer_);
  }

  void TearDownOnMainThread() override {
    ContentBrowserTest::TearDownOnMainThread();
    web_contents_observer_ = nullptr;
  }

  void PostRunTestOnMainThread() override {
    auth_env_.reset();
    // web_contents_observer_.ClearAndDelete();
    ContentBrowserTest::PostRunTestOnMainThread();
  }

  auto* TestServer() { return &https_server_; }

  WebContents* GetActiveWebContents() { return shell()->web_contents(); }

  RedirectChainDetector* GetRedirectChainHelper() {
    return RedirectChainDetector::FromWebContents(GetActiveWebContents());
  }

  // Perform a browser-based navigation to terminate the current redirect chain.
  // (NOTE: tests using WCOCallbackLogger must call this *after* checking the
  // log, since this navigation will be logged.)
  void EndRedirectChain() {
    ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                              TestServer()->GetURL("a.test", "/title1.html")));
  }

  void StartAppendingRedirectsTo(std::vector<std::string>* redirects) {
    GetRedirectChainHelper()->SetRedirectChainHandlerForTesting(
        base::BindRepeating(&AppendRedirects, redirects));
  }

  void StartAppendingReportsTo(std::vector<std::string>* reports) {
    web_contents_observer_->SetIssueReportingCallbackForTesting(
        base::BindRepeating(&AppendSitesInReport, reports));
  }

  void GetWebAuthnAssertion() {
    ASSERT_EQ("OK", EvalJs(GetActiveWebContents(), R"(
    let cred_id = new Uint8Array([1,2,3,4]);
    navigator.credentials.get({
      publicKey: {
        challenge: cred_id,
        userVerification: 'preferred',
        allowCredentials: [{
          type: 'public-key',
          id: cred_id,
          transports: ['usb', 'nfc', 'ble'],
        }],
        timeout: 10000
      }
    }).then(c => 'OK',
      e => e.toString());
  )",
                           EXECUTE_SCRIPT_NO_USER_GESTURE));
  }

  ContentMockCertVerifier::CertVerifier* mock_cert_verifier() {
    return mock_cert_verifier_.mock_cert_verifier();
  }

 protected:
  const std::string authn_hostname = "b.test";

 private:
  ContentMockCertVerifier mock_cert_verifier_;
  net::EmbeddedTestServer https_server_;
  raw_ptr<BtmWebContentsObserver> web_contents_observer_ = nullptr;
  std::unique_ptr<ScopedAuthenticatorEnvironmentForTesting> auth_env_;
};

IN_PROC_BROWSER_TEST_F(BtmWebAuthnBrowserTest,
                       WebAuthnAssertion_ConfirmWCOCallback) {
  // Start logging `WebContentsObserver` callbacks.
  WCOCallbackLogger::CreateForWebContents(GetActiveWebContents());
  auto* logger = WCOCallbackLogger::FromWebContents(GetActiveWebContents());

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL initial_url = TestServer()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), initial_url));

  const GURL bounce_url = TestServer()->GetURL(authn_hostname, "/title1.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), bounce_url));

  AccessCookieViaJSIn(GetActiveWebContents(),
                      GetActiveWebContents()->GetPrimaryMainFrame());

  GetWebAuthnAssertion();

  const GURL final_url = TestServer()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `final_url`.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), final_url));

  std::vector<std::string> expected_log = {
      "DidStartNavigation(a.test/title1.html)",
      "DidFinishNavigation(a.test/title1.html)",
      "DidStartNavigation(b.test/title1.html)",
      "DidFinishNavigation(b.test/title1.html)",
      "OnCookiesAccessed(RenderFrameHost, Change: b.test/title1.html)",
      "WebAuthnAssertionRequestSucceeded(b.test/title1.html)",
      "DidStartNavigation(d.test/title1.html)",
      "DidFinishNavigation(d.test/title1.html)"};
  if (base::FeatureList::IsEnabled(network::features::kGetCookiesOnSet)) {
    expected_log.insert(
        expected_log.begin() + 5,
        "OnCookiesAccessed(RenderFrameHost, Read: b.test/title1.html)");
  }

  EXPECT_THAT(logger->log(), testing::ContainerEq(expected_log));

  EndRedirectChain();

  std::vector<std::string> expected_redirects;
  // NOTE: The bounce detection isn't impacted (is exonerated) at this point by
  // the web authn assertion.
  expected_redirects.push_back(
      "[1/1] a.test/title1.html -> b.test/title1.html (Write) -> "
      "d.test/title1.html");
  // NOTE: Due the favicon.ico temporally iffy callbacks we could expect the
  // following outcome to help avoid flakiness.
  expected_redirects.push_back(
      "[1/1] a.test/title1.html -> b.test/title1.html (ReadWrite) -> "
      "d.test/title1.html");

  EXPECT_THAT(expected_redirects, Contains(redirects.front()));
}

// This test verifies that sites in a redirect chain with previous web authn
// assertions are not reported in the resulting issue when a navigation
// finishes.
IN_PROC_BROWSER_TEST_F(
    BtmWebAuthnBrowserTest,
    ReportRedirectorsInChain_OmitSitesWithWebAuthnAssertions) {
  WebContents* web_contents = GetActiveWebContents();

  std::vector<std::string> reports;
  StartAppendingReportsTo(&reports);

  // Visit initial page on a.test.
  ASSERT_TRUE(NavigateToURL(web_contents,
                            TestServer()->GetURL("a.test", "/title1.html")));

  GURL url = TestServer()->GetURL(authn_hostname, "/title1.html");
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(web_contents, url));

  GetWebAuthnAssertion();

  // Verify web authn assertion was recorded for `authn_hostname`, before
  // proceeding.
  std::optional<StateValue> state =
      GetBtmState(GetBtmService(web_contents), url);
  ASSERT_TRUE(state.has_value());
  ASSERT_FALSE(state->user_activation_times.has_value());
  ASSERT_TRUE(state->web_authn_assertion_times.has_value());

  // Navigate with a click (not a redirect) to d.test, which statefully
  // S-redirects to c.test and write a cookie on c.test.
  ASSERT_TRUE(NavigateToURLFromRenderer(
      web_contents,
      TestServer()->GetURL(
          "d.test", "/cross-site-with-samesite-none-cookie/c.test/title1.html"),
      TestServer()->GetURL("c.test", "/title1.html")));
  AccessCookieViaJSIn(web_contents, web_contents->GetPrimaryMainFrame());

  // Navigate without a click (i.e. by C-redirecting) to `authn_hostname` and
  // write a cookie on `authn_hostname`:
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, TestServer()->GetURL(authn_hostname, "/title1.html")));
  AccessCookieViaJSIn(web_contents, web_contents->GetPrimaryMainFrame());

  // Navigate without a click (i.e. by C-redirecting) to e.test, which
  // statefully S-redirects to f.test, which statefully S-redirects to g.test.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents,
      TestServer()->GetURL("e.test",
                           "/cross-site-with-samesite-none-cookie/f.test/"
                           "cross-site-with-samesite-none-cookie/g.test/"
                           "title1.html"),
      TestServer()->GetURL("g.test", "/title1.html")));

  EndRedirectChain();
  WaitOnStorage(GetBtmService(web_contents));

  EXPECT_THAT(reports, ElementsAre(("a.test"), ("d.test"), ("c.test"),
                                   ("e.test, f.test")));
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Verifies that a successfully registered service worker is tracked as a
// storage access.
IN_PROC_BROWSER_TEST_F(BtmBounceDetectorBrowserTest,
                       ServiceWorkerAccess_Storages) {
  // Start logging `WebContentsObserver` callbacks.
  WCOCallbackLogger::CreateForWebContents(GetActiveWebContents());
  auto* logger = WCOCallbackLogger::FromWebContents(GetActiveWebContents());

  // Navigate to URL to set service workers. This will result in a service
  // worker access from the RenderFrameHost.
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));

  // Register a service worker on the current page, and await its completion.
  ASSERT_EQ(true, EvalJs(GetActiveWebContents(), R"(
    (async () => {
      await navigator.serviceWorker.register('/service_worker/empty.js');
      await navigator.serviceWorker.ready;
      return true;
    })();
  )"));

  // Navigate away from and back to the URL in scope of the registered service
  // worker. This will result in a service worker access from the
  // NavigationHandle.
  ASSERT_TRUE(NavigateToURL(
      GetActiveWebContents(),
      embedded_test_server()->GetURL("/service_worker/empty.html")));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));

  // Validate that the expected callbacks to WebContentsObserver were made.
  EXPECT_THAT(logger->log(),
              testing::IsSupersetOf({"OnServiceWorkerAccessed(RenderFrameHost: "
                                     "127.0.0.1/service_worker/)",
                                     "OnServiceWorkerAccessed(NavigationHandle:"
                                     " 127.0.0.1/service_worker/)"}));
}

// TODO(crbug.com/40290702): Shared workers are not available on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SharedWorkerAccess_Storages DISABLED_SharedWorkerAccess_Storages
#else
#define MAYBE_SharedWorkerAccess_Storages SharedWorkerAccess_Storages
#endif
// Verifies that adding a shared worker to a frame is tracked as a storage
// access.
IN_PROC_BROWSER_TEST_F(BtmBounceDetectorBrowserTest,
                       MAYBE_SharedWorkerAccess_Storages) {
  // Start logging `WebContentsObserver` callbacks.
  WCOCallbackLogger::CreateForWebContents(GetActiveWebContents());
  auto* logger = WCOCallbackLogger::FromWebContents(GetActiveWebContents());

  // Add the WCOCallbackLogger as an observer of SharedWorkerService events.
  GetActiveWebContents()
      ->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetSharedWorkerService()
      ->AddObserver(logger);

  // Navigate to URL for shared worker.
  ASSERT_TRUE(NavigateToURL(
      GetActiveWebContents(),
      embedded_test_server()->GetURL("a.test", "/no-favicon.html")));

  // Create and start a shared worker on the current page.
  ASSERT_EQ(true,
            EvalJs(GetActiveWebContents(), JsReplace(
                                               R"(
    (async () => {
      const worker = await new Promise((resolve, reject) => {
        const worker =
            new SharedWorker("/workers/shared_fetcher_treat_as_public.js");
        worker.port.addEventListener("message", () => resolve(worker));
        worker.addEventListener("error", reject);
        worker.port.start();
      });

      const messagePromise = new Promise((resolve) => {
        const listener = (event) => resolve(event.data);
        worker.port.addEventListener("message", listener, { once: true });
      });

      worker.port.postMessage($1);

      const { error, ok } = await messagePromise;
      if (error !== undefined) {
        throw(error);
      }

      return ok;
    })();
  )",
                                               embedded_test_server()->GetURL(
                                                   "b.test", "/cors-ok.txt"))));

  // Validate that the expected callback to SharedWorkerService.Observer was
  // made.
  EXPECT_THAT(
      logger->log(),
      testing::Contains("OnSharedWorkerClientAdded(a.test/no-favicon.html)"));

  // Clean up the observer to avoid a dangling ptr.
  GetActiveWebContents()
      ->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetSharedWorkerService()
      ->RemoveObserver(logger);
}

// Verifies that adding a dedicated worker to a frame is tracked as a storage
// access.
IN_PROC_BROWSER_TEST_F(BtmBounceDetectorBrowserTest,
                       DedicatedWorkerAccess_Storages) {
  // Start logging `WebContentsObserver` callbacks.
  WCOCallbackLogger::CreateForWebContents(GetActiveWebContents());
  auto* logger = WCOCallbackLogger::FromWebContents(GetActiveWebContents());

  // Add the WCOCallbackLogger as an observer of DedicatedWorkerService events.
  GetActiveWebContents()
      ->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetDedicatedWorkerService()
      ->AddObserver(logger);

  // Navigate to URL for dedicated worker.
  ASSERT_TRUE(NavigateToURL(
      GetActiveWebContents(),
      embedded_test_server()->GetURL("a.test", "/no-favicon.html")));

  // Create and start a dedicated worker on the current page.
  ASSERT_EQ(true,
            EvalJs(GetActiveWebContents(), JsReplace(
                                               R"(
    (async () => {
      const worker = new Worker("/workers/fetcher_treat_as_public.js");

      const messagePromise = new Promise((resolve) => {
        const listener = (event) => resolve(event.data);
        worker.addEventListener("message", listener, { once: true });
      });

      worker.postMessage($1);

      const { error, ok } = await messagePromise;
      if (error !== undefined) {
        throw(error);
      }

      return ok;
    })();
  )",
                                               embedded_test_server()->GetURL(
                                                   "b.test", "/cors-ok.txt"))));

  // Validate that the expected callback to DedicatedWorkerService.Observer was
  // made.
  EXPECT_THAT(
      logger->log(),
      testing::Contains("OnDedicatedWorkerCreated(a.test/no-favicon.html)"));

  // Clean up the observer to avoid a dangling ptr.
  GetActiveWebContents()
      ->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetDedicatedWorkerService()
      ->RemoveObserver(logger);
}

// Tests that currently only work consistently when the trigger is (any) bounce.
// TODO(crbug.com/336161248) Make these tests use stateful bounces.
class BtmBounceTriggerBrowserTest : public BtmBounceDetectorBrowserTest {
 protected:
  BtmBounceTriggerBrowserTest() {
    enabled_features_.push_back(
        {features::kBtm, {{"triggering_action", "bounce"}}});
  }

  void SetUpOnMainThread() override {
    BtmBounceDetectorBrowserTest::SetUpOnMainThread();
    // BTM will only record bounces if 3PCs are blocked.
    browser_client().SetBlockThirdPartyCookiesByDefault(true);
    WebContents* web_contents = GetActiveWebContents();
    ASSERT_FALSE(btm::Are3PcsGenerallyEnabled(web_contents->GetBrowserContext(),
                                              web_contents));
  }
};

// Verifies that a HTTP 204 (No Content) response is treated like a bounce.
IN_PROC_BROWSER_TEST_F(BtmBounceTriggerBrowserTest, NoContent) {
  WebContents* web_contents = GetActiveWebContents();

  GURL committed_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, committed_url));

  BtmRedirectChainObserver observer(
      BtmService::Get(web_contents->GetBrowserContext()), committed_url);
  GURL nocontent_url = embedded_test_server()->GetURL("b.test", "/nocontent");
  ASSERT_TRUE(NavigateToURL(web_contents, nocontent_url, committed_url));
  observer.Wait();

  base::test::TestFuture<const std::vector<std::string>&> deleted_sites;
  BtmService::Get(web_contents->GetBrowserContext())
      ->DeleteEligibleSitesImmediately(deleted_sites.GetCallback());
  ASSERT_THAT(deleted_sites.Get(), ElementsAre("b.test"));
}

class BtmThrottlingBrowserTest : public BtmBounceDetectorBrowserTest {
 public:
  void SetUpOnMainThread() override {
    BtmBounceDetectorBrowserTest::SetUpOnMainThread();
    BtmWebContentsObserver::FromWebContents(GetActiveWebContents())
        ->SetClockForTesting(&test_clock_);
  }

  base::SimpleTestClock test_clock_;
};

IN_PROC_BROWSER_TEST_F(BtmThrottlingBrowserTest,
                       InteractionRecording_Throttled) {
  WebContents* web_contents = GetActiveWebContents();
  const base::Time start_time = test_clock_.Now();

  // Record user activation on a.test.
  const GURL url = embedded_test_server()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, url));
  SimulateMouseClick();
  // Verify the interaction was recorded in the BTM DB.
  std::optional<StateValue> state =
      GetBtmState(GetBtmService(web_contents), url);
  ASSERT_THAT(state->user_activation_times,
              testing::Optional(testing::Pair(start_time, start_time)));

  // Click again, just before kBtmTimestampUpdateInterval elapses.
  test_clock_.Advance(kBtmTimestampUpdateInterval - base::Seconds(1));
  SimulateMouseClick();
  // Verify the second interaction was NOT recorded, due to throttling.
  state = GetBtmState(GetBtmService(web_contents), url);
  ASSERT_THAT(state->user_activation_times,
              testing::Optional(testing::Pair(start_time, start_time)));

  // Click a third time, after kBtmTimestampUpdateInterval has passed since the
  // first click.
  test_clock_.Advance(base::Seconds(1));
  SimulateMouseClick();
  // Verify the third interaction WAS recorded.
  state = GetBtmState(GetBtmService(web_contents), url);
  ASSERT_THAT(state->user_activation_times,
              testing::Optional(testing::Pair(
                  start_time, start_time + kBtmTimestampUpdateInterval)));
}

IN_PROC_BROWSER_TEST_F(BtmThrottlingBrowserTest,
                       InteractionRecording_NotThrottled_AfterRefresh) {
  WebContents* web_contents = GetActiveWebContents();
  const base::Time start_time = test_clock_.Now();

  // Record user activation on a.test.
  const GURL url = embedded_test_server()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, url));
  SimulateMouseClick();
  // Verify the interaction was recorded in the BTM DB.
  std::optional<StateValue> state =
      GetBtmState(GetBtmService(web_contents), url);
  ASSERT_THAT(state->user_activation_times,
              testing::Optional(testing::Pair(start_time, start_time)));

  // Navigate to a new page and click, only a second after the previous click.
  test_clock_.Advance(base::Seconds(1));
  const GURL url2 = embedded_test_server()->GetURL("b.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, url2));
  SimulateMouseClick();
  // Verify the second interaction was also recorded (not throttled).
  state = GetBtmState(GetBtmService(web_contents), url2);
  ASSERT_THAT(state->user_activation_times,
              testing::Optional(testing::Pair(start_time + base::Seconds(1),
                                              start_time + base::Seconds(1))));
}

class AllSitesFollowingFirstPartyTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("*", "127.0.0.1");

    first_party_url_ = embedded_test_server()->GetURL("a.test", "/title1.html");
    third_party_url_ = embedded_test_server()->GetURL("b.test", "/title1.html");
    other_url_ = embedded_test_server()->GetURL("c.test", "/title1.html");
  }

  WebContents* GetActiveWebContents() { return shell()->web_contents(); }

 protected:
  GURL first_party_url_;
  GURL third_party_url_;
  GURL other_url_;
};

IN_PROC_BROWSER_TEST_F(AllSitesFollowingFirstPartyTest,
                       SiteFollowingFirstPartyIncluded) {
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), other_url_));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), first_party_url_));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), third_party_url_));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), third_party_url_));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), other_url_));

  EXPECT_THAT(RedirectHeuristicTabHelper::AllSitesFollowingFirstParty(
                  GetActiveWebContents(), first_party_url_),
              testing::ElementsAre(GetSiteForBtm(third_party_url_)));
}

IN_PROC_BROWSER_TEST_F(AllSitesFollowingFirstPartyTest,
                       SiteNotFollowingFirstPartyNotIncluded) {
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), first_party_url_));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), third_party_url_));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), other_url_));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), third_party_url_));

  EXPECT_THAT(RedirectHeuristicTabHelper::AllSitesFollowingFirstParty(
                  GetActiveWebContents(), first_party_url_),
              testing::ElementsAre(GetSiteForBtm(third_party_url_)));
}

IN_PROC_BROWSER_TEST_F(AllSitesFollowingFirstPartyTest, MultipleSitesIncluded) {
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), first_party_url_));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), third_party_url_));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), first_party_url_));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), other_url_));

  EXPECT_THAT(RedirectHeuristicTabHelper::AllSitesFollowingFirstParty(
                  GetActiveWebContents(), first_party_url_),
              testing::ElementsAre(GetSiteForBtm(third_party_url_),
                                   GetSiteForBtm(other_url_)));
}

IN_PROC_BROWSER_TEST_F(AllSitesFollowingFirstPartyTest,
                       NoFirstParty_NothingIncluded) {
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), third_party_url_));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), other_url_));

  EXPECT_THAT(RedirectHeuristicTabHelper::AllSitesFollowingFirstParty(
                  GetActiveWebContents(), first_party_url_),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_F(AllSitesFollowingFirstPartyTest,
                       NothingAfterFirstParty_NothingIncluded) {
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), other_url_));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), third_party_url_));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), first_party_url_));

  EXPECT_THAT(RedirectHeuristicTabHelper::AllSitesFollowingFirstParty(
                  GetActiveWebContents(), first_party_url_),
              testing::IsEmpty());
}

class BtmPrivacySandboxDataPreservationTest : public ContentBrowserTest {
 public:
  BtmPrivacySandboxDataPreservationTest()
      : embedded_https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    enabled_features.emplace_back(features::kPrivacySandboxAdsAPIsOverride);
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    RegisterTrustTokenTestHandler(&trust_token_request_handler_);
    embedded_https_test_server_.SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(embedded_https_test_server_.Start());
    browser_client_.emplace();
    browser_client().SetBlockThirdPartyCookiesByDefault(true);
    WebContents* web_contents = GetActiveWebContents();
    ASSERT_FALSE(btm::Are3PcsGenerallyEnabled(web_contents->GetBrowserContext(),
                                              web_contents));
  }

  WebContents* GetActiveWebContents() { return shell()->web_contents(); }

  base::expected<AttributionData, std::string> WaitForAttributionData() {
    WebContents* web_contents = GetActiveWebContents();
    AttributionDataModel* model = web_contents->GetBrowserContext()
                                      ->GetDefaultStoragePartition()
                                      ->GetAttributionDataModel();
    if (!model) {
      return base::unexpected("null attribution data model");
    }
    // Poll until data appears, failing if action_timeout() passes
    base::Time deadline = base::Time::Now() + TestTimeouts::action_timeout();
    while (base::Time::Now() < deadline) {
      base::test::TestFuture<AttributionData> future;
      model->GetAllDataKeys(future.GetCallback());
      AttributionData data = future.Get();
      if (!data.empty()) {
        return data;
      }
      Sleep(TestTimeouts::tiny_timeout());
    }
    return base::unexpected("timed out waiting for attribution data");
  }

  // TODO: crbug.com/1509946 - When embedded_https_test_server() is added to
  // AndroidBrowserTest, switch to using
  // PlatformBrowserTest::embedded_https_test_server() and delete this.
  net::EmbeddedTestServer embedded_https_test_server_;

  TpcBlockingBrowserClient& browser_client() { return browser_client_->impl(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  static void Sleep(base::TimeDelta delay) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), delay);
    run_loop.Run();
  }

  void RegisterTrustTokenTestHandler(
      network::test::TrustTokenRequestHandler* handler) {
    embedded_https_test_server_.RegisterRequestHandler(
        base::BindLambdaForTesting(
            [handler, this](const net::test_server::HttpRequest& request)
                -> std::unique_ptr<net::test_server::HttpResponse> {
              if (request.relative_url != "/issue") {
                return nullptr;
              }
              if (!base::Contains(request.headers, "Sec-Private-State-Token") ||
                  !base::Contains(request.headers,
                                  "Sec-Private-State-Token-Crypto-Version")) {
                return MakeTrustTokenFailureResponse();
              }

              std::optional<std::string> operation_result =
                  handler->Issue(request.headers.at("Sec-Private-State-Token"));

              if (!operation_result) {
                return MakeTrustTokenFailureResponse();
              }

              return MakeTrustTokenResponse(*operation_result);
            }));
  }

  std::unique_ptr<net::test_server::HttpResponse>
  MakeTrustTokenFailureResponse() {
    // No need to report a failure HTTP code here: returning a vanilla OK should
    // fail the Trust Tokens operation client-side.
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    return response;
  }

  // Constructs and returns an HTTP response bearing the given base64-encoded
  // Trust Tokens issuance or redemption protocol response message.
  std::unique_ptr<net::test_server::HttpResponse> MakeTrustTokenResponse(
      std::string_view contents) {
    std::string temp;
    CHECK(base::Base64Decode(contents, &temp));

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->AddCustomHeader("Sec-Private-State-Token", std::string(contents));
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    return response;
  }

  network::test::TrustTokenRequestHandler trust_token_request_handler_;
  std::optional<ContentBrowserTestTpcBlockingBrowserClient> browser_client_;
};

IN_PROC_BROWSER_TEST_F(BtmPrivacySandboxDataPreservationTest,
                       DontClearAttributionReportingApiData) {
  WebContents* web_contents = GetActiveWebContents();

  GURL toplevel_url =
      embedded_https_test_server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, toplevel_url));

  // Create image that registers an attribution source.
  GURL attribution_url = embedded_https_test_server_.GetURL(
      "b.test", "/attribution_reporting/register_source_headers.html");
  ASSERT_TRUE(ExecJs(web_contents, JsReplace(
                                       R"(
    let img = document.createElement('img');
    img.attributionSrc = $1;
    document.body.appendChild(img);)",
                                       attribution_url)));

  // Wait for the AttributionDataModel to show that source.
  ASSERT_OK_AND_ASSIGN(AttributionData data, WaitForAttributionData());
  ASSERT_THAT(GetOrigins(data),
              ElementsAre(url::Origin::Create(attribution_url)));

  // Make the attribution site eligible for BTM deletion.
  BtmServiceImpl* btm_service =
      BtmServiceImpl::Get(web_contents->GetBrowserContext());
  ASSERT_TRUE(btm_service != nullptr);
  base::test::TestFuture<void> record_bounce;
  btm_service->storage()
      ->AsyncCall(&BtmStorage::RecordBounce)
      .WithArgs(attribution_url, base::Time::Now())
      .Then(record_bounce.GetCallback());
  ASSERT_TRUE(record_bounce.Wait());

  // Trigger BTM deletion.
  base::test::TestFuture<const std::vector<std::string>&> deleted_sites;
  btm_service->DeleteEligibleSitesImmediately(deleted_sites.GetCallback());
  EXPECT_THAT(deleted_sites.Get(), ElementsAre(GetSiteForBtm(attribution_url)));

  base::test::TestFuture<AttributionData> post_deletion_data;
  web_contents->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetAttributionDataModel()
      ->GetAllDataKeys(post_deletion_data.GetCallback());

  // Confirm the attribution data was not deleted.
  EXPECT_THAT(GetOrigins(post_deletion_data.Get()),
              ElementsAre(url::Origin::Create(attribution_url)));
}

namespace {

class SiteStorage {
 public:
  constexpr SiteStorage() = default;

  virtual base::expected<std::string, std::string> ReadValue(
      RenderFrameHost* frame) const = 0;
  virtual testing::AssertionResult WriteValue(RenderFrameHost* frame,
                                              std::string_view value,
                                              bool partitioned) const = 0;

  virtual std::string_view name() const = 0;
};

class CookieStorage : public SiteStorage {
  base::expected<std::string, std::string> ReadValue(
      RenderFrameHost* frame) const override {
    EvalJsResult result =
        EvalJs(frame, "document.cookie", EXECUTE_SCRIPT_NO_USER_GESTURE);
    if (!result.is_ok()) {
      return base::unexpected(result.ExtractError());
    }
    return base::ok(result.ExtractString());
  }

  testing::AssertionResult WriteValue(RenderFrameHost* frame,
                                      std::string_view cookie,
                                      bool partitioned) const override {
    std::string value(cookie);
    if (partitioned) {
      value += ";Secure;Partitioned;SameSite=None";
    }

    FrameCookieAccessObserver obs(WebContents::FromRenderFrameHost(frame),
                                  frame, CookieOperation::kChange);
    testing::AssertionResult result =
        ExecJs(frame, JsReplace("document.cookie = $1;", value),
               EXECUTE_SCRIPT_NO_USER_GESTURE);
    if (result) {
      obs.Wait();
    }
    return result;
  }

  std::string_view name() const override { return "CookieStorage"; }
};

class LocalStorage : public SiteStorage {
  base::expected<std::string, std::string> ReadValue(
      RenderFrameHost* frame) const override {
    EvalJsResult result = EvalJs(frame, "localStorage.getItem('value')",
                                 EXECUTE_SCRIPT_NO_USER_GESTURE);
    if (!result.is_ok()) {
      return base::unexpected(result.ExtractError());
    }
    if (result == base::Value()) {
      return base::ok("");
    }
    return base::ok(result.ExtractString());
  }

  testing::AssertionResult WriteValue(RenderFrameHost* frame,
                                      std::string_view value,
                                      bool partitioned) const override {
    return ExecJs(frame, JsReplace("localStorage.setItem('value', $1);", value),
                  EXECUTE_SCRIPT_NO_USER_GESTURE);
  }

  std::string_view name() const override { return "LocalStorage"; }
};

void PrintTo(const SiteStorage* storage, std::ostream* os) {
  *os << storage->name();
}

static constexpr CookieStorage kCookieStorage;
static constexpr LocalStorage kLocalStorage;
}  // namespace

class BtmDataDeletionBrowserTest
    : public BtmBounceDetectorBrowserTest,
      public testing::WithParamInterface<const SiteStorage*> {
 public:
  void SetUpOnMainThread() override {
    BtmBounceDetectorBrowserTest::SetUpOnMainThread();
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(kContentTestDataDir);
    ASSERT_TRUE(https_server_.Start());

    browser_client().SetBlockThirdPartyCookiesByDefault(true);
    WebContents* web_contents = GetActiveWebContents();
    ASSERT_FALSE(btm::Are3PcsGenerallyEnabled(web_contents->GetBrowserContext(),
                                              web_contents));
  }

  const net::EmbeddedTestServer& https_server() const { return https_server_; }

  [[nodiscard]] testing::AssertionResult WriteToPartitionedStorage(
      std::string_view first_party_hostname,
      std::string_view third_party_hostname,
      std::string_view value) {
    WebContents* web_contents = GetActiveWebContents();

    if (!NavigateToURL(web_contents,
                       https_server().GetURL(first_party_hostname,
                                             "/page_with_blank_iframe.html"))) {
      return testing::AssertionFailure() << "Failed to navigate top-level";
    }

    const std::string_view kIframeId = "test_iframe";
    if (!NavigateIframeToURL(
            web_contents, kIframeId,
            https_server().GetURL(third_party_hostname, "/title1.html"))) {
      return testing::AssertionFailure() << "Failed to navigate iframe";
    }

    RenderFrameHost* iframe = ChildFrameAt(web_contents, 0);
    if (!iframe) {
      return testing::AssertionFailure() << "Child frame not found";
    }
    return WriteValue(iframe, value, /*partitioned=*/true);
  }

  [[nodiscard]] base::expected<std::string, std::string>
  ReadFromPartitionedStorage(std::string_view first_party_hostname,
                             std::string_view third_party_hostname) {
    WebContents* web_contents = GetActiveWebContents();

    if (!NavigateToURL(web_contents,
                       https_server().GetURL(first_party_hostname,
                                             "/page_with_blank_iframe.html"))) {
      return base::unexpected("Failed to navigate top-level");
    }

    const std::string_view kIframeId = "test_iframe";
    if (!NavigateIframeToURL(
            web_contents, kIframeId,
            https_server().GetURL(third_party_hostname, "/title1.html"))) {
      return base::unexpected("Failed to navigate iframe");
    }

    RenderFrameHost* iframe = ChildFrameAt(web_contents, 0);
    if (!iframe) {
      return base::unexpected("iframe not found");
    }
    return ReadValue(iframe);
  }

  [[nodiscard]] base::expected<std::string, std::string> ReadFromStorage(
      std::string_view hostname) {
    WebContents* web_contents = GetActiveWebContents();

    if (!NavigateToURL(web_contents,
                       https_server().GetURL(hostname, "/title1.html"))) {
      return base::unexpected("Failed to navigate");
    }

    return ReadValue(web_contents);
  }

  [[nodiscard]] testing::AssertionResult WriteToStorage(
      std::string_view hostname,
      std::string_view value) {
    WebContents* web_contents = GetActiveWebContents();

    if (!NavigateToURL(web_contents,
                       https_server().GetURL(hostname, "/title1.html"))) {
      return testing::AssertionFailure() << "Failed to navigate";
    }

    return WriteValue(web_contents, value);
  }

  // Navigates to host1, then performs a stateful bounce on host2 to host3.
  [[nodiscard]] testing::AssertionResult DoStatefulBounce(
      std::string_view host1,
      std::string_view host2,
      std::string_view host3) {
    WebContents* web_contents = GetActiveWebContents();

    if (!NavigateToURL(web_contents,
                       https_server().GetURL(host1, "/title1.html"))) {
      return testing::AssertionFailure() << "Failed to navigate to " << host1;
    }

    if (!NavigateToURLFromRenderer(
            web_contents, https_server().GetURL(host2, "/title1.html"))) {
      return testing::AssertionFailure() << "Failed to navigate to " << host2;
    }

    testing::AssertionResult result = WriteValue(web_contents, "bounce=yes");
    if (!result) {
      return result;
    }

    if (!NavigateToURLFromRendererWithoutUserGesture(
            web_contents, https_server().GetURL(host3, "/title1.html"))) {
      return testing::AssertionFailure() << "Failed to navigate to " << host3;
    }

    EndRedirectChain();

    return testing::AssertionSuccess();
  }

 private:
  const SiteStorage* storage() { return GetParam(); }

  [[nodiscard]] base::expected<std::string, std::string> ReadValue(
      const ToRenderFrameHost& frame) {
    return storage()->ReadValue(frame.render_frame_host());
  }

  [[nodiscard]] testing::AssertionResult WriteValue(
      const ToRenderFrameHost& frame,
      std::string_view value,
      bool partitioned = false) {
    return storage()->WriteValue(frame.render_frame_host(), value, partitioned);
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_P(BtmDataDeletionBrowserTest, DontDeleteIfTpcsEnabled) {
  // Do not block third-party cookies by default. This should make it such that
  // BTM deletion does not run.
  browser_client().SetBlockThirdPartyCookiesByDefault(false);
  WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(btm::Are3PcsGenerallyEnabled(web_contents->GetBrowserContext(),
                                           web_contents));

  // Perform a stateful bounce on b.test to make it eligible for deletion.
  ASSERT_TRUE(DoStatefulBounce("a.test", "b.test", "c.test"));
  // Confirm unpartitioned storage was written on b.test.
  EXPECT_THAT(ReadFromStorage("b.test"), base::test::ValueIs("bounce=yes"));
  // Navigate away from b.test since BTM won't delete its state while loaded.
  ASSERT_TRUE(NavigateToURL(web_contents,
                            https_server().GetURL("a.test", "/title1.html")));

  // Trigger BTM deletion.
  base::test::TestFuture<const std::vector<std::string>&> deleted_sites;
  BtmService::Get(web_contents->GetBrowserContext())
      ->DeleteEligibleSitesImmediately(deleted_sites.GetCallback());

  // Confirm that nothing was deleted.
  EXPECT_THAT(deleted_sites.Get(), IsEmpty());
  // Confirm b.test storage has not changed.
  EXPECT_THAT(ReadFromStorage("b.test"), base::test::ValueIs("bounce=yes"));
}

IN_PROC_BROWSER_TEST_P(BtmDataDeletionBrowserTest, DeleteDomain) {
  WebContents* web_contents = GetActiveWebContents();

  // Perform a stateful bounce on b.test to make it eligible for deletion.
  ASSERT_TRUE(DoStatefulBounce("a.test", "b.test", "c.test"));

  // Confirm unpartitioned storage was written on b.test.
  EXPECT_THAT(ReadFromStorage("b.test"), base::test::ValueIs("bounce=yes"));
  // Navigate away from b.test since BTM won't delete its state while loaded.
  ASSERT_TRUE(NavigateToURL(web_contents,
                            https_server().GetURL("a.test", "/title1.html")));

  // Trigger BTM deletion.
  base::test::TestFuture<const std::vector<std::string>&> deleted_sites;
  BtmService::Get(web_contents->GetBrowserContext())
      ->DeleteEligibleSitesImmediately(deleted_sites.GetCallback());
  ASSERT_THAT(deleted_sites.Get(), ElementsAre("b.test"));

  // Confirm b.test storage was deleted.
  EXPECT_THAT(ReadFromStorage("b.test"), base::test::ValueIs(""));
}

IN_PROC_BROWSER_TEST_P(BtmDataDeletionBrowserTest, DontDeleteOtherDomains) {
  WebContents* web_contents = GetActiveWebContents();

  // Set storage on a.test
  ASSERT_TRUE(WriteToStorage("a.test", "foo=bar"));
  // Confirm written.
  EXPECT_THAT(ReadFromStorage("a.test"), base::test::ValueIs("foo=bar"));

  // Perform a stateful bounce on b.test to make it eligible for deletion.
  ASSERT_TRUE(DoStatefulBounce("a.test", "b.test", "c.test"));

  // Trigger BTM deletion.
  base::test::TestFuture<const std::vector<std::string>&> deleted_sites;
  BtmService::Get(web_contents->GetBrowserContext())
      ->DeleteEligibleSitesImmediately(deleted_sites.GetCallback());
  ASSERT_THAT(deleted_sites.Get(), ElementsAre("b.test"));

  // Confirm a.test storage was NOT deleted.
  EXPECT_THAT(ReadFromStorage("a.test"), base::test::ValueIs("foo=bar"));
}

IN_PROC_BROWSER_TEST_P(BtmDataDeletionBrowserTest,
                       DontDeleteDomainWhenPartitioned) {
  WebContents* web_contents = GetActiveWebContents();

  // Set storage on b.test embedded in a.test.
  ASSERT_TRUE(WriteToPartitionedStorage("a.test", "b.test", "foo=bar"));
  // Confirm written.
  EXPECT_THAT(ReadFromPartitionedStorage("a.test", "b.test"),
              base::test::ValueIs("foo=bar"));

  // Perform a stateful bounce on b.test to make it eligible for deletion.
  ASSERT_TRUE(DoStatefulBounce("a.test", "b.test", "c.test"));

  // Trigger BTM deletion.
  base::test::TestFuture<const std::vector<std::string>&> deleted_sites;
  BtmService::Get(web_contents->GetBrowserContext())
      ->DeleteEligibleSitesImmediately(deleted_sites.GetCallback());
  ASSERT_THAT(deleted_sites.Get(), ElementsAre("b.test"));

  // Confirm partitioned storage was NOT deleted.
  EXPECT_THAT(ReadFromPartitionedStorage("a.test", "b.test"),
              base::test::ValueIs("foo=bar"));
}

IN_PROC_BROWSER_TEST_P(BtmDataDeletionBrowserTest, DeleteSubdomains) {
  WebContents* web_contents = GetActiveWebContents();

  // Set storage on sub.b.test
  ASSERT_TRUE(WriteToStorage("sub.b.test", "foo=bar"));
  // Confirm written.
  EXPECT_THAT(ReadFromStorage("sub.b.test"), base::test::ValueIs("foo=bar"));

  // Perform a stateful bounce on b.test to make it eligible for deletion.
  ASSERT_TRUE(DoStatefulBounce("a.test", "b.test", "c.test"));

  // Trigger BTM deletion.
  base::test::TestFuture<const std::vector<std::string>&> deleted_sites;
  BtmService::Get(web_contents->GetBrowserContext())
      ->DeleteEligibleSitesImmediately(deleted_sites.GetCallback());
  ASSERT_THAT(deleted_sites.Get(), ElementsAre("b.test"));

  // Confirm sub.b.test storage was deleted.
  EXPECT_THAT(ReadFromStorage("sub.b.test"), base::test::ValueIs(""));
}

IN_PROC_BROWSER_TEST_P(BtmDataDeletionBrowserTest, DeleteEmbedded3Ps) {
  WebContents* web_contents = GetActiveWebContents();

  // Set storage on a.test embedded in b.test.
  ASSERT_TRUE(WriteToPartitionedStorage("b.test", "a.test", "foo=bar"));
  // Confirm written.
  EXPECT_THAT(ReadFromPartitionedStorage("b.test", "a.test"),
              base::test::ValueIs("foo=bar"));

  // Perform a stateful bounce on b.test to make it eligible for deletion.
  ASSERT_TRUE(DoStatefulBounce("a.test", "b.test", "c.test"));

  // Trigger BTM deletion.
  base::test::TestFuture<const std::vector<std::string>&> deleted_sites;
  BtmService::Get(web_contents->GetBrowserContext())
      ->DeleteEligibleSitesImmediately(deleted_sites.GetCallback());
  ASSERT_THAT(deleted_sites.Get(), ElementsAre("b.test"));

  // Confirm partitioned a.test storage was deleted.
  EXPECT_THAT(ReadFromPartitionedStorage("b.test", "a.test"),
              base::test::ValueIs(""));
}

IN_PROC_BROWSER_TEST_P(BtmDataDeletionBrowserTest,
                       DeleteEmbedded3Ps_Subdomain) {
  WebContents* web_contents = GetActiveWebContents();

  // Set storage on a.test embedded in sub.b.test.
  ASSERT_TRUE(WriteToPartitionedStorage("sub.b.test", "a.test", "foo=bar"));
  // Confirm written.
  EXPECT_THAT(ReadFromPartitionedStorage("sub.b.test", "a.test"),
              base::test::ValueIs("foo=bar"));

  // Perform a stateful bounce on b.test to make it eligible for deletion.
  ASSERT_TRUE(DoStatefulBounce("a.test", "b.test", "c.test"));

  // Trigger BTM deletion.
  base::test::TestFuture<const std::vector<std::string>&> deleted_sites;
  BtmService::Get(web_contents->GetBrowserContext())
      ->DeleteEligibleSitesImmediately(deleted_sites.GetCallback());
  ASSERT_THAT(deleted_sites.Get(), ElementsAre("b.test"));

  // Confirm partitioned a.test storage was deleted.
  EXPECT_THAT(ReadFromPartitionedStorage("sub.b.test", "a.test"),
              base::test::ValueIs(""));
}

IN_PROC_BROWSER_TEST_P(BtmDataDeletionBrowserTest, DeleteEmbedded1Ps) {
  content::WebContents* web_contents = GetActiveWebContents();

  // Set storage on a.test embedded in another a.test.
  ASSERT_TRUE(WriteToPartitionedStorage("a.test", "a.test", "foo=bar"));
  // Confirm written.
  EXPECT_THAT(ReadFromPartitionedStorage("a.test", "a.test"),
              base::test::ValueIs("foo=bar"));

  // Perform a stateful bounce on a.test to make it eligible for deletion.
  ASSERT_TRUE(DoStatefulBounce("b.test", "a.test", "c.test"));

  // Trigger BTM deletion.
  base::test::TestFuture<const std::vector<std::string>&> deleted_sites;
  BtmService::Get(web_contents->GetBrowserContext())
      ->DeleteEligibleSitesImmediately(deleted_sites.GetCallback());
  ASSERT_THAT(deleted_sites.Get(), ElementsAre("a.test"));

  // Confirm partitioned a.test storage was deleted.
  EXPECT_THAT(ReadFromPartitionedStorage("a.test", "a.test"),
              base::test::ValueIs(""));
}

INSTANTIATE_TEST_SUITE_P(All,
                         BtmDataDeletionBrowserTest,
                         ::testing::Values(&kCookieStorage, &kLocalStorage));

class BtmBounceDetectorBFCacheTest : public BtmBounceDetectorBrowserTest,
                                     public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    if (IsBFCacheEnabled() &&
        !base::FeatureList::IsEnabled(features::kBackForwardCache)) {
      GTEST_SKIP() << "BFCache disabled";
    }
    BtmBounceDetectorBrowserTest::SetUp();
  }
  bool IsBFCacheEnabled() const { return GetParam(); }
  void SetUpOnMainThread() override {
    if (!IsBFCacheEnabled()) {
      DisableBackForwardCacheForTesting(
          GetActiveWebContents(),
          BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);
    }

    BtmBounceDetectorBrowserTest::SetUpOnMainThread();
  }
};

// Confirm that BTM records a bounce, even if the user immediately navigates
// away.
// TODO(https://crbug.com/425717555): Very flaky if BF Cache is disabled.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_LateCookieAccessTest DISABLED_LateCookieAccessTest
#else
#define MAYBE_LateCookieAccessTest LateCookieAccessTest
#endif
IN_PROC_BROWSER_TEST_P(BtmBounceDetectorBFCacheTest,
                       MAYBE_LateCookieAccessTest) {
  const GURL bounce_url =
      embedded_test_server()->GetURL("b.test", "/empty.html");
  const GURL final_url =
      embedded_test_server()->GetURL("c.test", "/empty.html");

  WebContents* const web_contents = GetActiveWebContents();
  RedirectChainDetector* wco =
      RedirectChainDetector::FromWebContents(web_contents);

  ASSERT_TRUE(NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/empty.html")));

  ASSERT_TRUE(NavigateToURLFromRenderer(web_contents, bounce_url));
  ASSERT_TRUE(ExecJs(web_contents, "document.cookie = 'bounce=true';",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));

  ASSERT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(web_contents, final_url));
  URLCookieAccessObserver cookie_observer(web_contents, final_url,
                                          CookieOperation::kChange);

  ASSERT_TRUE(ExecJs(web_contents, "document.cookie = 'final=yes';",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  cookie_observer.Wait();
  // Since cookies are reported serially, both cookie writes should have been
  // reported by now.

  const BtmRedirectContext& context = wco->CommittedRedirectContext();
  ASSERT_EQ(context.size(), 1u);
  const BtmRedirectInfo& redirect = context[0];
  EXPECT_EQ(redirect.redirector_url, bounce_url);
  // A request to /favicon.ico may cause a cookie read in addition to the write
  // we explicitly performed.
  EXPECT_THAT(
      redirect.access_type,
      testing::AnyOf(BtmDataAccessType::kWrite, BtmDataAccessType::kReadWrite));
}

// Confirm that WCO::OnCookiesAccessed() is always called even if the user
// immediately navigates away.
IN_PROC_BROWSER_TEST_P(BtmBounceDetectorBFCacheTest, CookieAccessReported) {
  const GURL url1 = embedded_test_server()->GetURL("a.test", "/empty.html");
  const GURL url2 = embedded_test_server()->GetURL("b.test", "/empty.html");
  const GURL url3 = embedded_test_server()->GetURL("c.test", "/empty.html");

  WebContents* const web_contents = GetActiveWebContents();
  WCOCallbackLogger::CreateForWebContents(web_contents);
  auto* logger = WCOCallbackLogger::FromWebContents(web_contents);

  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  ASSERT_TRUE(ExecJs(web_contents, "document.cookie = 'initial=true';",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  ASSERT_TRUE(NavigateToURL(web_contents, url2));
  ASSERT_TRUE(NavigateToURL(web_contents, url3));
  URLCookieAccessObserver cookie_observer(web_contents, url3,
                                          CookieOperation::kChange);
  ASSERT_TRUE(ExecJs(web_contents, "document.cookie = 'final=yes';",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  cookie_observer.Wait();

  EXPECT_THAT(
      logger->log(),
      testing::Contains(
          "OnCookiesAccessed(RenderFrameHost, Change: a.test/empty.html)"));
}

// Confirm that BTM records an interaction, even if the user immediately
// navigates away.
//
// TODO: crbug.com/376625002 - After moving to //content, this test was flaky
// because the navigation to final_url unexpectedly sometimes has a user
// gesture. Because there's no indication of a fault in BTM, we disabled this
// test to get the move done, but we should try to fix and re-enable it.
IN_PROC_BROWSER_TEST_P(BtmBounceDetectorBFCacheTest,
                       DISABLED_LateInteractionTest) {
  const GURL bounce_url =
      embedded_test_server()->GetURL("b.test", "/empty.html");
  const GURL final_url =
      embedded_test_server()->GetURL("c.test", "/empty.html");
  WebContents* const web_contents = GetActiveWebContents();
  RedirectChainDetector* wco =
      RedirectChainDetector::FromWebContents(web_contents);

  ASSERT_TRUE(NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/empty.html")));

  ASSERT_TRUE(NavigateToURLFromRenderer(web_contents, bounce_url));
  ::content::SimulateMouseClick(web_contents, 0,
                                blink::WebMouseEvent::Button::kLeft);
  // Consume the transient user activation so the next navigation is not
  // considered to be user-initiated and will be judged a bounce.
  if (EvalJs(web_contents, "!open('about:blank')",
             EXECUTE_SCRIPT_NO_USER_GESTURE)
          .ExtractBool()) {
    // Due to a race condition, the open() call might be executed before the
    // click is processed, causing open() to fail and leaving the window with
    // transient user activation. In such a case, just skip the test. (If we
    // used UserActivationObserver::Wait() here, it would defeat the purpose of
    // this test, which is to verify that BTM sees the interaction even if the
    // test doesn't wait for it.)
    GTEST_SKIP();
  }
  ASSERT_FALSE(
      web_contents->GetPrimaryMainFrame()->HasTransientUserActivation());

  ASSERT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(web_contents, final_url));
  UserActivationObserver interaction_observer(
      web_contents, web_contents->GetPrimaryMainFrame());
  ::content::SimulateMouseClick(web_contents, 0,
                                blink::WebMouseEvent::Button::kLeft);
  interaction_observer.Wait();

  const BtmRedirectContext& context = wco->CommittedRedirectContext();
  ASSERT_EQ(context.size(), 1u);
  const BtmRedirectInfo& redirect = context[0];
  EXPECT_EQ(redirect.redirector_url, bounce_url);
  EXPECT_THAT(redirect.has_sticky_activation, true);
}

IN_PROC_BROWSER_TEST_P(BtmBounceDetectorBFCacheTest, IsOrWasInPrimaryPage) {
  WebContents* const web_contents = GetActiveWebContents();

  ASSERT_TRUE(NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/empty.html")));
  RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
  EXPECT_TRUE(IsInPrimaryPage(*rfh));
  EXPECT_TRUE(btm::IsOrWasInPrimaryPage(*rfh));
  const GlobalRenderFrameHostId rfh_id = rfh->GetGlobalId();

  ASSERT_TRUE(NavigateToURL(
      web_contents, embedded_test_server()->GetURL("b.test", "/empty.html")));
  // Attempt to get a pointer to the RFH of the a.test page, although
  rfh = RenderFrameHost::FromID(rfh_id);
  if (IsBFCacheEnabled()) {
    // If the bfcache is enabled, the RFH should be in the cache.
    ASSERT_TRUE(rfh);
    EXPECT_TRUE(rfh->IsInLifecycleState(
        RenderFrameHost::LifecycleState::kInBackForwardCache));
    // The page is no longer primary, but it used to be:
    EXPECT_FALSE(IsInPrimaryPage(*rfh));
    EXPECT_TRUE(btm::IsOrWasInPrimaryPage(*rfh));
  } else {
    // If the bfcache is disabled, the RFH may or may not be in memory. If it
    // still is, it's only because it's pending deletion.
    if (rfh) {
      EXPECT_TRUE(rfh->IsInLifecycleState(
          RenderFrameHost::LifecycleState::kPendingDeletion));
      // The page is no longer primary, but it used to be:
      EXPECT_FALSE(IsInPrimaryPage(*rfh));
      EXPECT_TRUE(btm::IsOrWasInPrimaryPage(*rfh));
    }
  }
}

// For waiting until prerendering starts.
class PrerenderingObserver : public WebContentsObserver {
 public:
  explicit PrerenderingObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void Wait() { run_loop_.Run(); }

  GlobalRenderFrameHostId rfh_id() const {
    CHECK(rfh_id_.has_value());
    return rfh_id_.value();
  }

 private:
  base::RunLoop run_loop_;
  std::optional<GlobalRenderFrameHostId> rfh_id_;

  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;
};

void PrerenderingObserver::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  if (render_frame_host->IsInLifecycleState(
          RenderFrameHost::LifecycleState::kPrerendering)) {
    rfh_id_ = render_frame_host->GetGlobalId();
    run_loop_.Quit();
  }
}

// Confirm that IsOrWasInPrimaryPage() returns false for prerendered pages that
// are never activated.
IN_PROC_BROWSER_TEST_P(BtmBounceDetectorBFCacheTest,
                       PrerenderedPagesAreNotPrimary) {
  WebContents* const web_contents = GetActiveWebContents();

  ASSERT_TRUE(NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("a.test", "/empty.html?primary")));

  PrerenderingObserver observer(web_contents);
  ASSERT_TRUE(ExecJs(web_contents, R"(
    const elt = document.createElement('script');
    elt.setAttribute('type', 'speculationrules');
    elt.textContent = JSON.stringify({
      prerender: [{'urls': ['empty.html?prerendered']}]
    });    document.body.appendChild(elt);
  )"));
  observer.Wait();
  ASSERT_FALSE(testing::Test::HasFailure())
      << "Failed waiting for prerendering";

  RenderFrameHost* rfh = RenderFrameHost::FromID(observer.rfh_id());
  ASSERT_TRUE(rfh);
  EXPECT_FALSE(btm::IsOrWasInPrimaryPage(*rfh));

  // Navigating to another site may trigger destruction of the frame.
  ASSERT_TRUE(NavigateToURL(
      web_contents, embedded_test_server()->GetURL("b.test", "/empty.html")));

  rfh = RenderFrameHost::FromID(observer.rfh_id());
  if (rfh) {
    // Even if it's still in memory, it was never primary.
    EXPECT_FALSE(btm::IsOrWasInPrimaryPage(*rfh));
  }
}

// Confirm that IsOrWasInPrimaryPage() returns true for prerendered pages that
// get activated.
IN_PROC_BROWSER_TEST_P(BtmBounceDetectorBFCacheTest,
                       PrerenderedPagesCanBecomePrimary) {
  WebContents* const web_contents = GetActiveWebContents();

  ASSERT_TRUE(NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("a.test", "/empty.html?primary")));

  PrerenderingObserver observer(web_contents);
  ASSERT_TRUE(ExecJs(web_contents, R"(
    const elt = document.createElement('script');
    elt.setAttribute('type', 'speculationrules');
    elt.textContent = JSON.stringify({
      prerender: [{'urls': ['empty.html?prerendered']}]
    });
    document.body.appendChild(elt);
  )"));
  observer.Wait();
  ASSERT_FALSE(testing::Test::HasFailure())
      << "Failed waiting for prerendering";

  RenderFrameHost* rfh = RenderFrameHost::FromID(observer.rfh_id());
  ASSERT_TRUE(rfh);
  EXPECT_FALSE(btm::IsOrWasInPrimaryPage(*rfh));

  // Navigate to the prerendered page.
  ASSERT_TRUE(NavigateToURLFromRenderer(
      web_contents,
      embedded_test_server()->GetURL("a.test", "/empty.html?prerendered")));
  // Navigate to another page, so the prerendered page is no longer active.
  ASSERT_TRUE(NavigateToURL(
      web_contents, embedded_test_server()->GetURL("b.test", "/empty.html")));

  rfh = RenderFrameHost::FromID(observer.rfh_id());
  if (rfh) {
    EXPECT_FALSE(IsInPrimaryPage(*rfh));
    EXPECT_TRUE(btm::IsOrWasInPrimaryPage(*rfh));
  }
}

INSTANTIATE_TEST_SUITE_P(All, BtmBounceDetectorBFCacheTest, ::testing::Bool());

}  // namespace content
