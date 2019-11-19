// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"

#if defined(OS_ANDROID)
#include "base/system/sys_info.h"
#endif

namespace content {

namespace {

class MockContentBrowserClient final : public ContentBrowserClient {
 public:
  void UpdateRendererPreferencesForWorker(
      BrowserContext*,
      blink::mojom::RendererPreferences* prefs) override {
    if (do_not_track_enabled_) {
      prefs->enable_do_not_track = true;
      prefs->enable_referrers = true;
    }
  }

  void EnableDoNotTrack() { do_not_track_enabled_ = true; }

 private:
  bool do_not_track_enabled_ = false;
};

class DoNotTrackTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
#if defined(OS_ANDROID)
    // TODO(crbug.com/864403): It seems that we call unsupported Android APIs on
    // KitKat when we set a ContentBrowserClient. Don't call such APIs and make
    // this test available on KitKat.
    int32_t major_version = 0, minor_version = 0, bugfix_version = 0;
    base::SysInfo::OperatingSystemVersionNumbers(&major_version, &minor_version,
                                                 &bugfix_version);
    if (major_version < 5)
      return;
#endif

    original_client_ = SetBrowserClientForTesting(&client_);
  }
  void TearDownOnMainThread() override {
    if (original_client_)
      SetBrowserClientForTesting(original_client_);
  }

  // Returns false if we cannot enable do not track. It happens only when
  // Android Kitkat or older systems.
  bool EnableDoNotTrack() {
    if (!original_client_)
      return false;
    client_.EnableDoNotTrack();
    blink::mojom::RendererPreferences* prefs =
        shell()->web_contents()->GetMutableRendererPrefs();
    EXPECT_FALSE(prefs->enable_do_not_track);
    prefs->enable_do_not_track = true;
    return true;
  }

  void ExpectPageTextEq(const std::string& expected_content) {
    std::string text;
    ASSERT_TRUE(ExecuteScriptAndExtractString(
        shell(),
        "window.domAutomationController.send(document.body.innerText);",
        &text));
    EXPECT_EQ(expected_content, text);
  }

  std::string GetDOMDoNotTrackProperty() {
    std::string value;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        shell(),
        "window.domAutomationController.send("
        "    navigator.doNotTrack === null ? '' : navigator.doNotTrack)",
        &value));
    return value;
  }

  GURL GetURL(std::string relative_url) {
    return embedded_test_server()->GetURL(relative_url);
  }

 private:
  ContentBrowserClient* original_client_ = nullptr;
  MockContentBrowserClient client_;
};

std::unique_ptr<net::test_server::HttpResponse>
CaptureHeaderHandlerAndReturnScript(
    const std::string& path,
    net::test_server::HttpRequest::HeaderMap* header_map,
    const std::string& script,
    base::OnceClosure done_callback,
    const net::test_server::HttpRequest& request) {
  GURL request_url = request.GetURL();
  if (request_url.path() != path)
    return nullptr;

  *header_map = request.headers;
  std::move(done_callback).Run();
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/javascript");
  response->set_content(script);
  return response;
}

// Checks that the DNT header is not sent by default.
IN_PROC_BROWSER_TEST_F(DoNotTrackTest, NotEnabled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(shell(), GetURL("/echoheader?DNT")));
  ExpectPageTextEq("None");
  // And the DOM property is not set.
  EXPECT_EQ("", GetDOMDoNotTrackProperty());
}

// Checks that the DNT header is sent when the corresponding preference is set.
IN_PROC_BROWSER_TEST_F(DoNotTrackTest, Simple) {
  ASSERT_TRUE(embedded_test_server()->Start());
  if (!EnableDoNotTrack())
    return;
  EXPECT_TRUE(NavigateToURL(shell(), GetURL("/echoheader?DNT")));
  ExpectPageTextEq("1");
}

// Checks that the DNT header is preserved during redirects.
IN_PROC_BROWSER_TEST_F(DoNotTrackTest, Redirect) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL final_url = GetURL("/echoheader?DNT");
  GURL url = GetURL("/server-redirect?" + final_url.spec());
  if (!EnableDoNotTrack())
    return;
  // We don't check the result NavigateToURL as it returns true only if the
  // final URL is equal to the passed URL.
  EXPECT_TRUE(NavigateToURL(shell(), url, final_url /* expected_commit_url */));
  ExpectPageTextEq("1");
}

// Checks that the DOM property is set when the corresponding preference is set.
IN_PROC_BROWSER_TEST_F(DoNotTrackTest, DOMProperty) {
  ASSERT_TRUE(embedded_test_server()->Start());
  if (!EnableDoNotTrack())
    return;
  EXPECT_TRUE(NavigateToURL(shell(), GetURL("/echo")));
  EXPECT_EQ("1", GetDOMDoNotTrackProperty());
}

// Checks that the DNT header is sent in a request for a dedicated worker
// script.
IN_PROC_BROWSER_TEST_F(DoNotTrackTest, Worker) {
  const std::string kWorkerScript = R"(postMessage('DONE');)";
  net::test_server::HttpRequest::HeaderMap header_map;
  base::RunLoop loop;
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&CaptureHeaderHandlerAndReturnScript, "/capture",
                          &header_map, kWorkerScript, loop.QuitClosure()));
  ASSERT_TRUE(embedded_test_server()->Start());
  if (!EnableDoNotTrack())
    return;
  EXPECT_TRUE(NavigateToURL(
      shell(), GetURL("/workers/create_worker.html?worker_url=/capture")));
  loop.Run();

  EXPECT_TRUE(header_map.find("DNT") != header_map.end());
  EXPECT_EQ("1", header_map["DNT"]);

  // Wait until the worker script is loaded to stop the test from crashing
  // during destruction.
  EXPECT_EQ("DONE", EvalJs(shell(), "waitForMessage();"));
}

// Checks that the DNT header is sent in a request for shared worker script.
// Disabled on Android since a shared worker is not available on Android:
// crbug.com/869745.
#if defined(OS_ANDROID)
#define MAYBE_SharedWorker DISABLED_SharedWorker
#else
#define MAYBE_SharedWorker SharedWorker
#endif
IN_PROC_BROWSER_TEST_F(DoNotTrackTest, MAYBE_SharedWorker) {
  const std::string kWorkerScript =
      R"(self.onconnect = e => { e.ports[0].postMessage('DONE'); };)";
  net::test_server::HttpRequest::HeaderMap header_map;
  base::RunLoop loop;
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&CaptureHeaderHandlerAndReturnScript, "/capture",
                          &header_map, kWorkerScript, loop.QuitClosure()));
  ASSERT_TRUE(embedded_test_server()->Start());
  if (!EnableDoNotTrack())
    return;
  EXPECT_TRUE(NavigateToURL(
      shell(),
      GetURL("/workers/create_shared_worker.html?worker_url=/capture")));
  loop.Run();

  EXPECT_TRUE(header_map.find("DNT") != header_map.end());
  EXPECT_EQ("1", header_map["DNT"]);

  // Wait until the worker script is loaded to stop the test from crashing
  // during destruction.
  EXPECT_EQ("DONE", EvalJs(shell(), "waitForMessage();"));
}

// Checks that the DNT header is sent in a request for a service worker
// script.
IN_PROC_BROWSER_TEST_F(DoNotTrackTest, ServiceWorker_Register) {
  const std::string kWorkerScript = "// empty";
  net::test_server::HttpRequest::HeaderMap header_map;
  base::RunLoop loop;
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&CaptureHeaderHandlerAndReturnScript, "/capture",
                          &header_map, kWorkerScript, loop.QuitClosure()));
  ASSERT_TRUE(embedded_test_server()->Start());
  if (!EnableDoNotTrack())
    return;
  EXPECT_TRUE(NavigateToURL(
      shell(), GetURL("/service_worker/create_service_worker.html")));

  EXPECT_EQ("DONE", EvalJs(shell(), "register('/capture');"));
  loop.Run();

  EXPECT_TRUE(header_map.find("DNT") != header_map.end());
  EXPECT_EQ("1", header_map["DNT"]);

  // Service worker doesn't have to wait for onmessage event because
  // navigator.serviceWorker.ready can ensure that the script load has
  // been completed.
}

// Checks that the DNT header is sent in a request for a service worker
// script during update checking.
IN_PROC_BROWSER_TEST_F(DoNotTrackTest, ServiceWorker_Update) {
  const std::string kWorkerScript = "// empty";
  net::test_server::HttpRequest::HeaderMap header_map;
  base::RunLoop loop;
  // Wait for two requests to capture the request header for updating.
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &CaptureHeaderHandlerAndReturnScript, "/capture", &header_map,
      kWorkerScript, base::BarrierClosure(2, loop.QuitClosure())));
  ASSERT_TRUE(embedded_test_server()->Start());
  if (!EnableDoNotTrack())
    return;

  // Register a service worker, trigger update, then wait until the handler sees
  // the second request.
  EXPECT_TRUE(NavigateToURL(
      shell(), GetURL("/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('/capture');"));
  EXPECT_EQ("DONE", EvalJs(shell(), "update();"));
  loop.Run();

  EXPECT_TRUE(header_map.find("DNT") != header_map.end());
  EXPECT_EQ("1", header_map["DNT"]);

  // Service worker doesn't have to wait for onmessage event because
  // waiting for a promise by registration.update() can ensure that the script
  // load has been completed.
}

// Checks that the DNT header is preserved when fetching from a dedicated
// worker.
IN_PROC_BROWSER_TEST_F(DoNotTrackTest, FetchFromWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());
  if (!EnableDoNotTrack())
    return;
  EXPECT_TRUE(
      NavigateToURL(shell(), GetURL("/workers/fetch_from_worker.html")));
  EXPECT_EQ("1", EvalJs(shell(), "fetch_from_worker('/echoheader?DNT');"));
}

// Checks that the DNT header is preserved when fetching from a shared worker.
//
// Disabled on Android since a shared worker is not available on Android:
// crbug.com/869745.
#if defined(OS_ANDROID)
#define MAYBE_FetchFromSharedWorker DISABLED_FetchFromSharedWorker
#else
#define MAYBE_FetchFromSharedWorker FetchFromSharedWorker
#endif
IN_PROC_BROWSER_TEST_F(DoNotTrackTest, MAYBE_FetchFromSharedWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());
  if (!EnableDoNotTrack())
    return;
  EXPECT_TRUE(
      NavigateToURL(shell(), GetURL("/workers/fetch_from_shared_worker.html")));

  EXPECT_EQ("1",
            EvalJs(shell(), "fetch_from_shared_worker('/echoheader?DNT');"));
}

// Checks that the DNT header is preserved when fetching from a service worker.
IN_PROC_BROWSER_TEST_F(DoNotTrackTest, FetchFromServiceWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());
  if (!EnableDoNotTrack())
    return;
  EXPECT_TRUE(NavigateToURL(
      shell(), GetURL("/service_worker/fetch_from_service_worker.html")));

  EXPECT_EQ("ready", EvalJs(shell(), "setup();"));
  EXPECT_EQ("1",
            EvalJs(shell(), "fetch_from_service_worker('/echoheader?DNT');"));
}

// Checks that the DNT header is preserved when fetching from a page controlled
// by a service worker which doesn't have a fetch handler and falls back to the
// network.
IN_PROC_BROWSER_TEST_F(DoNotTrackTest,
                       FetchFromServiceWorkerControlledPage_NoFetchHandler) {
  ASSERT_TRUE(embedded_test_server()->Start());
  if (!EnableDoNotTrack())
    return;

  // Register a service worker which controls /service_worker.
  EXPECT_TRUE(NavigateToURL(
      shell(), GetURL("/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('empty.js');"));

  // Issue a request from a controlled page.
  EXPECT_TRUE(
      NavigateToURL(shell(), GetURL("/service_worker/fetch_from_page.html")));
  EXPECT_EQ("1", EvalJs(shell(), "fetch_from_page('/echoheader?DNT');"));
}

// Checks that the DNT header is preserved when fetching from a page controlled
// by a service worker which has a fetch handler but falls back to the network.
IN_PROC_BROWSER_TEST_F(DoNotTrackTest,
                       FetchFromServiceWorkerControlledPage_PassThrough) {
  ASSERT_TRUE(embedded_test_server()->Start());
  if (!EnableDoNotTrack())
    return;

  // Register a service worker which controls /service_worker.
  EXPECT_TRUE(NavigateToURL(
      shell(), GetURL("/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE",
            EvalJs(shell(), "register('fetch_event_pass_through.js');"));

  // Issue a request from a controlled page.
  EXPECT_TRUE(
      NavigateToURL(shell(), GetURL("/service_worker/fetch_from_page.html")));
  EXPECT_EQ("1", EvalJs(shell(), "fetch_from_page('/echoheader?DNT');"));
}

// Checks that the DNT header is preserved when fetching from a page controlled
// by a service worker which has a fetch handler and responds with fetch().
IN_PROC_BROWSER_TEST_F(DoNotTrackTest,
                       FetchFromServiceWorkerControlledPage_RespondWithFetch) {
  ASSERT_TRUE(embedded_test_server()->Start());
  if (!EnableDoNotTrack())
    return;

  // Register a service worker which controls /service_worker.
  EXPECT_TRUE(NavigateToURL(
      shell(), GetURL("/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE",
            EvalJs(shell(), "register('fetch_event_respond_with_fetch.js');"));

  // Issue a request from a controlled page.
  EXPECT_TRUE(
      NavigateToURL(shell(), GetURL("/service_worker/fetch_from_page.html")));
  EXPECT_EQ("1", EvalJs(shell(), "fetch_from_page('/echoheader?DNT');"));
}

}  // namespace

}  // namespace content
