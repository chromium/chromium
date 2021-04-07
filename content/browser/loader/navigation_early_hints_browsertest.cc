// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "components/network_session_configurator/common/network_switches.h"
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
#include "net/test/quic_simple_test_server.h"
#include "net/test/test_data_directory.h"

namespace content {

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
const char kHintedScriptPath[] = "/hinted.js";
const char kHintedScriptBody[] = "document.title = 'Done';";

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
    ContentBrowserTest::SetUpCommandLine(command_line);
  }

  void TearDown() override {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
    net::QuicSimpleTestServer::Shutdown();
    ContentBrowserTest::TearDown();
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

  void RegisterResponse(const ResponseEntry& entry) {
    net::QuicSimpleTestServer::AddResponseWithEarlyHints(
        entry.path, entry.headers, entry.body, entry.early_hints);
  }

  ResponseEntry CreatePageEntryWithHintedScript(
      net::HttpStatusCode status_code) {
    ResponseEntry hinted_script_entry(kHintedScriptPath, net::HTTP_OK);
    hinted_script_entry.headers["content-type"] = "application/javascript";
    hinted_script_entry.body = kHintedScriptBody;
    RegisterResponse(hinted_script_entry);

    ResponseEntry entry(kPageWithHintedScriptPath, status_code);
    entry.body = kPageWithHintedScriptBody;
    HeaderField link_header(
        "link",
        base::StringPrintf("<%s>; rel=preload; as=script", kHintedScriptPath));
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

 private:
  base::test::ScopedFeatureList feature_list_;

  ContentMockCertVerifier mock_cert_verifier_;
};

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, Basic) {
  ResponseEntry entry = CreatePageEntryWithHintedScript(net::HTTP_OK);
  RegisterResponse(entry);

  EXPECT_TRUE(NavigateToURLAndWaitTitle(
      net::QuicSimpleTestServer::GetFileURL(kPageWithHintedScriptPath),
      "Done"));
  EXPECT_TRUE(GetEarlyHintsManager());
  // TODO(crbug.com/671310): Make sure hinted resource is preloaded.
}

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, NavigationServerError) {
  ResponseEntry entry =
      CreatePageEntryWithHintedScript(net::HTTP_INTERNAL_SERVER_ERROR);
  entry.body = "Internal Server Error";
  RegisterResponse(entry);

  EXPECT_TRUE(NavigateToURL(shell(), net::QuicSimpleTestServer::GetFileURL(
                                         kPageWithHintedScriptPath)));
  EXPECT_FALSE(GetEarlyHintsManager());
}

IN_PROC_BROWSER_TEST_F(NavigationEarlyHintsTest, Redirect) {
  ResponseEntry entry = CreatePageEntryWithHintedScript(net::HTTP_FOUND);
  entry.headers["location"] = "/";
  entry.body = "";
  RegisterResponse(entry);

  EXPECT_TRUE(NavigateToURL(
      shell(), net::QuicSimpleTestServer::GetFileURL(kPageWithHintedScriptPath),
      net::QuicSimpleTestServer::GetFileURL("/")));
  EXPECT_FALSE(GetEarlyHintsManager());
}

// TODO(crbug.com/671310): Add following test cases:
//  * Two Early Hints responses which contain duplicated preloads.
//    * The previous preload finished before receiving the second duplicated
//      response.
//  * An invalid preload header in an Early Hints response.
//  * Make sure preload requests are throttled.

}  // namespace content
