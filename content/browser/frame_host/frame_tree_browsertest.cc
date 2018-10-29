// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/frame/sandbox_flags.h"
#include "url/url_constants.h"

namespace content {

namespace {

EvalJsResult GetOriginFromRenderer(FrameTreeNode* node) {
  return EvalJs(node, "self.origin");
}

}  // namespace

class FrameTreeBrowserTest : public ContentBrowserTest {
 public:
  FrameTreeBrowserTest() {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameTreeBrowserTest);
};

// Ensures FrameTree correctly reflects page structure during navigations.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, FrameTreeShape) {
  GURL base_url = embedded_test_server()->GetURL("A.com", "/site_isolation/");

  // Load doc without iframes. Verify FrameTree just has root.
  // Frame tree:
  //   Site-A Root
  NavigateToURL(shell(), base_url.Resolve("blank.html"));
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
      GetFrameTree()->root();
  EXPECT_EQ(0U, root->child_count());

  // Add 2 same-site frames. Verify 3 nodes in tree with proper names.
  // Frame tree:
  //   Site-A Root -- Site-A frame1
  //              \-- Site-A frame2
  WindowedNotificationObserver observer1(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &shell()->web_contents()->GetController()));
  NavigateToURL(shell(), base_url.Resolve("frames-X-X.html"));
  observer1.Wait();
  ASSERT_EQ(2U, root->child_count());
  EXPECT_EQ(0U, root->child_at(0)->child_count());
  EXPECT_EQ(0U, root->child_at(1)->child_count());
}

// TODO(ajwong): Talk with nasko and merge this functionality with
// FrameTreeShape.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, FrameTreeShape2) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/frame_tree/top.html"));

  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = wc->GetFrameTree()->root();

  // Check that the root node is properly created.
  ASSERT_EQ(3UL, root->child_count());
  EXPECT_EQ(std::string(), root->frame_name());

  ASSERT_EQ(2UL, root->child_at(0)->child_count());
  EXPECT_STREQ("1-1-name", root->child_at(0)->frame_name().c_str());

  // Verify the deepest node exists and has the right name.
  ASSERT_EQ(2UL, root->child_at(2)->child_count());
  EXPECT_EQ(1UL, root->child_at(2)->child_at(1)->child_count());
  EXPECT_EQ(0UL, root->child_at(2)->child_at(1)->child_at(0)->child_count());
  EXPECT_STREQ("3-1-name",
      root->child_at(2)->child_at(1)->child_at(0)->frame_name().c_str());

  // Navigate to about:blank, which should leave only the root node of the frame
  // tree in the browser process.
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  root = wc->GetFrameTree()->root();
  EXPECT_EQ(0UL, root->child_count());
  EXPECT_EQ(std::string(), root->frame_name());
}

// Test that we can navigate away if the previous renderer doesn't clean up its
// child frames.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, FrameTreeAfterCrash) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/frame_tree/top.html"));

  // Ensure the view and frame are live.
  RenderViewHost* rvh = shell()->web_contents()->GetRenderViewHost();
  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(rvh->GetMainFrame());
  EXPECT_TRUE(rvh->IsRenderViewLive());
  EXPECT_TRUE(rfh->IsRenderFrameLive());

  // Crash the renderer so that it doesn't send any FrameDetached messages.
  RenderProcessHostWatcher crash_observer(
      shell()->web_contents(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  ASSERT_TRUE(
      shell()->web_contents()->GetMainFrame()->GetProcess()->Shutdown(0));
  crash_observer.Wait();

  // The frame tree should be cleared.
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = wc->GetFrameTree()->root();
  EXPECT_EQ(0UL, root->child_count());

  // Ensure the view and frame aren't live anymore.
  EXPECT_FALSE(rvh->IsRenderViewLive());
  EXPECT_FALSE(rfh->IsRenderFrameLive());

  // Navigate to a new URL.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  NavigateToURL(shell(), url);
  EXPECT_EQ(0UL, root->child_count());
  EXPECT_EQ(url, root->current_url());

  // Ensure the view and frame are live again.
  EXPECT_TRUE(rvh->IsRenderViewLive());
  EXPECT_TRUE(rfh->IsRenderFrameLive());
}

// Test that we can navigate away if the previous renderer doesn't clean up its
// child frames.
// Flaky on Mac. http://crbug.com/452018
#if defined(OS_MACOSX)
#define MAYBE_NavigateWithLeftoverFrames DISABLED_NavigateWithLeftoverFrames
#else
#define MAYBE_NavigateWithLeftoverFrames NavigateWithLeftoverFrames
#endif
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, MAYBE_NavigateWithLeftoverFrames) {
  GURL base_url = embedded_test_server()->GetURL("A.com", "/site_isolation/");

  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/frame_tree/top.html"));

  // Hang the renderer so that it doesn't send any FrameDetached messages.
  // (This navigation will never complete, so don't wait for it.)
  shell()->LoadURL(GURL(kChromeUIHangURL));

  // Check that the frame tree still has children.
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = wc->GetFrameTree()->root();
  ASSERT_EQ(3UL, root->child_count());

  // Navigate to a new URL.  We use LoadURL because NavigateToURL will try to
  // wait for the previous navigation to stop.
  TestNavigationObserver tab_observer(wc, 1);
  shell()->LoadURL(base_url.Resolve("blank.html"));
  tab_observer.Wait();

  // The frame tree should now be cleared.
  EXPECT_EQ(0UL, root->child_count());
}

// Ensure that IsRenderFrameLive is true for main frames and same-site iframes.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, IsRenderFrameLive) {
  GURL main_url(embedded_test_server()->GetURL("/frame_tree/top.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()->root();

  // The root and subframe should each have a live RenderFrame.
  EXPECT_TRUE(
      root->current_frame_host()->render_view_host()->IsRenderViewLive());
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_TRUE(root->child_at(0)->current_frame_host()->IsRenderFrameLive());

  // Load a same-site page into iframe and it should still be live.
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  NavigateFrameToURL(root->child_at(0), http_url);
  EXPECT_TRUE(
      root->current_frame_host()->render_view_host()->IsRenderViewLive());
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_TRUE(root->child_at(0)->current_frame_host()->IsRenderFrameLive());
}

// Ensure that origins are correctly set on navigations.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, OriginSetOnNavigation) {
  GURL about_blank(url::kAboutBlankURL);
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  WebContents* contents = shell()->web_contents();

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(contents)->GetFrameTree()->root();

  // Extra '/' is added because the replicated origin is serialized in RFC 6454
  // format, which dictates no trailing '/', whereas GURL::GetOrigin does put a
  // '/' at the end.
  EXPECT_EQ(main_url.GetOrigin().spec(),
            root->current_origin().Serialize() + '/');
  EXPECT_EQ(
      main_url.GetOrigin().spec(),
      root->current_frame_host()->GetLastCommittedOrigin().Serialize() + '/');

  // The iframe is inititially same-origin.
  EXPECT_TRUE(
      root->current_frame_host()->GetLastCommittedOrigin().IsSameOriginWith(
          root->child_at(0)->current_frame_host()->GetLastCommittedOrigin()));
  EXPECT_EQ(root->current_origin().Serialize(), GetOriginFromRenderer(root));
  EXPECT_EQ(root->child_at(0)->current_origin().Serialize(),
            GetOriginFromRenderer(root->child_at(0)));

  // Navigate the iframe cross-origin.
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);
  EXPECT_EQ(frame_url, root->child_at(0)->current_url());
  EXPECT_EQ(frame_url.GetOrigin().spec(),
            root->child_at(0)->current_origin().Serialize() + '/');
  EXPECT_FALSE(
      root->current_frame_host()->GetLastCommittedOrigin().IsSameOriginWith(
          root->child_at(0)->current_frame_host()->GetLastCommittedOrigin()));
  EXPECT_EQ(root->current_origin().Serialize(), GetOriginFromRenderer(root));
  EXPECT_EQ(root->child_at(0)->current_origin().Serialize(),
            GetOriginFromRenderer(root->child_at(0)));

  // Parent-initiated about:blank navigation should inherit the parent's a.com
  // origin.
  NavigateIframeToURL(contents, "1-1-id", about_blank);
  EXPECT_EQ(about_blank, root->child_at(0)->current_url());
  EXPECT_EQ(main_url.GetOrigin().spec(),
            root->child_at(0)->current_origin().Serialize() + '/');
  EXPECT_EQ(root->current_frame_host()->GetLastCommittedOrigin().Serialize(),
            root->child_at(0)
                ->current_frame_host()
                ->GetLastCommittedOrigin()
                .Serialize());
  EXPECT_TRUE(
      root->current_frame_host()->GetLastCommittedOrigin().IsSameOriginWith(
          root->child_at(0)->current_frame_host()->GetLastCommittedOrigin()));
  EXPECT_EQ(root->current_origin().Serialize(), GetOriginFromRenderer(root));
  EXPECT_EQ(root->child_at(0)->current_origin().Serialize(),
            GetOriginFromRenderer(root->child_at(0)));

  GURL data_url("data:text/html,foo");
  EXPECT_TRUE(NavigateToURL(shell(), data_url));

  // Navigating to a data URL should set a unique origin.  This is represented
  // as "null" per RFC 6454.
  EXPECT_EQ("null", root->current_origin().Serialize());
  EXPECT_TRUE(contents->GetMainFrame()->GetLastCommittedOrigin().opaque());
  EXPECT_EQ("null", GetOriginFromRenderer(root));

  // Re-navigating to a normal URL should update the origin.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(main_url.GetOrigin().spec(),
            root->current_origin().Serialize() + '/');
  EXPECT_EQ(
      main_url.GetOrigin().spec(),
      contents->GetMainFrame()->GetLastCommittedOrigin().Serialize() + '/');
  EXPECT_FALSE(contents->GetMainFrame()->GetLastCommittedOrigin().opaque());
  EXPECT_EQ(root->current_origin().Serialize(), GetOriginFromRenderer(root));
}

// Tests a cross-origin navigation to a blob URL. The main frame initiates this
// navigation on its grandchild. It should wind up in the main frame's process.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, NavigateGrandchildToBlob) {
  WebContents* contents = shell()->web_contents();
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(contents)->GetFrameTree()->root();

  // First, snapshot the FrameTree for a normal A(B(A)) case where all frames
  // are served over http. The blob test should result in the same structure.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(b(a))")));
  std::string reference_tree = FrameTreeVisualizer().DepictFrameTree(root);

  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // The root node will initiate the navigation; its grandchild node will be the
  // target of the navigation.
  FrameTreeNode* target = root->child_at(0)->child_at(0);

  RenderFrameDeletedObserver deleted_observer(target->current_frame_host());
  std::string html =
      "<html><body><div>This is blob content.</div>"
      "<script>"
      "window.parent.parent.postMessage('HI', self.origin);"
      "</script></body></html>";
  std::string script = JsReplace(
      "new Promise((resolve) => {"
      "  window.addEventListener('message', resolve, false);"
      "  var blob = new Blob([$1], {type: 'text/html'});"
      "  var blob_url = URL.createObjectURL(blob);"
      "  frames[0][0].location.href = blob_url;"
      "}).then((event) => {"
      "  document.body.appendChild(document.createTextNode(event.data));"
      "  return event.source.location.href;"
      "});",
      html);
  std::string blob_url_string = EvalJs(root, script).ExtractString();
  // Wait for the RenderFrame to go away, if this will be cross-process.
  if (AreAllSitesIsolatedForTesting())
    deleted_observer.WaitUntilDeleted();
  EXPECT_EQ(GURL(blob_url_string), target->current_url());
  EXPECT_EQ(url::kBlobScheme, target->current_url().scheme());
  EXPECT_FALSE(target->current_origin().opaque());
  EXPECT_EQ("a.com", target->current_origin().host());
  EXPECT_EQ(url::kHttpScheme, target->current_origin().scheme());
  EXPECT_EQ("This is blob content.",
            EvalJs(target, "document.body.children[0].innerHTML"));
  EXPECT_EQ(reference_tree, FrameTreeVisualizer().DepictFrameTree(root));
}

IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, NavigateChildToAboutBlank) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  WebContentsImpl* contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // The leaf node (c.com) will be navigated. Its parent node (b.com) will
  // initiate the navigation.
  FrameTreeNode* target =
      contents->GetFrameTree()->root()->child_at(0)->child_at(0);
  FrameTreeNode* initiator = target->parent();

  // Give the target a name.
  EXPECT_TRUE(ExecJs(target, "window.name = 'target';"));

  // Use window.open(about:blank), then poll the document for access.
  EvalJsResult about_blank_origin = EvalJs(
      initiator,
      "new Promise(resolve => {"
      "  var didNavigate = false;"
      "  var intervalID = setInterval(function() {"
      "    if (!didNavigate) {"
      "      didNavigate = true;"
      "      window.open('about:blank', 'target');"
      "    }"
      "    // Poll the document until it doesn't throw a SecurityError.\n"
      "    try {"
      "      frames[0].document.write('Hi from ' + document.domain);"
      "    } catch (e) { return; }"
      "    clearInterval(intervalID);"
      "    resolve(frames[0].self.origin);"
      "  }, 16);"
      "});");
  EXPECT_EQ(target->current_origin(), about_blank_origin);
  EXPECT_EQ(GURL(url::kAboutBlankURL), target->current_url());
  EXPECT_EQ(url::kAboutScheme, target->current_url().scheme());
  EXPECT_FALSE(target->current_origin().opaque());
  EXPECT_EQ("b.com", target->current_origin().host());
  EXPECT_EQ(url::kHttpScheme, target->current_origin().scheme());

  EXPECT_EQ("Hi from b.com", EvalJs(target, "document.body.innerHTML"));
}

// Nested iframes, three origins: A(B(C)). Frame A navigates C to about:blank
// (via window.open). This should wind up in A's origin per the spec. Test fails
// because of http://crbug.com/564292
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest,
                       DISABLED_NavigateGrandchildToAboutBlank) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  WebContentsImpl* contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // The leaf node (c.com) will be navigated. Its grandparent node (a.com) will
  // initiate the navigation.
  FrameTreeNode* target =
      contents->GetFrameTree()->root()->child_at(0)->child_at(0);
  FrameTreeNode* initiator = target->parent()->parent();

  // Give the target a name.
  EXPECT_TRUE(ExecJs(target, "window.name = 'target';"));

  // Use window.open(about:blank), then poll the document for access.
  EvalJsResult about_blank_origin =
      EvalJs(initiator,
             "new Promise((resolve) => {"
             "  var didNavigate = false;"
             "  var intervalID = setInterval(() => {"
             "    if (!didNavigate) {"
             "      didNavigate = true;"
             "      window.open('about:blank', 'target');"
             "    }"
             "    // May raise a SecurityError, that's expected.\n"
             "    try {"
             "      frames[0][0].document.write('Hi from ' + document.domain);"
             "    } catch (e) { return; }"
             "    clearInterval(intervalID);"
             "    resolve(frames[0][0].self.origin);"
             "  }, 16);"
             "});");
  EXPECT_EQ(target->current_origin(), about_blank_origin);
  EXPECT_EQ(GURL(url::kAboutBlankURL), target->current_url());
  EXPECT_EQ(url::kAboutScheme, target->current_url().scheme());
  EXPECT_FALSE(target->current_origin().opaque());
  EXPECT_EQ("a.com", target->current_origin().host());
  EXPECT_EQ(url::kHttpScheme, target->current_origin().scheme());

  EXPECT_EQ("Hi from a.com", EvalJs(target, "document.body.innerHTML"));
}

// Ensures that iframe with srcdoc is always put in the same origin as its
// parent frame.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, ChildFrameWithSrcdoc) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  WebContentsImpl* contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = contents->GetFrameTree()->root();
  EXPECT_EQ(1U, root->child_count());

  FrameTreeNode* child = root->child_at(0);
  std::string frame_origin = EvalJs(child, "self.origin;").ExtractString();
  EXPECT_TRUE(
      child->current_frame_host()->GetLastCommittedOrigin().IsSameOriginWith(
          url::Origin::Create(GURL(frame_origin))));
  EXPECT_FALSE(
      root->current_frame_host()->GetLastCommittedOrigin().IsSameOriginWith(
          url::Origin::Create(GURL(frame_origin))));

  // Create a new iframe with srcdoc and add it to the main frame. It should
  // be created in the same SiteInstance as the parent.
  {
    std::string script("var f = document.createElement('iframe');"
                       "f.srcdoc = 'some content';"
                       "document.body.appendChild(f)");
    TestNavigationObserver observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, script));
    EXPECT_EQ(2U, root->child_count());
    observer.Wait();

    EXPECT_EQ(GURL(kAboutSrcDocURL), root->child_at(1)->current_url());
    EvalJsResult frame_origin = EvalJs(root->child_at(1), "self.origin");
    EXPECT_EQ(root->current_frame_host()->GetLastCommittedURL().GetOrigin(),
              GURL(frame_origin.ExtractString()));
    EXPECT_NE(child->current_frame_host()->GetLastCommittedURL().GetOrigin(),
              GURL(frame_origin.ExtractString()));
  }

  // Set srcdoc on the existing cross-site frame. It should navigate the frame
  // back to the origin of the parent.
  {
    std::string script("var f = document.getElementById('child-0');"
                       "f.srcdoc = 'some content';");
    TestNavigationObserver observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, script));
    observer.Wait();

    EXPECT_EQ(GURL(kAboutSrcDocURL), child->current_url());
    EXPECT_EQ(
        url::Origin::Create(root->current_frame_host()->GetLastCommittedURL())
            .Serialize(),
        EvalJs(child, "self.origin"));
  }
}

// Ensure that sandbox flags are correctly set in the main frame when set by
// Content-Security-Policy header.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, SandboxFlagsSetForMainFrame) {
  GURL main_url(embedded_test_server()->GetURL("/csp_sandboxed_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Verify that sandbox flags are set properly for the root FrameTreeNode and
  // RenderFrameHost. Root frame is sandboxed with "allow-scripts".
  EXPECT_EQ(blink::WebSandboxFlags::kNone,
            root->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(blink::WebSandboxFlags::kAll & ~blink::WebSandboxFlags::kScripts &
                ~blink::WebSandboxFlags::kAutomaticFeatures,
            root->active_sandbox_flags());
  EXPECT_EQ(root->active_sandbox_flags(),
            root->current_frame_host()->active_sandbox_flags());

  // Verify that child frames inherit sandbox flags from the root. First frame
  // has no explicitly set flags of its own, and should inherit those from the
  // root. Second frame is completely sandboxed.
  EXPECT_EQ(blink::WebSandboxFlags::kAll & ~blink::WebSandboxFlags::kScripts &
                ~blink::WebSandboxFlags::kAutomaticFeatures,
            root->child_at(0)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(blink::WebSandboxFlags::kAll & ~blink::WebSandboxFlags::kScripts &
                ~blink::WebSandboxFlags::kAutomaticFeatures,
            root->child_at(0)->active_sandbox_flags());
  EXPECT_EQ(root->child_at(0)->active_sandbox_flags(),
            root->child_at(0)->current_frame_host()->active_sandbox_flags());
  EXPECT_EQ(blink::WebSandboxFlags::kAll,
            root->child_at(1)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(blink::WebSandboxFlags::kAll,
            root->child_at(1)->active_sandbox_flags());
  EXPECT_EQ(root->child_at(1)->active_sandbox_flags(),
            root->child_at(1)->current_frame_host()->active_sandbox_flags());

  // Navigating the main frame to a different URL should clear sandbox flags.
  GURL unsandboxed_url(embedded_test_server()->GetURL("/title1.html"));
  NavigateFrameToURL(root, unsandboxed_url);

  // Verify that sandbox flags are cleared properly for the root FrameTreeNode
  // and RenderFrameHost.
  EXPECT_EQ(blink::WebSandboxFlags::kNone,
            root->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(blink::WebSandboxFlags::kNone, root->active_sandbox_flags());
  EXPECT_EQ(blink::WebSandboxFlags::kNone,
            root->current_frame_host()->active_sandbox_flags());
}

// Ensure that sandbox flags are correctly set when child frames are created.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, SandboxFlagsSetForChildFrames) {
  GURL main_url(embedded_test_server()->GetURL("/sandboxed_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()->root();

  // Verify that sandbox flags are set properly for all FrameTreeNodes.
  // First frame is completely sandboxed; second frame uses "allow-scripts",
  // which resets both SandboxFlags::Scripts and
  // SandboxFlags::AutomaticFeatures bits per blink::parseSandboxPolicy(), and
  // third frame has "allow-scripts allow-same-origin".
  EXPECT_EQ(blink::WebSandboxFlags::kNone,
            root->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(blink::WebSandboxFlags::kAll,
            root->child_at(0)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(blink::WebSandboxFlags::kAll & ~blink::WebSandboxFlags::kScripts &
                ~blink::WebSandboxFlags::kAutomaticFeatures,
            root->child_at(1)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(blink::WebSandboxFlags::kAll & ~blink::WebSandboxFlags::kScripts &
                ~blink::WebSandboxFlags::kAutomaticFeatures &
                ~blink::WebSandboxFlags::kOrigin,
            root->child_at(2)->effective_frame_policy().sandbox_flags);

  // Sandboxed frames should set a unique origin unless they have the
  // "allow-same-origin" directive.
  EXPECT_EQ("null", root->child_at(0)->current_origin().Serialize());
  EXPECT_EQ("null", root->child_at(1)->current_origin().Serialize());
  EXPECT_EQ(main_url.GetOrigin().spec(),
            root->child_at(2)->current_origin().Serialize() + "/");

  // Navigating to a different URL should not clear sandbox flags.
  GURL frame_url(embedded_test_server()->GetURL("/title1.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);
  EXPECT_EQ(blink::WebSandboxFlags::kAll,
            root->child_at(0)->effective_frame_policy().sandbox_flags);
}

// Ensure that sandbox flags are correctly set in the child frames when set by
// Content-Security-Policy header, and in combination with the sandbox iframe
// attribute.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest,
                       SandboxFlagsSetByCSPForChildFrames) {
  GURL main_url(embedded_test_server()->GetURL("/sandboxed_frames_csp.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Verify that sandbox flags are set properly for all FrameTreeNodes.
  // First frame has no iframe sandbox flags, but the framed document is served
  // with a CSP header which sets "allow-scripts", "allow-popups" and
  // "allow-pointer-lock".
  // Second frame is sandboxed with "allow-scripts", "allow-pointer-lock" and
  // "allow-orientation-lock", and the framed document is also served with a CSP
  // header which uses "allow-popups" and "allow-pointer-lock". The resulting
  // sandbox for the frame should only have "allow-pointer-lock".
  EXPECT_EQ(blink::WebSandboxFlags::kNone,
            root->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(blink::WebSandboxFlags::kNone, root->active_sandbox_flags());
  EXPECT_EQ(root->active_sandbox_flags(),
            root->current_frame_host()->active_sandbox_flags());
  EXPECT_EQ(blink::WebSandboxFlags::kNone,
            root->child_at(0)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(blink::WebSandboxFlags::kAll & ~blink::WebSandboxFlags::kScripts &
                ~blink::WebSandboxFlags::kAutomaticFeatures &
                ~blink::WebSandboxFlags::kPopups &
                ~blink::WebSandboxFlags::kPointerLock,
            root->child_at(0)->active_sandbox_flags());
  EXPECT_EQ(root->child_at(0)->active_sandbox_flags(),
            root->child_at(0)->current_frame_host()->active_sandbox_flags());
  EXPECT_EQ(blink::WebSandboxFlags::kAll & ~blink::WebSandboxFlags::kScripts &
                ~blink::WebSandboxFlags::kAutomaticFeatures &
                ~blink::WebSandboxFlags::kPointerLock &
                ~blink::WebSandboxFlags::kOrientationLock,
            root->child_at(1)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(blink::WebSandboxFlags::kAll & ~blink::WebSandboxFlags::kScripts &
                ~blink::WebSandboxFlags::kAutomaticFeatures &
                ~blink::WebSandboxFlags::kPointerLock,
            root->child_at(1)->active_sandbox_flags());
  EXPECT_EQ(root->child_at(1)->active_sandbox_flags(),
            root->child_at(1)->current_frame_host()->active_sandbox_flags());

  // Navigating to a different URL *should* clear CSP-set sandbox flags, but
  // should retain those flags set by the frame owner.
  GURL frame_url(embedded_test_server()->GetURL("/title1.html"));

  NavigateFrameToURL(root->child_at(0), frame_url);
  EXPECT_EQ(blink::WebSandboxFlags::kNone,
            root->child_at(0)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(blink::WebSandboxFlags::kNone,
            root->child_at(0)->active_sandbox_flags());
  EXPECT_EQ(root->child_at(0)->active_sandbox_flags(),
            root->child_at(0)->current_frame_host()->active_sandbox_flags());

  NavigateFrameToURL(root->child_at(1), frame_url);
  EXPECT_EQ(blink::WebSandboxFlags::kAll & ~blink::WebSandboxFlags::kScripts &
                ~blink::WebSandboxFlags::kAutomaticFeatures &
                ~blink::WebSandboxFlags::kPointerLock &
                ~blink::WebSandboxFlags::kOrientationLock,
            root->child_at(1)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(blink::WebSandboxFlags::kAll & ~blink::WebSandboxFlags::kScripts &
                ~blink::WebSandboxFlags::kAutomaticFeatures &
                ~blink::WebSandboxFlags::kPointerLock &
                ~blink::WebSandboxFlags::kOrientationLock,
            root->child_at(1)->active_sandbox_flags());
  EXPECT_EQ(root->child_at(1)->active_sandbox_flags(),
            root->child_at(1)->current_frame_host()->active_sandbox_flags());
}

// Ensure that a popup opened from a subframe sets its opener to the subframe's
// FrameTreeNode, and that the opener is cleared if the subframe is destroyed.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, SubframeOpenerSetForNewWindow) {
  GURL main_url(embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Open a new window from a subframe.
  ShellAddedObserver new_shell_observer;
  GURL popup_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(
      ExecJs(root->child_at(0), JsReplace("window.open($1);", popup_url)));
  Shell* new_shell = new_shell_observer.GetShell();
  WebContents* new_contents = new_shell->web_contents();
  WaitForLoadStop(new_contents);

  // Check that the new window's opener points to the correct subframe on
  // original window.
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(new_contents)->GetFrameTree()->root();
  EXPECT_EQ(root->child_at(0), popup_root->opener());

  // Close the original window.  This should clear the new window's opener.
  shell()->Close();
  EXPECT_EQ(nullptr, popup_root->opener());
}

class CrossProcessFrameTreeBrowserTest : public ContentBrowserTest {
 public:
  CrossProcessFrameTreeBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CrossProcessFrameTreeBrowserTest);
};

// Ensure that we can complete a cross-process subframe navigation.
IN_PROC_BROWSER_TEST_F(CrossProcessFrameTreeBrowserTest,
                       CreateCrossProcessSubframeProxies) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()->root();

  // There should not be a proxy for the root's own SiteInstance.
  SiteInstance* root_instance = root->current_frame_host()->GetSiteInstance();
  EXPECT_FALSE(root->render_manager()->GetRenderFrameProxyHost(root_instance));

  // Load same-site page into iframe.
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  NavigateFrameToURL(root->child_at(0), http_url);

  // Load cross-site page into iframe.
  GURL cross_site_url(
      embedded_test_server()->GetURL("foo.com", "/title2.html"));
  NavigateFrameToURL(root->child_at(0), cross_site_url);

  // Ensure that we have created a new process for the subframe.
  ASSERT_EQ(2U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  SiteInstance* child_instance = child->current_frame_host()->GetSiteInstance();
  RenderViewHost* rvh = child->current_frame_host()->render_view_host();
  RenderProcessHost* rph = child->current_frame_host()->GetProcess();

  EXPECT_NE(shell()->web_contents()->GetRenderViewHost(), rvh);
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(), child_instance);
  EXPECT_NE(shell()->web_contents()->GetMainFrame()->GetProcess(), rph);

  // Ensure that the root node has a proxy for the child node's SiteInstance.
  EXPECT_TRUE(root->render_manager()->GetRenderFrameProxyHost(child_instance));

  // Also ensure that the child has a proxy for the root node's SiteInstance.
  EXPECT_TRUE(child->render_manager()->GetRenderFrameProxyHost(root_instance));

  // The nodes should not have proxies for their own SiteInstance.
  EXPECT_FALSE(root->render_manager()->GetRenderFrameProxyHost(root_instance));
  EXPECT_FALSE(
      child->render_manager()->GetRenderFrameProxyHost(child_instance));

  // Ensure that the RenderViews and RenderFrames are all live.
  EXPECT_TRUE(
      root->current_frame_host()->render_view_host()->IsRenderViewLive());
  EXPECT_TRUE(
      child->current_frame_host()->render_view_host()->IsRenderViewLive());
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_TRUE(root->child_at(0)->current_frame_host()->IsRenderFrameLive());
}

IN_PROC_BROWSER_TEST_F(CrossProcessFrameTreeBrowserTest,
                       OriginSetOnCrossProcessNavigations) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()->root();

  EXPECT_EQ(root->current_origin().Serialize() + '/',
            main_url.GetOrigin().spec());

  // First frame is an about:blank frame.  Check that its origin is correctly
  // inherited from the parent.
  EXPECT_EQ(root->child_at(0)->current_origin().Serialize() + '/',
            main_url.GetOrigin().spec());

  // Second frame loads a same-site page.  Its origin should also be the same
  // as the parent.
  EXPECT_EQ(root->child_at(1)->current_origin().Serialize() + '/',
            main_url.GetOrigin().spec());

  // Load cross-site page into the first frame.
  GURL cross_site_url(
      embedded_test_server()->GetURL("foo.com", "/title2.html"));
  NavigateFrameToURL(root->child_at(0), cross_site_url);

  EXPECT_EQ(root->child_at(0)->current_origin().Serialize() + '/',
            cross_site_url.GetOrigin().spec());

  // The root's origin shouldn't have changed.
  EXPECT_EQ(root->current_origin().Serialize() + '/',
            main_url.GetOrigin().spec());

  GURL data_url("data:text/html,foo");
  NavigateFrameToURL(root->child_at(1), data_url);

  // Navigating to a data URL should set a unique origin.  This is represented
  // as "null" per RFC 6454.
  EXPECT_EQ(root->child_at(1)->current_origin().Serialize(), "null");
}

// Ensure that a popup opened from a sandboxed main frame inherits sandbox flags
// from its opener.
IN_PROC_BROWSER_TEST_F(CrossProcessFrameTreeBrowserTest,
                       SandboxFlagsSetForNewWindow) {
  GURL main_url(
      embedded_test_server()->GetURL("/sandboxed_main_frame_script.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Open a new window from the main frame.
  GURL popup_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  Shell* new_shell = OpenPopup(root->current_frame_host(), popup_url, "");
  EXPECT_TRUE(new_shell);
  WebContents* new_contents = new_shell->web_contents();

  // Check that the new window's sandbox flags correctly reflect the opener's
  // flags. Main frame sets allow-popups, allow-pointer-lock and allow-scripts.
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(new_contents)->GetFrameTree()->root();
  blink::WebSandboxFlags main_frame_sandbox_flags =
      root->current_frame_host()->active_sandbox_flags();
  EXPECT_EQ(blink::WebSandboxFlags::kAll & ~blink::WebSandboxFlags::kPopups &
                ~blink::WebSandboxFlags::kPointerLock &
                ~blink::WebSandboxFlags::kScripts &
                ~blink::WebSandboxFlags::kAutomaticFeatures,
            main_frame_sandbox_flags);

  EXPECT_EQ(main_frame_sandbox_flags,
            popup_root->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(main_frame_sandbox_flags, popup_root->active_sandbox_flags());
  EXPECT_EQ(main_frame_sandbox_flags,
            popup_root->current_frame_host()->active_sandbox_flags());
}

// FrameTreeBrowserTest variant where we isolate http://*.is, Iceland's top
// level domain. This is an analogue to isolating extensions, which we can use
// inside content_browsertests, where extensions don't exist. Iceland, like an
// extension process, is a special place with magical powers; we want to protect
// it from outsiders.
class IsolateIcelandFrameTreeBrowserTest : public ContentBrowserTest {
 public:
  IsolateIcelandFrameTreeBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // blink suppresses navigations to blob URLs of origins different from the
    // frame initiating the navigation. We disable those checks for this test,
    // to test what happens in a compromise scenario.
    command_line->AppendSwitch(switches::kDisableWebSecurity);
    command_line->AppendSwitchASCII(switches::kIsolateSitesForTesting, "*.is");
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IsolateIcelandFrameTreeBrowserTest);
};

// Regression test for https://crbug.com/644966
IN_PROC_BROWSER_TEST_F(IsolateIcelandFrameTreeBrowserTest,
                       ProcessSwitchForIsolatedBlob) {
  // Set up an iframe.
  WebContents* contents = shell()->web_contents();
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(contents)->GetFrameTree()->root();
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // The navigation targets an invalid blob url; that's intentional to trigger
  // an error response. The response should commit in a process dedicated to
  // http://b.is.
  EXPECT_EQ(
      "done",
      EvalJs(
          root,
          "new Promise((resolve) => {"
          "  var iframe_element = document.getElementsByTagName('iframe')[0];"
          "  iframe_element.onload = () => resolve('done');"
          "  iframe_element.src = 'blob:http://b.is:2932/';"
          "});"));
  WaitForLoadStop(contents);

  // Make sure we did a process transfer back to "b.is".
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.is/",
      FrameTreeVisualizer().DepictFrameTree(root));
}

}  // namespace content
