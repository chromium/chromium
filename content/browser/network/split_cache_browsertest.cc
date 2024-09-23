// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>

#include "base/path_service.h"
#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/back_forward_cache_disable.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/renderer.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/resource_load_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/features.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/scoped_mutually_exclusive_feature_list.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "url/gurl.h"

namespace content {
namespace {

class SplitCacheContentBrowserTest : public ContentBrowserTest {
 public:
  enum class Context { kMainFrame, kSameOriginFrame, kCrossOriginFrame };

  SplitCacheContentBrowserTest() = default;

  SplitCacheContentBrowserTest(const SplitCacheContentBrowserTest&) = delete;
  SplitCacheContentBrowserTest& operator=(const SplitCacheContentBrowserTest&) =
      delete;

  void SetUp() override {
    RenderWidgetHostImpl::DisableResizeAckCheckForTesting();
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Setup the server to allow serving separate sites, so we can perform
    // cross-process navigation.
    host_resolver()->AddRule("*", "127.0.0.1");

    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&SplitCacheContentBrowserTest::CachedScriptHandler,
                            base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  std::unique_ptr<net::test_server::HttpResponse> CachedScriptHandler(
      const net::test_server::HttpRequest& request) {
    GURL absolute_url = embedded_test_server()->GetURL(request.relative_url);

    // Return a page that redirects to d.com/title1.html.
    if (absolute_url.path() == "/redirect_to_d") {
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_SEE_OTHER);
      http_response->AddCustomHeader(
          "Location",
          embedded_test_server()->GetURL("d.com", "/title1.html").spec());
      return http_response;
    }

    // Return valid cacheable script.
    if (absolute_url.path() == "/script") {
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);
      http_response->set_content("console.log(\"Hello World\");");
      http_response->set_content_type("application/javascript");
      http_response->AddCustomHeader("Cache-Control", "max-age=1000");
      return http_response;
    }

    // A basic cacheable worker that loads 3p.com/script
    if (absolute_url.path() == "/worker.js") {
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);

      GURL resource = GenURL("3p.com", "/script");
      // Self-terminate the worker just after loading the third party
      // script, so that the parent context doesn't need to wait for the
      // worker's termination when cleaning up the test. See
      // https://crbug.com/1104847 for more details.
      std::string content = base::StringPrintf("importScripts('%s');\nclose();",
                                               resource.spec().c_str());

      http_response->set_content(content);
      http_response->set_content_type("application/javascript");
      http_response->AddCustomHeader("Cache-Control", "max-age=100000");
      return http_response;
    }

    // Make the document resource cacheable.
    if (absolute_url.path() == "/title1.html") {
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);
      http_response->AddCustomHeader("Cache-Control", "max-age=100000");
      return http_response;
    }

    // A cacheable worker that loads a nested worker on an origin provided
    // as a query param.
    if (absolute_url.path() == "/embedding_worker.js") {
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);

      GURL resource =
          GenURL(base::StringPrintf("%s.com", absolute_url.query().c_str()),
                 "/worker.js");

      const char kLoadWorkerScript[] = "let w = new Worker('%s');";
      std::string content =
          base::StringPrintf(kLoadWorkerScript, resource.spec().c_str());

      http_response->set_content(content);
      http_response->set_content_type("application/javascript");
      http_response->AddCustomHeader("Cache-Control", "max-age=100000");
      return http_response;
    }

    return nullptr;
  }

 protected:
  // Creates and loads subframe, waits for load to stop, and then returns
  // subframe from the web contents frame tree.
  RenderFrameHost* CreateSubframe(const GURL& sub_frame) {
    EXPECT_TRUE(ExecJs(shell(), GetSubframeScript(sub_frame)));
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

    return static_cast<WebContentsImpl*>(shell()->web_contents())
        ->GetPrimaryFrameTree()
        .root()
        ->child_at(0)
        ->current_frame_host();
  }

  // Loads 3p.com/script on page |url|, optionally from |sub_frame| if it's
  // valid, and returns whether the script was cached or not.
  bool TestResourceLoad(const GURL& url, const GURL& sub_frame) {
    return TestResourceLoadHelper(url, sub_frame, GURL());
  }

  // Loads 3p.com/script on page |url| from |worker| and returns whether
  // the script was cached or not.
  bool TestResourceLoadFromDedicatedWorker(const GURL& url,
                                           const GURL& worker) {
    DCHECK(worker.is_valid());
    return TestResourceLoadHelper(url, GURL(), worker);
  }

  // Loads 3p.com/script on page |url| from |worker| inside |sub_frame|
  // and returns whether the script was cached or not.
  bool TestResourceLoadFromDedicatedWorkerInIframe(const GURL& url,
                                                   const GURL& sub_frame,
                                                   const GURL& worker) {
    DCHECK(sub_frame.is_valid());
    DCHECK(worker.is_valid());
    return TestResourceLoadHelper(url, sub_frame, worker);
  }

  // Loads 3p.com/script on |popup| opened from page |url| and returns whether
  // the script was cached or not.
  bool TestResourceLoadFromPopup(const GURL& url, const GURL& popup) {
    DCHECK(popup.is_valid());
    return TestResourceLoadHelper(url, popup, GURL(), true);
  }

  // Loads 3p.com/script on page |url|. If |new_frame| is valid, it is loaded
  // from a new frame with that url; otherwise, it is loaded from the main
  // frame. This new frame is a popup if |use_popup|; otherwise, it is a
  // subframe. The load is optionally performed by |worker| if it's valid.
  bool TestResourceLoadHelper(const GURL& url,
                              const GURL& new_frame,
                              const GURL& worker,
                              bool use_popup = false) {
    DCHECK(url.is_valid());

    // Allocate a new process to prevent using the in-memory cache.
    // 1) Prevent the old page from entering the back-forward cache. Otherwise
    //    the old process will be kept alive, because it is still being used.
    // 2) Navigate to a WebUI URL, which uses a new process.
    DisableBFCacheForRFHForTesting(
        shell()->web_contents()->GetPrimaryMainFrame());
    EXPECT_TRUE(NavigateToURL(shell(), GetWebUIURL("blob-internals")));

    // In the case of a redirect, the observed URL will be different from
    // what NavigateToURL(...) expects.
    if (base::StartsWith(url.path(), "/redirect", base::CompareCase::SENSITIVE))
      EXPECT_FALSE(NavigateToURL(shell(), url));
    else
      EXPECT_TRUE(NavigateToURL(shell(), url));

    RenderFrameHost* host_to_load_resource =
        shell()->web_contents()->GetPrimaryMainFrame();
    RenderFrameHostImpl* main_frame =
        static_cast<RenderFrameHostImpl*>(host_to_load_resource);

    Shell* shell_to_observe = shell();

    if (new_frame.is_valid()) {
      // If there is supposed to be a subframe or popup, create it.
      if (use_popup) {
        shell_to_observe = OpenPopup(main_frame, new_frame, "");
        host_to_load_resource =
            static_cast<WebContentsImpl*>(shell_to_observe->web_contents())
                ->GetPrimaryMainFrame();
      } else {
        host_to_load_resource = CreateSubframe(new_frame);
      }
    }

    // `shell_to_observe` may still contain responses depending on process reuse
    // policies. Clear the in-memory cache in `shell_to_observe` to make sure
    // the following ResourceLoadObserver can observe network requests.
    base::RunLoop loop;
    shell_to_observe->web_contents()
        ->GetPrimaryMainFrame()
        ->GetProcess()
        ->GetRendererInterface()
        ->PurgeResourceCache(loop.QuitClosure());
    loop.Run();
    // Observe network requests.
    ResourceLoadObserver observer(shell_to_observe);

    GURL resource = GenURL("3p.com", "/script");

    // If there is supposed to be a worker to load this resource, create it.
    // Otherwise, load the resource directly.
    if (worker.is_valid()) {
      EXPECT_TRUE(ExecJs(host_to_load_resource, GetWorkerScript(worker)));
    } else {
      EXPECT_TRUE(
          ExecJs(host_to_load_resource, GetLoadResourceScript(resource)));
    }

    observer.WaitForResourceCompletion(resource);

    // Test the network isolation key.
    url::Origin top_frame_origin =
        main_frame->frame_tree_node()->current_origin();

    RenderFrameHostImpl* frame_host =
        static_cast<RenderFrameHostImpl*>(host_to_load_resource);
    url::Origin frame_origin;
    if (new_frame.is_empty()) {
      frame_origin = top_frame_origin;
    } else {
      frame_origin = url::Origin::Create(new_frame);
      if (use_popup && !frame_origin.opaque()) {
        // The popup is in a new WebContents, so its top_frame_origin is also
        // new unless it is blank.
        top_frame_origin = frame_origin;
      } else {
        // Take redirects and initially empty subframes/popups into account.
        frame_origin = frame_host->GetLastCommittedOrigin();
      }
    }

    if (!top_frame_origin.opaque() && !frame_origin.opaque()) {
      EXPECT_EQ(net::NetworkIsolationKey(net::SchemefulSite(top_frame_origin),
                                         net::SchemefulSite(frame_origin)),
                frame_host->GetNetworkIsolationKey());
    } else {
      EXPECT_TRUE(frame_host->GetNetworkIsolationKey().IsTransient());
    }

    return (*observer.GetResource(resource))->was_cached;
  }

  // Navigates to |url| and returns if the navigation resource was fetched from
  // the cache or not.
  bool NavigationResourceCached(const GURL& url,
                                const GURL& sub_frame,
                                bool subframe_navigation_resource_cached) {
    return NavigationResourceCached(url, url, sub_frame,
                                    subframe_navigation_resource_cached);
  }

  // Same as above, but allows explicitly specifying the expected commit URL
  // for the navigation to |url|, in case it differs.
  bool NavigationResourceCached(const GURL& url,
                                const GURL& expected_commit_url,
                                const GURL& sub_frame,
                                bool subframe_navigation_resource_cached) {
    // Do a cross-process navigation to clear the in-memory cache.
    // We assume that we don't start this call from "chrome://blob-internals",
    // as otherwise it won't be a cross-process navigation. We are relying on
    // this navigation to discard the old process.
    EXPECT_TRUE(NavigateToURL(shell(), GetWebUIURL("blob-internals")));

    // Observe network requests.
    ResourceLoadObserver observer(shell());

    EXPECT_TRUE(NavigateToURL(shell(), url, expected_commit_url));

    RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetPrimaryMainFrame());

    observer.WaitForResourceCompletion(url);

    if (sub_frame.is_valid()) {
      EXPECT_EQ(1U, main_frame->frame_tree_node()->child_count());
      NavigateFrameToURL(main_frame->frame_tree_node()->child_at(0), sub_frame);
      EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
      observer.WaitForResourceCompletion(sub_frame);
      EXPECT_EQ(subframe_navigation_resource_cached,
                (*observer.GetResource(sub_frame))->was_cached);
    }

    return (*observer.GetResource(url))->was_cached;
  }

  // Loads a dedicated worker script and checks to see whether or not the
  // script was cached.
  bool DedicatedWorkerScriptCached(const GURL& url,
                                   const GURL& sub_frame,
                                   const GURL& worker) {
    DCHECK(url.is_valid());
    DCHECK(worker.is_valid());

    // Do a cross-process navigation to clear the in-memory cache.
    // We assume that we don't start this call from "chrome://blob-internals",
    // as otherwise it won't be a cross-process navigation. We are relying on
    // this navigation to discard the old process.
    EXPECT_TRUE(NavigateToURL(shell(), GetWebUIURL("blob-internals")));

    // Observe network requests.
    ResourceLoadObserver observer(shell());

    EXPECT_TRUE(NavigateToURL(shell(), url));

    RenderFrameHost* host_to_load_resource =
        shell()->web_contents()->GetPrimaryMainFrame();

    // If there is supposed to be a subframe, create it.
    if (sub_frame.is_valid()) {
      host_to_load_resource = CreateSubframe(sub_frame);
    }

    EXPECT_TRUE(ExecJs(host_to_load_resource, GetWorkerScript(worker)));

    observer.WaitForResourceCompletion(GenURL("3p.com", "/script"));
    observer.WaitForResourceCompletion(worker);

    return (*observer.GetResource(worker))->was_cached;
  }

  bool NavigationRedirectCached(const GURL& url, const GURL& redirect_url) {
    // Do a cross-process navigation to clear the in-memory cache.
    // We assume that we don't start this call from "chrome://blob-internals",
    // as otherwise it won't be a cross-process navigation. We are relying on
    // this navigation to discard the old process.
    EXPECT_TRUE(NavigateToURL(shell(), GetWebUIURL("blob-internals")));

    EXPECT_TRUE(NavigateToURL(shell(), url));

    // Observe the redirect.
    ResourceLoadObserver observer(shell());
    EXPECT_TRUE(ExecJs(shell(), GetRedirectScript(redirect_url)));
    WaitForLoadStop(shell()->web_contents());
    observer.WaitForResourceCompletion(redirect_url);

    return (*observer.GetResource(redirect_url))->was_cached;
  }

  // Gets script to create subframe.
  std::string GetSubframeScript(const GURL& sub_frame) {
    const char kLoadIframeScript[] = R"(
        let iframe = document.createElement('iframe');
        iframe.src = $1;
        document.body.appendChild(iframe);
      )";
    return JsReplace(kLoadIframeScript, sub_frame);
  }

  // Gets script to create worker.
  std::string GetWorkerScript(const GURL& worker) {
    const char kLoadWorkerScript[] = "let w = new Worker($1);";
    return JsReplace(kLoadWorkerScript, worker);
  }

  // Gets script to load resource.
  std::string GetLoadResourceScript(const GURL& resource) {
    const char kLoadResourceScript[] = R"(
        let script = document.createElement('script');
        script.src = $1;
        document.body.appendChild(script);
      )";
    return JsReplace(kLoadResourceScript, resource);
  }

  // Gets script to redirect via JavaScript.
  std::string GetRedirectScript(const GURL& location) {
    const char kRedirectScript[] = R"(
        window.location.href = $1;
      )";
    return JsReplace(kRedirectScript, location);
  }

  GURL GenURL(const std::string& host, const std::string& path) {
    return embedded_test_server()->GetURL(host, path);
  }
};

enum class SplitCacheTestCase {
  kEnabledTripleKeyed,
  kEnabledTriplePlusCrossSiteMainFrameNavBool,
  kEnabledTriplePlusMainFrameNavInitiator,
  kEnabledTriplePlusNavInitiator
};

const struct {
  const SplitCacheTestCase test_case;
  base::test::FeatureRef feature;
} kTestCaseToFeatureMapping[] = {
    {SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool,
     net::features::kSplitCacheByCrossSiteMainFrameNavigationBoolean},
    {SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator,
     net::features::kSplitCacheByMainFrameNavigationInitiator},
    {SplitCacheTestCase::kEnabledTriplePlusNavInitiator,
     net::features::kSplitCacheByNavigationInitiator}};

class SplitCacheContentBrowserTestEnabled
    : public SplitCacheContentBrowserTest,
      public testing::WithParamInterface<SplitCacheTestCase> {
 public:
  SplitCacheContentBrowserTestEnabled()
      : split_cache_experiment_feature_list_(GetParam(),
                                             kTestCaseToFeatureMapping) {
    split_cache_always_enabled_feature_list_.InitAndEnableFeature(
        net::features::kSplitCacheByNetworkIsolationKey);
  }

 private:
  net::test::ScopedMutuallyExclusiveFeatureList
      split_cache_experiment_feature_list_;
  base::test::ScopedFeatureList split_cache_always_enabled_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SplitCacheContentBrowserTestEnabled,
    testing::ValuesIn(
        {SplitCacheTestCase::kEnabledTripleKeyed,
         SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool,
         SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator,
         SplitCacheTestCase::kEnabledTriplePlusNavInitiator}),
    [](const testing::TestParamInfo<SplitCacheTestCase>& info) {
      switch (info.param) {
        case SplitCacheTestCase::kEnabledTripleKeyed:
          return "SplitCacheEnabledTripleKeyed";
        case SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool:
          return "SplitCacheEnabledTriplePlusCrossSiteMainFrameNavigationBool";
        case SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator:
          return "SplitCacheEnabledTriplePlusMainFrameNavigationInitiator";
        case SplitCacheTestCase::kEnabledTriplePlusNavInitiator:
          return "SplitCacheEnabledTriplePlusNavigationInitiator";
      }
    });

class SplitCacheContentBrowserTestPlzDedicatedWorker
    : public SplitCacheContentBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  SplitCacheContentBrowserTestPlzDedicatedWorker() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    enabled_features.push_back(net::features::kSplitCacheByNetworkIsolationKey);

    // When the test parameter is true, we test the split cache with
    // PlzDedicatedWorker enabled.
    if (GetParam()) {
      enabled_features.push_back(blink::features::kPlzDedicatedWorker);
    } else {
      disabled_features.push_back(blink::features::kPlzDedicatedWorker);
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class SplitCacheContentBrowserTestDisabled
    : public SplitCacheContentBrowserTest {
 public:
  SplitCacheContentBrowserTestDisabled() {
    feature_list_.InitAndDisableFeature(
        net::features::kSplitCacheByNetworkIsolationKey);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(SplitCacheContentBrowserTestEnabled, MainFrame) {
  // Load a cacheable resource for the first time, and it's not cached.
  EXPECT_FALSE(TestResourceLoad(GenURL("a.com", "/title1.html"), GURL()));

  // The second time, it's cached.
  EXPECT_TRUE(TestResourceLoad(GenURL("a.com", "/title1.html"), GURL()));

  // Now load it from a different site, and the resource isn't cached because
  // the top frame origin is different.
  EXPECT_FALSE(TestResourceLoad(GenURL("b.com", "/title1.html"), GURL()));

  // Now load it from 3p.com, which is same-site to the cacheable
  // resource. Still not supposed to be cached.
  EXPECT_FALSE(TestResourceLoad(GenURL("3p.com", "/title1.html"), GURL()));
}

IN_PROC_BROWSER_TEST_P(SplitCacheContentBrowserTestEnabled, MainFrameRedirect) {
  // Load a cacheable resource for the first time, and it's not cached.
  EXPECT_FALSE(TestResourceLoad(GenURL("a.com", "/title1.html"), GURL()));
  ASSERT_TRUE(TestResourceLoad(GenURL("a.com", "/title1.html"), GURL()));

  // Load it from a a.com/redirect_to_d which redirects to d.com/title1.html and
  // the resource shouldn't be cached because now we're on d.com.
  EXPECT_FALSE(TestResourceLoad(GenURL("a.com", "/redirect_to_d"), GURL()));

  // Navigate to d.com directly. The main resource should be cached due to the
  // earlier navigation.
  EXPECT_TRUE(TestResourceLoad(GenURL("d.com", "/title1.html"), GURL()));
}

IN_PROC_BROWSER_TEST_P(SplitCacheContentBrowserTestEnabled, Subframe) {
  // Load a cacheable resource for the first time, and it's not cached.
  EXPECT_FALSE(TestResourceLoad(GenURL("a.com", "/title1.html"), GURL()));
  ASSERT_TRUE(TestResourceLoad(GenURL("a.com", "/title1.html"), GURL()));

  // Load the resource from a same-origin iframe on a page where it's already
  // cached. It should still be cached.
  EXPECT_TRUE(TestResourceLoad(GenURL("a.com", "/title1.html"),
                               GenURL("a.com", "/title1.html")));

  // Load the resource from a cross-origin iframe on a page where the
  // iframe hasn't been cached previously.
  EXPECT_FALSE(TestResourceLoad(GenURL("a.com", "/title1.html"),
                                GenURL("e.com", "/title1.html")));
  EXPECT_TRUE(TestResourceLoad(GenURL("a.com", "/title1.html"),
                               GenURL("e.com", "/title1.html")));

  // Load the resource from a same-origin iframe on a page where it's not
  // cached. It should not be cached.
  EXPECT_FALSE(TestResourceLoad(GenURL("e.com", "/title1.html"),
                                GenURL("e.com", "/title1.html")));
  EXPECT_TRUE(TestResourceLoad(GenURL("e.com", "/title1.html"),
                               GenURL("e.com", "/title1.html")));

  // Load the resource from a cross-origin iframe where the iframe's origin has
  // seen the object before but the top frame hasn't. It should not be cached.
  EXPECT_FALSE(TestResourceLoad(GenURL("f.com", "/title1.html"),
                                GenURL("a.com", "/title1.html")));
}

IN_PROC_BROWSER_TEST_P(SplitCacheContentBrowserTestEnabled, MainFrameDataUrl) {
  // Load a cacheable resource for the first time, and it's not cached.
  EXPECT_FALSE(TestResourceLoad(GenURL("a.com", "/title1.html"), GURL()));
  ASSERT_TRUE(TestResourceLoad(GenURL("a.com", "/title1.html"), GURL()));

  // Load the resource from a data url which has an opaque origin. It shouldn't
  // be cached.
  GURL data_url("data:text/html,<body>Hello World</body>");
  EXPECT_FALSE(TestResourceLoad(data_url, GURL()));

  // Load the same resource from the same data url, it shouldn't be cached
  // because the origin should be unique.
  EXPECT_FALSE(TestResourceLoad(data_url, GURL()));
}

IN_PROC_BROWSER_TEST_P(SplitCacheContentBrowserTestEnabled,
                       MainFrameAboutBlank) {
  // Load a cacheable resource for the first time, and it's not cached.
  EXPECT_FALSE(TestResourceLoad(GenURL("a.com", "/title1.html"), GURL()));
  ASSERT_TRUE(TestResourceLoad(GenURL("a.com", "/title1.html"), GURL()));

  // Load the resource from a document that points to about:blank.
  GURL blank_url(url::kAboutBlankURL);
  EXPECT_FALSE(TestResourceLoad(blank_url, GURL()));

  // Load the same resource from about:blank url again, it shouldn't be cached
  // because the origin is unique. TODO(crbug.com/40092527) will change this
  // behavior and about:blank main frame pages will inherit the origin of the
  // page that opened it.
  EXPECT_FALSE(TestResourceLoad(blank_url, GURL()));
}

IN_PROC_BROWSER_TEST_P(SplitCacheContentBrowserTestEnabled, SubFrameRedirect) {
  // Load a cacheable resource for the first time, and it's not cached.
  EXPECT_FALSE(TestResourceLoad(GenURL("a.com", "/title1.html"), GURL()));
  ASSERT_TRUE(TestResourceLoad(GenURL("a.com", "/title1.html"), GURL()));

  // Load it from a a.com/redirect_to_d which redirects to d.com/title1.html and
  // the resource shouldn't be cached because now we're on d.com.
  EXPECT_FALSE(TestResourceLoad(GenURL("a.com", "/title1.html"),
                                GenURL("a.com", "/redirect_to_d")));

  // Now load it from the d.com iframe directly. It should be cached.
  EXPECT_TRUE(TestResourceLoad(GenURL("a.com", "/title1.html"),
                               GenURL("d.com", "/title1.html")));
}

IN_PROC_BROWSER_TEST_P(SplitCacheContentBrowserTestEnabled, SubFrameDataUrl) {
  // Load a cacheable resource for the first time, and it's not cached.
  EXPECT_FALSE(TestResourceLoad(GenURL("a.com", "/title1.html"), GURL()));
  ASSERT_TRUE(TestResourceLoad(GenURL("a.com", "/title1.html"), GURL()));

  // Load the resource from a data url which has an opaque origin. It shouldn't
  // be cached.
  GURL data_url("data:text/html,<body>Hello World</body>");
  EXPECT_FALSE(TestResourceLoad(GenURL("a.com", "/title1.html"), data_url));

  // Load the same resource from the same data url. It shouldn't be cached
  // because the cache isn't used for transient origins.
  EXPECT_FALSE(TestResourceLoad(GenURL("a.com", "/title1.html"), data_url));
}

IN_PROC_BROWSER_TEST_P(SplitCacheContentBrowserTestEnabled,
                       SubframeAboutBlank) {
  // Load a cacheable resource for the first time, and it's not cached.
  EXPECT_FALSE(TestResourceLoad(GenURL("a.com", "/title1.html"), GURL()));
  ASSERT_TRUE(TestResourceLoad(GenURL("a.com", "/title1.html"), GURL()));

  // Load the resource from a subframe document that points to about:blank. The
  // resource is cached because the resource load is using the main frame's
  // URLLoaderFactory and main frame's factory has the NIK set to
  // (a.com, a.com) which is already in the cache.
  GURL blank_url(url::kAboutBlankURL);
  EXPECT_TRUE(TestResourceLoad(GenURL("a.com", "/title1.html"), blank_url));

  // Load the resource from a popup window that points to about:blank. The
  // resource is cached because the resource load is using the original main
  // frame's URLLoaderFactory and the original main frame's factory has the NIK
  // set to (a.com, a.com) which is already in the cache.
  EXPECT_TRUE(
      TestResourceLoadFromPopup(GenURL("a.com", "/title1.html"), blank_url));
}

IN_PROC_BROWSER_TEST_P(SplitCacheContentBrowserTestEnabled, Popup) {
  // Load a cacheable resource for the first time, and it's not cached.
  EXPECT_FALSE(TestResourceLoad(GenURL("a.com", "/title1.html"), GURL()));
  ASSERT_TRUE(TestResourceLoad(GenURL("a.com", "/title1.html"), GURL()));

  // Load the resource from a popup window that points to a new origin. The
  // resource is not cached because the resource load is using a NIK set to
  // (g.com, g.com).
  EXPECT_FALSE(TestResourceLoadFromPopup(GenURL("a.com", "/title1.html"),
                                         GenURL("g.com", "/title1.html")));

  // Load a cacheable resource for the first time, and it's not cached.
  EXPECT_FALSE(TestResourceLoad(GenURL("a.foo.com", "/title1.html"), GURL()));

  // The second time, it's cached when accessed with the same eTLD+1.
  EXPECT_TRUE(TestResourceLoad(GenURL("b.foo.com", "/title1.html"), GURL()));

  // Now load it from a different site, and the resource isn't cached.
  EXPECT_FALSE(TestResourceLoad(GenURL("b.com", "/title1.html"), GURL()));

  // Now load it from 3p.com, which is same-site to the cacheable
  // resource. Still not supposed to be cached.
  EXPECT_FALSE(TestResourceLoad(GenURL("3p.com", "/title1.html"), GURL()));

  // Test case with iframe. This should be a cache hit since the network
  // isolation key is (foo.com, foo.com) as in the first case.
  EXPECT_TRUE(TestResourceLoad(GenURL("a.foo.com", "/title1.html"),
                               GenURL("iframe.foo.com", "/title1.html")));
}

IN_PROC_BROWSER_TEST_F(SplitCacheContentBrowserTestDisabled, NonSplitCache) {
  // Load a cacheable resource for the first time, and it's not cached.
  EXPECT_FALSE(TestResourceLoad(GenURL("a.com", "/title1.html"), GURL()));

  // The second time, it's cached.
  EXPECT_TRUE(TestResourceLoad(GenURL("a.com", "/title1.html"), GURL()));

  // Now load it from a different site, and the resource is cached.
  EXPECT_TRUE(TestResourceLoad(GenURL("b.com", "/title1.html"), GURL()));

  // Load it from a cross-origin iframe, and it's still cached.
  EXPECT_TRUE(TestResourceLoad(GenURL("b.com", "/title1.html"),
                               GenURL("c.com", "/title1.html")));
}

IN_PROC_BROWSER_TEST_P(SplitCacheContentBrowserTestEnabled,
                       NavigationResources) {
  // Navigate for the first time, and it's not cached.
  EXPECT_FALSE(
      NavigationResourceCached(GenURL("a.com", "/title1.html"), GURL(), false));

  // The second time, it's cached.
  EXPECT_TRUE(
      NavigationResourceCached(GenURL("a.com", "/title1.html"), GURL(), false));

  // Navigate to a.com/redirect_to_d which redirects to d.com/title1.html.
  EXPECT_FALSE(NavigationResourceCached(GenURL("a.com", "/redirect_to_d"),
                                        GenURL("d.com", "/title1.html"), GURL(),
                                        false));

  // Navigate to d.com directly. The main resource should be cached due to the
  // earlier redirected navigation. Note that the HTTP cache doesn't deem the
  // earlier navigation cross-site (for the purposes of calculating the cache
  // key) since the navigation is browser-initiated.
  EXPECT_TRUE(
      NavigationResourceCached(GenURL("d.com", "/title1.html"), GURL(), false));

  bool expect_first_subframe_navigation_cached;
  switch (GetParam()) {
    case SplitCacheTestCase::kEnabledTripleKeyed:
    case SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool:
    case SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator:
      // The 'is-subframe-document-resource' boolean prevents this resource from
      // sharing a cache partition with the earlier top-level navigation, even
      // in cases where the initiator is same-origin with the page being loaded.
      expect_first_subframe_navigation_cached = false;
      break;
    case SplitCacheTestCase::kEnabledTriplePlusNavInitiator:
      // If the initiator is same-site with the resource being loaded then the
      // subframe navigation can share a cache partition with the top-level
      // navigation.
      expect_first_subframe_navigation_cached = true;
      break;
  }
  // Navigate to a subframe with the same top frame origin as in an earlier
  // navigation and same url as already navigated to earlier in a main frame
  // navigation.
  EXPECT_FALSE(NavigationResourceCached(
      GenURL("a.com", "/navigation_controller/page_with_iframe.html"),
      GenURL("a.com", "/title1.html"),
      expect_first_subframe_navigation_cached));

  // page_with_iframe.html is not added to the cache due to the request not
  // having a Max-Age header like the other requests and due to the embedded
  // test server not correctly handling requests with the If-None-Match and ETag
  // headers, but the a.com/title1.html subframe should have been. For more info
  // see: https://crbug.com/360903556
  EXPECT_FALSE(NavigationResourceCached(
      GenURL("a.com", "/navigation_controller/page_with_iframe.html"),
      GenURL("a.com", "/title1.html"), true));

  // Navigate to the same subframe document from a different top frame origin.
  // It should be a cache miss.
  EXPECT_FALSE(NavigationResourceCached(
      GenURL("b.com", "/navigation_controller/page_with_iframe.html"),
      GenURL("a.com", "/title1.html"), false));
}

IN_PROC_BROWSER_TEST_P(SplitCacheContentBrowserTestEnabled,
                       SubframeNavigationResources) {
  // Navigate for the first time, and it's not cached.
  NavigationResourceCached(
      GenURL("a.com", "/navigation_controller/page_with_iframe.html"),
      GenURL("a.com", "/title1.html"), false);

  // The second time it should be a cache hit.
  NavigationResourceCached(
      GenURL("a.com", "/navigation_controller/page_with_iframe.html"),
      GenURL("a.com", "/title1.html"), true);

  // Navigate to the same subframe document from a different top frame origin.
  // It should be a cache miss.
  NavigationResourceCached(
      GenURL("b.com", "/navigation_controller/page_with_iframe.html"),
      GenURL("a.com", "/title1.html"), false);

  // Navigate the subframe to a.com/redirect_to_d which redirects to
  // d.com/title1.html.
  NavigationResourceCached(
      GenURL("a.com", "/navigation_controller/page_with_iframe.html"),
      GenURL("a.com", "/redirect_to_d"), false);

  // Navigate to d.com directly. The resource should be cached due to the
  // earlier redirected navigation.
  NavigationResourceCached(
      GenURL("a.com", "/navigation_controller/page_with_iframe.html"),
      GenURL("d.com", "/title1.html"), true);
}

// Tests that when a subresource URL which is same-site to the fetching frame
// is later used to create a subframe from the same top-level site, it should
// not be a cache hit (crbug.com/1135149).
IN_PROC_BROWSER_TEST_P(SplitCacheContentBrowserTestEnabled,
                       SubframeNavigationResource) {
  // main.com iframes 3p.com which fetches a subresource 3p.com/script with
  // cache key (main.com, 3p.com, 3p.com/script). Then main.com iframes evil.com
  // which iframes 3p.com/script. It should be a cache miss since the first time
  // it was fetched as a subresource and second time as a subframe document
  // resource.

  // Fetch 3p.com/script from a 3p.com iframe, top-level site main.com.
  TestResourceLoad(GenURL("main.com", "/title1.html") /* top-level frame */,
                   GenURL("3p.com", "/title1.html") /* subframe */);

  // Create evil.com iframe inside top-level site main.com.
  NavigationResourceCached(
      GenURL("main.com", "/navigation_controller/page_with_iframe.html"),
      GenURL("evil.com", "/title1.html"), false);
  RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  EXPECT_EQ(1U, main_frame->frame_tree_node()->child_count());

  // Observe network requests.
  ResourceLoadObserver observer(shell());

  // Now iframe 3p.com/script within evil.com.
  GURL subframe_url = GenURL("3p.com", "/script");
  EXPECT_TRUE(ExecJs(main_frame->frame_tree_node()->child_at(0),
                     GetSubframeScript(subframe_url)));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  observer.WaitForResourceCompletion(subframe_url);
  EXPECT_EQ(false, (*observer.GetResource(subframe_url))->was_cached);
}

// Tests that a cross-site navigation to a document that was previously loaded
// via top-level navigation doesn't use the cache, since doing so could enable
// cross-site leaks.
IN_PROC_BROWSER_TEST_P(SplitCacheContentBrowserTestEnabled,
                       CrossSiteNavigation) {
  // Do a top-level navigation to a document to add it to the cache.
  EXPECT_FALSE(
      NavigationResourceCached(GenURL("a.com", "/title1.html"), GURL(), false));

  // Verify that the document did get added to the cache.
  EXPECT_TRUE(
      NavigationResourceCached(GenURL("a.com", "/title1.html"), GURL(), false));

  // Navigate to a cross-site document that performs a client-side redirect to
  // the document visited previously. Ensure that repeating this request results
  // in a cache hit, and then try again with a different initiating site.
  bool evil_com_initiator_first_navigation_result = NavigationRedirectCached(
      GenURL("evil.com", "/title1.html"), GenURL("a.com", "/title1.html"));

  EXPECT_TRUE(NavigationRedirectCached(GenURL("evil.com", "/title1.html"),
                                       GenURL("a.com", "/title1.html")));

  bool evil2_com_initiator_navigation_result = NavigationRedirectCached(
      GenURL("evil2.com", "/title1.html"), GenURL("a.com", "/title1.html"));

  switch (GetParam()) {
    case SplitCacheTestCase::kEnabledTripleKeyed:
      // If we aren't partitioning cross-site navigations then these should come
      // from the cache.
      EXPECT_TRUE(evil_com_initiator_first_navigation_result);
      EXPECT_TRUE(evil2_com_initiator_navigation_result);
      break;
    case SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool:
      // Using the cross-site navigation boolean, the first of these should not
      // be in the cache because it's cross-site, but a subsequent cross-site
      // navigation from a different initiator should result in a cache hit.
      EXPECT_FALSE(evil_com_initiator_first_navigation_result);
      EXPECT_TRUE(evil2_com_initiator_navigation_result);
      break;
    case SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator:
    case SplitCacheTestCase::kEnabledTriplePlusNavInitiator:
      // If we are keying on initiator then these two cross-site navigations
      // should be in separate cache partitions.
      EXPECT_FALSE(evil_com_initiator_first_navigation_result);
      EXPECT_FALSE(evil2_com_initiator_navigation_result);
      break;
  }
}

// Tests that a cross-origin but same-site navigation to a document that was
// previously loaded via top-level navigation uses the cache.
IN_PROC_BROWSER_TEST_P(SplitCacheContentBrowserTestEnabled,
                       CrossOriginSameSiteNavigation) {
  // Do a top-level navigation to a document to add it to the cache.
  EXPECT_FALSE(
      NavigationResourceCached(GenURL("a.com", "/title1.html"), GURL(), false));

  // Verify that the document did get added to the cache.
  EXPECT_TRUE(
      NavigationResourceCached(GenURL("a.com", "/title1.html"), GURL(), false));

  // Navigate to a cross-site document that performs a client-side redirect to
  // the document visited previously. Ensure that repeating this request results
  // in a cache hit, and then try again with a different initiating site that is
  // cross-origin but same-site.
  bool a_example_com_initiator_first_navigation_result =
      NavigationRedirectCached(GenURL("a.example.com", "/title1.html"),
                               GenURL("a.com", "/title1.html"));

  EXPECT_TRUE(NavigationRedirectCached(GenURL("a.example.com", "/title1.html"),
                                       GenURL("a.com", "/title1.html")));

  bool b_example_com_initiator_navigation_result = NavigationRedirectCached(
      GenURL("b.example.com", "/title1.html"), GenURL("a.com", "/title1.html"));

  switch (GetParam()) {
    case SplitCacheTestCase::kEnabledTripleKeyed:
      // If we aren't partitioning cross-site navigations then these should come
      // from the cache.
      EXPECT_TRUE(a_example_com_initiator_first_navigation_result);
      EXPECT_TRUE(b_example_com_initiator_navigation_result);
      break;
    case SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool:
      // Using the cross-site navigation boolean, the first of these should not
      // be in the cache because it's cross-site, but a subsequent cross-site
      // navigation from a different initiator should result in a cache hit.
      EXPECT_FALSE(a_example_com_initiator_first_navigation_result);
      EXPECT_TRUE(b_example_com_initiator_navigation_result);
      break;
    case SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator:
    case SplitCacheTestCase::kEnabledTriplePlusNavInitiator:
      // If we are keying on initiator then these two cross-origin but same-site
      // navigations should share a cache partition since the cache key
      // incorporates the site instead of the origin.
      EXPECT_FALSE(a_example_com_initiator_first_navigation_result);
      EXPECT_TRUE(b_example_com_initiator_navigation_result);
      break;
  }
}

// This class invokes ComputeHttpCacheSize on the Network Context and
// waits for the callback to be invoked.
class SplitCacheComputeHttpCacheSize {
 public:
  SplitCacheComputeHttpCacheSize() = default;
  ~SplitCacheComputeHttpCacheSize() = default;

  int64_t ComputeHttpCacheSize(BrowserContext* context,
                               base::Time start_time,
                               base::Time end_time) {
    last_computed_cache_size_ = -1;
    auto* network_context =
        context->GetDefaultStoragePartition()->GetNetworkContext();
    network::mojom::NetworkContext::ComputeHttpCacheSizeCallback size_callback =
        base::BindOnce(
            &SplitCacheComputeHttpCacheSize::ComputeCacheSizeCallback,
            base::Unretained(this));

    network_context->ComputeHttpCacheSize(start_time, end_time,
                                          std::move(size_callback));
    runloop_ = std::make_unique<base::RunLoop>();
    runloop_->Run();
    return last_computed_cache_size_;
  }

  void ComputeCacheSizeCallback(bool is_upper_bound, int64_t size) {
    last_computed_cache_size_ = size;
    runloop_->Quit();
  }

  SplitCacheComputeHttpCacheSize(const SplitCacheComputeHttpCacheSize&) =
      delete;
  SplitCacheComputeHttpCacheSize& operator=(
      const SplitCacheComputeHttpCacheSize&) = delete;

 private:
  std::unique_ptr<base::RunLoop> runloop_;
  int64_t last_computed_cache_size_ = -1;
};

// Tests that NotifyExternalCacheHit() has the correct value of
// is_subframe_document_resource by checking that the size of the http cache
// resources accessed after the resource is loaded from the blink cache is the
// same as before that.
// TODO(crbug.com/40164302): Test is flaky on Win.
#if BUILDFLAG(IS_WIN)
#define MAYBE_NotifyExternalCacheHitCheckSubframeBit \
  DISABLED_NotifyExternalCacheHitCheckSubframeBit
#else
#define MAYBE_NotifyExternalCacheHitCheckSubframeBit \
  NotifyExternalCacheHitCheckSubframeBit
#endif
IN_PROC_BROWSER_TEST_P(SplitCacheContentBrowserTestEnabled,
                       MAYBE_NotifyExternalCacheHitCheckSubframeBit) {
  ResourceLoadObserver observer(shell());
  BrowserContext* context = shell()->web_contents()->GetBrowserContext();
  std::unique_ptr<SplitCacheComputeHttpCacheSize> http_cache_size =
      std::make_unique<SplitCacheComputeHttpCacheSize>();

  // Since no resources are loaded yet, Http cache's size will be 0.
  EXPECT_EQ(0, http_cache_size->ComputeHttpCacheSize(context, base::Time(),
                                                     base::Time::Max()));

  // First fetch will populate the cache.
  GURL page_url(
      embedded_test_server()->GetURL("/page_with_cached_subresource.html"));
  EXPECT_TRUE(NavigateToURL(shell(), page_url));

  // Checking the size of resources should be >0 as resources have been loaded.
  int64_t size1 = http_cache_size->ComputeHttpCacheSize(context, base::Time(),
                                                        base::Time::Max());
  EXPECT_GT(size1, 0);
  ASSERT_EQ(2U, observer.resource_load_entries().size());
  EXPECT_TRUE(observer.memory_cached_loaded_urls().empty());
  observer.Reset();

  // Make sure time has moved forward from when the last entry was cached.
  base::Time start = base::Time::Now();
  while (start == base::Time::Now()) {
  }
  base::Time after_first = base::Time::Now();

  // Loading again should serve the request out of the in-memory cache.
  EXPECT_TRUE(NavigateToURL(shell(), page_url));

  ASSERT_EQ(1U, observer.resource_load_entries().size());
  ASSERT_EQ(1U, observer.memory_cached_loaded_urls().size());

  // Loading from the in-memory cache also changes the last accessed time of
  // those resources in the http cache. So if we check the size of resources
  // accessed after the first load till now, it will be the same as before the
  // 2nd navigation.
  int64_t size2 = http_cache_size->ComputeHttpCacheSize(context, after_first,
                                                        base::Time::Max());
  EXPECT_EQ(size1, size2);
}

IN_PROC_BROWSER_TEST_P(SplitCacheContentBrowserTestPlzDedicatedWorker,
                       DedicatedWorkers) {
  // Load 3p.com/script from a.com's worker. The first time it's loaded from the
  // network and the second it's cached.
  EXPECT_FALSE(TestResourceLoadFromDedicatedWorker(
      GenURL("a.com", "/title1.html"), GenURL("a.com", "/worker.js")));
  EXPECT_TRUE(TestResourceLoadFromDedicatedWorker(
      GenURL("a.com", "/title1.html"), GenURL("a.com", "/worker.js")));

  // Load 3p.com/script from a worker with a new top-frame origin. Due to split
  // caching it's a cache miss.
  EXPECT_FALSE(TestResourceLoadFromDedicatedWorker(
      GenURL("b.com", "/title1.html"), GenURL("b.com", "/worker.js")));
  EXPECT_TRUE(TestResourceLoadFromDedicatedWorker(
      GenURL("b.com", "/title1.html"), GenURL("b.com", "/worker.js")));

  // Load 3p.com/script from a nested worker with a new top-frame origin. Due to
  // split caching it's a cache miss.
  EXPECT_FALSE(TestResourceLoadFromDedicatedWorker(
      GenURL("c.com", "/title1.html"),
      GenURL("c.com", "/embedding_worker.js?c")));
  EXPECT_TRUE(TestResourceLoadFromDedicatedWorker(
      GenURL("c.com", "/title1.html"),
      GenURL("c.com", "/embedding_worker.js?c")));

  // Load 3p.com/script from a worker with a new top-frame origin and nested in
  // a cross-origin iframe. Due to split caching it's a cache miss.
  EXPECT_FALSE(TestResourceLoadFromDedicatedWorkerInIframe(
      GenURL("d.com", "/title1.html"), GenURL("e.com", "/title1.html"),
      GenURL("e.com", "/worker.js")));
  EXPECT_TRUE(TestResourceLoadFromDedicatedWorkerInIframe(
      GenURL("d.com", "/title1.html"), GenURL("e.com", "/title1.html"),
      GenURL("e.com", "/worker.js")));

  // Load 3p.com/script from a worker with a new top-frame origin and nested in
  // a cross-origin iframe whose URL has previously been loaded.
  EXPECT_FALSE(TestResourceLoadFromDedicatedWorkerInIframe(
      GenURL("f.com", "/title1.html"), GenURL("e.com", "/title1.html"),
      GenURL("e.com", "/worker.js")));
  EXPECT_TRUE(TestResourceLoadFromDedicatedWorkerInIframe(
      GenURL("f.com", "/title1.html"), GenURL("e.com", "/title1.html"),
      GenURL("e.com", "/worker.js")));
}

// https://crbug.com/1218723 started flaking after Field Trial Testing Config
// was enabled for content_browsertests.
IN_PROC_BROWSER_TEST_P(SplitCacheContentBrowserTestPlzDedicatedWorker,
                       DISABLED_DedicatedWorkersScripts) {
  // Load a.com's worker. The first time the worker script is loaded from the
  // network and the second it's cached.
  EXPECT_FALSE(DedicatedWorkerScriptCached(
      GenURL("a.com", "/title1.html"), GURL(), GenURL("a.com", "/worker.js")));
  EXPECT_TRUE(DedicatedWorkerScriptCached(
      GenURL("a.com", "/title1.html"), GURL(), GenURL("a.com", "/worker.js")));

  // Load a nested worker with a new top-frame origin. It's a cache miss for
  // the embedding worker the first time, as it hasn't been loaded yet, and
  // then the second time it's cached.
  EXPECT_FALSE(
      DedicatedWorkerScriptCached(GenURL("c.com", "/title1.html"), GURL(),
                                  GenURL("c.com", "/embedding_worker.js?c")));
  EXPECT_TRUE(
      DedicatedWorkerScriptCached(GenURL("c.com", "/title1.html"), GURL(),
                                  GenURL("c.com", "/embedding_worker.js?c")));

  // Load a worker with a new top-frame origin and nested in a cross-origin
  // iframe. It's a cache miss for the worker script the first time, then
  // the second time it's cached.
  EXPECT_FALSE(DedicatedWorkerScriptCached(GenURL("d.com", "/title1.html"),
                                           GenURL("e.com", "/title1.html"),
                                           GenURL("e.com", "/worker.js")));
  EXPECT_TRUE(DedicatedWorkerScriptCached(GenURL("d.com", "/title1.html"),
                                          GenURL("e.com", "/title1.html"),
                                          GenURL("e.com", "/worker.js")));

  // Load a worker with a new top-frame origin and nested in a cross-origin
  // iframe whose URL has previously been loaded. Due to split caching it's a
  // cache miss for the worker script the first time.
  EXPECT_FALSE(DedicatedWorkerScriptCached(GenURL("f.com", "/title1.html"),
                                           GenURL("e.com", "/title1.html"),
                                           GenURL("e.com", "/worker.js")));
  EXPECT_TRUE(DedicatedWorkerScriptCached(GenURL("f.com", "/title1.html"),
                                          GenURL("e.com", "/title1.html"),
                                          GenURL("e.com", "/worker.js")));
}

IN_PROC_BROWSER_TEST_F(SplitCacheContentBrowserTestDisabled, DedicatedWorkers) {
  // Load 3p.com/script from a.com's worker. The first time it's loaded from the
  // network and the second it's cached.
  EXPECT_FALSE(TestResourceLoadFromDedicatedWorker(
      GenURL("a.com", "/title1.html"), GenURL("a.com", "/worker.js")));
  EXPECT_TRUE(TestResourceLoadFromDedicatedWorker(
      GenURL("a.com", "/title1.html"), GenURL("a.com", "/worker.js")));

  // Load 3p.com/script from b.com's worker. The cache isn't split by top-frame
  // origin so the resource is already cached.
  EXPECT_TRUE(TestResourceLoadFromDedicatedWorker(
      GenURL("b.com", "/title1.html"), GenURL("b.com", "/worker.js")));

  // Load 3p.com/script from a nested worker with a new top-frame origin. The
  // cache isn't split by top-frame origin so the resource is already cached.
  EXPECT_TRUE(TestResourceLoadFromDedicatedWorker(
      GenURL("c.com", "/title1.html"),
      GenURL("c.com", "/embedding_worker.js?c")));

  // Load 3p.com/script from a worker with a new top-frame origin and nested in
  // a cross-origin iframe. The cache isn't split by top-frame origin so the
  // resource is already cached.
  EXPECT_TRUE(TestResourceLoadFromDedicatedWorkerInIframe(
      GenURL("d.com", "/title1.html"), GenURL("e.com", "/title1.html"),
      GenURL("e.com", "/worker.js")));
  EXPECT_TRUE(TestResourceLoadFromDedicatedWorkerInIframe(
      GenURL("f.com", "/title1.html"), GenURL("e.com", "/title1.html"),
      GenURL("e.com", "/worker.js")));
}

INSTANTIATE_TEST_SUITE_P(All,
                         SplitCacheContentBrowserTestPlzDedicatedWorker,
                         ::testing::Values(true, false));

class SplitCacheByIncludeCredentialsTest : public ContentBrowserTest {
 public:
  SplitCacheByIncludeCredentialsTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitAndEnableFeature(
        net::features::kSplitCacheByIncludeCredentials);
  }

  int cacheable_request_count() const { return cacheable_request_count_; }
  net::EmbeddedTestServer* https_server() { return &https_server_; }
  GURL CacheableUrl() { return https_server()->GetURL("b.test", "/cacheable"); }

  void RequestAnonymous(Shell* shell) {
    EXPECT_TRUE(ExecJs(shell, JsReplace(R"(
      new Promise(resolve => {
        const image = new Image();
        image.src = $1;
        image.crossOrigin = "anonymous";
        image.onload = resolve;
        document.body.appendChild(image);
      });
    )",
                                        CacheableUrl())));
  }

  void RequestUseCredentials(Shell* shell) {
    EXPECT_TRUE(ExecJs(shell, JsReplace(R"(
      new Promise(resolve => {
        const image = new Image();
        image.src = $1;
        image.crossOrigin = "use-credentials";
        image.onload = resolve;
        document.body.appendChild(image);
      });
    )",
                                        CacheableUrl())));
  }

 private:
  void SetUpOnMainThread() final {
    ContentBrowserTest::SetUpOnMainThread();

    cacheable_request_count_ = 0;
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->RegisterRequestHandler(
        base::BindRepeating(&SplitCacheByIncludeCredentialsTest::RequestHandler,
                            base::Unretained(this)));
    https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
    net::test_server::RegisterDefaultHandlers(https_server());
    SetupCrossSiteRedirector(https_server());
    CHECK(https_server()->Start());
  }

  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == "/cacheable") {
      cacheable_request_count_++;
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_content_type("image/svg+xml");
      response->set_content("<svg xmlns=\"http://www.w3.org/2000/svg\"></svg>");
      response->AddCustomHeader("Cache-Control", "max-age=3600");
      response->AddCustomHeader("Cross-Origin-Resource-Policy", "cross-origin");
      response->AddCustomHeader("Access-Control-Allow-Credentials", "true");
      response->AddCustomHeader(
          "Access-Control-Allow-Origin",
          https_server()->GetOrigin("a.test").Serialize());
      return response;
    }
    return nullptr;
  }

  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_;
  // Initialized and read from the UI thread. Written from the |https_server_|.
  std::atomic<int> cacheable_request_count_;
};

// Note: Compared to .DifferentProcess and .DifferentProcessVariant, this test
// emits requests from the same renderer process. This is useful for checking
// the behavior of blink's memory cache instead of the HTTP cache.
IN_PROC_BROWSER_TEST_F(SplitCacheByIncludeCredentialsTest, SameProcess) {
  GURL page_url(https_server()->GetURL("a.test", "/empty.html"));
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(0, cacheable_request_count());

  RequestAnonymous(shell());
  EXPECT_EQ(1, cacheable_request_count());
  RequestAnonymous(shell());
  EXPECT_EQ(1, cacheable_request_count());

  RequestUseCredentials(shell());
  EXPECT_EQ(2, cacheable_request_count());
  RequestUseCredentials(shell());
  EXPECT_EQ(2, cacheable_request_count());

  RequestAnonymous(shell());
  EXPECT_EQ(2, cacheable_request_count());
}

IN_PROC_BROWSER_TEST_F(SplitCacheByIncludeCredentialsTest, SameProcessVariant) {
  GURL page_url(https_server()->GetURL("a.test", "/empty.html"));
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(0, cacheable_request_count());

  RequestUseCredentials(shell());
  EXPECT_EQ(1, cacheable_request_count());
  RequestUseCredentials(shell());
  EXPECT_EQ(1, cacheable_request_count());

  RequestAnonymous(shell());
  EXPECT_EQ(2, cacheable_request_count());
  RequestAnonymous(shell());
  EXPECT_EQ(2, cacheable_request_count());

  RequestUseCredentials(shell());
  EXPECT_EQ(2, cacheable_request_count());
}

// Note: Compared to .SameProcess and .SameProcessVariant, this test emits
// requests from two different renderer process. This is useful for checking the
// behavior of the HTTP cache instead of blink's memory cache.
//
// COOP+COEP are used to get two same-origin documents loaded from different
// renderer process. This avoids interferences from
// SplitCacheByNetworkIsolationKey.
IN_PROC_BROWSER_TEST_F(SplitCacheByIncludeCredentialsTest, DifferentProcess) {
  GURL page_1_url(https_server()->GetURL("a.test", "/empty.html"));
  EXPECT_TRUE(NavigateToURL(shell(), page_1_url));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  GURL page_2_url =
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp");
  ShellAddedObserver shell_observer;
  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.open($1)", page_2_url)));
  Shell* new_shell = shell_observer.GetShell();
  EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));

  EXPECT_NE(static_cast<WebContentsImpl*>(shell()->web_contents())
                ->GetPrimaryMainFrame()
                ->GetProcess(),
            static_cast<WebContentsImpl*>(new_shell->web_contents())
                ->GetPrimaryMainFrame()
                ->GetProcess());

  EXPECT_EQ(0, cacheable_request_count());

  RequestAnonymous(shell());
  EXPECT_EQ(1, cacheable_request_count());
  RequestAnonymous(new_shell);
  EXPECT_EQ(1, cacheable_request_count());

  RequestUseCredentials(shell());
  EXPECT_EQ(2, cacheable_request_count());
  RequestUseCredentials(new_shell);
  EXPECT_EQ(2, cacheable_request_count());

  RequestAnonymous(shell());
  EXPECT_EQ(2, cacheable_request_count());
}

IN_PROC_BROWSER_TEST_F(SplitCacheByIncludeCredentialsTest,
                       DifferentProcessVariant) {
  GURL page_1_url(https_server()->GetURL("a.test", "/empty.html"));
  EXPECT_TRUE(NavigateToURL(shell(), page_1_url));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  GURL page_2_url =
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp");
  ShellAddedObserver shell_observer;
  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.open($1)", page_2_url)));
  Shell* new_shell = shell_observer.GetShell();
  EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));

  EXPECT_NE(static_cast<WebContentsImpl*>(shell()->web_contents())
                ->GetPrimaryMainFrame()
                ->GetProcess(),
            static_cast<WebContentsImpl*>(new_shell->web_contents())
                ->GetPrimaryMainFrame()
                ->GetProcess());

  EXPECT_EQ(0, cacheable_request_count());

  RequestUseCredentials(shell());
  EXPECT_EQ(1, cacheable_request_count());
  RequestUseCredentials(new_shell);
  EXPECT_EQ(1, cacheable_request_count());

  RequestAnonymous(shell());
  EXPECT_EQ(2, cacheable_request_count());
  RequestAnonymous(new_shell);
  EXPECT_EQ(2, cacheable_request_count());

  RequestUseCredentials(shell());
  EXPECT_EQ(2, cacheable_request_count());
}

}  // namespace
}  // namespace content
