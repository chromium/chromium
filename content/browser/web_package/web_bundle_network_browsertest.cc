// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/web_package/web_bundle_browsertest_base.h"
#include "content/browser/web_package/web_bundle_utils.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"

namespace content {

class WebBundleNetworkBrowserTest
    : public web_bundle_browsertest_utils::WebBundleBrowserTestBase {
 public:
  WebBundleNetworkBrowserTest(const WebBundleNetworkBrowserTest&) = delete;
  WebBundleNetworkBrowserTest& operator=(const WebBundleNetworkBrowserTest&) =
      delete;

 protected:
  WebBundleNetworkBrowserTest() = default;
  ~WebBundleNetworkBrowserTest() override = default;

  void SetUpOnMainThread() override {
    WebBundleBrowserTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void TearDownOnMainThread() override {
    // Shutdown the server to avoid the data race of |headers_| and |contents_|
    // caused by page reload on error.
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    WebBundleBrowserTestBase::TearDownOnMainThread();
  }

  void SetUp() override {
    feature_list_.InitWithFeatures({features::kWebBundlesFromNetwork}, {});
    WebBundleBrowserTestBase::SetUp();
  }

  void RegisterRequestHandler(const std::string& relative_url) {
    embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
        [this, relative_url](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url != relative_url)
            return nullptr;
          return std::make_unique<net::test_server::RawHttpResponse>(headers_,
                                                                     contents_);
        }));
  }

  void TestNavigationFailure(const GURL& url,
                             const std::string& expected_console_error) {
    std::string console_message = web_bundle_browsertest_utils::
        ExpectNavigationFailureAndReturnConsoleMessage(shell()->web_contents(),
                                                       url);
    EXPECT_EQ(expected_console_error, console_message);
  }

  void HistoryBackAndWaitUntilConsoleError(
      const std::string& expected_error_message) {
    WebContents* web_contents = shell()->web_contents();
    WebContentsConsoleObserver console_observer(web_contents);

    base::RunLoop run_loop;
    web_bundle_browsertest_utils::FinishNavigationObserver
        finish_navigation_observer(web_contents, run_loop.QuitClosure());
    EXPECT_TRUE(ExecJs(web_contents, "history.back();"));

    run_loop.Run();
    ASSERT_TRUE(finish_navigation_observer.error_code());
    EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
              *finish_navigation_observer.error_code());

    if (console_observer.messages().empty())
      ASSERT_TRUE(console_observer.Wait());

    ASSERT_FALSE(console_observer.messages().empty());
    EXPECT_EQ(expected_error_message,
              base::UTF16ToUTF8(console_observer.messages()[0].message));
  }

  void SetHeaders(const std::string& headers) { headers_ = headers; }
  void AddHeaders(const std::string& headers) { headers_ += headers; }
  void SetContents(const std::string& contents) { contents_ = contents; }
  const std::string& contents() const { return contents_; }

  void RunSharedNavigationTest(
      void (*setup_func)(net::EmbeddedTestServer*, GURL*, std::string*),
      void (*run_test_func)(WebContents*,
                            const GURL&,
                            const GURL&,
                            base::RepeatingCallback<GURL(const GURL&)>)) {
    const std::string wbn_path = "/test.wbn";
    RegisterRequestHandler(wbn_path);
    GURL url_origin;
    std::string web_bundle_content;
    (*setup_func)(embedded_test_server(), &url_origin, &web_bundle_content);
    SetContents(web_bundle_content);

    (*run_test_func)(shell()->web_contents(), url_origin.Resolve(wbn_path),
                     url_origin,
                     base::BindRepeating([](const GURL& url) { return url; }));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::string headers_ = web_bundle_browsertest_utils::kDefaultHeaders;
  std::string contents_;
};

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, Simple) {
  const std::string wbn_path = "/web_bundle/test.wbn";
  const std::string primary_url_path = "/web_bundle/test.html";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL wbn_url = embedded_test_server()->GetURL(wbn_path);
  const GURL primary_url = embedded_test_server()->GetURL(primary_url_path);

  SetContents(web_bundle_browsertest_utils::CreateSimpleWebBundle(primary_url));
  NavigateToBundleAndWaitForReady(wbn_url, primary_url);
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, SimpleWithScript) {
  const std::string wbn_path = "/web_bundle/test.wbn";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL wbn_url = embedded_test_server()->GetURL(wbn_path);
  const GURL primary_url =
      embedded_test_server()->GetURL("/web_bundle/test.html");
  const GURL script_url =
      embedded_test_server()->GetURL("/web_bundle/script.js");

  web_package::WebBundleBuilder builder;
  builder.AddPrimaryURL(primary_url);
  builder.AddExchange(primary_url,
                      {{":status", "200"}, {"content-type", "text/html"}},
                      "<script src=\"script.js\"></script>");
  builder.AddExchange(
      script_url,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "document.title = 'Ready';");

  std::vector<uint8_t> bundle = builder.CreateBundle();
  SetContents(std::string(bundle.begin(), bundle.end()));
  NavigateToBundleAndWaitForReady(wbn_url, primary_url);
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, Download) {
  const std::string wbn_path = "/web_bundle/test.wbn";
  const std::string primary_url_path = "/web_bundle/test.html";
  RegisterRequestHandler(wbn_path);
  AddHeaders("Content-Disposition:attachment; filename=test.wbn\n");
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL wbn_url = embedded_test_server()->GetURL(wbn_path);
  const GURL primary_url = embedded_test_server()->GetURL(primary_url_path);

  SetContents(web_bundle_browsertest_utils::CreateSimpleWebBundle(primary_url));
  WebContents* web_contents = shell()->web_contents();
  std::unique_ptr<web_bundle_browsertest_utils::DownloadObserver>
      download_observer =
          std::make_unique<web_bundle_browsertest_utils::DownloadObserver>(
              web_contents->GetBrowserContext()->GetDownloadManager());

  EXPECT_FALSE(NavigateToURL(web_contents, wbn_url));
  download_observer->WaitUntilDownloadCreated();
  EXPECT_EQ(wbn_url, download_observer->observed_url());
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, ContentLength) {
  const std::string wbn_path = "/web_bundle/test.wbn";
  const std::string primary_url_path = "/web_bundle/test.html";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL wbn_url = embedded_test_server()->GetURL(wbn_path);
  const GURL primary_url = embedded_test_server()->GetURL(primary_url_path);

  SetContents(web_bundle_browsertest_utils::CreateSimpleWebBundle(primary_url));
  AddHeaders(
      base::StringPrintf("Content-Length: %" PRIuS "\n", contents().size()));
  NavigateToBundleAndWaitForReady(wbn_url, primary_url);
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, NonSecureUrl) {
  const std::string wbn_path = "/web_bundle/test.wbn";
  const std::string primary_url_path = "/web_bundle/test.html";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL wbn_url = embedded_test_server()->GetURL("example.com", wbn_path);
  const GURL primary_url =
      embedded_test_server()->GetURL("example.com", primary_url_path);
  SetContents(web_bundle_browsertest_utils::CreateSimpleWebBundle(primary_url));
  TestNavigationFailure(
      wbn_url,
      "Web Bundle response must be served from HTTPS or localhost HTTP.");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, MissingNosniff) {
  const std::string wbn_path = "/web_bundle/test.wbn";
  const std::string primary_url_path = "/web_bundle/test.html";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL wbn_url = embedded_test_server()->GetURL(wbn_path);
  const GURL primary_url = embedded_test_server()->GetURL(primary_url_path);

  SetContents(web_bundle_browsertest_utils::CreateSimpleWebBundle(primary_url));
  SetHeaders(
      "HTTP/1.1 200 OK\n"
      "Content-Type: application/webbundle\n");
  TestNavigationFailure(wbn_url,
                        "Web Bundle response must have "
                        "\"X-Content-Type-Options: nosniff\" header.");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest,
                       PrimaryURLResourceNotFound) {
  const std::string wbn_path = "/web_bundle/test.wbn";
  const std::string primary_url_path = "/web_bundle/test.html";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL wbn_url = embedded_test_server()->GetURL(wbn_path);
  const GURL primary_url = embedded_test_server()->GetURL(primary_url_path);
  const GURL inner_url =
      embedded_test_server()->GetURL("/web_bundle/inner.html");
  web_package::WebBundleBuilder builder;
  builder.AddPrimaryURL(primary_url);
  builder.AddExchange(inner_url,
                      {{":status", "200"}, {"content-type", "text/html"}},
                      "<title>Ready</title>");
  std::vector<uint8_t> bundle = builder.CreateBundle();
  SetContents(std::string(bundle.begin(), bundle.end()));
  TestNavigationFailure(
      wbn_url, "The primary URL resource is not found in the web bundle.");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, PrimaryURLNotFound) {
  const std::string wbn_path = "/web_bundle/test.wbn";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL wbn_url = embedded_test_server()->GetURL(wbn_path);
  const GURL inner_url =
      embedded_test_server()->GetURL("/web_bundle/inner.html");
  web_package::WebBundleBuilder builder;
  builder.AddExchange(inner_url,
                      {{":status", "200"}, {"content-type", "text/html"}},
                      "<title>Ready</title>");
  std::vector<uint8_t> bundle = builder.CreateBundle();
  SetContents(std::string(bundle.begin(), bundle.end()));
  TestNavigationFailure(
      wbn_url, "Web Bundle is missing the Primary URL to navigate to.");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest,
                       PrimaryURLHasInvalidScheme) {
  const std::string wbn_path = "/web_bundle/test.wbn";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL wbn_url = embedded_test_server()->GetURL(wbn_path);
  web_package::WebBundleBuilder builder;
  builder.AddPrimaryURL("foo://bar/");
  builder.AddExchange("foo://bar/",
                      {{":status", "200"}, {"content-type", "text/html"}},
                      "<title>Ready</title>");
  std::vector<uint8_t> bundle = builder.CreateBundle();
  SetContents(std::string(bundle.begin(), bundle.end()));
  TestNavigationFailure(wbn_url,
                        web_bundle_utils::kInvalidPrimaryUrlErrorMessage);
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, ExchangeHasInvalidScheme) {
  const std::string wbn_path = "/web_bundle/test.wbn";
  const std::string primary_url_path = "/web_bundle/test.html";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL wbn_url = embedded_test_server()->GetURL(wbn_path);
  const GURL primary_url = embedded_test_server()->GetURL(primary_url_path);
  web_package::WebBundleBuilder builder;
  builder.AddPrimaryURL(primary_url);
  builder.AddExchange(primary_url,
                      {{":status", "200"}, {"content-type", "text/html"}},
                      "<title>Ready</title>");
  builder.AddExchange("foo://bar",
                      {{":status", "200"}, {"content-type", "text/html"}},
                      "<title>Ready</title>");
  std::vector<uint8_t> bundle = builder.CreateBundle();
  SetContents(std::string(bundle.begin(), bundle.end()));
  TestNavigationFailure(wbn_url,
                        web_bundle_utils::kInvalidExchangeUrlErrorMessage);
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, OriginMismatch) {
  const std::string wbn_path = "/web_bundle/test.wbn";
  const std::string primary_url_path = "/web_bundle/test.html";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL primary_url =
      embedded_test_server()->GetURL("localhost", primary_url_path);

  SetContents(web_bundle_browsertest_utils::CreateSimpleWebBundle(primary_url));
  TestNavigationFailure(
      embedded_test_server()->GetURL("127.0.0.1", wbn_path),
      "The origin of primary URL doesn't match with the origin of the web "
      "bundle.");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, InvalidFile) {
  const std::string wbn_path = "/web_bundle/test.wbn";
  const std::string primary_url_path = "/web_bundle/test.html";
  RegisterRequestHandler(wbn_path);
  SetContents("This is an invalid Web Bundle file.");
  ASSERT_TRUE(embedded_test_server()->Start());

  TestNavigationFailure(embedded_test_server()->GetURL(wbn_path),
                        "Failed to read metadata of Web Bundle file: Wrong "
                        "magic bytes.");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, DataDecoderRestart) {
  const std::string wbn_path = "/web_bundle/test.wbn";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL primary_url =
      embedded_test_server()->GetURL("/web_bundle/test.html");
  const GURL script_url =
      embedded_test_server()->GetURL("/web_bundle/script.js");
  const std::string primary_url_content = "<title>Ready</title>";
  const std::string script_url_content = "document.title = 'OK'";
  SetContents(primary_url_content + script_url_content);

  std::vector<std::pair<GURL, const std::string&>> items = {
      {primary_url, primary_url_content}, {script_url, script_url_content}};
  web_bundle_browsertest_utils::MockParserFactory mock_factory(items);

  NavigateToBundleAndWaitForReady(embedded_test_server()->GetURL(wbn_path),
                                  primary_url);

  EXPECT_EQ(1, mock_factory.GetParserCreationCount());
  mock_factory.SimulateParserDisconnect();

  ExecuteScriptAndWaitForTitle(R"(
    const script = document.createElement("script");
    script.src = "script.js";
    document.body.appendChild(script);)",
                               "OK");

  EXPECT_EQ(2, mock_factory.GetParserCreationCount());
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, ParseMetadataCrash) {
  const std::string wbn_path = "/web_bundle/test.wbn";
  RegisterRequestHandler(wbn_path);
  SetContents("<title>Ready</title>");
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL primary_url =
      embedded_test_server()->GetURL("/web_bundle/test.html");
  std::vector<std::pair<GURL, const std::string&>> items = {
      {primary_url, contents()}};
  web_bundle_browsertest_utils::MockParserFactory mock_factory(items);
  mock_factory.SimulateParseMetadataCrash();

  TestNavigationFailure(embedded_test_server()->GetURL(wbn_path),
                        "Failed to read metadata of Web Bundle file: Cannot "
                        "connect to the remote parser service");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, ParseResponseCrash) {
  const std::string wbn_path = "/web_bundle/test.wbn";
  RegisterRequestHandler(wbn_path);
  SetContents("<title>Ready</title>");
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL primary_url =
      embedded_test_server()->GetURL("/web_bundle/test.html");
  std::vector<std::pair<GURL, const std::string&>> items = {
      {primary_url, contents()}};
  web_bundle_browsertest_utils::MockParserFactory mock_factory(items);
  mock_factory.SimulateParseResponseCrash();

  TestNavigationFailure(embedded_test_server()->GetURL(wbn_path),
                        "Failed to read response header of Web Bundle file: "
                        "Cannot connect to the remote parser service");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, PathMismatch) {
  const std::string wbn_path = "/web_bundle/test.wbn";
  const std::string primary_url_path = "/other_dir/test.html";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL wbn_url = embedded_test_server()->GetURL(wbn_path);
  const GURL primary_url = embedded_test_server()->GetURL(primary_url_path);

  SetContents(web_bundle_browsertest_utils::CreateSimpleWebBundle(primary_url));
  TestNavigationFailure(
      wbn_url,
      base::StringPrintf("Path restriction mismatch: Can't navigate to %s in "
                         "the web bundle served from %s.",
                         primary_url.spec().c_str(), wbn_url.spec().c_str()));
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, BasicNavigation) {
  RunSharedNavigationTest(
      &web_bundle_browsertest_utils::SetUpBasicNavigationTest,
      &web_bundle_browsertest_utils::RunBasicNavigationTest);
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest,
                       BrowserInitiatedOutOfBundleNavigation) {
  RunSharedNavigationTest(&web_bundle_browsertest_utils::
                              SetUpBrowserInitiatedOutOfBundleNavigationTest,
                          &web_bundle_browsertest_utils::
                              RunBrowserInitiatedOutOfBundleNavigationTest);
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest,
                       RendererInitiatedOutOfBundleNavigation) {
  RunSharedNavigationTest(&web_bundle_browsertest_utils::
                              SetUpRendererInitiatedOutOfBundleNavigationTest,
                          &web_bundle_browsertest_utils::
                              RunRendererInitiatedOutOfBundleNavigationTest);
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, SameDocumentNavigation) {
  RunSharedNavigationTest(
      &web_bundle_browsertest_utils::SetUpSameDocumentNavigationTest,
      &web_bundle_browsertest_utils::RunSameDocumentNavigationTest);
}

// https://crbug.com/1219373 fails with BFCache field trial testing config.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_IframeNavigation DISABLED_IframeNavigation
#else
#define MAYBE_IframeNavigation IframeNavigation
#endif
IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, MAYBE_IframeNavigation) {
  RunSharedNavigationTest(
      &web_bundle_browsertest_utils::SetUpIframeNavigationTest,
      &web_bundle_browsertest_utils::RunIframeNavigationTest);
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest,
                       IframeOutOfBundleNavigation) {
  RunSharedNavigationTest(
      &web_bundle_browsertest_utils::SetUpIframeNavigationTest,
      &web_bundle_browsertest_utils::RunIframeOutOfBundleNavigationTest);
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest,
                       IframeParentInitiatedOutOfBundleNavigation) {
  RunSharedNavigationTest(
      &web_bundle_browsertest_utils::SetUpIframeNavigationTest,
      &web_bundle_browsertest_utils::
          RunIframeParentInitiatedOutOfBundleNavigationTest);
}

// https://crbug.com/1219373 fails with BFCache field trial testing config.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_IframeSameDocumentNavigation DISABLED_IframeSameDocumentNavigation
#else
#define MAYBE_IframeSameDocumentNavigation IframeSameDocumentNavigation
#endif
IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest,
                       MAYBE_IframeSameDocumentNavigation) {
  RunSharedNavigationTest(
      &web_bundle_browsertest_utils::SetUpIframeNavigationTest,
      &web_bundle_browsertest_utils::RunIframeSameDocumentNavigationTest);
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, CrossScopeNavigations) {
  const std::string wbn_path = "/web_bundle/path_test/in_scope/path_test.wbn";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());
  SetContents(web_bundle_browsertest_utils::CreatePathTestWebBundle(
      embedded_test_server()->base_url()));

  const GURL wbn_url = embedded_test_server()->GetURL(wbn_path);
  const GURL primary_url =
      embedded_test_server()->GetURL("/web_bundle/path_test/in_scope/");
  NavigateToBundleAndWaitForReady(wbn_url, primary_url);

  NavigateToURLAndWaitForTitle(
      embedded_test_server()->GetURL(
          "/web_bundle/path_test/in_scope/page.html"),
      "In scope page in Web Bundle / in scope script in Web Bundle");
  NavigateToURLAndWaitForTitle(
      embedded_test_server()->GetURL(
          "/web_bundle/path_test/out_scope/page.html"),
      "Out scope page from server / out scope script from server");
  NavigateToURLAndWaitForTitle(
      embedded_test_server()->GetURL(
          "/web_bundle/path_test/in_scope/page.html"),
      "In scope page from server / in scope script from server");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest,
                       CrossScopeHistoryNavigations) {
  const std::string wbn_path = "/web_bundle/path_test/in_scope/path_test.wbn";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());
  SetContents(web_bundle_browsertest_utils::CreatePathTestWebBundle(
      embedded_test_server()->base_url()));

  const GURL wbn_url = embedded_test_server()->GetURL(wbn_path);
  const GURL primary_url =
      embedded_test_server()->GetURL("/web_bundle/path_test/in_scope/");
  NavigateToBundleAndWaitForReady(wbn_url, primary_url);

  NavigateToURLAndWaitForTitle(
      embedded_test_server()->GetURL(
          "/web_bundle/path_test/in_scope/page.html"),
      "In scope page in Web Bundle / in scope script in Web Bundle");

  NavigateToURLAndWaitForTitle(
      embedded_test_server()->GetURL("/web_bundle/path_test/in_scope/"),
      "Ready");

  ExecuteScriptAndWaitForTitle(
      "history.back();",
      "In scope page in Web Bundle / in scope script in Web Bundle");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            embedded_test_server()->GetURL(
                "/web_bundle/path_test/in_scope/page.html"));

  NavigateToURLAndWaitForTitle(
      embedded_test_server()->GetURL(
          "/web_bundle/path_test/out_scope/page.html"),
      "Out scope page from server / out scope script from server");

  ExecuteScriptAndWaitForTitle(
      "history.back();",
      "In scope page in Web Bundle / in scope script in Web Bundle");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            embedded_test_server()->GetURL(
                "/web_bundle/path_test/in_scope/page.html"));
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest,
                       HistoryNavigationError_UnexpectedContentType) {
  // The test assumes the previous page gets deleted after navigation and doing
  // back navigation will recreate the page. Disable back/forward cache to
  // ensure that it doesn't get preserved in the cache.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);
  const std::string wbn_path = "/web_bundle/test.wbn";
  const std::string primary_url_path = "/web_bundle/test.html";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL wbn_url = embedded_test_server()->GetURL(wbn_path);
  const GURL primary_url = embedded_test_server()->GetURL(primary_url_path);

  SetHeaders(
      "HTTP/1.1 200 OK\n"
      "Cache-Control:no-store\n"
      "Content-Type:application/webbundle\n"
      "X-Content-Type-Options: nosniff\n");
  SetContents(web_bundle_browsertest_utils::CreateSimpleWebBundle(primary_url));
  NavigateToBundleAndWaitForReady(wbn_url, primary_url);
  NavigateToURLAndWaitForTitle(
      embedded_test_server()->GetURL("/web_bundle/empty_page.html"),
      "Empty Page");

  SetHeaders(
      "HTTP/1.1 200 OK\n"
      "Cache-Control:no-store\n"
      "Content-Type:application/foo_bar\n"
      "X-Content-Type-Options: nosniff\n");
  HistoryBackAndWaitUntilConsoleError("Unexpected content type.");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest,
                       HistoryNavigationError_MissingNosniff) {
  // The test assumes the previous page gets deleted after navigation and doing
  // back navigation will recreate the page. Disable back/forward cache to
  // ensure that it doesn't get preserved in the cache.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);
  const std::string wbn_path = "/web_bundle/test.wbn";
  const std::string primary_url_path = "/web_bundle/test.html";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL wbn_url = embedded_test_server()->GetURL(wbn_path);
  const GURL primary_url = embedded_test_server()->GetURL(primary_url_path);

  SetHeaders(
      "HTTP/1.1 200 OK\n"
      "Cache-Control:no-store\n"
      "Content-Type:application/webbundle\n"
      "X-Content-Type-Options: nosniff\n");
  SetContents(web_bundle_browsertest_utils::CreateSimpleWebBundle(primary_url));
  NavigateToBundleAndWaitForReady(wbn_url, primary_url);
  NavigateToURLAndWaitForTitle(
      embedded_test_server()->GetURL("/web_bundle/empty_page.html"),
      "Empty Page");

  SetHeaders(
      "HTTP/1.1 200 OK\n"
      "Cache-Control:no-store\n"
      "Content-Type:application/webbundle\n");
  HistoryBackAndWaitUntilConsoleError(
      "Web Bundle response must have \"X-Content-Type-Options: nosniff\" "
      "header.");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest,
                       HistoryNavigationError_UnexpectedRedirect) {
  // The test assumes the previous page gets deleted after navigation and doing
  // back navigation will recreate the page. Disable back/forward cache to
  // ensure that it doesn't get preserved in the cache.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);
  const std::string wbn_path = "/web_bundle/test.wbn";
  const std::string primary_url_path = "/web_bundle/test.html";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL wbn_url = embedded_test_server()->GetURL(wbn_path);
  const GURL primary_url = embedded_test_server()->GetURL(primary_url_path);

  SetHeaders(
      "HTTP/1.1 200 OK\n"
      "Cache-Control:no-store\n"
      "Content-Type:application/webbundle\n"
      "X-Content-Type-Options: nosniff\n");
  SetContents(web_bundle_browsertest_utils::CreateSimpleWebBundle(primary_url));
  NavigateToBundleAndWaitForReady(wbn_url, primary_url);
  NavigateToURLAndWaitForTitle(
      embedded_test_server()->GetURL("/web_bundle/empty_page.html"),
      "Empty Page");

  SetHeaders(
      "HTTP/1.1 302 OK\n"
      "Location:/web_bundle/empty_page.html\n"
      "X-Content-Type-Options: nosniff\n");
  SetContents("");
  HistoryBackAndWaitUntilConsoleError("Unexpected redirect.");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest,
                       HistoryNavigationError_ReadMetadataFailure) {
  // The test assumes the previous page gets deleted after navigation. Disable
  // back/forward cache to ensure that it doesn't get preserved in the cache.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);
  const std::string wbn_path = "/web_bundle/test.wbn";
  const std::string primary_url_path = "/web_bundle/test.html";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL wbn_url = embedded_test_server()->GetURL(wbn_path);
  const GURL primary_url = embedded_test_server()->GetURL(primary_url_path);

  AddHeaders("Cache-Control:no-store\n");
  SetContents(web_bundle_browsertest_utils::CreateSimpleWebBundle(primary_url));
  NavigateToBundleAndWaitForReady(wbn_url, primary_url);
  NavigateToURLAndWaitForTitle(
      embedded_test_server()->GetURL("/web_bundle/empty_page.html"),
      "Empty Page");

  SetContents("This is an invalid Web Bundle file.");
  HistoryBackAndWaitUntilConsoleError(
      "Failed to read metadata of Web Bundle file: Wrong magic bytes.");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest,
                       HistoryNavigationError_ExpectedUrlNotFound) {
  // The test assumes the previous page gets deleted after navigation and doing
  // back navigation will recreate the page. Disable back/forward cache to
  // ensure that it doesn't get preserved in the cache.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);
  const std::string wbn_path = "/web_bundle/test.wbn";
  const std::string primary_url_path = "/web_bundle/test.html";
  const std::string alt_primary_url_path = "/web_bundle/alt.html";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL wbn_url = embedded_test_server()->GetURL(wbn_path);
  const GURL primary_url = embedded_test_server()->GetURL(primary_url_path);

  AddHeaders("Cache-Control:no-store\n");
  SetContents(web_bundle_browsertest_utils::CreateSimpleWebBundle(primary_url));
  NavigateToBundleAndWaitForReady(wbn_url, primary_url);
  NavigateToURLAndWaitForTitle(
      embedded_test_server()->GetURL("/web_bundle/empty_page.html"),
      "Empty Page");

  SetContents(web_bundle_browsertest_utils::CreateSimpleWebBundle(
      embedded_test_server()->GetURL(alt_primary_url_path)));
  HistoryBackAndWaitUntilConsoleError(
      "The expected URL resource is not found in the web bundle.");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, Iframe) {
  const std::string wbn_path = "/test.wbn";
  RegisterRequestHandler(wbn_path);

  net::EmbeddedTestServer third_party_server;
  GURL primary_url_origin;
  GURL third_party_origin;
  std::string web_bundle_content;
  web_bundle_browsertest_utils::SetUpSubPageTest(
      embedded_test_server(), &third_party_server, &primary_url_origin,
      &third_party_origin, &web_bundle_content);
  SetContents(web_bundle_content);
  NavigateToBundleAndWaitForReady(primary_url_origin.Resolve(wbn_path),
                                  primary_url_origin.Resolve("/top"));
  web_bundle_browsertest_utils::RunSubPageTest(
      shell()->web_contents(), primary_url_origin, third_party_origin,
      &web_bundle_browsertest_utils::AddIframeAndWaitForMessage,
      false /* support_third_party_wbn_page */);
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, WindowOpen) {
  const std::string wbn_path = "/test.wbn";
  RegisterRequestHandler(wbn_path);

  net::EmbeddedTestServer third_party_server;
  GURL primary_url_origin;
  GURL third_party_origin;
  std::string web_bundle_content;
  web_bundle_browsertest_utils::SetUpSubPageTest(
      embedded_test_server(), &third_party_server, &primary_url_origin,
      &third_party_origin, &web_bundle_content);
  SetContents(web_bundle_content);
  NavigateToBundleAndWaitForReady(primary_url_origin.Resolve(wbn_path),
                                  primary_url_origin.Resolve("/top"));
  web_bundle_browsertest_utils::RunSubPageTest(
      shell()->web_contents(), primary_url_origin, third_party_origin,
      &web_bundle_browsertest_utils::WindowOpenAndWaitForMessage,
      false /* support_third_party_wbn_page */);
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, OutScopeSubPage) {
  const std::string wbn_path = "/in_scope/test.wbn";
  const std::string primary_url_path = "/in_scope/test.html";

  RegisterRequestHandler(wbn_path);
  web_bundle_browsertest_utils::RegisterRequestHandlerForSubPageTest(
      embedded_test_server(), "");
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL origin = embedded_test_server()->GetURL("/");
  web_package::WebBundleBuilder builder;
  builder.AddPrimaryURL(origin.Resolve(primary_url_path));
  web_bundle_browsertest_utils::AddHtmlFile(&builder, origin, primary_url_path,
                                            R"(
    <script>
    window.addEventListener('message',
                            event => domAutomationController.send(event.data),
                            false);
    document.title = 'Ready';
    </script>
    )");
  web_bundle_browsertest_utils::AddHtmlFile(
      &builder, origin, "/in_scope/subpage",
      web_bundle_browsertest_utils::CreateSubPageHtml("in-scope-wbn-page"));
  web_bundle_browsertest_utils::AddScriptFile(
      &builder, origin, "/in_scope/script",
      web_bundle_browsertest_utils::CreateScriptForSubPageTest(
          "in-scope-wbn-script"));
  web_bundle_browsertest_utils::AddHtmlFile(
      &builder, origin, "/out_scope/subpage",
      web_bundle_browsertest_utils::CreateSubPageHtml("out-scope-wbn-page"));
  web_bundle_browsertest_utils::AddScriptFile(
      &builder, origin, "/out_scope/script",
      web_bundle_browsertest_utils::CreateScriptForSubPageTest(
          "out-scope-wbn-script"));
  std::vector<uint8_t> bundle = builder.CreateBundle();
  SetContents(std::string(bundle.begin(), bundle.end()));
  NavigateToBundleAndWaitForReady(origin.Resolve(wbn_path),
                                  origin.Resolve(primary_url_path));
  const auto funcs = {
      &web_bundle_browsertest_utils::AddIframeAndWaitForMessage,
      &web_bundle_browsertest_utils::WindowOpenAndWaitForMessage};
  for (const auto func : funcs) {
    EXPECT_EQ(
        "in-scope-wbn-page in-scope-wbn-script",
        (*func)(
            shell()->web_contents(),
            origin.Resolve("/in_scope/subpage").Resolve("#/in_scope/script")));
    EXPECT_EQ(
        "in-scope-wbn-page server-script",
        (*func)(
            shell()->web_contents(),
            origin.Resolve("/in_scope/subpage").Resolve("#/out_scope/script")));
    EXPECT_EQ(
        "server-page server-script",
        (*func)(
            shell()->web_contents(),
            origin.Resolve("/out_scope/subpage").Resolve("#/in_scope/script")));
    EXPECT_EQ(
        "server-page server-script",
        (*func)(shell()->web_contents(), origin.Resolve("/out_scope/subpage")
                                             .Resolve("#/out_scope/script")));
  }
}
}  // namespace content
