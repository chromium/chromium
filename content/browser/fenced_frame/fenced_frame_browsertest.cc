// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/fenced_frame/fenced_frame.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame.mojom-test-utils.h"
#include "content/public/browser/frame_type.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/mock_web_contents_observer.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom.h"
#include "url/gurl.h"

namespace content {

class FencedFrameBrowserTest : public ContentBrowserTest {
 protected:
  FencedFrameBrowserTest() = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* primary_main_frame_host() {
    return web_contents()->GetMainFrame();
  }

  test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

 private:
  test::FencedFrameTestHelper fenced_frame_test_helper_;
};

// Tests that the renderer can create a <fencedframe> that results in a
// browser-side content::FencedFrame also being created.
IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, CreateFromScriptAndDestroy) {
  const GURL main_url = embedded_test_server()->GetURL(
      "fencedframe.test", "/fenced_frames/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "fencedframe.test", "/title1.html")));
  RenderFrameHostImplWrapper primary_rfh(primary_main_frame_host());
  RenderFrameHostImplWrapper fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(primary_rfh.get(),
                                                   main_url));
  FrameTreeNode* fenced_frame_root_node = fenced_frame_rfh->frame_tree_node();
  EXPECT_TRUE(fenced_frame_root_node->render_manager()
                  ->GetProxyToOuterDelegate()
                  ->is_render_frame_proxy_live());

  // Test `RenderFrameHostImpl::IsInPrimaryMainFrame`.
  EXPECT_TRUE(primary_rfh->IsInPrimaryMainFrame());
  EXPECT_FALSE(fenced_frame_rfh->IsInPrimaryMainFrame());

  // Test `FrameTreeNode::IsFencedFrameRoot()`.
  EXPECT_FALSE(
      web_contents()->GetPrimaryFrameTree().root()->IsFencedFrameRoot());
  EXPECT_FALSE(primary_rfh->child_at(0)->IsFencedFrameRoot());
  EXPECT_TRUE(fenced_frame_root_node->IsFencedFrameRoot());

  // Test `FrameTreeNode::IsInFencedFrameTree()`.
  EXPECT_FALSE(
      web_contents()->GetPrimaryFrameTree().root()->IsInFencedFrameTree());
  EXPECT_FALSE(primary_rfh->child_at(0)->IsInFencedFrameTree());
  EXPECT_TRUE(fenced_frame_root_node->IsInFencedFrameTree());

  EXPECT_TRUE(ExecJs(primary_rfh.get(),
                     "const ff = document.querySelector('fencedframe');\
                     ff.remove();"));
  ASSERT_TRUE(fenced_frame_rfh.WaitUntilRenderFrameDeleted());

  EXPECT_TRUE(primary_rfh->GetFencedFrames().empty());
  EXPECT_TRUE(fenced_frame_rfh.IsDestroyed());
}

IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, CreateFromParser) {
  const GURL top_level_url = embedded_test_server()->GetURL(
      "fencedframe.test", "/fenced_frames/basic.html");
  EXPECT_TRUE(NavigateToURL(shell(), top_level_url));

  // The fenced frame is set-up synchronously, so it should exist immediately.
  RenderFrameHostImplWrapper dummy_child_frame(
      primary_main_frame_host()->child_at(0)->current_frame_host());
  EXPECT_NE(dummy_child_frame->inner_tree_main_frame_tree_node_id(),
            FrameTreeNode::kFrameTreeNodeInvalidId);
  FrameTreeNode* inner_frame_tree_node = FrameTreeNode::GloballyFindByID(
      dummy_child_frame->inner_tree_main_frame_tree_node_id());
  EXPECT_TRUE(inner_frame_tree_node);
}

IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, Navigation) {
  const GURL main_url =
      embedded_test_server()->GetURL("fencedframe.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // WebContentsObservers should not be notified of commits happening
  // in the non-primary navigation controller.
  testing::NiceMock<MockWebContentsObserver> web_contents_observer(
      web_contents());
  EXPECT_CALL(web_contents_observer, NavigationEntryCommitted(testing::_))
      .Times(0);
  EXPECT_CALL(web_contents_observer, NavigationEntryChanged(testing::_))
      .Times(0);

  RenderFrameHostImpl* primary_rfh = primary_main_frame_host();

  const GURL fenced_frame_url = embedded_test_server()->GetURL(
      "fencedframe.test", "/fenced_frames/title1.html");
  RenderFrameHost* fenced_frame_rfh =
      fenced_frame_test_helper().CreateFencedFrame(primary_rfh,
                                                   fenced_frame_url);

  // Test that a fenced frame navigation does not impact the primary main
  // frame...
  EXPECT_EQ(main_url, primary_rfh->GetLastCommittedURL());
  // ... but should target the correct frame.
  EXPECT_EQ(fenced_frame_url, fenced_frame_rfh->GetLastCommittedURL());
  EXPECT_EQ(url::Origin::Create(fenced_frame_url),
            fenced_frame_rfh->GetLastCommittedOrigin());
}

IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, AboutBlankNavigation) {
  const GURL main_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameHostImpl* primary_rfh = primary_main_frame_host();

  const GURL fenced_frame_url = embedded_test_server()->GetURL(
      "fencedframe.test", "/fenced_frames/title1.html");
  fenced_frame_test_helper().CreateFencedFrame(primary_rfh, fenced_frame_url);

  std::vector<FencedFrame*> fenced_frames = primary_rfh->GetFencedFrames();
  ASSERT_EQ(1ul, fenced_frames.size());
  FencedFrame* fenced_frame = fenced_frames.back();

  // Exepct the origin is correct.
  EXPECT_EQ(url::Origin::Create(fenced_frame_url),
            EvalJs(fenced_frame->GetInnerRoot(), "self.origin;"));

  // Assigning the location from the parent cause the SiteInstance
  // to be calculated incorrectly and crash. see https://crbug.com/1268238.
  // We can't use `NavigateFrameInFencedFrameTree` because that navigates
  // from the inner frame tree and we want the navigation to occur from
  // the outer frame tree.
  EXPECT_TRUE(
      ExecJs(primary_rfh,
             "document.querySelector('fencedframe').src = 'about:blank';"));

  fenced_frame->WaitForDidStopLoadingForTesting();
  EXPECT_TRUE(!fenced_frame->GetInnerRoot()->IsErrorDocument());

  EXPECT_EQ("null", EvalJs(fenced_frame->GetInnerRoot(), "self.origin;"));
}

IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, FrameIteration) {
  const GURL main_url =
      embedded_test_server()->GetURL("fencedframe.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImplWrapper primary_rfh(primary_main_frame_host());

  const GURL fenced_frame_url = embedded_test_server()->GetURL(
      "fencedframe.test", "/fenced_frames/title1.html");
  RenderFrameHostImplWrapper fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(primary_rfh.get(),
                                                   fenced_frame_url));

  // Test that the outer => inner delegate mechanism works correctly.
  EXPECT_THAT(CollectAllRenderFrameHosts(primary_rfh.get()),
              testing::ElementsAre(primary_rfh.get(), fenced_frame_rfh.get()));

  // Test that the inner => outer delegate mechanism works correctly.
  EXPECT_EQ(nullptr, fenced_frame_rfh->GetParent());
  EXPECT_EQ(fenced_frame_rfh->GetParentOrOuterDocument(), primary_rfh.get());
  EXPECT_EQ(fenced_frame_rfh->GetOutermostMainFrame(), primary_rfh.get());
  EXPECT_EQ(fenced_frame_rfh->GetParentOrOuterDocumentOrEmbedder(),
            primary_rfh.get());
  EXPECT_EQ(fenced_frame_rfh->GetOutermostMainFrameOrEmbedder(),
            primary_rfh.get());

  // WebContentsImpl::ForEachFrameTree should include fenced frames.
  bool visited_fenced_frame_frame_tree = false;
  web_contents()->ForEachFrameTree(
      base::BindLambdaForTesting([&](FrameTree* frame_tree) {
        if (frame_tree == fenced_frame_rfh->frame_tree()) {
          visited_fenced_frame_frame_tree = true;
        }
      }));
  EXPECT_TRUE(visited_fenced_frame_frame_tree);
}

// Test that ensures we can post from an cross origin iframe into the
// fenced frame root.
IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, CrossOriginMessagePost) {
  const GURL main_url = embedded_test_server()->GetURL(
      "fencedframe.test", "/fenced_frames/title1.html");
  const GURL cross_origin_iframe_url =
      embedded_test_server()->GetURL("b.com", "/fenced_frames/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "fencedframe.test", "/title1.html")));
  RenderFrameHostImplWrapper primary_rfh(primary_main_frame_host());
  RenderFrameHostImplWrapper fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(primary_rfh.get(),
                                                   main_url));

  constexpr char kAddIframeScript[] = R"({
        (()=>{
            return new Promise((resolve) => {
              const frame = document.createElement('iframe');
              frame.addEventListener('load', () => {resolve();});
              frame.src = $1;
              document.body.appendChild(frame);
            });
        })();
        })";
  EXPECT_TRUE(ExecJs(fenced_frame_rfh.get(),
                     JsReplace(kAddIframeScript, cross_origin_iframe_url)));

  RenderFrameHostImpl* iframe = static_cast<RenderFrameHostImpl*>(
      ChildFrameAt(fenced_frame_rfh.get(), 0));

  EXPECT_TRUE(
      EvalJs(iframe->GetParent(), R"(window.addEventListener('message', (e) => {
                e.source.postMessage('echo ' + e.data, "*");
              }, false); true)")
          .ExtractBool());
  EXPECT_EQ("echo test", EvalJs(iframe, R"((async() => {
                      let promise = new Promise(function(resolve, reject) {
                        window.addEventListener('message', (e) => {
                          resolve(e.data)
                        }, false);
                        window.parent.postMessage('test', "*");
                      });
                      let result = await promise;
                      return result;
                    })())"));
}

// Test that when the documents inside fenced frame tree are loading,
// WebContentsObserver::DocumentOnLoadCompletedInPrimaryMainFrame is not invoked
// for fenced frames as it is only invoked for primary main frames.
IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest,
                       DocumentOnLoadCompletedInPrimaryMainFrame) {
  // Initialize a MockWebContentsObserver to ensure that
  // DocumentOnLoadCompletedInPrimaryMainFrame is only invoked for primary main
  // RenderFrameHosts.
  testing::NiceMock<MockWebContentsObserver> web_contents_observer(
      web_contents());

  // Navigate to an initial primary page. This should result in invoking
  // DocumentOnLoadCompletedInPrimaryMainFrame once.
  EXPECT_CALL(web_contents_observer,
              DocumentOnLoadCompletedInPrimaryMainFrame())
      .Times(1);
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "fencedframe.test", "/title1.html")));
  RenderFrameHostImplWrapper primary_rfh(primary_main_frame_host());

  // Once the fenced frame complets loading, it shouldn't result in
  // invoking DocumentOnLoadCompletedInPrimaryMainFrame.
  EXPECT_CALL(web_contents_observer,
              DocumentOnLoadCompletedInPrimaryMainFrame())
      .Times(0);
  const GURL fenced_frame_url = embedded_test_server()->GetURL(
      "fencedframe.test", "/fenced_frames/title1.html");
  RenderFrameHostImplWrapper inner_fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(primary_rfh.get(),
                                                   fenced_frame_url));
  FrameTreeNode* fenced_frame_root_node =
      inner_fenced_frame_rfh->frame_tree_node();
  EXPECT_FALSE(fenced_frame_root_node->IsLoading());
}

// Test that when the documents inside the fenced frame tree are loading,
// WebContentsObserver::PrimaryMainDocumentElementAvailable is not invoked for
// fenced frames as it is only invoked for primary main frames.
IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest,
                       PrimaryMainDocumentElementAvailable) {
  // Initialize a MockWebContentsObserver to ensure that
  // PrimaryMainDocumentElementAvailable is only invoked for primary main
  // RenderFrameHosts.
  testing::NiceMock<MockWebContentsObserver> web_contents_observer(
      web_contents());
  testing::InSequence s;

  // Navigate to an initial primary page. This should result in invoking
  // PrimaryMainDocumentElementAvailable once.
  EXPECT_CALL(web_contents_observer, PrimaryMainDocumentElementAvailable())
      .Times(1);
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "fencedframe.test", "/title1.html")));
  RenderFrameHostImplWrapper primary_rfh(primary_main_frame_host());

  // Once the fenced frame completes loading, it shouldn't result in
  // invoking PrimaryMainDocumentElementAvailable.
  EXPECT_CALL(web_contents_observer, PrimaryMainDocumentElementAvailable())
      .Times(0);
  const GURL fenced_frame_url = embedded_test_server()->GetURL(
      "fencedframe.test", "/fenced_frames/title1.html");
  RenderFrameHostImplWrapper inner_fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(primary_rfh.get(),
                                                   fenced_frame_url));
  FrameTreeNode* fenced_frame_root_node =
      inner_fenced_frame_rfh->frame_tree_node();
  EXPECT_FALSE(fenced_frame_root_node->IsLoading());
}

// Test that a fenced-frame does not perform any of the Android main-frame
// viewport behaviors like zoom-out-to-fit-content or parsing the viewport
// <meta>.
IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, ViewportSettings) {
  const GURL top_level_url = embedded_test_server()->GetURL(
      "fencedframe.test", "/fenced_frames/viewport.html");
  EXPECT_TRUE(NavigateToURL(shell(), top_level_url));

  RenderFrameHostImplWrapper primary_rfh(primary_main_frame_host());
  std::vector<FencedFrame*> fenced_frames = primary_rfh->GetFencedFrames();
  ASSERT_EQ(1ul, fenced_frames.size());
  FencedFrame* fenced_frame = fenced_frames.back();

  fenced_frame->WaitForDidStopLoadingForTesting();

  // Ensure various dimensions and properties in the fenced frame
  // match the dimensions of the <fencedframe> in the parent and do
  // not take into account the <meta name="viewport"> in the
  // fenced-frame page.
  EXPECT_EQ(
      EvalJs(fenced_frame->GetInnerRoot(), "window.innerWidth").ExtractInt(),
      314);
  EXPECT_EQ(
      EvalJs(fenced_frame->GetInnerRoot(), "window.innerHeight").ExtractInt(),
      271);
  EXPECT_EQ(EvalJs(fenced_frame->GetInnerRoot(),
                   "document.documentElement.clientWidth")
                .ExtractInt(),
            314);
  EXPECT_EQ(EvalJs(fenced_frame->GetInnerRoot(), "window.visualViewport.scale")
                .ExtractDouble(),
            1.0);
}

// Test that FrameTree::CollectNodesForIsLoading doesn't include inner
// WebContents nodes.
IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, NodesForIsLoading) {
  GURL url_a(embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL fenced_frame_url(embedded_test_server()->GetURL(
      "fencedframe.test", "/fenced_frames/title1.html"));

  // 1. Navigate to an initial primary page.
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "a.com", "/page_with_iframe.html")));
  RenderFrameHostImplWrapper primary_rfh(primary_main_frame_host());
  FrameTree& primary_frame_tree = web_contents()->GetPrimaryFrameTree();

  // 2. Create a fenced frame embedded inside primary page.
  RenderFrameHostImplWrapper outer_fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(primary_rfh.get(),
                                                   fenced_frame_url));

  // 3. Create a inner WebContents and attach it to the main contents. Navigate
  // the inner web contents to an initial page.
  WebContentsImpl* inner_contents =
      static_cast<WebContentsImpl*>(CreateAndAttachInnerContents(
          primary_rfh.get()->child_at(0)->current_frame_host()));
  ASSERT_TRUE(NavigateToURLFromRenderer(inner_contents, url_b));

  RenderFrameHostImpl* inner_contents_rfh = inner_contents->GetMainFrame();
  FrameTree& inner_contents_primary_frame_tree =
      inner_contents->GetPrimaryFrameTree();
  ASSERT_TRUE(inner_contents_rfh);

  // 4. Create a fenced frame embedded inside inner WebContents.
  RenderFrameHostImplWrapper inner_fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(inner_contents_rfh,
                                                   fenced_frame_url));

  // 5. FrameTree::CollectNodesForIsLoading should only include primary_rfh and
  // outer_fenced_frame_rfh when checked against outer delegate FrameTree.
  std::vector<RenderFrameHostImpl*> outer_web_contents_frames;
  for (auto* ftn :
       web_contents()->GetPrimaryFrameTree().CollectNodesForIsLoading()) {
    outer_web_contents_frames.push_back(ftn->current_frame_host());
  }
  EXPECT_EQ(outer_web_contents_frames.size(), 2u);
  EXPECT_THAT(outer_web_contents_frames,
              testing::UnorderedElementsAre(primary_rfh.get(),
                                            outer_fenced_frame_rfh.get()));

  // 6. FrameTree::CollectNodesForIsLoading should only include
  // inner_contents_rfh and inner_fenced_frame_rfh when checked against inner
  // delegate FrameTree.
  std::vector<RenderFrameHostImpl*> inner_web_contents_frames;
  for (auto* ftn :
       inner_contents->GetPrimaryFrameTree().CollectNodesForIsLoading()) {
    inner_web_contents_frames.push_back(ftn->current_frame_host());
  }
  EXPECT_EQ(inner_web_contents_frames.size(), 2u);
  EXPECT_THAT(inner_web_contents_frames,
              testing::UnorderedElementsAre(inner_contents_rfh,
                                            inner_fenced_frame_rfh.get()));

  // 7. Check that FrameTree::LoadingTree returns the correct FrameTree for both
  // outer and inner WebContents frame trees.
  EXPECT_NE(primary_frame_tree.LoadingTree(),
            inner_contents_primary_frame_tree.LoadingTree());
  EXPECT_EQ(primary_frame_tree.LoadingTree(), &primary_frame_tree);
  EXPECT_EQ(inner_contents_primary_frame_tree.LoadingTree(),
            &inner_contents_primary_frame_tree);
}

IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest,
                       NoErrorPageOnEmptyFrameHttpError) {
  const GURL kInitialUrl(
      embedded_test_server()->GetURL("a.com", "/title1.html"));
  const GURL kEmpty404Url(
      embedded_test_server()->GetURL("a.com", "/fenced_frames/empty404.html"));

  // Load an initial page.
  EXPECT_TRUE(NavigateToURL(shell(), kInitialUrl));
  RenderFrameHostImplWrapper initial_rfh(primary_main_frame_host());

  // Add a fenced frame empty page with 404 status.
  RenderFrameHostImplWrapper fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(initial_rfh.get(),
                                                   kEmpty404Url));
  ASSERT_TRUE(fenced_frame_rfh);

  // Confirm no error page was generated in its place.
  std::string contents =
      EvalJs(fenced_frame_rfh.get(), "document.body.textContent;")
          .ExtractString();
  EXPECT_EQ(contents, std::string());
}

namespace {

enum class FrameTypeWithOrigin {
  kSameOriginIframe,
  kCrossOriginIframe,
  kSameOriginFencedFrame,
  kCrossOriginFencedFrame,
};

const std::vector<FrameTypeWithOrigin> kTestParameters[] = {
    {},

    {FrameTypeWithOrigin::kSameOriginIframe},
    {FrameTypeWithOrigin::kCrossOriginIframe},
    {FrameTypeWithOrigin::kSameOriginIframe,
     FrameTypeWithOrigin::kSameOriginIframe},
    {FrameTypeWithOrigin::kSameOriginIframe,
     FrameTypeWithOrigin::kCrossOriginIframe},
    {FrameTypeWithOrigin::kCrossOriginIframe,
     FrameTypeWithOrigin::kSameOriginIframe},
    {FrameTypeWithOrigin::kCrossOriginIframe,
     FrameTypeWithOrigin::kCrossOriginIframe},

    {FrameTypeWithOrigin::kSameOriginFencedFrame},
    {FrameTypeWithOrigin::kCrossOriginFencedFrame},
    {FrameTypeWithOrigin::kSameOriginFencedFrame,
     FrameTypeWithOrigin::kSameOriginIframe},
    {FrameTypeWithOrigin::kSameOriginFencedFrame,
     FrameTypeWithOrigin::kCrossOriginIframe},
    {FrameTypeWithOrigin::kCrossOriginFencedFrame,
     FrameTypeWithOrigin::kSameOriginIframe},
    {FrameTypeWithOrigin::kCrossOriginFencedFrame,
     FrameTypeWithOrigin::kCrossOriginIframe}};

static std::string TestParamToString(
    ::testing::TestParamInfo<std::tuple<std::vector<FrameTypeWithOrigin>,
                                        bool /* shadow_dom_fenced_frame */>>
        param_info) {
  std::string out;
  for (const auto& frame_type : std::get<0>(param_info.param)) {
    switch (frame_type) {
      case FrameTypeWithOrigin::kSameOriginIframe:
        out += "SameI_";
        break;
      case FrameTypeWithOrigin::kCrossOriginIframe:
        out += "CrossI_";
        break;
      case FrameTypeWithOrigin::kSameOriginFencedFrame:
        out += "SameF_";
        break;
      case FrameTypeWithOrigin::kCrossOriginFencedFrame:
        out += "CrossF_";
        break;
    }
  }
  if (std::get<1>(param_info.param)) {
    out += "ShadowDOM";
  } else {
    out += "MPArch";
  }
  return out;
}

const char* kSameOriginHostName = "a.example";
const char* kCrossOriginHostName = "b.example";

const char* GetHostNameForFrameType(FrameTypeWithOrigin type) {
  switch (type) {
    case FrameTypeWithOrigin::kSameOriginIframe:
      return kSameOriginHostName;
    case FrameTypeWithOrigin::kCrossOriginIframe:
      return kCrossOriginHostName;
    case FrameTypeWithOrigin::kSameOriginFencedFrame:
      return kSameOriginHostName;
    case FrameTypeWithOrigin::kCrossOriginFencedFrame:
      return kCrossOriginHostName;
  }
}

bool IsFencedFrameType(FrameTypeWithOrigin type) {
  switch (type) {
    case FrameTypeWithOrigin::kSameOriginIframe:
      return false;
    case FrameTypeWithOrigin::kCrossOriginIframe:
      return false;
    case FrameTypeWithOrigin::kSameOriginFencedFrame:
      return true;
    case FrameTypeWithOrigin::kCrossOriginFencedFrame:
      return true;
  }
}

}  // namespace

class FencedFrameNestedFrameBrowserTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<
          std::tuple<std::vector<FrameTypeWithOrigin>,
                     bool /* shadow_dom_fenced_frame */>> {
 protected:
  FencedFrameNestedFrameBrowserTest() {
    if (std::get<1>(GetParam())) {
      feature_list_.InitAndEnableFeatureWithParameters(
          blink::features::kFencedFrames,
          {{"implementation_type", "shadow_dom"}});
    } else {
      fenced_frame_helper_ = std::make_unique<test::FencedFrameTestHelper>();
    }
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* LoadNestedFrame() {
    const GURL main_url =
        embedded_test_server()->GetURL(kSameOriginHostName, "/title1.html");
    EXPECT_TRUE(NavigateToURL(shell(), main_url));
    RenderFrameHostImpl* frame =
        static_cast<RenderFrameHostImpl*>(web_contents()->GetMainFrame());
    int depth = 0;
    for (const auto& type : std::get<0>(GetParam())) {
      ++depth;
      frame = CreateFrame(frame, type, depth);
    }
    return frame;
  }

  bool IsInFencedFrameTest() {
    for (const auto& type : std::get<0>(GetParam())) {
      if (IsFencedFrameType(type))
        return true;
    }
    return false;
  }

 private:
  RenderFrameHostImpl* CreateFrame(RenderFrameHostImpl* parent,
                                   FrameTypeWithOrigin type,
                                   int depth) {
    const GURL url = embedded_test_server()->GetURL(
        GetHostNameForFrameType(type),
        "/fenced_frames/title1.html?depth=" + base::NumberToString(depth));

    if (IsFencedFrameType(type)) {
      if (fenced_frame_helper_) {
        return static_cast<RenderFrameHostImpl*>(
            fenced_frame_helper_->CreateFencedFrame(parent, url));
      }
      // FencedFrameTestHelper only supports the MPArch version of fenced
      // frames. So need to maually create a fenced frame for the ShadowDOM
      // version.
      constexpr char kAddFencedFrameScript[] = R"({
          frame = document.createElement('fencedframe');
          document.body.appendChild(frame);
        })";
      EXPECT_TRUE(ExecJs(parent, kAddFencedFrameScript));
      constexpr char kNavigateFencedFrameScript[] = R"({
          document.body.getElementsByTagName('fencedframe')[0].src = $1;
        })";
      RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
          parent->child_at(0)->current_frame_host());
      TestFrameNavigationObserver observer(rfh);
      EXPECT_TRUE(ExecJs(parent, JsReplace(kNavigateFencedFrameScript, url)));
      observer.Wait();
      return static_cast<RenderFrameHostImpl*>(ChildFrameAt(parent, 0));
    }
    constexpr char kAddIframeScript[] = R"({
        (()=>{
            return new Promise((resolve) => {
              const frame = document.createElement('iframe');
              frame.addEventListener('load', () => {resolve();});
              frame.src = $1;
              document.body.appendChild(frame);
            });
        })();
        })";
    EXPECT_TRUE(ExecJs(parent, JsReplace(kAddIframeScript, url)));

    return static_cast<RenderFrameHostImpl*>(ChildFrameAt(parent, 0));
  }

  std::unique_ptr<test::FencedFrameTestHelper> fenced_frame_helper_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(FencedFrameNestedFrameBrowserTest,
                       IsNestedWithinFencedFrame) {
  RenderFrameHostImpl* rfh = LoadNestedFrame();
  EXPECT_EQ(IsInFencedFrameTest(), rfh->IsNestedWithinFencedFrame());
}

// Tests that NavigationHandle::GetNavigatingFrameType() returns the correct
// type.
IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, NavigationHandleFrameType) {
  {
    DidFinishNavigationObserver observer(
        web_contents(),
        base::BindLambdaForTesting([](NavigationHandle* navigation_handle) {
          EXPECT_TRUE(navigation_handle->IsInPrimaryMainFrame());
          DCHECK_EQ(navigation_handle->GetNavigatingFrameType(),
                    FrameType::kPrimaryMainFrame);
        }));
    EXPECT_TRUE(NavigateToURL(
        shell(),
        embedded_test_server()->GetURL("fencedframe.test", "/title1.html")));
  }

  {
    DidFinishNavigationObserver observer(
        web_contents(),
        base::BindLambdaForTesting([](NavigationHandle* navigation_handle) {
          EXPECT_FALSE(navigation_handle->IsInMainFrame());
          DCHECK_EQ(navigation_handle->GetNavigatingFrameType(),
                    FrameType::kSubframe);
        }));
    constexpr char kAddIframeScript[] = R"({
        (()=>{
            return new Promise((resolve) => {
              const frame = document.createElement('iframe');
              frame.addEventListener('load', () => {resolve();});
              frame.src = $1;
              document.body.appendChild(frame);
            });
        })();
        })";
    EXPECT_TRUE(ExecJs(
        primary_main_frame_host(),
        JsReplace(kAddIframeScript, embedded_test_server()->GetURL(
                                        "fencedframe.test", "/empty.html"))));
  }
  {
    const GURL fenced_frame_url = embedded_test_server()->GetURL(
        "fencedframe.test", "/fenced_frames/title1.html");
    RenderFrameHostImplWrapper fenced_frame_rfh(
        fenced_frame_test_helper().CreateFencedFrame(primary_main_frame_host(),
                                                     fenced_frame_url));
    DidFinishNavigationObserver observer(
        web_contents(),
        base::BindLambdaForTesting([](NavigationHandle* navigation_handle) {
          EXPECT_TRUE(
              navigation_handle->GetRenderFrameHost()->IsFencedFrameRoot());
          DCHECK_EQ(navigation_handle->GetNavigatingFrameType(),
                    FrameType::kFencedFrameRoot);
        }));
    fenced_frame_test_helper().NavigateFrameInFencedFrameTree(
        fenced_frame_rfh.get(), fenced_frame_url);
  }
}

INSTANTIATE_TEST_SUITE_P(FencedFrameNestedFrameBrowserTest,
                         FencedFrameNestedFrameBrowserTest,
                         testing::Combine(testing::ValuesIn(kTestParameters),
                                          testing::Bool()),
                         TestParamToString);

}  // namespace content
