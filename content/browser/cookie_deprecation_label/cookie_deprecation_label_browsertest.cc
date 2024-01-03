// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/containers/contains.h"
#include "base/metrics/histogram_base.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/cookie_deprecation_label/cookie_deprecation_label_test_utils.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

using ::net::test_server::EmbeddedTestServer;

constexpr char kSecCookieDeprecationHeaderStatus[] =
    "Privacy.3PCD.SecCookieDeprecationHeaderStatus";

class CookieDeprecationLabelBrowserTestBase : public ContentBrowserTest {
 public:
  CookieDeprecationLabelBrowserTestBase() = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  WebContents* web_contents() { return shell()->web_contents(); }

 protected:
  void AddImageToDocument(const GURL& src_url) {
    ASSERT_EQ(true,
              EvalJs(shell(),
                     base::StrCat({"((() => { const img = "
                                   "document.createElement('img'); img.src = '",
                                   src_url.spec(), "'; return true; })())"})));
  }

  void FetchWithSecCookieDeprecationHeader(const GURL& url,
                                           const std::string& header_value) {
    ASSERT_EQ(true,
              EvalJs(shell(),
                     base::StrCat({"((() => {"
                                   "const headers = new Headers();"
                                   "headers.append('Sec-Cookie-Deprecation', '",
                                   header_value,
                                   "');"
                                   "fetch('",
                                   url.spec(),
                                   "', { headers });"
                                   "return true; })())"})));
  }

  std::unique_ptr<net::EmbeddedTestServer> CreateTestServer(
      net::test_server::EmbeddedTestServer::Type type) {
    auto server = std::make_unique<net::EmbeddedTestServer>(type);
    server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    server->ServeFilesFromSourceDirectory("content/test/data");
    return server;
  }
};

class CookieDeprecationLabelDisabledBrowserTest
    : public CookieDeprecationLabelBrowserTestBase {
 public:
  CookieDeprecationLabelDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kCookieDeprecationFacilitatedTesting);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CookieDeprecationLabelDisabledBrowserTest,
                       FeatureDisabled_CookieDeprecationLabelHeaderNotAdded) {
  base::HistogramTester histograms;

  auto https_server = CreateTestServer(EmbeddedTestServer::TYPE_HTTPS);
  auto response_a_a =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/a_a");
  auto response_a_b =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/a_b");

  ASSERT_TRUE(https_server->Start());

  ASSERT_TRUE(NavigateToURL(web_contents(),
                            https_server->GetURL("d.test", "/hello.html")));
  AddImageToDocument(/*src_url=*/https_server->GetURL("a.test", "/a_a"));

  // [a.test/a] - Non opted-in request should not receive a label header.
  response_a_a->WaitForRequest();
  ASSERT_FALSE(base::Contains(response_a_a->http_request()->headers,
                              "Sec-Cookie-Deprecation"));
  auto http_response_a_a =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response_a_a->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response_a_a->AddCustomHeader(
      "Location", https_server->GetURL("a.test", "/a_b").spec());
  // a.test opts in to receiving the label.
  http_response_a_a->AddCustomHeader(
      "Set-Cookie",
      "receive-cookie-deprecation=any-value; Secure; HttpOnly; "
      "Path=/; SameSite=None; Partitioned");
  response_a_a->Send(http_response_a_a->ToResponseString());
  response_a_a->Done();

  // [a.test/b] - Even if opted-in, the request should not receive a label
  //              header when the feature is disabled.
  response_a_b->WaitForRequest();
  ASSERT_FALSE(base::Contains(response_a_b->http_request()->headers,
                              "Sec-Cookie-Deprecation"));

  auto http_response_a_b =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response_a_b->set_code(net::HTTP_OK);
  response_a_b->Send(http_response_a_b->ToResponseString());
  response_a_b->Done();

  content::FetchHistogramsFromChildProcesses();
  histograms.ExpectTotalCount(kSecCookieDeprecationHeaderStatus, 0);
}

class CookieDeprecationLabelEnabledBrowserTest
    : public CookieDeprecationLabelBrowserTestBase {
 public:
  CookieDeprecationLabelEnabledBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kCookieDeprecationFacilitatedTesting,
        {{"label", "label_test"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CookieDeprecationLabelEnabledBrowserTest,
                       HeaderAddedOnceOptedIn) {
  base::HistogramTester histograms;
  constexpr base::HistogramBase::Sample kNoCookie = 2;
  constexpr base::HistogramBase::Sample kHeaderSet = 0;

  auto https_server = CreateTestServer(EmbeddedTestServer::TYPE_HTTPS);
  auto response_a_a =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/a_a");
  auto response_a_b =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/a_b");
  auto response_a_c =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/a_c");
  auto response_a_d =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/a_d");
  auto response_a_e =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/a_e");
  auto response_b_a =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/b_a");

  ASSERT_TRUE(https_server->Start());

  GURL initial_page_url = https_server->GetURL("d.test", "/hello.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), initial_page_url));

  base::HistogramBase::Count no_cookie_requests = 0;
  content::FetchHistogramsFromChildProcesses();
  histograms.ExpectBucketCount(kSecCookieDeprecationHeaderStatus, kNoCookie,
                               ++no_cookie_requests);

  AddImageToDocument(/*src_url=*/https_server->GetURL("a.test", "/a_a"));

  // [a.test/a] - Non opted-in request should not receive a label header.
  response_a_a->WaitForRequest();
  ASSERT_FALSE(base::Contains(response_a_a->http_request()->headers,
                              "Sec-Cookie-Deprecation"));

  content::FetchHistogramsFromChildProcesses();
  histograms.ExpectBucketCount(kSecCookieDeprecationHeaderStatus, kNoCookie,
                               ++no_cookie_requests);

  auto http_response_a_a =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response_a_a->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response_a_a->AddCustomHeader(
      "Location", https_server->GetURL("a.test", "/a_b").spec());
  // a.test opts in to receiving the label.
  http_response_a_a->AddCustomHeader(
      "Set-Cookie",
      "receive-cookie-deprecation=any-value; Secure; HttpOnly; "
      "Path=/; SameSite=None; Partitioned");
  response_a_a->Send(http_response_a_a->ToResponseString());
  response_a_a->Done();

  // [a.test/b] - Opted-in requests should receive a label header.
  response_a_b->WaitForRequest();
  ASSERT_TRUE(base::Contains(response_a_b->http_request()->headers,
                             "Sec-Cookie-Deprecation"));
  base::HistogramBase::Count header_set_requests = 0;
  content::FetchHistogramsFromChildProcesses();
  histograms.ExpectBucketCount(kSecCookieDeprecationHeaderStatus, kHeaderSet,
                               ++header_set_requests);
  EXPECT_EQ(response_a_b->http_request()->headers.at("Sec-Cookie-Deprecation"),
            "label_test");

  auto http_response_a_b =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response_a_b->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response_a_b->AddCustomHeader(
      "Location", https_server->GetURL("b.test", "/b_a").spec());
  response_a_b->Send(http_response_a_b->ToResponseString());
  response_a_b->Done();

  // [b.test/a] - Redirection request to non opted-in sites should not receive a
  //              label header.
  response_b_a->WaitForRequest();
  ASSERT_FALSE(base::Contains(response_b_a->http_request()->headers,
                              "Sec-Cookie-Deprecation"));
  content::FetchHistogramsFromChildProcesses();
  histograms.ExpectBucketCount(kSecCookieDeprecationHeaderStatus, kNoCookie,
                               ++no_cookie_requests);

  auto http_response_b_a =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response_b_a->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response_b_a->AddCustomHeader(
      "Location", https_server->GetURL("a.test", "/a_c").spec());
  response_b_a->Send(http_response_b_a->ToResponseString());
  response_b_a->Done();

  // [a.test/c] - Redirection to an opted-in site following a non opted-in site
  //              should include the label header.
  response_a_c->WaitForRequest();
  ASSERT_TRUE(base::Contains(response_a_c->http_request()->headers,
                             "Sec-Cookie-Deprecation"));
  content::FetchHistogramsFromChildProcesses();
  histograms.ExpectBucketCount(kSecCookieDeprecationHeaderStatus, kHeaderSet,
                               ++header_set_requests);
  EXPECT_EQ(response_a_c->http_request()->headers.at("Sec-Cookie-Deprecation"),
            "label_test");

  auto http_response_a_c =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response_a_c->set_code(net::HTTP_OK);
  response_a_c->Send(http_response_a_c->ToResponseString());
  response_a_c->Done();

  // Make a second request from the same top level site
  ASSERT_TRUE(NavigateToURL(web_contents(), initial_page_url));
  AddImageToDocument(/*src_url=*/https_server->GetURL("a.test", "/a_d"));
  // [a.test/d] - a.test was previously opted-in, it should receive the header.
  response_a_d->WaitForRequest();
  ASSERT_TRUE(base::Contains(response_a_d->http_request()->headers,
                             "Sec-Cookie-Deprecation"));
  content::FetchHistogramsFromChildProcesses();
  histograms.ExpectBucketCount(kSecCookieDeprecationHeaderStatus, kHeaderSet,
                               ++header_set_requests);
  EXPECT_EQ(response_a_d->http_request()->headers.at("Sec-Cookie-Deprecation"),
            "label_test");
  auto http_response_a_d =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response_a_d->set_code(net::HTTP_OK);
  response_a_d->Send(http_response_a_d->ToResponseString());
  response_a_d->Done();

  // Make a third requests from a different top level site
  ASSERT_TRUE(NavigateToURL(web_contents(),
                            https_server->GetURL("a.test", "/hello.html")));
  AddImageToDocument(/*src_url=*/https_server->GetURL("a.test", "/a_e"));
  // [a.test/e] - a.test was previously opted-in but from the d.test top level
  //              site. Given the current top level site is now a.test, it
  //              should not receive the header.
  response_a_e->WaitForRequest();
  ASSERT_FALSE(base::Contains(response_a_e->http_request()->headers,
                              "Sec-Cookie-Deprecation"));
  content::FetchHistogramsFromChildProcesses();
  histograms.ExpectBucketCount(kSecCookieDeprecationHeaderStatus, kHeaderSet,
                               header_set_requests);
  histograms.ExpectBucketCount(
      kSecCookieDeprecationHeaderStatus, kNoCookie,
      no_cookie_requests + 3);  // 2 navigations and 1 image
  auto http_response_a_e =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response_a_e->set_code(net::HTTP_OK);
  response_a_e->Send(http_response_a_e->ToResponseString());
  response_a_e->Done();
}

IN_PROC_BROWSER_TEST_F(CookieDeprecationLabelEnabledBrowserTest,
                       HeaderCanOnlyBeSetViaOptInCookie) {
  auto https_server = CreateTestServer(EmbeddedTestServer::TYPE_HTTPS);
  auto response_a_a =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/a_a");
  auto response_a_b =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/a_b");

  ASSERT_TRUE(https_server->Start());

  GURL initial_page_url = https_server->GetURL("a.test", "/hello.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), initial_page_url));
  FetchWithSecCookieDeprecationHeader(
      /*url=*/https_server->GetURL("a.test", "/a_a"),
      /*header_value=*/"not_label_test");

  response_a_a->WaitForRequest();
  // The header cannot be added unless it set using the opt-in cookie.
  ASSERT_FALSE(base::Contains(response_a_a->http_request()->headers,
                              "Sec-Cookie-Deprecation"));
  auto http_response_a_a =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response_a_a->set_code(net::HTTP_OK);
  // a.test opts in to receiving the label.
  http_response_a_a->AddCustomHeader(
      "Set-Cookie",
      "receive-cookie-deprecation=any-value; Secure; HttpOnly; "
      "Path=/; SameSite=None; Partitioned");
  response_a_a->Send(http_response_a_a->ToResponseString());
  response_a_a->Done();

  FetchWithSecCookieDeprecationHeader(
      /*url=*/https_server->GetURL("a.test", "/a_b"),
      /*header_value=*/"not_label_test");

  response_a_b->WaitForRequest();
  ASSERT_TRUE(base::Contains(response_a_b->http_request()->headers,
                             "Sec-Cookie-Deprecation"));
  // The header value is the one configured for the feature not the value
  // manually set on the request.
  EXPECT_EQ(response_a_b->http_request()->headers.at("Sec-Cookie-Deprecation"),
            "label_test");

  auto http_response_a_b =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response_a_b->set_code(net::HTTP_OK);
  response_a_b->Send(http_response_a_b->ToResponseString());
  response_a_b->Done();
}

IN_PROC_BROWSER_TEST_F(CookieDeprecationLabelEnabledBrowserTest,
                       OptedInRedirectChain_HeaderAdded) {
  auto https_server = CreateTestServer(EmbeddedTestServer::TYPE_HTTPS);
  auto response_a_a =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/a_a");
  auto response_a_b =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/a_b");
  auto response_a_c =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/a_c");

  ASSERT_TRUE(https_server->Start());

  GURL initial_page_url = https_server->GetURL("d.test", "/hello.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), initial_page_url));
  AddImageToDocument(/*src_url=*/https_server->GetURL("a.test", "/a_a"));

  response_a_a->WaitForRequest();
  ASSERT_FALSE(base::Contains(response_a_a->http_request()->headers,
                              "Sec-Cookie-Deprecation"));
  auto http_response_a_a =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response_a_a->set_code(net::HTTP_MOVED_PERMANENTLY);
  // Redirect without opting-in to receiving the label.
  http_response_a_a->AddCustomHeader(
      "Location", https_server->GetURL("a.test", "/a_b").spec());
  response_a_a->Send(http_response_a_a->ToResponseString());
  response_a_a->Done();

  response_a_b->WaitForRequest();
  ASSERT_FALSE(base::Contains(response_a_b->http_request()->headers,
                              "Sec-Cookie-Deprecation"));
  auto http_response_a_b =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response_a_b->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response_a_b->AddCustomHeader(
      "Location", https_server->GetURL("a.test", "/a_c").spec());
  // Opt-in without specifying the Path
  http_response_a_b->AddCustomHeader(
      "Set-Cookie",
      "receive-cookie-deprecation=any-value; Secure; HttpOnly; "
      "SameSite=None; Partitioned");
  response_a_b->Send(http_response_a_b->ToResponseString());
  response_a_b->Done();

  response_a_c->WaitForRequest();
  ASSERT_TRUE(base::Contains(response_a_c->http_request()->headers,
                             "Sec-Cookie-Deprecation"));
  EXPECT_EQ(response_a_c->http_request()->headers.at("Sec-Cookie-Deprecation"),
            "label_test");
  auto http_response_a_c =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response_a_c->set_code(net::HTTP_OK);
  response_a_c->Send(http_response_a_c->ToResponseString());
  response_a_c->Done();
}

IN_PROC_BROWSER_TEST_F(CookieDeprecationLabelEnabledBrowserTest,
                       InvalidOptInCookie_HeaderNotAdded) {
  const struct {
    const char* description;
    const char* header_value;
  } kTestCases[] = {
      {"Not Secure",
       "receive-cookie-deprecation=any-value; HttpOnly; Path=/; SameSite=None; "
       "Partitioned"},
      {"Not HttpOnly",
       "receive-cookie-deprecation=any-value; Secure; Path=/; SameSite=None; "
       "Partitioned"},
      {"Not Partitioned",
       "receive-cookie-deprecation=any-value; Secure; HttpOnly; Path=/; "
       "SameSite=None;"},
      {"Default SameSite",
       "receive-cookie-deprecation=any-value; HttpOnly; Path=/; Secure; "
       "Partitioned"},
      {"Non matching Path",
       "receive-cookie-deprecation=any-value; Secure; HttpOnly; Secure; "
       "Path=/non-matching; SameSite=None; Partitioned"},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);

    auto https_server = CreateTestServer(EmbeddedTestServer::TYPE_HTTPS);
    auto response_a_a =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            https_server.get(), "/a_a");
    auto response_a_b =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            https_server.get(), "/a_b");
    ASSERT_TRUE(https_server->Start());

    GURL initial_page_url = https_server->GetURL("d.test", "/hello.html");
    ASSERT_TRUE(NavigateToURL(web_contents(), initial_page_url));
    AddImageToDocument(/*src_url=*/https_server->GetURL("a.test", "/a_a"));

    // [a.test/a] - Non opted-in request should not receive a label header.
    response_a_a->WaitForRequest();
    ASSERT_FALSE(base::Contains(response_a_a->http_request()->headers,
                                "Sec-Cookie-Deprecation"));
    auto http_response_a_a =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response_a_a->set_code(net::HTTP_MOVED_PERMANENTLY);
    http_response_a_a->AddCustomHeader(
        "Location", https_server->GetURL("a.test", "/a_b").spec());
    // a.test opts in to receiving the label.
    http_response_a_a->AddCustomHeader("Set-Cookie", test_case.header_value);
    response_a_a->Send(http_response_a_a->ToResponseString());
    response_a_a->Done();

    // [a.test/b] - The request should not receive the header as the opt-in
    // cookie was not valid.
    response_a_b->WaitForRequest();
    ASSERT_FALSE(base::Contains(response_a_b->http_request()->headers,
                                "Sec-Cookie-Deprecation"));
    auto http_response_a_b =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response_a_b->set_code(net::HTTP_OK);
    response_a_b->Send(http_response_a_b->ToResponseString());
    response_a_b->Done();
  }
}

IN_PROC_BROWSER_TEST_F(CookieDeprecationLabelEnabledBrowserTest,
                       RequestNotSecure_HeaderNotAdded) {
  auto https_server = CreateTestServer(EmbeddedTestServer::TYPE_HTTPS);
  auto http_server = CreateTestServer(EmbeddedTestServer::TYPE_HTTP);

  auto response_a_a =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/a_a");
  auto response_a_b =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/a_b");
  auto response_a_c =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          http_server.get(), "/a_c");

  ASSERT_TRUE(http_server->Start());
  ASSERT_TRUE(https_server->Start());

  // Setup the cookie for a.test and confirm that we receive it over https.
  ASSERT_TRUE(
      NavigateToURL(shell(), https_server->GetURL("a.test", "/hello.html")));
  AddImageToDocument(/*src_url=*/https_server->GetURL("a.test", "/a_a"));
  response_a_a->WaitForRequest();
  ASSERT_FALSE(base::Contains(response_a_a->http_request()->headers,
                              "Sec-Cookie-Deprecation"));
  auto http_response_a_a =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response_a_a->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response_a_a->AddCustomHeader(
      "Set-Cookie",
      "receive-cookie-deprecation=any-value; Secure; HttpOnly; "
      "Path=/; SameSite=None; Partitioned");
  http_response_a_a->AddCustomHeader(
      "Location", https_server->GetURL("a.test", "/a_b").spec());
  response_a_a->Send(http_response_a_a->ToResponseString());
  response_a_a->Done();
  response_a_b->WaitForRequest();
  ASSERT_TRUE(base::Contains(response_a_b->http_request()->headers,
                             "Sec-Cookie-Deprecation"));
  auto http_response_a_b =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response_a_b->set_code(net::HTTP_OK);
  response_a_b->Send(http_response_a_b->ToResponseString());
  response_a_b->Done();

  // Confirms that the header is not sent for a.test over http.
  ASSERT_TRUE(
      NavigateToURL(shell(), http_server->GetURL("a.test", "/hello.html")));
  AddImageToDocument(/*src_url=*/http_server->GetURL("a.test", "/a_c"));
  response_a_c->WaitForRequest();
  ASSERT_FALSE(base::Contains(response_a_c->http_request()->headers,
                              "Sec-Cookie-Deprecation"));
  auto http_response_a_c =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response_a_c->set_code(net::HTTP_OK);
  response_a_c->Send(http_response_a_c->ToResponseString());
  response_a_c->Done();
}

IN_PROC_BROWSER_TEST_F(CookieDeprecationLabelEnabledBrowserTest,
                       NotAllowed_EmptyLabelReturned) {
  MockCookieDeprecationLabelContentBrowserClientBase<
      ContentBrowserTestContentBrowserClient>
      browser_client;
  EXPECT_CALL(browser_client, IsCookieDeprecationLabelAllowedForContext)
      .WillOnce(testing::Return(false));

  auto https_server = CreateTestServer(EmbeddedTestServer::TYPE_HTTPS);
  ASSERT_TRUE(https_server->Start());

  EXPECT_TRUE(
      NavigateToURL(shell(), https_server->GetURL("a.test", "/hello.html")));
  EXPECT_EQ(EvalJs(shell(), R"((async () => {
        return await navigator.cookieDeprecationLabel.getValue();
      })())"),
            "");
}

IN_PROC_BROWSER_TEST_F(CookieDeprecationLabelEnabledBrowserTest,
                       Allowed_LabelReturned) {
  MockCookieDeprecationLabelContentBrowserClientBase<
      ContentBrowserTestContentBrowserClient>
      browser_client;
  EXPECT_CALL(browser_client, IsCookieDeprecationLabelAllowedForContext)
      .WillOnce(testing::Return(true));

  auto https_server = CreateTestServer(EmbeddedTestServer::TYPE_HTTPS);
  ASSERT_TRUE(https_server->Start());

  EXPECT_TRUE(
      NavigateToURL(shell(), https_server->GetURL("a.test", "/hello.html")));
  EXPECT_EQ(EvalJs(shell(), R"((async () => {
        return await navigator.cookieDeprecationLabel.getValue();
      })())"),
            "label_test");
}

IN_PROC_BROWSER_TEST_F(CookieDeprecationLabelEnabledBrowserTest,
                       OffTheRecord_EmptyLabelReturned) {
  auto https_server = CreateTestServer(EmbeddedTestServer::TYPE_HTTPS);
  ASSERT_TRUE(https_server->Start());

  auto* incognito_shell = CreateOffTheRecordBrowser();

  EXPECT_TRUE(NavigateToURL(incognito_shell,
                            https_server->GetURL("a.test", "/hello.html")));
  EXPECT_EQ(EvalJs(incognito_shell, R"((async () => {
        return await navigator.cookieDeprecationLabel.getValue();
      })())"),
            "");
}

class CookieDeprecationLabelEnabledEmptyLabelBrowserTest
    : public CookieDeprecationLabelBrowserTestBase {
 public:
  CookieDeprecationLabelEnabledEmptyLabelBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kCookieDeprecationFacilitatedTesting, {{"label", ""}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CookieDeprecationLabelEnabledEmptyLabelBrowserTest,
                       EmptyLabel_CookieDeprecationLabelHeaderNotAdded) {
  base::HistogramTester histograms;
  auto https_server = CreateTestServer(EmbeddedTestServer::TYPE_HTTPS);
  auto response_a_a =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/a_a");
  auto response_a_b =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/a_b");

  ASSERT_TRUE(https_server->Start());

  ASSERT_TRUE(NavigateToURL(web_contents(),
                            https_server->GetURL("d.test", "/hello.html")));
  AddImageToDocument(/*src_url=*/https_server->GetURL("a.test", "/a_a"));

  // [a.test/a] - Non opted-in request should not receive a label header.
  response_a_a->WaitForRequest();
  ASSERT_FALSE(base::Contains(response_a_a->http_request()->headers,
                              "Sec-Cookie-Deprecation"));
  auto http_response_a_a =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response_a_a->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response_a_a->AddCustomHeader(
      "Location", https_server->GetURL("a.test", "/a_b").spec());
  // a.test opts in to receiving the label.
  http_response_a_a->AddCustomHeader(
      "Set-Cookie",
      "receive-cookie-deprecation=any-value; Secure; HttpOnly; "
      "Path=/; SameSite=None; Partitioned");
  response_a_a->Send(http_response_a_a->ToResponseString());
  response_a_a->Done();

  // [a.test/b] - Even if opted-in, the request should not receive a label
  //              header when the label is empty.
  response_a_b->WaitForRequest();
  ASSERT_FALSE(base::Contains(response_a_b->http_request()->headers,
                              "Sec-Cookie-Deprecation"));
  // kNoLabel = 1
  content::FetchHistogramsFromChildProcesses();
  // This is a side effect of using an empty label "" as a sentinel value to
  // indicate that the client is not eligible. When it is but the label is
  // empty, it also records `kNoLabel`. 3 requests: nav, img & redirect
  histograms.ExpectBucketCount(kSecCookieDeprecationHeaderStatus, 1, 3);

  auto http_response_a_b =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response_a_b->set_code(net::HTTP_OK);
  response_a_b->Send(http_response_a_b->ToResponseString());
  response_a_b->Done();
}

class CookieDeprecationLabelOffTheRecordEnabledBrowserTest
    : public CookieDeprecationLabelBrowserTestBase {
 public:
  CookieDeprecationLabelOffTheRecordEnabledBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kCookieDeprecationFacilitatedTesting,
        {{"label", "label_test"}, {"enable_otr_profiles", "true"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Ensure that cookie deprecation labels are present in incognito mode if the
// "enable_otr_profiles" feature parameter is true. See also the
// CookieDeprecationLabelEnabledBrowserTest.OffTheRecord_EmptyLabelReturned
// test.
IN_PROC_BROWSER_TEST_F(CookieDeprecationLabelOffTheRecordEnabledBrowserTest,
                       OffTheRecord_LabelReturned) {
  auto https_server = CreateTestServer(EmbeddedTestServer::TYPE_HTTPS);
  ASSERT_TRUE(https_server->Start());

  auto* incognito_shell = CreateOffTheRecordBrowser();

  EXPECT_TRUE(NavigateToURL(incognito_shell,
                            https_server->GetURL("a.test", "/hello.html")));
  EXPECT_EQ(EvalJs(incognito_shell, R"((async () => {
        return await navigator.cookieDeprecationLabel.getValue();
      })())"),
            "label_test");
}

}  // namespace

}  // namespace content
