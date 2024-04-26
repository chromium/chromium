// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/features.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/render_document_feature.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

namespace {

// Test with RenderDocument enabled.
class RenderDocumentHostBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InitAndEnableRenderDocumentFeature(
        &feature_list_for_render_document_,
        GetRenderDocumentLevelName(RenderDocumentLevel::kAllFrames));
    // Disable BackForwardCache so that the RenderFrameHost changes aren't
    // caused by proactive BrowsingInstance swap.
    feature_list_for_back_forward_cache_.InitAndDisableFeature(
        features::kBackForwardCache);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* current_main_frame() {
    return web_contents()->GetPrimaryFrameTree().root()->current_frame_host();
  }

 private:
  base::test::ScopedFeatureList feature_list_for_render_document_;
  base::test::ScopedFeatureList feature_list_for_back_forward_cache_;
};

}  // namespace

// A new RenderFrameHost must be used after a same process navigation.
IN_PROC_BROWSER_TEST_F(RenderDocumentHostBrowserTest, BasicMainFrame) {
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));
  GURL url_3(embedded_test_server()->GetURL("/title3.html"));

  // 1) Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  RenderFrameHostWrapper rfh_1(current_main_frame());

  // 2) Navigate to A2.
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  RenderFrameHostWrapper rfh_2(current_main_frame());
  EXPECT_TRUE(rfh_1.WaitUntilRenderFrameDeleted());

  // 3) Navigate to A3.
  EXPECT_TRUE(NavigateToURL(shell(), url_3));
  EXPECT_TRUE(rfh_2.WaitUntilRenderFrameDeleted());
}

// A new RenderFrameHost must be used after a same process subframe navigation.
// This test two cases, when the RenderFrame is not a local root and when it is.
IN_PROC_BROWSER_TEST_F(RenderDocumentHostBrowserTest, BasicSubframe) {
  GURL url(embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  GURL url_subframe_2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_subframe_3(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_subframe_4(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Setup a main frame with a same-process subframe
  EXPECT_TRUE(NavigateToURL(shell(), url));
  FrameTreeNode* subframe = current_main_frame()->child_at(0);
  RenderFrameHostImpl* child_rfh_1 = subframe->current_frame_host();
  RenderFrameDeletedObserver delete_child_rfh_1(child_rfh_1);

  // 2) Navigate the subframe same-process. (non local root case).
  NavigateIframeToURL(web_contents(), "test_iframe", url_subframe_2);
  RenderFrameHostImpl* child_rfh_2 = subframe->current_frame_host();
  EXPECT_TRUE(delete_child_rfh_1.deleted());
  EXPECT_NE(child_rfh_1, child_rfh_2);
  RenderFrameDeletedObserver delete_child_rfh_2(child_rfh_2);

  // 3) Navigate the subframe cross-process.
  NavigateIframeToURL(web_contents(), "test_iframe", url_subframe_3);
  RenderFrameHostImpl* child_rfh_3 = subframe->current_frame_host();
  EXPECT_TRUE(delete_child_rfh_1.deleted());
  EXPECT_NE(child_rfh_2, child_rfh_3);
  RenderFrameDeletedObserver delete_child_rfh_3(child_rfh_3);

  // 4) Navigate the subframe same-process. (local root case).
  NavigateIframeToURL(web_contents(), "test_iframe", url_subframe_4);
  RenderFrameHostImpl* child_rfh_4 = subframe->current_frame_host();
  EXPECT_TRUE(delete_child_rfh_3.deleted());
  EXPECT_NE(child_rfh_3, child_rfh_4);
}

// Two windows are scriptable with each other. Test it works appropriately after
// one of them doing a same-origin navigation.
IN_PROC_BROWSER_TEST_F(RenderDocumentHostBrowserTest, PopupScriptableNavigate) {
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));

  // 1) Navigate and open a new window.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  ShellAddedObserver shell_added_observer;
  EXPECT_TRUE(ExecJs(shell(), JsReplace("w = window.open($1)", url_1)));
  WebContents* new_contents = shell_added_observer.GetShell()->web_contents();
  EXPECT_TRUE(WaitForLoadStop(new_contents));

  // Both content have the same origin, so they are cross-scriptable.
  EXPECT_EQ(new_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
            web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());

  // 2) Reference a variable in between the two windows.
  EXPECT_TRUE(ExecJs(web_contents(),
                     "other_window = w.window;"
                     "other_window.foo = 'bar_1'"));

  // The object is accessible from each side.
  EXPECT_EQ("bar_1", EvalJs(web_contents(), "other_window.foo;"));
  EXPECT_EQ("bar_1", EvalJs(new_contents, "window.foo;"));

  // The URL is accessible from each side.
  EXPECT_EQ(url_1, EvalJs(web_contents(), "other_window.location.href;"));
  EXPECT_EQ(url_1, EvalJs(new_contents, "window.location.href;"));

  // 3) Navigate the new window same-process.
  int process_id = new_contents->GetPrimaryMainFrame()->GetProcess()->GetID();
  EXPECT_TRUE(NavigateToURL(new_contents, url_2));
  EXPECT_EQ(process_id,
            new_contents->GetPrimaryMainFrame()->GetProcess()->GetID());

  // The URL is accessible from each side and correctly reflects the current
  // value.
  EXPECT_EQ(url_2, EvalJs(web_contents(), "other_window.location.href;"));
  EXPECT_EQ(url_2, EvalJs(new_contents, "window.location.href;"));

  // The object is no longer accessible from each side.
  EXPECT_EQ(nullptr, EvalJs(web_contents(), "other_window.foo;"));
  EXPECT_EQ(nullptr, EvalJs(new_contents, "window.foo"));

  // Define the variable again.
  EXPECT_TRUE(ExecJs(web_contents(), "other_window.foo = 'bar_2';"));

  // The object is accessible from each side.
  EXPECT_EQ("bar_2", EvalJs(web_contents(), "other_window.foo;"));
  EXPECT_EQ("bar_2", EvalJs(new_contents, "window.foo;"));
}

// Two frames are scriptable with each other. Test it works appropriately after
// one of them doing a same-origin navigation.
IN_PROC_BROWSER_TEST_F(RenderDocumentHostBrowserTest,
                       SubframeScriptableNavigate) {
  GURL url_1(embedded_test_server()->GetURL("/page_with_iframe.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));
  GURL url_3(embedded_test_server()->GetURL("/title3.html"));

  // 1) Setup a main frame with an subframe.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  RenderFrameHostImpl* main_rfh = current_main_frame();
  RenderFrameHostImpl* child_rfh_1 =
      main_rfh->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver delete_child_rfh_1(child_rfh_1);

  // Both content have the same origin, so they are cross-scriptable.
  EXPECT_EQ(main_rfh->GetLastCommittedOrigin(),
            child_rfh_1->GetLastCommittedOrigin());
  // Reference a variable in between the two windows.
  EXPECT_TRUE(ExecJs(main_rfh,
                     "other_window = document.querySelector('iframe')"
                     "                       .contentWindow;"
                     "other_window.foo = 'bar_1'"));
  // The object is accessible from each side.
  EXPECT_EQ("bar_1", EvalJs(main_rfh, "other_window.foo;"));
  EXPECT_EQ("bar_1", EvalJs(child_rfh_1, "window.foo;"));

  // 2) Navigate the subframe.
  NavigateIframeToURL(web_contents(), "test_iframe", url_2);
  EXPECT_TRUE(delete_child_rfh_1.deleted());
  RenderFrameHostImpl* subframe_rfh_2 =
      main_rfh->child_at(0)->current_frame_host();

  // The object is no longer accessible from each side.
  EXPECT_EQ(nullptr, EvalJs(main_rfh, "other_window.foo;"));
  EXPECT_EQ(nullptr, EvalJs(subframe_rfh_2, "window.foo"));

  // Define the variable again.
  EXPECT_TRUE(ExecJs(main_rfh, "other_window.foo = 'bar_2';"));

  // The object is accessible from each side.
  EXPECT_EQ("bar_2", EvalJs(main_rfh, "other_window.foo;"));
  EXPECT_EQ("bar_2", EvalJs(subframe_rfh_2, "window.foo;"));
}

// A new window is created and the opener injects a new function into it before
// loading the document. This makes sure the window is not replaced and the
// function still accessible from the new document.
IN_PROC_BROWSER_TEST_F(RenderDocumentHostBrowserTest, InjectedFunction) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));

  // 1) Navigate and open a new window with an injected function.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ShellAddedObserver shell_added_observer;
  EXPECT_TRUE(ExecJs(
      shell(), JsReplace("w = window.open($1);"
                         "w.injected_function = () => { return 'It works!'; };",
                         url)));
  WebContents* new_contents = shell_added_observer.GetShell()->web_contents();

  // 2) Test the function is available to the new document.
  EXPECT_EQ("It works!", EvalJs(new_contents, "injected_function();"));
}

}  // namespace content
