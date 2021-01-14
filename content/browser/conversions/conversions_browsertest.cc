// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/browser/conversions/conversion_manager_impl.h"
#include "content/browser/conversions/conversion_test_utils.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/test_content_browser_client.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

// Waits for the a given |report_url| to be received by the test server. Wraps a
// ControllableHttpResponse so that it can wait for the server request in a
// thread-safe manner. Therefore, these must be registered prior to |server|
// starting.
struct ExpectedReportWaiter {
  // ControllableHTTPResponses can only wait for relative urls, so only supply
  // the path + query.
  ExpectedReportWaiter(const GURL& report_url, net::EmbeddedTestServer* server)
      : expected_url(report_url),
        response(std::make_unique<net::test_server::ControllableHttpResponse>(
            server,
            report_url.path() + "?" + report_url.query())) {}

  GURL expected_url;
  std::unique_ptr<net::test_server::ControllableHttpResponse> response;

  bool HasRequest() { return !!response->http_request(); }

  // Returns the url for the HttpRequest handled by |response|. This returns a
  // URL formatted with the host defined in the headers. This would not match
  // |expected_url| if the host for report url was not set properly.
  GURL WaitForRequestUrl() {
    if (!response->http_request())
      response->WaitForRequest();

    // The embedded test server resolves all urls to 127.0.0.1, so get the real
    // request host from the request headers.
    const net::test_server::HttpRequest& request = *response->http_request();
    DCHECK(request.headers.find("Host") != request.headers.end());
    const GURL& request_url = request.GetURL();
    GURL header_url = GURL("https://" + request.headers.at("Host"));
    std::string host = header_url.host();
    GURL::Replacements replace_host;
    replace_host.SetHostStr(host);

    // Clear the port as it is assigned by the EmbeddedTestServer at runtime.
    replace_host.SetPortStr("");
    return request_url.ReplaceComponents(replace_host);
  }
};

}  // namespace

class ConversionsBrowserTest : public ContentBrowserTest {
 public:
  ConversionsBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kConversionMeasurement);
    ConversionManagerImpl::RunInMemoryForTesting();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kConversionsDebugMode);

    // Sets up the blink runtime feature for ConversionMeasurement.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    net::test_server::RegisterDefaultHandlers(https_server_.get());
    https_server_->ServeFilesFromSourceDirectory("content/test/data");
    SetupCrossSiteRedirector(https_server_.get());
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

 protected:
  ConversionDisallowingContentBrowserClient disallowed_browser_client_;

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

// Verifies that storage initialization does not hang when initialized in a
// browsertest context, see https://crbug.com/1080764).
IN_PROC_BROWSER_TEST_F(ConversionsBrowserTest,
                       FeatureEnabled_StorageInitWithoutHang) {}

IN_PROC_BROWSER_TEST_F(ConversionsBrowserTest,
                       ImpressionConversion_ReportSent) {
  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL(
          "https://a.test/.well-known/"
          "register-conversion?impression-data=1&conversion-data=7&credit=100"),
      https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/conversions/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  // Create an anchor tag with impression attributes and click the link. By
  // default the target is set to "_top".
  GURL conversion_url = https_server()->GetURL(
      "b.test", "/conversions/page_with_conversion_redirect.html");
  EXPECT_TRUE(
      ExecJs(web_contents(),
             JsReplace(R"(
    createImpressionTag("link" /* id */,
                        $1 /* url */,
                        "1" /* impression data */,
                        $2 /* conversion_destination */);)",
                       conversion_url, url::Origin::Create(conversion_url))));

  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('link');"));
  observer.Wait();

  // Register a conversion with the original page as the reporting origin.
  EXPECT_TRUE(
      ExecJs(web_contents(), JsReplace("registerConversionForOrigin(7, $1)",
                                       url::Origin::Create(impression_url))));

  EXPECT_EQ(expected_report.expected_url, expected_report.WaitForRequestUrl());
}

IN_PROC_BROWSER_TEST_F(ConversionsBrowserTest,
                       WindowOpenImpressionConversion_ReportSent) {
  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL(
          "https://a.test/.well-known/"
          "register-conversion?impression-data=1&conversion-data=7&credit=100"),
      https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/conversions/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  // Create an anchor tag with impression attributes and click the link. By
  // default the target is set to "_top".
  GURL conversion_url = https_server()->GetURL(
      "b.test", "/conversions/page_with_conversion_redirect.html");
  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(
      ExecJs(web_contents(),
             JsReplace(R"(window.open($1, '_top', '',
               {impressionData: '1', conversionDestination: $2});)",
                       conversion_url, url::Origin::Create(conversion_url))));
  observer.Wait();

  // Register a conversion with the original page as the reporting origin.
  EXPECT_TRUE(
      ExecJs(web_contents(), JsReplace("registerConversionForOrigin(7, $1)",
                                       url::Origin::Create(impression_url))));

  EXPECT_EQ(expected_report.expected_url, expected_report.WaitForRequestUrl());
}

IN_PROC_BROWSER_TEST_F(ConversionsBrowserTest,
                       ImpressionFromCrossOriginSubframe_ReportSent) {
  ExpectedReportWaiter expected_report(
      GURL(
          "https://a.test/.well-known/"
          "register-conversion?impression-data=1&conversion-data=7&credit=100"),
      https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL page_url = https_server()->GetURL("a.test", "/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  GURL subframe_url = https_server()->GetURL(
      "c.test", "/conversions/page_with_impression_creator.html");
  EXPECT_TRUE(ExecJs(shell(), R"(
    let frame= document.getElementById('test_iframe');
    frame.setAttribute('allow', 'conversion-measurement');)"));
  NavigateIframeToURL(web_contents(), "test_iframe", subframe_url);
  RenderFrameHost* subframe = ChildFrameAt(web_contents()->GetMainFrame(), 0);

  // Create an impression tag in the subframe and target a popup window.
  GURL conversion_url = https_server()->GetURL(
      "b.test", "/conversions/page_with_conversion_redirect.html");
  EXPECT_TRUE(ExecJs(subframe, JsReplace(R"(
    createImpressionTagWithTarget("link" /* id */,
                        $1 /* url */,
                        "1" /* impression data */,
                        $2 /* conversion_destination */,
                        "new_frame" /* target */);)",
                                         conversion_url,
                                         url::Origin::Create(conversion_url))));

  ShellAddedObserver new_shell_observer;
  TestNavigationObserver observer(nullptr);
  observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(subframe, "simulateClick('link');"));
  WebContents* popup_contents = new_shell_observer.GetShell()->web_contents();
  observer.Wait();

  // Register a conversion with the original page as the reporting origin.
  EXPECT_TRUE(
      ExecJs(popup_contents, JsReplace("registerConversionForOrigin(7, $1)",
                                       url::Origin::Create(page_url))));

  EXPECT_EQ(expected_report.expected_url, expected_report.WaitForRequestUrl());
}

IN_PROC_BROWSER_TEST_F(ConversionsBrowserTest,
                       ImpressionConversionSameDomain_ReportSent) {
  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL(
          "https://a.test/.well-known/"
          "register-conversion?impression-data=1&conversion-data=7&credit=100"),
      https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/conversions/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  // Create an anchor tag with impression attributes and click the link. By
  // default the target is set to "_top".
  GURL conversion_url = https_server()->GetURL(
      "b.test", "/conversions/page_with_conversion_redirect.html");
  GURL conversion_dest_url = https_server()->GetURL(
      "sub.b.test", "/conversions/page_with_conversion_redirect.html");
  EXPECT_TRUE(ExecJs(
      web_contents(),
      JsReplace(R"(
    createImpressionTag("link" /* id */,
                        $1 /* url */,
                        "1" /* impression data */,
                        $2 /* conversion_destination */);)",
                conversion_url, url::Origin::Create(conversion_dest_url))));

  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('link');"));
  observer.Wait();

  // Register a conversion with the original page as the reporting origin.
  EXPECT_TRUE(
      ExecJs(web_contents(), JsReplace("registerConversionForOrigin(7, $1)",
                                       url::Origin::Create(impression_url))));

  EXPECT_EQ(expected_report.expected_url, expected_report.WaitForRequestUrl());
}

IN_PROC_BROWSER_TEST_F(
    ConversionsBrowserTest,
    ConversionOnDifferentSubdomainThanLandingPage_ReportSent) {
  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL(
          "https://a.test/.well-known/"
          "register-conversion?impression-data=1&conversion-data=7&credit=100"),
      https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/conversions/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  // Create an anchor tag with impression attributes and click the link. By
  // default the target is set to "_top".
  GURL conversion_landing_url = https_server()->GetURL(
      "b.test", "/conversions/page_with_conversion_redirect.html");
  GURL conversion_dest_url = https_server()->GetURL(
      "sub.b.test", "/conversions/page_with_conversion_redirect.html");
  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace(R"(
    createImpressionTag("link" /* id */,
                        $1 /* url */,
                        "1" /* impression data */,
                        $2 /* conversion_destination */);)",
                               conversion_landing_url,
                               url::Origin::Create(conversion_dest_url))));

  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('link');"));
  observer.Wait();

  // Navigate to a same domain origin that is different than the landing page
  // for the click and convert there. A report should still be sent.
  GURL conversion_url = https_server()->GetURL(
      "other.b.test", "/conversions/page_with_conversion_redirect.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), conversion_url));

  // Register a conversion with the original page as the reporting origin.
  EXPECT_TRUE(
      ExecJs(web_contents(), JsReplace("registerConversionForOrigin(7, $1)",
                                       url::Origin::Create(impression_url))));

  EXPECT_EQ(expected_report.expected_url, expected_report.WaitForRequestUrl());
}

IN_PROC_BROWSER_TEST_F(
    ConversionsBrowserTest,
    MultipleImpressionsPerConversion_ReportsSentWithAttribution) {
  std::vector<ExpectedReportWaiter> expected_reports;
  expected_reports.emplace_back(
      GURL("https://d.test/.well-known/"
           "register-conversion?impression-data=1&conversion-data=7&credit=0"),
      https_server());
  expected_reports.emplace_back(
      GURL(
          "https://d.test/.well-known/"
          "register-conversion?impression-data=2&conversion-data=7&credit=100"),
      https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL first_impression_url = https_server()->GetURL(
      "a.test", "/conversions/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), first_impression_url));

  GURL second_impression_url = https_server()->GetURL(
      "c.test", "/conversions/page_with_impression_creator.html");
  Shell* shell2 =
      Shell::CreateNewWindow(shell()->web_contents()->GetBrowserContext(),
                             GURL(), nullptr, gfx::Size(100, 100));
  EXPECT_TRUE(NavigateToURL(shell2->web_contents(), second_impression_url));

  // Register impressions from both windows.
  GURL conversion_url = https_server()->GetURL(
      "b.test", "/conversions/page_with_conversion_redirect.html");
  url::Origin reporting_origin =
      url::Origin::Create(https_server()->GetURL("d.test", "/"));
  std::string impression_js = R"(
    createImpressionTagWithReporting("link" /* id */,
                        $1 /* url */,
                        $2 /* impression data */,
                        $3 /* conversion_destination */,
                        $4 /* reporting_origin */);)";

  TestNavigationObserver first_nav_observer(shell()->web_contents());
  EXPECT_TRUE(
      ExecJs(shell(),
             JsReplace(impression_js, conversion_url, "1" /* impression_data */,
                       url::Origin::Create(conversion_url), reporting_origin)));
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('link');"));
  first_nav_observer.Wait();

  TestNavigationObserver second_nav_observer(shell2->web_contents());
  EXPECT_TRUE(
      ExecJs(shell2,
             JsReplace(impression_js, conversion_url, "2" /* impression_data */,
                       url::Origin::Create(conversion_url), reporting_origin)));
  EXPECT_TRUE(ExecJs(shell2, "simulateClick('link');"));
  second_nav_observer.Wait();

  // Register a conversion after both impressions have been registered.
  EXPECT_TRUE(ExecJs(shell2, JsReplace("registerConversionForOrigin(7, $1)",
                                       reporting_origin)));

  for (auto& report : expected_reports) {
    if (!report.response->http_request())
      report.response->WaitForRequest();
    EXPECT_EQ(report.expected_url, report.WaitForRequestUrl());
  }
}

IN_PROC_BROWSER_TEST_F(ConversionsBrowserTest,
                       ConversionRegisteredWithEmbedderDisallow_NoData) {
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&disallowed_browser_client_);

  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL(
          "https://a.test/.well-known/"
          "register-conversion?impression-data=1&conversion-data=7&credit=100"),
      https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/conversions/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  // Create an anchor tag with impression attributes and click the link. By
  // default the target is set to "_top".
  GURL conversion_url = https_server()->GetURL(
      "b.test", "/conversions/page_with_conversion_redirect.html");
  EXPECT_TRUE(
      ExecJs(web_contents(),
             JsReplace(R"(
    createImpressionTag("link" /* id */,
                        $1 /* url */,
                        "1" /* impression data */,
                        $2 /* conversion_destination */);)",
                       conversion_url, url::Origin::Create(conversion_url))));

  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('link');"));
  observer.Wait();

  // Register a conversion with the original page as the reporting origin.
  EXPECT_TRUE(
      ExecJs(web_contents(), JsReplace("registerConversionForOrigin(7, $1)",
                                       url::Origin::Create(impression_url))));

  // Since we want to verify that a report _isn't_ sent, we can't really wait on
  // any event here. The best thing we can do is just impose a short delay and
  // verify the browser didn't send anything. Worst case, this should start
  // flakily failing if the logic breaks.
  base::RunLoop run_loop;
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(100));
  run_loop.Run();
  EXPECT_FALSE(expected_report.HasRequest());

  SetBrowserClientForTesting(old_browser_client);
}

}  // namespace content
