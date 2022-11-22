// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_BROWSERTEST_BASE_H_
#define CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_BROWSERTEST_BASE_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "components/web_package/mojom/web_bundle_parser.mojom-forward.h"
#include "components/web_package/test_support/mock_web_bundle_parser_factory.h"
#include "components/web_package/web_bundle_builder.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_type.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
namespace web_bundle_browsertest_utils {

constexpr char kTestPageUrl[] = "https://test.example.org/";

constexpr char kDefaultHeaders[] =
    "HTTP/1.1 200 OK\n"
    "Content-Type: application/webbundle\n"
    "X-Content-Type-Options: nosniff\n";

constexpr char kHeadersForHtml[] =
    "HTTP/1.1 200 OK\n"
    "Content-Type: text/html\n";

constexpr char kHeadersForJavaScript[] =
    "HTTP/1.1 200 OK\n"
    "Content-Type: application/javascript\n";

base::FilePath GetTestDataPath(base::StringPiece file);

#if BUILDFLAG(IS_ANDROID)
void CopyFileAndGetContentUri(const base::FilePath& file,
                              GURL* content_uri,
                              base::FilePath* new_file_path);
#endif  // BUILDFLAG(IS_ANDROID)

std::string ExecuteAndGetString(const ToRenderFrameHost& adapter,
                                const std::string& script);

void NavigateAndWaitForTitle(content::WebContents* web_contents,
                             const GURL& test_data_url,
                             const GURL& expected_commit_url,
                             base::StringPiece ascii_title);

class DownloadObserver : public DownloadManager::Observer {
 public:
  explicit DownloadObserver(DownloadManager* manager);

  DownloadObserver(const DownloadObserver&) = delete;
  DownloadObserver& operator=(const DownloadObserver&) = delete;

  ~DownloadObserver() override;

  void WaitUntilDownloadCreated();
  const GURL& observed_url() const;

  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;

 private:
  raw_ptr<DownloadManager> manager_;
  base::RunLoop run_loop_;
  GURL url_;
};

class MockParserFactory {
 public:
  MockParserFactory(std::vector<GURL> urls,
                    const base::FilePath& response_body_file);

  explicit MockParserFactory(
      const std::vector<std::pair<GURL, const std::string&>> items);

  MockParserFactory(const MockParserFactory&) = delete;
  MockParserFactory& operator=(const MockParserFactory&) = delete;

  int GetParserCreationCount() const;
  void SimulateParserDisconnect();
  void SimulateParseMetadataCrash();
  void SimulateParseResponseCrash();

 private:
  void FinishSetUp(web_package::mojom::BundleMetadataPtr metadata);

  void BindWebBundleParserFactory(
      mojo::PendingReceiver<web_package::mojom::WebBundleParserFactory>
          receiver);

  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  web_package::MockWebBundleParserFactory wrapped_factory_;
};

class TestBrowserClient : public ContentBrowserClient {
 public:
  TestBrowserClient() = default;

  TestBrowserClient(const TestBrowserClient&) = delete;
  TestBrowserClient& operator=(const TestBrowserClient&) = delete;

  ~TestBrowserClient() override = default;
  bool CanAcceptUntrustedExchangesIfNeeded() override;
};

class WebBundleBrowserTestBase : public ContentBrowserTest {
 public:
  WebBundleBrowserTestBase(const WebBundleBrowserTestBase&) = delete;
  WebBundleBrowserTestBase& operator=(const WebBundleBrowserTestBase&) = delete;

 protected:
  WebBundleBrowserTestBase() = default;
  ~WebBundleBrowserTestBase() override = default;

  void SetUpOnMainThread() override;

  void TearDownOnMainThread() override;

  void NavigateToBundleAndWaitForReady(const GURL& test_data_url,
                                       const GURL& expected_commit_url);

  void RunTestScript(const std::string& script);

  void ExecuteScriptAndWaitForTitle(const std::string& script,
                                    const std::string& title);

  void NavigateToURLAndWaitForTitle(const GURL& url, const std::string& title);

  void CreateTemporaryWebBundleFile(const std::string& content,
                                    base::FilePath* file_path);

 private:
  raw_ptr<ContentBrowserClient> original_client_ = nullptr;
  TestBrowserClient browser_client_;
  base::ScopedTempDir temp_dir_;
};

class FinishNavigationObserver : public WebContentsObserver {
 public:
  explicit FinishNavigationObserver(WebContents* contents,
                                    base::OnceClosure done_closure);

  ~FinishNavigationObserver() override;

  FinishNavigationObserver(const FinishNavigationObserver&) = delete;
  FinishNavigationObserver& operator=(const FinishNavigationObserver&) = delete;

  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  void set_navigations_remaining(int navigations_remaining);
  const absl::optional<net::Error>& error_code() const;
  const std::vector<NavigationType>& navigation_types() const;

 private:
  base::OnceClosure done_closure_;
  absl::optional<net::Error> error_code_;

  int navigations_remaining_ = 1;
  std::vector<NavigationType> navigation_types_;
};

std::string ExpectNavigationFailureAndReturnConsoleMessage(
    content::WebContents* web_contents,
    const GURL& url);

FrameTreeNode* GetFirstChild(WebContents* web_contents);

std::string CreateSimpleWebBundle(const GURL& primary_url);

void AddHtmlFile(web_package::WebBundleBuilder* builder,
                 const GURL& base_url,
                 const std::string& path,
                 const std::string& content);

void AddScriptFile(web_package::WebBundleBuilder* builder,
                   const GURL& base_url,
                   const std::string& path,
                   const std::string& content);

std::string CreatePathTestWebBundle(const GURL& base_url);

std::string CreateSubPageHtml(const std::string& page_info);

std::string CreateScriptForSubPageTest(const std::string& script_info);

void RegisterRequestHandlerForSubPageTest(net::EmbeddedTestServer* server,
                                          const std::string& prefix);

// Sets up |primary_server| and |third_party_server| to return server generated
// sub page HTML files and JavaScript files:
//  - |primary_server| will return a sub page file created by
//    CreateSubPageHtml("") for all URL which ends with "subpage", and returns a
//    script file created by CreateScriptForSubPageTest("") for all URL which
//    ends with "script".
//  - |third_party_server| will return a sub page file created by
//    CreateSubPageHtml("third-party:") for all URL which ends with "subpage",
//    and returns a script file created by
//    CreateScriptForSubPageTest("third-party:") for all URL which ends with
//    "script".
// And generates a web bundle file which contains the following files:
//  - in |primary_server|'s origin:
//    - /top : web bundle file's primary URL.
//    - /subpage : returns CreateSubPageHtml("wbn-page").
//    - /script : returns CreateScriptForSubPageTest("wbn-script").
//  - in |third_party_server|'s origin:
//    - /subpage : returns CreateSubPageHtml("third-party:wbn-page").
//    - /script : returns CreateScriptForSubPageTest("third-party:wbn-script").
// When the sub page is loaded using iframe or window.open(), a script of the
// URL hash of the sub page is loaded. And domAutomationController.send() will
// be called via postMessage(). So we can know whether the sub page and the
// script are loaded from the web bundle file or the server.
void SetUpSubPageTest(net::EmbeddedTestServer* primary_server,
                      net::EmbeddedTestServer* third_party_server,
                      GURL* primary_url_origin,
                      GURL* third_party_origin,
                      std::string* web_bundle_content);

std::string AddIframeAndWaitForMessage(const ToRenderFrameHost& adapter,
                                       const GURL& url);

std::string WindowOpenAndWaitForMessage(const ToRenderFrameHost& adapter,
                                        const GURL& url);

// Runs tests for subpages  (iframe / window.open()). This function calls
// |test_func| to create an iframe (AddIframeAndWaitForMessage) or to open a new
// window (WindowOpenAndWaitForMessage).
// |support_third_party_wbn_page| must be true when third party pages should be
// served from servers even if they are in the web bundle.
void RunSubPageTest(const ToRenderFrameHost& adapter,
                    const GURL& primary_url_origin,
                    const GURL& third_party_origin,
                    std::string (*test_func)(const content::ToRenderFrameHost&,
                                             const GURL&),
                    bool support_third_party_wbn_page);

std::string CreateHtmlForNavigationTest(const std::string& page_info,
                                        const std::string& additional_html);

std::string CreateScriptForNavigationTest(const std::string& script_info);

void AddHtmlAndScriptForNavigationTest(web_package::WebBundleBuilder* builder,
                                       const GURL& base_url,
                                       const std::string& path,
                                       const std::string& additional_html);

std::string GetLoadResultForNavigationTest(const ToRenderFrameHost& adapter);

// Sets up |server| to return server generated page HTML files and JavaScript
// files. |server| will returns a page file created by
// CreateHtmlForNavigationTest(relative_url + " from server") for all URL which
// ends with "page/", and returns a script file created by
// CreateScriptForNavigationTest(relative_url + " from server") for all URL
// which ends with "script".
void SetUpNavigationTestServer(net::EmbeddedTestServer* server,
                               GURL* url_origin);

void RunScriptAndObserveNavigation(
    const std::string& message,
    WebContents* web_contents,
    const ToRenderFrameHost& execution_target,
    const std::string& script,
    const std::vector<NavigationType> expected_navigation_types,
    const GURL& expected_last_comitted_url,
    const GURL& expected_last_inner_url,
    const std::string& expected_load_result);

void SetUpSharedNavigationsTest(net::EmbeddedTestServer* server,
                                const std::vector<std::string>& pathes,
                                GURL* url_origin,
                                std::string* web_bundle_content);

void SetUpBasicNavigationTest(net::EmbeddedTestServer* server,
                              GURL* url_origin,
                              std::string* web_bundle_content);

// Runs test for basic history navigations (back/forward/reload).
void RunBasicNavigationTest(
    WebContents* web_contents,
    const GURL& web_bundle_url,
    const GURL& url_origin,
    base::RepeatingCallback<GURL(const GURL&)> get_url_for_bundle);

void SetUpBrowserInitiatedOutOfBundleNavigationTest(
    net::EmbeddedTestServer* server,
    GURL* url_origin,
    std::string* web_bundle_content);

// Runs test for history navigations after browser initiated navigation going
// out of the web bundle.
void RunBrowserInitiatedOutOfBundleNavigationTest(
    WebContents* web_contents,
    const GURL& web_bundle_url,
    const GURL& url_origin,
    base::RepeatingCallback<GURL(const GURL&)> get_url_for_bundle);

void SetUpRendererInitiatedOutOfBundleNavigationTest(
    net::EmbeddedTestServer* server,
    GURL* url_origin,
    std::string* web_bundle_content);

// Runs test for history navigations after renderer initiated navigation going
// out of the web bundle.
void RunRendererInitiatedOutOfBundleNavigationTest(
    WebContents* web_contents,
    const GURL& web_bundle_url,
    const GURL& url_origin,
    base::RepeatingCallback<GURL(const GURL&)> get_url_for_bundle);

void SetUpSameDocumentNavigationTest(net::EmbeddedTestServer* server,
                                     GURL* url_origin,
                                     std::string* web_bundle_content);

// Runs test for history navigations after same document navigations.
void RunSameDocumentNavigationTest(
    WebContents* web_contents,
    const GURL& web_bundle_url,
    const GURL& url_origin,
    base::RepeatingCallback<GURL(const GURL&)> get_url_for_bundle);

void SetUpIframeNavigationTest(net::EmbeddedTestServer* server,
                               GURL* url_origin,
                               std::string* web_bundle_content);

// Runs test for history navigations with an iframe.
void RunIframeNavigationTest(
    WebContents* web_contents,
    const GURL& web_bundle_url,
    const GURL& url_origin,
    base::RepeatingCallback<GURL(const GURL&)> get_url_for_bundle);

// Runs test for navigations in an iframe after going out of the web bundle by
// changing location.href inside the iframe.
void RunIframeOutOfBundleNavigationTest(
    WebContents* web_contents,
    const GURL& web_bundle_url,
    const GURL& url_origin,
    base::RepeatingCallback<GURL(const GURL&)> get_url_for_bundle);

// Runs test for navigations in an iframe after going out of the web bundle by
// changing iframe.src from the parent frame.
void RunIframeParentInitiatedOutOfBundleNavigationTest(
    WebContents* web_contents,
    const GURL& web_bundle_url,
    const GURL& url_origin,
    base::RepeatingCallback<GURL(const GURL&)> get_url_for_bundle);

// Runs test for history navigations in an iframe after same document
// navigation.
void RunIframeSameDocumentNavigationTest(
    WebContents* web_contents,
    const GURL& web_bundle_url,
    const GURL& url_origin,
    base::RepeatingCallback<GURL(const GURL&)> get_url_for_bundle);

enum class TestFilePathMode {
  kNormalFilePath,
#if BUILDFLAG(IS_ANDROID)
  kContentURI,
#endif  // BUILDFLAG(IS_ANDROID)
};

// Adding web_bundle_browsertest_utils:: extra to the prefix so the files using
// these directives outside of the namespace don't fail.
#if BUILDFLAG(IS_ANDROID)
#define TEST_FILE_PATH_MODE_PARAMS                                     \
  testing::Values(                                                     \
      web_bundle_browsertest_utils::TestFilePathMode::kNormalFilePath, \
      web_bundle_browsertest_utils::TestFilePathMode::kContentURI)
#else
#define TEST_FILE_PATH_MODE_PARAMS \
  testing::Values(                 \
      web_bundle_browsertest_utils::TestFilePathMode::kNormalFilePath)
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace web_bundle_browsertest_utils
}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_BROWSERTEST_BASE_H_
