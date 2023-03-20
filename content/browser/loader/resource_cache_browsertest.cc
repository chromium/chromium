// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/resource_cache.mojom.h"

namespace content {

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

  // Create a ResourceCache in the renderer.
  mojo::PendingRemote<blink::mojom::ResourceCache> pending_remote;
  frame->GetRemoteInterfaces()->GetInterface(
      pending_remote.InitWithNewPipeAndPassReceiver());

  // Create another renderer, navigate to the same page.
  Shell* second_shell = CreateBrowser();
  ASSERT_TRUE(NavigateToURL(second_shell, kUrl));
  RenderFrameHostImpl* second_frame = static_cast<RenderFrameHostImpl*>(
      second_shell->web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(second_frame);

  // Set ResourceCache remote in the new renderer.
  second_frame->SetResourceCache(std::move(pending_remote));
  second_frame->FlushMojomFrameRemoteForTesting();

  // Fetch the same script in the new renderer.
  ASSERT_TRUE(FetchScript(second_frame, kScriptUrl));

  FetchHistogramsFromChildProcesses();

  histograms.ExpectUniqueSample("Blink.MemoryCache.Remote.IsInCache.script",
                                true, 1);
  histograms.ExpectTotalCount(
      "Blink.MemoryCache.Remote.Visible.Running.IPCSendDelay", 1);
  histograms.ExpectTotalCount(
      "Blink.MemoryCache.Remote.Visible.Running.IPCRecvDelay", 1);
}

}  // namespace content
