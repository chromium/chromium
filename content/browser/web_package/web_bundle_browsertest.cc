// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/web_package/test_support/web_bundle_builder.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_package/web_bundle_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_type.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"

#if defined(OS_ANDROID)
#include "base/android/content_uri_utils.h"
#endif  // OS_ANDROID

namespace content {
namespace {

// "%2F" is treated as an invalid character for file URLs.
constexpr char kInvalidFileUrl[] = "file:///tmp/test%2F/a.wbn";

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

base::FilePath GetTestDataPath(base::StringPiece file) {
  base::FilePath test_data_dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
  return test_data_dir
      .Append(base::FilePath(FILE_PATH_LITERAL("content/test/data/web_bundle")))
      .AppendASCII(file);
}

#if defined(OS_ANDROID)
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
#endif  // OS_ANDROID

std::string ExecuteAndGetString(const ToRenderFrameHost& adapter,
                                const std::string& script) {
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      adapter, "domAutomationController.send(" + script + ")", &result));
  return result;
}

void NavigateAndWaitForTitle(content::WebContents* web_contents,
                             const GURL& test_data_url,
                             const GURL& expected_commit_url,
                             base::StringPiece ascii_title) {
  base::string16 expected_title = base::ASCIIToUTF16(ascii_title);
  TitleWatcher title_watcher(web_contents, expected_title);
  EXPECT_TRUE(NavigateToURL(web_contents, test_data_url, expected_commit_url));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

class DownloadObserver : public DownloadManager::Observer {
 public:
  explicit DownloadObserver(DownloadManager* manager) : manager_(manager) {
    manager_->AddObserver(this);
  }
  ~DownloadObserver() override { manager_->RemoveObserver(this); }

  void WaitUntilDownloadCreated() { run_loop_.Run(); }
  const GURL& observed_url() const { return url_; }

  // content::DownloadManager::Observer implementation.
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override {
    url_ = item->GetURL();
    run_loop_.Quit();
  }

 private:
  DownloadManager* manager_;
  base::RunLoop run_loop_;
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(DownloadObserver);
};

class MockParserFactory;

class MockParser final : public web_package::mojom::WebBundleParser {
 public:
  using Index = base::flat_map<GURL, web_package::mojom::BundleIndexValuePtr>;

  MockParser(
      MockParserFactory* factory,
      mojo::PendingReceiver<web_package::mojom::WebBundleParser> receiver,
      const Index& index,
      const GURL& primary_url,
      bool simulate_parse_metadata_crash,
      bool simulate_parse_response_crash)
      : factory_(factory),
        receiver_(this, std::move(receiver)),
        index_(index),
        primary_url_(primary_url),
        simulate_parse_metadata_crash_(simulate_parse_metadata_crash),
        simulate_parse_response_crash_(simulate_parse_response_crash) {}
  ~MockParser() override = default;

 private:
  // web_package::mojom::WebBundleParser implementation.
  void ParseMetadata(ParseMetadataCallback callback) override;
  void ParseResponse(uint64_t response_offset,
                     uint64_t response_length,
                     ParseResponseCallback callback) override;

  MockParserFactory* factory_;
  mojo::Receiver<web_package::mojom::WebBundleParser> receiver_;
  const Index& index_;
  const GURL primary_url_;
  const bool simulate_parse_metadata_crash_;
  const bool simulate_parse_response_crash_;

  DISALLOW_COPY_AND_ASSIGN(MockParser);
};

class MockParserFactory final
    : public web_package::mojom::WebBundleParserFactory {
 public:
  MockParserFactory(std::vector<GURL> urls,
                    const base::FilePath& response_body_file)
      : primary_url_(urls[0]) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    int64_t response_body_file_size;
    EXPECT_TRUE(
        base::GetFileSize(response_body_file, &response_body_file_size));
    for (const auto& url : urls) {
      web_package::mojom::BundleIndexValuePtr item =
          web_package::mojom::BundleIndexValue::New();
      item->response_locations.push_back(
          web_package::mojom::BundleResponseLocation::New(
              0u, response_body_file_size));
      index_.insert({url, std::move(item)});
    }
    in_process_data_decoder_.service()
        .SetWebBundleParserFactoryBinderForTesting(
            base::BindRepeating(&MockParserFactory::BindWebBundleParserFactory,
                                base::Unretained(this)));
  }
  MockParserFactory(
      const std::vector<std::pair<GURL, const std::string&>> items)
      : primary_url_(items[0].first) {
    uint64_t offset = 0;
    for (const auto& item : items) {
      web_package::mojom::BundleIndexValuePtr index_value =
          web_package::mojom::BundleIndexValue::New();
      index_value->response_locations.push_back(
          web_package::mojom::BundleResponseLocation::New(
              offset, item.second.length()));
      offset += item.second.length();
      index_.insert({item.first, std::move(index_value)});
    }
    in_process_data_decoder_.service()
        .SetWebBundleParserFactoryBinderForTesting(
            base::BindRepeating(&MockParserFactory::BindWebBundleParserFactory,
                                base::Unretained(this)));
  }
  ~MockParserFactory() override = default;

  int GetParserCreationCount() const { return parser_creation_count_; }
  void SimulateParserDisconnect() { parser_ = nullptr; }
  void SimulateParseMetadataCrash() { simulate_parse_metadata_crash_ = true; }
  void SimulateParseResponseCrash() { simulate_parse_response_crash_ = true; }

 private:
  void BindWebBundleParserFactory(
      mojo::PendingReceiver<web_package::mojom::WebBundleParserFactory>
          receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  // web_package::mojom::WebBundleParserFactory implementation.
  void GetParserForFile(
      mojo::PendingReceiver<web_package::mojom::WebBundleParser> receiver,
      base::File file) override {
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      file.Close();
    }
    DCHECK(!parser_);
    parser_ = std::make_unique<MockParser>(
        this, std::move(receiver), index_, primary_url_,
        simulate_parse_metadata_crash_, simulate_parse_response_crash_);
    parser_creation_count_++;
  }

  void GetParserForDataSource(
      mojo::PendingReceiver<web_package::mojom::WebBundleParser> receiver,
      mojo::PendingRemote<web_package::mojom::BundleDataSource> data_source)
      override {
    DCHECK(!parser_);
    parser_ = std::make_unique<MockParser>(
        this, std::move(receiver), index_, primary_url_,
        simulate_parse_metadata_crash_, simulate_parse_response_crash_);
    parser_creation_count_++;
  }

  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  mojo::ReceiverSet<web_package::mojom::WebBundleParserFactory> receivers_;
  bool simulate_parse_metadata_crash_ = false;
  bool simulate_parse_response_crash_ = false;
  std::unique_ptr<MockParser> parser_;
  int parser_creation_count_ = 0;
  base::flat_map<GURL, web_package::mojom::BundleIndexValuePtr> index_;
  const GURL primary_url_;

  DISALLOW_COPY_AND_ASSIGN(MockParserFactory);
};

void MockParser::ParseMetadata(ParseMetadataCallback callback) {
  if (simulate_parse_metadata_crash_) {
    factory_->SimulateParserDisconnect();
    return;
  }

  base::flat_map<GURL, web_package::mojom::BundleIndexValuePtr> items;
  for (const auto& item : index_) {
    items.insert({item.first, item.second.Clone()});
  }

  web_package::mojom::BundleMetadataPtr metadata =
      web_package::mojom::BundleMetadata::New();
  metadata->primary_url = primary_url_;
  metadata->requests = std::move(items);

  std::move(callback).Run(std::move(metadata), nullptr);
}

void MockParser::ParseResponse(uint64_t response_offset,
                               uint64_t response_length,
                               ParseResponseCallback callback) {
  if (simulate_parse_response_crash_) {
    factory_->SimulateParserDisconnect();
    return;
  }
  web_package::mojom::BundleResponsePtr response =
      web_package::mojom::BundleResponse::New();
  response->response_code = 200;
  response->response_headers.insert({"content-type", "text/html"});
  response->payload_offset = response_offset;
  response->payload_length = response_length;
  std::move(callback).Run(std::move(response), nullptr);
}

class TestBrowserClient : public ContentBrowserClient {
 public:
  TestBrowserClient() = default;
  ~TestBrowserClient() override = default;
  bool CanAcceptUntrustedExchangesIfNeeded() override { return true; }
  std::string GetAcceptLangs(BrowserContext* context) override {
    return accept_langs_;
  }
  void SetAcceptLangs(const std::string langs) { accept_langs_ = langs; }

 private:
  std::string accept_langs_ = "en";
  DISALLOW_COPY_AND_ASSIGN(TestBrowserClient);
};

class WebBundleBrowserTestBase : public ContentBrowserTest {
 protected:
  WebBundleBrowserTestBase() = default;
  ~WebBundleBrowserTestBase() override = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    original_client_ = SetBrowserClientForTesting(&browser_client_);
  }

  void TearDownOnMainThread() override {
    ContentBrowserTest::TearDownOnMainThread();
    SetBrowserClientForTesting(original_client_);
  }

  void SetAcceptLangs(const std::string langs) {
    browser_client_.SetAcceptLangs(langs);
  }

  void NavigateToBundleAndWaitForReady(const GURL& test_data_url,
                                       const GURL& expected_commit_url) {
    NavigateAndWaitForTitle(shell()->web_contents(), test_data_url,
                            expected_commit_url, "Ready");
  }

  void RunTestScript(const std::string& script) {
    EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                              "loadScript('" + script + "');"));
    base::string16 ok = base::ASCIIToUTF16("OK");
    TitleWatcher title_watcher(shell()->web_contents(), ok);
    title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16("FAIL"));
    EXPECT_EQ(ok, title_watcher.WaitAndGetTitle());
  }

  void ExecuteScriptAndWaitForTitle(const std::string& script,
                                    const std::string& title) {
    base::string16 title16 = base::ASCIIToUTF16(title);
    TitleWatcher title_watcher(shell()->web_contents(), title16);
    EXPECT_TRUE(ExecuteScript(shell()->web_contents(), script));
    EXPECT_EQ(title16, title_watcher.WaitAndGetTitle());
  }

  void NavigateToURLAndWaitForTitle(const GURL& url, const std::string& title) {
    ExecuteScriptAndWaitForTitle(
        base::StringPrintf("location.href = '%s';", url.spec().c_str()), title);
  }

  void CreateTemporaryWebBundleFile(const std::string& content,
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

 private:
  ContentBrowserClient* original_client_ = nullptr;
  TestBrowserClient browser_client_;
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(WebBundleBrowserTestBase);
};

class FinishNavigationObserver : public WebContentsObserver {
 public:
  explicit FinishNavigationObserver(WebContents* contents,
                                    base::OnceClosure done_closure)
      : WebContentsObserver(contents), done_closure_(std::move(done_closure)) {}

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    navigation_types_.push_back(
        NavigationRequest::From(navigation_handle)->navigation_type());
    error_code_ = navigation_handle->GetNetErrorCode();
    --navigations_remaining_;

    if (navigations_remaining_ == 0)
      std::move(done_closure_).Run();
  }

  void set_navigations_remaining(int navigations_remaining) {
    navigations_remaining_ = navigations_remaining;
  }

  const base::Optional<net::Error>& error_code() const { return error_code_; }
  const std::vector<NavigationType>& navigation_types() const {
    return navigation_types_;
  }

 private:
  base::OnceClosure done_closure_;
  base::Optional<net::Error> error_code_;

  int navigations_remaining_ = 1;
  std::vector<NavigationType> navigation_types_;

  DISALLOW_COPY_AND_ASSIGN(FinishNavigationObserver);
};

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
    console_observer.Wait();

  if (console_observer.messages().empty()) {
    ADD_FAILURE() << "Could not find a console message.";
    return std::string();
  }
  return base::UTF16ToUTF8(console_observer.messages()[0].message);
}

FrameTreeNode* GetFirstChild(WebContents* web_contents) {
  return static_cast<WebContentsImpl*>(web_contents)
      ->GetFrameTree()
      ->root()
      ->child_at(0);
}

std::string CreateSimpleWebBundle(const GURL& primary_url) {
  web_package::test::WebBundleBuilder builder(primary_url.spec(), "");
  builder.AddExchange(primary_url.spec(),
                      {{":status", "200"}, {"content-type", "text/html"}},
                      "<title>Ready</title>");
  std::vector<uint8_t> bundle = builder.CreateBundle();
  return std::string(bundle.begin(), bundle.end());
}

void AddHtmlFile(web_package::test::WebBundleBuilder* builder,
                 const GURL& base_url,
                 const std::string& path,
                 const std::string& content) {
  builder->AddExchange(base_url.Resolve(path).spec(),
                       {{":status", "200"}, {"content-type", "text/html"}},
                       content);
}

void AddScriptFile(web_package::test::WebBundleBuilder* builder,
                   const GURL& base_url,
                   const std::string& path,
                   const std::string& content) {
  builder->AddExchange(
      base_url.Resolve(path).spec(),
      {{":status", "200"}, {"content-type", "application/javascript"}},
      content);
}

std::string CreatePathTestWebBundle(const GURL& base_url) {
  const std::string primary_url_path = "/web_bundle/path_test/in_scope/";
  web_package::test::WebBundleBuilder builder(
      base_url.Resolve(primary_url_path).spec(), "");
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
                      std::string* web_bundle_content) {
  RegisterRequestHandlerForSubPageTest(primary_server, "");
  RegisterRequestHandlerForSubPageTest(third_party_server, "third-party:");

  ASSERT_TRUE(primary_server->Start());
  ASSERT_TRUE(third_party_server->Start());
  *primary_url_origin = primary_server->GetURL("/");
  *third_party_origin = third_party_server->GetURL("/");

  web_package::test::WebBundleBuilder builder(
      primary_url_origin->Resolve("/top").spec(), "");
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
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(adapter,
                                                     base::StringPrintf(
                                                         R"(
  (function(){
    const iframe = document.createElement('iframe');
    iframe.src = '%s';
    document.body.appendChild(iframe);
  })();
  )",
                                                         url.spec().c_str()),
                                                     &result));
  return result;
}

std::string WindowOpenAndWaitForMessage(const ToRenderFrameHost& adapter,
                                        const GURL& url) {
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      adapter,
      base::StringPrintf(R"(
        if (document.last_win) {
          // Close the latest window to avoid OOM-killer on Android.
          document.last_win.close();
        }
        document.last_win = window.open('%s', '_blank');
      )",
                         url.spec().c_str()),
      &result));
  return result;
}

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

void AddHtmlAndScriptForNavigationTest(
    web_package::test::WebBundleBuilder* builder,
    const GURL& base_url,
    const std::string& path,
    const std::string& additional_html) {
  AddHtmlFile(builder, base_url, path,
              CreateHtmlForNavigationTest(path + " from wbn", additional_html));
  AddScriptFile(builder, base_url, path + "script",
                CreateScriptForNavigationTest(path + "script from wbn"));
}

std::string GetLoadResultForNavigationTest(const ToRenderFrameHost& adapter) {
  std::string result;
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
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(adapter, script, &result));
  return result;
}

// Sets up |server| to return server generated page HTML files and JavaScript
// files. |server| will returns a page file created by
// CreateHtmlForNavigationTest(relative_url + " from server") for all URL which
// ends with "page/", and returns a script file created by
// CreateScriptForNavigationTest(relative_url + " from server") for all URL
// which ends with "script".
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
  web_package::test::WebBundleBuilder builder(
      url_origin->Resolve("/top-page/").spec(), "");
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
      "location.href = '/1-page/';", {NAVIGATION_TYPE_NEW_PAGE},
      get_url_for_bundle.Run(
          url_origin.Resolve("/1-page/")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/1-page/") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Navigate to /2-page/", web_contents, web_contents /* execution_target */,
      "location.href = '/2-page/';", {NAVIGATION_TYPE_NEW_PAGE},
      get_url_for_bundle.Run(
          url_origin.Resolve("/2-page/")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/2-page/") /* expected_last_inner_url */,
      "/2-page/ from wbn, /2-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Back navigate to /1-page/", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_EXISTING_PAGE},
      get_url_for_bundle.Run(
          url_origin.Resolve("/1-page/")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/1-page/") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Back navigate to /top-page/", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_EXISTING_PAGE},
      get_url_for_bundle.Run(
          url_origin.Resolve("/top-page/")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/top-page/") /* expected_last_inner_url */,
      "/top-page/ from wbn, /top-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Forward navigate to /1-page/", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_EXISTING_PAGE},
      get_url_for_bundle.Run(
          url_origin.Resolve("/1-page/")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/1-page/") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Reload /1-page/", web_contents, web_contents /* execution_target */,
      "location.reload();", {NAVIGATION_TYPE_EXISTING_PAGE},
      get_url_for_bundle.Run(
          url_origin.Resolve("/1-page/")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/1-page/") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Forward navigate to /2-page/", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_EXISTING_PAGE},
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

// Runs test for history navigations after browser initiated navigation going
// out of the web bundle.
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
      "location.href = '/1-page/';", {NAVIGATION_TYPE_NEW_PAGE},
      get_url_for_bundle.Run(
          url_origin.Resolve("/1-page/")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/1-page/") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Navigate to /2-page/", web_contents, web_contents /* execution_target */,
      "location.href = '/2-page/';", {NAVIGATION_TYPE_NEW_PAGE},
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
      "location.href = '/4-page/';", {NAVIGATION_TYPE_NEW_PAGE},
      url_origin.Resolve("/4-page/") /* expected_last_comitted_url */,
      url_origin.Resolve("/4-page/") /* expected_last_inner_url */,
      "/4-page/ from server, /4-page/script from server");
  RunScriptAndObserveNavigation(
      "Back navigate to /3-page/", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_EXISTING_PAGE},
      url_origin.Resolve("/3-page/") /* expected_last_comitted_url */,
      url_origin.Resolve("/3-page/") /* expected_last_inner_url */,
      "/3-page/ from server, /3-page/script from server");
  RunScriptAndObserveNavigation(
      "Back navigate to /2-page/", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_EXISTING_PAGE},
      get_url_for_bundle.Run(
          url_origin.Resolve("/2-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/2-page/") /* expected_last_inner_url */,
      "/2-page/ from wbn, /2-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Back navigate to /1-page/", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_EXISTING_PAGE},
      get_url_for_bundle.Run(
          url_origin.Resolve("/1-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/1-page/") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Forward navigate to /2-page/", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_EXISTING_PAGE},
      get_url_for_bundle.Run(
          url_origin.Resolve("/2-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/2-page/") /* expected_last_inner_url */,
      "/2-page/ from wbn, /2-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Forward navigate to /3-page/", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_EXISTING_PAGE},
      url_origin.Resolve("/3-page/") /* expected_last_comitted_url */,
      url_origin.Resolve("/3-page/") /* expected_last_inner_url */,
      "/3-page/ from server, /3-page/script from server");
  RunScriptAndObserveNavigation(
      "Forward navigate to /4-page/", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_EXISTING_PAGE},
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

// Runs test for history navigations after renderer initiated navigation going
// out of the web bundle.
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
      "location.href = '/1-page/';", {NAVIGATION_TYPE_NEW_PAGE},
      get_url_for_bundle.Run(
          url_origin.Resolve("/1-page/")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/1-page/") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Navigate to /2-page/", web_contents, web_contents /* execution_target */,
      "location.href = '/2-page/';", {NAVIGATION_TYPE_NEW_PAGE},
      get_url_for_bundle.Run(
          url_origin.Resolve("/2-page/")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/2-page/") /* expected_last_inner_url */,
      "/2-page/ from wbn, /2-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Navigate to /server-page/", web_contents,
      web_contents /* execution_target */, "location.href = '/server-page/';",
      {NAVIGATION_TYPE_NEW_PAGE},
      url_origin.Resolve("/server-page/") /* expected_last_comitted_url */,
      url_origin.Resolve("/server-page/") /* expected_last_inner_url */,
      "/server-page/ from server, /server-page/script from server");
  // Navigation from the out of web bundle page must be loaded from the server
  // even if the page is in the web bundle.
  RunScriptAndObserveNavigation(
      "Navigate to /3-page/", web_contents, web_contents /* execution_target */,
      "location.href = '/3-page/';", {NAVIGATION_TYPE_NEW_PAGE},
      url_origin.Resolve("/3-page/") /* expected_last_comitted_url */,
      url_origin.Resolve("/3-page/") /* expected_last_inner_url */,
      "/3-page/ from server, /3-page/script from server");
  RunScriptAndObserveNavigation(
      "Back navigate to /server-page/", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_EXISTING_PAGE},
      url_origin.Resolve("/server-page/") /* expected_last_comitted_url */,
      url_origin.Resolve("/server-page/") /* expected_last_inner_url */,
      "/server-page/ from server, /server-page/script from server");
  RunScriptAndObserveNavigation(
      "Back navigate to /2-page/", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_EXISTING_PAGE},
      get_url_for_bundle.Run(
          url_origin.Resolve("/2-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/2-page/") /* expected_last_inner_url */,
      "/2-page/ from wbn, /2-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Back navigate to /1-page/", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_EXISTING_PAGE},
      get_url_for_bundle.Run(
          url_origin.Resolve("/1-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/1-page/") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Forward navigate to /2-page/", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_EXISTING_PAGE},
      get_url_for_bundle.Run(
          url_origin.Resolve("/2-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/2-page/") /* expected_last_inner_url */,
      "/2-page/ from wbn, /2-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Forward navigate to /server-page/", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_EXISTING_PAGE},
      url_origin.Resolve("/server-page/") /* expected_last_comitted_url */,
      url_origin.Resolve("/server-page/") /* expected_last_inner_url */,
      "/server-page/ from server, /server-page/script from server");
  RunScriptAndObserveNavigation(
      "Forward navigate to /3-page/", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_EXISTING_PAGE},
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

// Runs test for history navigations after same document navigations.
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
      "location.href = '/1-page/';", {NAVIGATION_TYPE_NEW_PAGE},
      get_url_for_bundle.Run(
          url_origin.Resolve("/1-page/")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/1-page/") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Navigate to /1-page/#hash1", web_contents,
      web_contents /* execution_target */, "location.href = '#hash1';",
      {NAVIGATION_TYPE_NEW_PAGE},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/1-page/#hash1")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/1-page/#hash1") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Navigate to /1-page/#hash2", web_contents,
      web_contents /* execution_target */, "location.href = '#hash2';",
      {NAVIGATION_TYPE_NEW_PAGE},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/1-page/#hash2")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/1-page/#hash2") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Navigate to /2-page/", web_contents, web_contents /* execution_target */,
      "location.href = '/2-page/';", {NAVIGATION_TYPE_NEW_PAGE},
      get_url_for_bundle.Run(
          url_origin.Resolve("/2-page/")) /* expected_last_comitted_url  */,
      url_origin.Resolve("/2-page/") /* expected_last_inner_url */,
      "/2-page/ from wbn, /2-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Back navigate to /1-page/#hash2", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_EXISTING_PAGE},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/1-page/#hash2")) /* expected_last_comitted_url */,
      url_origin.Resolve("/1-page/#hash2") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Back navigate to /1-page/#hash1", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_EXISTING_PAGE},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/1-page/#hash1")) /* expected_last_comitted_url */,
      url_origin.Resolve("/1-page/#hash1") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Back navigate to /1-page/", web_contents,
      web_contents /* execution_target */, "history.back();",
      {NAVIGATION_TYPE_EXISTING_PAGE},
      get_url_for_bundle.Run(
          url_origin.Resolve("/1-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/1-page/") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Forward navigate to /1-page/#hash1", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_EXISTING_PAGE},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/1-page/#hash1")) /* expected_last_comitted_url */,
      url_origin.Resolve("/1-page/#hash1") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Forward navigate to /1-page/#hash2", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_EXISTING_PAGE},
      get_url_for_bundle.Run(url_origin.Resolve(
          "/1-page/#hash2")) /* expected_last_comitted_url */,
      url_origin.Resolve("/1-page/#hash2") /* expected_last_inner_url */,
      "/1-page/ from wbn, /1-page/script from wbn");
  RunScriptAndObserveNavigation(
      "Forward navigate to /2-page/", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_EXISTING_PAGE},
      get_url_for_bundle.Run(
          url_origin.Resolve("/2-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/2-page/") /* expected_last_inner_url */,
      "/2-page/ from wbn, /2-page/script from wbn");
}

void SetUpIframeNavigationTest(net::EmbeddedTestServer* server,
                               GURL* url_origin,
                               std::string* web_bundle_content) {
  SetUpNavigationTestServer(server, url_origin);
  web_package::test::WebBundleBuilder builder(
      url_origin->Resolve("/top-page/").spec(), "");
  const std::vector<std::string> pathes = {"/top-page/", "/1-page/",
                                           "/2-page/"};
  for (const auto& path : pathes)
    AddHtmlAndScriptForNavigationTest(&builder, *url_origin, path, "");
  AddHtmlAndScriptForNavigationTest(&builder, *url_origin, "/iframe-test-page/",
                                    "<iframe src=\"/1-page/\" />");

  std::vector<uint8_t> bundle = builder.CreateBundle();
  *web_bundle_content = std::string(bundle.begin(), bundle.end());
}

// Runs test for history navigations with an iframe.
void RunIframeNavigationTest(
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
      {NAVIGATION_TYPE_NEW_PAGE, NAVIGATION_TYPE_AUTO_SUBFRAME},
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
      {NAVIGATION_TYPE_EXISTING_PAGE},
      get_url_for_bundle.Run(
          url_origin.Resolve("/top-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/top-page/") /* expected_last_inner_url */,
      "/top-page/ from wbn, /top-page/script from wbn");

  RunScriptAndObserveNavigation(
      "Forward navigate to /iframe-test-page/", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_EXISTING_PAGE, NAVIGATION_TYPE_AUTO_SUBFRAME},
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

// Runs test for navigations in an iframe after going out of the web bundle by
// changing location.href inside the iframe.
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
      {NAVIGATION_TYPE_NEW_PAGE, NAVIGATION_TYPE_AUTO_SUBFRAME},
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
      {NAVIGATION_TYPE_NEW_PAGE, NAVIGATION_TYPE_AUTO_SUBFRAME},
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
  bool is_same_process = (iframe_node->parent()->GetProcess() ==
                          iframe_node->current_frame_host()->GetProcess());

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
  EXPECT_EQ(is_same_process
                ? "/1-page/ from wbn, /1-page/script from wbn"
                : "/1-page/ from server, /1-page/script from server",
            GetLoadResultForNavigationTest(GetFirstChild(web_contents)));
}

// Runs test for history navigations in an iframe after same document
// navigation.
void RunIframeSameDocumentNavigationTest(
    WebContents* web_contents,
    const GURL& web_bundle_url,
    const GURL& url_origin,
    base::RepeatingCallback<GURL(const GURL&)> get_url_for_bundle) {
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
      {NAVIGATION_TYPE_NEW_PAGE, NAVIGATION_TYPE_AUTO_SUBFRAME},
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
      {NAVIGATION_TYPE_EXISTING_PAGE},
      get_url_for_bundle.Run(
          url_origin.Resolve("/top-page/")) /* expected_last_comitted_url */,
      url_origin.Resolve("/top-page/") /* expected_last_inner_url */,
      "/top-page/ from wbn, /top-page/script from wbn");

  RunScriptAndObserveNavigation(
      "Forward navigate to /iframe-test-page/", web_contents,
      web_contents /* execution_target */, "history.forward();",
      {NAVIGATION_TYPE_EXISTING_PAGE, NAVIGATION_TYPE_AUTO_SUBFRAME},
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

}  // namespace

class InvalidTrustableWebBundleFileUrlBrowserTest : public ContentBrowserTest {
 protected:
  InvalidTrustableWebBundleFileUrlBrowserTest() = default;
  ~InvalidTrustableWebBundleFileUrlBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    original_client_ = SetBrowserClientForTesting(&browser_client_);
  }

  void TearDownOnMainThread() override {
    ContentBrowserTest::TearDownOnMainThread();
    SetBrowserClientForTesting(original_client_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kTrustableWebBundleFileUrl,
                                    kInvalidFileUrl);
  }

 private:
  ContentBrowserClient* original_client_ = nullptr;
  TestBrowserClient browser_client_;

  DISALLOW_COPY_AND_ASSIGN(InvalidTrustableWebBundleFileUrlBrowserTest);
};

IN_PROC_BROWSER_TEST_F(InvalidTrustableWebBundleFileUrlBrowserTest,
                       NoCrashOnNavigation) {
  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(shell()->web_contents(),
                                                      run_loop.QuitClosure());
  EXPECT_FALSE(NavigateToURL(shell()->web_contents(), GURL(kInvalidFileUrl)));
  run_loop.Run();
  ASSERT_TRUE(finish_navigation_observer.error_code());
  EXPECT_EQ(net::ERR_INVALID_URL, *finish_navigation_observer.error_code());
}

enum class TestFilePathMode {
  kNormalFilePath,
#if defined(OS_ANDROID)
  kContentURI,
#endif  // OS_ANDROID
};

#if defined(OS_ANDROID)
#define TEST_FILE_PATH_MODE_PARAMS                   \
  testing::Values(TestFilePathMode::kNormalFilePath, \
                  TestFilePathMode::kContentURI)
#else
#define TEST_FILE_PATH_MODE_PARAMS \
  testing::Values(TestFilePathMode::kNormalFilePath)
#endif  // OS_ANDROID

class WebBundleTrustableFileBrowserTest
    : public testing::WithParamInterface<TestFilePathMode>,
      public WebBundleBrowserTestBase {
 protected:
  WebBundleTrustableFileBrowserTest() = default;
  ~WebBundleTrustableFileBrowserTest() override = default;

  void SetUp() override {
    InitializeTestDataUrl();
    SetEmptyPageUrl();
    WebBundleBrowserTestBase::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kTrustableWebBundleFileUrl,
                                    test_data_url().spec());
  }

  void WriteWebBundleFile(const std::string& contents) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_FALSE(contents.empty());
    ASSERT_TRUE(base::WriteFile(test_data_file_path_, contents.data(),
                                contents.size()) > 0);
  }

  void WriteCommonWebBundleFile() {
    std::string contents;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::ReadFileToString(
          GetTestDataPath("web_bundle_browsertest.wbn"), &contents));
    }
    WriteWebBundleFile(contents);
  }

  const GURL& test_data_url() const { return test_data_url_; }
  const GURL& empty_page_url() const { return empty_page_url_; }

  void RunSharedNavigationTest(
      void (*setup_func)(net::EmbeddedTestServer*, GURL*, std::string*),
      void (*run_test_func)(WebContents*,
                            const GURL&,
                            const GURL&,
                            base::RepeatingCallback<GURL(const GURL&)>)) {
    GURL url_origin;
    std::string web_bundle_content;
    (*setup_func)(embedded_test_server(), &url_origin, &web_bundle_content);
    WriteWebBundleFile(web_bundle_content);

    (*run_test_func)(shell()->web_contents(), test_data_url(), url_origin,
                     base::BindRepeating([](const GURL& url) { return url; }));
  }

 private:
  void InitializeTestDataUrl() {
    base::FilePath file_path;
    CreateTemporaryWebBundleFile("", &file_path);
    if (GetParam() == TestFilePathMode::kNormalFilePath) {
      test_data_file_path_ = file_path;
      test_data_url_ = net::FilePathToFileURL(file_path);
      return;
    }
#if defined(OS_ANDROID)
    DCHECK_EQ(TestFilePathMode::kContentURI, GetParam());
    CopyFileAndGetContentUri(file_path, &test_data_url_, &test_data_file_path_);
#endif  // OS_ANDROID
  }

  void SetEmptyPageUrl() {
    if (GetParam() == TestFilePathMode::kNormalFilePath) {
      empty_page_url_ =
          net::FilePathToFileURL(GetTestDataPath("empty_page.html"));
      return;
    }
#if defined(OS_ANDROID)
    DCHECK_EQ(TestFilePathMode::kContentURI, GetParam());
    CopyFileAndGetContentUri(GetTestDataPath("empty_page.html"),
                             &empty_page_url_, nullptr /* new_file_path */);
#endif  // OS_ANDROID
  }

  GURL test_data_url_;
  base::FilePath test_data_file_path_;

  GURL empty_page_url_;

  DISALLOW_COPY_AND_ASSIGN(WebBundleTrustableFileBrowserTest);
};

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest,
                       TrustableWebBundleFile) {
  WriteCommonWebBundleFile();
  NavigateToBundleAndWaitForReady(test_data_url(), GURL(kTestPageUrl));
}

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest, RangeRequest) {
  WriteCommonWebBundleFile();
  NavigateToBundleAndWaitForReady(test_data_url(), GURL(kTestPageUrl));
  RunTestScript("test-range-request.js");
}

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest, BasicNavigation) {
  RunSharedNavigationTest(&SetUpBasicNavigationTest, &RunBasicNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest,
                       BrowserInitiatedOutOfBundleNavigation) {
  RunSharedNavigationTest(&SetUpBrowserInitiatedOutOfBundleNavigationTest,
                          &RunBrowserInitiatedOutOfBundleNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest,
                       RendererInitiatedOutOfBundleNavigation) {
  RunSharedNavigationTest(&SetUpRendererInitiatedOutOfBundleNavigationTest,
                          &RunRendererInitiatedOutOfBundleNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest,
                       SameDocumentNavigation) {
  RunSharedNavigationTest(&SetUpSameDocumentNavigationTest,
                          &RunSameDocumentNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest, IframeNavigation) {
  RunSharedNavigationTest(&SetUpIframeNavigationTest, &RunIframeNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest,
                       IframeOutOfBundleNavigation) {
  RunSharedNavigationTest(&SetUpIframeNavigationTest,
                          &RunIframeOutOfBundleNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest,
                       IframeParentInitiatedOutOfBundleNavigation) {
  RunSharedNavigationTest(&SetUpIframeNavigationTest,
                          &RunIframeParentInitiatedOutOfBundleNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest,
                       IframeSameDocumentNavigation) {
  RunSharedNavigationTest(&SetUpIframeNavigationTest,
                          &RunIframeSameDocumentNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest, BaseURI) {
  WriteCommonWebBundleFile();
  NavigateToBundleAndWaitForReady(test_data_url(), GURL(kTestPageUrl));
  EXPECT_EQ(ExecuteAndGetString(shell()->web_contents(),
                                "(new Request('./foo/bar')).url"),
            "https://test.example.org/foo/bar");
  EXPECT_EQ(ExecuteAndGetString(shell()->web_contents(), R"(
            (() => {
              const base_element = document.createElement('base');
              base_element.href = 'https://example.org/piyo/';
              document.body.appendChild(base_element);
              return document.baseURI;
            })()
            )"),
            "https://example.org/piyo/");
  EXPECT_EQ(ExecuteAndGetString(shell()->web_contents(),
                                "(new Request('./foo/bar')).url"),
            "https://example.org/piyo/foo/bar");
}

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest, Iframe) {
  net::EmbeddedTestServer third_party_server;
  GURL primary_url_origin;
  GURL third_party_origin;
  std::string web_bundle_content;
  SetUpSubPageTest(embedded_test_server(), &third_party_server,
                   &primary_url_origin, &third_party_origin,
                   &web_bundle_content);
  WriteWebBundleFile(web_bundle_content);

  NavigateToBundleAndWaitForReady(test_data_url(),
                                  primary_url_origin.Resolve("/top"));
  RunSubPageTest(shell()->web_contents(), primary_url_origin,
                 third_party_origin, &AddIframeAndWaitForMessage,
                 true /* support_third_party_wbn_page */);
}

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest, WindowOpen) {
  net::EmbeddedTestServer third_party_server;
  GURL primary_url_origin;
  GURL third_party_origin;
  std::string web_bundle_content;
  SetUpSubPageTest(embedded_test_server(), &third_party_server,
                   &primary_url_origin, &third_party_origin,
                   &web_bundle_content);
  WriteWebBundleFile(web_bundle_content);

  NavigateToBundleAndWaitForReady(test_data_url(),
                                  primary_url_origin.Resolve("/top"));
  RunSubPageTest(shell()->web_contents(), primary_url_origin,
                 third_party_origin, &WindowOpenAndWaitForMessage,
                 true /* support_third_party_wbn_page */);
}

INSTANTIATE_TEST_SUITE_P(WebBundleTrustableFileBrowserTest,
                         WebBundleTrustableFileBrowserTest,
                         TEST_FILE_PATH_MODE_PARAMS);

class WebBundleTrustableFileNotFoundBrowserTest
    : public WebBundleBrowserTestBase {
 protected:
  WebBundleTrustableFileNotFoundBrowserTest() = default;
  ~WebBundleTrustableFileNotFoundBrowserTest() override = default;
  void SetUp() override {
    test_data_url_ = net::FilePathToFileURL(GetTestDataPath("not_found"));
    WebBundleBrowserTestBase::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kTrustableWebBundleFileUrl,
                                    test_data_url().spec());
  }
  const GURL& test_data_url() const { return test_data_url_; }

 private:
  GURL test_data_url_;

  DISALLOW_COPY_AND_ASSIGN(WebBundleTrustableFileNotFoundBrowserTest);
};

IN_PROC_BROWSER_TEST_F(WebBundleTrustableFileNotFoundBrowserTest, NotFound) {
  std::string console_message = ExpectNavigationFailureAndReturnConsoleMessage(
      shell()->web_contents(), test_data_url());

  EXPECT_EQ("Failed to read metadata of Web Bundle file: FILE_ERROR_FAILED",
            console_message);
}

class WebBundleFileBrowserTest
    : public testing::WithParamInterface<TestFilePathMode>,
      public WebBundleBrowserTestBase {
 protected:
  WebBundleFileBrowserTest() = default;
  ~WebBundleFileBrowserTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures({features::kWebBundles}, {});
    WebBundleBrowserTestBase::SetUp();
  }

  GURL GetTestUrlForFile(base::FilePath file_path) const {
    GURL content_uri;
    if (GetParam() == TestFilePathMode::kNormalFilePath) {
      content_uri = net::FilePathToFileURL(file_path);
    } else {
#if defined(OS_ANDROID)
      DCHECK_EQ(TestFilePathMode::kContentURI, GetParam());
      CopyFileAndGetContentUri(file_path, &content_uri,
                               nullptr /* new_file_path */);
#endif  // OS_ANDROID
    }
    return content_uri;
  }

  void RunSharedNavigationTest(
      void (*setup_func)(net::EmbeddedTestServer*, GURL*, std::string*),
      void (*run_test_func)(WebContents*,
                            const GURL&,
                            const GURL&,
                            base::RepeatingCallback<GURL(const GURL&)>)) {
    GURL url_origin;
    std::string web_bundle_content;
    (*setup_func)(embedded_test_server(), &url_origin, &web_bundle_content);

    base::FilePath file_path;
    CreateTemporaryWebBundleFile(web_bundle_content, &file_path);
    const GURL test_data_url = GetTestUrlForFile(file_path);

    (*run_test_func)(
        shell()->web_contents(), test_data_url, url_origin,
        base::BindRepeating(&web_bundle_utils::GetSynthesizedUrlForWebBundle,
                            test_data_url));
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(WebBundleFileBrowserTest);
};

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, BasicNavigation) {
  RunSharedNavigationTest(&SetUpBasicNavigationTest, &RunBasicNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest,
                       BrowserInitiatedOutOfBundleNavigation) {
  RunSharedNavigationTest(&SetUpBrowserInitiatedOutOfBundleNavigationTest,
                          &RunBrowserInitiatedOutOfBundleNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest,
                       RendererInitiatedOutOfBundleNavigation) {
  RunSharedNavigationTest(&SetUpRendererInitiatedOutOfBundleNavigationTest,
                          &RunRendererInitiatedOutOfBundleNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, SameDocumentNavigation) {
  RunSharedNavigationTest(&SetUpSameDocumentNavigationTest,
                          &RunSameDocumentNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, IframeNavigation) {
  RunSharedNavigationTest(&SetUpIframeNavigationTest, &RunIframeNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, IframeOutOfBundleNavigation) {
  RunSharedNavigationTest(&SetUpIframeNavigationTest,
                          &RunIframeOutOfBundleNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest,
                       IframeParentInitiatedOutOfBundleNavigation) {
  RunSharedNavigationTest(&SetUpIframeNavigationTest,
                          &RunIframeParentInitiatedOutOfBundleNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, IframeSameDocumentNavigation) {
  RunSharedNavigationTest(&SetUpIframeNavigationTest,
                          &RunIframeSameDocumentNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, InvalidWebBundleFile) {
  const GURL test_data_url =
      GetTestUrlForFile(GetTestDataPath("invalid_web_bundle.wbn"));

  std::string console_message = ExpectNavigationFailureAndReturnConsoleMessage(
      shell()->web_contents(), test_data_url);

  EXPECT_EQ("Failed to read metadata of Web Bundle file: Wrong magic bytes.",
            console_message);
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest,
                       ResponseParseErrorInMainResource) {
  const GURL test_data_url = GetTestUrlForFile(
      GetTestDataPath("broken_bundle_broken_first_entry.wbn"));

  std::string console_message = ExpectNavigationFailureAndReturnConsoleMessage(
      shell()->web_contents(), test_data_url);

  EXPECT_EQ(
      "Failed to read response header of Web Bundle file: Response headers map "
      "must have exactly one pseudo-header, :status.",
      console_message);
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest,
                       ResponseParseErrorInSubresource) {
  const GURL test_data_url = GetTestUrlForFile(
      GetTestDataPath("broken_bundle_broken_script_entry.wbn"));
  NavigateToBundleAndWaitForReady(
      test_data_url, web_bundle_utils::GetSynthesizedUrlForWebBundle(
                         test_data_url, GURL(kTestPageUrl)));

  WebContents* web_contents = shell()->web_contents();
  WebContentsConsoleObserver console_observer(web_contents);

  ExecuteScriptAndWaitForTitle(R"(
    const script = document.createElement("script");
    script.onerror = () => { document.title = "load failed";};
    script.src = "script.js";
    document.body.appendChild(script);)",
                               "load failed");

  if (console_observer.messages().empty())
    console_observer.Wait();

  ASSERT_FALSE(console_observer.messages().empty());
  EXPECT_EQ(
      "Failed to read response header of Web Bundle file: Response headers map "
      "must have exactly one pseudo-header, :status.",
      base::UTF16ToUTF8(console_observer.messages()[0].message));
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, NoLocalFileScheme) {
  const GURL test_data_url =
      GetTestUrlForFile(GetTestDataPath("web_bundle_browsertest.wbn"));
  NavigateToBundleAndWaitForReady(
      test_data_url, web_bundle_utils::GetSynthesizedUrlForWebBundle(
                         test_data_url, GURL(kTestPageUrl)));

  auto expected_title = base::ASCIIToUTF16("load failed");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16("Local Script"));

  const GURL script_file_url =
      net::FilePathToFileURL(GetTestDataPath("local_script.js"));
  const std::string script = base::StringPrintf(R"(
    const script = document.createElement("script");
    script.onerror = () => { document.title = "load failed";};
    script.src = "%s";
    document.body.appendChild(script);)",
                                                script_file_url.spec().c_str());
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(), script));

  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, DataDecoderRestart) {
  constexpr char kTestPage1Url[] = "https://test.example.org/page1.html";
  constexpr char kTestPage2Url[] = "https://test.example.org/page2.html";
  // The content of this file will be read as response body of any exchange.
  base::FilePath test_file_path = GetTestDataPath("mocked.wbn");
  MockParserFactory mock_factory(
      {GURL(kTestPageUrl), GURL(kTestPage1Url), GURL(kTestPage2Url)},
      test_file_path);
  const GURL test_data_url = GetTestUrlForFile(test_file_path);
  NavigateAndWaitForTitle(shell()->web_contents(), test_data_url,
                          web_bundle_utils::GetSynthesizedUrlForWebBundle(
                              test_data_url, GURL(kTestPageUrl)),
                          kTestPageUrl);

  EXPECT_EQ(1, mock_factory.GetParserCreationCount());
  mock_factory.SimulateParserDisconnect();

  NavigateToURLAndWaitForTitle(GURL(kTestPage1Url), kTestPage1Url);
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            web_bundle_utils::GetSynthesizedUrlForWebBundle(
                test_data_url, GURL(kTestPage1Url)));

  EXPECT_EQ(2, mock_factory.GetParserCreationCount());
  mock_factory.SimulateParserDisconnect();

  NavigateToURLAndWaitForTitle(GURL(kTestPage2Url), kTestPage2Url);
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            web_bundle_utils::GetSynthesizedUrlForWebBundle(
                test_data_url, GURL(kTestPage2Url)));

  EXPECT_EQ(3, mock_factory.GetParserCreationCount());
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, ParseMetadataCrash) {
  base::FilePath test_file_path = GetTestDataPath("mocked.wbn");
  MockParserFactory mock_factory({GURL(kTestPageUrl)}, test_file_path);
  mock_factory.SimulateParseMetadataCrash();

  std::string console_message = ExpectNavigationFailureAndReturnConsoleMessage(
      shell()->web_contents(), GetTestUrlForFile(test_file_path));

  EXPECT_EQ(
      "Failed to read metadata of Web Bundle file: Cannot connect to the "
      "remote parser service",
      console_message);
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, ParseResponseCrash) {
  base::FilePath test_file_path = GetTestDataPath("mocked.wbn");
  MockParserFactory mock_factory({GURL(kTestPageUrl)}, test_file_path);
  mock_factory.SimulateParseResponseCrash();

  std::string console_message = ExpectNavigationFailureAndReturnConsoleMessage(
      shell()->web_contents(), GetTestUrlForFile(test_file_path));

  EXPECT_EQ(
      "Failed to read response header of Web Bundle file: Cannot connect to "
      "the remote parser service",
      console_message);
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, Variants) {
  SetAcceptLangs("ja,en");
  const GURL test_data_url =
      GetTestUrlForFile(GetTestDataPath("variants_test.wbn"));
  NavigateAndWaitForTitle(shell()->web_contents(), test_data_url,
                          web_bundle_utils::GetSynthesizedUrlForWebBundle(
                              test_data_url, GURL(kTestPageUrl)),
                          "lang=ja");
  SetAcceptLangs("en,ja");
  NavigateAndWaitForTitle(shell()->web_contents(), test_data_url,
                          web_bundle_utils::GetSynthesizedUrlForWebBundle(
                              test_data_url, GURL(kTestPageUrl)),
                          "lang=en");

  ExecuteScriptAndWaitForTitle(R"(
    (async function() {
      const headers = {Accept: 'application/octet-stream'};
      const resp = await fetch('/type', {headers});
      const data = await resp.json();
      document.title = data.text;
    })();)",
                               "octet-stream");
  ExecuteScriptAndWaitForTitle(R"(
    (async function() {
      const headers = {Accept: 'application/json'};
      const resp = await fetch('/type', {headers});
      const data = await resp.json();
      document.title = data.text;
    })();)",
                               "json");
  ExecuteScriptAndWaitForTitle(R"(
    (async function() {
      const headers = {Accept: 'foo/bar'};
      const resp = await fetch('/type', {headers});
      const data = await resp.json();
      document.title = data.text;
    })();)",
                               "octet-stream");

  ExecuteScriptAndWaitForTitle(R"(
    (async function() {
      const resp = await fetch('/lang');
      const data = await resp.json();
      document.title = data.text;
    })();)",
                               "ja");
  // If Accept-Language header is explicitly set, respect it.
  ExecuteScriptAndWaitForTitle(R"(
    (async function() {
      const headers = {'Accept-Language': 'fr'};
      const resp = await fetch('/lang', {headers});
      const data = await resp.json();
      document.title = data.text;
    })();)",
                               "fr");
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, IframeNavigationNoCrash) {
  // Regression test for crbug.com/1058721. There was a bug that navigation of
  // OOPIF's remote iframe in Web Bundle file cause crash.
  const GURL test_data_url =
      GetTestUrlForFile(GetTestDataPath("web_bundle_browsertest.wbn"));
  NavigateToBundleAndWaitForReady(
      test_data_url, web_bundle_utils::GetSynthesizedUrlForWebBundle(
                         test_data_url, GURL(kTestPageUrl)));

  const std::string empty_page_path = "/web_bundle/empty_page.html";
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL empty_page_url = embedded_test_server()->GetURL(empty_page_path);

  ExecuteScriptAndWaitForTitle(
      base::StringPrintf(R"(
    (async function() {
      const empty_page_url = '%s';
      const iframe = document.createElement('iframe');
      const onload = () => {
        iframe.removeEventListener('load', onload);
        document.title = 'Iframe loaded';
      }
      iframe.addEventListener('load', onload);
      iframe.src = empty_page_url;
      document.body.appendChild(iframe);
    })();)",
                         empty_page_url.spec().c_str()),
      "Iframe loaded");

  ExecuteScriptAndWaitForTitle(R"(
    (async function() {
      const iframe = document.querySelector("iframe");
      const onload = () => {
        document.title = 'Iframe loaded again';
      }
      iframe.addEventListener('load', onload);
      iframe.src = iframe.src + '?';
    })();)",
                               "Iframe loaded again");
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, Iframe) {
  net::EmbeddedTestServer third_party_server;
  GURL primary_url_origin;
  GURL third_party_origin;
  std::string web_bundle_content;
  SetUpSubPageTest(embedded_test_server(), &third_party_server,
                   &primary_url_origin, &third_party_origin,
                   &web_bundle_content);

  base::FilePath file_path;
  CreateTemporaryWebBundleFile(web_bundle_content, &file_path);
  const GURL test_data_url = GetTestUrlForFile(file_path);
  NavigateToBundleAndWaitForReady(
      test_data_url, web_bundle_utils::GetSynthesizedUrlForWebBundle(
                         test_data_url, primary_url_origin.Resolve("/top")));
  RunSubPageTest(shell()->web_contents(), primary_url_origin,
                 third_party_origin, &AddIframeAndWaitForMessage,
                 true /* support_third_party_wbn_page */);
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, WindowOpen) {
  net::EmbeddedTestServer third_party_server;
  GURL primary_url_origin;
  GURL third_party_origin;
  std::string web_bundle_content;
  SetUpSubPageTest(embedded_test_server(), &third_party_server,
                   &primary_url_origin, &third_party_origin,
                   &web_bundle_content);

  base::FilePath file_path;
  CreateTemporaryWebBundleFile(web_bundle_content, &file_path);
  const GURL test_data_url = GetTestUrlForFile(file_path);
  NavigateToBundleAndWaitForReady(
      test_data_url, web_bundle_utils::GetSynthesizedUrlForWebBundle(
                         test_data_url, primary_url_origin.Resolve("/top")));
  RunSubPageTest(shell()->web_contents(), primary_url_origin,
                 third_party_origin, &WindowOpenAndWaitForMessage,
                 true /* support_third_party_wbn_page */);
}

INSTANTIATE_TEST_SUITE_P(WebBundleFileBrowserTest,
                         WebBundleFileBrowserTest,
                         TEST_FILE_PATH_MODE_PARAMS);

class WebBundleNetworkBrowserTest : public WebBundleBrowserTestBase {
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
    std::string console_message =
        ExpectNavigationFailureAndReturnConsoleMessage(shell()->web_contents(),
                                                       url);
    EXPECT_EQ(expected_console_error, console_message);
  }

  void HistoryBackAndWaitUntilConsoleError(
      const std::string& expected_error_message) {
    WebContents* web_contents = shell()->web_contents();
    WebContentsConsoleObserver console_observer(web_contents);

    base::RunLoop run_loop;
    FinishNavigationObserver finish_navigation_observer(web_contents,
                                                        run_loop.QuitClosure());
    EXPECT_TRUE(ExecuteScript(web_contents, "history.back();"));

    run_loop.Run();
    ASSERT_TRUE(finish_navigation_observer.error_code());
    EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
              *finish_navigation_observer.error_code());

    if (console_observer.messages().empty())
      console_observer.Wait();

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
  std::string headers_ = kDefaultHeaders;
  std::string contents_;

  DISALLOW_COPY_AND_ASSIGN(WebBundleNetworkBrowserTest);
};

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, Simple) {
  const std::string wbn_path = "/web_bundle/test.wbn";
  const std::string primary_url_path = "/web_bundle/test.html";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL wbn_url = embedded_test_server()->GetURL(wbn_path);
  const GURL primary_url = embedded_test_server()->GetURL(primary_url_path);

  SetContents(CreateSimpleWebBundle(primary_url));
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

  web_package::test::WebBundleBuilder builder(primary_url.spec(), "");
  builder.AddExchange(primary_url.spec(),
                      {{":status", "200"}, {"content-type", "text/html"}},
                      "<script src=\"script.js\"></script>");
  builder.AddExchange(
      script_url.spec(),
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

  SetContents(CreateSimpleWebBundle(primary_url));
  WebContents* web_contents = shell()->web_contents();
  std::unique_ptr<DownloadObserver> download_observer =
      std::make_unique<DownloadObserver>(BrowserContext::GetDownloadManager(
          web_contents->GetBrowserContext()));

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

  SetContents(CreateSimpleWebBundle(primary_url));
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
  SetContents(CreateSimpleWebBundle(primary_url));
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

  SetContents(CreateSimpleWebBundle(primary_url));
  SetHeaders(
      "HTTP/1.1 200 OK\n"
      "Content-Type: application/webbundle\n");
  TestNavigationFailure(wbn_url,
                        "Web Bundle response must have "
                        "\"X-Content-Type-Options: nosniff\" header.");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, PrimaryURLNotFound) {
  const std::string wbn_path = "/web_bundle/test.wbn";
  const std::string primary_url_path = "/web_bundle/test.html";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL wbn_url = embedded_test_server()->GetURL(wbn_path);
  const GURL primary_url = embedded_test_server()->GetURL(primary_url_path);
  const GURL inner_url =
      embedded_test_server()->GetURL("/web_bundle/inner.html");
  web_package::test::WebBundleBuilder builder(primary_url.spec(), "");
  builder.AddExchange(inner_url.spec(),
                      {{":status", "200"}, {"content-type", "text/html"}},
                      "<title>Ready</title>");
  std::vector<uint8_t> bundle = builder.CreateBundle();
  SetContents(std::string(bundle.begin(), bundle.end()));
  TestNavigationFailure(
      wbn_url, "The primary URL resource is not found in the web bundle.");
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, OriginMismatch) {
  const std::string wbn_path = "/web_bundle/test.wbn";
  const std::string primary_url_path = "/web_bundle/test.html";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL primary_url =
      embedded_test_server()->GetURL("localhost", primary_url_path);

  SetContents(CreateSimpleWebBundle(primary_url));
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

  TestNavigationFailure(
      embedded_test_server()->GetURL(wbn_path),
      "Failed to read metadata of Web Bundle file: Wrong magic bytes.");
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
  MockParserFactory mock_factory(items);

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
  MockParserFactory mock_factory(items);
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
  MockParserFactory mock_factory(items);
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

  SetContents(CreateSimpleWebBundle(primary_url));
  TestNavigationFailure(
      wbn_url,
      base::StringPrintf("Path restriction mismatch: Can't navigate to %s in "
                         "the web bundle served from %s.",
                         primary_url.spec().c_str(), wbn_url.spec().c_str()));
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, BasicNavigation) {
  RunSharedNavigationTest(&SetUpBasicNavigationTest, &RunBasicNavigationTest);
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest,
                       BrowserInitiatedOutOfBundleNavigation) {
  RunSharedNavigationTest(&SetUpBrowserInitiatedOutOfBundleNavigationTest,
                          &RunBrowserInitiatedOutOfBundleNavigationTest);
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest,
                       RendererInitiatedOutOfBundleNavigation) {
  RunSharedNavigationTest(&SetUpRendererInitiatedOutOfBundleNavigationTest,
                          &RunRendererInitiatedOutOfBundleNavigationTest);
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, SameDocumentNavigation) {
  RunSharedNavigationTest(&SetUpSameDocumentNavigationTest,
                          &RunSameDocumentNavigationTest);
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, IframeNavigation) {
  RunSharedNavigationTest(&SetUpIframeNavigationTest, &RunIframeNavigationTest);
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest,
                       IframeOutOfBundleNavigation) {
  RunSharedNavigationTest(&SetUpIframeNavigationTest,
                          &RunIframeOutOfBundleNavigationTest);
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest,
                       IframeParentInitiatedOutOfBundleNavigation) {
  RunSharedNavigationTest(&SetUpIframeNavigationTest,
                          &RunIframeParentInitiatedOutOfBundleNavigationTest);
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest,
                       IframeSameDocumentNavigation) {
  RunSharedNavigationTest(&SetUpIframeNavigationTest,
                          &RunIframeSameDocumentNavigationTest);
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, CrossScopeNavigations) {
  const std::string wbn_path = "/web_bundle/path_test/in_scope/path_test.wbn";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());
  SetContents(CreatePathTestWebBundle(embedded_test_server()->base_url()));

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
  SetContents(CreatePathTestWebBundle(embedded_test_server()->base_url()));

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
  SetContents(CreateSimpleWebBundle(primary_url));
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
  SetContents(CreateSimpleWebBundle(primary_url));
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
  SetContents(CreateSimpleWebBundle(primary_url));
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
  const std::string wbn_path = "/web_bundle/test.wbn";
  const std::string primary_url_path = "/web_bundle/test.html";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL wbn_url = embedded_test_server()->GetURL(wbn_path);
  const GURL primary_url = embedded_test_server()->GetURL(primary_url_path);

  AddHeaders("Cache-Control:no-store\n");
  SetContents(CreateSimpleWebBundle(primary_url));
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
  const std::string wbn_path = "/web_bundle/test.wbn";
  const std::string primary_url_path = "/web_bundle/test.html";
  const std::string alt_primary_url_path = "/web_bundle/alt.html";
  RegisterRequestHandler(wbn_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL wbn_url = embedded_test_server()->GetURL(wbn_path);
  const GURL primary_url = embedded_test_server()->GetURL(primary_url_path);

  AddHeaders("Cache-Control:no-store\n");
  SetContents(CreateSimpleWebBundle(primary_url));
  NavigateToBundleAndWaitForReady(wbn_url, primary_url);
  NavigateToURLAndWaitForTitle(
      embedded_test_server()->GetURL("/web_bundle/empty_page.html"),
      "Empty Page");

  SetContents(CreateSimpleWebBundle(
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
  SetUpSubPageTest(embedded_test_server(), &third_party_server,
                   &primary_url_origin, &third_party_origin,
                   &web_bundle_content);
  SetContents(web_bundle_content);
  NavigateToBundleAndWaitForReady(primary_url_origin.Resolve(wbn_path),
                                  primary_url_origin.Resolve("/top"));
  RunSubPageTest(shell()->web_contents(), primary_url_origin,
                 third_party_origin, &AddIframeAndWaitForMessage,
                 false /* support_third_party_wbn_page */);
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, WindowOpen) {
  const std::string wbn_path = "/test.wbn";
  RegisterRequestHandler(wbn_path);

  net::EmbeddedTestServer third_party_server;
  GURL primary_url_origin;
  GURL third_party_origin;
  std::string web_bundle_content;
  SetUpSubPageTest(embedded_test_server(), &third_party_server,
                   &primary_url_origin, &third_party_origin,
                   &web_bundle_content);
  SetContents(web_bundle_content);
  NavigateToBundleAndWaitForReady(primary_url_origin.Resolve(wbn_path),
                                  primary_url_origin.Resolve("/top"));
  RunSubPageTest(shell()->web_contents(), primary_url_origin,
                 third_party_origin, &WindowOpenAndWaitForMessage,
                 false /* support_third_party_wbn_page */);
}

IN_PROC_BROWSER_TEST_F(WebBundleNetworkBrowserTest, OutScopeSubPage) {
  const std::string wbn_path = "/in_scope/test.wbn";
  const std::string primary_url_path = "/in_scope/test.html";

  RegisterRequestHandler(wbn_path);
  RegisterRequestHandlerForSubPageTest(embedded_test_server(), "");
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL origin = embedded_test_server()->GetURL("/");
  web_package::test::WebBundleBuilder builder(
      origin.Resolve(primary_url_path).spec(), "");
  AddHtmlFile(&builder, origin, primary_url_path, R"(
    <script>
    window.addEventListener('message',
                            event => domAutomationController.send(event.data),
                            false);
    document.title = 'Ready';
    </script>
    )");
  AddHtmlFile(&builder, origin, "/in_scope/subpage",
              CreateSubPageHtml("in-scope-wbn-page"));
  AddScriptFile(&builder, origin, "/in_scope/script",
                CreateScriptForSubPageTest("in-scope-wbn-script"));
  AddHtmlFile(&builder, origin, "/out_scope/subpage",
              CreateSubPageHtml("out-scope-wbn-page"));
  AddScriptFile(&builder, origin, "/out_scope/script",
                CreateScriptForSubPageTest("out-scope-wbn-script"));
  std::vector<uint8_t> bundle = builder.CreateBundle();
  SetContents(std::string(bundle.begin(), bundle.end()));
  NavigateToBundleAndWaitForReady(origin.Resolve(wbn_path),
                                  origin.Resolve(primary_url_path));
  const auto funcs = {&AddIframeAndWaitForMessage,
                      &WindowOpenAndWaitForMessage};
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
