// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_cert_verifier_browser_test.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
namespace {

using ::testing::Eq;

const char kReportingHost[] = "reporting.com";
const char kCrossOriginHost[] = "crossorigin.com";

// This browsertest focuses on testing that Reporting-Endpoints header can be
// parsed before navigation commits and there's no memory leak from the
// transient reporting endpoints created during navigation.
class CrossOriginOpenerPolicyReportingBrowserTest
    : public CertVerifierBrowserTest {
 public:
  CrossOriginOpenerPolicyReportingBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    // Enable COOP and DocumentReporting:
    feature_list_.InitWithFeatures(
        // Enabled features:
        {network::features::kCrossOriginOpenerPolicy,
         net::features::kDocumentReporting},
        // Disabled features:
        {});
  }

  int32_t reports_uploaded() {
    base::AutoLock auto_lock(lock_);
    return reports_uploaded_;
  }
  net::EmbeddedTestServer* https_server() { return &https_server_; }
  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

 private:
  void SetUp() override { ContentBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    mock_cert_verifier()->set_default_result(net::OK);

    {
      base::AutoLock auto_lock(lock_);
      reports_uploaded_ = 0;
    }

    host_resolver()->AddRule("*", "127.0.0.1");

    https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
    SetupCrossSiteRedirector(https_server());
    net::test_server::RegisterDefaultHandlers(&https_server_);
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/upload",
        base::BindRepeating(
            &CrossOriginOpenerPolicyReportingBrowserTest::ReportsUploadHandler,
            base::Unretained(this))));
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/redirect-coop-reporting",
        base::BindRepeating(&CrossOriginOpenerPolicyReportingBrowserTest::
                                COOPReportingRedirectHandler,
                            base::Unretained(this))));
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/coop-reporting",
        base::BindRepeating(&CrossOriginOpenerPolicyReportingBrowserTest::
                                COOPReportingNavHandler,
                            base::Unretained(this))));
    ASSERT_TRUE(https_server_.Start());
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(https_server_.ShutdownAndWaitUntilComplete());
  }

  GURL GetCollectorURL() const {
    return https_server_.GetURL(kReportingHost, "/upload");
  }

  // Send response with Reporting-Endpoints headers and optional COOP if `coop`
  // query parameter is present, redirecting to the URL specified in request
  // query `dest` parameter.
  std::unique_ptr<net::test_server::HttpResponse> COOPReportingRedirectHandler(
      const net::test_server::HttpRequest& request) {
    GURL request_url = request.GetURL();
    net::test_server::RequestQuery query =
        net::test_server::ParseQuery(request_url);

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HttpStatusCode::HTTP_FOUND);
    http_response->AddCustomHeader("Location", query["dest"].front());
    if (query.find("coop") != query.end()) {
      http_response->AddCustomHeader("Cross-Origin-Opener-Policy",
                                     "same-origin; report-to=\"default\"");
    }
    http_response->AddCustomHeader(
        "Reporting-Endpoints", "default=\"" + GetCollectorURL().spec() + "\"");
    return http_response;
  }

  // Send response with COOP and Reporting-Endpoints headers, content set to
  // the request url.
  std::unique_ptr<net::test_server::HttpResponse> COOPReportingNavHandler(
      const net::test_server::HttpRequest& request) {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HttpStatusCode::HTTP_OK);
    http_response->AddCustomHeader("Cross-Origin-Opener-Policy",
                                   "same-origin; report-to=\"default\"");
    http_response->AddCustomHeader(
        "Reporting-Endpoints", "default=\"" + GetCollectorURL().spec() + "\"");
    http_response->set_content_type("text/plain");
    http_response->set_content("OK");
    return http_response;
  }

  // Handles reports uploaded to the server and increment reports upload count.
  std::unique_ptr<net::test_server::HttpResponse> ReportsUploadHandler(
      const net::test_server::HttpRequest& request) {
    {
      base::AutoLock auto_lock(lock_);
      reports_uploaded_++;
    }
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HttpStatusCode::HTTP_NO_CONTENT);
    return http_response;
  }

  net::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList feature_list_;
  base::Lock lock_;
  int32_t GUARDED_BY(lock_) reports_uploaded_ = 0;
};

}  // namespace

// This observer manually queues reports (type doesn't matter) for each
// transient reporting source created during this navigation after this
// navigation has finished. Such reports must be dropped because transient
// reporting sources should be marked as expired at this point.
class SendTestReportsAtNavigationFinishObserver : public WebContentsObserver {
 public:
  SendTestReportsAtNavigationFinishObserver(WebContents* web_contents,
                                            const GURL& url)
      : WebContentsObserver(web_contents), url_(url) {}

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    auto* request = NavigationRequest::From(navigation_handle);
    reporting_sources_ =
        request->coop_status().TransientReportingSourcesForTesting();
  }

  void DidFinishLoad(RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    net::SchemefulSite site = net::SchemefulSite(url_);
    auto* network_context =
        render_frame_host->GetStoragePartition()->GetNetworkContext();
    // Queue reports using transient reporting sources. These sources should be
    // marked as expired now so we expect no reports being sent out.
    for (const base::UnguessableToken& reporting_source : reporting_sources_) {
      network_context->QueueReport(
          "type", "default", url_, reporting_source,
          net::NetworkAnonymizationKey::CreateSameSite(site),
          base::Value::Dict());
    }
  }

 private:
  const GURL url_;
  std::vector<base::UnguessableToken> reporting_sources_;
};

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyReportingBrowserTest,
                       BasicReportingEndpointsReportSucceedOnOpener) {
  EXPECT_TRUE(NavigateToURL(
      shell(), https_server()->GetURL(kReportingHost, "/coop-reporting")));
  GURL first_popup(https_server()->GetURL(kReportingHost, "/echo"));
  GURL second_popup(https_server()->GetURL("a.com", "/echo"));

  // Open a popup so we have more active top level documents in browsing context
  // group. This ensures second popup will generate a report.
  OpenPopup(shell(), first_popup, "first");

  // Create a second popup to trigger coop enforcement.
  OpenPopup(shell(), second_popup, "second");

  EXPECT_TRUE(ExecJs(web_contents(), R"(
    new Promise(r => setTimeout(r, 1000));
  )"));
  // Opening second popup should produce one navigation-from-response report
  // queued by opener's reporter.
  EXPECT_THAT(reports_uploaded(), Eq(1));
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyReportingBrowserTest,
                       ParseReportingEndpointsDuringNavigation) {
  EXPECT_TRUE(NavigateToURL(shell(), https_server()->GetURL("a.com", "/echo")));

  GURL url(https_server()->GetURL(kReportingHost, "/coop-reporting"));

  // Create a new popup. Navigate it later after installing the navigation
  // observer inside.
  WebContentsAddedObserver popup_observer;
  ASSERT_TRUE(ExecJs(shell(), "popup = window.open()"));
  WebContents* popup = popup_observer.GetWebContents();
  SendTestReportsAtNavigationFinishObserver send_reports_observer(popup, url);
  ASSERT_TRUE(ExecJs(shell(), JsReplace("popup.location.href = $1;", url)));
  WaitForLoadStop(popup);

  EXPECT_TRUE(ExecJs(web_contents(), R"(
    new Promise(r => setTimeout(r, 1000));
  )"));
  // Opening /coop-reporting should produce one navigation-to-response report,
  // reports queued against transient reporting sources will be dropped after
  // navigation completes.
  EXPECT_THAT(reports_uploaded(), Eq(1));
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyReportingBrowserTest,
                       ParseReportingEndpointsDuringRedirects) {
  // Opener COOP matches the redirect COOP.
  EXPECT_TRUE(NavigateToURL(
      shell(), https_server()->GetURL(
                   kReportingHost,
                   "/set-header?cross-origin-opener-policy: same-origin")));

  GURL redirect_final_page(https_server()->GetURL(kCrossOriginHost, "/echo"));
  // Redirect header contains Reporting-Endpoints and COOP.
  GURL redirect_initial_page(https_server()->GetURL(
      kReportingHost,
      "/redirect-coop-reporting?coop&dest=" + redirect_final_page.spec()));

  // Create a new popup. Navigate it later after installing the navigation
  // observer inside.
  WebContentsAddedObserver popup_observer;
  ASSERT_TRUE(ExecJs(shell(), "popup = window.open()"));
  WebContents* popup = popup_observer.GetWebContents();
  SendTestReportsAtNavigationFinishObserver send_reports_observer(
      popup, redirect_initial_page);
  ASSERT_TRUE(ExecJs(
      shell(), JsReplace("popup.location.href = $1;", redirect_initial_page)));
  WaitForLoadStop(popup);

  EXPECT_TRUE(ExecJs(web_contents(), R"(
    new Promise(r => setTimeout(r, 1000));
  )"));
  // Redirecting should produce one navigation-from-response as only the
  // redirect header contains coop and reporting, reports queued against
  // transient reporting sources will be dropped after navigation completes.
  EXPECT_THAT(reports_uploaded(), Eq(1));
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyReportingBrowserTest,
                       RedirectHeadersRemovedAfterNavigationComplete) {
  EXPECT_TRUE(
      NavigateToURL(shell(), https_server()->GetURL(kReportingHost, "/echo")));

  GURL redirect_final_page(https_server()->GetURL(kReportingHost, "/echo"));
  // Redirect header only contains Reporting-Endpoints, no COOP configured.
  GURL redirect_initial_page(https_server()->GetURL(
      kReportingHost,
      "/redirect-coop-reporting?dest=" + redirect_final_page.spec()));

  // Create a new popup. Navigate it later after installing the navigation
  // observer inside.
  WebContentsAddedObserver popup_observer;
  ASSERT_TRUE(ExecJs(shell(), "popup = window.open()"));
  WebContents* popup = popup_observer.GetWebContents();
  SendTestReportsAtNavigationFinishObserver send_reports_observer(
      popup, redirect_initial_page);
  ASSERT_TRUE(ExecJs(
      shell(), JsReplace("popup.location.href = $1;", redirect_initial_page)));
  WaitForLoadStop(popup);

  EXPECT_TRUE(ExecJs(web_contents(), R"(
    new Promise(r => setTimeout(r, 1000));
  )"));
  // Redirecting should produce zero reports (no COOP configured), reports
  // queued against transient reporting sources will be dropped after navigation
  // completes.
  EXPECT_THAT(reports_uploaded(), Eq(0));
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyReportingBrowserTest,
                       RedirectHeadersRemovedAfterNavigationFailed) {
  EXPECT_TRUE(
      NavigateToURL(shell(), https_server()->GetURL(kReportingHost, "/echo")));

  GURL redirect_final_page(https_server()->GetURL("a.com", "/fail"));
  // Redirect header only contains Reporting-Endpoints, no COOP configured.
  GURL redirect_initial_page(https_server()->GetURL(
      kReportingHost,
      "/redirect-coop-reporting?dest=" + redirect_final_page.spec()));

  // Create a new popup. Navigate it later after installing the navigation
  // observer inside.
  WebContentsAddedObserver popup_observer;
  ASSERT_TRUE(ExecJs(shell(), "popup = window.open()"));
  WebContents* popup = popup_observer.GetWebContents();
  SendTestReportsAtNavigationFinishObserver send_reports_observer(
      popup, redirect_initial_page);
  ASSERT_TRUE(ExecJs(
      shell(), JsReplace("popup.location.href = $1;", redirect_initial_page)));
  WaitForLoadStop(popup);

  EXPECT_TRUE(ExecJs(web_contents(), R"(
    new Promise(r => setTimeout(r, 1000));
  )"));
  // Redirecting should produce zero report, reports queued against transient
  // reporting sources will be dropped after navigation failed.
  EXPECT_THAT(reports_uploaded(), Eq(0));
}

}  // namespace content
