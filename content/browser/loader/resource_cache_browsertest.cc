// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/loader/resource_cache_manager.h"
#include "content/browser/process_lock.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/resource_cache.mojom.h"

namespace content {

namespace {

constexpr char kHistogramIsInCacheScript[] =
    "Blink.MemoryCache.Remote.IsInCache.script";
constexpr char kHistogramIPCSendDelay[] =
    "Blink.MemoryCache.Remote.Visible.Running.IPCSendDelay";
constexpr char kHistogramIPCRecvDelay[] =
    "Blink.MemoryCache.Remote.Visible.Running.IPCRecvDelay";

}  // namespace

class ResourceCacheTest : public ContentBrowserTest {
 public:
  ResourceCacheTest() {
    feature_list_.InitAndEnableFeature(blink::features::kRemoteResourceCache);
  }

  ~ResourceCacheTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  ResourceCacheManager& resource_cache_manager() {
    return *static_cast<StoragePartitionImpl*>(
                shell()
                    ->web_contents()
                    ->GetBrowserContext()
                    ->GetDefaultStoragePartition())
                ->GetResourceCacheManager();
  }

  bool IsRenderFrameHostingRemoteCache(RenderFrameHostImpl* render_frame_host) {
    return resource_cache_manager().IsRenderFrameHostHostingRemoteCache(
        *render_frame_host);
  }

  bool FetchScript(RenderFrameHostImpl* frame, GURL url) {
    EvalJsResult result = EvalJs(frame, JsReplace(R"(
      new Promise(resolve => {
        const script = document.createElement("script");
        script.src = $1;
        script.onerror = () => resolve("error");
        script.onload = () => resolve("fetched");
        document.body.appendChild(script);
      });
    )",
                                                  url));
    return result.ExtractString() == "fetched";
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that histograms are recorded when there are two renderers that have
// the same process isolation policy.
IN_PROC_BROWSER_TEST_F(ResourceCacheTest, RecordHistograms) {
  const GURL kUrl = embedded_test_server()->GetURL("/simple_page.html");
  const GURL kScriptUrl = embedded_test_server()->GetURL("/cacheable.js");

  base::HistogramTester histograms;

  // Navigate to a page and fetch a script.
  ASSERT_TRUE(NavigateToURL(shell(), kUrl));
  RenderFrameHostImpl* frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(frame);
  ASSERT_TRUE(FetchScript(frame, kScriptUrl));

  // Create another renderer, navigate to the same page.
  Shell* second_shell = CreateBrowser();
  ASSERT_TRUE(NavigateToURL(second_shell, kUrl));
  RenderFrameHostImpl* second_frame = static_cast<RenderFrameHostImpl*>(
      second_shell->web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(second_frame);

  // Fetch the same script in the new renderer.
  ASSERT_TRUE(FetchScript(second_frame, kScriptUrl));

  ASSERT_TRUE(IsRenderFrameHostingRemoteCache(frame));
  ASSERT_FALSE(IsRenderFrameHostingRemoteCache(second_frame));

  FetchHistogramsFromChildProcesses();

  histograms.ExpectUniqueSample(kHistogramIsInCacheScript, true, 1);
  histograms.ExpectTotalCount(kHistogramIPCSendDelay, 1);
  histograms.ExpectTotalCount(kHistogramIPCRecvDelay, 1);
}

// Tests that resource cache hosting renderer migration happens when a hosting
// renderer has gone.
IN_PROC_BROWSER_TEST_F(ResourceCacheTest, HostingRendererDisconnected) {
  const GURL kUrl = embedded_test_server()->GetURL("/simple_page.html");
  const GURL kScriptUrl = embedded_test_server()->GetURL("/cacheable.js");

  base::HistogramTester histograms;

  // We currently have 1 tab that hasn't been navigated anywhere. Open another
  // tab and navigate that tab to a test page so that when the first tab also
  // navigates to that page, it will trigger ResourceCache creation in the
  // second tab.
  Shell* second_shell = CreateBrowser();
  ASSERT_TRUE(NavigateToURL(second_shell, kUrl));
  RenderFrameHostImpl* second_frame = static_cast<RenderFrameHostImpl*>(
      second_shell->web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(second_frame);
  ASSERT_TRUE(FetchScript(second_frame, kScriptUrl));

  // Navigate to the test page in the first tab. This triggers ResourceCache
  // creation in the second tab.
  ASSERT_TRUE(NavigateToURL(shell(), kUrl));
  RenderFrameHostImpl* frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(frame);

  ASSERT_FALSE(IsRenderFrameHostingRemoteCache(frame));
  ASSERT_TRUE(IsRenderFrameHostingRemoteCache(second_frame));

  // Crash the second shell to migrate the ResourceCache from the second tab to
  // the first tab.
  RenderFrameDeletedObserver observer(second_frame);
  ScopedAllowRendererCrashes allow_renderer_crashes(second_frame->GetProcess());
  second_frame->GetProcess()->ForceCrash();
  ASSERT_TRUE(observer.WaitUntilDeleted());

  // Create the third tab, navigate to the test page and fetch script.
  Shell* third_shell = CreateBrowser();
  ASSERT_TRUE(NavigateToURL(third_shell, kUrl));
  RenderFrameHostImpl* third_frame = static_cast<RenderFrameHostImpl*>(
      third_shell->web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(third_frame);
  ASSERT_TRUE(FetchScript(third_frame, kScriptUrl));

  ASSERT_TRUE(IsRenderFrameHostingRemoteCache(frame));
  ASSERT_FALSE(IsRenderFrameHostingRemoteCache(third_frame));

  FetchHistogramsFromChildProcesses();

  // Histograms should be recorded with a cache miss because the first renderer
  // didn't fetch the script.
  histograms.ExpectUniqueSample(kHistogramIsInCacheScript, false, 1);
  histograms.ExpectTotalCount(kHistogramIPCSendDelay, 1);
  histograms.ExpectTotalCount(kHistogramIPCRecvDelay, 1);
}

// Tests that same-origin-same-process navigation doesn't change resource cache
// hosting renderer.
IN_PROC_BROWSER_TEST_F(ResourceCacheTest, HostingRendererNavigateToSameOrigin) {
  const GURL kUrl = embedded_test_server()->GetURL("/simple_page.html");
  const GURL kScriptUrl = embedded_test_server()->GetURL("/cacheable.js");

  // Disable BFCache so that RenderFrameHost swap won't happen.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::DisableForTestingReason::
                                        TEST_ASSUMES_NO_RENDER_FRAME_CHANGE);

  base::HistogramTester histograms;

  // Navigate to a test page and fetch a script.
  ASSERT_TRUE(NavigateToURL(shell(), kUrl));
  RenderFrameHostImpl* frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(frame);
  ASSERT_TRUE(FetchScript(frame, kScriptUrl));

  // Create a new tab, navigate to the test page in the tab. This triggers
  // ResourceCache creation in the first tab.
  Shell* second_shell = CreateBrowser();
  ASSERT_TRUE(NavigateToURL(second_shell, kUrl));
  RenderFrameHostImpl* second_frame = static_cast<RenderFrameHostImpl*>(
      second_shell->web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(second_frame);

  ASSERT_TRUE(IsRenderFrameHostingRemoteCache(frame));
  ASSERT_FALSE(IsRenderFrameHostingRemoteCache(second_frame));

  // Trigger same-origin-same-process navigation. This shouldn't change the
  // resource cache host.
  const GURL kUrl2 = embedded_test_server()->GetURL("/hello.html");
  ASSERT_TRUE(NavigateToURL(shell(), kUrl2));

  ASSERT_TRUE(IsRenderFrameHostingRemoteCache(frame));
  ASSERT_FALSE(IsRenderFrameHostingRemoteCache(second_frame));

  ASSERT_TRUE(FetchScript(second_frame, kScriptUrl));

  FetchHistogramsFromChildProcesses();

  // Histograms should be recorded with a cache hit because the first tab
  // fetched the script in the first navigation.
  histograms.ExpectUniqueSample(kHistogramIsInCacheScript, true, 1);
  histograms.ExpectTotalCount(kHistogramIPCSendDelay, 1);
  histograms.ExpectTotalCount(kHistogramIPCRecvDelay, 1);
}

class ResourceCacheBFCacheTest : public ResourceCacheTest,
                                 public testing::WithParamInterface<bool> {
 public:
  ResourceCacheBFCacheTest() {
    if (IsBackForwardCacheEnabled()) {
      feature_list_.InitWithFeaturesAndParameters(
          GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
          GetDefaultDisabledBackForwardCacheFeaturesForTesting());
    } else {
      feature_list_.InitAndDisableFeature(features::kBackForwardCache);
    }
  }

  bool IsBackForwardCacheEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, ResourceCacheBFCacheTest, testing::Bool());

// Tests that an inactive renderer stops hosting a ResourceCache. The second
// active renderer becomes a new host. Once the inactive renderer becomes
// active, it should use the ResourceCache that is hosted by the second
// renderer.
IN_PROC_BROWSER_TEST_P(ResourceCacheBFCacheTest,
                       HostingRendererNavigateToAnotherOriginAndBack) {
  // Labels for renderers:
  // * R1: RenderFrameHost lives in the first tab, navigated to `kUrl`.
  // * R2: RenderFrameHost lives in the second tab, navigated to `kUrl`.
  // * R3: RenderFrameHost lives in the first tab, navigated to
  //      `kDifferentUrl`
  // * R4: RenderFrameHost lives in the first tab, navigated back to `kUrl`.

  const GURL kUrl = embedded_test_server()->GetURL("/simple_page.html");
  const GURL kDifferentOriginUrl = embedded_test_server()->GetURL(
      "different-origin.example.com", "/simple_page.html");
  // Fetched in R1 and R2.
  const GURL kScriptUrl = embedded_test_server()->GetURL("/cacheable.js");
  // Fetched in R2 and R4.
  const GURL kScriptUrl2 = embedded_test_server()->GetURL("/cacheable2.js");

  base::HistogramTester histograms;

  // Navigate to an origin in two tabs so that the first tab hosts a
  // ResourceCache.
  ASSERT_TRUE(NavigateToURL(shell(), kUrl));
  RenderFrameHostImpl* render_frame_host1 = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(render_frame_host1);

  Shell* second_shell = CreateBrowser();

  ASSERT_TRUE(NavigateToURL(second_shell, kUrl));
  RenderFrameHostImpl* render_frame_host2 = static_cast<RenderFrameHostImpl*>(
      second_shell->web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(render_frame_host2);

  ASSERT_TRUE(IsRenderFrameHostingRemoteCache(render_frame_host1));
  ASSERT_FALSE(IsRenderFrameHostingRemoteCache(render_frame_host2));

  ASSERT_TRUE(FetchScript(render_frame_host1, kScriptUrl));
  ASSERT_TRUE(FetchScript(render_frame_host2, kScriptUrl));

  // Histograms should be recorded with a cache hit because R1 and R2 fetched
  // `kScriptUrl`.
  FetchHistogramsFromChildProcesses();
  histograms.ExpectUniqueSample(kHistogramIsInCacheScript, true, 1);
  histograms.ExpectTotalCount(kHistogramIPCSendDelay, 1);
  histograms.ExpectTotalCount(kHistogramIPCRecvDelay, 1);

  // Navigate to a different origin in the first tab. This triggers
  // ResourceCache migration.
  RenderFrameHostImplWrapper render_frame_host1_wrapper(render_frame_host1);
  ASSERT_TRUE(NavigateToURL(shell(), kDifferentOriginUrl));
  RenderFrameHostImpl* render_frame_host3 = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(render_frame_host3);

  if (IsBackForwardCacheEnabled()) {
    ASSERT_FALSE(IsRenderFrameHostingRemoteCache(render_frame_host1));
  } else {
    // R1 may or may not be destroyed at this point. Wait for deletion if
    // needed.
    ASSERT_TRUE(render_frame_host1_wrapper.WaitUntilRenderFrameDeleted());
  }
  ASSERT_TRUE(IsRenderFrameHostingRemoteCache(render_frame_host2));
  ASSERT_FALSE(IsRenderFrameHostingRemoteCache(render_frame_host3));

  // This fetch should not count up kHistogramIsInCacheScript.
  ASSERT_TRUE(FetchScript(render_frame_host2, kScriptUrl2));
  FetchHistogramsFromChildProcesses();
  histograms.ExpectUniqueSample(kHistogramIsInCacheScript, true, 1);

  // Navigate back to the first origin in the first tab.
  {
    TestFrameNavigationObserver observer(
        shell()->web_contents()->GetPrimaryMainFrame());
    shell()->web_contents()->GetController().GoBack();
    observer.Wait();
  }

  RenderFrameHostImpl* render_frame_host4 = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(render_frame_host4);
  if (IsBackForwardCacheEnabled()) {
    ASSERT_EQ(render_frame_host1, render_frame_host4);
  }

  ASSERT_TRUE(IsRenderFrameHostingRemoteCache(render_frame_host2));
  ASSERT_FALSE(IsRenderFrameHostingRemoteCache(render_frame_host4));

  ASSERT_TRUE(FetchScript(render_frame_host4, kScriptUrl2));

  // Histograms should be recorded twice with a cache hit because R2 and R4
  // fetched `kScriptUrl2`, in addition to `kScriptUrl` in R1 and R2 above.
  FetchHistogramsFromChildProcesses();
  histograms.ExpectUniqueSample(kHistogramIsInCacheScript, true, 2);
  histograms.ExpectTotalCount(kHistogramIPCSendDelay, 2);
  histograms.ExpectTotalCount(kHistogramIPCRecvDelay, 2);
}

// TODO(https://crbug.com/141426): Add following tests.
// * HostingRendererDisconnectedAndNoOtherRendererCanHost

class ResourceCacheDisableSiteIsolationTest : public ResourceCacheTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableSiteIsolation);
    ResourceCacheTest::SetUpCommandLine(command_line);
  }
};

// Tests that different origins don't share a resource cache when site isolation
// is disabled.
IN_PROC_BROWSER_TEST_F(ResourceCacheDisableSiteIsolationTest,
                       TwoOriginsInProcess) {
  constexpr const char* kCrossOrigin = "cross-origin.example.com";
  const GURL kUrl = embedded_test_server()->GetURL("/simple_page.html");
  const GURL kScriptUrl = embedded_test_server()->GetURL("/cacheable.js");
  const GURL kCrossOriginUrl =
      embedded_test_server()->GetURL(kCrossOrigin, "/simple_page.html");
  const GURL kCrossOriginScriptUrl =
      embedded_test_server()->GetURL(kCrossOrigin, "/cacheable.js");

  base::HistogramTester histograms;

  // Navigate to a test page and fetch a script.
  ASSERT_TRUE(NavigateToURL(shell(), kUrl));
  RenderFrameHostImpl* frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(frame);
  ASSERT_TRUE(FetchScript(frame, kScriptUrl));

  // Create another tab, navigate to a cross origin page.
  // It should not receive a resource cache remote and should not cause the
  // first tab to create a ResourceCache.
  Shell* second_shell = CreateBrowser();
  ASSERT_TRUE(NavigateToURL(second_shell, kCrossOriginUrl));
  RenderFrameHostImpl* second_frame = static_cast<RenderFrameHostImpl*>(
      second_shell->web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(second_frame);

  // Make sure both frames are hosted in a same process but have different
  // origins.
  ASSERT_EQ(frame->GetProcess()->GetProcessLock(),
            second_frame->GetProcess()->GetProcessLock());
  ASSERT_NE(frame->GetLastCommittedOrigin(),
            second_frame->GetLastCommittedOrigin());

  ASSERT_FALSE(IsRenderFrameHostingRemoteCache(frame));
  ASSERT_FALSE(IsRenderFrameHostingRemoteCache(second_frame));

  // Fetch a script in the new renderer.
  ASSERT_TRUE(FetchScript(second_frame, kCrossOriginScriptUrl));

  // Close the second frame.
  second_shell->Close();

  // Create yet another renderer, navigate to the cross origin page.
  // It should not receive a resource cache remote.
  Shell* third_shell = CreateBrowser();
  ASSERT_TRUE(NavigateToURL(third_shell, kCrossOriginUrl));
  RenderFrameHostImpl* third_frame = static_cast<RenderFrameHostImpl*>(
      third_shell->web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(third_shell);
  ASSERT_EQ(frame->GetProcess()->GetProcessLock(),
            third_frame->GetProcess()->GetProcessLock());
  ASSERT_NE(frame->GetLastCommittedOrigin(),
            third_frame->GetLastCommittedOrigin());

  // Fetch a script in the new renderer.
  ASSERT_TRUE(FetchScript(third_frame, kCrossOriginScriptUrl));

  ASSERT_FALSE(IsRenderFrameHostingRemoteCache(frame));

  FetchHistogramsFromChildProcesses();

  // No histogram should be recorded as none of renderers should get a
  // resource cache remote.
  histograms.ExpectUniqueSample(kHistogramIsInCacheScript, false, 0);
  histograms.ExpectTotalCount(kHistogramIPCSendDelay, 0);
  histograms.ExpectTotalCount(kHistogramIPCRecvDelay, 0);
}

}  // namespace content
