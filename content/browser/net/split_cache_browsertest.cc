// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/strings/pattern.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/resource_load_observer.h"
#include "net/base/features.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "url/gurl.h"

namespace content {

class SplitCacheContentBrowserTest : public ContentBrowserTest {
 public:
  enum class Context { kMainFrame, kSameOriginFrame, kCrossOriginFrame };
  SplitCacheContentBrowserTest() = default;

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

    return std::unique_ptr<net::test_server::HttpResponse>();
  }

 protected:
  // Creates and loads subframe, waits for load to stop, and then returns
  // subframe from the web contents frame tree.
  RenderFrameHost* CreateSubframe(const GURL& sub_frame) {
    EXPECT_TRUE(ExecuteScript(shell(), GetSubframeScript(sub_frame)));
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

    return static_cast<WebContentsImpl*>(shell()->web_contents())
        ->GetFrameTree()
        ->root()
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

    // Clear the in-memory cache held by the current process:
    // 1) Prevent the old page from entering the back-forward cache. Otherwise
    //    the old process will be kept alive, because it is still being used.
    // 2) Navigate to a WebUI URL, which uses a new process.
    BackForwardCache::DisableForRenderFrameHost(
        shell()->web_contents()->GetMainFrame(), "test");
    EXPECT_TRUE(NavigateToURL(shell(), GetWebUIURL(kChromeUIGpuHost)));

    // In the case of a redirect, the observed URL will be different from
    // what NavigateToURL(...) expects.
    if (base::StartsWith(url.path(), "/redirect", base::CompareCase::SENSITIVE))
      EXPECT_FALSE(NavigateToURL(shell(), url));
    else
      EXPECT_TRUE(NavigateToURL(shell(), url));

    RenderFrameHost* host_to_load_resource =
        shell()->web_contents()->GetMainFrame();
    RenderFrameHostImpl* main_frame =
        static_cast<RenderFrameHostImpl*>(host_to_load_resource);

    Shell* shell_to_observe = shell();

    if (new_frame.is_valid()) {
      // If there is supposed to be a subframe or popup, create it.
      if (use_popup) {
        shell_to_observe = OpenPopup(main_frame, new_frame, "");
        host_to_load_resource =
            static_cast<WebContentsImpl*>(shell_to_observe->web_contents())
                ->GetMainFrame();
      } else {
        host_to_load_resource = CreateSubframe(new_frame);
      }
    }

    // Observe network requests.
    ResourceLoadObserver observer(shell_to_observe);

    GURL resource = GenURL("3p.com", "/script");

    // If there is supposed to be a worker to load this resource, create it.
    // Otherwise, load the resource directly.
    if (worker.is_valid()) {
      EXPECT_TRUE(
          ExecuteScript(host_to_load_resource, GetWorkerScript(worker)));
    } else {
      EXPECT_TRUE(ExecuteScript(host_to_load_resource,
                                GetLoadResourceScript(resource)));
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
      EXPECT_EQ(net::NetworkIsolationKey(top_frame_origin, frame_origin),
                frame_host->GetNetworkIsolationKey());
    } else {
      EXPECT_TRUE(frame_host->GetNetworkIsolationKey().IsTransient());
    }

    return (*observer.FindResource(resource))->was_cached;
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
    // We assume that we don't start this call from "chrome://gpu", as
    // otherwise it won't be a cross-process navigation. We are relying
    // on this navigation to discard the old process.
    EXPECT_TRUE(NavigateToURL(shell(), GetWebUIURL("gpu")));

    // Observe network requests.
    ResourceLoadObserver observer(shell());

    EXPECT_TRUE(NavigateToURL(shell(), url, expected_commit_url));

    RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetMainFrame());

    observer.WaitForResourceCompletion(url);

    if (sub_frame.is_valid()) {
      EXPECT_EQ(1U, main_frame->frame_tree_node()->child_count());
      NavigateFrameToURL(main_frame->frame_tree_node()->child_at(0), sub_frame);
      EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
      observer.WaitForResourceCompletion(sub_frame);
      EXPECT_EQ(subframe_navigation_resource_cached,
                (*observer.FindResource(sub_frame))->was_cached);
    }

    return (*observer.FindResource(url))->was_cached;
  }

  // Loads a dedicated worker script and checks to see whether or not the
  // script was cached.
  bool DedicatedWorkerScriptCached(const GURL& url,
                                   const GURL& sub_frame,
                                   const GURL& worker) {
    DCHECK(url.is_valid());
    DCHECK(worker.is_valid());

    // Do a cross-process navigation to clear the in-memory cache.
    // We assume that we don't start this call from "chrome://gpu", as
    // otherwise it won't be a cross-process navigation. We are relying
    // on this navigation to discard the old process.
    EXPECT_TRUE(NavigateToURL(shell(), GetWebUIURL("gpu")));

    // Observe network requests.
    ResourceLoadObserver observer(shell());

    EXPECT_TRUE(NavigateToURL(shell(), url));

    RenderFrameHost* host_to_load_resource =
        shell()->web_contents()->GetMainFrame();

    // If there is supposed to be a subframe, create it.
    if (sub_frame.is_valid()) {
      host_to_load_resource = CreateSubframe(sub_frame);
    }

    EXPECT_TRUE(ExecuteScript(host_to_load_resource, GetWorkerScript(worker)));

    observer.WaitForResourceCompletion(GenURL("3p.com", "/script"));
    observer.WaitForResourceCompletion(worker);

    return (*observer.FindResource(worker))->was_cached;
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

  GURL GenURL(const std::string& host, const std::string& path) {
    return embedded_test_server()->GetURL(host, path);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SplitCacheContentBrowserTest);
};

class SplitCacheWithFrameOriginContentBrowserTest
    : public SplitCacheContentBrowserTest {
 public:
  SplitCacheWithFrameOriginContentBrowserTest() {
    feature_list.InitWithFeatures(
        {net::features::kSplitCacheByNetworkIsolationKey,
         net::features::kAppendFrameOriginToNetworkIsolationKey},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

class SplitCacheRegistrableDomainContentBrowserTest
    : public SplitCacheContentBrowserTest {
 public:
  SplitCacheRegistrableDomainContentBrowserTest() {
    feature_list.InitWithFeatures(
        // enabled_features
        {net::features::kSplitCacheByNetworkIsolationKey,
         net::features::kAppendFrameOriginToNetworkIsolationKey},
        // disabled_features
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

class SplitCacheContentBrowserTestEnabled
    : public SplitCacheContentBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  SplitCacheContentBrowserTestEnabled() {
    std::vector<base::Feature> enabled_features;
    enabled_features.push_back(net::features::kSplitCacheByNetworkIsolationKey);

    // When the test parameter is true, we test the split cache with
    // PlzDedicatedWorker enabled.
    if (GetParam())
      enabled_features.push_back(blink::features::kPlzDedicatedWorker);

    feature_list.InitWithFeatures(
        enabled_features,
        {net::features::kAppendFrameOriginToNetworkIsolationKey});
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

class SplitCacheContentBrowserTestDisabled
    : public SplitCacheContentBrowserTest {
 public:
  SplitCacheContentBrowserTestDisabled() {
    feature_list.InitAndDisableFeature(
        net::features::kSplitCacheByNetworkIsolationKey);
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

#if defined(THREAD_SANITIZER)
// Flaky under TSan: https://crbug.com/995181
#define MAYBE_SplitCache DISABLED_SplitCache
#else
#define MAYBE_SplitCache SplitCache
#endif

IN_PROC_BROWSER_TEST_P(SplitCacheContentBrowserTestEnabled, MAYBE_SplitCache) {
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

  // Load it from a a.com/redirect_to_d which redirects to d.com/title1.html and
  // the resource shouldn't be cached because now we're on d.com.
  EXPECT_FALSE(TestResourceLoad(GenURL("a.com", "/redirect_to_d"), GURL()));

  // Navigate to d.com directly. The main resource should be cached due to the
  // earlier navigation.
  EXPECT_TRUE(TestResourceLoad(GenURL("d.com", "/title1.html"), GURL()));

  // Load the resource from a same-origin iframe on a page where it's already
  // cached. It should still be cached.
  EXPECT_TRUE(TestResourceLoad(GenURL("a.com", "/title1.html"),
                               GenURL("a.com", "/title1.html")));

  // Load the resource from a cross-origin iframe on a page where it's already
  // cached. It should still be cached.
  EXPECT_TRUE(TestResourceLoad(GenURL("a.com", "/title1.html"),
                               GenURL("d.com", "/title1.html")));

  // Load the resource from a same-origin iframe on a page where it's not
  // cached. It should not be cached.
  EXPECT_FALSE(TestResourceLoad(GenURL("e.com", "/title1.html"),
                                GenURL("e.com", "/title1.html")));

  // Load the resource from a cross-origin iframe where the iframe's origin has
  // seen the object before but the top frame hasn't. It should not be cached.
  EXPECT_FALSE(TestResourceLoad(GenURL("f.com", "/title1.html"),
                                GenURL("a.com", "/title1.html")));

  // Load the resource from a data url which has an opaque origin. It shouldn't
  // be cached.
  GURL data_url("data:text/html,<body>Hello World</body>");
  EXPECT_FALSE(TestResourceLoad(data_url, GURL()));

  // Load the same resource from the same data url, it shouldn't be cached
  // because the origin should be unique.
  EXPECT_FALSE(TestResourceLoad(data_url, GURL()));

  // Load the resource from a document that points to about:blank.
  GURL blank_url("about:blank");
  EXPECT_FALSE(TestResourceLoad(blank_url, GURL()));

  // Load the same resource from about:blank url again, it shouldn't be cached
  // because the origin is unique. TODO(crbug.com/888079) will change this
  // behavior and about:blank main frame pages will inherit the origin of the
  // page that opened it.
  EXPECT_FALSE(TestResourceLoad(blank_url, GURL()));
}

IN_PROC_BROWSER_TEST_F(SplitCacheWithFrameOriginContentBrowserTest,
                       SplitCache) {
  // Load a cacheable resource for the first time, and it's not cached.
  EXPECT_FALSE(TestResourceLoad(GenURL("a.com", "/title1.html"), GURL()));

  // The second time, it's cached.
  EXPECT_TRUE(TestResourceLoad(GenURL("a.com", "/title1.html"), GURL()));

  // Load it from a a.com/redirect_to_d which redirects to d.com/title1.html and
  // the resource shouldn't be cached because now we're on d.com.
  EXPECT_FALSE(TestResourceLoad(GenURL("a.com", "/title1.html"),
                                GenURL("a.com", "/redirect_to_d")));

  // Now load it from the d.com iframe directly. It should be cached.
  EXPECT_TRUE(TestResourceLoad(GenURL("a.com", "/title1.html"),
                               GenURL("d.com", "/title1.html")));

  // Load the resource from a same-origin iframe on a page where it's already
  // cached. It should still be cached.
  EXPECT_TRUE(TestResourceLoad(GenURL("a.com", "/title1.html"),
                               GenURL("a.com", "/title1.html")));

  // Load the resource from a cross-origin iframe on a page where the
  // iframe hasn't been cached previously.  It should not be cached.
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

  // Load the resource from a data url which has an opaque origin. It shouldn't
  // be cached.
  GURL data_url("data:text/html,<body>Hello World</body>");
  EXPECT_FALSE(TestResourceLoad(GenURL("a.com", "/title1.html"), data_url));

  // Load the same resource from the same data url, it shouldn't be cached
  // because the origin should be unique.
  EXPECT_FALSE(TestResourceLoad(GenURL("a.com", "/title1.html"), data_url));

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

  // Load the resource from a popup window that points to a new origin. The
  // resource is not cached because the resource load is using a NIK set to
  // (g.com, g.com).
  EXPECT_FALSE(TestResourceLoadFromPopup(GenURL("a.com", "/title1.html"),
                                         GenURL("g.com", "/title1.html")));
}

IN_PROC_BROWSER_TEST_F(SplitCacheRegistrableDomainContentBrowserTest,
                       SplitCache) {
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

IN_PROC_BROWSER_TEST_F(SplitCacheWithFrameOriginContentBrowserTest,
                       SplitCacheDedicatedWorkers) {
  // Load 3p.com/script from a.com's worker. The first time it's loaded from the
  // network and the second it's cached.
  EXPECT_FALSE(TestResourceLoadFromDedicatedWorker(
      GenURL("a.com", "/title1.html"), GenURL("a.com", "/worker.js")));
  EXPECT_TRUE(TestResourceLoadFromDedicatedWorker(
      GenURL("a.com", "/title1.html"), GenURL("a.com", "/worker.js")));

  // Load 3p.com/script from a worker with a new top frame origin. Due to split
  // caching it's a cache miss.
  EXPECT_FALSE(TestResourceLoadFromDedicatedWorker(
      GenURL("b.com", "/title1.html"), GenURL("b.com", "/worker.js")));
  EXPECT_TRUE(TestResourceLoadFromDedicatedWorker(
      GenURL("b.com", "/title1.html"), GenURL("b.com", "/worker.js")));

  // Cross origin workers.
  EXPECT_FALSE(TestResourceLoadFromDedicatedWorkerInIframe(
      GenURL("d.com", "/title1.html"), GenURL("e.com", "/title1.html"),
      GenURL("e.com", "/worker.js")));
  EXPECT_TRUE(TestResourceLoadFromDedicatedWorkerInIframe(
      GenURL("d.com", "/title1.html"), GenURL("e.com", "/title1.html"),
      GenURL("e.com", "/worker.js")));

  // Load 3p.com/script from a nested worker with a new top-frame origin. Due to
  // split caching it's a cache miss.
  EXPECT_FALSE(TestResourceLoadFromDedicatedWorker(
      GenURL("c.com", "/title1.html"),
      GenURL("c.com", "/embedding_worker.js?c")));
  EXPECT_TRUE(TestResourceLoadFromDedicatedWorker(
      GenURL("c.com", "/title1.html"),
      GenURL("c.com", "/embedding_worker.js?c")));
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
  // earlier redirected navigation.
  EXPECT_TRUE(
      NavigationResourceCached(GenURL("d.com", "/title1.html"), GURL(), false));

  // Navigate to a subframe with the same top frame origin as in an earlier
  // navigation and same url as already navigated to earlier in a main frame
  // navigation. It should be a cache hit for the subframe resource.
  EXPECT_FALSE(NavigationResourceCached(
      GenURL("a.com", "/navigation_controller/page_with_iframe.html"),
      GenURL("a.com", "/title1.html"), true));

  // Navigate to the same subframe document from a different top frame origin.
  // It should be a cache miss.
  EXPECT_FALSE(NavigationResourceCached(
      GenURL("b.com", "/navigation_controller/page_with_iframe.html"),
      GenURL("a.com", "/title1.html"), false));
}

IN_PROC_BROWSER_TEST_F(SplitCacheWithFrameOriginContentBrowserTest,
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

IN_PROC_BROWSER_TEST_P(SplitCacheContentBrowserTestEnabled,
                       SplitCacheDedicatedWorkers) {
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

IN_PROC_BROWSER_TEST_P(SplitCacheContentBrowserTestEnabled,
                       SplitCacheDedicatedWorkersScripts) {
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

IN_PROC_BROWSER_TEST_F(SplitCacheContentBrowserTestDisabled,
                       SplitCacheDedicatedWorkers) {
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
                         SplitCacheContentBrowserTestEnabled,
                         ::testing::Values(true, false));

}  // namespace content
