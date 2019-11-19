// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/manifest_manager.mojom.h"

namespace content {

class ManifestBrowserTest;

// Mock of a WebContentsDelegate that catches messages sent to the console.
class MockWebContentsDelegate : public WebContentsDelegate {
 public:
  MockWebContentsDelegate(WebContents* web_contents, ManifestBrowserTest* test)
      : web_contents_(web_contents),
        test_(test) {
  }

  bool DidAddMessageToConsole(WebContents* source,
                              blink::mojom::ConsoleMessageLevel log_level,
                              const base::string16& message,
                              int32_t line_no,
                              const base::string16& source_id) override;

 private:
  WebContents* web_contents_;
  ManifestBrowserTest* test_;
};

class ManifestBrowserTest : public ContentBrowserTest,
                            public WebContentsObserver {
 protected:
  friend MockWebContentsDelegate;

  ManifestBrowserTest() : console_error_count_(0) {
    cors_embedded_test_server_.reset(new net::EmbeddedTestServer);
    cors_embedded_test_server_->ServeFilesFromSourceDirectory(
        GetTestDataFilePath());
  }

  ~ManifestBrowserTest() override {}

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    DCHECK(shell()->web_contents());

    mock_web_contents_delegate_.reset(
        new MockWebContentsDelegate(shell()->web_contents(), this));
    shell()->web_contents()->SetDelegate(mock_web_contents_delegate_.get());
    Observe(shell()->web_contents());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void GetManifestAndWait() {
    shell()->web_contents()->GetManifest(base::BindOnce(
        &ManifestBrowserTest::OnGetManifest, base::Unretained(this)));

    message_loop_runner_ = new MessageLoopRunner();
    message_loop_runner_->Run();
  }

  void OnGetManifest(const GURL& manifest_url,
                     const blink::Manifest& manifest) {
    manifest_url_ = manifest_url;
    manifest_ = manifest;
    message_loop_runner_->Quit();
  }

  const blink::Manifest& manifest() const { return manifest_; }

  const GURL& manifest_url() const {
    return manifest_url_;
  }

  int GetConsoleErrorCount() const {
    // The IPCs reporting console errors are not FIFO with the manifest IPCs.
    // Waiting for a round-trip channel-associated message will wait until any
    // already enqueued channel-associated IPCs arrive at the browser process.
    mojo::AssociatedRemote<blink::mojom::ManifestManager> remote;
    shell()
        ->web_contents()
        ->GetMainFrame()
        ->GetRemoteAssociatedInterfaces()
        ->GetInterface(&remote);
    remote.FlushForTesting();
    return console_error_count_;
  }

  void OnReceivedConsoleError() {
    console_error_count_++;
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
  void DidUpdateFaviconURL(const std::vector<FaviconURL>& candidates) override {
    manifests_reported_when_favicon_url_updated_.push_back(
        reported_manifest_urls_.size());
  }

  void DidUpdateWebManifestURL(
      const base::Optional<GURL>& manifest_url) override {
    if (!manifest_url) {
      reported_manifest_urls_.emplace_back();
      return;
    }
    EXPECT_FALSE(manifest_url->is_empty());
    EXPECT_TRUE(manifest_url->is_valid());
    reported_manifest_urls_.push_back(*manifest_url);
  }

 private:
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
  std::unique_ptr<MockWebContentsDelegate> mock_web_contents_delegate_;
  std::unique_ptr<net::EmbeddedTestServer> cors_embedded_test_server_;
  GURL manifest_url_;
  blink::Manifest manifest_;
  int console_error_count_;
  std::vector<GURL> reported_manifest_urls_;
  std::vector<size_t> manifests_reported_when_favicon_url_updated_;

  DISALLOW_COPY_AND_ASSIGN(ManifestBrowserTest);
};

// The implementation of DidAddMessageToConsole isn't inlined because it needs
// to know about |test_|.
bool MockWebContentsDelegate::DidAddMessageToConsole(
    WebContents* source,
    blink::mojom::ConsoleMessageLevel log_level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  DCHECK(source == web_contents_);

  if (log_level == blink::mojom::ConsoleMessageLevel::kError ||
      log_level == blink::mojom::ConsoleMessageLevel::kWarning)
    test_->OnReceivedConsoleError();
  return false;
}

// If a page has no manifest, requesting a manifest should return the empty
// manifest. The URL should be empty.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, NoManifest) {
  GURL test_url = embedded_test_server()->GetURL("/manifest/no-manifest.html");

  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  GetManifestAndWait();
  EXPECT_TRUE(manifest().IsEmpty());
  EXPECT_TRUE(manifest_url().is_empty());
  EXPECT_EQ(0, GetConsoleErrorCount());
  EXPECT_TRUE(reported_manifest_urls().empty());
  ASSERT_EQ(1u, manifests_reported_when_favicon_url_updated().size());
  EXPECT_EQ(0u, manifests_reported_when_favicon_url_updated()[0]);
}

// If a page manifest points to a 404 URL, requesting the manifest should return
// the empty manifest. However, the manifest URL will be non-empty.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, 404Manifest) {
  GURL test_url = GetTestUrl("manifest", "404-manifest.html");

  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  GetManifestAndWait();
  EXPECT_TRUE(manifest().IsEmpty());
  EXPECT_FALSE(manifest_url().is_empty());
  EXPECT_EQ(0, GetConsoleErrorCount());
  ASSERT_EQ(1u, reported_manifest_urls().size());
  EXPECT_EQ(manifest_url(), reported_manifest_urls()[0]);
  EXPECT_EQ(0u, manifests_reported_when_favicon_url_updated().size());
}

// If a page has an empty manifest, requesting the manifest should return the
// manifest with default values. The manifest URL should be non-empty.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, EmptyManifest) {
  GURL test_url =
      embedded_test_server()->GetURL("/manifest/empty-manifest.html");

  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  GetManifestAndWait();
  EXPECT_FALSE(manifest().IsEmpty());
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
  EXPECT_TRUE(manifest().IsEmpty());
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
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, DummyManifest) {
  GURL test_url =
      embedded_test_server()->GetURL("/manifest/dummy-manifest.html");

  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  GetManifestAndWait();
  EXPECT_FALSE(manifest().IsEmpty());
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
    EXPECT_TRUE(manifest().IsEmpty());
    EXPECT_TRUE(manifest_url().is_empty());
    EXPECT_TRUE(reported_manifest_urls().empty());
  }

  {
    std::string manifest_link =
        embedded_test_server()->GetURL("/manifest/dummy-manifest.json").spec();
    ASSERT_TRUE(
        ExecuteScript(shell(), "setManifestTo('" + manifest_link + "')"));

    GetManifestAndWait();
    EXPECT_FALSE(manifest().IsEmpty());
    EXPECT_FALSE(manifest_url().is_empty());
    expected_manifest_urls.push_back(manifest_url());
    EXPECT_EQ(expected_manifest_urls, reported_manifest_urls());
  }
  {
    std::string manifest_link =
        embedded_test_server()->GetURL("/manifest/empty-manifest.json").spec();
    ASSERT_TRUE(
        ExecuteScript(shell(), "setManifestTo('" + manifest_link + "')"));

    GetManifestAndWait();
    EXPECT_FALSE(manifest().IsEmpty());
    EXPECT_FALSE(manifest_url().is_empty());
    expected_manifest_urls.push_back(manifest_url());
    EXPECT_EQ(expected_manifest_urls, reported_manifest_urls());
  }

  {
    ASSERT_TRUE(ExecuteScript(shell(), "clearManifest()"));

    GetManifestAndWait();
    EXPECT_TRUE(manifest().IsEmpty());
    EXPECT_TRUE(manifest_url().is_empty());
    expected_manifest_urls.push_back(manifest_url());
    EXPECT_EQ(expected_manifest_urls, reported_manifest_urls());
    ASSERT_EQ(1u, manifests_reported_when_favicon_url_updated().size());
    EXPECT_EQ(0u, manifests_reported_when_favicon_url_updated()[0]);
  }

  EXPECT_EQ(0, GetConsoleErrorCount());
}

// If a page's manifest lives in a different origin, it should follow the CORS
// rules and requesting the manifest should return an empty manifest (unless the
// response contains CORS headers).
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, CorsManifest) {
  ASSERT_TRUE(cors_embedded_test_server()->Start());
  ASSERT_NE(embedded_test_server()->port(),
            cors_embedded_test_server()->port());

  GURL test_url =
      embedded_test_server()->GetURL("/manifest/dynamic-manifest.html");
  std::vector<GURL> expected_manifest_urls;

  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  std::string manifest_link = cors_embedded_test_server()->GetURL(
      "/manifest/dummy-manifest.json").spec();
  ASSERT_TRUE(ExecuteScript(shell(), "setManifestTo('" + manifest_link + "')"));

  GetManifestAndWait();
  EXPECT_TRUE(manifest().IsEmpty());
  EXPECT_FALSE(manifest_url().is_empty());
  // 1 error for CORS violation
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
      embedded_test_server()->GetURL("/manifest/dummy-manifest.json").spec();
  ASSERT_TRUE(ExecuteScript(shell(), "setManifestTo('" + manifest_link + "')"));
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

  std::string manifest_link = cors_embedded_test_server()->GetURL(
      "/manifest/manifest-cors.json").spec();
  ASSERT_TRUE(ExecuteScript(shell(), "setManifestTo('" + manifest_link + "')"));

  GetManifestAndWait();
  EXPECT_FALSE(manifest().IsEmpty());
  EXPECT_FALSE(manifest_url().is_empty());
  EXPECT_EQ(0, GetConsoleErrorCount());
  ASSERT_EQ(1u, reported_manifest_urls().size());
  EXPECT_EQ(manifest_url(), reported_manifest_urls()[0]);
  ASSERT_EQ(1u, manifests_reported_when_favicon_url_updated().size());
  EXPECT_EQ(0u, manifests_reported_when_favicon_url_updated()[0]);
}

// If a page's manifest is in an insecure origin while the page is in a secure
// origin, requesting the manifest should return the empty manifest.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, MixedContentManifest) {
  std::unique_ptr<net::EmbeddedTestServer> https_server(
      new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
  https_server->ServeFilesFromSourceDirectory(GetTestDataFilePath());

  ASSERT_TRUE(https_server->Start());

  GURL test_url =
      embedded_test_server()->GetURL("/manifest/dynamic-manifest.html");

  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  std::string manifest_link =
      https_server->GetURL("/manifest/dummy-manifest.json").spec();
  ASSERT_TRUE(ExecuteScript(shell(), "setManifestTo('" + manifest_link + "')"));

  GetManifestAndWait();
  EXPECT_TRUE(manifest().IsEmpty());
  EXPECT_FALSE(manifest_url().is_empty());
  // 1 error for mixed-content check violation
  EXPECT_EQ(1, GetConsoleErrorCount());
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
  EXPECT_FALSE(manifest().IsEmpty());
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
        embedded_test_server()->GetURL("/manifest/dummy-manifest.html");

    ASSERT_TRUE(NavigateToURL(shell(), test_url));

    GetManifestAndWait();
    EXPECT_FALSE(manifest().IsEmpty());
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
    EXPECT_TRUE(manifest().IsEmpty());
    EXPECT_EQ(0, GetConsoleErrorCount());
    EXPECT_TRUE(manifest_url().is_empty());
    EXPECT_EQ(expected_manifest_urls, reported_manifest_urls());
    ASSERT_EQ(2u, manifests_reported_when_favicon_url_updated().size());
    EXPECT_EQ(1u, manifests_reported_when_favicon_url_updated()[1]);
  }

  {
    GURL test_url =
        embedded_test_server()->GetURL("/manifest/dummy-manifest.html");

    ASSERT_TRUE(NavigateToURL(shell(), test_url));

    GetManifestAndWait();
    EXPECT_FALSE(manifest().IsEmpty());
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
      embedded_test_server()->GetURL("/manifest/dummy-manifest.html");

  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  {
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    ASSERT_TRUE(ExecuteScript(
        shell(), "history.pushState({foo: \"bar\"}, 'page', 'page.html');"));
    navigation_observer.Wait();
  }

  GetManifestAndWait();
  EXPECT_FALSE(manifest().IsEmpty());
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
      embedded_test_server()->GetURL("/manifest/dummy-manifest.html");

  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  {
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    ASSERT_TRUE(
        ExecuteScript(shell(),
                      "var a = document.createElement('a'); a.href='#foo';"
                      "document.body.appendChild(a); a.click();"));
    navigation_observer.Wait();
  }

  GetManifestAndWait();
  EXPECT_FALSE(manifest().IsEmpty());
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
  if (iter == request.headers.end() || request.relative_url != "/manifest.json")
    return std::unique_ptr<net::test_server::HttpResponse>();

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
                        custom_embedded_test_server->base_url(),
                        "foobar"));

  ASSERT_TRUE(NavigateToURL(
      shell(), custom_embedded_test_server->GetURL("/index.html")));

  GetManifestAndWait();
  EXPECT_FALSE(manifest().IsEmpty());
  EXPECT_FALSE(manifest_url().is_empty());
  EXPECT_EQ(0, GetConsoleErrorCount());
  ASSERT_EQ(1u, reported_manifest_urls().size());
  EXPECT_EQ(manifest_url(), reported_manifest_urls()[0]);
  ASSERT_EQ(1u, manifests_reported_when_favicon_url_updated().size());
  EXPECT_EQ(1u, manifests_reported_when_favicon_url_updated()[0]);

  // The custom embedded test server will fill the name field with the cookie
  // content.
  EXPECT_TRUE(base::EqualsASCII(manifest().name.string(), "foobar"));
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
  if (iter != request.headers.end() || request.relative_url != "/manifest.json")
    return std::unique_ptr<net::test_server::HttpResponse>();

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
                        custom_embedded_test_server->base_url(),
                        "foobar"));

  ASSERT_TRUE(NavigateToURL(
      shell(), custom_embedded_test_server->GetURL("/index.html")));

  GetManifestAndWait();
  EXPECT_FALSE(manifest().IsEmpty());
  EXPECT_FALSE(manifest_url().is_empty());
  EXPECT_EQ(0, GetConsoleErrorCount());
  ASSERT_EQ(1u, reported_manifest_urls().size());
  EXPECT_EQ(manifest_url(), reported_manifest_urls()[0]);
  ASSERT_EQ(1u, manifests_reported_when_favicon_url_updated().size());
  EXPECT_EQ(1u, manifests_reported_when_favicon_url_updated()[0]);

  // The custom embedded test server will fill set the name to 'no cookies' if
  // it did not find cookies.
  EXPECT_TRUE(base::EqualsASCII(manifest().name.string(), "no cookies"));
}

// This tests that fetching a Manifest from a unique origin always fails,
// regardless of the CORS headers on the manifest. It also tests that no
// manifest change notifications are reported when the origin is unique.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, UniqueOrigin) {
  GURL test_url = embedded_test_server()->GetURL("/manifest/sandboxed.html");

  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  std::string manifest_link =
      embedded_test_server()->GetURL("/manifest/dummy-manifest.json").spec();
  ASSERT_TRUE(ExecuteScript(shell(), "setManifestTo('" + manifest_link + "')"));

  // Same-origin manifest will not be fetched from a unique origin, regardless
  // of CORS headers.
  GetManifestAndWait();
  EXPECT_TRUE(manifest().IsEmpty());
  EXPECT_TRUE(manifest_url().is_empty());
  EXPECT_EQ(0, GetConsoleErrorCount());
  EXPECT_EQ(0u, reported_manifest_urls().size());

  manifest_link =
      embedded_test_server()->GetURL("/manifest/manifest-cors.json").spec();
  ASSERT_TRUE(ExecuteScript(shell(), "setManifestTo('" + manifest_link + "')"));

  GetManifestAndWait();
  EXPECT_TRUE(manifest().IsEmpty());
  EXPECT_TRUE(manifest_url().is_empty());
  EXPECT_EQ(0, GetConsoleErrorCount());
  EXPECT_EQ(0u, reported_manifest_urls().size());
}

} // namespace content
