// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web_bundle_browsertest_base.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/common/content_client.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/http_response.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/content_uri_utils.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace content {
namespace web_bundle_browsertest_utils {

base::FilePath GetTestDataPath(base::StringPiece file) {
  base::FilePath test_data_dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
  return test_data_dir
      .Append(base::FilePath(FILE_PATH_LITERAL("content/test/data/web_bundle")))
      .AppendASCII(file);
}

#if BUILDFLAG(IS_ANDROID)
void CopyFileAndGetContentUri(const base::FilePath& file,
                              GURL* content_uri,
                              base::FilePath* new_file_path) {
  DCHECK(content_uri);
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath tmp_dir;
  ASSERT_TRUE(base::GetTempDir(&tmp_dir));
  // The directory name "web_bundle" must be kept in sync with
  // content/shell/android/browsertests_apk/res/xml/file_paths.xml
  base::FilePath tmp_wbn_dir = tmp_dir.AppendASCII("web_bundle");
  ASSERT_TRUE(base::CreateDirectoryAndGetError(tmp_wbn_dir, nullptr));
  base::FilePath tmp_dir_in_tmp_wbn_dir;
  ASSERT_TRUE(
      base::CreateTemporaryDirInDir(tmp_wbn_dir, "", &tmp_dir_in_tmp_wbn_dir));
  base::FilePath temp_file = tmp_dir_in_tmp_wbn_dir.Append(file.BaseName());
  ASSERT_TRUE(base::CopyFile(file, temp_file));
  if (new_file_path)
    *new_file_path = temp_file;
  *content_uri = GURL(base::GetContentUriFromFilePath(temp_file).value());
}
#endif  // BUILDFLAG(IS_ANDROID)

std::string ExecuteAndGetString(const ToRenderFrameHost& adapter,
                                const std::string& script) {
  return EvalJs(adapter, script).ExtractString();
}

void NavigateAndWaitForTitle(content::WebContents* web_contents,
                             const GURL& test_data_url,
                             const GURL& expected_commit_url,
                             base::StringPiece ascii_title) {
  std::u16string expected_title = base::ASCIIToUTF16(ascii_title);
  TitleWatcher title_watcher(web_contents, expected_title);
  EXPECT_TRUE(NavigateToURL(web_contents, test_data_url, expected_commit_url));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

DownloadObserver::DownloadObserver(DownloadManager* manager)
    : manager_(manager) {
  manager_->AddObserver(this);
}

DownloadObserver::~DownloadObserver() {
  manager_->RemoveObserver(this);
}

void DownloadObserver::WaitUntilDownloadCreated() {
  run_loop_.Run();
}
const GURL& DownloadObserver::observed_url() const {
  return url_;
}

// content::DownloadManager::Observer implementation.
void DownloadObserver::OnDownloadCreated(content::DownloadManager* manager,
                                         download::DownloadItem* item) {
  url_ = item->GetURL();
  run_loop_.Quit();
}

MockParserFactory::MockParserFactory(std::vector<GURL> urls,
                                     const base::FilePath& response_body_file) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  int64_t response_body_file_size;
  EXPECT_TRUE(base::GetFileSize(response_body_file, &response_body_file_size));

  base::flat_map<GURL, web_package::mojom::BundleResponseLocationPtr> requests;
  for (const auto& url : urls) {
    requests.insert({url, web_package::mojom::BundleResponseLocation::New(
                              0u, response_body_file_size)});
  }
  web_package::mojom::BundleMetadataPtr metadata =
      web_package::mojom::BundleMetadata::New();
  metadata->primary_url = urls[0];
  metadata->requests = std::move(requests);

  FinishSetUp(std::move(metadata));
}
MockParserFactory::MockParserFactory(
    const std::vector<std::pair<GURL, const std::string&>> items) {
  base::flat_map<GURL, web_package::mojom::BundleResponseLocationPtr> requests;
  uint64_t offset = 0;
  for (const auto& item : items) {
    requests.insert(
        {item.first, web_package::mojom::BundleResponseLocation::New(
                         offset, item.second.length())});
    offset += item.second.length();
  }
  web_package::mojom::BundleMetadataPtr metadata =
      web_package::mojom::BundleMetadata::New();
  metadata->primary_url = items[0].first;
  metadata->requests = std::move(requests);

  FinishSetUp(std::move(metadata));
}

void MockParserFactory::FinishSetUp(
    web_package::mojom::BundleMetadataPtr metadata) {
  wrapped_factory_.SetMetadataParseResult(std::move(metadata));

  web_package::mojom::BundleResponsePtr response =
      web_package::mojom::BundleResponse::New();
  response->response_code = 200;
  response->response_headers.insert({"content-type", "text/html"});
  wrapped_factory_.SetResponseParseResult(std::move(response));

  in_process_data_decoder_.service().SetWebBundleParserFactoryBinderForTesting(
      base::BindRepeating(&MockParserFactory::BindWebBundleParserFactory,
                          base::Unretained(this)));
}

int MockParserFactory::GetParserCreationCount() const {
  return wrapped_factory_.GetParserCreationCount();
}
void MockParserFactory::SimulateParserDisconnect() {
  wrapped_factory_.SimulateParserDisconnect();
}
void MockParserFactory::SimulateParseMetadataCrash() {
  wrapped_factory_.SimulateParseMetadataCrash();
}
void MockParserFactory::SimulateParseResponseCrash() {
  wrapped_factory_.SimulateParseResponseCrash();
}

void MockParserFactory::BindWebBundleParserFactory(
    mojo::PendingReceiver<web_package::mojom::WebBundleParserFactory>
        receiver) {
  wrapped_factory_.AddReceiver(std::move(receiver));
}

bool TestBrowserClient::CanAcceptUntrustedExchangesIfNeeded() {
  return true;
}

void WebBundleBrowserTestBase::SetUpOnMainThread() {
  ContentBrowserTest::SetUpOnMainThread();
  original_client_ = SetBrowserClientForTesting(&browser_client_);
}

void WebBundleBrowserTestBase::TearDownOnMainThread() {
  ContentBrowserTest::TearDownOnMainThread();
  SetBrowserClientForTesting(original_client_);
}

void WebBundleBrowserTestBase::NavigateToBundleAndWaitForReady(
    const GURL& test_data_url,
    const GURL& expected_commit_url) {
  NavigateAndWaitForTitle(shell()->web_contents(), test_data_url,
                          expected_commit_url, "Ready");
}

void WebBundleBrowserTestBase::RunTestScript(const std::string& script) {
  EXPECT_TRUE(ExecJs(shell()->web_contents(), "loadScript('" + script + "');"));
  std::u16string ok = u"OK";
  TitleWatcher title_watcher(shell()->web_contents(), ok);
  title_watcher.AlsoWaitForTitle(u"FAIL");
  EXPECT_EQ(ok, title_watcher.WaitAndGetTitle());
}

void WebBundleBrowserTestBase::ExecuteScriptAndWaitForTitle(
    const std::string& script,
    const std::string& title) {
  std::u16string title16 = base::ASCIIToUTF16(title);
  TitleWatcher title_watcher(shell()->web_contents(), title16);
  EXPECT_TRUE(ExecJs(shell()->web_contents(), script));
  EXPECT_EQ(title16, title_watcher.WaitAndGetTitle());
}

void WebBundleBrowserTestBase::NavigateToURLAndWaitForTitle(
    const GURL& url,
    const std::string& title) {
  ExecuteScriptAndWaitForTitle(
      base::StringPrintf("location.href = '%s';", url.spec().c_str()), title);
}

void WebBundleBrowserTestBase::CreateTemporaryWebBundleFile(
    const std::string& content,
    base::FilePath* file_path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (!temp_dir_.IsValid()) {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }
  base::FilePath tmp_file_path;
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &tmp_file_path));
  if (!content.empty())
    ASSERT_TRUE(base::WriteFile(tmp_file_path, content));
  *file_path = tmp_file_path.AddExtension(FILE_PATH_LITERAL(".wbn"));
  ASSERT_TRUE(base::Move(tmp_file_path, *file_path));
}

FinishNavigationObserver::FinishNavigationObserver(
    WebContents* contents,
    base::OnceClosure done_closure)
    : WebContentsObserver(contents), done_closure_(std::move(done_closure)) {}

FinishNavigationObserver::~FinishNavigationObserver() = default;

void FinishNavigationObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  navigation_types_.push_back(
      NavigationRequest::From(navigation_handle)->navigation_type());
  error_code_ = navigation_handle->GetNetErrorCode();
  --navigations_remaining_;

  if (navigations_remaining_ == 0)
    std::move(done_closure_).Run();
}

void FinishNavigationObserver::set_navigations_remaining(
    int navigations_remaining) {
  navigations_remaining_ = navigations_remaining;
}

const absl::optional<net::Error>& FinishNavigationObserver::error_code() const {
  return error_code_;
}
const std::vector<NavigationType>& FinishNavigationObserver::navigation_types()
    const {
  return navigation_types_;
}

std::string ExpectNavigationFailureAndReturnConsoleMessage(
    content::WebContents* web_contents,
    const GURL& url) {
  WebContentsConsoleObserver console_observer(web_contents);
  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(web_contents,
                                                      run_loop.QuitClosure());
  EXPECT_FALSE(NavigateToURL(web_contents, url));
  run_loop.Run();
  if (!finish_navigation_observer.error_code()) {
    ADD_FAILURE() << "Unexpected navigation success: " << url;
    return std::string();
  }

  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            *finish_navigation_observer.error_code());
  if (console_observer.messages().empty())
    EXPECT_TRUE(console_observer.Wait());

  if (console_observer.messages().empty()) {
    ADD_FAILURE() << "Could not find a console message.";
    return std::string();
  }
  return base::UTF16ToUTF8(console_observer.messages()[0].message);
}

FrameTreeNode* GetFirstChild(WebContents* web_contents) {
  return static_cast<WebContentsImpl*>(web_contents)
      ->GetPrimaryFrameTree()
      .root()
      ->child_at(0);
}

std::string CreateSimpleWebBundle(const GURL& primary_url) {
  web_package::WebBundleBuilder builder;
  builder.AddPrimaryURL(primary_url);
  builder.AddExchange(primary_url,
                      {{":status", "200"}, {"content-type", "text/html"}},
                      "<title>Ready</title>");
  std::vector<uint8_t> bundle = builder.CreateBundle();
  return std::string(bundle.begin(), bundle.end());
}

void AddHtmlFile(web_package::WebBundleBuilder* builder,
                 const GURL& base_url,
                 const std::string& path,
                 const std::string& content) {
  builder->AddExchange(base_url.Resolve(path),
                       {{":status", "200"}, {"content-type", "text/html"}},
                       content);
}

void AddScriptFile(web_package::WebBundleBuilder* builder,
                   const GURL& base_url,
                   const std::string& path,
                   const std::string& content) {
  builder->AddExchange(
      base_url.Resolve(path),
      {{":status", "200"}, {"content-type", "application/javascript"}},
      content);
}

std::string CreatePathTestWebBundle(const GURL& base_url) {
  const std::string primary_url_path = "/web_bundle/path_test/in_scope/";
  web_package::WebBundleBuilder builder;
  builder.AddPrimaryURL(base_url.Resolve(primary_url_path));
  AddHtmlFile(&builder, base_url, primary_url_path, "<title>Ready</title>");
  AddHtmlFile(
      &builder, base_url, "/web_bundle/path_test/in_scope/page.html",
      "<script>const page_info = 'In scope page in Web Bundle';</script>"
      "<script src=\"page.js\"></script>");
  AddScriptFile(
      &builder, base_url, "/web_bundle/path_test/in_scope/page.js",
      "document.title = page_info + ' / in scope script in Web Bundle';");
  AddHtmlFile(
      &builder, base_url, "/web_bundle/path_test/out_scope/page.html",
      "<script>const page_info = 'Out scope page in Web Bundle';</script>"
      "<script src=\"page.js\"></script>");
  AddScriptFile(
      &builder, base_url, "/web_bundle/path_test/out_scope/page.js",
      "document.title = page_info + ' / out scope script in Web Bundle';");
  std::vector<uint8_t> bundle = builder.CreateBundle();
  return std::string(bundle.begin(), bundle.end());
}

std::string CreateSubPageHtml(const std::string& page_info) {
  return base::StringPrintf(R"(
    <body><script>
      const page_info = '%s';
      let script  = document.createElement('script');
      script.src = location.hash.substr(1);
      document.body.appendChild(script);
    </script></body>)",
                            page_info.c_str());
}

std::string CreateScriptForSubPageTest(const std::string& script_info) {
  return base::StringPrintf(
      R"(
        if (window.opener) {
          window.opener.postMessage(page_info + ' %s', '*');
        } else {
          window.parent.window.postMessage(page_info + ' %s', '*');
        }
        )",
      script_info.c_str(), script_info.c_str());
}

void RegisterRequestHandlerForSubPageTest(net::EmbeddedTestServer* server,
                                          const std::string& prefix) {
  server->RegisterRequestHandler(base::BindRepeating(
      [](const std::string& prefix,
         const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (base::EndsWith(request.relative_url, "subpage",
                           base::CompareCase::SENSITIVE)) {
          return std::make_unique<net::test_server::RawHttpResponse>(
              kHeadersForHtml, CreateSubPageHtml(prefix + "server-page"));
        }
        if (base::EndsWith(request.relative_url, "script",
                           base::CompareCase::SENSITIVE)) {
          return std::make_unique<net::test_server::RawHttpResponse>(
              kHeadersForJavaScript,
              CreateScriptForSubPageTest(prefix + "server-script"));
        }
        return nullptr;
      },
      prefix));
}

void SetUpSubPageTest(net::EmbeddedTestServer* primary_server,
                      net::EmbeddedTestServer* third_party_server,
                      GURL* primary_url_origin,
                      GURL* third_party_origin,
                      std::string* web_bundle_content) {
  RegisterRequestHandlerForSubPageTest(primary_server, "");
  RegisterRequestHandlerForSubPageTest(third_party_server, "third-party:");

  ASSERT_TRUE(primary_server->Start());
  ASSERT_TRUE(third_party_server->Start());
  *primary_url_origin = primary_server->GetURL("/");
  *third_party_origin = third_party_server->GetURL("/");

  web_package::WebBundleBuilder builder;
  builder.AddPrimaryURL(primary_url_origin->Resolve("/top"));
  AddHtmlFile(&builder, *primary_url_origin, "/top", R"(
    <script>
    window.addEventListener('message',
                            event => domAutomationController.send(event.data),
                            false);
    document.title = 'Ready';
    </script>
    )");
  AddHtmlFile(&builder, *primary_url_origin, "/subpage",
              CreateSubPageHtml("wbn-page"));
  AddScriptFile(&builder, *primary_url_origin, "/script",
                CreateScriptForSubPageTest("wbn-script"));

  AddHtmlFile(&builder, *third_party_origin, "/subpage",
              CreateSubPageHtml("third-party:wbn-page"));
  AddScriptFile(&builder, *third_party_origin, "/script",
                CreateScriptForSubPageTest("third-party:wbn-script"));

  std::vector<uint8_t> bundle = builder.CreateBundle();
  *web_bundle_content = std::string(bundle.begin(), bundle.end());
}

std::string AddIframeAndWaitForMessage(const ToRenderFrameHost& adapter,
                                       const GURL& url) {
  return EvalJs(adapter,
                JsReplace(
                    R"(
  (function(){
    const iframe = document.createElement('iframe');
    iframe.src = $1;
    document.body.appendChild(iframe);
  })();
  )",
                    url),
                EXECUTE_SCRIPT_USE_MANUAL_REPLY)
      .ExtractString();
}

std::string WindowOpenAndWaitForMessage(const ToRenderFrameHost& adapter,
                                        const GURL& url) {
  return EvalJs(adapter,
                JsReplace(R"(
        if (document.last_win) {
          // Close the latest window to avoid OOM-killer on Android.
          document.last_win.close();
        }
        document.last_win = window.open($1, '_blank');
      )",
                          url),
                EXECUTE_SCRIPT_USE_MANUAL_REPLY)
      .ExtractString();
}

void RunSubPageTest(const ToRenderFrameHost& adapter,
                    const GURL& primary_url_origin,
                    const GURL& third_party_origin,
                    std::string (*test_func)(const content::ToRenderFrameHost&,
                                             const GURL&),
                    bool support_third_party_wbn_page) {
  EXPECT_EQ(
      "wbn-page wbn-script",
      (*test_func)(adapter,
                   primary_url_origin.Resolve("/subpage").Resolve("#/script")));
  EXPECT_EQ("wbn-page server-script",
            (*test_func)(adapter, primary_url_origin.Resolve("/subpage")
                                      .Resolve("#/not-in-wbn-script")));

  EXPECT_EQ(
      support_third_party_wbn_page ? "wbn-page third-party:wbn-script"
                                   : "wbn-page third-party:server-script",
      (*test_func)(adapter,
                   primary_url_origin.Resolve("/subpage")
                       .Resolve(std::string("#") +
                                third_party_origin.Resolve("/script").spec())));
  EXPECT_EQ(
      "wbn-page third-party:server-script",
      (*test_func)(
          adapter,
          primary_url_origin.Resolve("/subpage")
              .Resolve(
                  std::string("#") +
                  third_party_origin.Resolve("/not-in-wbn-script").spec())));

  EXPECT_EQ(
      "server-page server-script",
      (*test_func)(adapter, primary_url_origin.Resolve("/not-in-wbn-subpage")
                                .Resolve("#/script")));
  EXPECT_EQ(
      support_third_party_wbn_page
          ? "third-party:wbn-page third-party:wbn-script"
          : "third-party:server-page third-party:server-script",
      (*test_func)(adapter,
                   third_party_origin.Resolve("/subpage").Resolve("#script")));
  EXPECT_EQ(
      "third-party:server-page third-party:server-script",
      (*test_func)(adapter, third_party_origin.Resolve("/not-in-wbn-subpage")
                                .Resolve("#script")));
}

std::string CreateHtmlForNavigationTest(const std::string& page_info,
                                        const std::string& additional_html) {
  return base::StringPrintf(
      R"(
        <body><script>
        document.page_info = '%s';
        document.title='Ready';
        </script>%s</body>
      )",
      page_info.c_str(), additional_html.c_str());
}

std::string CreateScriptForNavigationTest(const std::string& script_info) {
  return base::StringPrintf("document.script_info = '%s';",
                            script_info.c_str());
}

void AddHtmlAndScriptForNavigationTest(web_package::WebBundleBuilder* builder,
                                       const GURL& base_url,
                                       const std::string& path,
                                       const std::string& additional_html) {
  AddHtmlFile(builder, base_url, path,
              CreateHtmlForNavigationTest(path + " from wbn", additional_html));
  AddScriptFile(builder, base_url, path + "script",
                CreateScriptForNavigationTest(path + "script from wbn"));
}

std::string GetLoadResultForNavigationTest(const ToRenderFrameHost& adapter) {
  std::string script = R"(
    (async () => {
      const script = document.createElement('script');
      script.src = './script';
      script.addEventListener('load', () => {
          domAutomationController.send(
                document.page_info + ', ' + document.script_info);
        }, false);
      script.addEventListener('error', () => {
          domAutomationController.send(
                document.page_info + ' failed to load script');
        }, false);

      if (!document.body) {
        await new Promise((resolve) => {
          document.addEventListener('DOMContentLoaded', resolve);
        });
      }
      document.body.appendChild(script);
    })()
    )";
  return EvalJs(adapter, script, EXECUTE_SCRIPT_USE_MANUAL_REPLY)
      .ExtractString();
}

void SetUpNavigationTestServer(net::EmbeddedTestServer* server,
                               GURL* url_origin) {
  server->RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (base::EndsWith(request.relative_url, "page/",
                           base::CompareCase::SENSITIVE)) {
          return std::make_unique<net::test_server::RawHttpResponse>(
              kHeadersForHtml, CreateHtmlForNavigationTest(
                                   request.relative_url + " from server", ""));
        }
        if (base::EndsWith(request.relative_url, "script",
                           base::CompareCase::SENSITIVE)) {
          return std::make_unique<net::test_server::RawHttpResponse>(
              kHeadersForJavaScript,
              CreateScriptForNavigationTest(request.relative_url +
                                            " from server"));
        }
        return nullptr;
      }));

  ASSERT_TRUE(server->Start());
  *url_origin = server->GetURL("/");
}

void RunScriptAndObserveNavigation(
    const std::string& message,
    WebContents* web_contents,
    const ToRenderFrameHost& execution_target,
    const std::string& script,
    const std::vector<NavigationType> expected_navigation_types,
    const GURL& expected_last_comitted_url,
    const GURL& expected_last_inner_url,
    const std::string& expected_load_result) {
  SCOPED_TRACE(message);
  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(web_contents,
                                                      run_loop.QuitClosure());
  finish_navigation_observer.set_navigations_remaining(
      expected_navigation_types.size());
  EXPECT_TRUE(ExecJs(execution_target, script));
  run_loop.Run();
  EXPECT_EQ(finish_navigation_observer.navigation_types(),
            expected_navigation_types);
  EXPECT_EQ(web_contents->GetLastCommittedURL(), expected_last_comitted_url);
  EXPECT_EQ(expected_load_result, GetLoadResultForNavigationTest(web_contents));
  EXPECT_EQ(ExecuteAndGetString(web_contents, "window.location.href"),
            expected_last_inner_url);
  EXPECT_EQ(ExecuteAndGetString(web_contents, "document.location.href"),
            expected_last_inner_url);
  EXPECT_EQ(ExecuteAndGetString(web_contents, "document.URL"),
            expected_last_inner_url);
}

void SetUpSharedNavigationsTest(net::EmbeddedTestServer* server,
                                const std::vector<std::string>& pathes,
                                GURL* url_origin,
                                std::string* web_bundle_content) {
  SetUpNavigationTestServer(server, url_origin);
  web_package::WebBundleBuilder builder;
  builder.AddPrimaryURL(url_origin->Resolve("/top-page/"));
  for (const auto& path : pathes)
    AddHtmlAndScriptForNavigationTest(&builder, *url_origin, path, "");

  std::vector<uint8_t> bundle = builder.CreateBundle();
  *web_bundle_content = std::string(bundle.begin(), bundle.end());
}

void SetUpBasicNavigationTest(net::EmbeddedTestServer* server,
                              GURL* url_origin,
                              std::string* web_bundle_content) {
  SetUpSharedNavigationsTest(server, {"/top-page/", "/1-page/", "/2-page/"},
                             url_origin, web_bundle_content);
}

// Runs test for basic history navigations (back/forward/reload).
void RunBasicNavigationTest(
    WebContents* web_contents,
    const GURL& web_bundle_url,
    const GURL& url_origin,
    base::RepeatingCallback<GURL(const GURL&)> get_url_for_bundle) {
  NavigateAndWaitForTitle(
      web_contents, web_bundle_url,
      get_url_for_bundle.Run(url_origin.Resolve("/top-page/")), "Ready");
  RunScriptAndObserveNavigation(
      "Navigate to /1-page/", web_contents, web_contents /* execution_target */,
      "location.href = '/1-page/';", {NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY},
      get_url_for_bundle.Run(
          url_origin.Resolve("/1-page/")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/1-page/") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Navigate to /2-page/", web_contents, web_contents /* execution_target */,
      "location.href = '/2-page/';", {NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY},
      get_url_for_bundle.Run(
          url_origin.Resolve("/2-page/")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/2-page/") /* expected_last_inner_url */,
      "/2-page/ from wbn, /2-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Back navigate to /1-page/", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      get_url_for_bundle.Run(
          url_origin.Resolve("/1-page/")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/1-page/") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Back navigate to /top-page/", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      get_url_for_bundle.Run(
          url_origin.Resolve("/top-page/")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/top-page/") /* expected_last_inner_url */,
      "/top-page/ from wbn, /top-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Forward navigate to /1-page/", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      get_url_for_bundle.Run(
          url_origin.Resolve("/1-page/")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/1-page/") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Reload /1-page/", web_contents, web_contents /* execution_target */,
      "location.reload();", {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      get_url_for_bundle.Run(
          url_origin.Resolve("/1-page/")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/1-page/") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Forward navigate to /2-page/", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      get_url_for_bundle.Run(
          url_origin.Resolve("/2-page/")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/2-page/") /* expected_last_inner_url */,
      "/2-page/ from wbn, /2-page/script from wbn");
}

void SetUpBrowserInitiatedOutOfBundleNavigationTest(
    net::EmbeddedTestServer* server,
    GURL* url_origin,
    std::string* web_bundle_content) {
  SetUpSharedNavigationsTest(
      server, {"/top-page/", "/1-page/", "/2-page/", "/3-page/", "/4-page/"},
      url_origin, web_bundle_content);
}

void RunBrowserInitiatedOutOfBundleNavigationTest(
    WebContents* web_contents,
    const GURL& web_bundle_url,
    const GURL& url_origin,
    base::RepeatingCallback<GURL(const GURL&)> get_url_for_bundle) {
  NavigateAndWaitForTitle(
      web_contents, web_bundle_url,
      get_url_for_bundle.Run(url_origin.Resolve("/top-page/")), "Ready");
  RunScriptAndObserveNavigation(
      "Navigate to /1-page/", web_contents, web_contents /* execution_target */,
      "location.href = '/1-page/';", {NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY},
      get_url_for_bundle.Run(
          url_origin.Resolve("/1-page/")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/1-page/") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Navigate to /2-page/", web_contents, web_contents /* execution_target */,
      "location.href = '/2-page/';", {NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY},
      get_url_for_bundle.Run(
          url_origin.Resolve("/2-page/")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/2-page/") /* expected_last_inner_url */,
      "/2-page/ from wbn, /2-page/script from wbn");
  {
    SCOPED_TRACE("Browser initiated navigation to /3-page/");
    EXPECT_TRUE(NavigateToURL(web_contents, url_origin.Resolve("/3-page/")));
    EXPECT_EQ(web_contents->GetLastCommittedURL(),
              url_origin.Resolve("/3-page/"));
    // Browser initiated navigation must be loaded from the server even if the
    // page is in the web bundle.
    EXPECT_EQ("/3-page/ from server, /3-page/script from server",
              GetLoadResultForNavigationTest(web_contents));
  }
  // Navigation from the out of web bundle page must be loaded from the server
  // even if the page is in the web bundle.
  RunScriptAndObserveNavigation(
      "Navigate to /4-page/", web_contents, web_contents /* execution_target */,
      "location.href = '/4-page/';", {NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY},
      url_origin.Resolve("/4-page/") /* expected_last_comitted_url */,
      url_origin.Resolve("/4-page/") /* expected_last_inner_url */,
      "/4-page/ from server, /4-page/script from server");
  RunScriptAndObserveNavigation(
      "Back navigate to /3-page/", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      url_origin.Resolve("/3-page/") /* expected_last_comitted_url */,
      url_origin.Resolve("/3-page/") /* expected_last_inner_url */,
      "/3-page/ from server, /3-page/script from server");
  RunScriptAndObserveNavigation(
      "Back navigate to /2-page/", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      get_url_for_bundle.Run(
          url_origin.Resolve("/2-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/2-page/") /* expected_last_inner_url */,
      "/2-page/ from wbn, /2-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Back navigate to /1-page/", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      get_url_for_bundle.Run(
          url_origin.Resolve("/1-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/1-page/") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Forward navigate to /2-page/", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      get_url_for_bundle.Run(
          url_origin.Resolve("/2-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/2-page/") /* expected_last_inner_url */,
      "/2-page/ from wbn, /2-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Forward navigate to /3-page/", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      url_origin.Resolve("/3-page/") /* expected_last_comitted_url */,
      url_origin.Resolve("/3-page/") /* expected_last_inner_url */,
      "/3-page/ from server, /3-page/script from server");
  RunScriptAndObserveNavigation(
      "Forward navigate to /4-page/", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      url_origin.Resolve("/4-page/") /* expected_last_comitted_url */,
      url_origin.Resolve("/4-page/") /* expected_last_inner_url */,
      "/4-page/ from server, /4-page/script from server");
}

void SetUpRendererInitiatedOutOfBundleNavigationTest(
    net::EmbeddedTestServer* server,
    GURL* url_origin,
    std::string* web_bundle_content) {
  SetUpSharedNavigationsTest(server,
                             {"/top-page/", "/1-page/", "/2-page/", "/3-page/"},
                             url_origin, web_bundle_content);
}

void RunRendererInitiatedOutOfBundleNavigationTest(
    WebContents* web_contents,
    const GURL& web_bundle_url,
    const GURL& url_origin,
    base::RepeatingCallback<GURL(const GURL&)> get_url_for_bundle) {
  NavigateAndWaitForTitle(
      web_contents, web_bundle_url,
      get_url_for_bundle.Run(url_origin.Resolve("/top-page/")), "Ready");
  RunScriptAndObserveNavigation(
      "Navigate to /1-page/", web_contents, web_contents /* execution_target */,
      "location.href = '/1-page/';", {NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY},
      get_url_for_bundle.Run(
          url_origin.Resolve("/1-page/")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/1-page/") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Navigate to /2-page/", web_contents, web_contents /* execution_target */,
      "location.href = '/2-page/';", {NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY},
      get_url_for_bundle.Run(
          url_origin.Resolve("/2-page/")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/2-page/") /* expected_last_inner_url */,
      "/2-page/ from wbn, /2-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Navigate to /server-page/", web_contents,
      web_contents /* execution_target */, "location.href = '/server-page/';",
      {NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY},
      url_origin.Resolve("/server-page/") /* expected_last_comitted_url */,
      url_origin.Resolve("/server-page/") /* expected_last_inner_url */,
      "/server-page/ from server, /server-page/script from server");
  // Navigation from the out of web bundle page must be loaded from the server
  // even if the page is in the web bundle.
  RunScriptAndObserveNavigation(
      "Navigate to /3-page/", web_contents, web_contents /* execution_target */,
      "location.href = '/3-page/';", {NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY},
      url_origin.Resolve("/3-page/") /* expected_last_comitted_url */,
      url_origin.Resolve("/3-page/") /* expected_last_inner_url */,
      "/3-page/ from server, /3-page/script from server");
  RunScriptAndObserveNavigation(
      "Back navigate to /server-page/", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      url_origin.Resolve("/server-page/") /* expected_last_comitted_url */,
      url_origin.Resolve("/server-page/") /* expected_last_inner_url */,
      "/server-page/ from server, /server-page/script from server");
  RunScriptAndObserveNavigation(
      "Back navigate to /2-page/", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      get_url_for_bundle.Run(
          url_origin.Resolve("/2-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/2-page/") /* expected_last_inner_url */,
      "/2-page/ from wbn, /2-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Back navigate to /1-page/", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      get_url_for_bundle.Run(
          url_origin.Resolve("/1-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/1-page/") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Forward navigate to /2-page/", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      get_url_for_bundle.Run(
          url_origin.Resolve("/2-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/2-page/") /* expected_last_inner_url */,
      "/2-page/ from wbn, /2-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Forward navigate to /server-page/", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      url_origin.Resolve("/server-page/") /* expected_last_comitted_url */,
      url_origin.Resolve("/server-page/") /* expected_last_inner_url */,
      "/server-page/ from server, /server-page/script from server");
  RunScriptAndObserveNavigation(
      "Forward navigate to /3-page/", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      url_origin.Resolve("/3-page/") /* expected_last_comitted_url */,
      url_origin.Resolve("/3-page/") /* expected_last_inner_url */,
      "/3-page/ from server, /3-page/script from server");
}

void SetUpSameDocumentNavigationTest(net::EmbeddedTestServer* server,
                                     GURL* url_origin,
                                     std::string* web_bundle_content) {
  SetUpSharedNavigationsTest(server, {"/top-page/", "/1-page/", "/2-page/"},
                             url_origin, web_bundle_content);
}

void RunSameDocumentNavigationTest(
    WebContents* web_contents,
    const GURL& web_bundle_url,
    const GURL& url_origin,
    base::RepeatingCallback<GURL(const GURL&)> get_url_for_bundle) {
  NavigateAndWaitForTitle(
      web_contents, web_bundle_url,
      get_url_for_bundle.Run(url_origin.Resolve("/top-page/")), "Ready");
  RunScriptAndObserveNavigation(
      "Navigate to /1-page/", web_contents, web_contents /* execution_target */,
      "location.href = '/1-page/';", {NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY},
      get_url_for_bundle.Run(
          url_origin.Resolve("/1-page/")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/1-page/") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Navigate to /1-page/#hash1", web_contents,
      web_contents /* execution_target */, "location.href = '#hash1';",
      {NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/1-page/#hash1")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/1-page/#hash1") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Navigate to /1-page/#hash2", web_contents,
      web_contents /* execution_target */, "location.href = '#hash2';",
      {NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/1-page/#hash2")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/1-page/#hash2") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Navigate to /2-page/", web_contents, web_contents /* execution_target */,
      "location.href = '/2-page/';", {NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY},
      get_url_for_bundle.Run(
          url_origin.Resolve("/2-page/")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/2-page/") /* expected_last_inner_url */,
      "/2-page/ from wbn, /2-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Back navigate to /1-page/#hash2", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/1-page/#hash2")) /* expected_last_comitted_url */,
      url_origin.Resolve("/1-page/#hash2") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Back navigate to /1-page/#hash1", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/1-page/#hash1")) /* expected_last_comitted_url */,
      url_origin.Resolve("/1-page/#hash1") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Back navigate to /1-page/", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      get_url_for_bundle.Run(
          url_origin.Resolve("/1-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/1-page/") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Forward navigate to /1-page/#hash1", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/1-page/#hash1")) /* expected_last_comitted_url */,
      url_origin.Resolve("/1-page/#hash1") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Forward navigate to /1-page/#hash2", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/1-page/#hash2")) /* expected_last_comitted_url */,
      url_origin.Resolve("/1-page/#hash2") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Forward navigate to /2-page/", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      get_url_for_bundle.Run(
          url_origin.Resolve("/2-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/2-page/") /* expected_last_inner_url */,
      "/2-page/ from wbn, /2-page/script from wbn");
}

void SetUpIframeNavigationTest(net::EmbeddedTestServer* server,
                               GURL* url_origin,
                               std::string* web_bundle_content) {
  SetUpNavigationTestServer(server, url_origin);
  web_package::WebBundleBuilder builder;
  builder.AddPrimaryURL(url_origin->Resolve("/top-page/"));
  const std::vector<std::string> pathes = {"/top-page/", "/1-page/",
                                           "/2-page/"};
  for (const auto& path : pathes)
    AddHtmlAndScriptForNavigationTest(&builder, *url_origin, path, "");
  AddHtmlAndScriptForNavigationTest(&builder, *url_origin, "/iframe-test-page/",
                                    "<iframe src=\"/1-page/\" />");

  std::vector<uint8_t> bundle = builder.CreateBundle();
  *web_bundle_content = std::string(bundle.begin(), bundle.end());
}

void RunIframeNavigationTest(
    WebContents* web_contents,
    const GURL& web_bundle_url,
    const GURL& url_origin,
    base::RepeatingCallback<GURL(const GURL&)> get_url_for_bundle) {
  // The test assumes the previous page gets deleted after navigation and doing
  // back navigation will recreate the page. Disable back/forward cache to
  // ensure that it doesn't get preserved in the cache.
  DisableBackForwardCacheForTesting(web_contents,
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);
  NavigateAndWaitForTitle(
      web_contents, web_bundle_url,
      get_url_for_bundle.Run(url_origin.Resolve("/top-page/")), "Ready");

  RunScriptAndObserveNavigation(
      "Navigate to /iframe-test-page/", web_contents,
      web_contents /* execution_target */,
      "location.href = '/iframe-test-page/';",
      {NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, NAVIGATION_TYPE_AUTO_SUBFRAME},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/iframe-test-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/iframe-test-page/") /* expected_last_inner_url */,
      "/iframe-test-page/ from wbn, /iframe-test-page/script from wbn");
  EXPECT_EQ("/1-page/ from wbn, /1-page/script from wbn",
            GetLoadResultForNavigationTest(GetFirstChild(web_contents)));

  RunScriptAndObserveNavigation(
      "Navigate the iframe to /2-page/", web_contents,
      GetFirstChild(web_contents) /* execution_target */,
      "location.href = '/2-page/';", {NAVIGATION_TYPE_NEW_SUBFRAME},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/iframe-test-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/iframe-test-page/") /* expected_last_inner_url */,
      "/iframe-test-page/ from wbn, /iframe-test-page/script from wbn");
  EXPECT_EQ("/2-page/ from wbn, /2-page/script from wbn",
            GetLoadResultForNavigationTest(GetFirstChild(web_contents)));

  RunScriptAndObserveNavigation(
      "Back navigate the iframe to /1-page/", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_AUTO_SUBFRAME},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/iframe-test-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/iframe-test-page/") /* expected_last_inner_url */,
      "/iframe-test-page/ from wbn, /iframe-test-page/script from wbn");
  EXPECT_EQ("/1-page/ from wbn, /1-page/script from wbn",
            GetLoadResultForNavigationTest(GetFirstChild(web_contents)));

  RunScriptAndObserveNavigation(
      "Back navigate to /top-page/", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      get_url_for_bundle.Run(
          url_origin.Resolve("/top-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/top-page/") /* expected_last_inner_url */,
      "/top-page/ from wbn, /top-page/script from wbn");

  RunScriptAndObserveNavigation(
      "Forward navigate to /iframe-test-page/", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
       NAVIGATION_TYPE_AUTO_SUBFRAME},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/iframe-test-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/iframe-test-page/") /* expected_last_inner_url */,
      "/iframe-test-page/ from wbn, /iframe-test-page/script from wbn");
  EXPECT_EQ("/1-page/ from wbn, /1-page/script from wbn",
            GetLoadResultForNavigationTest(GetFirstChild(web_contents)));

  RunScriptAndObserveNavigation(
      "Forward navigate the iframe to /2-page/", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_AUTO_SUBFRAME},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/iframe-test-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/iframe-test-page/") /* expected_last_inner_url */,
      "/iframe-test-page/ from wbn, /iframe-test-page/script from wbn");
  EXPECT_EQ("/2-page/ from wbn, /2-page/script from wbn",
            GetLoadResultForNavigationTest(GetFirstChild(web_contents)));
}

void RunIframeOutOfBundleNavigationTest(
    WebContents* web_contents,
    const GURL& web_bundle_url,
    const GURL& url_origin,
    base::RepeatingCallback<GURL(const GURL&)> get_url_for_bundle) {
  NavigateAndWaitForTitle(
      web_contents, web_bundle_url,
      get_url_for_bundle.Run(url_origin.Resolve("/top-page/")), "Ready");

  RunScriptAndObserveNavigation(
      "Navigate to /iframe-test-page/", web_contents,
      web_contents /* execution_target */,
      "location.href = '/iframe-test-page/';",
      {NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, NAVIGATION_TYPE_AUTO_SUBFRAME},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/iframe-test-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/iframe-test-page/") /* expected_last_inner_url */,
      "/iframe-test-page/ from wbn, /iframe-test-page/script from wbn");
  EXPECT_EQ("/1-page/ from wbn, /1-page/script from wbn",
            GetLoadResultForNavigationTest(GetFirstChild(web_contents)));

  // The web bundle doesn't contain /server-page/. So the server returns the
  // page and script.
  RunScriptAndObserveNavigation(
      "Navigate the iframe to /server-page/", web_contents,
      GetFirstChild(web_contents) /* execution_target */,
      "location.href = /server-page/;", {NAVIGATION_TYPE_NEW_SUBFRAME},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/iframe-test-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/iframe-test-page/") /* expected_last_inner_url */,
      "/iframe-test-page/ from wbn, /iframe-test-page/script from wbn");
  EXPECT_EQ("/server-page/ from server, /server-page/script from server",
            GetLoadResultForNavigationTest(GetFirstChild(web_contents)));

  // Even if location.href is changed by /server-page/, /1-page/ is loaded from
  // the bundle.
  RunScriptAndObserveNavigation(
      "Navigate the iframe to /1-page/", web_contents,
      GetFirstChild(web_contents) /* execution_target */,
      "location.href = /1-page/;", {NAVIGATION_TYPE_NEW_SUBFRAME},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/iframe-test-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/iframe-test-page/") /* expected_last_inner_url */,
      "/iframe-test-page/ from wbn, /iframe-test-page/script from wbn");
  EXPECT_EQ("/1-page/ from wbn, /1-page/script from wbn",
            GetLoadResultForNavigationTest(GetFirstChild(web_contents)));
}

// Runs test for navigations in an iframe after going out of the web bundle by
// changing iframe.src from the parent frame.
void RunIframeParentInitiatedOutOfBundleNavigationTest(
    WebContents* web_contents,
    const GURL& web_bundle_url,
    const GURL& url_origin,
    base::RepeatingCallback<GURL(const GURL&)> get_url_for_bundle) {
  NavigateAndWaitForTitle(
      web_contents, web_bundle_url,
      get_url_for_bundle.Run(url_origin.Resolve("/top-page/")), "Ready");

  RunScriptAndObserveNavigation(
      "Navigate to /iframe-test-page/", web_contents,
      web_contents /* execution_target */,
      "location.href = '/iframe-test-page/';",
      {NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, NAVIGATION_TYPE_AUTO_SUBFRAME},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/iframe-test-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/iframe-test-page/") /* expected_last_inner_url */,
      "/iframe-test-page/ from wbn, /iframe-test-page/script from wbn");
  EXPECT_EQ("/1-page/ from wbn, /1-page/script from wbn",
            GetLoadResultForNavigationTest(GetFirstChild(web_contents)));

  // The web bundle doesn't contain /server-page/. So the server returns the
  // page and script.
  RunScriptAndObserveNavigation(
      "Navigate the iframe to /server-page/", web_contents,
      web_contents /* execution_target */,
      "document.querySelector('iframe').src = /server-page/;",
      {NAVIGATION_TYPE_NEW_SUBFRAME},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/iframe-test-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/iframe-test-page/") /* expected_last_inner_url */,
      "/iframe-test-page/ from wbn, /iframe-test-page/script from wbn");
  EXPECT_EQ("/server-page/ from server, /server-page/script from server",
            GetLoadResultForNavigationTest(GetFirstChild(web_contents)));

  FrameTreeNode* iframe_node = GetFirstChild(web_contents);
  bool no_proxy_to_parent =
      iframe_node->render_manager()->GetProxyToParent() == nullptr;

  RunScriptAndObserveNavigation(
      "Navigate the iframe to /1-page/", web_contents,
      web_contents /* execution_target */,
      "document.querySelector('iframe').src = /1-page/;",
      {NAVIGATION_TYPE_NEW_SUBFRAME},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/iframe-test-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/iframe-test-page/") /* expected_last_inner_url */,
      "/iframe-test-page/ from wbn, /iframe-test-page/script from wbn");

  // TODO(crbug.com/1040800): Currently the remote iframe can't load the page
  // from web bundle. To support this case we need to change
  // NavigationControllerImpl::NavigateFromFrameProxy() to correctly handle
  // the WebBundleHandleTracker.
  EXPECT_EQ(no_proxy_to_parent
                ? "/1-page/ from wbn, /1-page/script from wbn"
                : "/1-page/ from server, /1-page/script from server",
            GetLoadResultForNavigationTest(GetFirstChild(web_contents)));
}

void RunIframeSameDocumentNavigationTest(
    WebContents* web_contents,
    const GURL& web_bundle_url,
    const GURL& url_origin,
    base::RepeatingCallback<GURL(const GURL&)> get_url_for_bundle) {
  // The test assumes the previous page gets deleted after navigation and doing
  // back navigation will recreate the page. Disable back/forward cache to
  // ensure that it doesn't get preserved in the cache.
  DisableBackForwardCacheForTesting(web_contents,
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);
  NavigateAndWaitForTitle(
      web_contents, web_bundle_url,
      get_url_for_bundle.Run(url_origin.Resolve("/top-page/")), "Ready");
  NavigateAndWaitForTitle(
      web_contents, web_bundle_url,
      get_url_for_bundle.Run(url_origin.Resolve("/top-page/")), "Ready");

  RunScriptAndObserveNavigation(
      "Navigate to /iframe-test-page/", web_contents,
      web_contents /* execution_target */,
      "location.href = '/iframe-test-page/';",
      {NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, NAVIGATION_TYPE_AUTO_SUBFRAME},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/iframe-test-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/iframe-test-page/") /* expected_last_inner_url */,
      "/iframe-test-page/ from wbn, /iframe-test-page/script from wbn");
  EXPECT_EQ("/1-page/ from wbn, /1-page/script from wbn",
            GetLoadResultForNavigationTest(GetFirstChild(web_contents)));

  RunScriptAndObserveNavigation(
      "Navigate the iframe to /1-page/#hash1", web_contents,
      GetFirstChild(web_contents) /* execution_target */,
      "location.href = '#hash1';", {NAVIGATION_TYPE_NEW_SUBFRAME},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/iframe-test-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/iframe-test-page/") /* expected_last_inner_url */,
      "/iframe-test-page/ from wbn, /iframe-test-page/script from wbn");
  EXPECT_EQ("/1-page/ from wbn, /1-page/script from wbn",
            GetLoadResultForNavigationTest(GetFirstChild(web_contents)));
  EXPECT_EQ(ExecuteAndGetString(GetFirstChild(web_contents), "location.href"),
            url_origin.Resolve("/1-page/#hash1"));

  RunScriptAndObserveNavigation(
      "Navigate the iframe to /1-page/#hash2", web_contents,
      GetFirstChild(web_contents) /* execution_target */,
      "location.href = '#hash2';", {NAVIGATION_TYPE_NEW_SUBFRAME},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/iframe-test-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/iframe-test-page/") /* expected_last_inner_url */,
      "/iframe-test-page/ from wbn, /iframe-test-page/script from wbn");
  EXPECT_EQ("/1-page/ from wbn, /1-page/script from wbn",
            GetLoadResultForNavigationTest(GetFirstChild(web_contents)));
  EXPECT_EQ(ExecuteAndGetString(GetFirstChild(web_contents), "location.href"),
            url_origin.Resolve("/1-page/#hash2"));

  RunScriptAndObserveNavigation(
      "Navigate the iframe to /2-page/", web_contents,
      GetFirstChild(web_contents) /* execution_target */,
      "location.href = '/2-page/';", {NAVIGATION_TYPE_NEW_SUBFRAME},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/iframe-test-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/iframe-test-page/") /* expected_last_inner_url */,
      "/iframe-test-page/ from wbn, /iframe-test-page/script from wbn");
  EXPECT_EQ("/2-page/ from wbn, /2-page/script from wbn",
            GetLoadResultForNavigationTest(GetFirstChild(web_contents)));
  EXPECT_EQ(ExecuteAndGetString(GetFirstChild(web_contents), "location.href"),
            url_origin.Resolve("/2-page/"));

  RunScriptAndObserveNavigation(
      "Back navigate the iframe to /1-page/#hash2", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_AUTO_SUBFRAME},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/iframe-test-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/iframe-test-page/") /* expected_last_inner_url */,
      "/iframe-test-page/ from wbn, /iframe-test-page/script from wbn");
  EXPECT_EQ("/1-page/ from wbn, /1-page/script from wbn",
            GetLoadResultForNavigationTest(GetFirstChild(web_contents)));
  EXPECT_EQ(ExecuteAndGetString(GetFirstChild(web_contents), "location.href"),
            url_origin.Resolve("/1-page/#hash2"));

  RunScriptAndObserveNavigation(
      "Back navigate the iframe to /1-page/#hash1", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_AUTO_SUBFRAME},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/iframe-test-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/iframe-test-page/") /* expected_last_inner_url */,
      "/iframe-test-page/ from wbn, /iframe-test-page/script from wbn");
  EXPECT_EQ("/1-page/ from wbn, /1-page/script from wbn",
            GetLoadResultForNavigationTest(GetFirstChild(web_contents)));
  EXPECT_EQ(ExecuteAndGetString(GetFirstChild(web_contents), "location.href"),
            url_origin.Resolve("/1-page/#hash1"));

  RunScriptAndObserveNavigation(
      "Back navigate the iframe to /1-page/", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_AUTO_SUBFRAME},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/iframe-test-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/iframe-test-page/") /* expected_last_inner_url */,
      "/iframe-test-page/ from wbn, /iframe-test-page/script from wbn");
  EXPECT_EQ("/1-page/ from wbn, /1-page/script from wbn",
            GetLoadResultForNavigationTest(GetFirstChild(web_contents)));
  EXPECT_EQ(ExecuteAndGetString(GetFirstChild(web_contents), "location.href"),
            url_origin.Resolve("/1-page/"));

  RunScriptAndObserveNavigation(
      "Back navigate to /top-page/", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY},
      get_url_for_bundle.Run(
          url_origin.Resolve("/top-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/top-page/") /* expected_last_inner_url */,
      "/top-page/ from wbn, /top-page/script from wbn");

  RunScriptAndObserveNavigation(
      "Forward navigate to /iframe-test-page/", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
       NAVIGATION_TYPE_AUTO_SUBFRAME},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/iframe-test-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/iframe-test-page/") /* expected_last_inner_url */,
      "/iframe-test-page/ from wbn, /iframe-test-page/script from wbn");
  EXPECT_EQ("/1-page/ from wbn, /1-page/script from wbn",
            GetLoadResultForNavigationTest(GetFirstChild(web_contents)));
  EXPECT_EQ(ExecuteAndGetString(GetFirstChild(web_contents), "location.href"),
            url_origin.Resolve("/1-page/"));

  RunScriptAndObserveNavigation(
      "Forward navigate the iframe to /1-page/#hash1", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_AUTO_SUBFRAME},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/iframe-test-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/iframe-test-page/") /* expected_last_inner_url */,
      "/iframe-test-page/ from wbn, /iframe-test-page/script from wbn");
  EXPECT_EQ("/1-page/ from wbn, /1-page/script from wbn",
            GetLoadResultForNavigationTest(GetFirstChild(web_contents)));
  EXPECT_EQ(ExecuteAndGetString(GetFirstChild(web_contents), "location.href"),
            url_origin.Resolve("/1-page/#hash1"));

  RunScriptAndObserveNavigation(
      "Forward navigate the iframe to /1-page/#hash2", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_AUTO_SUBFRAME},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/iframe-test-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/iframe-test-page/") /* expected_last_inner_url */,
      "/iframe-test-page/ from wbn, /iframe-test-page/script from wbn");
  EXPECT_EQ("/1-page/ from wbn, /1-page/script from wbn",
            GetLoadResultForNavigationTest(GetFirstChild(web_contents)));
  EXPECT_EQ(ExecuteAndGetString(GetFirstChild(web_contents), "location.href"),
            url_origin.Resolve("/1-page/#hash2"));

  RunScriptAndObserveNavigation(
      "Forward navigate the iframe to /2-page/", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_AUTO_SUBFRAME},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/iframe-test-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/iframe-test-page/") /* expected_last_inner_url */,
      "/iframe-test-page/ from wbn, /iframe-test-page/script from wbn");
  EXPECT_EQ("/2-page/ from wbn, /2-page/script from wbn",
            GetLoadResultForNavigationTest(GetFirstChild(web_contents)));
}

}  // namespace web_bundle_browsertest_utils
}  // namespace content
