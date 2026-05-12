// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/notimplemented.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/to_string.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/renderer.mojom.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_FUCHSIA)
namespace {
// See the definition of `v8::internal::ScriptCompileTimerScope::CacheBehaviour`
// in `v8/src/codegen/compiler.cc` for the correspondence.
int CacheBehaviourNameToInt(std::string_view name) {
  if (name == "kProduceCodeCache") {
    return 0;
  }
  if (name == "kHitIsolateCacheWhenNoCache") {
    return 1;
  }
  if (name == "kConsumeCodeCache") {
    return 2;
  }
  if (name == "kNoCacheBecauseScriptTooSmall") {
    return 5;
  }
  if (name == "kNoCacheBecauseCacheTooCold") {
    return 6;
  }
  if (name == "kHitIsolateCacheWhenProduceCodeCache") {
    return 14;
  }
  if (name == "kHitIsolateCacheWhenConsumeCodeCache") {
    return 15;
  }
  if (name == "kNoCacheBecauseInlineScriptCacheTooCold") {
    return 22;
  }
  // TODO: Add other values if necessary.
  NOTIMPLEMENTED();
  return -1;
}

int GetProduceCacheCount(const base::HistogramTester& histogram_tester) {
  return histogram_tester.GetBucketCount(
             "V8.CompileScript.CacheBehaviour",
             CacheBehaviourNameToInt("kProduceCodeCache")) +
         histogram_tester.GetBucketCount(
             "V8.CompileScript.CacheBehaviour",
             CacheBehaviourNameToInt("kHitIsolateCacheWhenProduceCodeCache"));
}

int GetConsumeCacheCount(const base::HistogramTester& histogram_tester) {
  return histogram_tester.GetBucketCount(
             "V8.CompileScript.CacheBehaviour",
             CacheBehaviourNameToInt("kConsumeCodeCache")) +
         histogram_tester.GetBucketCount(
             "V8.CompileScript.CacheBehaviour",
             CacheBehaviourNameToInt("kHitIsolateCacheWhenConsumeCodeCache"));
}

}  // namespace

namespace content {

class InlineScriptCodeCacheBrowserTest : public ContentBrowserTest {
 public:
  InlineScriptCodeCacheBrowserTest()
      : InlineScriptCodeCacheBrowserTest(base::FieldTrialParams()) {}

 protected:
  explicit InlineScriptCodeCacheBrowserTest(
      const base::FieldTrialParams& extra_params) {
    base::FieldTrialParams params = extra_params;
    base::FieldTrialParams default_params = {{"timeout", "1s"}};
    params.merge(default_params);
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{net::features::kSplitCacheByNetworkIsolationKey, {}},
         {net::features::kSplitCodeCacheByNetworkIsolationKey, {}},
         {blink::features::kUsePersistentCacheForCodeCache, {}},
         {blink::features::kInlineScriptCache, params}},
        {{}});
  }

 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&InlineScriptCodeCacheBrowserTest::RequestHandler,
                            base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request) {
    GURL absolute_url = embedded_test_server()->GetURL(request.relative_url);

    if (absolute_url.GetPath() == "/inline-script.html" ||
        absolute_url.GetPath() == "/same-inline-script.html") {
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);
      std::string title = absolute_url.GetPath() == "/inline-script.html"
                              ? "A Page"
                              : "A PageDifferent Page";
      // Run a short, heavy script before the test script to warmup connection
      // to the inline script cache backend.
      std::string content =
          base::StrCat({"<html><head><title>", title, "</title></head>",
                        "<body><script type='text/javascript'>",
                        GenerateUniqueHeavyJavascriptSource(),
                        "</script><script type='text/javascript'>",
                        GetCacheableLongStaticJavascriptSource(),
                        "</script></body></html>"});

      http_response->set_content(content);
      http_response->set_content_type("text/html");
      return http_response;
    }
    if (absolute_url.GetPath() == "/default-cachehint.html" ||
        absolute_url.GetPath() == "/eager-cachehint.html" ||
        absolute_url.GetPath() == "/never-cachehint.html") {
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);
      std::string cachehint_value = "default";
      if (absolute_url.GetPath() == "/eager-cachehint.html") {
        cachehint_value = "eager";
      } else if (absolute_url.GetPath() == "/never-cachehint.html") {
        cachehint_value = "never";
      }
      std::string content = base::StrCat(
          {"<html><head><title>Title</title></head><body>",
           "<script type='text/javascript'>",
           GenerateUniqueHeavyJavascriptSource(),
           "</script><script type='text/javascript' cachehint='",
           cachehint_value, "'>", GetCacheableLongStaticJavascriptSource(),
           "</script></body></html>\n"});
      http_response->set_content(content);
      http_response->set_content_type("text/html");
      return http_response;
    }
    if (absolute_url.GetPath() == "/short-inline-script.html") {
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);

      // HTML with a very short inline script.
      std::string short_script = "/*short*/";
      EXPECT_LT(short_script.length(),
                blink::features::kInlineScriptCacheMinScriptLength.Get());
      std::string content =
          base::StrCat({"<html><head><title>Title</title></head><body>",
                        "<script type='text/javascript'>", short_script,
                        "</script></body></html>\n"});

      http_response->set_content(content);
      http_response->set_content_type("text/html");
      return http_response;
    }
    if (absolute_url.GetPath() == "/empty.html") {
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);
      http_response->set_content("<html><body></body></html>");
      http_response->set_content_type("text/html");
      return http_response;
    }

    return nullptr;
  }

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

  void LoadIframe(const GURL& url) {
    EXPECT_TRUE(
        ExecJs(shell()->web_contents(),
               JsReplace("const iframe = document.createElement('iframe');"
                         "iframe.src = $1;"
                         "document.body.appendChild(iframe);",
                         url)));
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
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

 private:
  std::string GenerateUniqueHeavyJavascriptSource() {
    ++served_count_;
    // A time-consuming script to run.
    std::string source =
        base::StrCat({"const a=[...Array(10000000).keys()];let s=",
                      base::NumberToString(served_count_), ";",
                      "for(let i=0;i<a.length;i++)s+=i*(i+1)-a[i];"
                      "console.log(s);"});
    EXPECT_LT(source.length(),
              blink::features::kInlineScriptCacheMinScriptLength.Get());
    return source;
  }

  // A script longer enough to be cached.
  std::string GetCacheableLongStaticJavascriptSource() {
    return base::StrCat(
        {"function testFunction() {\n",
         std::string(blink::features::kInlineScriptCacheMinScriptLength.Get(),
                     '/'),
         "\nreturn 42;\n}"});
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  uint32_t served_count_ = 0;
};

class InlineScriptCacheHintBrowserTest
    : public InlineScriptCodeCacheBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  InlineScriptCacheHintBrowserTest()
      : InlineScriptCodeCacheBrowserTest(
            {{"enable_for_default_hint", GetParam() ? "true" : "false"}}) {}
};

INSTANTIATE_TEST_SUITE_P(All,
                         InlineScriptCacheHintBrowserTest,
                         testing::Bool());

// TODO(crbug.com/498265776): Test is expected to time out on some slow
// builders.
#if defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    ((BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)) && !defined(NDEBUG))
#define MAYBE_DefaultCacheHint DISABLED_DefaultCacheHint
#else
#define MAYBE_DefaultCacheHint DefaultCacheHint
#endif
IN_PROC_BROWSER_TEST_P(InlineScriptCacheHintBrowserTest,
                       MAYBE_DefaultCacheHint) {
  GURL url =
      embedded_test_server()->GetURL("example.com", "/default-cachehint.html");

  // Step 1: Load the page first time.
  {
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(NavigateToURL(shell(), url));
    FetchHistogramsFromChildProcesses();
    if (GetParam()) {
      histogram_tester.ExpectBucketCount(
          "V8.CompileScript.CacheBehaviour",
          CacheBehaviourNameToInt("kNoCacheBecauseInlineScriptCacheTooCold"),
          1);
    }
  }

  PurgeResourceCacheFromTheMainFrame();
  ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  // Step 2: Reload to produce cache.
  {
    bool produced = false;
    for (int i = 0; i < 5; i++) {
      base::HistogramTester histogram_tester;
      ASSERT_TRUE(NavigateToURL(shell(), url));
      FetchHistogramsFromChildProcesses();
      if (GetProduceCacheCount(histogram_tester) == 1) {
        produced = true;
        break;
      }
      PurgeResourceCacheFromTheMainFrame();
      ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
    }
    if (GetParam()) {
      EXPECT_TRUE(produced) << "Failed to produce cache";
    } else {
      EXPECT_FALSE(produced) << "Cache should not be produced";
    }
  }
}

// TODO(crbug.com/498265776): Test is expected to time out on some slow
// builders.
#if defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    (BUILDFLAG(IS_WIN) && !defined(NDEBUG))
#define MAYBE_EagerCacheHint DISABLED_EagerCacheHint
#else
#define MAYBE_EagerCacheHint EagerCacheHint
#endif
IN_PROC_BROWSER_TEST_P(InlineScriptCacheHintBrowserTest, MAYBE_EagerCacheHint) {
  GURL url =
      embedded_test_server()->GetURL("example.com", "/eager-cachehint.html");

  // Step 1: Load the page first time.
  {
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(NavigateToURL(shell(), url));
    FetchHistogramsFromChildProcesses();
    histogram_tester.ExpectBucketCount(
        "V8.CompileScript.CacheBehaviour",
        CacheBehaviourNameToInt("kNoCacheBecauseInlineScriptCacheTooCold"), 1);
  }

  PurgeResourceCacheFromTheMainFrame();
  ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  // Step 2: Reload to produce cache.
  {
    bool produced = false;
    for (int i = 0; i < 5; i++) {
      base::HistogramTester histogram_tester;
      ASSERT_TRUE(NavigateToURL(shell(), url));
      FetchHistogramsFromChildProcesses();
      if (GetProduceCacheCount(histogram_tester) == 1) {
        produced = true;
        break;
      }
      PurgeResourceCacheFromTheMainFrame();
      ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
    }
    EXPECT_TRUE(produced) << "Failed to produce cache";
  }
}

// TODO(crbug.com/498265776): Test is expected to time out on some slow
// builders.
#if defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    (BUILDFLAG(IS_WIN) && !defined(NDEBUG))
#define MAYBE_NeverCacheHint DISABLED_NeverCacheHint
#else
#define MAYBE_NeverCacheHint NeverCacheHint
#endif
IN_PROC_BROWSER_TEST_P(InlineScriptCacheHintBrowserTest, MAYBE_NeverCacheHint) {
  GURL url =
      embedded_test_server()->GetURL("example.com", "/never-cachehint.html");

  // Load multiple times, expect NO cache produced.
  const int num_tries = 3;
  base::HistogramTester histogram_tester;
  for (int i = 0; i < num_tries; i++) {
    ASSERT_TRUE(NavigateToURL(shell(), url));
    PurgeResourceCacheFromTheMainFrame();
    ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  }
  FetchHistogramsFromChildProcesses();
  EXPECT_EQ(GetProduceCacheCount(histogram_tester), 0);
  EXPECT_EQ(GetConsumeCacheCount(histogram_tester), 0);
}

// TODO(crbug.com/498265776): Test is failing on ChromeOS and Linux MSan.
// TODO(crbug.com/499507137): Disabled everywhere due to flakiness.
IN_PROC_BROWSER_TEST_F(InlineScriptCodeCacheBrowserTest,
                       DISABLED_CacheProducedOnSecondAttempt) {
  GURL url =
      embedded_test_server()->GetURL("example.com", "/inline-script.html");

  // Step 1: Load the page first time. No cache is used.
  {
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(NavigateToURL(shell(), url));
    FetchHistogramsFromChildProcesses();
    histogram_tester.ExpectBucketCount(
        "V8.CompileScript.CacheBehaviour",
        CacheBehaviourNameToInt("kNoCacheBecauseInlineScriptCacheTooCold"), 1);
  }

  PurgeResourceCacheFromTheMainFrame();
  ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  // Step 2: Reload the page. The script will be compiled from source and then
  // producing the cache to store it on persistent storage.
  {
    bool produced = false;
    for (int i = 0; i < 5; i++) {
      base::HistogramTester histogram_tester;
      ASSERT_TRUE(NavigateToURL(shell(), url));
      FetchHistogramsFromChildProcesses();
      if (GetProduceCacheCount(histogram_tester) == 1) {
        produced = true;
        break;
      }
      PurgeResourceCacheFromTheMainFrame();
      ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
    }
    EXPECT_TRUE(produced) << "Failed to produce cache";
  }

  PurgeResourceCacheFromTheMainFrame();
  ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  // Step 3: Reload the page again. The script will be compiled from code cache.

  {
    bool consumed = false;
    for (int i = 0; i < 5; i++) {
      base::HistogramTester histogram_tester;
      ASSERT_TRUE(NavigateToURL(shell(), url));
      FetchHistogramsFromChildProcesses();
      if (GetConsumeCacheCount(histogram_tester) == 1) {
        consumed = true;
        break;
      }
      PurgeResourceCacheFromTheMainFrame();
      ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
    }
    EXPECT_TRUE(consumed) << "Failed to consume cache";
  }
}

// TODO(crbug.com/498265776): Test is failing on ChromeOS and Linux MSan.
// TODO(crbug.com/499208353): Disabled everywhere for flakiness.
IN_PROC_BROWSER_TEST_F(InlineScriptCodeCacheBrowserTest,
                       DISABLED_CacheSharedOnDifferentPage) {
  GURL url_1 =
      embedded_test_server()->GetURL("example.com", "/inline-script.html");
  GURL url_2 =
      embedded_test_server()->GetURL("example.com", "/same-inline-script.html");

  // Step 1: Load the page first time. No cache is used.
  {
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(NavigateToURL(shell(), url_1));
    FetchHistogramsFromChildProcesses();
    histogram_tester.ExpectBucketCount(
        "V8.CompileScript.CacheBehaviour",
        CacheBehaviourNameToInt("kNoCacheBecauseInlineScriptCacheTooCold"), 1);
  }

  PurgeResourceCacheFromTheMainFrame();
  ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  // Step 2: Reload the page. The script will be compiled from source and then
  // producing the cache to store it on persistent storage.
  {
    bool produced = false;
    for (int i = 0; i < 5; i++) {
      base::HistogramTester histogram_tester;
      ASSERT_TRUE(NavigateToURL(shell(), url_1));
      FetchHistogramsFromChildProcesses();
      if (GetProduceCacheCount(histogram_tester) == 1) {
        produced = true;
        break;
      }
      PurgeResourceCacheFromTheMainFrame();
      ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
    }
    EXPECT_TRUE(produced) << "Failed to produce cache";
  }

  PurgeResourceCacheFromTheMainFrame();
  ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  // Step 3: Load the different URL with the same NIK and the same inline
  // script. The script will be compiled from code cache.
  {
    bool consumed = false;
    for (int i = 0; i < 5; i++) {
      base::HistogramTester histogram_tester;
      ASSERT_TRUE(NavigateToURL(shell(), url_2));
      FetchHistogramsFromChildProcesses();
      if (GetConsumeCacheCount(histogram_tester) == 1) {
        consumed = true;
        break;
      }
      PurgeResourceCacheFromTheMainFrame();
      ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
    }
    EXPECT_TRUE(consumed) << "Failed to consume cache";
  }
}

#if BUILDFLAG(IS_LINUX) && defined(MEMORY_SANITIZER)
// TODO(https://crbug.com/498800334): Flaky.
#define MAYBE_NotProducedForShortScript DISABLED_NotProducedForShortScript
#else
#define MAYBE_NotProducedForShortScript NotProducedForShortScript
#endif
IN_PROC_BROWSER_TEST_F(InlineScriptCodeCacheBrowserTest,
                       MAYBE_NotProducedForShortScript) {
  // Even after multiple page loads, the code cache should not be produced for
  // short scripts.
  const int num_tries = 3;
  base::HistogramTester histogram_tester;
  for (int i = 0; i < num_tries; i++) {
    // Use different query parameters so that BFCache would not work.
    GURL url = embedded_test_server()->GetURL(
        "example.com", base::StrCat({"/short-inline-script.html?i=",
                                     base::NumberToString(i)}));
    ASSERT_TRUE(NavigateToURL(shell(), url));
    PurgeResourceCacheFromTheMainFrame();
    ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  }
  FetchHistogramsFromChildProcesses();
  histogram_tester.ExpectBucketCount(
      "V8.CompileScript.CacheBehaviour",
      CacheBehaviourNameToInt("kNoCacheBecauseScriptTooSmall"), num_tries);
}

// TODO(crbug.com/498265776): Test is failing on ChromeOS and Linux MSan.
// TODO(crbug.com/499371224): Disabled everywhere for flakiness.
IN_PROC_BROWSER_TEST_F(InlineScriptCodeCacheBrowserTest,
                       DISABLED_IsolatedByNik) {
  GURL top_domain_a =
      embedded_test_server()->GetURL("a.example", "/empty.html");
  GURL iframe_domain_b =
      embedded_test_server()->GetURL("b.example", "/inline-script.html");
  GURL top_domain_c =
      embedded_test_server()->GetURL("c.example", "/empty.html");

  // Step 1: Load top_domain_a and embed iframe_domain_b. Cache is cold.
  {
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(NavigateToURL(shell(), top_domain_a));
    LoadIframe(iframe_domain_b);
    FetchHistogramsFromChildProcesses();
    histogram_tester.ExpectBucketCount(
        "V8.CompileScript.CacheBehaviour",
        CacheBehaviourNameToInt("kNoCacheBecauseInlineScriptCacheTooCold"), 1);
  }

  PurgeResourceCacheFromTheMainFrame();
  PurgeResourceCacheFromTheFirstSubFrame();
  ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  // Step 2: Reload the same setup (top A, iframe B). Cache should be produced
  // or it could hit the Isolate cache if the process is reused.
  {
    bool produced = false;
    for (int i = 0; i < 5; i++) {
      base::HistogramTester histogram_tester;
      ASSERT_TRUE(NavigateToURL(shell(), top_domain_a));
      LoadIframe(iframe_domain_b);
      FetchHistogramsFromChildProcesses();
      if (GetProduceCacheCount(histogram_tester) == 1) {
        produced = true;
        break;
      }
      PurgeResourceCacheFromTheMainFrame();
      PurgeResourceCacheFromTheFirstSubFrame();
      ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
    }
    EXPECT_TRUE(produced) << "Failed to produce cache";
  }

  PurgeResourceCacheFromTheMainFrame();
  PurgeResourceCacheFromTheFirstSubFrame();
  ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  // Step 3: Load top_domain_c and embed iframe_domain_b.
  // Since NIK splits cache, there should be no inline script cache created by
  // domain A while it could hit the Isolate cache if the process is reused.
  {
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(NavigateToURL(shell(), top_domain_c));
    LoadIframe(iframe_domain_b);
    FetchHistogramsFromChildProcesses();

    int no_cache_count =
        histogram_tester.GetBucketCount(
            "V8.CompileScript.CacheBehaviour",
            CacheBehaviourNameToInt(
                "kNoCacheBecauseInlineScriptCacheTooCold")) +
        histogram_tester.GetBucketCount(
            "V8.CompileScript.CacheBehaviour",
            CacheBehaviourNameToInt("kHitIsolateCacheWhenNoCache"));
    EXPECT_EQ(no_cache_count, 1);
  }
}

// TODO(crbug.com/498265776): Test timed out on some slow builders like MSan,
// TSan, and Win 10 (dbg).
#if defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    (BUILDFLAG(IS_WIN) && !defined(NDEBUG))
#define MAYBE_ProducedCacheHitsOnAnotherProcess \
  DISABLED_ProducedCacheHitsOnAnotherProcess
#else
#define MAYBE_ProducedCacheHitsOnAnotherProcess \
  ProducedCacheHitsOnAnotherProcess
#endif
IN_PROC_BROWSER_TEST_F(InlineScriptCodeCacheBrowserTest,
                       MAYBE_ProducedCacheHitsOnAnotherProcess) {
  GURL url = embedded_test_server()->GetURL("a.example", "/inline-script.html");

  // Step 1: Load the page first time.
  {
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(NavigateToURL(shell(), url));
    FetchHistogramsFromChildProcesses();
    histogram_tester.ExpectBucketCount(
        "V8.CompileScript.CacheBehaviour",
        CacheBehaviourNameToInt("kNoCacheBecauseInlineScriptCacheTooCold"), 1);
  }

  PurgeResourceCacheFromTheMainFrame();
  ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  // Step 2: Reload the page to produce the cache.
  {
    bool produced = false;
    for (int i = 0; i < 5; i++) {
      base::HistogramTester histogram_tester;
      ASSERT_TRUE(NavigateToURL(shell(), url));
      FetchHistogramsFromChildProcesses();
      if (GetProduceCacheCount(histogram_tester) == 1) {
        produced = true;
        break;
      }
      PurgeResourceCacheFromTheMainFrame();
      ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
    }
    EXPECT_TRUE(produced) << "Failed to produce cache";
  }

  // Step 3: Try recreating the window to ensure the next load happens in a
  // new renderer process. If fail, skip the step 4.
  // TODO(crbug.com/512201557): Make sure to use another process or to clear
  // every in-memory cache of the process for test reliability.
  const ChildProcessId process_id_before_recreate =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
  RecreateWindow();
  const ChildProcessId process_id_after_recreate =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();

  if (process_id_before_recreate != process_id_after_recreate) {
    // Step 4: Load the page again. Since it's a new process, it cannot hit the
    // in-memory Isolate cache. It must strictly hit the persistent code cache.
    bool consumed = false;
    for (int i = 0; i < 5; i++) {
      base::HistogramTester histogram_tester;
      ASSERT_TRUE(NavigateToURL(shell(), url));
      FetchHistogramsFromChildProcesses();
      const int persistent_consume_count = histogram_tester.GetBucketCount(
          "V8.CompileScript.CacheBehaviour",
          CacheBehaviourNameToInt("kConsumeCodeCache"));
      if (persistent_consume_count == 1) {
        consumed = true;
        break;
      }
      PurgeResourceCacheFromTheMainFrame();
      ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
    }
    EXPECT_TRUE(consumed) << "Failed to consume persistent code cache "
                             "(kConsumeCodeCache) in the new process.";
  }
}

}  // namespace content
#endif  // !BUILDFLAG(IS_FUCHSIA)
