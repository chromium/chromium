// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/web_package/web_bundle_builder.h"
#include "components/web_package/web_bundle_utils.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_package/web_bundle_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

const char kUuidInPackageURL[] =
    "uuid-in-package:429fcc4e-0696-4bad-b099-ee9175f023ae";
const char kUuidInPackageURL2[] =
    "uuid-in-package:e219d992-b7f7-4da7-9722-481bc40cfda1";
const char kUuidURLPrefix[] = "uuid-in-package:";
const char kUuidTestBundlePath[] = "/web_bundle/uuid-in-package.wbn";
const char kUuidTestPagePath[] =
    "/web_bundle/script_web_bundle_uuid_in_package.html";

class TestBrowserClient : public ContentBrowserClient {
 public:
  TestBrowserClient() = default;
  ~TestBrowserClient() override = default;
  bool HandleExternalProtocol(
      const GURL& url,
      base::RepeatingCallback<WebContents*()> web_contents_getter,
      int frame_tree_node_id,
      NavigationUIData* navigation_data,
      bool is_primary_main_frame,
      bool is_in_fenced_frame_tree,
      network::mojom::WebSandboxFlags sandbox_flags,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const absl::optional<url::Origin>& initiating_origin,
      content::RenderFrameHost* initiator_document,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory)
      override {
    EXPECT_FALSE(observed_url_.has_value());
    observed_url_ = url;
    return true;
  }

  GURL observed_url() const { return observed_url_ ? *observed_url_ : GURL(); }

 private:
  absl::optional<GURL> observed_url_;
};

class FinishNavigationObserver : public WebContentsObserver {
 public:
  FinishNavigationObserver() = default;
  ~FinishNavigationObserver() override = default;
  explicit FinishNavigationObserver(WebContents* contents,
                                    const GURL& expected_url,
                                    base::OnceClosure done_closure,
                                    bool wait_for_finish_load = false)
      : WebContentsObserver(contents),
        expected_url_(expected_url),
        done_closure_(std::move(done_closure)),
        wait_for_finish_load_(wait_for_finish_load) {}

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetURL() == expected_url_) {
      error_code_ = navigation_handle->GetNetErrorCode();
      if (!wait_for_finish_load_)
        std::move(done_closure_).Run();
    }
  }

  void DidFinishLoad(RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    if (wait_for_finish_load_ && error_code_.has_value())
      std::move(done_closure_).Run();
  }

  const absl::optional<net::Error>& error_code() const { return error_code_; }

 private:
  GURL expected_url_;
  base::OnceClosure done_closure_;
  absl::optional<net::Error> error_code_;
  bool wait_for_finish_load_;
};

int64_t GetTestDataFileSize(const base::FilePath::CharType* file_path) {
  int64_t file_size;
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath test_data_dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
  CHECK(base::GetFileSize(test_data_dir.Append(base::FilePath(file_path)),
                          &file_size));
  return file_size;
}

FrameTreeNode* GetFirstChild(WebContents* web_contents) {
  return static_cast<WebContentsImpl*>(web_contents)
      ->GetPrimaryFrameTree()
      .root()
      ->child_at(0);
}

}  // namespace

// Tests for <script type=webbundle>
class WebBundleElementBrowserTest : public ContentBrowserTest {
 public:
 protected:
  WebBundleElementBrowserTest() {
    feature_list_.InitWithFeatures(
        {}, {net::features::kForceIsolationInfoFrameOriginToTopLevelFrame,
             net::features::kEnableDoubleKeyNetworkAnonymizationKey});
  }
  ~WebBundleElementBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    original_client_ = SetBrowserClientForTesting(&browser_client_);
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &WebBundleElementBrowserTest::HandleTestWebBundleRequest,
        base::Unretained(this)));
    https_server_.RegisterRequestMonitor(base::BindRepeating(
        &WebBundleElementBrowserTest::MonitorResourceRequest,
        base::Unretained(this)));
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &WebBundleElementBrowserTest::InvalidResponseHandler,
        base::Unretained(this)));
    https_server_.AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(https_server_.Start());
  }

  void TearDownOnMainThread() override {
    ContentBrowserTest::TearDownOnMainThread();
    SetBrowserClientForTesting(original_client_);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ContentBrowserTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    ContentBrowserTest::TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  std::string GetScriptForWebBundle(const char* web_bundle_url) {
    return base::StringPrintf(R"HTML(
        {
          const script = document.createElement('script');
          script.type = 'webbundle';
          script.textContent = JSON.stringify({"source": '%s'});
          script.onload = () => window.domAutomationController.send('loaded');
          script.onerror = () => window.domAutomationController.send('failed');
          document.body.appendChild(script);
        }
      )HTML",
                              web_bundle_url);
  }

  void CreateIframeAndWaitForOnload(const std::string& url) {
    DOMMessageQueue dom_message_queue(shell()->web_contents());
    std::string message;
    ExecuteScriptAsync(
        shell(),
        "let iframe = document.createElement('iframe');"
        "iframe.src = '" +
            url +
            "';"
            "iframe.onload = function() {"
            "   window.domAutomationController.send('iframe.onload');"
            "};"
            "document.body.appendChild(iframe);");
    EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"iframe.onload\"", message);
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleTestWebBundleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != "/web_bundle/test.wbn")
      return nullptr;
    GURL test1_url(https_server_.GetURL("/web_bundle/test1.txt"));
    GURL test2_url(https_server_.GetURL("/web_bundle/test2.txt"));
    web_package::WebBundleBuilder builder;
    builder.AddExchange(test1_url,
                        {{":status", "200"}, {"content-type", "text/plain"}},
                        "test1");
    builder.AddExchange(test2_url,
                        {{":status", "200"}, {"content-type", "text/plain"}},
                        "test2");
    auto bundle = builder.CreateBundle();
    std::string body(reinterpret_cast<const char*>(bundle.data()),
                     bundle.size());
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(body);
    http_response->set_content_type("application/webbundle");
    http_response->AddCustomHeader("X-Content-Type-Options", "nosniff");
    return http_response;
  }

  std::unique_ptr<net::test_server::HttpResponse> InvalidResponseHandler(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != "/web_bundle/invalid-response")
      return nullptr;
    return std::make_unique<net::test_server::RawHttpResponse>(
        "", "Not a valid HTTP response.");
  }

  GURL GetObservedUnknownSchemeUrl() { return browser_client_.observed_url(); }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  void MonitorResourceRequest(const net::test_server::HttpRequest& request) {
    // This should be called on `EmbeddedTestServer::io_thread_`.
    EXPECT_FALSE(
        content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    base::AutoLock auto_lock(lock_);
    request_count_by_path_[request.GetURL().PathForRequest()]++;
  }

  int GetRequestCount(const GURL& url) {
    EXPECT_TRUE(content::BrowserThread::CurrentlyOn(BrowserThread::UI));
    base::AutoLock auto_lock(lock_);
    return request_count_by_path_[url.PathForRequest()];
  }

 private:
  content::ContentMockCertVerifier mock_cert_verifier_;
  raw_ptr<ContentBrowserClient> original_client_ = nullptr;
  TestBrowserClient browser_client_;
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_{
      net::EmbeddedTestServer::Type::TYPE_HTTPS};
  // Counts of requests sent to the server. Keyed by path (not by full URL)
  std::map<std::string, int> request_count_by_path_ GUARDED_BY(lock_);
  base::Lock lock_;
};

IN_PROC_BROWSER_TEST_F(WebBundleElementBrowserTest,
                       WebBundleResourceShouldBeReused) {
  // The tentative spec:
  // https://docs.google.com/document/d/1GEJ3wTERGEeTG_4J0QtAwaNXhPTza0tedd00A7vPVsw/edit

  // Tests that webbundle resources are surely re-used when we remove a <script
  // type=webbunble> and add a new <script type=webbundle> with the same bundle
  // URL to the removed one, in the same microtask scope.

  GURL url(https_server()->GetURL("/web_bundle/empty.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  {
    // Add a <script type=webbundle>.
    DOMMessageQueue dom_message_queue(shell()->web_contents());
    ExecuteScriptAsync(shell(),
                       R"HTML(
        const script = document.createElement("script");
        script.type = "webbundle";
        script.textContent =
              JSON.stringify({"source": "/web_bundle/test.wbn",
                              "resources": ["/web_bundle/test1.txt"]});
        document.body.appendChild(script);
        (async () => {
          const response = await fetch("/web_bundle/test1.txt");
          const text = await response.text();
          window.domAutomationController.send(`fetch: ${text}`);
        })();

      )HTML");
    std::string message;
    EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
    EXPECT_EQ(message, "\"fetch: test1\"");
    EXPECT_EQ(GetRequestCount(https_server()->GetURL("/web_bundle/test.wbn")),
              1);
  }
  {
    // Remove the <script type=webbundle> from the document, and then add a new
    // <script type=webbundle> whose bundle URL is same to the removed one in
    // the same microtask scope, The added element should re-use the webbundle
    // resource which the old <script type=webbundle> has been using. Thus, the
    // bundle shouldn't be fetched twice.
    DOMMessageQueue dom_message_queue(shell()->web_contents());
    ExecuteScriptAsync(shell(),
                       R"HTML(
        script.remove();

        const script2 = document.createElement("script");
        script2.type = "webbundle";
        script2.textContent =
              JSON.stringify({"source": "/web_bundle/test.wbn",
                              "resources": ["/web_bundle/test2.txt"]});
        document.body.appendChild(script2);

        (async () => {
          const response = await fetch("/web_bundle/test2.txt");
          const text = await response.text();
          window.domAutomationController.send(`fetch: ${text}`);
        })();
      )HTML");
    std::string message;
    EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
    EXPECT_EQ(message, "\"fetch: test2\"")
        << "A new script element's rule should be effective.";
    EXPECT_EQ(GetRequestCount(https_server()->GetURL("/web_bundle/test.wbn")),
              1)
        << "A bundle should not be fetched twice.";
  }
}

IN_PROC_BROWSER_TEST_F(WebBundleElementBrowserTest, SubframeLoad) {
  base::HistogramTester histogram_tester;
  GURL url(https_server()->GetURL(kUuidTestPagePath));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Create an iframe with a uuid-in-package resource in a bundle.
  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(
      shell()->web_contents(), GURL(kUuidInPackageURL), run_loop.QuitClosure());
  ExecuteScriptAsync(
      shell(),
      base::StringPrintf("let iframe = document.createElement('iframe');"
                         "iframe.src = '%s';"
                         "document.body.appendChild(iframe);",
                         GURL(kUuidInPackageURL).spec().c_str()));

  run_loop.Run();
  EXPECT_EQ(net::OK, *finish_navigation_observer.error_code());

  // Check the metrics recorded in the network process.
  FetchHistogramsFromChildProcesses();
  int64_t web_bundle_size = GetTestDataFileSize(
      FILE_PATH_LITERAL("content/test/data/web_bundle/uuid-in-package.wbn"));
  histogram_tester.ExpectUniqueSample("SubresourceWebBundles.ReceivedSize",
                                      web_bundle_size, 1);
  histogram_tester.ExpectUniqueSample("SubresourceWebBundles.ContentLength",
                                      web_bundle_size, 1);
}

IN_PROC_BROWSER_TEST_F(WebBundleElementBrowserTest, SubframeLoadError) {
  GURL url(https_server()->GetURL("/web_bundle/invalid_web_bundle.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Attempt to create an iframe with a resource in a broken WebBundle.
  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(
      shell()->web_contents(), GURL(kUuidInPackageURL), run_loop.QuitClosure());
  ExecuteScriptAsync(
      shell(),
      base::StringPrintf("let iframe = document.createElement('iframe');"
                         "iframe.src = '%s';"
                         "document.body.appendChild(iframe);",
                         GURL(kUuidInPackageURL).spec().c_str()));
  run_loop.Run();
  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            *finish_navigation_observer.error_code());
}

IN_PROC_BROWSER_TEST_F(WebBundleElementBrowserTest, HistogramSameOriginCount) {
  base::HistogramTester histogram_tester;

  GURL url(https_server()->GetURL("/web_bundle/same_origin_web_bundle.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  FetchHistogramsFromChildProcesses();
  histogram_tester.ExpectUniqueSample(
      "SubresourceWebBundles.OriginType",
      web_package::ScriptWebBundleOriginType::kSameOrigin, 1);
}

IN_PROC_BROWSER_TEST_F(WebBundleElementBrowserTest, HistogramCrossOriginCount) {
  base::HistogramTester histogram_tester;

  GURL url(https_server()->GetURL("/web_bundle/cross_origin_web_bundle.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  FetchHistogramsFromChildProcesses();
  histogram_tester.ExpectUniqueSample(
      "SubresourceWebBundles.OriginType",
      web_package::ScriptWebBundleOriginType::kCrossOrigin, 1);
}

IN_PROC_BROWSER_TEST_F(WebBundleElementBrowserTest, BundleFetchError) {
  base::HistogramTester histogram_tester;

  GURL url(https_server()->GetURL("/web_bundle/empty.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // "/web_bundle/invalid-response" returns an invalid HTTP response which
  // causes ERR_INVALID_HTTP_RESPONSE network error.
  DOMMessageQueue dom_message_queue(shell()->web_contents());
  ExecuteScriptAsync(shell(),
                     GetScriptForWebBundle("/web_bundle/invalid-response"));
  std::string message;
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"failed\"", message);

  // Check the metrics recorded in the network process.
  FetchHistogramsFromChildProcesses();
  histogram_tester.ExpectUniqueSample(
      "SubresourceWebBundles.BundleFetchErrorCode",
      -net::ERR_INVALID_HTTP_RESPONSE, 1);
}

IN_PROC_BROWSER_TEST_F(WebBundleElementBrowserTest,
                       BundleRedirectionIsForbidden) {
  GURL url(https_server()->GetURL("/web_bundle/empty.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(
      "* URL redirection of Subresource Web Bundles is currently not "
      "supported.");

  DOMMessageQueue dom_message_queue(shell()->web_contents());

  const std::pair<const char*, const char*> test_cases[] = {
      {"/web_bundle/uuid-in-package.wbn", "loaded"},
      {"/server-redirect?/web_bundle/uuid-in-package.wbn", "failed"}};

  for (const auto& [input_url, expected_message] : test_cases) {
    ExecuteScriptAsync(shell(), GetScriptForWebBundle(input_url));
    std::string message;
    EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
    EXPECT_EQ(base::StrCat({"\"", expected_message, "\""}), message);

    if (std::string(expected_message) == "failed")
      ASSERT_TRUE(console_observer.Wait());
  }
}

IN_PROC_BROWSER_TEST_F(WebBundleElementBrowserTest, FollowLink) {
  GURL url(https_server()->GetURL(kUuidTestPagePath));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Clicking a link to a uuid-in-package resource in a bundle should not be
  // loaded from the bundle.
  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(
      shell()->web_contents(), GURL(kUuidInPackageURL), run_loop.QuitClosure());
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     "document.getElementById('link').click();"));
  run_loop.Run();
  EXPECT_EQ(net::ERR_ABORTED, *finish_navigation_observer.error_code());
  EXPECT_EQ(GURL(kUuidInPackageURL), GetObservedUnknownSchemeUrl());
}

IN_PROC_BROWSER_TEST_F(WebBundleElementBrowserTest, IframeChangeSource) {
  GURL main_url(https_server()->GetURL("/simple_page.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create an iframe whose document has <script type="webbundle">.
  CreateIframeAndWaitForOnload(kUuidTestPagePath);

  // Attempt to navigate the iframe to a bundled resource.
  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(
      shell()->web_contents(), GURL(kUuidInPackageURL), run_loop.QuitClosure());
  ExecuteScriptAsync(
      shell(), base::StringPrintf("iframe.src = '%s';",
                                  GURL(kUuidInPackageURL).spec().c_str()));
  run_loop.Run();
  EXPECT_EQ(net::ERR_ABORTED, *finish_navigation_observer.error_code());
  EXPECT_EQ(GURL(kUuidInPackageURL), GetObservedUnknownSchemeUrl());
}

IN_PROC_BROWSER_TEST_F(WebBundleElementBrowserTest, IframeFollowLink) {
  GURL main_url(https_server()->GetURL("/simple_page.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create an iframe whose document has <script type="webbundle">.
  CreateIframeAndWaitForOnload(kUuidTestPagePath);

  // Click a link inside the iframe. The resource should not be loaded from
  // the bundle.
  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(
      shell()->web_contents(), GURL(kUuidInPackageURL), run_loop.QuitClosure());
  ExecuteScriptAsync(shell(),
                     "iframe.contentDocument.getElementById('link').click();");
  run_loop.Run();
  EXPECT_EQ(net::ERR_ABORTED, *finish_navigation_observer.error_code());
  EXPECT_EQ(GURL(kUuidInPackageURL), GetObservedUnknownSchemeUrl());
}

IN_PROC_BROWSER_TEST_F(WebBundleElementBrowserTest,
                       NavigationFromSiblingFrame) {
  GURL main_url(https_server()->GetURL(kUuidTestPagePath));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create an iframe and wait for the initial load.
  DOMMessageQueue dom_message_queue(shell()->web_contents());
  std::string message;
  ExecuteScriptAsync(shell(),
                     "let iframe1 = document.createElement('iframe');"
                     "iframe1.name = 'iframe1';"
                     "iframe1.src = '/simple_page.html';"
                     "iframe1.onload = function() {"
                     "   window.domAutomationController.send('iframe1.onload');"
                     "};"
                     "document.body.appendChild(iframe1);");
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"iframe1.onload\"", message);

  // Create another iframe and wait for the initial load.
  ExecuteScriptAsync(shell(),
                     "let iframe2 = document.createElement('iframe');"
                     "iframe2.name = 'iframe2';"
                     "iframe2.src = '/simple_page.html';"
                     "iframe2.onload = function() {"
                     "   window.domAutomationController.send('iframe2.onload');"
                     "};"
                     "document.body.appendChild(iframe2);");
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"iframe2.onload\"", message);

  // Navigate iframe2 to a uuid-in-package URL by clicking a link in iframe1,
  // which should not be loaded from the WebBundle associated with the parent
  // document.
  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(
      shell()->web_contents(), GURL(kUuidInPackageURL), run_loop.QuitClosure());
  ExecuteScriptAsync(
      shell(),
      base::StringPrintf("let a = iframe1.contentDocument.createElement('a');"
                         "a.href = '%s';"
                         "a.target = 'iframe2';"
                         "iframe1.contentDocument.body.appendChild(a);"
                         "a.click();",
                         GURL(kUuidInPackageURL).spec().c_str()));
  run_loop.Run();
  EXPECT_EQ(net::ERR_ABORTED, *finish_navigation_observer.error_code());
  EXPECT_EQ(GURL(kUuidInPackageURL), GetObservedUnknownSchemeUrl());
}

IN_PROC_BROWSER_TEST_F(WebBundleElementBrowserTest,
                       GrandChildShouldNotBeLoadedFromBundle) {
  GURL main_url(https_server()->GetURL(kUuidTestPagePath));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create an iframe with a uuid-in-package resource, which has a nested iframe
  // with another uuid-in-package resource in the bundle. The resource for the
  // nested should iframe not be loaded from the bundle.
  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(
      shell()->web_contents(), GURL(kUuidInPackageURL), run_loop.QuitClosure());
  ExecuteScriptAsync(
      shell(), base::StringPrintf(
                   "let iframe = document.createElement('iframe');"
                   "iframe.src = '%s1084e1fc-2122-4155-a4dd-28efb2e8ccb1';"
                   "document.body.appendChild(iframe);",
                   kUuidURLPrefix));
  run_loop.Run();
  EXPECT_EQ(net::ERR_ABORTED, *finish_navigation_observer.error_code());
  EXPECT_EQ(GURL(kUuidInPackageURL), GetObservedUnknownSchemeUrl());
}

IN_PROC_BROWSER_TEST_F(WebBundleElementBrowserTest, NetworkAnonymizationKey) {
  GURL bundle_url(https_server()->GetURL("bundle.test", kUuidTestBundlePath));
  GURL page_url(https_server()->GetURL(
      "page.test", "/web_bundle/frame_parent.html?wbn=" + bundle_url.spec() +
                       "&frame=" + GURL(kUuidInPackageURL).spec().c_str()));
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  std::u16string expected_title(u"OK");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  RenderFrameHost* main_frame = shell()->web_contents()->GetPrimaryMainFrame();
  RenderFrameHost* urn_frame = ChildFrameAt(main_frame, 0);
  EXPECT_EQ("https://page.test https://bundle.test",
            *urn_frame->GetNetworkIsolationKey().ToCacheKeyString());
}

IN_PROC_BROWSER_TEST_F(WebBundleElementBrowserTest, ReloadSubframe) {
  GURL url(https_server()->GetURL(kUuidTestPagePath));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Create an iframe with a uuid-in-package resource in a bundle.
  {
    base::RunLoop run_loop;
    FinishNavigationObserver finish_navigation_observer(shell()->web_contents(),
                                                        GURL(kUuidInPackageURL),
                                                        run_loop.QuitClosure());
    ExecuteScriptAsync(
        shell(),
        base::StringPrintf("let iframe = document.createElement('iframe');"
                           "iframe.src = '%s';"
                           "document.body.appendChild(iframe);",
                           GURL(kUuidInPackageURL).spec().c_str()));
    run_loop.Run();
    EXPECT_EQ(net::OK, *finish_navigation_observer.error_code());
  }
  FrameTreeNode* iframe_node = GetFirstChild(shell()->web_contents());
  EXPECT_EQ(iframe_node->current_url(), GURL(kUuidInPackageURL));

  // Reload the iframe.
  {
    base::RunLoop run_loop;
    FinishNavigationObserver finish_navigation_observer(shell()->web_contents(),
                                                        GURL(kUuidInPackageURL),
                                                        run_loop.QuitClosure());
    iframe_node->current_frame_host()->Reload();
    run_loop.Run();
    EXPECT_EQ(net::OK, *finish_navigation_observer.error_code());
  }
}

IN_PROC_BROWSER_TEST_F(WebBundleElementBrowserTest, SubframeHistoryNavigation) {
  GURL url(https_server()->GetURL(kUuidTestPagePath));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Create an iframe with a uuid-in-package resource in a bundle.
  {
    base::RunLoop run_loop;
    FinishNavigationObserver finish_navigation_observer(shell()->web_contents(),
                                                        GURL(kUuidInPackageURL),
                                                        run_loop.QuitClosure());
    ExecuteScriptAsync(
        shell(),
        base::StringPrintf("let iframe = document.createElement('iframe');"
                           "iframe.src = '%s';"
                           "document.body.appendChild(iframe);",
                           GURL(kUuidInPackageURL).spec().c_str()));
    run_loop.Run();
    EXPECT_EQ(net::OK, *finish_navigation_observer.error_code());
  }
  FrameTreeNode* iframe_node = GetFirstChild(shell()->web_contents());
  EXPECT_EQ(iframe_node->current_url(), GURL(kUuidInPackageURL));

  // Navigate the iframe to a page outside the bundle.
  {
    GURL another_page_url(https_server()->GetURL("/simple_page.html"));
    base::RunLoop run_loop;
    FinishNavigationObserver finish_navigation_observer(
        shell()->web_contents(), another_page_url, run_loop.QuitClosure());
    EXPECT_TRUE(ExecJs(iframe_node,
                       base::StringPrintf("location.href = '%s';",
                                          another_page_url.spec().c_str())));
    run_loop.Run();
    EXPECT_EQ(net::OK, *finish_navigation_observer.error_code());
    EXPECT_EQ(iframe_node->current_url(), another_page_url);
  }

  // Back navigate the iframe to the uuid-in-package resource in the bundle.
  {
    base::RunLoop run_loop;
    // We need to wait for onload, otherwise the next navigation (by changing
    // iframe.src) will not create a history entry. See the comment in
    // LocalFrame::NavigationShouldReplaceCurrentHistoryEntry().
    FinishNavigationObserver finish_navigation_observer(
        shell()->web_contents(), GURL(kUuidInPackageURL),
        run_loop.QuitClosure(), true /* wait_for_finish_load */);
    EXPECT_TRUE(ExecJs(iframe_node, "history.back()"));
    run_loop.Run();
    EXPECT_EQ(net::OK, *finish_navigation_observer.error_code());
    EXPECT_EQ(iframe_node->current_url(), GURL(kUuidInPackageURL));
  }

  // Navigate the iframe to another uuid-in-package resource in the bundle, by
  // changing iframe.src attribute.
  {
    base::RunLoop run_loop;
    FinishNavigationObserver finish_navigation_observer(
        shell()->web_contents(), GURL(kUuidInPackageURL2),
        run_loop.QuitClosure());
    ExecuteScriptAsync(
        shell(), base::StringPrintf("iframe.src = '%s';",
                                    GURL(kUuidInPackageURL2).spec().c_str()));
    run_loop.Run();
    EXPECT_EQ(net::OK, *finish_navigation_observer.error_code());
    EXPECT_EQ(iframe_node->current_url(), GURL(kUuidInPackageURL2));
  }

  // Back navigate the iframe to the first uuid-in-package resource.
  {
    base::RunLoop run_loop;
    FinishNavigationObserver finish_navigation_observer(shell()->web_contents(),
                                                        GURL(kUuidInPackageURL),
                                                        run_loop.QuitClosure());
    EXPECT_TRUE(ExecJs(iframe_node, "history.back()"));
    run_loop.Run();
    EXPECT_EQ(net::OK, *finish_navigation_observer.error_code());
    EXPECT_EQ(iframe_node->current_url(), GURL(kUuidInPackageURL));
  }

  // Forward navigate the iframe to the second uuid-in-package resource.
  {
    base::RunLoop run_loop;
    FinishNavigationObserver finish_navigation_observer(
        shell()->web_contents(), GURL(kUuidInPackageURL2),
        run_loop.QuitClosure());
    EXPECT_TRUE(ExecJs(iframe_node, "history.forward()"));
    run_loop.Run();
    EXPECT_EQ(net::OK, *finish_navigation_observer.error_code());
    EXPECT_EQ(iframe_node->current_url(), GURL(kUuidInPackageURL2));
  }

  GURL url_with_hash(GURL(kUuidInPackageURL2).spec() + "#hash");
  // Same document navigation.
  {
    base::RunLoop run_loop;
    FinishNavigationObserver finish_navigation_observer(
        shell()->web_contents(), url_with_hash, run_loop.QuitClosure());
    EXPECT_TRUE(ExecJs(iframe_node, "location.href = '#hash';"));
    run_loop.Run();
    EXPECT_EQ(net::OK, *finish_navigation_observer.error_code());
    EXPECT_EQ(iframe_node->current_url(), url_with_hash);
  }

  // Back from same document navigation.
  {
    base::RunLoop run_loop;
    FinishNavigationObserver finish_navigation_observer(
        shell()->web_contents(), GURL(kUuidInPackageURL2),
        run_loop.QuitClosure());
    EXPECT_TRUE(ExecJs(iframe_node, "history.back()"));
    run_loop.Run();
    EXPECT_EQ(net::OK, *finish_navigation_observer.error_code());
    EXPECT_EQ(iframe_node->current_url(), GURL(kUuidInPackageURL2));
  }

  // Forward navigate to #hash.
  {
    base::RunLoop run_loop;
    FinishNavigationObserver finish_navigation_observer(
        shell()->web_contents(), url_with_hash, run_loop.QuitClosure());
    EXPECT_TRUE(ExecJs(iframe_node, "history.forward()"));
    run_loop.Run();
    EXPECT_EQ(net::OK, *finish_navigation_observer.error_code());
    EXPECT_EQ(iframe_node->current_url(), url_with_hash);
  }
}

}  // namespace content
