// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/manifest/manifest.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest_manager.mojom.h"

namespace content {

namespace {

using ::testing::Contains;
using ::testing::HasSubstr;

}  // namespace

class ManifestBrowserTest;

// Mock of a WebContentsDelegate that catches messages sent to the console.
class MockWebContentsDelegate : public WebContentsDelegate {
 public:
  explicit MockWebContentsDelegate(ManifestBrowserTest* test) : test_(test) {}

  bool DidAddMessageToConsole(WebContents* source,
                              blink::mojom::ConsoleMessageLevel log_level,
                              const std::u16string& message,
                              int32_t line_no,
                              const std::u16string& source_id) override;
  PreloadingEligibility IsPrerender2Supported(
      WebContents& web_contents) override {
    return PreloadingEligibility::kEligible;
  }

 private:
  raw_ptr<ManifestBrowserTest> test_ = nullptr;
};

class ManifestBrowserTest : public ContentBrowserTest,
                            public WebContentsObserver {
 protected:
  friend MockWebContentsDelegate;

  ManifestBrowserTest()
      : cors_embedded_test_server_(
            std::make_unique<net::EmbeddedTestServer>()) {
    cors_embedded_test_server_->ServeFilesFromSourceDirectory(
        GetTestDataFilePath());
  }

  ManifestBrowserTest(const ManifestBrowserTest&) = delete;
  ManifestBrowserTest& operator=(const ManifestBrowserTest&) = delete;

  ~ManifestBrowserTest() override {}

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    DCHECK(shell()->web_contents());

    mock_web_contents_delegate_ =
        std::make_unique<MockWebContentsDelegate>(this);
    shell()->web_contents()->SetDelegate(mock_web_contents_delegate_.get());
    Observe(shell()->web_contents());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void GetManifestAndWait() {
    shell()->web_contents()->GetPrimaryPage().GetManifest(base::BindOnce(
        &ManifestBrowserTest::OnGetManifest, base::Unretained(this)));

    message_loop_runner_ = new MessageLoopRunner();
    message_loop_runner_->Run();
  }

  void OnGetManifest(blink::mojom::ManifestRequestResult result,
                     const GURL& manifest_url,
                     blink::mojom::ManifestPtr manifest) {
    manifest_url_ = manifest_url;
    manifest_ = std::move(manifest);
    message_loop_runner_->Quit();
  }

  const blink::mojom::Manifest& manifest() const {
    DCHECK(manifest_);
    return *manifest_;
  }

  const GURL& manifest_url() const { return manifest_url_; }

  int GetConsoleErrorCount() const {
    // The IPCs reporting console errors are not FIFO with the manifest IPCs.
    // Waiting for a round-trip channel-associated message will wait until any
    // already enqueued channel-associated IPCs arrive at the browser process.
    mojo::AssociatedRemote<blink::mojom::ManifestManager> remote;
    shell()
        ->web_contents()
        ->GetPrimaryMainFrame()
        ->GetRemoteAssociatedInterfaces()
        ->GetInterface(&remote);
    remote.FlushForTesting();
    return console_errors_.size();
  }

  const std::vector<std::string>& console_errors() const {
    return console_errors_;
  }

  void OnReceivedConsoleError(std::u16string_view message) {
    console_errors_.push_back(base::UTF16ToUTF8(message));
  }

  net::EmbeddedTestServer* cors_embedded_test_server() const {
    return cors_embedded_test_server_.get();
  }

  const std::vector<GURL>& reported_manifest_urls() {
    return reported_manifest_urls_;
  }

  // Contains the number of manifests that had been received when each favicon
  // URL change is received.
  const std::vector<size_t>& manifests_reported_when_favicon_url_updated() {
    return manifests_reported_when_favicon_url_updated_;
  }

  // WebContentsObserver:
  void DidUpdateFaviconURL(
      RenderFrameHost* rfh,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) override {
    manifests_reported_when_favicon_url_updated_.push_back(
        reported_manifest_urls_.size());
  }

  void DidUpdateWebManifestURL(RenderFrameHost* rfh,
                               const GURL& manifest_url) override {
    if (manifest_url.is_empty()) {
      reported_manifest_urls_.emplace_back();
      return;
    }
    EXPECT_TRUE(manifest_url.is_valid());
    reported_manifest_urls_.push_back(manifest_url);
  }

 private:
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
  std::unique_ptr<MockWebContentsDelegate> mock_web_contents_delegate_;
  std::unique_ptr<net::EmbeddedTestServer> cors_embedded_test_server_;
  GURL manifest_url_;
  blink::mojom::ManifestPtr manifest_ = blink::mojom::Manifest::New();
  std::vector<std::string> console_errors_;
  std::vector<GURL> reported_manifest_urls_;
  std::vector<size_t> manifests_reported_when_favicon_url_updated_;
};

// The implementation of DidAddMessageToConsole isn't inlined because it needs
// to know about |test_|.
bool MockWebContentsDelegate::DidAddMessageToConsole(
    WebContents* source,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id) {
  DCHECK_EQ(source->GetDelegate(), this);

  if (log_level == blink::mojom::ConsoleMessageLevel::kError ||
      log_level == blink::mojom::ConsoleMessageLevel::kWarning) {
    test_->OnReceivedConsoleError(message);
  }
  return false;
}

// If a page has no manifest, requesting a manifest should return the empty
// manifest. The URL should be empty.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, NoManifest) {
  GURL test_url = embedded_test_server()->GetURL("/manifest/no-manifest.html");

  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  GetManifestAndWait();
  EXPECT_FALSE(blink::IsEmptyManifest(manifest()));
  EXPECT_TRUE(blink::IsDefaultManifest(manifest(), test_url));
  EXPECT_TRUE(manifest_url().is_empty());
  EXPECT_EQ(0, GetConsoleErrorCount());
  EXPECT_TRUE(reported_manifest_urls().empty());
  ASSERT_EQ(1u, manifests_reported_when_favicon_url_updated().size());
  EXPECT_EQ(0u, manifests_reported_when_favicon_url_updated()[0]);
}

// If a page manifest points to a 404 URL, requesting the manifest should return
// the empty manifest. However, the manifest URL will be non-empty.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, 404Manifest) {
  GURL test_url = embedded_test_server()->GetURL("/manifest/404-manifest.html");

  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  GetManifestAndWait();
  EXPECT_FALSE(blink::IsEmptyManifest(manifest()));
  EXPECT_TRUE(blink::IsDefaultManifest(manifest(), test_url));
  EXPECT_FALSE(manifest_url().is_empty());
  // 1 error for syntax errors in manifest/thereisnomanifestthere.json.
  EXPECT_EQ(1, GetConsoleErrorCount());
  ASSERT_EQ(1u, reported_manifest_urls().size());
  EXPECT_EQ(manifest_url(), reported_manifest_urls()[0]);
  EXPECT_EQ(1u, manifests_reported_when_favicon_url_updated().size());
}

// If a page has an empty manifest, requesting the manifest should return the
// manifest with default values. The manifest URL should be non-empty.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, EmptyManifest) {
  GURL test_url =
      embedded_test_server()->GetURL("/manifest/empty-manifest.html");

  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  GetManifestAndWait();
  EXPECT_FALSE(blink::IsEmptyManifest(manifest()));
  EXPECT_FALSE(manifest_url().is_empty());
  ASSERT_EQ(test_url.GetWithoutFilename(), manifest().scope);
  EXPECT_EQ(0, GetConsoleErrorCount());
  ASSERT_EQ(1u, reported_manifest_urls().size());
  EXPECT_EQ(manifest_url(), reported_manifest_urls()[0]);
  ASSERT_EQ(1u, manifests_reported_when_favicon_url_updated().size());
  EXPECT_EQ(1u, manifests_reported_when_favicon_url_updated()[0]);
}

// If a page's manifest can't be parsed correctly, requesting the manifest
// should return an empty manifest. The manifest URL should be non-empty.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, ParseErrorManifest) {
  GURL test_url =
      embedded_test_server()->GetURL("/manifest/parse-error-manifest.html");

  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  GetManifestAndWait();
  EXPECT_FALSE(blink::IsEmptyManifest(manifest()));
  EXPECT_TRUE(blink::IsDefaultManifest(manifest(), test_url));
  EXPECT_FALSE(manifest_url().is_empty());
  EXPECT_EQ(1, GetConsoleErrorCount());
  ASSERT_EQ(1u, reported_manifest_urls().size());
  EXPECT_EQ(manifest_url(), reported_manifest_urls()[0]);
  ASSERT_EQ(1u, manifests_reported_when_favicon_url_updated().size());
  EXPECT_EQ(1u, manifests_reported_when_favicon_url_updated()[0]);
}

// If a page has a manifest that can be fetched and parsed, requesting the
// manifest should return a properly filled manifest. The manifest URL should be
// non-empty.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, SampleManifest) {
  GURL test_url =
      embedded_test_server()->GetURL("/manifest/sample-manifest.html");

  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  GetManifestAndWait();
  EXPECT_FALSE(blink::IsEmptyManifest(manifest()));
  EXPECT_FALSE(manifest_url().is_empty());

  EXPECT_EQ(0, GetConsoleErrorCount());
  ASSERT_EQ(1u, reported_manifest_urls().size());
  EXPECT_EQ(manifest_url(), reported_manifest_urls()[0]);
  ASSERT_EQ(1u, manifests_reported_when_favicon_url_updated().size());
  EXPECT_EQ(1u, manifests_reported_when_favicon_url_updated()[0]);
}

// If a page changes manifest during its life-time, requesting the manifest
// should return the current manifest.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, DynamicManifest) {
  GURL test_url =
      embedded_test_server()->GetURL("/manifest/dynamic-manifest.html");

  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  std::vector<GURL> expected_manifest_urls;

  {
    GetManifestAndWait();
    EXPECT_FALSE(blink::IsEmptyManifest(manifest()));
    EXPECT_TRUE(blink::IsDefaultManifest(manifest(), test_url));
    EXPECT_TRUE(manifest_url().is_empty());
    EXPECT_TRUE(reported_manifest_urls().empty());
  }

  {
    std::string manifest_link =
        embedded_test_server()->GetURL("/manifest/sample-manifest.json").spec();
    ASSERT_TRUE(ExecJs(shell(), "setManifestTo('" + manifest_link + "')"));

    GetManifestAndWait();
    EXPECT_FALSE(blink::IsEmptyManifest(manifest()));
    EXPECT_FALSE(blink::IsDefaultManifest(manifest(), test_url));
    EXPECT_FALSE(manifest_url().is_empty());
    expected_manifest_urls.push_back(manifest_url());
    EXPECT_EQ(expected_manifest_urls, reported_manifest_urls());
  }
  {
    std::string manifest_link =
        embedded_test_server()->GetURL("/manifest/empty-manifest.json").spec();
    ASSERT_TRUE(ExecJs(shell(), "setManifestTo('" + manifest_link + "')"));

    GetManifestAndWait();
    EXPECT_FALSE(blink::IsEmptyManifest(manifest()));
    EXPECT_FALSE(manifest_url().is_empty());
    expected_manifest_urls.push_back(manifest_url());
    EXPECT_EQ(expected_manifest_urls, reported_manifest_urls());
  }

  {
    ASSERT_TRUE(ExecJs(shell(), "clearManifest()"));

    GetManifestAndWait();
    // There is always a default manifest.
    EXPECT_FALSE(blink::IsEmptyManifest(manifest()));
    EXPECT_TRUE(blink::IsDefaultManifest(manifest(), test_url));
    EXPECT_TRUE(manifest_url().is_empty());
    expected_manifest_urls.push_back(manifest_url());
    EXPECT_EQ(expected_manifest_urls, reported_manifest_urls());
    ASSERT_EQ(1u, manifests_reported_when_favicon_url_updated().size());
    EXPECT_EQ(0u, manifests_reported_when_favicon_url_updated()[0]);
  }

  EXPECT_EQ(0, GetConsoleErrorCount());
}

// This page has a manifest with only file handlers specified. Asking
// for just the manifest should succeed with a non empty manifest.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, FileHandlerManifest) {
  GURL test_url =
      embedded_test_server()->GetURL("/manifest/file-handler-manifest.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  GetManifestAndWait();
  EXPECT_FALSE(blink::IsEmptyManifest(manifest()));
  EXPECT_FALSE(manifest_url().is_empty());
  EXPECT_FALSE(manifest().file_handlers.empty());
  EXPECT_EQ(0, GetConsoleErrorCount());
  ASSERT_EQ(1u, reported_manifest_urls().size());
  EXPECT_EQ(manifest_url(), reported_manifest_urls()[0]);
}

// If a page's manifest lives in a different origin, it should follow the CORS
// rules and requesting the manifest should return an empty manifest (unless the
// response contains CORS headers).
// Flaky: crbug.com/1122546
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, DISABLED_CorsManifest) {
  ASSERT_TRUE(cors_embedded_test_server()->Start());
  ASSERT_NE(embedded_test_server()->port(),
            cors_embedded_test_server()->port());

  GURL test_url =
      embedded_test_server()->GetURL("/manifest/dynamic-manifest.html");
  std::vector<GURL> expected_manifest_urls;

  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  std::string manifest_link = cors_embedded_test_server()
                                  ->GetURL("/manifest/sample-manifest.json")
                                  .spec();
  ASSERT_TRUE(ExecJs(shell(), "setManifestTo('" + manifest_link + "')"));

  GetManifestAndWait();
  EXPECT_FALSE(blink::IsEmptyManifest(manifest()));
  EXPECT_TRUE(blink::IsDefaultManifest(manifest(), test_url));
  EXPECT_FALSE(manifest_url().is_empty());
  EXPECT_THAT(console_errors(), Contains(HasSubstr("CORS")));
  EXPECT_EQ(1, GetConsoleErrorCount());
  expected_manifest_urls.push_back(manifest_url());
  EXPECT_EQ(expected_manifest_urls, reported_manifest_urls());

  // The purpose of this second load is to make sure the first load is fully
  // finished. The first load will fail because of Access Control error but the
  // underlying Blink loader will continue fetching the file. There is no
  // reliable way to know when the fetch is finished from the browser test
  // except by fetching the same file from same origin, making it succeed when
  // it is actually fully loaded.
  manifest_link =
      embedded_test_server()->GetURL("/manifest/sample-manifest.json").spec();
  ASSERT_TRUE(ExecJs(shell(), "setManifestTo('" + manifest_link + "')"));
  GetManifestAndWait();
  expected_manifest_urls.push_back(manifest_url());
  EXPECT_EQ(expected_manifest_urls, reported_manifest_urls());
  ASSERT_EQ(1u, manifests_reported_when_favicon_url_updated().size());
  EXPECT_EQ(0u, manifests_reported_when_favicon_url_updated()[0]);
}

// If a page's manifest lives in a different origin, it should be accessible if
// it has valid access controls headers.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, CorsManifestWithAcessControls) {
  ASSERT_TRUE(cors_embedded_test_server()->Start());
  ASSERT_NE(embedded_test_server()->port(),
            cors_embedded_test_server()->port());

  GURL test_url =
      embedded_test_server()->GetURL("/manifest/dynamic-manifest.html");

  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  std::string manifest_link = cors_embedded_test_server()
                                  ->GetURL("/manifest/manifest-cors.json")
                                  .spec();
  ASSERT_TRUE(ExecJs(shell(), "setManifestTo('" + manifest_link + "')"));

  GetManifestAndWait();
  EXPECT_FALSE(blink::IsEmptyManifest(manifest()));
  EXPECT_FALSE(manifest_url().is_empty());
  EXPECT_EQ(0, GetConsoleErrorCount());
  ASSERT_EQ(1u, reported_manifest_urls().size());
  EXPECT_EQ(manifest_url(), reported_manifest_urls()[0]);
  ASSERT_EQ(1u, manifests_reported_when_favicon_url_updated().size());
  EXPECT_EQ(0u, manifests_reported_when_favicon_url_updated()[0]);
}

// If a page's manifest is in an insecure origin while the page is in a secure
// origin, requesting the manifest should return the empty manifest.
// TODO(crbug.com/40742592): Flaky test.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, DISABLED_MixedContentManifest) {
  ASSERT_TRUE(cors_embedded_test_server()->Start());
  std::unique_ptr<net::EmbeddedTestServer> https_server(
      new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
  https_server->ServeFilesFromSourceDirectory(GetTestDataFilePath());

  ASSERT_TRUE(https_server->Start());

  GURL test_url = https_server->GetURL("/manifest/dynamic-manifest.html");

  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  GURL manifest_link = cors_embedded_test_server()->GetURL(
      "insecure.example", "/manifest/manifest-cors.json");
  // Ensure the manifest really is mixed content:
  ASSERT_FALSE(network::IsUrlPotentiallyTrustworthy(manifest_link));
  ASSERT_TRUE(ExecJs(shell(), JsReplace("setManifestTo($1)", manifest_link)));

  GetManifestAndWait();
  EXPECT_FALSE(blink::IsEmptyManifest(manifest()));
  EXPECT_TRUE(blink::IsDefaultManifest(manifest(), test_url));
  EXPECT_FALSE(manifest_url().is_empty());
  EXPECT_THAT(console_errors(), Contains(HasSubstr("Mixed Content")));
  ASSERT_EQ(1u, reported_manifest_urls().size());
  EXPECT_EQ(manifest_url(), reported_manifest_urls()[0]);
  ASSERT_EQ(1u, manifests_reported_when_favicon_url_updated().size());
  EXPECT_EQ(0u, manifests_reported_when_favicon_url_updated()[0]);
}

// If a page's manifest has some parsing errors, they should show up in the
// developer console.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, ParsingErrorsManifest) {
  GURL test_url =
      embedded_test_server()->GetURL("/manifest/parsing-errors.html");

  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  GetManifestAndWait();
  EXPECT_FALSE(blink::IsEmptyManifest(manifest()));
  EXPECT_FALSE(manifest_url().is_empty());
  ASSERT_EQ(test_url.GetWithoutFilename(), manifest().scope);
  EXPECT_EQ(7, GetConsoleErrorCount());
  ASSERT_EQ(1u, reported_manifest_urls().size());
  EXPECT_EQ(manifest_url(), reported_manifest_urls()[0]);
  ASSERT_EQ(1u, manifests_reported_when_favicon_url_updated().size());
  EXPECT_EQ(1u, manifests_reported_when_favicon_url_updated()[0]);
}

// If a page has a manifest and the page is navigated to a page without a
// manifest, the page's manifest should be updated.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, Navigation) {
  std::vector<GURL> expected_manifest_urls;
  {
    GURL test_url =
        embedded_test_server()->GetURL("/manifest/sample-manifest.html");

    ASSERT_TRUE(NavigateToURL(shell(), test_url));

    GetManifestAndWait();
    EXPECT_FALSE(blink::IsEmptyManifest(manifest()));
    EXPECT_FALSE(manifest_url().is_empty());
    EXPECT_EQ(0, GetConsoleErrorCount());
    expected_manifest_urls.push_back(manifest_url());
    EXPECT_EQ(expected_manifest_urls, reported_manifest_urls());
    ASSERT_EQ(1u, manifests_reported_when_favicon_url_updated().size());
    EXPECT_EQ(1u, manifests_reported_when_favicon_url_updated()[0]);
  }

  {
    GURL test_url =
        embedded_test_server()->GetURL("/manifest/no-manifest.html");

    ASSERT_TRUE(NavigateToURL(shell(), test_url));

    GetManifestAndWait();
    EXPECT_TRUE(blink::IsDefaultManifest(manifest(), test_url));
    EXPECT_FALSE(blink::IsEmptyManifest(manifest()));
    EXPECT_EQ(0, GetConsoleErrorCount());
    EXPECT_TRUE(manifest_url().is_empty());
    EXPECT_EQ(expected_manifest_urls, reported_manifest_urls());
    ASSERT_EQ(2u, manifests_reported_when_favicon_url_updated().size());
    EXPECT_EQ(1u, manifests_reported_when_favicon_url_updated()[1]);
  }

  {
    GURL test_url =
        embedded_test_server()->GetURL("/manifest/sample-manifest.html");

    ASSERT_TRUE(NavigateToURL(shell(), test_url));

    GetManifestAndWait();
    EXPECT_FALSE(blink::IsEmptyManifest(manifest()));
    EXPECT_FALSE(manifest_url().is_empty());
    EXPECT_EQ(0, GetConsoleErrorCount());
    expected_manifest_urls.push_back(manifest_url());
    EXPECT_EQ(expected_manifest_urls, reported_manifest_urls());
    ASSERT_EQ(3u, manifests_reported_when_favicon_url_updated().size());
    EXPECT_EQ(2u, manifests_reported_when_favicon_url_updated()[2]);
  }
}

// If a page has a manifest and the page is navigated using pushState (ie. same
// page), it should keep its manifest state.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, PushStateNavigation) {
  GURL test_url =
      embedded_test_server()->GetURL("/manifest/sample-manifest.html");

  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  {
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    ASSERT_TRUE(ExecJs(
        shell(), "history.pushState({foo: \"bar\"}, 'page', 'page.html');"));
    navigation_observer.Wait();
  }

  GetManifestAndWait();
  EXPECT_FALSE(blink::IsEmptyManifest(manifest()));
  EXPECT_FALSE(manifest_url().is_empty());
  EXPECT_EQ(0, GetConsoleErrorCount());
  ASSERT_EQ(1u, reported_manifest_urls().size());
  EXPECT_EQ(manifest_url(), reported_manifest_urls()[0]);
  ASSERT_EQ(2u, manifests_reported_when_favicon_url_updated().size());
  EXPECT_EQ(1u, manifests_reported_when_favicon_url_updated()[0]);
  EXPECT_EQ(1u, manifests_reported_when_favicon_url_updated()[1]);
}

// If a page has a manifest and is navigated using an anchor (ie. same page), it
// should keep its manifest state.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, AnchorNavigation) {
  GURL test_url =
      embedded_test_server()->GetURL("/manifest/sample-manifest.html");

  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  {
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    ASSERT_TRUE(ExecJs(shell(),
                       "var a = document.createElement('a'); a.href='#foo';"
                       "document.body.appendChild(a); a.click();"));
    navigation_observer.Wait();
  }

  GetManifestAndWait();
  EXPECT_FALSE(blink::IsEmptyManifest(manifest()));
  EXPECT_FALSE(manifest_url().is_empty());
  EXPECT_EQ(0, GetConsoleErrorCount());
  ASSERT_EQ(1u, reported_manifest_urls().size());
  EXPECT_EQ(manifest_url(), reported_manifest_urls()[0]);
  ASSERT_EQ(2u, manifests_reported_when_favicon_url_updated().size());
  EXPECT_EQ(1u, manifests_reported_when_favicon_url_updated()[0]);
  EXPECT_EQ(1u, manifests_reported_when_favicon_url_updated()[1]);
}

namespace {

std::unique_ptr<net::test_server::HttpResponse> CustomHandleRequestForCookies(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url == "/index.html") {
    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse());
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/html");
    http_response->set_content(
        "<html><head>"
        "<link rel=manifest crossorigin='use-credentials' href=/manifest.json>"
        "</head></html>");
    return std::move(http_response);
  }

  const auto& iter = request.headers.find("Cookie");
  if (iter == request.headers.end() ||
      request.relative_url != "/manifest.json") {
    return nullptr;
  }

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse());
  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("application/json");
  http_response->set_content(
      base::StringPrintf("{\"name\": \"%s\"}", iter->second.c_str()));

  return std::move(http_response);
}

}  // anonymous namespace

// This tests that when fetching a Manifest with 'use-credentials' set, the
// cookies associated with it are passed along the request.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, UseCredentialsSendCookies) {
  std::unique_ptr<net::EmbeddedTestServer> custom_embedded_test_server(
      new net::EmbeddedTestServer());
  custom_embedded_test_server->RegisterRequestHandler(
      base::BindRepeating(&CustomHandleRequestForCookies));

  ASSERT_TRUE(custom_embedded_test_server->Start());

  ASSERT_TRUE(SetCookie(shell()->web_contents()->GetBrowserContext(),
                        custom_embedded_test_server->base_url(), "foobar"));

  ASSERT_TRUE(NavigateToURL(
      shell(), custom_embedded_test_server->GetURL("/index.html")));

  GetManifestAndWait();
  EXPECT_FALSE(blink::IsEmptyManifest(manifest()));
  EXPECT_FALSE(manifest_url().is_empty());
  EXPECT_EQ(0, GetConsoleErrorCount());
  ASSERT_EQ(1u, reported_manifest_urls().size());
  EXPECT_EQ(manifest_url(), reported_manifest_urls()[0]);
  ASSERT_EQ(1u, manifests_reported_when_favicon_url_updated().size());
  EXPECT_EQ(1u, manifests_reported_when_favicon_url_updated()[0]);

  // The custom embedded test server will fill the name field with the cookie
  // content.
  EXPECT_EQ(u"foobar", manifest().name);
}

namespace {

std::unique_ptr<net::test_server::HttpResponse> CustomHandleRequestForNoCookies(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url == "/index.html") {
    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse());
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/html");
    http_response->set_content(
        "<html><head><link rel=manifest href=/manifest.json></head></html>");
    return std::move(http_response);
  }

  const auto& iter = request.headers.find("Cookie");
  if (iter != request.headers.end() ||
      request.relative_url != "/manifest.json") {
    return nullptr;
  }

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse());
  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("application/json");
  http_response->set_content("{\"name\": \"no cookies\"}");

  return std::move(http_response);
}

}  // anonymous namespace

// This tests that when fetching a Manifest without 'use-credentials' set, the
// cookies associated with it are not passed along the request.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, NoUseCredentialsNoCookies) {
  std::unique_ptr<net::EmbeddedTestServer> custom_embedded_test_server(
      new net::EmbeddedTestServer());
  custom_embedded_test_server->RegisterRequestHandler(
      base::BindRepeating(&CustomHandleRequestForNoCookies));

  ASSERT_TRUE(custom_embedded_test_server->Start());

  ASSERT_TRUE(SetCookie(shell()->web_contents()->GetBrowserContext(),
                        custom_embedded_test_server->base_url(), "foobar"));

  ASSERT_TRUE(NavigateToURL(
      shell(), custom_embedded_test_server->GetURL("/index.html")));

  GetManifestAndWait();
  EXPECT_FALSE(blink::IsEmptyManifest(manifest()));
  EXPECT_FALSE(manifest_url().is_empty());
  EXPECT_EQ(0, GetConsoleErrorCount());
  ASSERT_EQ(1u, reported_manifest_urls().size());
  EXPECT_EQ(manifest_url(), reported_manifest_urls()[0]);
  ASSERT_EQ(1u, manifests_reported_when_favicon_url_updated().size());
  EXPECT_EQ(1u, manifests_reported_when_favicon_url_updated()[0]);

  // The custom embedded test server will fill set the name to 'no cookies' if
  // it did not find cookies.
  EXPECT_EQ(u"no cookies", manifest().name);
}

// This tests that fetching a Manifest from a unique origin always fails,
// regardless of the CORS headers on the manifest. It also tests that no
// manifest change notifications are reported when the origin is unique.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, UniqueOrigin) {
  GURL test_url = embedded_test_server()->GetURL("/manifest/sandboxed.html");

  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  std::string manifest_link =
      embedded_test_server()->GetURL("/manifest/sample-manifest.json").spec();
  ASSERT_TRUE(ExecJs(shell(), "setManifestTo('" + manifest_link + "')"));

  // Same-origin manifest will not be fetched from a unique origin, regardless
  // of CORS headers. Manifest URL is still returned though.
  GetManifestAndWait();
  EXPECT_TRUE(blink::IsEmptyManifest(manifest()));
  EXPECT_FALSE(manifest_url().is_empty());
  EXPECT_EQ(0, GetConsoleErrorCount());
  EXPECT_EQ(0u, reported_manifest_urls().size());

  manifest_link =
      embedded_test_server()->GetURL("/manifest/manifest-cors.json").spec();
  ASSERT_TRUE(ExecJs(shell(), "setManifestTo('" + manifest_link + "')"));

  GetManifestAndWait();
  EXPECT_TRUE(blink::IsEmptyManifest(manifest()));
  EXPECT_FALSE(manifest_url().is_empty());
  EXPECT_EQ(0, GetConsoleErrorCount());
  EXPECT_EQ(0u, reported_manifest_urls().size());
}

// This is testing the crash scenario encountered by https://crbug.com/1369363.
// In it a GetManifest() request by WebAppInstallTask was interrupted by a page
// navigation which destructed the internal ManifestManagerHost and forced the
// GetManifest() callback to be invoked during the destruction stack frame. The
// callback, which considered empty manifests valid for proceeding, proceeded to
// read other data on the (not destroyed) WebContents that were also in the
// middle of destruction and triggered a UAF crash.
//
// This test checks that the callback does not get invoked during the
// ManifestManagerHost destruction stack frame and other fields of the
// WebContents are still valid to synchronously access by the callback.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest,
                       GetManifestInterruptedByDestruction) {
  // Attempting to fetch the manifest on this page will hang forever, giving
  // this test time to interrupt the manifest request with the destruction of
  // ManifestManagerHost.
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/manifest/hung-manifest.html")));

  base::RunLoop run_loop;
  WebContents* web_contents = shell()->web_contents();
  web_contents->GetPrimaryPage().GetManifest(base::BindLambdaForTesting(
      [&](blink::mojom::ManifestRequestResult, const GURL& url,
          blink::mojom::ManifestPtr manifest) {
        EXPECT_TRUE(url.is_empty());
        EXPECT_TRUE(blink::IsEmptyManifest(manifest));

        // Accessing fields on the web_contents at this point in time should be
        // safe.
        std::ignore = web_contents->GetFaviconURLs().empty();

        run_loop.Quit();
      }));

  // Unchecked downcast to get access to the
  // ReinitializeDocumentAssociatedDataForTesting() method. This method was not
  // put on the base class to avoid polluting the public interface with
  // implementation details only used by this test.
  auto* render_frame_host_impl =
      static_cast<RenderFrameHostImpl*>(web_contents->GetPrimaryMainFrame());

  // Resetting the DocumentAssociatedData, which owns ManifestManagerHost and
  // the pending GetManifest() callback, forces the callback to be invoked.
  // This is intended to reproduce the effects of a page navigation seen in
  // https://crbug.com/1369363, this shortcut is used as it was too difficult to
  // determine the exact sequence of JavaScript commands needed to cause the
  // page navigation to trigger
  // RenderFrameHostImpl::DidCommitNavigationInternal() (which reassigns the
  // DocumentAssociatedData that owns ManifestManagerHost) without first
  // triggering the callback safely as the in flight network request was
  // cancelled.
  render_frame_host_impl->ReinitializeDocumentAssociatedDataForTesting();

  run_loop.Run();
}

class ManifestBrowserPrerenderingTest : public ManifestBrowserTest {
 public:
  ManifestBrowserPrerenderingTest()
      : prerender_helper_(
            base::BindRepeating(&ManifestBrowserPrerenderingTest::web_contents,
                                base::Unretained(this))) {}

  ~ManifestBrowserPrerenderingTest() override = default;

 protected:
  test::PrerenderTestHelper& prerender_helper() { return prerender_helper_; }

 private:
  test::PrerenderTestHelper prerender_helper_;
};

// Tests that GetManifest() returns an empty manifest if it's requested in
// prerendering.
IN_PROC_BROWSER_TEST_F(ManifestBrowserPrerenderingTest,
                       GetManifestInPrerendering) {
  GURL test_url =
      embedded_test_server()->GetURL("/manifest/empty-manifest.html");

  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  {
    base::RunLoop run_loop;
    web_contents()->GetPrimaryPage().GetManifest(base::BindLambdaForTesting(
        [&](blink::mojom::ManifestRequestResult, const GURL& manifest_url,
            blink::mojom::ManifestPtr manifest) {
          // Get the manifest on a primary page.
          EXPECT_FALSE(manifest_url.is_empty());
          EXPECT_FALSE(blink::IsEmptyManifest(*manifest));
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  GURL prerender_url =
      embedded_test_server()->GetURL("/manifest/sample-manifest.html");
  // Loads a page in the prerender.
  FrameTreeNodeId host_id = prerender_helper().AddPrerender(prerender_url);
  content::RenderFrameHost* prerender_rfh =
      prerender_helper().GetPrerenderedMainFrameHost(host_id);
  {
    base::RunLoop run_loop;
    prerender_rfh->GetPage().GetManifest(base::BindLambdaForTesting(
        [&](blink::mojom::ManifestRequestResult, const GURL& manifest_url,
            blink::mojom::ManifestPtr manifest) {
          // Ensure that the manifest is empty in prerendering.
          EXPECT_TRUE(manifest_url.is_empty());
          EXPECT_TRUE(blink::IsEmptyManifest(*manifest));
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  prerender_helper().NavigatePrimaryPage(prerender_url);
  {
    base::RunLoop run_loop;
    prerender_rfh->GetPage().GetManifest(base::BindLambdaForTesting(
        [&](blink::mojom::ManifestRequestResult, const GURL& manifest_url,
            blink::mojom::ManifestPtr manifest) {
          // Ensure that getting the manifest works after prerendering
          // activation.
          EXPECT_FALSE(manifest_url.is_empty());
          EXPECT_FALSE(blink::IsEmptyManifest(*manifest));
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

class ManifestFencedFrameBrowserTest : public ManifestBrowserTest {
 public:
  ManifestFencedFrameBrowserTest() = default;
  ~ManifestFencedFrameBrowserTest() override = default;

 protected:
  test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

 private:
  test::FencedFrameTestHelper fenced_frame_test_helper_;
};

// Tests that GetManifest() returns an empty manifest if it's requested in
// a fenced frame.
IN_PROC_BROWSER_TEST_F(ManifestFencedFrameBrowserTest,
                       GetManifestInFencedFrame) {
  const GURL test_url =
      embedded_test_server()->GetURL("/manifest/empty-manifest.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  const GURL fenced_frame_url =
      embedded_test_server()->GetURL("/fenced_frames/title1.html");

  content::RenderFrameHost* fenced_frame_rfh =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(), fenced_frame_url);

  // Add a manifest to `fenced_frame_rfh`.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh,
                     R"( var link = document.createElement('link');
                         link.rel = 'manifest';
                         link.href = '../manifest/sample-manifest.json';
                         document.head.appendChild(link);)"));

  base::RunLoop run_loop;
  fenced_frame_rfh->GetPage().GetManifest(base::BindLambdaForTesting(
      [&](blink::mojom::ManifestRequestResult, const GURL& manifest_url,
          blink::mojom::ManifestPtr manifest) {
        // Even though `fenced_frame_rfh` has a manifest updated above,
        // this should get an empty manifest since it's not a primary main
        // frame.
        EXPECT_TRUE(manifest_url.is_empty());
        EXPECT_TRUE(blink::IsEmptyManifest(*manifest));
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace content
