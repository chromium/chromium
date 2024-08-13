// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/code_cache/generated_code_cache.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/browser/renderer_host/code_cache_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/renderer.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/v8_compile_hints_histograms.h"

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

std::string CodeCacheTestCaseToString(CodeCacheTestCase param) {
  switch (param) {
    case CodeCacheTestCase::kCachePartitioningEnabled:
      return "CachePartitioningEnabled";
    case CodeCacheTestCase::kCachePartitioningDisabled:
      return "CachePartitioningDisabled";
  }
}

enum class BackgroundResourceFetchTestCase {
  kBackgroundResourceFetchEnabled,
  kBackgroundResourceFetchDisabled,
};

std::string BackgroundResourceFetchTestCaseToString(
    BackgroundResourceFetchTestCase param) {
  switch (param) {
    case BackgroundResourceFetchTestCase::kBackgroundResourceFetchEnabled:
      return "BackgroundResourceFetchEnabled";
    case BackgroundResourceFetchTestCase::kBackgroundResourceFetchDisabled:
      return "BackgroundResourceFetchDisabled";
  }
}

class CodeCacheBrowserTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<
          std::pair<CodeCacheTestCase, BackgroundResourceFetchTestCase>> {
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

    if (IsBackgroundResourceFetchEnabled()) {
      feature_background_resource_fetch_.InitAndEnableFeature(
          blink::features::kBackgroundResourceFetch);
    } else {
      feature_background_resource_fetch_.InitAndDisableFeature(
          blink::features::kBackgroundResourceFetch);
    }
  }

  bool IsCachePartitioningEnabled() const {
    return GetParam().first == CodeCacheTestCase::kCachePartitioningEnabled;
  }

  bool IsBackgroundResourceFetchEnabled() const {
    return GetParam().second ==
           BackgroundResourceFetchTestCase::kBackgroundResourceFetchEnabled;
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
      GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(done_callback_));

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
      http_response->AddCustomHeader("Cache-Control", "max-age=100000");
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

    // Returns a JavaScript module file that should be cacheable by the
    // GeneratedCodeCache (>1024 characters).
    if (absolute_url.path() == "/cacheable_module.js") {
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);

      std::string content =
          "const title = () => { return 'Module Loaded'; }\n"
          "export default title\n";

      // Make sure the script is long enough to be eligible for caching.
      for (int i = 0; i < 16; i++) {
        content += std::string(64, '/') + '\n';
      }

      http_response->set_content(content);
      http_response->set_content_type("application/javascript");
      http_response->AddCustomHeader("Cache-Control", "max-age=100000");
      return http_response;
    }

    // Returns an HTML file that will load /cacheable_module.js.
    if (absolute_url.path() == "/cacheable_module.html") {
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);

      std::string content =
          "<html><head><title>Title</title></head>"
          "<script type='module'>\n"
          "import title from \"./cacheable_module.js\";\n"
          "document.title = title();"
          "</script></html>";

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
  void PurgeResourceCacheFromTheMainFrame() {
    base::RunLoop loop;
    static_cast<WebContentsImpl*>(shell()->web_contents())
        ->GetPrimaryFrameTree()
        .root()
        ->current_frame_host()
        ->GetProcess()
        ->GetRendererInterface()
        ->PurgeResourceCache(loop.QuitClosure());
    loop.Run();
  }
  void PurgeResourceCacheFromTheFirstSubFrame() {
    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetPrimaryFrameTree()
                              .root();
    CHECK(root->child_count() != 0);
    base::RunLoop loop;
    root->child_at(root->child_count() - 1)
        ->current_frame_host()
        ->GetProcess()
        ->GetRendererInterface()
        ->PurgeResourceCache(loop.QuitClosure());
    loop.Run();
  }
  GeneratedCodeCacheContext* GetGeneratedCodeCacheContext() {
    return shell()
        ->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition()
        ->GetGeneratedCodeCacheContext();
  }

  base::OnceClosure done_callback_;

 private:
  base::test::ScopedFeatureList feature_split_cache_by_network_isolation_key_;
  base::test::ScopedFeatureList feature_third_party_storage_partitioning_;
  base::test::ScopedFeatureList
      feature_split_code_cache_by_network_isolation_key_;
  base::test::ScopedFeatureList feature_background_resource_fetch_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    CodeCacheBrowserTest,
    testing::Values(
        std::make_pair(
            CodeCacheTestCase::kCachePartitioningEnabled,
            BackgroundResourceFetchTestCase::kBackgroundResourceFetchEnabled),
        std::make_pair(
            CodeCacheTestCase::kCachePartitioningEnabled,
            BackgroundResourceFetchTestCase::kBackgroundResourceFetchDisabled),
        std::make_pair(
            CodeCacheTestCase::kCachePartitioningDisabled,
            BackgroundResourceFetchTestCase::kBackgroundResourceFetchEnabled),
        std::make_pair(
            CodeCacheTestCase::kCachePartitioningDisabled,
            BackgroundResourceFetchTestCase::kBackgroundResourceFetchDisabled)),
    [](const testing::TestParamInfo<
        std::pair<CodeCacheTestCase, BackgroundResourceFetchTestCase>>& info) {
      return CodeCacheTestCaseToString(info.param.first) +
             BackgroundResourceFetchTestCaseToString(info.param.second);
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

    PurgeResourceCacheFromTheFirstSubFrame();
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

    PurgeResourceCacheFromTheFirstSubFrame();
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

    // TODO(crbug.com/40628019): The kMiss and kCreate counts are zero
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
    // TODO(crbug.com/40628019): Once the Generated Code Cache is used for
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

    // TODO(crbug.com/40628019): Once the Generated Code Cache is used
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

class CodeCacheSizeChecker {
 public:
  CodeCacheSizeChecker(GeneratedCodeCacheContext* cache_context,
                       const GURL& url,
                       const GURL& origin,
                       size_t expected_size)
      : cache_context_(cache_context),
        url_(url),
        origin_(origin),
        expected_size_(expected_size) {}

  size_t Wait() {
    base::test::TestFuture<void> done;
    done_callback_ = done.GetCallback();

    GeneratedCodeCacheContext::GetTaskRunner(cache_context_)
        ->PostTask(FROM_HERE,
                   base::BindOnce(&CodeCacheSizeChecker::GetCodeCache,
                                  base::Unretained(this)));
    CHECK(done.Wait());
    return expected_size_;
  }

 private:
  void GetCodeCache() {
    net::NetworkIsolationKey nik = net::NetworkIsolationKey(
        net::SchemefulSite(origin_), net::SchemefulSite(origin_));
    cache_context_->generated_js_code_cache()->FetchEntry(
        url_, GURL(), nik,
        base::BindOnce(&CodeCacheSizeChecker::FetchCallback,
                       base::Unretained(this)));
  }

  void FetchCallback(const base::Time&, mojo_base::BigBuffer data) {
    if (data.size() >= expected_size_) {
      GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(done_callback_));
    } else {
      // Retries because the CodeCacheHost's IPC may delay.
      GeneratedCodeCacheContext::GetTaskRunner(cache_context_)
          ->PostDelayedTask(FROM_HERE,
                            base::BindOnce(&CodeCacheSizeChecker::GetCodeCache,
                                           base::Unretained(this)),
                            base::Microseconds(100));
    }
  }

  scoped_refptr<GeneratedCodeCacheContext> cache_context_;
  const GURL url_;
  const GURL origin_;
  const size_t expected_size_;
  size_t actual_size_ = 0;
  base::OnceClosure done_callback_;
};

IN_PROC_BROWSER_TEST_P(CodeCacheBrowserTest,
                       GeneratedCodeCacheSizeClassicScript) {
  // With this, we can query the code cache in a unified way in platforms which
  // use origin locks differently.
  CodeCacheHostImpl::SetUseEmptySecondaryKeyForTesting();
  GURL url = embedded_test_server()->GetURL("c.com", "/cacheable.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GeneratedCodeCacheContext* cache_context = GetGeneratedCodeCacheContext();
  // Wait until compile hints were written into the cache.
  const GURL& cacheable_script =
      embedded_test_server()->GetURL("c.com", "/cacheable.js");
  constexpr size_t kTimeStampSize = 24;  // Tag + actual data.
  CodeCacheSizeChecker code_cache_size_checker(
      cache_context, cacheable_script,
      embedded_test_server()->GetURL("c.com", "/"), kTimeStampSize);
  EXPECT_EQ(kTimeStampSize, code_cache_size_checker.Wait());

  // Clear Blink side cache.
  PurgeResourceCacheFromTheMainFrame();
  // Navigate away.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  // Navigate to the same page.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  // We expect that the generated code cache is larger than the timestamp data.
  CodeCacheSizeChecker code_cache_size_checker2(
      cache_context, cacheable_script,
      embedded_test_server()->GetURL("c.com", "/"), kTimeStampSize + 1);
  code_cache_size_checker2.Wait();
}

IN_PROC_BROWSER_TEST_P(CodeCacheBrowserTest,
                       GeneratedCodeCacheSizeModuleScript) {
  // With this, we can query the code cache in a unified way in platforms which
  // use origin locks differently.
  CodeCacheHostImpl::SetUseEmptySecondaryKeyForTesting();
  GURL url = embedded_test_server()->GetURL("c.com", "/cacheable_module.html");
  const std::u16string expected_title = u"Module Loaded";
  {
    TitleWatcher watcher(shell()->web_contents(), expected_title);
    EXPECT_TRUE(NavigateToURL(shell(), url));
    EXPECT_EQ(expected_title, watcher.WaitAndGetTitle());
  }

  GeneratedCodeCacheContext* cache_context = GetGeneratedCodeCacheContext();
  // Wait until compile hints were written into the cache.
  const GURL& cacheable_module_script =
      embedded_test_server()->GetURL("c.com", "/cacheable_module.js");
  constexpr size_t kTimeStampSize = 24;  // Tag + actual data.
  CodeCacheSizeChecker code_cache_size_checker(
      cache_context, cacheable_module_script,
      embedded_test_server()->GetURL("c.com", "/"), kTimeStampSize);
  EXPECT_EQ(kTimeStampSize, code_cache_size_checker.Wait());

  // Clear Blink side cache.
  PurgeResourceCacheFromTheMainFrame();
  // Navigate away.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  // Navigate to the same page.
  {
    TitleWatcher watcher(shell()->web_contents(), expected_title);
    EXPECT_TRUE(NavigateToURL(shell(), url));
    EXPECT_EQ(expected_title, watcher.WaitAndGetTitle());
  }
  // We expect that the generated code cache is larger than the timestamp data.
  CodeCacheSizeChecker code_cache_size_checker2(
      cache_context, cacheable_module_script,
      embedded_test_server()->GetURL("c.com", "/"), kTimeStampSize + 1);
  code_cache_size_checker2.Wait();
}

class CompileHintsBrowserTest : public ContentBrowserTest {
 public:
  CompileHintsBrowserTest() = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &CompileHintsBrowserTest::CachedScriptHandler, base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  std::unique_ptr<net::test_server::HttpResponse> CachedScriptHandler(
      const net::test_server::HttpRequest& request) {
    GURL absolute_url = embedded_test_server()->GetURL(request.relative_url);

    // Returns a JavaScript file that should be cacheable by the
    // GeneratedCodeCache (>1024 characters).
    if (absolute_url.path() == "/cacheable.js") {
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);
      http_response->AddCustomHeader("Cache-Control", "max-age=604800");

      // The script has to include a function so that we can test compile hints
      // with it.
      std::string content = R"(
        let variable = 'hello!';
        function foo() {}
        foo();
        )";

      // Make sure the script is long enough to be eligible for caching.
      for (int i = 0; i < 16; i++) {
        content += std::string(64, '/') + '\n';
      }

      http_response->set_content(content);
      http_response->set_content_type("application/javascript");
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
    return nullptr;
  }
};

class LocalCompileHintsBrowserTest : public CompileHintsBrowserTest {
 public:
  LocalCompileHintsBrowserTest() {
    local_compile_hints_.InitAndEnableFeature(
        blink::features::kLocalCompileHints);
    interactive_detector_ignore_fcp_.InitAndEnableFeature(
        blink::features::kInteractiveDetectorIgnoreFcp);
  }

 private:
  base::test::ScopedFeatureList local_compile_hints_;
  base::test::ScopedFeatureList interactive_detector_ignore_fcp_;
};

class NoLocalCompileHintsBrowserTest : public CompileHintsBrowserTest {
 public:
  NoLocalCompileHintsBrowserTest() {
    local_compile_hints_.InitAndDisableFeature(
        blink::features::kLocalCompileHints);
  }

 private:
  base::test::ScopedFeatureList local_compile_hints_;
};

IN_PROC_BROWSER_TEST_F(NoLocalCompileHintsBrowserTest, NoCompileHints) {
  // TODO(chromium:1495723): Migrate this test to use use counters once we no
  // longer have the histograms.

  // With this, we can query the code cache in a unified way in platforms which
  // use origin locks differently.
  CodeCacheHostImpl::SetUseEmptySecondaryKeyForTesting();

  GURL cacheable_page =
      embedded_test_server()->GetURL("c.com", "/cacheable.html");
  GURL other_page = embedded_test_server()->GetURL("a.com", "/empty.html");

  {
    // Navigate to the page which requests a cacheable
    // javascript resource (/cacheable.js).
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(NavigateToURL(shell(), cacheable_page));

    FetchHistogramsFromChildProcesses();
    EXPECT_EQ(
        1, histogram_tester.GetBucketCount(
               blink::v8_compile_hints::kStatusHistogram,
               blink::v8_compile_hints::Status::
                   kNoCompileHintsClassicNonStreaming) +
               histogram_tester.GetBucketCount(
                   blink::v8_compile_hints::kStatusHistogram,
                   blink::v8_compile_hints::Status::kNoCompileHintsStreaming));
  }

  // Navigate away.
  EXPECT_TRUE(NavigateToURL(shell(), other_page));

  {
    // Navigate to the same test page again and check that local compile hints
    // weren't hit.
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(NavigateToURL(shell(), cacheable_page));

    FetchHistogramsFromChildProcesses();

    EXPECT_EQ(
        1, histogram_tester.GetBucketCount(
               blink::v8_compile_hints::kStatusHistogram,
               blink::v8_compile_hints::Status::
                   kNoCompileHintsClassicNonStreaming) +
               histogram_tester.GetBucketCount(
                   blink::v8_compile_hints::kStatusHistogram,
                   blink::v8_compile_hints::Status::kNoCompileHintsStreaming));
  }

  // Navigate away.
  EXPECT_TRUE(NavigateToURL(shell(), other_page));

  {
    // Navigate to the same test page again and check for a code cache hit.
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(NavigateToURL(shell(), cacheable_page));

    FetchHistogramsFromChildProcesses();

    histogram_tester.ExpectBucketCount(
        blink::v8_compile_hints::kStatusHistogram,
        blink::v8_compile_hints::Status::kConsumeCodeCacheClassicNonStreaming,
        1);
  }
}

IN_PROC_BROWSER_TEST_F(LocalCompileHintsBrowserTest, LocalCompileHints) {
  // TODO(chromium:1495723): Migrate this test to use use counters once we no
  // longer have the histograms.

  // With this, we can query the code cache in a unified way in platforms which
  // use origin locks differently.
  CodeCacheHostImpl::SetUseEmptySecondaryKeyForTesting();

  GURL cacheable_page =
      embedded_test_server()->GetURL("c.com", "/cacheable.html");
  GURL other_page = embedded_test_server()->GetURL("a.com", "/empty.html");

  {
    // Navigate to the page which requests a cacheable
    // javascript resource (/cacheable.js). Once the page is interactive, local
    // compile hints will be generated.
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(NavigateToURL(shell(), cacheable_page));

    GeneratedCodeCacheContext* cache_context =
        shell()
            ->web_contents()
            ->GetBrowserContext()
            ->GetDefaultStoragePartition()
            ->GetGeneratedCodeCacheContext();
    // Wait until compile hints were written into the cache.
    const GURL& cacheable_script =
        embedded_test_server()->GetURL("c.com", "/cacheable.js");
    constexpr int kCompileHintsCacheSize = 28;  // Tag + actual data.
    CodeCacheSizeChecker code_cache_size_checker(
        cache_context, cacheable_script, GURL("http://c.com/"),
        kCompileHintsCacheSize);
    code_cache_size_checker.Wait();

    FetchHistogramsFromChildProcesses();
    EXPECT_EQ(1, histogram_tester.GetBucketCount(
                     blink::v8_compile_hints::kStatusHistogram,
                     blink::v8_compile_hints::Status::
                         kProduceCompileHintsClassicNonStreaming) +
                     histogram_tester.GetBucketCount(
                         blink::v8_compile_hints::kStatusHistogram,
                         blink::v8_compile_hints::Status::
                             kProduceCompileHintsStreaming));
    EXPECT_EQ(1,
              histogram_tester.GetBucketCount(
                  blink::v8_compile_hints::kLocalCompileHintsGeneratedHistogram,
                  blink::v8_compile_hints::LocalCompileHintsGenerated::kFinal));
  }

  // Navigate away.
  EXPECT_TRUE(NavigateToURL(shell(), other_page));

  {
    // Navigate to the same test page again and check for a local compile hints
    // hit.
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(NavigateToURL(shell(), cacheable_page));

    FetchHistogramsFromChildProcesses();

    EXPECT_EQ(1, histogram_tester.GetBucketCount(
                     blink::v8_compile_hints::kStatusHistogram,
                     blink::v8_compile_hints::Status::
                         kConsumeLocalCompileHintsClassicNonStreaming) +
                     histogram_tester.GetBucketCount(
                         blink::v8_compile_hints::kStatusHistogram,
                         blink::v8_compile_hints::Status::
                             kConsumeLocalCompileHintsStreaming));
  }

  // Navigate away.
  EXPECT_TRUE(NavigateToURL(shell(), other_page));

  {
    // Navigate to the same test page again and check for a code cache hit.
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(NavigateToURL(shell(), cacheable_page));

    FetchHistogramsFromChildProcesses();

    histogram_tester.ExpectBucketCount(
        blink::v8_compile_hints::kStatusHistogram,
        blink::v8_compile_hints::Status::kConsumeCodeCacheClassicNonStreaming,
        1);
  }
}

}  // namespace content
