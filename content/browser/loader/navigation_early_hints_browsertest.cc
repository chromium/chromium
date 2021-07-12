// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/loader/navigation_early_hints_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/quic_simple_test_server.h"
#include "net/test/test_data_directory.h"

namespace content {

using PreloadedResources = NavigationEarlyHintsManager::PreloadedResources;

namespace {

struct HeaderField {
  HeaderField(const std::string& name, const std::string& value)
      : name(name), value(value) {}

  std::string name;
  std::string value;
};

struct ResponseEntry {
  ResponseEntry(const std::string& path, net::HttpStatusCode status_code)
      : path(path) {
    headers[":path"] = path;
    headers[":status"] = base::StringPrintf("%d", status_code);
  }

  void AddEarlyHints(const std::vector<HeaderField>& headers) {
    spdy::Http2HeaderBlock hints_headers;
    for (const auto& header : headers)
      hints_headers.AppendValueOrAddHeader(header.name, header.value);
    early_hints.push_back(std::move(hints_headers));
  }

  std::string path;
  spdy::Http2HeaderBlock headers;
  std::string body;
  std::vector<spdy::Http2HeaderBlock> early_hints;
};

const char kPageWithHintedScriptPath[] = "/page_with_hinted_js.html";
const char kPageWithHintedScriptBody[] = "<script src=\"/hinted.js\"></script>";

const char kPageWithHintedCorsScriptPath[] = "/page_with_hinted_cors_js.html";
const char kPageWithHintedCorsScriptBody[] =
    "<script src=\"/hinted.js\" crossorigin></script>";

const char kPageWithIframePath[] = "/page_with_iframe.html";
const char kPageWithIframeBody[] =
    "<iframe src=\"page_with_hinted_js.html\"></iframe>";

const char kPageWithHintedModuleScriptPath[] =
    "/page_with_hinted_module_js.html";
const char kPageWithHintedModuleScriptBody[] =
    "<script src=\"/hinted.js\" type=\"module\"></script>";

const char kHintedScriptPath[] = "/hinted.js";
const char kHintedScriptBody[] = "document.title = 'Done';";

const char kHintedStylesheetPath[] = "/hinted.css";
const char kHintedStylesheetBody[] = "/*empty*/";

}  // namespace

// Most tests use EmbeddedTestServer but this uses QuicSimpleTestServer because
// Early Hints are only plumbed over HTTP/2 or HTTP/3 (QUIC).
class NavigationEarlyHintsTest : public ContentBrowserTest {
 public:
  NavigationEarlyHintsTest() = default;
  ~NavigationEarlyHintsTest() override = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ConfigureMockCertVerifier();
    ASSERT_TRUE(net::QuicSimpleTestServer::Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kOriginToForceQuicOn, "*");
    mock_cert_verifier_.SetUpCommandLine(command_line);
    feature_list_.InitAndEnableFeature(
        features::kEarlyHintsPreloadForNavigation);
    cross_origin_server_.RegisterRequestHandler(
        base::BindRepeating(&NavigationEarlyHintsTest::HandleCrossOriginRequest,
                            base::Unretained(this)));
    ASSERT_TRUE(cross_origin_server_.Start());
    ContentBrowserTest::SetUpCommandLine(command_line);
  }

  void TearDown() override {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
    net::QuicSimpleTestServer::Shutdown();
    ContentBrowserTest::TearDown();
  }

  net::test_server::EmbeddedTestServer& cross_origin_server() {
    return cross_origin_server_;
  }

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  void ConfigureMockCertVerifier() {
    auto test_cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "quic-chain.pem");
    net::CertVerifyResult verify_result;
    verify_result.verified_cert = test_cert;
    mock_cert_verifier_.mock_cert_verifier()->AddResultForCert(
        test_cert, verify_result, net::OK);
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
  }

  HeaderField CreatePreloadLinkForScript() {
    return HeaderField(
        "link",
        base::StringPrintf("<%s>; rel=preload; as=script", kHintedScriptPath));
  }

  HeaderField CreatePreloadLinkForCorsScript() {
    return HeaderField(
        "link", base::StringPrintf("<%s>; rel=preload; as=script; crossorigin",
                                   kHintedScriptPath));
  }

  HeaderField CreateModulePreloadLink() {
    return HeaderField("link", base::StringPrintf("<%s>; rel=modulepreload",
                                                  kHintedScriptPath));
  }

  HeaderField CreatePreloadLinkForStylesheet() {
    return HeaderField("link",
                       base::StringPrintf("<%s>; rel=preload; as=stylesheet",
                                          kHintedStylesheetPath));
  }

  void RegisterResponse(const ResponseEntry& entry) {
    net::QuicSimpleTestServer::AddResponseWithEarlyHints(
        entry.path, entry.headers, entry.body, entry.early_hints);
  }

  void RegisterHintedScriptResource() {
    ResponseEntry hinted_script_entry(kHintedScriptPath, net::HTTP_OK);
    hinted_script_entry.headers["content-type"] = "application/javascript";
    hinted_script_entry.body = kHintedScriptBody;
    RegisterResponse(hinted_script_entry);
  }

  void RegisterHintedStylesheetResource() {
    ResponseEntry hinted_script_entry(kHintedStylesheetPath, net::HTTP_OK);
    hinted_script_entry.headers["content-type"] = "text/css";
    hinted_script_entry.body = kHintedStylesheetBody;
    RegisterResponse(hinted_script_entry);
  }

  ResponseEntry CreatePageEntryWithHintedScript(
      net::HttpStatusCode status_code) {
    RegisterHintedScriptResource();

    ResponseEntry entry(kPageWithHintedScriptPath, status_code);
    entry.body = kPageWithHintedScriptBody;
    HeaderField link_header = CreatePreloadLinkForScript();
    entry.AddEarlyHints({std::move(link_header)});

    return entry;
  }

  ResponseEntry CreatePageEntryWithHintedCorsScript(
      net::HttpStatusCode status_code) {
    RegisterHintedScriptResource();

    ResponseEntry entry(kPageWithHintedCorsScriptPath, status_code);
    entry.body = kPageWithHintedCorsScriptBody;
    HeaderField link_header = CreatePreloadLinkForCorsScript();
    entry.AddEarlyHints({std::move(link_header)});

    return entry;
  }

  ResponseEntry CreatePageEntryWithHintedModuleScript(
      net::HttpStatusCode status_code) {
    RegisterHintedScriptResource();

    ResponseEntry entry(kPageWithHintedModuleScriptPath, status_code);
    entry.body = kPageWithHintedModuleScriptBody;
    HeaderField link_header = CreateModulePreloadLink();
    entry.AddEarlyHints({std::move(link_header)});

    return entry;
  }

  bool NavigateToURLAndWaitTitle(const GURL& url, const std::string& title) {
    std::u16string title16 = base::ASCIIToUTF16(title);
    TitleWatcher title_watcher(shell()->web_contents(), title16);
    if (!NavigateToURL(shell(), url))
      return false;
    return title16 == title_watcher.WaitAndGetTitle();
  }

  NavigationEarlyHintsManager* GetEarlyHintsManager() {
    RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetMainFrame());
    return rfh->early_hints_manager();
  }

  PreloadedResources WaitForPreloadedResources() {
    base::RunLoop loop;
    PreloadedResources result;
    if (!GetEarlyHintsManager())
      return result;

    GetEarlyHintsManager()->WaitForPreloadsFinishedForTesting(
        base::BindLambdaForTesting([&](PreloadedResources preloaded_resources) {
          result = preloaded_resources;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  enum class FetchResult {
    kFetched,
    kBlocked,
  };
  FetchResult FetchScriptOnDocument(GURL src) {
    EvalJsResult result = EvalJs(shell(), JsReplace(R"(
      new Promise(resolve => {
        const script = document.createElement("script");
        script.src = $1;
        script.onerror = () => resolve("blocked");
        script.onload = () => resolve("fetched");
        document.body.appendChild(script);
      });
    )",
                                                    src));
    return result.ExtractString() == "fetched" ? FetchResult::kFetched
                                               : FetchResult::kBlocked;
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleCrossOriginRequest(
      const net::test_server::HttpRequest& request) {
    GURL relative_url = request.base_url.Resolve(request.relative_url);
    if (relative_url.path() != "/hinted.js")
      return nullptr;

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("application/javascript");
    response->set_content("/*empty*/");

    std::string query = relative_url.query();
    if (query == "corp-cross-origin") {
      response->AddCustomHeader("Cross-Origin-Resource-Policy", "cross-origin");
    } else if (query == "corp-same-origin") {
      response->AddCustomHeader("Cross-Origin-Resource-Policy", "same-origin");
    }

    return std::move(response);
  }

  base::test::ScopedFeatureList feature_list_;

  ContentMockCertVerifier mock_cert_verifier_;

  net::EmbeddedTestServer cross_origin_server_;
};

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, Basic) {
  ResponseEntry entry = CreatePageEntryWithHintedScript(net::HTTP_OK);
  RegisterResponse(entry);

  EXPECT_TRUE(NavigateToURLAndWaitTitle(
      net::QuicSimpleTestServer::GetFileURL(kPageWithHintedScriptPath),
      "Done"));
  PreloadedResources preloads = WaitForPreloadedResources();
  EXPECT_EQ(preloads.size(), 1UL);

  GURL preloaded_url = net::QuicSimpleTestServer::GetFileURL(kHintedScriptPath);
  auto it = preloads.find(preloaded_url);
  ASSERT_TRUE(it != preloads.end());
  ASSERT_FALSE(it->second.was_canceled);
  ASSERT_TRUE(it->second.error_code.has_value());
  EXPECT_EQ(it->second.error_code.value(), net::OK);
}

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, CorsAttribute) {
  ResponseEntry entry = CreatePageEntryWithHintedCorsScript(net::HTTP_OK);
  RegisterResponse(entry);

  EXPECT_TRUE(NavigateToURLAndWaitTitle(
      net::QuicSimpleTestServer::GetFileURL(kPageWithHintedCorsScriptPath),
      "Done"));
  PreloadedResources preloads = WaitForPreloadedResources();
  EXPECT_EQ(preloads.size(), 1UL);

  GURL preloaded_url = net::QuicSimpleTestServer::GetFileURL(kHintedScriptPath);
  auto it = preloads.find(preloaded_url);
  ASSERT_TRUE(it != preloads.end());
  ASSERT_FALSE(it->second.was_canceled);
  ASSERT_TRUE(it->second.error_code.has_value());
  EXPECT_EQ(it->second.error_code.value(), net::OK);
}

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, ModulePreload) {
  ResponseEntry entry = CreatePageEntryWithHintedModuleScript(net::HTTP_OK);
  RegisterResponse(entry);

  EXPECT_TRUE(NavigateToURLAndWaitTitle(
      net::QuicSimpleTestServer::GetFileURL(kPageWithHintedModuleScriptPath),
      "Done"));
  PreloadedResources preloads = WaitForPreloadedResources();
  EXPECT_EQ(preloads.size(), 1UL);

  GURL preloaded_url = net::QuicSimpleTestServer::GetFileURL(kHintedScriptPath);
  auto it = preloads.find(preloaded_url);
  ASSERT_TRUE(it != preloads.end());
  ASSERT_FALSE(it->second.was_canceled);
  ASSERT_TRUE(it->second.error_code.has_value());
  EXPECT_EQ(it->second.error_code.value(), net::OK);
}

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, DisallowPreloadFromIframe) {
  ResponseEntry page_entry(kPageWithIframePath, net::HTTP_OK);
  page_entry.body = kPageWithIframeBody;
  RegisterResponse(page_entry);

  ResponseEntry iframe_entry = CreatePageEntryWithHintedScript(net::HTTP_OK);
  RegisterResponse(iframe_entry);

  EXPECT_TRUE(NavigateToURL(
      shell(), net::QuicSimpleTestServer::GetFileURL(kPageWithIframePath)));

  // Find RenderFrameHost for the iframe.
  std::vector<RenderFrameHost*> all_frames =
      CollectAllRenderFrameHosts(shell()->web_contents());
  ASSERT_EQ(all_frames.size(), 2UL);
  ASSERT_TRUE(all_frames[1]->IsDescendantOf(all_frames[0]));
  RenderFrameHostImpl* iframe_host =
      static_cast<RenderFrameHostImpl*>(all_frames[1]);

  EXPECT_TRUE(WaitForLoadStop(WebContents::FromRenderFrameHost(iframe_host)));
  ASSERT_EQ(iframe_host->GetLastCommittedURL(),
            net::QuicSimpleTestServer::GetFileURL(kPageWithHintedScriptPath));

  // NavigationEarlyHintsManager should not be created for subframes. If it were
  // created it should have been created before navigation commit.
  EXPECT_EQ(iframe_host->early_hints_manager(), nullptr);
}

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, NavigationServerError) {
  ResponseEntry entry =
      CreatePageEntryWithHintedScript(net::HTTP_INTERNAL_SERVER_ERROR);
  entry.body = "Internal Server Error";
  RegisterResponse(entry);

  EXPECT_TRUE(NavigateToURL(shell(), net::QuicSimpleTestServer::GetFileURL(
                                         kPageWithHintedScriptPath)));
  PreloadedResources preloads = WaitForPreloadedResources();
  EXPECT_TRUE(preloads.empty());
}

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, Redirect) {
  ResponseEntry entry = CreatePageEntryWithHintedScript(net::HTTP_FOUND);
  entry.headers["location"] = "/";
  entry.body = "";
  RegisterResponse(entry);

  EXPECT_TRUE(NavigateToURL(
      shell(), net::QuicSimpleTestServer::GetFileURL(kPageWithHintedScriptPath),
      net::QuicSimpleTestServer::GetFileURL("/")));
  PreloadedResources preloads = WaitForPreloadedResources();
  EXPECT_TRUE(preloads.empty());
}

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, InvalidPreloadLink) {
  const std::string kPath = "/hinted.html";

  RegisterHintedScriptResource();

  ResponseEntry entry(kPath, net::HTTP_OK);
  entry.body = "body";
  entry.AddEarlyHints(
      {HeaderField("link", base::StringPrintf("<%s>; rel=preload; as=invalid",
                                              kHintedScriptPath))});
  RegisterResponse(entry);

  EXPECT_TRUE(
      NavigateToURL(shell(), net::QuicSimpleTestServer::GetFileURL(kPath)));
  PreloadedResources preloads = WaitForPreloadedResources();
  EXPECT_TRUE(preloads.empty());
}

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, DuplicatePreloads) {
  RegisterHintedScriptResource();
  RegisterHintedStylesheetResource();

  ResponseEntry entry(kPageWithHintedScriptPath, net::HTTP_OK);
  entry.body = kPageWithHintedScriptBody;

  // Set two Early Hints responses which contain duplicate preload link headers.
  HeaderField script_link_header = CreatePreloadLinkForScript();
  HeaderField stylesheet_link_header = CreatePreloadLinkForStylesheet();
  entry.AddEarlyHints({script_link_header});
  entry.AddEarlyHints({script_link_header, stylesheet_link_header});
  RegisterResponse(entry);

  EXPECT_TRUE(NavigateToURLAndWaitTitle(
      net::QuicSimpleTestServer::GetFileURL(kPageWithHintedScriptPath),
      "Done"));
  PreloadedResources preloads = WaitForPreloadedResources();
  EXPECT_EQ(preloads.size(), 2UL);

  GURL script_url = net::QuicSimpleTestServer::GetFileURL(kHintedScriptPath);
  GURL stylesheet_url =
      net::QuicSimpleTestServer::GetFileURL(kHintedStylesheetPath);
  EXPECT_TRUE(preloads.contains(script_url));
  EXPECT_TRUE(preloads.contains(stylesheet_url));
}

const char kPageWithCrossOriginScriptPage[] =
    "/page_with_cross_origin_script.html";

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, CORP_Pass) {
  // The server response's is a script with
  // `Cross-Origin-Resource-Policy: cross-origin`.
  const GURL kCrossOriginScriptUrl =
      cross_origin_server().GetURL("/hinted.js?corp-cross-origin");

  ResponseEntry page_entry(kPageWithCrossOriginScriptPage, net::HTTP_OK);
  HeaderField link_header = HeaderField(
      "link", base::StringPrintf("<%s>; rel=preload; as=script",
                                 kCrossOriginScriptUrl.spec().c_str()));
  page_entry.AddEarlyHints({std::move(link_header)});
  RegisterResponse(page_entry);

  EXPECT_TRUE(NavigateToURL(shell(), net::QuicSimpleTestServer::GetFileURL(
                                         kPageWithCrossOriginScriptPage)));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(FetchScriptOnDocument(kCrossOriginScriptUrl),
            FetchResult::kFetched);

  PreloadedResources preloads = WaitForPreloadedResources();
  EXPECT_EQ(preloads.size(), 1UL);

  auto it = preloads.find(kCrossOriginScriptUrl);
  ASSERT_TRUE(it != preloads.end());
  ASSERT_FALSE(it->second.was_canceled);
  ASSERT_TRUE(it->second.error_code.has_value());
  EXPECT_EQ(it->second.error_code.value(), net::OK);
}

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, CORP_Blocked) {
  // The server response's is a script with
  // `Cross-Origin-Resource-Policy: same-origin`.
  const GURL kCrossOriginScriptUrl =
      cross_origin_server().GetURL("/hinted.js?corp-same-origin");

  ResponseEntry page_entry(kPageWithCrossOriginScriptPage, net::HTTP_OK);
  HeaderField link_header = HeaderField(
      "link", base::StringPrintf("<%s>; rel=preload; as=script",
                                 kCrossOriginScriptUrl.spec().c_str()));
  page_entry.AddEarlyHints({std::move(link_header)});
  RegisterResponse(page_entry);

  EXPECT_TRUE(NavigateToURL(shell(), net::QuicSimpleTestServer::GetFileURL(
                                         kPageWithCrossOriginScriptPage)));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // The script fetch should be blocked.
  EXPECT_EQ(FetchScriptOnDocument(kCrossOriginScriptUrl),
            FetchResult::kBlocked);
}

}  // namespace content
