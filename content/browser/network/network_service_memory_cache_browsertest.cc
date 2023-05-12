// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/resource_load_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/features.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"
#include "url/gurl.h"

namespace content {

namespace {

constexpr const char kTestPagePath[] = "/empty.html";
constexpr const char kCrossOrignIsolatedPath[] = "/cross-origin-isolated.html";

// Cacheable, no Access-Control-Allow-Origin / Cross-Origin-Resource-Policy.
constexpr const char kCacheableIsolatedImagePath[] = "/cacheable-isolated.jpg";

// Cacheable, Access-Control-Allow-Origin is "*".
constexpr const char kCacheableImagePath[] = "/cacheable.svg";

// Cache-Control is "no-store".
constexpr const char kNoStoreImagePath[] = "/nostore.jpg";

}  // namespace

class NetworkServiceMemoryCacheBrowserTest : public ContentBrowserTest {
 public:
  NetworkServiceMemoryCacheBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{network::features::kNetworkServiceMemoryCache,
                              net::features::kSplitCacheByNetworkIsolationKey},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    // Setup the server to allow serving separate sites.
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    // Setup a cross origin server.
    cross_origin_server_.ServeFilesFromSourceDirectory(GetTestDataFilePath());
    net::test_server::RegisterDefaultHandlers(&cross_origin_server_);
    ASSERT_TRUE(cross_origin_server_.Start());
  }

  net::EmbeddedTestServer& cross_origin_server() {
    return cross_origin_server_;
  }

  bool FetchResource(const GURL& url) {
    ResourceLoadObserver observer(shell());
    EvalJsResult result = EvalJs(shell(), JsReplace(R"(
      fetch($1)
        .then(_ => true)
        .catch(_ => false);
    )",
                                                    url));
    if (!result.ExtractBool())
      return false;
    observer.WaitForResourceCompletion(url);
    return true;
  }

  bool LoadImageViaImgTag(const GURL& url) {
    ResourceLoadObserver observer(shell());
    EvalJsResult result = EvalJs(shell(), JsReplace(R"(
      new Promise(resolve => {
        const img = document.createElement("img");
        img.src = $1;
        img.onerror = () => resolve(false);
        img.onload = () => resolve(true);
        document.body.appendChild(img);
      });
    )",
                                                    url));
    if (!result.ExtractBool())
      return false;
    observer.WaitForResourceCompletion(url);
    return true;
  }

  bool ResourceIsInMemoryCache(const GURL& resource_url) {
    ResourceLoadObserver observer(shell());
    if (!FetchResource(resource_url))
      return false;
    observer.WaitForResourceCompletion(resource_url);
    return (*observer.GetResource(resource_url))
        ->was_in_network_service_memory_cache;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  net::EmbeddedTestServer cross_origin_server_;
};

IN_PROC_BROWSER_TEST_F(NetworkServiceMemoryCacheBrowserTest, Basic) {
  const GURL resource_url =
      embedded_test_server()->GetURL(kCacheableIsolatedImagePath);

  // Fetch a cacheable resource to store it in the in-memory cache.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(kTestPagePath)));
  ASSERT_TRUE(FetchResource(resource_url));

  ASSERT_TRUE(ResourceIsInMemoryCache(resource_url));
}

IN_PROC_BROWSER_TEST_F(NetworkServiceMemoryCacheBrowserTest, NoStore) {
  const GURL resource_url = embedded_test_server()->GetURL(kNoStoreImagePath);

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(kTestPagePath)));
  ASSERT_TRUE(FetchResource(resource_url));
  ASSERT_FALSE(ResourceIsInMemoryCache(resource_url));
}

IN_PROC_BROWSER_TEST_F(NetworkServiceMemoryCacheBrowserTest, CorsBlocked) {
  const GURL resource_url =
      cross_origin_server().GetURL(kCacheableIsolatedImagePath);

  // Navigate to a cross site and fetch a resource to store the response in the
  // in-memory cache.
  EXPECT_TRUE(
      NavigateToURL(shell(), cross_origin_server().GetURL(kTestPagePath)));
  ASSERT_TRUE(FetchResource(resource_url));
  ASSERT_TRUE(ResourceIsInMemoryCache(resource_url));

  // Navigate to a test page and try to fetch the cross origin resource. It
  // should be blocked by CORS checks.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(kTestPagePath)));
  ASSERT_FALSE(FetchResource(resource_url));
}

IN_PROC_BROWSER_TEST_F(NetworkServiceMemoryCacheBrowserTest, CorpBlocked) {
  const GURL resource_url =
      cross_origin_server().GetURL(kCacheableIsolatedImagePath);

  // Store a cross-site image in the in-memory cache. The response doesn't have
  // Cross-Origin-Resource-Policy.
  EXPECT_TRUE(
      NavigateToURL(shell(), cross_origin_server().GetURL(kTestPagePath)));
  ASSERT_TRUE(FetchResource(resource_url));
  ASSERT_TRUE(ResourceIsInMemoryCache(resource_url));

  // Try to load the image from a cross origin isolated page. It should be
  // blocked by CORP checks.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(kCrossOrignIsolatedPath)));
  ASSERT_FALSE(LoadImageViaImgTag(resource_url));
}

IN_PROC_BROWSER_TEST_F(NetworkServiceMemoryCacheBrowserTest, SplitCache) {
  const GURL test_page1 =
      embedded_test_server()->GetURL("a.test", kTestPagePath);
  const GURL test_page2 = cross_origin_server().GetURL("b.test", kTestPagePath);
  const GURL resource_url =
      embedded_test_server()->GetURL("x.test", kCacheableImagePath);

  // Fetch an image on a site. Cached response will be available for the same
  // site.
  EXPECT_TRUE(NavigateToURL(shell(), test_page1));
  ASSERT_TRUE(FetchResource(resource_url));
  ASSERT_TRUE(ResourceIsInMemoryCache(resource_url));

  // Navigate to a different site and fetch the same image. There should be no
  // cached response yet.
  EXPECT_TRUE(NavigateToURL(shell(), test_page2));
  ASSERT_FALSE(ResourceIsInMemoryCache(resource_url));
}

}  // namespace content
