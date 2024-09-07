// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_timeouts.h"
#include "base/test/with_feature_override.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/browser/renderer_host/cross_process_frame_connector.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/site_per_process_browsertest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/render_document_feature.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::WhenSorted;

namespace content {

namespace {

void AddPagehideHandler(const ToRenderFrameHost& target, const char* message) {
  EXPECT_TRUE(
      ExecJs(target, JsReplace("window.onpagehide = function() { "
                               "  window.domAutomationController.send($1);"
                               "}",
                               message)));
}

}  // namespace

// Tests that there are no crashes if a subframe is detached in its pagehide
// handler. See https://crbug.com/590054.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, DetachInPagehideHandler) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(b))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "        +--Site B -- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  EXPECT_EQ(1, EvalJs(root->child_at(0), "frames.length;"));

  RenderFrameDeletedObserver deleted_observer(
      root->child_at(0)->child_at(0)->current_frame_host());

  // Add a pagehide handler to the grandchild that causes it to be synchronously
  // detached, then navigate it.
  EXPECT_TRUE(
      ExecJs(root->child_at(0)->child_at(0),
             "window.onpagehide=function(e){\n"
             "    window.parent.document.getElementById('child-0').remove();\n"
             "};\n"));
  auto script = JsReplace("window.document.getElementById('child-0').src = $1",
                          embedded_test_server()->GetURL(
                              "c.com", "/cross_site_iframe_factory.html?c"));
  EXPECT_TRUE(ExecJs(root->child_at(0), script));

  deleted_observer.WaitUntilDeleted();

  EXPECT_EQ(0, EvalJs(root->child_at(0), "frames.length;"));

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));
}

// Tests that trying to navigate in the pagehide handler doesn't crash the
// browser.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, NavigateInPagehideHandler) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(b))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "        +--Site B -- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  EXPECT_EQ(1,
            EvalJs(root->child_at(0)->current_frame_host(), "frames.length;"));

  // Add a pagehide handler to B's subframe.
  EXPECT_TRUE(ExecJs(root->child_at(0)->child_at(0)->current_frame_host(),
                     "window.onpagehide=function(e){\n"
                     "    window.location = '#navigate';\n"
                     "};\n"));

  // Navigate B's subframe to a cross-site C.
  RenderFrameDeletedObserver deleted_observer(
      root->child_at(0)->child_at(0)->current_frame_host());
  auto script = JsReplace("window.document.getElementById('child-0').src = $1",
                          embedded_test_server()->GetURL(
                              "c.com", "/cross_site_iframe_factory.html"));
  EXPECT_TRUE(ExecJs(root->child_at(0)->current_frame_host(), script));

  // Wait until B's subframe RenderFrameHost is destroyed.
  deleted_observer.WaitUntilDeleted();

  // Check that C's subframe is alive and the navigation in the pagehide handler
  // was ignored.
  EXPECT_EQ(0, EvalJs(root->child_at(0)->child_at(0)->current_frame_host(),
                      "frames.length;"));

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://c.com/",
      DepictFrameTree(root));
}

// Verifies that when navigating an OOPIF to same site and then canceling
// navigation from beforeunload handler popup will not remove the
// RemoteFrameView from OOPIF's owner element in the parent process. This test
// uses OOPIF visibility to make sure RemoteFrameView exists after beforeunload
// is handled.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       CanceledBeforeUnloadShouldNotClearRemoteFrameView) {
  GURL a_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), a_url));

  FrameTreeNode* child_node =
      web_contents()->GetPrimaryFrameTree().root()->child_at(0);
  GURL b_url(embedded_test_server()->GetURL(
      "b.com", "/render_frame_host/beforeunload.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child_node, b_url));
  CrossProcessFrameConnector* frame_connector_delegate =
      static_cast<RenderWidgetHostViewChildFrame*>(
          child_node->current_frame_host()->GetView())
          ->FrameConnectorForTesting();

  // Need user gesture for 'beforeunload' to fire.
  PrepContentsForBeforeUnloadTest(web_contents());

  // Simulate user choosing to stay on the page after beforeunload fired.
  SetShouldProceedOnBeforeUnload(shell(), true /* proceed */,
                                 false /* success */);

  // First, hide the <iframe>. This goes through RemoteFrameView::Hide() and
  // eventually updates the CrossProcessFrameConnector. Also,
  // RemoteFrameView::self_visible_ will be set to false which can only be
  // undone by calling RemoteFrameView::Show. Therefore, potential calls to
  // RemoteFrameView::SetParentVisible(true) would not update the visibility at
  // the browser side.
  ASSERT_TRUE(
      ExecJs(web_contents(),
             "document.querySelector('iframe').style.visibility = 'hidden';"));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return frame_connector_delegate->IsHidden(); }));

  // Now we navigate the child to about:blank, but since we do not proceed with
  // the navigation, the OOPIF should stay alive and RemoteFrameView intact.
  AppModalDialogWaiter dialog_waiter(shell());
  ASSERT_TRUE(ExecJs(web_contents(),
                     "document.querySelector('iframe').src = 'about:blank';"));
  dialog_waiter.Wait();

  // Sanity check: We should still have an OOPIF and hence a RWHVCF.
  ASSERT_TRUE(static_cast<RenderWidgetHostViewBase*>(
                  child_node->current_frame_host()->GetView())
                  ->IsRenderWidgetHostViewChildFrame());

  // Now make the <iframe> visible again. This calls RemoteFrameView::Show()
  // only if the RemoteFrameView is the EmbeddedContentView of the corresponding
  // HTMLFrameOwnerElement.
  ASSERT_TRUE(
      ExecJs(web_contents(),
             "document.querySelector('iframe').style.visibility = 'visible';"));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !frame_connector_delegate->IsHidden(); }));
}

// Ensure that after a main frame with an OOPIF is navigated cross-site, the
// pagehide handler in the OOPIF sees correct main frame origin, namely the old
// and not the new origin.  See https://crbug.com/825283.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ParentOriginDoesNotChangeInPagehideHandler) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Open a popup on b.com.  The b.com subframe on the main frame will use this
  // in its pagehide handler.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Save the WebContents instance created via the popup to be able to listen
  // for messages that occur in it.
  auto* popup_shell = OpenPopup(shell()->web_contents(), b_url, "popup");
  WebContents* popup_web_contents = popup_shell->web_contents();

  // Add a pagehide handler to b.com subframe, which will look up the top
  // frame's origin and send it via domAutomationController.  Unfortunately,
  // the subframe's browser-side state will have been torn down when it runs
  // the pagehide handler, so to ensure that the message can be received, send
  // it through the popup.
  EXPECT_TRUE(ExecJs(root->child_at(0),
                     "window.onpagehide = function(e) {"
                     "  window.open('','popup').domAutomationController.send("
                     "      'top-origin ' + location.ancestorOrigins[0]);"
                     "};"));

  // Navigate the main frame to c.com and wait for the message from the
  // subframe's pagehide handler.
  GURL c_url(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // NOTE: The message occurs in the WebContents for the popup.
  DOMMessageQueue msg_queue(popup_web_contents);
  EXPECT_TRUE(NavigateToURL(shell(), c_url));
  std::string message, top_origin;
  while (msg_queue.WaitForMessage(&message)) {
    base::TrimString(message, "\"", &message);
    auto message_parts = base::SplitString(message, " ", base::TRIM_WHITESPACE,
                                           base::SPLIT_WANT_NONEMPTY);
    if (message_parts[0] == "top-origin") {
      top_origin = message_parts[1];
      break;
    }
  }

  // The top frame's origin should be a.com, not c.com.
  EXPECT_EQ(top_origin + "/", main_url.DeprecatedGetOriginAsURL().spec());
}

// Verify that when the last active frame in a process is going away as part of
// OnUnload, the mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame is
// received prior to the process starting to shut down, ensuring that any
// related unload work also happens before shutdown. See
// https://crbug.com/867274 and https://crbug.com/794625.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       UnloadACKArrivesPriorToProcessShutdownRequest) {
  GURL start_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  RenderFrameHostImpl* rfh = web_contents()->GetPrimaryMainFrame();
  rfh->DisableUnloadTimerForTesting();

  // Navigate cross-site.  Since the current frame is the last active frame in
  // the current process, the process will eventually shut down.  Once the
  // process goes away, ensure that the
  // mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame was received (i.e.,
  // that we didn't just simulate OnUnloaded() due to the process erroneously
  // going away before the mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame
  // was received, as in https://crbug.com/867274).
  RenderProcessHostWatcher watcher(
      rfh->GetProcess(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  bool received_unload = false;
  auto unload_ack_filter = base::BindLambdaForTesting([&]() {
    received_unload = true;
    return false;
  });
  rfh->SetUnloadACKCallbackForTesting(unload_ack_filter);

  // Disable the BackForwardCache to ensure the old process is going to be
  // released.
  DisableBackForwardCacheForTesting(web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL cross_site_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), cross_site_url));
  watcher.Wait();
  EXPECT_TRUE(received_unload);
  EXPECT_TRUE(watcher.did_exit_normally());
}

// This is a regression test for https://crbug.com/891423 in which tabs showing
// beforeunload dialogs stalled navigation and triggered the "hung process"
// dialog.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NoCommitTimeoutWithBeforeUnloadDialog) {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // Navigate first tab to a.com.
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), a_url));
  RenderProcessHost* a_process =
      web_contents->GetPrimaryMainFrame()->GetProcess();

  // Open b.com in a second tab.  Using a renderer-initiated navigation is
  // important to leave a.com and b.com SiteInstances in the same
  // BrowsingInstance (so the b.com -> a.com navigation in the next test step
  // will reuse the process associated with the first a.com tab).
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  Shell* new_shell = OpenPopup(web_contents, b_url, "newtab");
  WebContents* new_contents = new_shell->web_contents();
  EXPECT_TRUE(WaitForLoadStop(new_contents));
  RenderProcessHost* b_process =
      new_contents->GetPrimaryMainFrame()->GetProcess();
  EXPECT_NE(a_process, b_process);

  // Disable the beforeunload hang monitor (otherwise there will be a race
  // between the beforeunload dialog and the beforeunload hang timer) and give
  // the page a gesture to allow dialogs.
  web_contents->GetPrimaryMainFrame()
      ->DisableBeforeUnloadHangMonitorForTesting();
  web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptWithUserGestureForTests(
      std::u16string(), base::NullCallback(), ISOLATED_WORLD_ID_GLOBAL);

  // Hang the first contents in a beforeunload dialog.
  BeforeUnloadBlockingDelegate test_delegate(web_contents);
  EXPECT_TRUE(
      ExecJs(web_contents, "window.onbeforeunload=function(e){ return 'x' }"));
  EXPECT_TRUE(ExecJs(web_contents,
                     "setTimeout(function() { window.location.reload() }, 0)"));
  test_delegate.Wait();

  // Attempt to navigate the second tab to a.com.  This will attempt to reuse
  // the hung process.
  base::TimeDelta kTimeout = base::Milliseconds(100);
  NavigationRequest::SetCommitTimeoutForTesting(kTimeout);
  GURL hung_url(embedded_test_server()->GetURL("a.com", "/title3.html"));
  UnresponsiveRendererObserver unresponsive_renderer_observer(new_contents);
  EXPECT_TRUE(
      ExecJs(new_contents, JsReplace("window.location = $1", hung_url)));

  // Verify that we will not be notified about the unresponsive renderer.
  // Before changes in https://crrev.com/c/1089797, the test would get notified
  // and therefore |hung_process| would be non-null.
  RenderProcessHost* hung_process =
      unresponsive_renderer_observer.Wait(kTimeout * 10);
  EXPECT_FALSE(hung_process);

  // Reset the timeout.
  NavigationRequest::SetCommitTimeoutForTesting(base::TimeDelta());
}

// Test that pagehide handlers in iframes are run, even when the removed subtree
// is complicated with nested iframes in different processes.
//     A1                         A1
//    / \                        / \
//   B1  D  --- Navigate --->   E   D
//  / \
// C1  C2
// |   |
// B2  A2
//     |
//     C3
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, PagehideHandlerSubframes) {
  // The test expects the previous document to be deleted on navigation.
  DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c(b),c(a(c))),d)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Add a pagehide handler to every frames. It notifies the browser using the
  // DomAutomationController it has been executed.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  AddPagehideHandler(root, "A1");
  AddPagehideHandler(root->child_at(0), "B1");
  AddPagehideHandler(root->child_at(0)->child_at(0), "C1");
  AddPagehideHandler(root->child_at(0)->child_at(1), "C2");
  AddPagehideHandler(root->child_at(0)->child_at(0)->child_at(0), "B2");
  AddPagehideHandler(root->child_at(0)->child_at(1)->child_at(0), "A2");
  AddPagehideHandler(root->child_at(0)->child_at(1)->child_at(0)->child_at(0),
                     "C3");
  DOMMessageQueue dom_message_queue(
      WebContents::FromRenderFrameHost(web_contents()->GetPrimaryMainFrame()));

  // Disable the unload timer on B1.
  root->child_at(0)->current_frame_host()->DisableUnloadTimerForTesting();

  // Process B and C are expected to shutdown once every unload handler has
  // run.
  RenderProcessHostWatcher shutdown_B(
      root->child_at(0)->current_frame_host()->GetProcess(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  RenderProcessHostWatcher shutdown_C(
      root->child_at(0)->child_at(0)->current_frame_host()->GetProcess(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  // Navigate B to E.
  GURL e_url(embedded_test_server()->GetURL("e.com", "/title1.html"));
  NavigateFrameToURL(root->child_at(0), e_url);

  // Collect pagehide handler messages.
  std::string message;
  std::vector<std::string> messages;
  for (int i = 0; i < 6; ++i) {
    EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
    base::TrimString(message, "\"", &message);
    messages.push_back(message);
  }
  EXPECT_FALSE(dom_message_queue.PopMessage(&message));

  // Check every frame in the replaced subtree has executed its pagehide
  // handler.
  EXPECT_THAT(messages,
              WhenSorted(ElementsAre("A2", "B1", "B2", "C1", "C2", "C3")));

  // In every renderer process, check ancestors have executed their unload
  // handler before their children. This is a slightly less restrictive
  // condition than the specification which requires it to be global instead of
  // per process.
  // https://html.spec.whatwg.org/multipage/browsing-the-web.html#unloading-documents
  //
  // In process B:
  auto B1 = base::ranges::find(messages, "B1");
  auto B2 = base::ranges::find(messages, "B2");
  EXPECT_LT(B1, B2);

  // In process C:
  auto C2 = base::ranges::find(messages, "C2");
  auto C3 = base::ranges::find(messages, "C3");
  EXPECT_LT(C2, C3);

  // Make sure the processes are deleted at some point.
  shutdown_B.Wait();
  shutdown_C.Wait();
}

// Check that unload handlers in iframe don't prevents the main frame to be
// deleted after a timeout.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, SlowUnloadHandlerInIframe) {
  GURL initial_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL next_url(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate on a page with an iframe.
  EXPECT_TRUE(NavigateToURL(shell(), initial_url));

  // 2) Act as if there was an infinite unload handler in B.
  RenderFrameHostImpl* rfh_b = web_contents()
                                   ->GetPrimaryFrameTree()
                                   .root()
                                   ->child_at(0)
                                   ->current_frame_host();
  rfh_b->DoNotDeleteForTesting();

  // With BackForwardCache, old document doesn't fire unload handlers as the
  // page is stored in BackForwardCache on navigation.
  DisableBackForwardCacheForTesting(web_contents(),
                                    BackForwardCache::TEST_USES_UNLOAD_EVENT);

  // 3) Navigate and check the old document is deleted after some time.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  RenderFrameDeletedObserver deleted_observer(root->current_frame_host());
  EXPECT_TRUE(NavigateToURL(shell(), next_url));
  deleted_observer.WaitUntilDeleted();
}

// Navigate from A(B(A(B)) to C. Check the pagehide handler are executed,
// executed in the right order and the processes for A and B are removed.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, PagehideHandlerABAB) {
  // The test expects the previous document to be deleted on navigation.
  DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL initial_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(a(b)))"));
  GURL next_url(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate on a page with an iframe.
  EXPECT_TRUE(NavigateToURL(shell(), initial_url));

  // 2) Add pagehide handler on every frame.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  AddPagehideHandler(root, "A1");
  AddPagehideHandler(root->child_at(0), "B1");
  AddPagehideHandler(root->child_at(0)->child_at(0), "A2");
  AddPagehideHandler(root->child_at(0)->child_at(0)->child_at(0), "B2");
  root->current_frame_host()->DisableUnloadTimerForTesting();

  DOMMessageQueue dom_message_queue(
      WebContents::FromRenderFrameHost(web_contents()->GetPrimaryMainFrame()));
  RenderProcessHostWatcher shutdown_A(
      root->current_frame_host()->GetProcess(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  RenderProcessHostWatcher shutdown_B(
      root->child_at(0)->current_frame_host()->GetProcess(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  // 3) Navigate cross process.
  EXPECT_TRUE(NavigateToURL(shell(), next_url));

  // 4) Wait for pagehide handler messages and check they are sent in order.
  std::vector<std::string> messages;
  std::string message;
  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
    base::TrimString(message, "\"", &message);
    messages.push_back(message);
  }
  EXPECT_FALSE(dom_message_queue.PopMessage(&message));

  EXPECT_THAT(messages, WhenSorted(ElementsAre("A1", "A2", "B1", "B2")));
  auto A1 = base::ranges::find(messages, "A1");
  auto A2 = base::ranges::find(messages, "A2");
  auto B1 = base::ranges::find(messages, "B1");
  auto B2 = base::ranges::find(messages, "B2");
  EXPECT_LT(A1, A2);
  EXPECT_LT(B1, B2);

  // Make sure the processes are deleted at some point.
  shutdown_A.Wait();
  shutdown_B.Wait();
}

// Start with A(B(C)), navigate C to D and then B to E. By emulating a slow
// unload handler in B,C and D, the end result is C is in pending deletion in B
// and B is in pending deletion in A.
//   (1)     (2)     (3)
//|       |       |       |
//|   A   |  A    |   A   |
//|   |   |  |    |    \  |
//|   B   |  B    |  B  E |
//|   |   |   \   |   \   |
//|   C   | C  D  | C  D  |
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, UnloadNestedPendingDeletion) {
  std::string onunload_script = "window.onunload = function(){}";
  GURL url_abc(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  GURL url_d(embedded_test_server()->GetURL("d.com", "/title1.html"));
  GURL url_e(embedded_test_server()->GetURL("e.com", "/title1.html"));

  // 1) Navigate to a page with an iframe.
  EXPECT_TRUE(NavigateToURL(shell(), url_abc));
  RenderFrameHostImpl* rfh_a = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_c = rfh_b->child_at(0)->current_frame_host();
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_a->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_b->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_c->lifecycle_state());

  // Act as if there was a slow unload handler on rfh_b and rfh_c.
  // The navigating frames are waiting for
  // mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame.
  auto unload_ack_filter = base::BindRepeating([] { return true; });
  rfh_b->SetUnloadACKCallbackForTesting(unload_ack_filter);
  rfh_c->SetUnloadACKCallbackForTesting(unload_ack_filter);
  EXPECT_TRUE(ExecJs(rfh_b->frame_tree_node(), onunload_script));
  EXPECT_TRUE(ExecJs(rfh_c->frame_tree_node(), onunload_script));
  rfh_b->DisableUnloadTimerForTesting();
  rfh_c->DisableUnloadTimerForTesting();

  RenderFrameDeletedObserver delete_b(rfh_b), delete_c(rfh_c);

  // 2) Navigate rfh_c to D.
  EXPECT_TRUE(NavigateToURLFromRenderer(rfh_c->frame_tree_node(), url_d));
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_a->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_b->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kRunningUnloadHandlers,
            rfh_c->lifecycle_state());
  RenderFrameHostImpl* rfh_d = rfh_b->child_at(0)->current_frame_host();
  // Set an arbitrarily long timeout to ensure the subframe unload timer doesn't
  // fire before we call OnDetach().
  rfh_d->SetSubframeUnloadTimeoutForTesting(base::Seconds(30));

  RenderFrameDeletedObserver delete_d(rfh_d);

  // Act as if there was a slow unload handler on rfh_d.
  // The non navigating frames are waiting for mojom::FrameHost::Detach.
  rfh_d->DoNotDeleteForTesting();
  EXPECT_TRUE(ExecJs(rfh_d->frame_tree_node(), onunload_script));

  // 3) Navigate rfh_b to E.
  EXPECT_TRUE(NavigateToURLFromRenderer(rfh_b->frame_tree_node(), url_e));
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_a->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kRunningUnloadHandlers,
            rfh_b->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kRunningUnloadHandlers,
            rfh_c->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kRunningUnloadHandlers,
            rfh_d->lifecycle_state());

  // rfh_d completes its unload event. It deletes the frame, including rfh_c.
  EXPECT_FALSE(delete_c.deleted());
  EXPECT_FALSE(delete_d.deleted());
  rfh_d->DetachForTesting();
  EXPECT_TRUE(delete_c.deleted());
  EXPECT_TRUE(delete_d.deleted());

  // rfh_b completes its unload event.
  EXPECT_FALSE(delete_b.deleted());
  rfh_b->SetUnloadACKCallbackForTesting(base::NullCallback());
  rfh_b->OnUnloadACK();
  EXPECT_TRUE(delete_b.deleted());
}

// A set of nested frames A1(B1(A2)) are pending deletion because of a
// navigation. This tests what happens if only A2 has a pagehide handler.
// If B1's mojom::FrameHost::Detach is called before A2, it should not destroy
// itself and its children, but rather wait for A2.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, PartialPagehideHandler) {
  // The test expects the previous document to be deleted on navigation.
  DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url_aba(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(a))"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to A1(B1(A2))
  EXPECT_TRUE(NavigateToURL(shell(), url_aba));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  RenderFrameHostImpl* a1 = root->current_frame_host();
  RenderFrameHostImpl* b1 = a1->child_at(0)->current_frame_host();
  RenderFrameHostImpl* a2 = b1->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver delete_a1(a1);
  RenderFrameDeletedObserver delete_a2(a2);
  RenderFrameDeletedObserver delete_b1(b1);

  // Disable Detach and mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame.
  // They will be called manually.
  auto unload_ack_filter = base::BindRepeating([] { return true; });
  a1->SetUnloadACKCallbackForTesting(unload_ack_filter);
  a1->DoNotDeleteForTesting();
  a2->DoNotDeleteForTesting();

  a1->DisableUnloadTimerForTesting();
  // Set an arbitrarily long timeout to ensure the subframe unload timer doesn't
  // fire before we call OnDetach().
  b1->SetSubframeUnloadTimeoutForTesting(base::Seconds(30));

  // Add pagehide handler on A2, but not on the other frames.
  AddPagehideHandler(a2->frame_tree_node(), "A2");

  DOMMessageQueue dom_message_queue(
      WebContents::FromRenderFrameHost(web_contents()->GetPrimaryMainFrame()));

  // 2) Navigate cross process.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));

  // Check that pagehide handlers are executed.
  std::string message, message_unused;
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
  EXPECT_FALSE(dom_message_queue.PopMessage(&message_unused));
  EXPECT_EQ("\"A2\"", message);

  // No RenderFrameHost are deleted so far.
  EXPECT_FALSE(delete_a1.deleted());
  EXPECT_FALSE(delete_b1.deleted());
  EXPECT_FALSE(delete_a2.deleted());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kRunningUnloadHandlers,
            a1->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kReadyToBeDeleted,
            b1->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kRunningUnloadHandlers,
            a2->lifecycle_state());

  // 3) B1 receives confirmation it has been deleted. This has no effect,
  //    because it is still waiting on A2 to be deleted.
  b1->DetachForTesting();
  EXPECT_FALSE(delete_a1.deleted());
  EXPECT_FALSE(delete_b1.deleted());
  EXPECT_FALSE(delete_a2.deleted());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kRunningUnloadHandlers,
            a1->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kReadyToBeDeleted,
            b1->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kRunningUnloadHandlers,
            a2->lifecycle_state());

  // 4) A2 received confirmation that it has been deleted and destroy B1 and A2.
  a2->DetachForTesting();
  EXPECT_FALSE(delete_a1.deleted());
  EXPECT_TRUE(delete_b1.deleted());
  EXPECT_TRUE(delete_a2.deleted());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kRunningUnloadHandlers,
            a1->lifecycle_state());

  // 5) A1 receives mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame and
  // deletes itself.
  a1->ResumeDeletionForTesting();
  a1->SetUnloadACKCallbackForTesting(base::NullCallback());
  a1->OnUnloadACK();
  EXPECT_TRUE(delete_a1.deleted());
}

// Test RenderFrameHostImpl::PendingDeletionCheckCompletedOnSubtree.
//
// After a navigation commit, some children with no pagehide handler may be
// eligible for immediate deletion. Several configurations are tested:
//
// Before navigation commit
//
//              0               |  N  : No pagehide handler
//   ‑‑‑‑‑‑‑‑‑‑‑‑‑‑‑‑‑‑‑‑‑      | [N] : Pagehide handler
//  |  |  |  |  |   |     |     |
// [1] 2 [3] 5  7   9     12    |
//        |  |  |  / \   / \    |
//        4 [6] 8 10 11 13 [14] |
//
// After navigation commit (expected)
//
//              0               |  N  : No pagehide handler
//   ---------------------      | [N] : Pagehide handler
//  |     |  |            |     |
// [1]   [3] 5            12    |
//           |             \    |
//          [6]            [14] |
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       PendingDeletionCheckCompletedOnSubtree) {
  // The test expects the previous document to be deleted on navigation.
  DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url_1(embedded_test_server()->GetURL(
      "a.com",
      "/cross_site_iframe_factory.html?a(a,a,a(a),a(a),a(a),a(a,a),a(a,a))"));
  GURL url_2(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to 0(1,2,3(4),5(6),7(8),9(10,11),12(13,14));
  EXPECT_TRUE(NavigateToURL(shell(), url_1));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  RenderFrameHostImpl* rfh_0 = root->current_frame_host();
  RenderFrameHostImpl* rfh_1 = rfh_0->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_2 = rfh_0->child_at(1)->current_frame_host();
  RenderFrameHostImpl* rfh_3 = rfh_0->child_at(2)->current_frame_host();
  RenderFrameHostImpl* rfh_4 = rfh_3->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_5 = rfh_0->child_at(3)->current_frame_host();
  RenderFrameHostImpl* rfh_6 = rfh_5->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_7 = rfh_0->child_at(4)->current_frame_host();
  RenderFrameHostImpl* rfh_8 = rfh_7->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_9 = rfh_0->child_at(5)->current_frame_host();
  RenderFrameHostImpl* rfh_10 = rfh_9->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_11 = rfh_9->child_at(1)->current_frame_host();
  RenderFrameHostImpl* rfh_12 = rfh_0->child_at(6)->current_frame_host();
  RenderFrameHostImpl* rfh_13 = rfh_12->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_14 = rfh_12->child_at(1)->current_frame_host();

  RenderFrameDeletedObserver delete_a0(rfh_0), delete_a1(rfh_1),
      delete_a2(rfh_2), delete_a3(rfh_3), delete_a4(rfh_4), delete_a5(rfh_5),
      delete_a6(rfh_6), delete_a7(rfh_7), delete_a8(rfh_8), delete_a9(rfh_9),
      delete_a10(rfh_10), delete_a11(rfh_11), delete_a12(rfh_12),
      delete_a13(rfh_13), delete_a14(rfh_14);

  // Add the pagehide handlers.
  AddPagehideHandler(rfh_1->frame_tree_node(), "");
  AddPagehideHandler(rfh_3->frame_tree_node(), "");
  AddPagehideHandler(rfh_6->frame_tree_node(), "");
  AddPagehideHandler(rfh_14->frame_tree_node(), "");

  // Disable Detach and mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame.
  auto unload_ack_filter = base::BindRepeating([] { return true; });
  rfh_0->SetUnloadACKCallbackForTesting(unload_ack_filter);
  rfh_0->DoNotDeleteForTesting();
  rfh_1->DoNotDeleteForTesting();
  rfh_3->DoNotDeleteForTesting();
  rfh_5->DoNotDeleteForTesting();
  rfh_6->DoNotDeleteForTesting();
  rfh_12->DoNotDeleteForTesting();
  rfh_14->DoNotDeleteForTesting();
  rfh_0->DisableUnloadTimerForTesting();

  // 2) Navigate cross process and check the tree. See diagram above.
  EXPECT_TRUE(NavigateToURL(shell(), url_2));

  EXPECT_FALSE(delete_a0.deleted());
  EXPECT_FALSE(delete_a1.deleted());
  EXPECT_TRUE(delete_a2.deleted());
  EXPECT_FALSE(delete_a3.deleted());
  EXPECT_TRUE(delete_a4.deleted());
  EXPECT_FALSE(delete_a5.deleted());
  EXPECT_FALSE(delete_a6.deleted());
  EXPECT_TRUE(delete_a7.deleted());
  EXPECT_TRUE(delete_a8.deleted());
  EXPECT_TRUE(delete_a9.deleted());
  EXPECT_TRUE(delete_a10.deleted());
  EXPECT_TRUE(delete_a11.deleted());
  EXPECT_FALSE(delete_a12.deleted());
  EXPECT_TRUE(delete_a13.deleted());
  EXPECT_FALSE(delete_a14.deleted());
}

// When an iframe is detached, check that pagehide handlers execute in all of
// its child frames. Start from A(B(C)) and delete B from A.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       DetachedIframePagehideHandlerABC) {
  GURL initial_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));

  // 1) Navigate to a(b(c))
  EXPECT_TRUE(NavigateToURL(shell(), initial_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  RenderFrameHostImpl* rfh_a = root->current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_c = rfh_b->child_at(0)->current_frame_host();

  // 2) Add pagehide handlers on B and C.
  AddPagehideHandler(rfh_b->frame_tree_node(), "B");
  AddPagehideHandler(rfh_c->frame_tree_node(), "C");

  DOMMessageQueue dom_message_queue(web_contents());
  RenderProcessHostWatcher shutdown_B(
      rfh_b->GetProcess(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  RenderProcessHostWatcher shutdown_C(
      rfh_c->GetProcess(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  // 3) Detach B from A.
  ExecuteScriptAsync(root, "document.querySelector('iframe').remove();");

  // 4) Wait for pagehide handler.
  std::vector<std::string> messages(2);
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&messages[0]));
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&messages[1]));
  std::string unused;
  EXPECT_FALSE(dom_message_queue.PopMessage(&unused));

  std::sort(messages.begin(), messages.end());
  EXPECT_EQ("\"B\"", messages[0]);
  EXPECT_EQ("\"C\"", messages[1]);

  // Make sure the processes are deleted at some point.
  shutdown_B.Wait();
  shutdown_C.Wait();
}

#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER) || \
    !defined(NDEBUG) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
// Too slow under sanitizers and debug builds, even with increased timeout:
// https://crbug.com/1096612
// Disabled for Linux/Android due to failures: https://crbug.com/1494811
#define MAYBE_DetachedIframePagehideHandlerABCB \
  DISABLED_DetachedIframePagehideHandlerABCB
#else
#define MAYBE_DetachedIframePagehideHandlerABCB \
  DetachedIframePagehideHandlerABCB
#endif

// When an iframe is detached, check that pagehide handlers execute in all of
// its child frames. Start from A(B1(C(B2))) and delete B1 from A.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       MAYBE_DetachedIframePagehideHandlerABCB) {
  // This test takes longer to run, because multiple processes are waiting on
  // each other's documents to execute pagehide handler before destroying their
  // documents. https://crbug.com/1311985
  base::test::ScopedRunLoopTimeout increase_timeout(
      FROM_HERE, TestTimeouts::action_max_timeout());

  GURL initial_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c(b)))"));

  // 1) Navigate to a(b(c(b)))
  EXPECT_TRUE(NavigateToURL(shell(), initial_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  RenderFrameHostImplWrapper rfh_a(root->current_frame_host());
  RenderFrameHostImplWrapper rfh_b1(rfh_a->child_at(0)->current_frame_host());
  RenderFrameHostImplWrapper rfh_c(rfh_b1->child_at(0)->current_frame_host());
  RenderFrameHostImplWrapper rfh_b2(rfh_c->child_at(0)->current_frame_host());

  // 2) Add pagehide handlers on B1, B2 and C.
  AddPagehideHandler(rfh_b1->frame_tree_node(), "B1");
  rfh_b1->DisableUnloadTimerForTesting();
  AddPagehideHandler(rfh_b2->frame_tree_node(), "B2");
  rfh_b2->DisableUnloadTimerForTesting();
  AddPagehideHandler(rfh_c->frame_tree_node(), "C");
  rfh_c->DisableUnloadTimerForTesting();

  DOMMessageQueue dom_message_queue(web_contents());
  RenderProcessHostWatcher shutdown_B(
      rfh_b1->GetProcess(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  RenderProcessHostWatcher shutdown_C(
      rfh_c->GetProcess(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  // 3) Detach B from A.
  ExecuteScriptAsync(root, "document.querySelector('iframe').remove();");

  // 4) Wait for pagehide handler.
  std::vector<std::string> messages(3);
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&messages[0]));
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&messages[1]));
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&messages[2]));
  std::string unused;
  EXPECT_FALSE(dom_message_queue.PopMessage(&unused));

  std::sort(messages.begin(), messages.end());
  EXPECT_EQ("\"B1\"", messages[0]);
  EXPECT_EQ("\"B2\"", messages[1]);
  EXPECT_EQ("\"C\"", messages[2]);

  // Make sure the processes are deleted at some point.
  shutdown_B.Wait();
  shutdown_C.Wait();
}

// When an iframe is detached, check that pagehide handlers execute in all of
// its child frames. Start from A1(A2(B)), delete A2 from itself.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       DetachedIframePagehideHandlerAAB) {
  GURL initial_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a(b))"));

  // 1) Navigate to a(a(b)).
  EXPECT_TRUE(NavigateToURL(shell(), initial_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  RenderFrameHostImpl* rfh_a1 = root->current_frame_host();
  RenderFrameHostImpl* rfh_a2 = rfh_a1->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a2->child_at(0)->current_frame_host();

  // 2) Add pagehide handlers on A2 ad B.
  AddPagehideHandler(rfh_a2->frame_tree_node(), "A2");
  AddPagehideHandler(rfh_b->frame_tree_node(), "B");

  DOMMessageQueue dom_message_queue(web_contents());
  RenderProcessHostWatcher shutdown_B(
      rfh_b->GetProcess(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  // 3) A2 detaches itself.
  ExecuteScriptAsync(rfh_a2->frame_tree_node(),
                     "parent.document.querySelector('iframe').remove();");

  // 4) Wait for pagehide handler.
  std::vector<std::string> messages(2);
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&messages[0]));
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&messages[1]));
  std::string unused;
  EXPECT_FALSE(dom_message_queue.PopMessage(&unused));

  std::sort(messages.begin(), messages.end());
  EXPECT_EQ("\"A2\"", messages[0]);
  EXPECT_EQ("\"B\"", messages[1]);

  // Make sure the process is deleted at some point.
  shutdown_B.Wait();
}

// Tests that running layout from an pagehide handler inside teardown of the
// RenderWidget (inside WidgetMsg_Close) can succeed.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       RendererInitiatedWindowCloseWithPagehide) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/empty.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // We will window.open() another URL on the same domain so they share a
  // renderer. This window has an pagehide handler that forces layout to occur.
  // Then we (in a new stack) close that window causing that layout. If all
  // goes well the window closes. If it goes poorly, the renderer may crash.
  //
  // This path is special because the unload results from window.close() which
  // avoids the user-initiated close path through ViewMsg_ClosePage. In that
  // path the pagehide handlers are run early, before the actual teardown of
  // the closing RenderWidget.
  GURL open_url = embedded_test_server()->GetURL(
      "a.com", "/pagehide_handler_force_layout.html");

  // Listen for messages from the window that the test opens, and convert them
  // into the document title, which we can wait on in the main test window.
  EXPECT_TRUE(ExecJs(root,
                     "window.addEventListener('message', function(event) {\n"
                     "  document.title = event.data;\n"
                     "});"));

  // This performs window.open() and waits for the title of the original
  // document to change to signal that the pagehide handler has been registered.
  {
    std::u16string title_when_loaded = u"loaded";
    TitleWatcher title_watcher(shell()->web_contents(), title_when_loaded);
    EXPECT_TRUE(ExecJs(root, JsReplace("var w = window.open($1)", open_url)));
    EXPECT_EQ(title_watcher.WaitAndGetTitle(), title_when_loaded);
  }

  // The closes the window and waits for the title of the original document to
  // change again to signal that the pagehide handler has run.
  {
    std::u16string title_when_done = u"unloaded";
    TitleWatcher title_watcher(shell()->web_contents(), title_when_done);
    EXPECT_TRUE(ExecJs(root, "w.close()"));
    EXPECT_EQ(title_watcher.WaitAndGetTitle(), title_when_done);
  }
}

// Regression test for https://crbug.com/960006.
//
// 1. Navigate to a1(a2(b3),c4),
// 2. b3 has a slow unload handler.
// 3. a2 navigates same process.
// 4. When the new document is loaded, a message is sent to c4 to check it
//    cannot see b3 anymore, even if b3 is still unloading.
IN_PROC_BROWSER_TEST_P(
    SitePerProcessBrowserTest,
    IsDetachedSubframeObservableDuringUnloadHandlerSameProcess) {
  GURL page_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a(b),c)"));
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  RenderFrameHostImpl* node1 =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetPrimaryFrameTree()
          .root()
          ->current_frame_host();
  RenderFrameHostImpl* node2 = node1->child_at(0)->current_frame_host();
  RenderFrameHostImpl* node3 = node2->child_at(0)->current_frame_host();
  RenderFrameHostImpl* node4 = node1->child_at(1)->current_frame_host();
  ASSERT_TRUE(ExecJs(node1, "window.name = 'node1'"));
  ASSERT_TRUE(ExecJs(node2, "window.name = 'node2'"));
  ASSERT_TRUE(ExecJs(node3, "window.name = 'node3'"));
  ASSERT_TRUE(ExecJs(node4, "window.name = 'node4'"));

  ASSERT_TRUE(ExecJs(node1, "window.node2 = window[0]"));
  ASSERT_TRUE(ExecJs(node1, "window.node3 = window[0][0]"));
  ASSERT_TRUE(ExecJs(node1, "window.node4 = window[1]"));

  // Test sanity check.
  EXPECT_EQ(true, EvalJs(node1, "!!window.node2"));
  EXPECT_EQ(true, EvalJs(node1, "!!window.node3"));
  EXPECT_EQ(true, EvalJs(node1, "!!window.node4"));

  // Simulate a long-running unload handler in |node3|.
  node3->DoNotDeleteForTesting();
  node2->DisableUnloadTimerForTesting();
  ASSERT_TRUE(ExecJs(node3, "window.onunload = ()=>{}"));

  // Prepare |node4| to respond to postMessage with a report of whether it can
  // still find |node3|.
  const char* kPostMessageHandlerScript = R"(
      window.postMessageGotData == false;
      window.postMessageCallback = function() {};
      function receiveMessage(event) {
          console.log('node4 - receiveMessage...');

          var can_node3_be_found = false;
          try {
            can_node3_be_found = !!top[0][0];  // top.node2.node3
          } catch(e) {
            can_node3_be_found = false;
          }

          window.postMessageGotData = true;
          window.postMessageData = can_node3_be_found;
          window.postMessageCallback(window.postMessageData);
      }
      window.addEventListener("message", receiveMessage, false);
  )";
  ASSERT_TRUE(ExecJs(node4, kPostMessageHandlerScript));

  // Make |node1| navigate |node2| same process and after the navigation
  // succeeds, send a post message to |node4|. We expect that the effects of the
  // commit should be visible to |node4| by the time it receives the posted
  // message.
  const char* kNavigationScript = R"(
      var node2_frame = document.getElementsByTagName('iframe')[0];
      node2_frame.onload = function() {
          console.log('node2_frame.onload ...');
          window.node4.postMessage('try to find node3', '*');
      };
      node2_frame.src = $1;
  )";
  GURL url = embedded_test_server()->GetURL("a.com", "/title1.html");
  ASSERT_TRUE(ExecJs(node1, JsReplace(kNavigationScript, url)));

  // Check if |node4| has seen |node3| even after |node2| navigation finished
  // (no other frame should see |node3| after the navigation of its parent).
  const char* kPostMessageResultsScript = R"(
      new Promise(function (resolve, reject) {
          if (window.postMessageGotData)
            resolve(window.postMessageData);
          else
            window.postMessageCallback = resolve;
      });
  )";
  EXPECT_EQ(false, EvalJs(node4, kPostMessageResultsScript));
}

// Regression test for https://crbug.com/960006.
//
// 1. Navigate to a1(a2(b3),c4),
// 2. b3 has a slow unload handler.
// 3. a2 navigates cross process.
// 4. When the new document is loaded, a message is sent to c4 to check it
//    cannot see b3 anymore, even if b3 is still unloading.
//
// Note: This test is the same as the above, except it uses a cross-process
// navigation at step 3.
IN_PROC_BROWSER_TEST_P(
    SitePerProcessBrowserTest,
    IsDetachedSubframeObservableDuringUnloadHandlerCrossProcess) {
  GURL page_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a(b),c)"));
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  RenderFrameHostImpl* node1 =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetPrimaryFrameTree()
          .root()
          ->current_frame_host();
  RenderFrameHostImpl* node2 = node1->child_at(0)->current_frame_host();
  RenderFrameHostImpl* node3 = node2->child_at(0)->current_frame_host();
  RenderFrameHostImpl* node4 = node1->child_at(1)->current_frame_host();
  ASSERT_TRUE(ExecJs(node1, "window.name = 'node1'"));
  ASSERT_TRUE(ExecJs(node2, "window.name = 'node2'"));
  ASSERT_TRUE(ExecJs(node3, "window.name = 'node3'"));
  ASSERT_TRUE(ExecJs(node4, "window.name = 'node4'"));

  ASSERT_TRUE(ExecJs(node1, "window.node2 = window[0]"));
  ASSERT_TRUE(ExecJs(node1, "window.node3 = window[0][0]"));
  ASSERT_TRUE(ExecJs(node1, "window.node4 = window[1]"));

  // Test sanity check.
  EXPECT_EQ(true, EvalJs(node1, "!!window.node2"));
  EXPECT_EQ(true, EvalJs(node1, "!!window.node3"));
  EXPECT_EQ(true, EvalJs(node1, "!!window.node4"));

  // Add a long-running unload handler to |node3|.
  node3->DoNotDeleteForTesting();
  node2->DisableUnloadTimerForTesting();
  ASSERT_TRUE(ExecJs(node3, "window.onunload = ()=>{}"));

  // Prepare |node4| to respond to postMessage with a report of whether it can
  // still find |node3|.
  const char* kPostMessageHandlerScript = R"(
      window.postMessageGotData == false;
      window.postMessageCallback = function() {};
      function receiveMessage(event) {
          console.log('node4 - receiveMessage...');

          var can_node3_be_found = false;
          try {
            can_node3_be_found = !!top[0][0];  // top.node2.node3
          } catch(e) {
            can_node3_be_found = false;
          }

          window.postMessageGotData = true;
          window.postMessageData = can_node3_be_found;
          window.postMessageCallback(window.postMessageData);
      }
      window.addEventListener("message", receiveMessage, false);
  )";
  ASSERT_TRUE(ExecJs(node4, kPostMessageHandlerScript));

  // Make |node1| navigate |node2| cross process and after the navigation
  // succeeds, send a post message to |node4|. We expect that the effects of the
  // commit should be visible to |node4| by the time it receives the posted
  // message.
  const char* kNavigationScript = R"(
      var node2_frame = document.getElementsByTagName('iframe')[0];
      node2_frame.onload = function() {
          console.log('node2_frame.onload ...');
          window.node4.postMessage('try to find node3', '*');
      };
      node2_frame.src = $1;
  )";
  GURL url = embedded_test_server()->GetURL("d.com", "/title1.html");
  ASSERT_TRUE(ExecJs(node1, JsReplace(kNavigationScript, url)));

  // Check if |node4| has seen |node3| even after |node2| navigation finished
  // (no other frame should see |node3| after the navigation of its parent).
  const char* kPostMessageResultsScript = R"(
      new Promise(function (resolve, reject) {
          if (window.postMessageGotData)
            resolve(window.postMessageData);
          else
            window.postMessageCallback = resolve;
      });
  )";
  EXPECT_EQ(false, EvalJs(node4, kPostMessageResultsScript));
}

// Regression test. https://crbug.com/963330
// 1. Start from A1(B2,C3)
// 2. B2 is the "focused frame", is deleted and starts unloading.
// 3. C3 commits a new navigation before B2 has completed its unload.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, FocusedFrameUnload) {
  // 1) Start from A1(B2,C3)
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(b,c)")));
  RenderFrameHostImpl* A1 = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* B2 = A1->child_at(0)->current_frame_host();
  RenderFrameHostImpl* C3 = A1->child_at(1)->current_frame_host();
  FrameTree* frame_tree = A1->frame_tree();

  // 2.1) Make B2 to be the focused frame.
  EXPECT_EQ(A1->frame_tree_node(), frame_tree->GetFocusedFrame());
  EXPECT_TRUE(ExecJs(A1, "document.querySelector('iframe').focus()"));
  EXPECT_EQ(B2->frame_tree_node(), frame_tree->GetFocusedFrame());

  // 2.2 Unload B2. Drop detach message to simulate a long unloading.
  B2->SetSubframeUnloadTimeoutForTesting(base::Seconds(30));

  EXPECT_FALSE(B2->GetSuddenTerminationDisablerState(
      blink::mojom::SuddenTerminationDisablerType::kUnloadHandler));
  B2->DoNotDeleteForTesting();
  EXPECT_TRUE(ExecJs(B2, "window.onunload = ()=>{};"));
  EXPECT_TRUE(B2->GetSuddenTerminationDisablerState(
      blink::mojom::SuddenTerminationDisablerType::kUnloadHandler));

  EXPECT_TRUE(B2->IsActive());
  EXPECT_TRUE(ExecJs(A1, "document.querySelector('iframe').remove()"));
  EXPECT_EQ(nullptr, frame_tree->GetFocusedFrame());
  EXPECT_EQ(2u, A1->child_count());
  EXPECT_TRUE(B2->IsPendingDeletion());

  // 3. C3 navigates.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      C3->frame_tree_node(),
      embedded_test_server()->GetURL("d.com", "/title1.html")));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(2u, A1->child_count());
}

// Test the unload timeout is effective.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, UnloadTimeout) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImpl* A1 = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* B2 = A1->child_at(0)->current_frame_host();

  // Simulate the iframe being slow to unload by dropping the
  // mojom::FrameHost::Detach API sent from B2 to the browser.
  EXPECT_TRUE(ExecJs(B2, "window.onunload = ()=>{};"));
  B2->DoNotDeleteForTesting();

  RenderFrameDeletedObserver delete_B2(B2);
  EXPECT_TRUE(ExecJs(A1, "document.querySelector('iframe').remove()"));
  delete_B2.WaitUntilDeleted();
}

// Test that an unloading child can PostMessage its cross-process parent.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       UnloadPostMessageToParentCrossProcess) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImpl* A1 = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* B2 = A1->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver delete_B2(B2);
  EXPECT_TRUE(ExecJs(B2, R"(
    window.addEventListener("unload", function() {
      window.parent.postMessage("B2 message", "*");
    });
  )"));
  EXPECT_TRUE(ExecJs(A1, R"(
    window.received_message = "nothing received";
    var received = false;
    window.addEventListener('message', function(event) {
      received_message = event.data;
    });
    document.querySelector('iframe').remove();
  )"));
  delete_B2.WaitUntilDeleted();
  // TODO(crbug.com/41459857): PostMessage called from an unloading frame
  // must work. A1 must received 'B2 message'. This is not the case here.
  EXPECT_EQ("nothing received", EvalJs(A1, "received_message"));
}

// Test that an unloading child can PostMessage its same-process parent.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       UnloadPostMessageToParentSameProcess) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImpl* A1 = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* A2 = A1->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver delete_A1(A2);
  EXPECT_TRUE(ExecJs(A2, R"(
    window.addEventListener("pagehide", function() {
      window.parent.postMessage("A2 message", "*");
    });
  )"));
  EXPECT_TRUE(ExecJs(A1, R"(
    window.received_message = "nothing received";
    var received = false;
    window.addEventListener('message', function(event) {
      received_message = event.data;
    });
    document.querySelector('iframe').remove();
  )"));
  delete_A1.WaitUntilDeleted();
  EXPECT_EQ("A2 message", EvalJs(A1, "received_message"));
}

// Related to issue https://crbug.com/950625.
//
// 1. Start from A1(B1)
// 2. Navigate A1 to A3, same-process.
// 3. A1 requests the browser to detach B1, but this message is dropped.
// 4. The browser must be resilient and detach B1 when A3 commits.
// TODO(crbug.com/40914915): Fix flakes and re-enable test.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       DISABLED_SameProcessNavigationResilientToDetachDropped) {
  // The test assumes the previous page gets deleted after navigation. Disable
  // back-forward cache to ensure that it doesn't get preserved in the cache.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);
  GURL A1_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL A3_url(embedded_test_server()->GetURL("a.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), A1_url));
  RenderFrameHostImpl* A1 = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* B1 = A1->child_at(0)->current_frame_host();

  B1->DoNotDeleteForTesting();
  RenderFrameDeletedObserver delete_B1(B1);
  shell()->LoadURL(A3_url);
  delete_B1.WaitUntilDeleted();
}

#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
// See crbug.com/1275848.
#define MAYBE_NestedSubframeWithPagehideHandler \
  DISABLED_NestedSubframeWithPagehideHandler
#else
#define MAYBE_NestedSubframeWithPagehideHandler \
  NestedSubframeWithPagehideHandler
#endif
// After a same-origin iframe navigation, check that grandchild iframe are
// properly deleted and their pagehide handler executed.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       MAYBE_NestedSubframeWithPagehideHandler) {
  GURL main_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(b,c))");
  GURL iframe_new_url = embedded_test_server()->GetURL("b.com", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // In the document tree: A1(B2(B3,C4)) navigate B2 to B5.
  RenderFrameHostImpl* A1 = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* B2 = A1->child_at(0)->current_frame_host();
  RenderFrameHostImpl* B3 = B2->child_at(0)->current_frame_host();
  RenderFrameHostImpl* C4 = B2->child_at(1)->current_frame_host();

  RenderFrameDeletedObserver delete_B2(B2);
  RenderFrameDeletedObserver delete_B3(B3);
  RenderFrameDeletedObserver delete_C4(C4);

  AddPagehideHandler(B2, "B2");
  AddPagehideHandler(B3, "B3");
  AddPagehideHandler(C4, "C4");

  // Navigate the iframe same-process.
  bool will_delete_b2 = B2->ShouldChangeRenderFrameHostOnSameSiteNavigation();
  ExecuteScriptAsync(B2, JsReplace("location.href = $1", iframe_new_url));

  DOMMessageQueue dom_message_queue(
      WebContents::FromRenderFrameHost(web_contents()->GetPrimaryMainFrame()));

  // All the documents must be properly deleted:
  if (will_delete_b2) {
    delete_B2.WaitUntilDeleted();
  }
  delete_B3.WaitUntilDeleted();
  delete_C4.WaitUntilDeleted();

  // The pagehide handlers must have run:
  std::string message;
  std::vector<std::string> messages;
  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
    base::TrimString(message, "\"", &message);
    messages.push_back(message);
  }
  EXPECT_FALSE(dom_message_queue.PopMessage(&message));
  EXPECT_THAT(messages, WhenSorted(ElementsAre("B2", "B3", "C4")));
}

// Some tests need an https server because third-party cookies are used, and
// SameSite=None cookies must be Secure. This is a separate fixture due to
// use the ContentMockCertVerifier.
class SitePerProcessSSLBrowserTest : public SitePerProcessBrowserTest {
 protected:
  SitePerProcessSSLBrowserTest() = default;
  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  void SetUpOnMainThread() override {
    SitePerProcessBrowserTest::SetUpOnMainThread();
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    https_server()->AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(https_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    SitePerProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    SitePerProcessBrowserTest::TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  content::ContentMockCertVerifier mock_cert_verifier_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

// Pagehide handlers should be able to do things that might require for instance
// the RenderFrameHostImpl to stay alive.
// - use console.log (handled via RFHI::DidAddMessageToConsole).
// - use history.replaceState (handled via RFHI::OnUpdateState).
// - use document.cookie
// - use localStorage
//
// Test case:
//  1. Start on A1(B2). B2 has a pagehide handler.
//  2. Go to A3.
//  3. Go back to A4(B5).
//
// TODO(crbug.com/41457585): history.replaceState is broken in OOPIFs.
//
// This test is similar to PagehideHandlersArePowerfulGrandChild, but with a
// different frame hierarchy.
//
// TODO(crbug.com/40283595): investigate test flakes and re-enable test.
IN_PROC_BROWSER_TEST_P(SitePerProcessSSLBrowserTest,
                       DISABLED_PagehideHandlersArePowerful) {
  // The test expects the previous document to be deleted on navigation.
  DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  // Navigate to a page hosting a cross-origin frame.
  GURL url =
      https_server()->GetURL("a.com", "/cross_site_iframe_factory.html?a(b)");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* A1 = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* B2 = A1->child_at(0)->current_frame_host();

  // Increase Unload timeout to prevent the previous document from
  // being deleted before it has finished running B2 pagehide handler.
  A1->DisableUnloadTimerForTesting();
  B2->SetSubframeUnloadTimeoutForTesting(base::Seconds(30));

  // Add a pagehide handler to the subframe and try in that handler to preserve
  // state that we will try to recover later.
  ASSERT_TRUE(ExecJs(B2, R"(
    window.addEventListener("pagehide", function() {
      // Waiting for 100ms, to give more time for browser-side things to go bad
      // and delete RenderFrameHostImpl prematurely.
      var start = (new Date()).getTime();
      do {
        curr = (new Date()).getTime();
      } while (start + 100 > curr);

      // Test that various RFHI-dependent things work fine in an unload handler.
      stateObj = { "history_test_key": "history_test_value" }
      history.replaceState(stateObj, 'title', window.location.href);
      console.log('console.log() sent');

      // As a sanity check, test that RFHI-independent things also work fine.
      localStorage.localstorage_test_key = 'localstorage_test_value';
      document.cookie = 'cookie_test_key=' +
                        'cookie_test_value; SameSite=none; Secure';
    });
  )"));

  // Navigate A1(B2) to A3.
  {
    // Prepare observers.
    WebContentsConsoleObserver console_observer(web_contents());
    console_observer.SetPattern("console.log() sent");
    RenderFrameDeletedObserver B2_deleted(B2);

    // Navigate
    GURL away_url(https_server()->GetURL("a.com", "/title1.html"));
    ASSERT_TRUE(ExecJs(A1, JsReplace("location = $1", away_url)));

    // Observers must be reached.
    B2_deleted.WaitUntilDeleted();
    ASSERT_TRUE(console_observer.Wait());

    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
    EXPECT_EQ(away_url, web_contents()->GetLastCommittedURL());
  }

  // Navigate back from A3 to A4(B5).
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Temporary extra expectations to investigate:
  // https://bugs.chromium.org/p/chromium/issues/detail?id=1215493
  EXPECT_EQ(url, web_contents()->GetLastCommittedURL());
  EXPECT_EQ(
      2u, CollectAllRenderFrameHosts(web_contents()->GetPrimaryPage()).size());

  RenderFrameHostImpl* A4 = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* B5 = A4->child_at(0)->current_frame_host();

  // Verify that we can recover the data that should have been persisted by the
  // pagehide handler.
  EXPECT_EQ("localstorage_test_value",
            EvalJs(B5, "localStorage.localstorage_test_key"));
  EXPECT_EQ("cookie_test_key=cookie_test_value", EvalJs(B5, "document.cookie"));

  // TODO(lukasza): https://crbug.com/960976: Make the verification below
  // unconditional, once the bug is fixed.
  if (!AreAllSitesIsolatedForTesting()) {
    EXPECT_EQ("history_test_value",
              EvalJs(B5, "history.state.history_test_key"));
  }
}

// Pagehide handlers should be able to do things that might require for instance
// the RenderFrameHostImpl to stay alive.
// - use console.log (handled via RFHI::DidAddMessageToConsole).
// - use history.replaceState (handled via RFHI::OnUpdateState).
// - use document.cookie
// - use localStorage
//
// Test case:
//  1. Start on A1(B2(C3)). C3 has an unload handler.
//  2. Go to A4.
//  3. Go back to A5(B6(C7)).
//
// TODO(crbug.com/41457585): history.replaceState is broken in OOPIFs.
//
// This test is similar to PagehideHandlersArePowerful, but with a different
// frame hierarchy.
//
// TODO(crbug.com/40283595): investigate test flakes and re-enable test.
IN_PROC_BROWSER_TEST_P(SitePerProcessSSLBrowserTest,
                       DISABLED_PagehideHandlersArePowerfulGrandChild) {
  // The test expects the previous document to be deleted on navigation.
  DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  // Navigate to a page hosting a cross-origin frame.
  GURL url = https_server()->GetURL("a.com",
                                    "/cross_site_iframe_factory.html?a(b(c))");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* A1 = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* B2 = A1->child_at(0)->current_frame_host();
  RenderFrameHostImpl* C3 = B2->child_at(0)->current_frame_host();

  // Increase Unload timeout to prevent the previous document from
  // being deleleted before it has finished running C3 unload handler.
  A1->DisableUnloadTimerForTesting();
  B2->SetSubframeUnloadTimeoutForTesting(base::Seconds(30));
  C3->SetSubframeUnloadTimeoutForTesting(base::Seconds(30));

  // Add a pagehide handler to the subframe and try in that handler to preserve
  // state that we will try to recover later.
  ASSERT_TRUE(ExecJs(C3, R"(
    window.addEventListener("pagehide", function() {
      // Waiting for 100ms, to give more time for browser-side things to go bad
      // and delete RenderFrameHostImpl prematurely.
      var start = (new Date()).getTime();
      do {
        curr = (new Date()).getTime();
      } while (start + 100 > curr);

      // Test that various RFHI-dependent things work fine in an unload handler.
      stateObj = { "history_test_key": "history_test_value" }
      history.replaceState(stateObj, 'title', window.location.href);
      console.log('console.log() sent');

      // As a sanity check, test that RFHI-independent things also work fine.
      localStorage.localstorage_test_key = 'localstorage_test_value';
      document.cookie = 'cookie_test_key=' +
                        'cookie_test_value; SameSite=none; Secure';
    });
  )"));

  // Navigate A1(B2(C3) to A4.
  {
    // Prepare observers.
    WebContentsConsoleObserver console_observer(web_contents());
    console_observer.SetPattern("console.log() sent");
    RenderFrameDeletedObserver B2_deleted(B2);
    RenderFrameDeletedObserver C3_deleted(C3);

    // Navigate
    GURL away_url(https_server()->GetURL("a.com", "/title1.html"));
    ASSERT_TRUE(ExecJs(A1, JsReplace("location = $1", away_url)));

    // Observers must be reached.
    B2_deleted.WaitUntilDeleted();
    C3_deleted.WaitUntilDeleted();
    ASSERT_TRUE(console_observer.Wait());

    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
    EXPECT_EQ(away_url, web_contents()->GetLastCommittedURL());
  }

  // Navigate back from A4 to A5(B6(C7))
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Temporary extra expectations to investigate:
  // https://bugs.chromium.org/p/chromium/issues/detail?id=1215493
  EXPECT_EQ(url, web_contents()->GetLastCommittedURL());
  EXPECT_EQ(
      3u, CollectAllRenderFrameHosts(web_contents()->GetPrimaryPage()).size());

  RenderFrameHostImpl* A5 = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* B6 = A5->child_at(0)->current_frame_host();
  RenderFrameHostImpl* C7 = B6->child_at(0)->current_frame_host();

  // Verify that we can recover the data that should have been persisted by the
  // pagehide handler.
  EXPECT_EQ("localstorage_test_value",
            EvalJs(C7, "localStorage.localstorage_test_key"));
  EXPECT_EQ("cookie_test_key=cookie_test_value", EvalJs(C7, "document.cookie"));

  // TODO(lukasza): https://crbug.com/960976: Make the verification below
  // unconditional, once the bug is fixed.
  if (!AreAllSitesIsolatedForTesting()) {
    EXPECT_EQ("history_test_value",
              EvalJs(C7, "history.state.history_test_key"));
  }
}

// Execute a pagehide handler from the initial empty document.
//
// Start from A1(B2(B3)).
// B3 is the initial empty document created by B2. A pagehide handler is added
// to B3. A1 deletes B2.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       UnloadInInitialEmptyDocument) {
  // 1. Start from A1(B2).
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  RenderFrameHostImpl* a1 = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* b2 = a1->child_at(0)->current_frame_host();

  // 2. Create a new frame without navigating it. It stays on the initial empty
  //    document B3. Current state is with A1(B2(B3)).
  ASSERT_EQ(0u, b2->child_count());
  EXPECT_TRUE(ExecJs(b2, R"(
    let iframe = document.createElement("iframe");
    document.body.appendChild(iframe);
    iframe.contentWindow.onpagehide = () => {
      window.domAutomationController.send("B3 unloaded");
    }
  )"));
  ASSERT_EQ(1u, b2->child_count());
  RenderFrameHostImpl* b3 = b2->child_at(0)->current_frame_host();

  auto has_pagehide_handler = [](RenderFrameHostImpl* rfh) {
    return rfh->GetSuddenTerminationDisablerState(
        blink::mojom::SuddenTerminationDisablerType::kPageHideHandler);
  };
  EXPECT_FALSE(has_pagehide_handler(a1));
  EXPECT_FALSE(has_pagehide_handler(b2));
  EXPECT_TRUE(has_pagehide_handler(b3));

  // 3. A1 deletes B2. This triggers the pagehide handler from B3.
  DOMMessageQueue dom_message_queue(
      WebContents::FromRenderFrameHost(web_contents()->GetPrimaryMainFrame()));
  ExecuteScriptAsync(a1, "document.querySelector('iframe').remove();");

  // Check the pagehide handler is executed.
  std::string message;
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"B3 unloaded\"", message);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessSSLBrowserTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));

// This test sets up a main frame which has an OOPIF. The main frame commits a
// same-site navigation. The test then stops at the stage where the unload
// handler of the OOPIF is running and the main frame RenderFrameHost's
// `DocumentAssociatedData` is retrieved from the OOPIF. The test shows that
// the `DocumentAssociatedData` is different from the one before navigation if
// RenderDocument feature is not enabled for all frames. One place we have seen
// this issue is in Protected Audience auctions. Please see crbug.com/1422301.
IN_PROC_BROWSER_TEST_P(
    SitePerProcessBrowserTest,
    MainFrameDocumentAssociatedDataChangesOnSameSiteNavigation) {
  GURL initial_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL next_url(embedded_test_server()->GetURL("login.a.com", "/title1.html"));

  // 1) Navigate on a page with an OOPIF.
  EXPECT_TRUE(NavigateToURL(shell(), initial_url));

  FrameTreeNode* root_ftn = web_contents()->GetPrimaryFrameTree().root();
  RenderFrameHostImpl* main_rfh = root_ftn->current_frame_host();

  // 2) Act as if there was an infinite unload handler in the OOPIF.
  RenderFrameHostImpl* child_rfh = root_ftn->child_at(0)->current_frame_host();

  child_rfh->DoNotDeleteForTesting();

  // Set an arbitrarily long timeout to ensure the subframe unload timer doesn't
  // fire before we call OnDetach().
  child_rfh->SetSubframeUnloadTimeoutForTesting(base::Seconds(30));

  // With BackForwardCache, old document doesn't fire unload handlers as the
  // page is stored in BackForwardCache on navigation.
  DisableBackForwardCacheForTesting(web_contents(),
                                    BackForwardCache::TEST_USES_UNLOAD_EVENT);

  // 3) Retrieve the weak pointer to the owned page by the main
  // RenderFrameHost's `DocumentAssociatedData`.
  base::WeakPtr<PageImpl> weak_ptr_page = child_rfh->GetPage().GetWeakPtrImpl();

  // 4) Navigate the main frame to a same-site url. The unload handler of the
  // OOPIF is running.
  EXPECT_TRUE(NavigateToURL(shell(), next_url));
  EXPECT_TRUE(child_rfh->IsPendingDeletion());

  // 5) If RenderDocument feature is not enabled for all frames, the main frame
  // RenderFrameHost will be the same.
  EXPECT_EQ(
      main_rfh ==
          web_contents()->GetPrimaryFrameTree().root()->current_frame_host(),
      GetRenderDocumentLevel() < RenderDocumentLevel::kAllFrames);

  // 6) If RenderDocument feature is not enabled for all frames, verify
  // `PageImpl` has changed by checking the weak pointer.
  EXPECT_EQ(weak_ptr_page == nullptr,
            GetRenderDocumentLevel() < RenderDocumentLevel::kAllFrames);
}

}  // namespace content
