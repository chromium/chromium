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
#include "content/public/browser/navigating_frame_type.h"
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
                     "document.querySelector('fencedframe').remove();"));
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

namespace {

enum class FrameType {
  kSameOriginIframe,
  kCrossOriginIframe,
  kSameOriginFencedFrame,
  kCrossOriginFencedFrame,
};

const std::vector<FrameType> kTestParameters[] = {
    {},

    {FrameType::kSameOriginIframe},
    {FrameType::kCrossOriginIframe},
    {FrameType::kSameOriginIframe, FrameType::kSameOriginIframe},
    {FrameType::kSameOriginIframe, FrameType::kCrossOriginIframe},
    {FrameType::kCrossOriginIframe, FrameType::kSameOriginIframe},
    {FrameType::kCrossOriginIframe, FrameType::kCrossOriginIframe},

    {FrameType::kSameOriginFencedFrame},
    {FrameType::kCrossOriginFencedFrame},
    {FrameType::kSameOriginFencedFrame, FrameType::kSameOriginIframe},
    {FrameType::kSameOriginFencedFrame, FrameType::kCrossOriginIframe},
    {FrameType::kCrossOriginFencedFrame, FrameType::kSameOriginIframe},
    {FrameType::kCrossOriginFencedFrame, FrameType::kCrossOriginIframe}};

static std::string TestParamToString(
    ::testing::TestParamInfo<
        std::tuple<std::vector<FrameType>, bool /* shadow_dom_fenced_frame */>>
        param_info) {
  std::string out;
  for (const auto& frame_type : std::get<0>(param_info.param)) {
    switch (frame_type) {
      case FrameType::kSameOriginIframe:
        out += "SameI_";
        break;
      case FrameType::kCrossOriginIframe:
        out += "CrossI_";
        break;
      case FrameType::kSameOriginFencedFrame:
        out += "SameF_";
        break;
      case FrameType::kCrossOriginFencedFrame:
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

const char* GetHostNameForFrameType(FrameType type) {
  switch (type) {
    case FrameType::kSameOriginIframe:
      return kSameOriginHostName;
    case FrameType::kCrossOriginIframe:
      return kCrossOriginHostName;
    case FrameType::kSameOriginFencedFrame:
      return kSameOriginHostName;
    case FrameType::kCrossOriginFencedFrame:
      return kCrossOriginHostName;
  }
}

bool IsFencedFrameType(FrameType type) {
  switch (type) {
    case FrameType::kSameOriginIframe:
      return false;
    case FrameType::kCrossOriginIframe:
      return false;
    case FrameType::kSameOriginFencedFrame:
      return true;
    case FrameType::kCrossOriginFencedFrame:
      return true;
  }
}

}  // namespace

class FencedFrameNestedFrameBrowserTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<
          std::tuple<std::vector<FrameType>,
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
                                   FrameType type,
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
                    NavigatingFrameType::kPrimaryMainFrame);
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
                    NavigatingFrameType::kSubframe);
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
                    NavigatingFrameType::kFencedFrameRoot);
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
