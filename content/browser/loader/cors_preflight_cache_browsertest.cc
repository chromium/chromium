// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/cpp/cors/cors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

const char kTestPath[] = "/loader/cors_preflight.html";
const std::u16string kTestDone = std::u16string(u"DONE");

// Tests end to end behaviors on CORS preflight and its cache.
class CorsPreflightCacheBrowserTest : public ContentBrowserTest {
 public:
  CorsPreflightCacheBrowserTest(const CorsPreflightCacheBrowserTest&) = delete;
  CorsPreflightCacheBrowserTest& operator=(
      const CorsPreflightCacheBrowserTest&) = delete;

 protected:
  CorsPreflightCacheBrowserTest() = default;
  ~CorsPreflightCacheBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());

    cross_origin_test_server_.RegisterRequestHandler(base::BindRepeating(
        &CorsPreflightCacheBrowserTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(cross_origin_test_server_.Start());
  }

 protected:
  uint16_t cross_origin_port() { return cross_origin_test_server_.port(); }
  size_t options_count() {
    base::AutoLock lock(lock_);
    return options_count_;
  }
  size_t get_count() {
    base::AutoLock lock(lock_);
    return get_count_;
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->AddCustomHeader(
        network::cors::header_names::kAccessControlAllowOrigin, "*");
    if (request.method == net::test_server::METHOD_OPTIONS) {
      response->AddCustomHeader(
          network::cors::header_names::kAccessControlAllowMethods,
          "GET, OPTIONS");
      response->AddCustomHeader(
          network::cors::header_names::kAccessControlAllowHeaders, "foo");
      response->AddCustomHeader(
          network::cors::header_names::kAccessControlMaxAge, "60");
      base::AutoLock lock(lock_);
      options_count_++;
    } else if (request.method == net::test_server::METHOD_GET) {
      base::AutoLock lock(lock_);
      get_count_++;
    }
    return response;
  }

  net::EmbeddedTestServer cross_origin_test_server_;
  base::Lock lock_;

  size_t options_count_ GUARDED_BY(lock_) = 0;
  size_t get_count_ GUARDED_BY(lock_) = 0;
};

IN_PROC_BROWSER_TEST_F(CorsPreflightCacheBrowserTest, Default) {
  std::unique_ptr<TitleWatcher> watcher1 =
      std::make_unique<TitleWatcher>(shell()->web_contents(), kTestDone);
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(base::StringPrintf(
                   "%s?;%d;default", kTestPath, cross_origin_port()))));
  EXPECT_EQ(kTestDone, watcher1->WaitAndGetTitle());
  EXPECT_EQ(1u, options_count());
  EXPECT_EQ(1u, get_count());

  // Make another fetch request, and OPTIONS request hits the preflight cache.
  std::unique_ptr<TitleWatcher> watcher2 =
      std::make_unique<TitleWatcher>(shell()->web_contents(), kTestDone);
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(base::StringPrintf(
                   "%s?;%d;default", kTestPath, cross_origin_port()))));
  EXPECT_EQ(kTestDone, watcher2->WaitAndGetTitle());
  EXPECT_EQ(1u, options_count());
  EXPECT_EQ(2u, get_count());

  // Make another fetch request with reload cache mode, and it should not hit
  // the preflight cache.
  std::unique_ptr<TitleWatcher> watcher3 =
      std::make_unique<TitleWatcher>(shell()->web_contents(), kTestDone);
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(base::StringPrintf(
                   "%s?;%d;reload", kTestPath, cross_origin_port()))));
  EXPECT_EQ(kTestDone, watcher3->WaitAndGetTitle());
  EXPECT_EQ(2u, options_count());
  EXPECT_EQ(3u, get_count());
}

IN_PROC_BROWSER_TEST_F(CorsPreflightCacheBrowserTest, ClearCache) {
  // Cache should be empty at first. Expect OPTIONS
  std::unique_ptr<TitleWatcher> watcher1 =
      std::make_unique<TitleWatcher>(shell()->web_contents(), kTestDone);
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(base::StringPrintf(
                   "%s?;%d;default", kTestPath, cross_origin_port()))));
  EXPECT_EQ(kTestDone, watcher1->WaitAndGetTitle());
  EXPECT_EQ(1u, options_count());
  EXPECT_EQ(1u, get_count());

  // Make another fetch request, and OPTIONS request hits the preflight cache.
  std::unique_ptr<TitleWatcher> watcher2 =
      std::make_unique<TitleWatcher>(shell()->web_contents(), kTestDone);
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(base::StringPrintf(
                   "%s?;%d;default", kTestPath, cross_origin_port()))));
  EXPECT_EQ(kTestDone, watcher2->WaitAndGetTitle());
  EXPECT_EQ(1u, options_count());
  EXPECT_EQ(2u, get_count());

  // Clear the PreflightCache, make a request and it should not hit the
  // Preflight cache
  BrowsingDataRemover* remover =
      shell()->web_contents()->GetBrowserContext()->GetBrowsingDataRemover();
  BrowsingDataRemoverCompletionObserver completion_observer(remover);
  remover->RemoveAndReply(
      base::Time(), base::Time::Max(), BrowsingDataRemover::DATA_TYPE_CACHE,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
      &completion_observer);
  completion_observer.BlockUntilCompletion();
  // Make another fetch request with reload cache mode, and it should not hit
  // the preflight cache.
  std::unique_ptr<TitleWatcher> watcher3 =
      std::make_unique<TitleWatcher>(shell()->web_contents(), kTestDone);
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(base::StringPrintf(
                   "%s?;%d;reload", kTestPath, cross_origin_port()))));
  EXPECT_EQ(kTestDone, watcher3->WaitAndGetTitle());
  EXPECT_EQ(2u, options_count());
  EXPECT_EQ(3u, get_count());
}

}  // namespace

}  // namespace content
