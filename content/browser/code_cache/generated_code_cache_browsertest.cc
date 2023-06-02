// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/browser/code_cache/generated_code_cache.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"

namespace content {

namespace {

bool SupportsSharedWorker() {
#if BUILDFLAG(IS_ANDROID)
  // SharedWorkers are not enabled on Android. https://crbug.com/154571
  return false;
#else
  return true;
#endif
}

}  // namespace

enum class CodeCacheTestCase {
  kCachePartitioningEnabled,
  kCachePartitioningDisabled,
};

class CodeCacheBrowserTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<CodeCacheTestCase> {
 public:
  CodeCacheBrowserTest() {
    // Enable the split HTTP cache since the GeneratedCodeCache won't consider
    // partitioning by NIK unless the HTTP cache does.
    feature_split_cache_by_network_isolation_key_.InitAndEnableFeature(
        net::features::kSplitCacheByNetworkIsolationKey);

    // Enable third-party storage partitioning so that for the Shared Worker
    // test, different Shared Workers are used in the third-party contexts with
    // differing top-level sites.
    feature_third_party_storage_partitioning_.InitAndEnableFeature(
        net::features::kThirdPartyStoragePartitioning);

    feature_split_code_cache_by_network_isolation_key_.InitWithFeatureState(
        net::features::kSplitCodeCacheByNetworkIsolationKey,
        IsCachePartitioningEnabled());
  }

  bool IsCachePartitioningEnabled() const {
    return GetParam() == CodeCacheTestCase::kCachePartitioningEnabled;
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &CodeCacheBrowserTest::CachedScriptHandler, base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  std::unique_ptr<net::test_server::HttpResponse> CachedScriptHandler(
      const net::test_server::HttpRequest& request) {
    GURL absolute_url = embedded_test_server()->GetURL(request.relative_url);

    // Worker scripts will fetch this once the cacheable resource has been
    // loaded and the test logic (checking histograms) can continue.
    if (absolute_url.path() == "/done.js") {
      content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                   std::move(done_callback_));

      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);

      http_response->set_content("//done!");
      http_response->set_content_type("application/javascript");
      return http_response;
    }

    // Returns a JavaScript file that should be cacheable by the
    // GeneratedCodeCache (>1024 characters).
    if (absolute_url.path() == "/cacheable.js") {
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);

      std::string content = "let variable = 'hello!';\n";

      // Make sure the script is long enough to be eligible for caching.
      for (int i = 0; i < 16; i++) {
        content += std::string(64, '/') + '\n';
      }

      http_response->set_content(content);
      http_response->set_content_type("application/javascript");
      // NOTE: It seems that if we set a Cache-Control header (even
      // 'Cache-Control: max-age=100000'), this script no longer gets cached in
      // the GeneratedCodeCache, so be sure not to set one here.
      return http_response;
    }

    // Returns an HTML file that will load /cacheable.js.
    if (absolute_url.path() == "/cacheable.html") {
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);

      std::string content =
          "<html><head><title>Title</title></head>"
          "<script src='/cacheable.js'></script></html>";

      http_response->set_content(content);
      return http_response;
    }

    // Returns a JavaScript file that should itself be eligible for caching in
    // the GeneratedCodeCache and that will load /cacheable.js via
    // importScripts.
    if (absolute_url.path() == "/worker.js") {
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);

      // Self-terminate the worker just after loading the scripts so that the
      // parent context doesn't need to wait for the worker's termination when
      // cleaning up the test.
      std::string content = base::StringPrintf(
          "importScripts('cacheable.js', 'done.js');"
          "close();");
      for (int i = 0; i < 16; i++) {
        content += std::string(64, '/') + '\n';
      }

      http_response->set_content(content);
      http_response->set_content_type("application/javascript");
      return http_response;
    }

    // Return a page that will create a Shared Worker that uses /worker.js.
    if (absolute_url.path() == "/shared-worker.html") {
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);

      std::string content =
          "<html><head><title>Title</title></head>"
          "<script>let w = new SharedWorker('worker.js');</script></html>";

      http_response->set_content(content);
      return http_response;
    }

    return nullptr;
  }

  void LoadIframe(const GURL& url) {
    // The script executed here isn't large enough to be eligible for the
    // Generated Code Cache and therefore shouldn't affect any metrics by being
    // run.
    EXPECT_TRUE(
        ExecJs(shell()->web_contents(),
               JsReplace("const iframe = document.createElement('iframe');"
                         "iframe.src = $1;"
                         "document.body.appendChild(iframe);",
                         url)));
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

 protected:
  base::OnceClosure done_callback_;

 private:
  base::test::ScopedFeatureList feature_split_cache_by_network_isolation_key_;
  base::test::ScopedFeatureList feature_third_party_storage_partitioning_;
  base::test::ScopedFeatureList
      feature_split_code_cache_by_network_isolation_key_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    CodeCacheBrowserTest,
    testing::ValuesIn({CodeCacheTestCase::kCachePartitioningEnabled,
                       CodeCacheTestCase::kCachePartitioningDisabled}),
    [](const testing::TestParamInfo<CodeCacheTestCase>& info) {
      switch (info.param) {
        case (CodeCacheTestCase::kCachePartitioningEnabled):
          return "CachePartitioningEnabled";
        case (CodeCacheTestCase::kCachePartitioningDisabled):
          return "CachePartitioningDisabled";
      }
    });

IN_PROC_BROWSER_TEST_P(CodeCacheBrowserTest, CachingFromThirdPartyFrames) {
  GURL a_com_parent_page =
      embedded_test_server()->GetURL("a.com", "/empty.html");
  GURL b_com_parent_page =
      embedded_test_server()->GetURL("b.com", "/empty.html");
  GURL c_com_cacheable_js_page =
      embedded_test_server()->GetURL("c.com", "/cacheable.html");
  {
    // Navigate to the parent page and load an iframe that requests a cacheable
    // javascript resource (/cacheable.js). This should result in one
    // GeneratedCodeCache miss and one create.
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(NavigateToURL(shell(), a_com_parent_page));

    LoadIframe(c_com_cacheable_js_page);

    FetchHistogramsFromChildProcesses();

    histogram_tester.ExpectBucketCount(
        "SiteIsolatedCodeCache.JS.Behaviour",
        GeneratedCodeCache::CacheEntryStatus::kMiss, 1);
    histogram_tester.ExpectBucketCount(
        "SiteIsolatedCodeCache.JS.Behaviour",
        GeneratedCodeCache::CacheEntryStatus::kCreate, 1);
    histogram_tester.ExpectBucketCount(
        "SiteIsolatedCodeCache.JS.Behaviour",
        GeneratedCodeCache::CacheEntryStatus::kHit, 0);
  }

  {
    // Navigate to the same test page again and check for a GeneratedCodeCache
    // hit.
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(NavigateToURL(shell(), a_com_parent_page));

    LoadIframe(c_com_cacheable_js_page);

    FetchHistogramsFromChildProcesses();

    histogram_tester.ExpectBucketCount(
        "SiteIsolatedCodeCache.JS.Behaviour",
        GeneratedCodeCache::CacheEntryStatus::kMiss, 0);
    histogram_tester.ExpectBucketCount(
        "SiteIsolatedCodeCache.JS.Behaviour",
        GeneratedCodeCache::CacheEntryStatus::kHit, 1);
  }

  {
    // Navigate to a page with a different top-level site and verify that:
    //
    //  - When cache partitioning is disabled, /cacheable.js is served from the
    //    GeneratedCodeCache, since resources will be cached per-origin.
    //
    //  - When cache partitioning is enabled, /cacheable.js is not served from
    //    the GeneratedCodeCache, since resources are also partitioned by the
    //    top-level site.
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(NavigateToURL(shell(), b_com_parent_page));

    LoadIframe(c_com_cacheable_js_page);

    FetchHistogramsFromChildProcesses();

    // Note: We don't check the kCreate counts below because for some reason,
    // the previous part of the test causes the /cacheable.js entry to be doomed
    // and then re-created in this part of the test (so the kCreate count is
    // always one more than we'd expect).
    if (IsCachePartitioningEnabled()) {
      histogram_tester.ExpectBucketCount(
          "SiteIsolatedCodeCache.JS.Behaviour",
          GeneratedCodeCache::CacheEntryStatus::kMiss, 1);
      histogram_tester.ExpectBucketCount(
          "SiteIsolatedCodeCache.JS.Behaviour",
          GeneratedCodeCache::CacheEntryStatus::kHit, 0);
    } else {
      histogram_tester.ExpectBucketCount(
          "SiteIsolatedCodeCache.JS.Behaviour",
          GeneratedCodeCache::CacheEntryStatus::kMiss, 0);
      histogram_tester.ExpectBucketCount(
          "SiteIsolatedCodeCache.JS.Behaviour",
          GeneratedCodeCache::CacheEntryStatus::kHit, 1);
    }
  }
}

IN_PROC_BROWSER_TEST_P(CodeCacheBrowserTest,
                       CachingFromThirdPartySharedWorkers) {
  if (!SupportsSharedWorker()) {
    return;
  }
  GURL a_com_parent_page =
      embedded_test_server()->GetURL("a.com", "/empty.html");
  GURL b_com_parent_page =
      embedded_test_server()->GetURL("b.com", "/empty.html");
  GURL c_com_worker_js_page =
      embedded_test_server()->GetURL("c.com", "/shared-worker.html");
  {
    // Navigate to a parent page and create a cross-site iframe that creates a
    // SharedWorker which attempts to load a cacheable script via importScripts.

    // Create a callback that will get called when the worker requests /done.js,
    // indicating that /worker.js and /cacheable.js have been loaded and we can
    // proceed with checking histograms.
    base::test::TestFuture<void> worker_done;
    done_callback_ = worker_done.GetCallback();

    base::HistogramTester histogram_tester;
    EXPECT_TRUE(NavigateToURL(shell(), a_com_parent_page));

    LoadIframe(c_com_worker_js_page);

    ASSERT_TRUE(worker_done.Wait());

    FetchHistogramsFromChildProcesses();

    // TODO(https://crbug.com/964467): The kMiss and kCreate counts are zero
    // here because for Dedicated Workers and Shared Workers the Generated Code
    // Cache isn't used yet. Once it is, update these counts (there are two
    // scripts that could be cached, depending on the implementation -
    // worker.js, the script that contains the Shared Worker code, and
    // cacheable.js, which is loaded by the Shared Worker via a call to
    // importScripts).
    histogram_tester.ExpectBucketCount(
        "SiteIsolatedCodeCache.JS.Behaviour",
        GeneratedCodeCache::CacheEntryStatus::kMiss, 0);
    histogram_tester.ExpectBucketCount(
        "SiteIsolatedCodeCache.JS.Behaviour",
        GeneratedCodeCache::CacheEntryStatus::kCreate, 0);
    histogram_tester.ExpectBucketCount(
        "SiteIsolatedCodeCache.JS.Behaviour",
        GeneratedCodeCache::CacheEntryStatus::kHit, 0);
  }

  {
    base::test::TestFuture<void> worker_done;
    done_callback_ = worker_done.GetCallback();

    base::HistogramTester histogram_tester;
    EXPECT_TRUE(NavigateToURL(shell(), a_com_parent_page));

    LoadIframe(c_com_worker_js_page);

    ASSERT_TRUE(worker_done.Wait());

    FetchHistogramsFromChildProcesses();

    histogram_tester.ExpectBucketCount(
        "SiteIsolatedCodeCache.JS.Behaviour",
        GeneratedCodeCache::CacheEntryStatus::kMiss, 0);
    // TODO(https://crbug.com/964467): Once the Generated Code Cache is used for
    // Shared Workers, check that the scripts loaded from the worker context
    // were re-used from the test section above.
    histogram_tester.ExpectBucketCount(
        "SiteIsolatedCodeCache.JS.Behaviour",
        GeneratedCodeCache::CacheEntryStatus::kHit, 0);
  }

  {
    base::test::TestFuture<void> worker_done;
    done_callback_ = worker_done.GetCallback();

    base::HistogramTester histogram_tester;
    EXPECT_TRUE(NavigateToURL(shell(), b_com_parent_page));

    LoadIframe(c_com_worker_js_page);

    ASSERT_TRUE(worker_done.Wait());

    FetchHistogramsFromChildProcesses();

    // TODO(https://crbug.com/964467): Once the Generated Code Cache is used
    // for Shared Workers, check that the worker scripts re-used in the test
    // section above do not get re-used for this part of the test when
    // `IsCachePartitioningEnabled()` returns true (but are re-used otherwise).
    histogram_tester.ExpectBucketCount(
        "SiteIsolatedCodeCache.JS.Behaviour",
        GeneratedCodeCache::CacheEntryStatus::kMiss, 0);
    histogram_tester.ExpectBucketCount(
        "SiteIsolatedCodeCache.JS.Behaviour",
        GeneratedCodeCache::CacheEntryStatus::kHit, 0);
  }
}

}  // namespace content
