// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

const char kReloadTestPath[] = "/loader/reload_test.html";
// The test page should request resources as the content structure is described
// below. Reload and the same page navigation will affect only the top frame
// resource, reload_test.html. But bypassing reload will affect all resources.
// +- reload_test.html
//     +- empty16x16.png
//     +- simple_frame.html
//         +- empty16x16.png

const char kNoCacheControl[] = "";
const char kMaxAgeCacheControl[] = "max-age=0";
const char kNoCacheCacheControl[] = "no-cache";

struct RequestLog {
  std::string relative_url;
  std::string cache_control;
};

struct ExpectedCacheControl {
  const char* top_main;
  const char* others;
};

const ExpectedCacheControl kExpectedCacheControlForNormalLoad = {
    kNoCacheControl, kNoCacheControl};
const ExpectedCacheControl kExpectedCacheControlForReload = {
    kMaxAgeCacheControl, kNoCacheControl};
const ExpectedCacheControl kExpectedCacheControlForBypassingReload = {
    kNoCacheCacheControl, kNoCacheCacheControl};

// Tests end to end behaviors between Blink and content around reload variants.
class ReloadCacheControlBrowserTest : public ContentBrowserTest {
 protected:
  ReloadCacheControlBrowserTest() {}
  ~ReloadCacheControlBrowserTest() override = default;

  void SetUpOnMainThread() override {
    SetUpTestServerOnMainThread();
  }

  void SetUpTestServerOnMainThread() {
    // ContentBrowserTest creates embedded_test_server instance with
    // a registered HandleFileRequest for "content/test/data".
    // Because the handler is registered as the first handler, MonitorHandler
    // is needed to capture all requests.
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &ReloadCacheControlBrowserTest::MonitorRequestHandler,
        base::Unretained(this)));

    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  void CheckCacheControl(const ExpectedCacheControl& expectation) {
    base::AutoLock lock(request_log_lock_);
    EXPECT_EQ(4u, request_log_.size());
    for (const auto& log : request_log_) {
      if (log.relative_url == kReloadTestPath)
        EXPECT_EQ(expectation.top_main, log.cache_control);
      else
        EXPECT_EQ(expectation.others, log.cache_control);
    }
    request_log_.clear();
  }

  std::vector<RequestLog> request_log_;
  base::Lock request_log_lock_;

 private:
  void MonitorRequestHandler(const HttpRequest& request) {
    RequestLog log;
    log.relative_url = request.relative_url;
    auto cache_control = request.headers.find("Cache-Control");
    log.cache_control = cache_control == request.headers.end()
                            ? kNoCacheControl
                            : cache_control->second;
    base::AutoLock lock(request_log_lock_);
    request_log_.push_back(log);
  }

  DISALLOW_COPY_AND_ASSIGN(ReloadCacheControlBrowserTest);
};

// Test if reload issues requests with proper cache control flags.
IN_PROC_BROWSER_TEST_F(ReloadCacheControlBrowserTest, NormalReload) {
  GURL url(embedded_test_server()->GetURL(kReloadTestPath));

  EXPECT_TRUE(NavigateToURL(shell(), url));
  CheckCacheControl(kExpectedCacheControlForNormalLoad);

  ReloadBlockUntilNavigationsComplete(shell(), 1);
  CheckCacheControl(kExpectedCacheControlForReload);

  shell()->ShowDevTools();
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  CheckCacheControl(kExpectedCacheControlForReload);

  shell()->CloseDevTools();
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  CheckCacheControl(kExpectedCacheControlForReload);
}

// Test if bypassing reload issues requests with proper cache control flags.
IN_PROC_BROWSER_TEST_F(ReloadCacheControlBrowserTest, BypassingReload) {
  GURL url(embedded_test_server()->GetURL(kReloadTestPath));

  NavigateToURLBlockUntilNavigationsComplete(shell(), url, 1);
  CheckCacheControl(kExpectedCacheControlForNormalLoad);

  ReloadBypassingCacheBlockUntilNavigationsComplete(shell(), 1);
  CheckCacheControl(kExpectedCacheControlForBypassingReload);

  shell()->ShowDevTools();
  ReloadBypassingCacheBlockUntilNavigationsComplete(shell(), 1);
  CheckCacheControl(kExpectedCacheControlForBypassingReload);

  shell()->CloseDevTools();
  ReloadBypassingCacheBlockUntilNavigationsComplete(shell(), 1);
  CheckCacheControl(kExpectedCacheControlForBypassingReload);
}

// Test if the same page navigation issues requests with proper cache control
// flags.
IN_PROC_BROWSER_TEST_F(ReloadCacheControlBrowserTest, NavigateToSame) {
  GURL url(embedded_test_server()->GetURL(kReloadTestPath));

  // The first navigation is just a normal load.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  CheckCacheControl(kExpectedCacheControlForNormalLoad);

  // The second navigation is the same page navigation. This should be handled
  // as a reload, revalidating the main resource, but following cache protocols
  // for others.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  CheckCacheControl(kExpectedCacheControlForReload);

  shell()->ShowDevTools();
  EXPECT_TRUE(NavigateToURL(shell(), url));
  CheckCacheControl(kExpectedCacheControlForReload);

  shell()->CloseDevTools();
  EXPECT_TRUE(NavigateToURL(shell(), url));
  CheckCacheControl(kExpectedCacheControlForReload);
}

// Reloading with ReloadType::NORMAL should respect service workers.
IN_PROC_BROWSER_TEST_F(ReloadCacheControlBrowserTest,
                       NormalReload_ControlledByServiceWorker) {
  // Prepare for a service worker.
  EXPECT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('fetch_event_blob.js');"));

  // Open a page served by the service worker.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/service_worker/empty.html")));
  EXPECT_EQ(base::UTF8ToUTF16("Title"), shell()->web_contents()->GetTitle());

  // Reload from the browser. The page is still controlled by the service
  // worker.
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(base::UTF8ToUTF16("Title"), shell()->web_contents()->GetTitle());
}

// Reloading with ReloadType::BYPASSING_CACHE should bypass service workers.
IN_PROC_BROWSER_TEST_F(ReloadCacheControlBrowserTest,
                       BypassingReload_ControlledByServiceWorker) {
  // Prepare for a service worker.
  EXPECT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('fetch_event_blob.js');"));

  // Open a page served by the service worker.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/service_worker/empty.html")));
  EXPECT_EQ(base::UTF8ToUTF16("Title"), shell()->web_contents()->GetTitle());

  // Reload from the browser with pressing shift key. It bypasses the service
  // worker.
  ReloadBypassingCacheBlockUntilNavigationsComplete(shell(), 1);
  EXPECT_EQ(base::UTF8ToUTF16("ServiceWorker test - empty page"),
            shell()->web_contents()->GetTitle());
}

}  // namespace

}  // namespace content
