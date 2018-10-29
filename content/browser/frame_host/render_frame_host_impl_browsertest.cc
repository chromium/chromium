// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_host_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/interface_provider_filtering.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/did_commit_provisional_load_interceptor.h"
#include "content/test/frame_host_test_interface.mojom.h"
#include "content/test/test_content_browser_client.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom.h"

namespace content {

namespace {

// Implementation of ContentBrowserClient that overrides
// OverridePageVisibilityState() and allows consumers to set a value.
class PrerenderTestContentBrowserClient : public TestContentBrowserClient {
 public:
  PrerenderTestContentBrowserClient()
      : override_enabled_(false),
        visibility_override_(blink::mojom::PageVisibilityState::kVisible) {}
  ~PrerenderTestContentBrowserClient() override {}

  void EnableVisibilityOverride(
      blink::mojom::PageVisibilityState visibility_override) {
    override_enabled_ = true;
    visibility_override_ = visibility_override;
  }

  void OverridePageVisibilityState(
      RenderFrameHost* render_frame_host,
      blink::mojom::PageVisibilityState* visibility_state) override {
    if (override_enabled_)
      *visibility_state = visibility_override_;
  }

 private:
  bool override_enabled_;
  blink::mojom::PageVisibilityState visibility_override_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderTestContentBrowserClient);
};
}  // anonymous namespace

// TODO(mlamouri): part of these tests were removed because they were dependent
// on an environment were focus is guaranteed. This is only for
// interactive_ui_tests so these bits need to move there.
// See https://crbug.com/491535
class RenderFrameHostImplBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Test that when creating a new window, the main frame is correctly focused.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       IsFocused_AtLoad) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetTestUrl("render_frame_host", "focus.html")));

  // The main frame should be focused.
  WebContents* web_contents = shell()->web_contents();
  EXPECT_EQ(web_contents->GetMainFrame(), web_contents->GetFocusedFrame());
}

// Test that if the content changes the focused frame, it is correctly exposed.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       IsFocused_Change) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetTestUrl("render_frame_host", "focus.html")));

  WebContents* web_contents = shell()->web_contents();

  std::string frames[2] = { "frame1", "frame2" };
  for (const std::string& frame : frames) {
    ExecuteScriptAndGetValue(
        web_contents->GetMainFrame(), "focus" + frame + "()");

    // The main frame is not the focused frame in the frame tree but the main
    // frame is focused per RFHI rules because one of its descendant is focused.
    // TODO(mlamouri): we should check the frame focus state per RFHI, see the
    // general comment at the beginning of this test file.
    EXPECT_NE(web_contents->GetMainFrame(), web_contents->GetFocusedFrame());
    EXPECT_EQ(frame, web_contents->GetFocusedFrame()->GetFrameName());
  }
}

// Tests focus behavior when the focused frame is removed from the frame tree.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, RemoveFocusedFrame) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetTestUrl("render_frame_host", "focus.html")));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  ExecuteScriptAndGetValue(web_contents->GetMainFrame(), "focusframe4()");

  EXPECT_NE(web_contents->GetMainFrame(), web_contents->GetFocusedFrame());
  EXPECT_EQ("frame4", web_contents->GetFocusedFrame()->GetFrameName());
  EXPECT_EQ("frame3",
            web_contents->GetFocusedFrame()->GetParent()->GetFrameName());
  EXPECT_NE(-1, web_contents->GetFrameTree()->focused_frame_tree_node_id_);

  ExecuteScriptAndGetValue(web_contents->GetMainFrame(), "detachframe(3)");
  EXPECT_EQ(nullptr, web_contents->GetFocusedFrame());
  EXPECT_EQ(-1, web_contents->GetFrameTree()->focused_frame_tree_node_id_);

  ExecuteScriptAndGetValue(web_contents->GetMainFrame(), "focusframe2()");
  EXPECT_NE(nullptr, web_contents->GetFocusedFrame());
  EXPECT_NE(web_contents->GetMainFrame(), web_contents->GetFocusedFrame());
  EXPECT_NE(-1, web_contents->GetFrameTree()->focused_frame_tree_node_id_);

  ExecuteScriptAndGetValue(web_contents->GetMainFrame(), "detachframe(2)");
  EXPECT_EQ(nullptr, web_contents->GetFocusedFrame());
  EXPECT_EQ(-1, web_contents->GetFrameTree()->focused_frame_tree_node_id_);
}

// Test that a frame is visible/hidden depending on its WebContents visibility
// state.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       GetVisibilityState_Basic) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,foo")));
  WebContents* web_contents = shell()->web_contents();

  web_contents->WasShown();
  EXPECT_EQ(blink::mojom::PageVisibilityState::kVisible,
            web_contents->GetMainFrame()->GetVisibilityState());

  web_contents->WasHidden();
  EXPECT_EQ(blink::mojom::PageVisibilityState::kHidden,
            web_contents->GetMainFrame()->GetVisibilityState());
}

// Test that a frame visibility can be overridden by the ContentBrowserClient.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       GetVisibilityState_Override) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,foo")));
  WebContents* web_contents = shell()->web_contents();

  PrerenderTestContentBrowserClient new_client;
  ContentBrowserClient* old_client = SetBrowserClientForTesting(&new_client);

  web_contents->WasShown();
  EXPECT_EQ(blink::mojom::PageVisibilityState::kVisible,
            web_contents->GetMainFrame()->GetVisibilityState());

  new_client.EnableVisibilityOverride(
      blink::mojom::PageVisibilityState::kPrerender);
  EXPECT_EQ(blink::mojom::PageVisibilityState::kPrerender,
            web_contents->GetMainFrame()->GetVisibilityState());

  SetBrowserClientForTesting(old_client);
}

namespace {

class TestJavaScriptDialogManager : public JavaScriptDialogManager,
                                    public WebContentsDelegate {
 public:
  TestJavaScriptDialogManager()
      : message_loop_runner_(new MessageLoopRunner), url_invalidate_count_(0) {}
  ~TestJavaScriptDialogManager() override {}

  // This waits until either WCD::BeforeUnloadFired is called (the unload has
  // been handled) or JSDM::RunJavaScriptDialog/RunBeforeUnloadDialog is called
  // (a request to display a dialog has been received).
  void Wait() {
    message_loop_runner_->Run();
    message_loop_runner_ = new MessageLoopRunner;
  }

  // Runs the dialog callback.
  void Run(bool success, const base::string16& user_input) {
    std::move(callback_).Run(success, user_input);
  }

  int num_beforeunload_dialogs_seen() { return num_beforeunload_dialogs_seen_; }
  int num_beforeunload_fired_seen() { return num_beforeunload_fired_seen_; }
  bool proceed() { return proceed_; }

  // WebContentsDelegate

  JavaScriptDialogManager* GetJavaScriptDialogManager(
      WebContents* source) override {
    return this;
  }

  void BeforeUnloadFired(WebContents* tab,
                         bool proceed,
                         bool* proceed_to_fire_unload) override {
    ++num_beforeunload_fired_seen_;
    proceed_ = proceed;
    message_loop_runner_->Quit();
  }

  // JavaScriptDialogManager

  void RunJavaScriptDialog(WebContents* web_contents,
                           RenderFrameHost* render_frame_host,
                           JavaScriptDialogType dialog_type,
                           const base::string16& message_text,
                           const base::string16& default_prompt_text,
                           DialogClosedCallback callback,
                           bool* did_suppress_message) override {
    callback_ = std::move(callback);
    message_loop_runner_->Quit();
  }

  void RunBeforeUnloadDialog(WebContents* web_contents,
                             RenderFrameHost* render_frame_host,
                             bool is_reload,
                             DialogClosedCallback callback) override {
    ++num_beforeunload_dialogs_seen_;
    callback_ = std::move(callback);
    message_loop_runner_->Quit();
  }

  bool HandleJavaScriptDialog(WebContents* web_contents,
                              bool accept,
                              const base::string16* prompt_override) override {
    return true;
  }

  void CancelDialogs(WebContents* web_contents, bool reset_state) override {}

  // Keep track of whether the tab has notified us of a navigation state change
  // which invalidates the displayed URL.
  void NavigationStateChanged(WebContents* source,
                              InvalidateTypes changed_flags) override {
    if (changed_flags & INVALIDATE_TYPE_URL)
      url_invalidate_count_++;
  }

  int url_invalidate_count() { return url_invalidate_count_; }
  void reset_url_invalidate_count() { url_invalidate_count_ = 0; }

 private:
  DialogClosedCallback callback_;

  // The MessageLoopRunner used to spin the message loop.
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  // The number of times NavigationStateChanged has been called.
  int url_invalidate_count_;

  // The total number of beforeunload dialogs seen by this dialog manager.
  int num_beforeunload_dialogs_seen_ = 0;

  // The total number of BeforeUnloadFired events witnessed by the
  // WebContentsDelegate.
  int num_beforeunload_fired_seen_ = 0;

  // The |proceed| value returned by the last unload event.
  bool proceed_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestJavaScriptDialogManager);
};

class DropBeforeUnloadACKFilter : public BrowserMessageFilter {
 public:
  DropBeforeUnloadACKFilter() : BrowserMessageFilter(FrameMsgStart) {}

 protected:
  ~DropBeforeUnloadACKFilter() override {}

 private:
  // BrowserMessageFilter:
  bool OnMessageReceived(const IPC::Message& message) override {
    return message.type() == FrameHostMsg_BeforeUnload_ACK::ID;
  }

  DISALLOW_COPY_AND_ASSIGN(DropBeforeUnloadACKFilter);
};

mojo::ScopedMessagePipeHandle CreateDisconnectedMessagePipeHandle() {
  mojo::MessagePipe pipe;
  return std::move(pipe.handle0);
}

}  // namespace

// Tests that a beforeunload dialog in an iframe doesn't stop the beforeunload
// timer of a parent frame.
// TODO(avi): flaky on Linux TSAN: http://crbug.com/795326
#if defined(OS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_IframeBeforeUnloadParentHang DISABLED_IframeBeforeUnloadParentHang
#else
#define MAYBE_IframeBeforeUnloadParentHang IframeBeforeUnloadParentHang
#endif
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       MAYBE_IframeBeforeUnloadParentHang) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestJavaScriptDialogManager dialog_manager;
  wc->SetDelegate(&dialog_manager);

  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  // Make an iframe with a beforeunload handler.
  std::string script =
      "var iframe = document.createElement('iframe');"
      "document.body.appendChild(iframe);"
      "iframe.contentWindow.onbeforeunload=function(e){return 'x'};";
  EXPECT_TRUE(content::ExecuteScript(wc, script));
  EXPECT_TRUE(WaitForLoadStop(wc));
  // JavaScript onbeforeunload dialogs require a user gesture.
  for (auto* frame : wc->GetAllFrames())
    frame->ExecuteJavaScriptWithUserGestureForTests(base::string16());

  // Force a process switch by going to a privileged page. The beforeunload
  // timer will be started on the top-level frame but will be paused while the
  // beforeunload dialog is shown by the subframe.
  GURL web_ui_page(std::string(kChromeUIScheme) + "://" +
                   std::string(kChromeUIGpuHost));
  shell()->LoadURL(web_ui_page);
  dialog_manager.Wait();

  RenderFrameHostImpl* main_frame =
      static_cast<RenderFrameHostImpl*>(wc->GetMainFrame());
  EXPECT_TRUE(main_frame->is_waiting_for_beforeunload_ack());

  // Set up a filter to make sure that when the dialog is answered below and the
  // renderer sends the beforeunload ACK, it gets... ahem... lost.
  scoped_refptr<DropBeforeUnloadACKFilter> filter =
      new DropBeforeUnloadACKFilter();
  main_frame->GetProcess()->AddFilter(filter.get());

  // Answer the dialog.
  dialog_manager.Run(true, base::string16());

  // There will be no beforeunload ACK, so if the beforeunload ACK timer isn't
  // functioning then the navigation will hang forever and this test will time
  // out. If this waiting for the load stop works, this test won't time out.
  EXPECT_TRUE(WaitForLoadStop(wc));
  EXPECT_EQ(web_ui_page, wc->GetLastCommittedURL());

  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
}

// Tests that a gesture is required in a frame before it can request a
// beforeunload dialog.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       BeforeUnloadDialogRequiresGesture) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestJavaScriptDialogManager dialog_manager;
  wc->SetDelegate(&dialog_manager);

  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "beforeunload.html")));
  // Disable the hang monitor, otherwise there will be a race between the
  // beforeunload dialog and the beforeunload hang timer.
  wc->GetMainFrame()->DisableBeforeUnloadHangMonitorForTesting();

  // Reload. There should be no beforeunload dialog because there was no gesture
  // on the page. If there was, this WaitForLoadStop call will hang.
  wc->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(wc));

  // Give the page a user gesture and try reloading again. This time there
  // should be a dialog. If there is no dialog, the call to Wait will hang.
  wc->GetMainFrame()->ExecuteJavaScriptWithUserGestureForTests(
      base::string16());
  wc->GetController().Reload(ReloadType::NORMAL, false);
  dialog_manager.Wait();

  // Answer the dialog.
  dialog_manager.Run(true, base::string16());
  EXPECT_TRUE(WaitForLoadStop(wc));

  // The reload should have cleared the user gesture bit, so upon leaving again
  // there should be no beforeunload dialog.
  shell()->LoadURL(GURL("about:blank"));
  EXPECT_TRUE(WaitForLoadStop(wc));

  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
}

// Test for crbug.com/80401.  Canceling a beforeunload dialog should reset
// the URL to the previous page's URL.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       CancelBeforeUnloadResetsURL) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestJavaScriptDialogManager dialog_manager;
  wc->SetDelegate(&dialog_manager);

  GURL url(GetTestUrl("render_frame_host", "beforeunload.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  PrepContentsForBeforeUnloadTest(wc);

  // Navigate to a page that triggers a cross-site transition.
  GURL url2(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  shell()->LoadURL(url2);
  dialog_manager.Wait();

  // Cancel the dialog.
  dialog_manager.reset_url_invalidate_count();
  dialog_manager.Run(false, base::string16());
  EXPECT_FALSE(wc->IsLoading());

  // Verify there are no pending history items after the dialog is cancelled.
  // (see crbug.com/93858)
  NavigationEntry* entry = wc->GetController().GetPendingEntry();
  EXPECT_EQ(nullptr, entry);
  EXPECT_EQ(url, wc->GetVisibleURL());

  // There should have been at least one NavigationStateChange event for
  // invalidating the URL in the address bar, to avoid leaving the stale URL
  // visible.
  EXPECT_GE(dialog_manager.url_invalidate_count(), 1);

  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
}

// Helper class for beforunload tests.  Sets up a custom dialog manager for the
// main WebContents and provides helpers to register and test beforeunload
// handlers.
//
// TODO(alexmos): Refactor other beforeunload tests in this file to use this
// class.
class RenderFrameHostImplBeforeUnloadBrowserTest
    : public RenderFrameHostImplBrowserTest {
 public:
  RenderFrameHostImplBeforeUnloadBrowserTest() {}

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  TestJavaScriptDialogManager* dialog_manager() {
    return dialog_manager_.get();
  }

  void CloseDialogAndProceed() {
    dialog_manager_->Run(true /* navigation should proceed */,
                         base::string16());
  }

  void CloseDialogAndCancel() {
    dialog_manager_->Run(false /* navigation should proceed */,
                         base::string16());
  }

  // Installs a beforeunload handler in the given frame.
  // |before_unload_options| specify whether the handler should send a "ping"
  // message through domAutomationController, and/or whether it should trigger
  // the modal beforeunload confirmation dialog.
  enum BeforeUnloadOptions {
    SHOW_DIALOG = 1,
    SEND_PING = 2,
  };
  void InstallBeforeUnloadHandler(FrameTreeNode* ftn,
                                  int before_unload_options) {
    std::string script = "window.onbeforeunload = () => { ";
    if (before_unload_options & SEND_PING)
      script += "domAutomationController.send('ping'); ";
    if (before_unload_options & SHOW_DIALOG)
      script += "return 'x'; ";
    script += " }";
    EXPECT_TRUE(ExecuteScript(ftn, script));
  }

  int RetrievePingsFromMessageQueue(DOMMessageQueue* msg_queue) {
    int num_pings = 0;
    std::string message;
    while (msg_queue->PopMessage(&message)) {
      base::TrimString(message, "\"", &message);
      // Only count messages from beforeunload.  For example, an ExecuteScript
      // sends its own message to DOMMessageQueue, which we need to ignore.
      if (message == "ping")
        ++num_pings;
    }
    return num_pings;
  }

 protected:
  void SetUpOnMainThread() override {
    RenderFrameHostImplBrowserTest::SetUpOnMainThread();
    dialog_manager_.reset(new TestJavaScriptDialogManager);
    web_contents()->SetDelegate(dialog_manager_.get());
  }

  void TearDownOnMainThread() override {
    web_contents()->SetDelegate(nullptr);
    web_contents()->SetJavaScriptDialogManagerForTesting(nullptr);
    RenderFrameHostImplBrowserTest::TearDownOnMainThread();
  }

 private:
  std::unique_ptr<TestJavaScriptDialogManager> dialog_manager_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostImplBeforeUnloadBrowserTest);
};

// Check that when a frame performs a browser-initiated navigation, its
// cross-site subframe is able to execute a beforeunload handler and put up a
// dialog to cancel or allow the navigation. This matters especially in
// --site-per-process mode; see https://crbug.com/853021.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBeforeUnloadBrowserTest,
                       SubframeShowsDialogWhenMainFrameNavigates) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Install a beforeunload handler in the first iframe.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  InstallBeforeUnloadHandler(root->child_at(0), SHOW_DIALOG);

  // Disable beforeunload timer to prevent flakiness.
  PrepContentsForBeforeUnloadTest(web_contents());

  // Navigate cross-site and wait for the beforeunload dialog to be shown from
  // the subframe.
  GURL cross_site_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  shell()->LoadURL(cross_site_url);
  dialog_manager()->Wait();

  // Only the main frame should be marked as waiting for beforeunload ACK as
  // the frame being navigated.
  RenderFrameHostImpl* main_frame = web_contents()->GetMainFrame();
  RenderFrameHostImpl* child = root->child_at(0)->current_frame_host();
  EXPECT_TRUE(main_frame->is_waiting_for_beforeunload_ack());
  EXPECT_FALSE(child->is_waiting_for_beforeunload_ack());

  // Sanity check that the main frame is waiting for subframe's beforeunload
  // ACK.
  EXPECT_EQ(main_frame, child->GetBeforeUnloadInitiator());
  EXPECT_EQ(main_frame, main_frame->GetBeforeUnloadInitiator());
  EXPECT_EQ(1u, main_frame->beforeunload_pending_replies_.size());

  // In --site-per-process mode, the beforeunload ACK should come back from the
  // child RFH.  Without --site-per-process, it will come from the main frame
  // RFH, which processes beforeunload for both main frame and child frame,
  // since they are in the same process.
  RenderFrameHostImpl* frame_that_sent_beforeunload_ipc =
      AreAllSitesIsolatedForTesting() ? child : main_frame;
  EXPECT_TRUE(main_frame->beforeunload_pending_replies_.count(
      frame_that_sent_beforeunload_ipc));

  // Answer the dialog with "cancel" to stay on current page.
  CloseDialogAndCancel();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(main_url, web_contents()->GetLastCommittedURL());

  // Verify beforeunload state has been cleared.
  EXPECT_FALSE(main_frame->is_waiting_for_beforeunload_ack());
  EXPECT_FALSE(child->is_waiting_for_beforeunload_ack());
  EXPECT_EQ(nullptr, main_frame->GetBeforeUnloadInitiator());
  EXPECT_EQ(nullptr, child->GetBeforeUnloadInitiator());
  EXPECT_EQ(0u, main_frame->beforeunload_pending_replies_.size());

  // Try navigating again.  The dialog should come up again.
  shell()->LoadURL(cross_site_url);
  dialog_manager()->Wait();
  EXPECT_TRUE(main_frame->is_waiting_for_beforeunload_ack());

  // Now answer the dialog and allow the navigation to proceed.  Disable
  // SwapOut ACK on the old frame so that it sticks around in pending delete
  // state, since the test later verifies that it has received the beforeunload
  // ACK.
  TestFrameNavigationObserver commit_observer(root);
  main_frame->DisableSwapOutTimerForTesting();
  CloseDialogAndProceed();
  commit_observer.WaitForCommit();
  EXPECT_EQ(cross_site_url, web_contents()->GetLastCommittedURL());
  EXPECT_FALSE(
      web_contents()->GetMainFrame()->is_waiting_for_beforeunload_ack());

  // The navigation that succeeded was a browser-initiated, main frame
  // navigation, so it swapped RenderFrameHosts. |main_frame| should now be
  // pending deletion and waiting for swapout ACK, but it should not be waiting
  // for the beforeunload ACK.
  EXPECT_FALSE(main_frame->is_active());
  EXPECT_FALSE(main_frame->is_waiting_for_beforeunload_ack());
  EXPECT_EQ(0u, main_frame->beforeunload_pending_replies_.size());
  EXPECT_EQ(nullptr, main_frame->GetBeforeUnloadInitiator());
}

// Check that when a frame with multiple cross-site subframes navigates, all
// the subframes execute their beforeunload handlers, but at most one
// beforeunload dialog is allowed per navigation.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBeforeUnloadBrowserTest,
                       MultipleSubframes) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c),b,c(d),c,d)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Install a beforeunload handler in five of eight frames to send a ping via
  // domAutomationController and request a beforeunload dialog.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  InstallBeforeUnloadHandler(root, SEND_PING | SHOW_DIALOG);
  InstallBeforeUnloadHandler(root->child_at(0)->child_at(0),
                             SEND_PING | SHOW_DIALOG);
  InstallBeforeUnloadHandler(root->child_at(1), SEND_PING | SHOW_DIALOG);
  InstallBeforeUnloadHandler(root->child_at(2), SEND_PING | SHOW_DIALOG);
  InstallBeforeUnloadHandler(root->child_at(2)->child_at(0),
                             SEND_PING | SHOW_DIALOG);

  // Disable beforeunload timer to prevent flakiness.
  PrepContentsForBeforeUnloadTest(web_contents());

  // Navigate main frame cross-site and wait for the beforeunload dialog to be
  // shown from one of the frames.
  DOMMessageQueue msg_queue;
  GURL cross_site_url(embedded_test_server()->GetURL("e.com", "/title1.html"));
  shell()->LoadURL(cross_site_url);
  dialog_manager()->Wait();

  // Answer the dialog and allow the navigation to proceed.
  CloseDialogAndProceed();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(cross_site_url, web_contents()->GetLastCommittedURL());

  // We should've received five beforeunload pings.
  EXPECT_EQ(5, RetrievePingsFromMessageQueue(&msg_queue));

  // No more beforeunload dialogs shouldn't been shown, due to a policy of at
  // most one dialog per navigation.
  EXPECT_EQ(1, dialog_manager()->num_beforeunload_dialogs_seen());
}

// Similar to the test above, but test scenarios where the subframes with
// beforeunload handlers aren't local roots.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBeforeUnloadBrowserTest,
                       NonLocalRootSubframes) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a(b),c(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Install a beforeunload handler in two of five frames to send a ping via
  // domAutomationController and request a beforeunload dialog.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  InstallBeforeUnloadHandler(root->child_at(0), SEND_PING | SHOW_DIALOG);
  InstallBeforeUnloadHandler(root->child_at(0)->child_at(0),
                             SEND_PING | SHOW_DIALOG);

  // Disable beforeunload timer to prevent flakiness.
  PrepContentsForBeforeUnloadTest(web_contents());

  // Navigate and wait for the beforeunload dialog to be shown from one of the
  // frames.
  DOMMessageQueue msg_queue;
  GURL cross_site_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  shell()->LoadURL(cross_site_url);
  dialog_manager()->Wait();

  // Answer the dialog and allow the navigation to proceed.
  CloseDialogAndProceed();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(cross_site_url, web_contents()->GetLastCommittedURL());

  // We should've received two beforeunload pings.
  EXPECT_EQ(2, RetrievePingsFromMessageQueue(&msg_queue));

  // No more beforeunload dialogs shouldn't been shown, due to a policy of at
  // most one dialog per navigation.
  EXPECT_EQ(1, dialog_manager()->num_beforeunload_dialogs_seen());
}

// Test that cross-site subframes run the beforeunload handler when the main
// frame performs a renderer-initiated navigation.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBeforeUnloadBrowserTest,
                       RendererInitiatedNavigation) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,b,c)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Install a beforeunload handler in both a.com frames to send a ping via
  // domAutomationController.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  InstallBeforeUnloadHandler(root, SEND_PING);
  InstallBeforeUnloadHandler(root->child_at(0), SEND_PING);

  // Install a beforeunload handler in the b.com frame to put up a dialog.
  InstallBeforeUnloadHandler(root->child_at(1), SHOW_DIALOG);

  // Disable beforeunload timer to prevent flakiness.
  PrepContentsForBeforeUnloadTest(web_contents());

  // Start a same-site renderer-initiated navigation.  The beforeunload dialog
  // from the b.com frame should be shown.  The other two a.com frames should
  // send pings from their beforeunload handlers.
  DOMMessageQueue msg_queue;
  GURL new_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  TestNavigationManager navigation_manager(web_contents(), new_url);
  EXPECT_TRUE(ExecuteScript(root, "location.href = '" + new_url.spec() + "';"));
  dialog_manager()->Wait();

  // Answer the dialog and allow the navigation to proceed.  Note that at this
  // point, without site isolation, the navigation hasn't started yet, as the
  // navigating frame is still processing beforeunload for all its descendant
  // local frames.  With site isolation, the a.com frames have finished
  // beforeunload, and the browser process has received OnBeginNavigation, but
  // the navigation is paused until the b.com subframe process finishes running
  // beforeunload.
  CloseDialogAndProceed();

  // Wait for navigation to end.
  navigation_manager.WaitForNavigationFinished();
  EXPECT_EQ(new_url, web_contents()->GetLastCommittedURL());

  // We should have received two pings from two a.com frames.  If we receive
  // more, that probably means we ran beforeunload an extra time in the a.com
  // frames.
  EXPECT_EQ(2, RetrievePingsFromMessageQueue(&msg_queue));
  EXPECT_EQ(1, dialog_manager()->num_beforeunload_dialogs_seen());
}

// Similar to the test above, but check a navigation in a subframe rather than
// the main frame.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBeforeUnloadBrowserTest,
                       RendererInitiatedNavigationInSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c),c)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Install a beforeunload handler to send a ping in all frames.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  InstallBeforeUnloadHandler(root, SEND_PING);
  InstallBeforeUnloadHandler(root->child_at(0), SEND_PING);
  InstallBeforeUnloadHandler(root->child_at(0)->child_at(0), SEND_PING);
  InstallBeforeUnloadHandler(root->child_at(1), SEND_PING);

  // Disable beforeunload timer to prevent flakiness.
  PrepContentsForBeforeUnloadTest(web_contents());

  // Start a renderer-initiated navigation in the middle frame.
  DOMMessageQueue msg_queue;
  GURL new_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  TestNavigationManager navigation_manager(web_contents(), new_url);
  EXPECT_TRUE(ExecuteScript(root->child_at(0),
                            "location.href = '" + new_url.spec() + "';"));
  navigation_manager.WaitForNavigationFinished();
  EXPECT_EQ(new_url,
            root->child_at(0)->current_frame_host()->GetLastCommittedURL());

  // We should have received two pings from the b.com frame and its child.
  // Other frames' beforeunload handlers shouldn't have run.
  EXPECT_EQ(2, RetrievePingsFromMessageQueue(&msg_queue));

  // We shouldn't have seen any beforeunload dialogs.
  EXPECT_EQ(0, dialog_manager()->num_beforeunload_dialogs_seen());
}

// Ensure that when a beforeunload handler deletes a subframe which is also
// running beforeunload, the navigation can still proceed.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBeforeUnloadBrowserTest,
                       DetachSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Install a beforeunload handler in root frame to delete the subframe.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  std::string script =
      "window.onbeforeunload = () => { "
      "  document.body.removeChild(document.querySelector('iframe'));"
      "}";
  EXPECT_TRUE(ExecuteScript(root, script));

  // Install a beforeunload handler which never finishes in subframe.
  EXPECT_TRUE(ExecuteScript(root->child_at(0),
                            "window.onbeforeunload = () => { while (1) ; }"));

  // Disable beforeunload timer to prevent flakiness.
  PrepContentsForBeforeUnloadTest(web_contents());

  // Navigate main frame and ensure that it doesn't time out.  When the main
  // frame detaches the subframe, the RFHI destruction should unblock the
  // navigation from waiting on the subframe's beforeunload ACK.
  GURL new_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), new_url));
}

// Ensure that A(B(A)) cases work sanely with beforeunload handlers.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBeforeUnloadBrowserTest,
                       RendererInitiatedNavigationInABAB) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(a(b)))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Install a beforeunload handler to send a ping in all frames.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  InstallBeforeUnloadHandler(root, SEND_PING);
  InstallBeforeUnloadHandler(root->child_at(0), SEND_PING);
  InstallBeforeUnloadHandler(root->child_at(0)->child_at(0), SEND_PING);
  InstallBeforeUnloadHandler(root->child_at(0)->child_at(0)->child_at(0),
                             SEND_PING);

  // Disable beforeunload timer to prevent flakiness.
  PrepContentsForBeforeUnloadTest(web_contents());

  // Navigate the main frame.
  DOMMessageQueue msg_queue;
  GURL new_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), new_url));

  // We should have received four pings.
  EXPECT_EQ(4, RetrievePingsFromMessageQueue(&msg_queue));

  // We shouldn't have seen any beforeunload dialogs.
  EXPECT_EQ(0, dialog_manager()->num_beforeunload_dialogs_seen());
}

// Ensure that the beforeunload timeout works properly when
// beforeunload handlers from subframes time out.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBeforeUnloadBrowserTest,
                       TimeoutInSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Install a beforeunload handler to send a ping in main frame.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  InstallBeforeUnloadHandler(root, SEND_PING);

  // Install a beforeunload handler which never finishes in subframe.
  EXPECT_TRUE(ExecuteScript(root->child_at(0),
                            "window.onbeforeunload = () => { while (1) ; }"));

  // Navigate the main frame.  We should eventually time out on the subframe
  // beforeunload handler and complete the navigation.
  GURL new_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), new_url));
}

// Ensure that the beforeunload timeout isn't restarted when a frame attempts
// to show a beforeunload dialog and fails because the dialog is already being
// shown by another frame.  See https://crbug.com/865223.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBeforeUnloadBrowserTest,
                       TimerNotRestartedBySecondDialog) {
  // This test exercises a scenario that's only possible with
  // --site-per-process.
  if (!AreAllSitesIsolatedForTesting())
    return;

  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImpl* main_frame = web_contents()->GetMainFrame();

  // Install a beforeunload handler to show a dialog in both frames.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  InstallBeforeUnloadHandler(root, SHOW_DIALOG);
  InstallBeforeUnloadHandler(root->child_at(0), SHOW_DIALOG);

  // Extend the beforeunload timeout to prevent flakiness.  This test can't use
  // PrepContentsForBeforeUnloadTest(), as that clears the timer altogether,
  // and this test needs the timer to be valid, to see whether it gets paused
  // and not restarted correctly.
  main_frame->SetBeforeUnloadTimeoutDelayForTesting(
      base::TimeDelta::FromSeconds(30));

  // Start a navigation in the main frame.
  GURL new_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  shell()->LoadURL(new_url);

  // We should have two pending beforeunload ACKs at this point, and the
  // beforeunload timer should be running.
  EXPECT_EQ(2u, main_frame->beforeunload_pending_replies_.size());
  EXPECT_TRUE(main_frame->beforeunload_timeout_->IsRunning());

  // Wait for the dialog from one of the frames.  Note that either frame could
  // be the first to trigger the dialog.
  dialog_manager()->Wait();

  // The dialog should've canceled the timer.
  EXPECT_FALSE(main_frame->beforeunload_timeout_->IsRunning());

  // Don't close the dialog and allow the second beforeunload to come in and
  // attempt to show a dialog.  This should fail due to the intervention of at
  // most one dialog per navigation and respond to the renderer with the
  // confirmation to proceed, which should trigger a beforeunload ACK
  // from the second frame. Wait for that beforeunload ACK.  After it's
  // received, there will be one ACK remaining for the frame that's currently
  // showing the dialog.
  while (main_frame->beforeunload_pending_replies_.size() > 1) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  // Ensure that the beforeunload timer hasn't been restarted, since the first
  // beforeunload dialog is still up at this point.
  EXPECT_FALSE(main_frame->beforeunload_timeout_->IsRunning());

  // Cancel the dialog and make sure we stay on the old page.
  CloseDialogAndCancel();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(main_url, web_contents()->GetLastCommittedURL());
}

namespace {

// A helper to execute some script in a frame just before it is deleted, such
// that no message loops are pumped and no sync IPC messages are processed
// between script execution and the destruction of the RenderFrameHost  .
class ExecuteScriptBeforeRenderFrameDeletedHelper
    : public RenderFrameDeletedObserver {
 public:
  ExecuteScriptBeforeRenderFrameDeletedHelper(RenderFrameHost* observed_frame,
                                              const std::string& script)
      : RenderFrameDeletedObserver(observed_frame), script_(script) {}

 protected:
  // WebContentsObserver:
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override {
    const bool was_deleted = deleted();
    RenderFrameDeletedObserver::RenderFrameDeleted(render_frame_host);
    if (deleted() && !was_deleted)
      ExecuteScriptAsync(render_frame_host, script_);
  }

 private:
  std::string script_;

  DISALLOW_COPY_AND_ASSIGN(ExecuteScriptBeforeRenderFrameDeletedHelper);
};

}  // namespace

// Regression test for https://crbug.com/728171 where the sync IPC channel has a
// connection error but we don't properly check for it. This occurs because we
// send a sync window.open IPC after the RenderFrameHost is destroyed.
//
// The test creates two WebContents rendered in the same process. The first is
// is the window-opener of the second, so the first window can be used to relay
// information collected during the destruction of the RenderFrame in the second
// WebContents back to the browser process.
//
// The issue is then reproduced by asynchronously triggering a call to
// window.open() in the main frame of the second WebContents in response to
// WebContentsObserver::RenderFrameDeleted -- that is, just before the RFHI is
// destroyed on the browser side. The test assumes that between these two
// events, the UI message loop is not pumped, and no sync IPC messages are
// processed on the UI thread.
//
// Note that if the second WebContents scheduled a call to window.close() to
// close itself after it calls window.open(), the CreateNewWindow sync IPC could
// be dispatched *before* WidgetHostMsg_Close in the browser process, provided
// that the browser happened to be in IPC::SyncChannel::WaitForReply on the UI
// thread (most likely after sending GpuCommandBufferMsg_* messages), in which
// case incoming sync IPCs to this thread are dispatched, but the message loop
// is not pumped, so proxied non-sync IPCs are not delivered.
//
// Furthermore, on Android, exercising window.open() must be delayed until after
// content::RemoveShellView returns, as that method calls into JNI to close the
// view corresponding to the WebContents, which will then call back into native
// code and may run nested message loops and send sync IPC messages.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       FrameDetached_WindowOpenIPCFails) {
  NavigateToURL(shell(), GetTestUrl("", "title1.html"));
  EXPECT_EQ(1u, Shell::windows().size());
  GURL test_url = GetTestUrl("render_frame_host", "window_open.html");
  std::string open_script =
      base::StringPrintf("popup = window.open('%s');", test_url.spec().c_str());

  TestNavigationObserver second_contents_navigation_observer(nullptr, 1);
  second_contents_navigation_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(content::ExecuteScript(shell(), open_script));
  second_contents_navigation_observer.Wait();

  ASSERT_EQ(2u, Shell::windows().size());
  Shell* new_shell = Shell::windows()[1];
  ExecuteScriptBeforeRenderFrameDeletedHelper deleted_observer(
      new_shell->web_contents()->GetMainFrame(), "callWindowOpen();");
  new_shell->Close();
  deleted_observer.WaitUntilDeleted();

  bool did_call_window_open = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "domAutomationController.send(!!popup.didCallWindowOpen)",
      &did_call_window_open));
  EXPECT_TRUE(did_call_window_open);

  std::string result_of_window_open;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell(), "domAutomationController.send(String(popup.resultOfWindowOpen))",
      &result_of_window_open));
  EXPECT_EQ("null", result_of_window_open);
}

namespace {
void PostRequestMonitor(int* post_counter,
                        const net::test_server::HttpRequest& request) {
  if (request.method != net::test_server::METHOD_POST)
    return;
  (*post_counter)++;
  auto it = request.headers.find("Content-Type");
  CHECK(it != request.headers.end());
  CHECK(!it->second.empty());
}
}  // namespace

// Verifies form submits and resubmits work.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, POSTNavigation) {
  net::EmbeddedTestServer http_server;
  base::FilePath content_test_data(FILE_PATH_LITERAL("content/test/data"));
  http_server.AddDefaultHandlers(content_test_data);
  int post_counter = 0;
  http_server.RegisterRequestMonitor(
      base::Bind(&PostRequestMonitor, &post_counter));
  ASSERT_TRUE(http_server.Start());

  GURL url(http_server.GetURL("/session_history/form.html"));
  GURL post_url = http_server.GetURL("/echotitle");

  // Navigate to a page with a form.
  TestNavigationObserver observer(shell()->web_contents());
  NavigateToURL(shell(), url);
  EXPECT_EQ(url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // Submit the form.
  GURL submit_url("javascript:submitForm('isubmit')");
  NavigateToURL(shell(), submit_url);

  // Check that a proper POST navigation was done.
  EXPECT_EQ("text=&select=a",
            base::UTF16ToASCII(shell()->web_contents()->GetTitle()));
  EXPECT_EQ(post_url, shell()->web_contents()->GetLastCommittedURL());
  EXPECT_TRUE(shell()
                  ->web_contents()
                  ->GetController()
                  .GetLastCommittedEntry()
                  ->GetHasPostData());

  // Reload and verify the form was submitted.
  shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ("text=&select=a",
            base::UTF16ToASCII(shell()->web_contents()->GetTitle()));
  CHECK_EQ(2, post_counter);
}

namespace {

class NavigationHandleGrabber : public WebContentsObserver {
 public:
  explicit NavigationHandleGrabber(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetURL().path() != "/title2.html")
      return;
    static_cast<NavigationHandleImpl*>(navigation_handle)
        ->set_complete_callback_for_testing(
            base::Bind(&NavigationHandleGrabber::SendingNavigationCommitted,
                       base::Unretained(this), navigation_handle));
  }

  void SendingNavigationCommitted(
      NavigationHandle* navigation_handle,
      NavigationThrottle::ThrottleCheckResult result) {
    if (navigation_handle->GetURL().path() != "/title2.html")
      return;
    ExecuteScriptAsync(web_contents(), "document.open();");
  }

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetURL().path() != "/title2.html")
      return;
    if (navigation_handle->HasCommitted())
      committed_title2_ = true;
    run_loop_.QuitClosure().Run();
  }

  void WaitForTitle2() { run_loop_.Run(); }

  bool committed_title2() { return committed_title2_; }

 private:
  bool committed_title2_ = false;
  base::RunLoop run_loop_;
};
}  // namespace

// Verifies that if a frame aborts a navigation right after it starts, it is
// cancelled.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, FastNavigationAbort) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  NavigateToURL(shell(), url);

  // Now make a navigation.
  NavigationHandleGrabber observer(shell()->web_contents());
  const base::string16 title = base::ASCIIToUTF16("done");
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "window.location.href='/title2.html'"));
  observer.WaitForTitle2();
  // Flush IPCs to make sure the renderer didn't tell us to navigate. Need to
  // make two round trips.
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(), ""));
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(), ""));
  EXPECT_FALSE(observer.committed_title2());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       TerminationDisablersClearedOnRendererCrash) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "beforeunload.html")));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame =
      static_cast<RenderFrameHostImpl*>(wc->GetMainFrame());

  EXPECT_TRUE(main_frame->GetSuddenTerminationDisablerState(
      blink::kBeforeUnloadHandler));

  // Make the renderer crash.
  RenderProcessHost* renderer_process = main_frame->GetProcess();
  RenderProcessHostWatcher crash_observer(
      renderer_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  renderer_process->Shutdown(0);
  crash_observer.Wait();

  EXPECT_FALSE(main_frame->GetSuddenTerminationDisablerState(
      blink::kBeforeUnloadHandler));

  // This should not trigger a DCHECK once the renderer sends up the termination
  // disabler flags.
  shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_TRUE(main_frame->GetSuddenTerminationDisablerState(
      blink::kBeforeUnloadHandler));
}

// Aborted renderer-initiated navigations that don't destroy the current
// document (e.g. no error page is displayed) must not cancel pending
// XMLHttpRequests.
// See https://crbug.com/762945.
IN_PROC_BROWSER_TEST_F(
    ContentBrowserTest,
    AbortedRendererInitiatedNavigationDoNotCancelPendingXHR) {
  net::test_server::ControllableHttpResponse xhr_response(
      embedded_test_server(), "/xhr_request");
  EXPECT_TRUE(embedded_test_server()->Start());

  GURL main_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // 1) Send an xhr request, but do not send its response for the moment.
  const char* send_slow_xhr =
      "var request = new XMLHttpRequest();"
      "request.addEventListener('abort', () => document.title = 'xhr aborted');"
      "request.addEventListener('load', () => document.title = 'xhr loaded');"
      "request.open('GET', '%s');"
      "request.send();";
  const GURL slow_url = embedded_test_server()->GetURL("/xhr_request");
  EXPECT_TRUE(content::ExecuteScript(
      shell(), base::StringPrintf(send_slow_xhr, slow_url.spec().c_str())));
  xhr_response.WaitForRequest();

  // 2) In the meantime, create a renderer-initiated navigation. It will be
  // aborted.
  TestNavigationManager observer(shell()->web_contents(),
                                 GURL("customprotocol:aborted"));
  EXPECT_TRUE(content::ExecuteScript(
      shell(), "window.location = 'customprotocol:aborted'"));
  EXPECT_FALSE(observer.WaitForResponse());
  observer.WaitForNavigationFinished();

  // 3) Send the response for the XHR requests.
  xhr_response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Connection: close\r\n"
      "Content-Length: 2\r\n"
      "Content-Type: text/plain; charset=utf-8\r\n"
      "\r\n"
      "OK");
  xhr_response.Done();

  // 4) Wait for the XHR request to complete.
  const base::string16 xhr_aborted_title = base::ASCIIToUTF16("xhr aborted");
  const base::string16 xhr_loaded_title = base::ASCIIToUTF16("xhr loaded");
  TitleWatcher watcher(shell()->web_contents(), xhr_loaded_title);
  watcher.AlsoWaitForTitle(xhr_aborted_title);

  EXPECT_EQ(xhr_loaded_title, watcher.WaitAndGetTitle());
}

// A browser-initiated javascript-url navigation must not prevent the current
// document from loading.
// See https://crbug.com/766149.
IN_PROC_BROWSER_TEST_F(ContentBrowserTest,
                       BrowserInitiatedJavascriptUrlDoNotPreventLoading) {
  net::test_server::ControllableHttpResponse main_document_response(
      embedded_test_server(), "/main_document");
  EXPECT_TRUE(embedded_test_server()->Start());

  GURL main_document_url(embedded_test_server()->GetURL("/main_document"));
  TestNavigationManager main_document_observer(shell()->web_contents(),
                                               main_document_url);

  // 1) Navigate. Send the header but not the body. The navigation commits in
  //    the browser. The renderer is still loading the document.
  {
    shell()->LoadURL(main_document_url);
    EXPECT_TRUE(main_document_observer.WaitForRequestStart());
    main_document_observer.ResumeNavigation();  // Send the request.

    main_document_response.WaitForRequest();
    main_document_response.Send(
        "HTTP/1.1 200 OK\r\n"
        "Connection: close\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "\r\n");

    EXPECT_TRUE(main_document_observer.WaitForResponse());
    main_document_observer.ResumeNavigation();  // Commit the navigation.
  }

  // 2) A browser-initiated javascript-url navigation happens.
  {
    GURL javascript_url(
        "javascript:window.domAutomationController.send('done')");
    shell()->LoadURL(javascript_url);
    DOMMessageQueue dom_message_queue(WebContents::FromRenderFrameHost(
        shell()->web_contents()->GetMainFrame()));
    std::string done;
    EXPECT_TRUE(dom_message_queue.WaitForMessage(&done));
    EXPECT_EQ("\"done\"", done);
  }

  // 3) The end of the response is issued. The renderer must be able to receive
  //    it.
  {
    const base::string16 document_loaded_title =
        base::ASCIIToUTF16("document loaded");
    TitleWatcher watcher(shell()->web_contents(), document_loaded_title);
    main_document_response.Send(
        "<script>"
        "   window.onload = function(){"
        "     document.title = 'document loaded'"
        "   }"
        "</script>");
    main_document_response.Done();
    EXPECT_EQ(document_loaded_title, watcher.WaitAndGetTitle());
  }
}

// Test that a same-document browser-initiated navigation doesn't prevent a
// document from loading. See https://crbug.com/769645.
IN_PROC_BROWSER_TEST_F(
    ContentBrowserTest,
    SameDocumentBrowserInitiatedNavigationWhileDocumentIsLoading) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  EXPECT_TRUE(embedded_test_server()->Start());

  // 1) Load a new document. It reaches the ReadyToCommit stage and then is slow
  //    to load.
  GURL url(embedded_test_server()->GetURL("/main_document"));
  TestNavigationManager observer_new_document(shell()->web_contents(), url);
  shell()->LoadURL(url);

  // The navigation starts
  EXPECT_TRUE(observer_new_document.WaitForRequestStart());
  observer_new_document.ResumeNavigation();

  // The server sends the first part of the response and waits.
  response.WaitForRequest();
  response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "<html>"
      "  <body>"
      "    <div id=\"anchor\"></div>"
      "    <script>"
      "      domAutomationController.send('First part received')"
      "    </script>");

  // The browser reaches the ReadyToCommit stage.
  EXPECT_TRUE(observer_new_document.WaitForResponse());
  RenderFrameHostImpl* main_rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());
  DOMMessageQueue dom_message_queue(WebContents::FromRenderFrameHost(main_rfh));
  observer_new_document.ResumeNavigation();

  // Wait for the renderer to load the first part of the response.
  std::string first_part_received;
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&first_part_received));
  EXPECT_EQ("\"First part received\"", first_part_received);

  // 2) In the meantime, a browser-initiated same-document navigation commits.
  GURL anchor_url(url.spec() + "#anchor");
  TestNavigationManager observer_same_document(shell()->web_contents(),
                                               anchor_url);
  shell()->LoadURL(anchor_url);
  observer_same_document.WaitForNavigationFinished();

  // 3) The last part of the response is received.
  response.Send(
      "    <script>"
      "      domAutomationController.send('Second part received')"
      "    </script>"
      "  </body>"
      "</html>");
  response.Done();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // The renderer should be able to load the end of the response.
  std::string second_part_received;
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&second_part_received));
  EXPECT_EQ("\"Second part received\"", second_part_received);
}

namespace {

// Allows injecting a fake, test-provided |interface_provider_request| into
// DidCommitProvisionalLoad messages in a given |web_contents| instead of the
// real one coming from the renderer process.
class ScopedFakeInterfaceProviderRequestInjector
    : public DidCommitProvisionalLoadInterceptor {
 public:
  explicit ScopedFakeInterfaceProviderRequestInjector(WebContents* web_contents)
      : DidCommitProvisionalLoadInterceptor(web_contents) {}
  ~ScopedFakeInterfaceProviderRequestInjector() override = default;

  // Sets the fake InterfaceProvider |request| to inject into the next incoming
  // DidCommitProvisionalLoad message.
  void set_fake_request_for_next_commit(
      service_manager::mojom::InterfaceProviderRequest request) {
    next_fake_request_ = std::move(request);
  }

  const GURL& url_of_last_commit() const { return url_of_last_commit_; }

  const service_manager::mojom::InterfaceProviderRequest&
  original_request_of_last_commit() const {
    return original_request_of_last_commit_;
  }

 protected:
  bool WillDispatchDidCommitProvisionalLoad(
      RenderFrameHost* render_frame_host,
      ::FrameHostMsg_DidCommitProvisionalLoad_Params* params,
      service_manager::mojom::InterfaceProviderRequest*
          interface_provider_request) override {
    url_of_last_commit_ = params->url;
    original_request_of_last_commit_ = std::move(*interface_provider_request);
    *interface_provider_request = std::move(next_fake_request_);
    return true;
  }

 private:
  service_manager::mojom::InterfaceProviderRequest next_fake_request_;
  service_manager::mojom::InterfaceProviderRequest
      original_request_of_last_commit_;
  GURL url_of_last_commit_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFakeInterfaceProviderRequestInjector);
};

// Monitors the |document_scoped_interface_provider_binding_| of the given
// |render_frame_host| for incoming interface requests for |interface_name|, and
// invokes |callback| synchronously just before such a request would be
// dispatched.
class ScopedInterfaceRequestMonitor
    : public service_manager::mojom::InterfaceProviderInterceptorForTesting {
 public:
  ScopedInterfaceRequestMonitor(RenderFrameHost* render_frame_host,
                                base::StringPiece interface_name,
                                base::RepeatingClosure callback)
      : rfhi_(static_cast<RenderFrameHostImpl*>(render_frame_host)),
        impl_(binding().SwapImplForTesting(this)),
        interface_name_(interface_name),
        request_callback_(callback) {}

  ~ScopedInterfaceRequestMonitor() override {
    auto* old_impl = binding().SwapImplForTesting(impl_);
    DCHECK_EQ(old_impl, this);
  }

 protected:
  // service_manager::mojom::InterfaceProviderInterceptorForTesting:
  service_manager::mojom::InterfaceProvider* GetForwardingInterface() override {
    return impl_;
  }

  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle pipe) override {
    if (interface_name == interface_name_)
      request_callback_.Run();
    GetForwardingInterface()->GetInterface(interface_name, std::move(pipe));
  }

 private:
  mojo::Binding<service_manager::mojom::InterfaceProvider>& binding() {
    return rfhi_->document_scoped_interface_provider_binding_for_testing();
  }

  RenderFrameHostImpl* rfhi_;
  service_manager::mojom::InterfaceProvider* impl_;

  std::string interface_name_;
  base::RepeatingClosure request_callback_;

  DISALLOW_COPY_AND_ASSIGN(ScopedInterfaceRequestMonitor);
};

// Calls |callback| whenever a navigation finishes in |render_frame_host|.
class DidFinishNavigationObserver : public WebContentsObserver {
 public:
  DidFinishNavigationObserver(RenderFrameHost* render_frame_host,
                              base::RepeatingClosure callback)
      : WebContentsObserver(
            WebContents::FromRenderFrameHost(render_frame_host)),
        callback_(callback) {}

 protected:
  // WebContentsObserver:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    callback_.Run();
  }

 private:
  base::RepeatingClosure callback_;
  DISALLOW_COPY_AND_ASSIGN(DidFinishNavigationObserver);
};

}  // namespace

// For cross-document navigations, the DidCommitProvisionalLoad message from
// the renderer process will have its |interface_provider_request| argument set
// to the request end of a new InterfaceProvider interface connection that will
// be used by the newly committed document to access services exposed by the
// RenderFrameHost.
//
// This test verifies that even if that |interface_provider_request| already has
// pending interface requests, the RenderFrameHost binds the InterfaceProvider
// request in such a way that these pending interface requests are dispatched
// strictly after WebContentsObserver::DidFinishNavigation has fired, so that
// the requests will be served correctly in the security context of the newly
// committed document (i.e. GetLastCommittedURL/Origin will have been updated).
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTest,
    EarlyInterfaceRequestsFromNewDocumentDispatchedAfterNavigationFinished) {
  const GURL first_url(embedded_test_server()->GetURL("/title1.html"));
  const GURL second_url(embedded_test_server()->GetURL("/title2.html"));

  // Load a URL that maps to the same SiteInstance as the second URL, to make
  // sure the second navigation will not be cross-process.
  ASSERT_TRUE(NavigateToURL(shell(), first_url));

  // Prepare an InterfaceProviderRequest with pending interface requests.
  service_manager::mojom::InterfaceProviderPtr
      interface_provider_with_pending_request;
  service_manager::mojom::InterfaceProviderRequest
      interface_provider_request_with_pending_request =
          mojo::MakeRequest(&interface_provider_with_pending_request);
  mojom::FrameHostTestInterfacePtr test_interface;
  interface_provider_with_pending_request->GetInterface(
      mojom::FrameHostTestInterface::Name_,
      mojo::MakeRequest(&test_interface).PassMessagePipe());

  // Replace the |interface_provider_request| argument in the next
  // DidCommitProvisionalLoad message coming from the renderer with the
  // rigged |interface_provider_with_pending_request| from above.
  ScopedFakeInterfaceProviderRequestInjector injector(shell()->web_contents());
  injector.set_fake_request_for_next_commit(
      std::move(interface_provider_request_with_pending_request));

  // Expect that by the time the interface request for FrameHostTestInterface is
  // dispatched to the RenderFrameHost, WebContentsObserver::DidFinishNavigation
  // will have already been invoked.
  bool did_finish_navigation = false;
  auto* main_rfh = shell()->web_contents()->GetMainFrame();
  DidFinishNavigationObserver navigation_finish_observer(
      main_rfh, base::BindLambdaForTesting([&did_finish_navigation]() {
        did_finish_navigation = true;
      }));

  base::RunLoop wait_until_interface_request_is_dispatched;
  ScopedInterfaceRequestMonitor monitor(
      main_rfh, mojom::FrameHostTestInterface::Name_,
      base::BindLambdaForTesting([&]() {
        EXPECT_TRUE(did_finish_navigation);
        wait_until_interface_request_is_dispatched.Quit();
      }));

  // Start the same-process navigation.
  test::ScopedInterfaceFilterBypass filter_bypass;
  ASSERT_TRUE(NavigateToURL(shell(), second_url));
  EXPECT_EQ(main_rfh, shell()->web_contents()->GetMainFrame());
  EXPECT_EQ(second_url, injector.url_of_last_commit());
  EXPECT_TRUE(injector.original_request_of_last_commit().is_pending());

  // Wait until the interface request for FrameHostTestInterface is dispatched.
  wait_until_interface_request_is_dispatched.Run();
}

// The InterfaceProvider interface, which is used by the RenderFrame to access
// Mojo services exposed by the RenderFrameHost, is not Channel-associated,
// thus not synchronized with navigation IPC messages. As a result, when the
// renderer commits a load, the DidCommitProvisional message might be at race
// with GetInterface messages, for example, an interface request issued by the
// previous document in its unload handler might arrive to the browser process
// just a moment after DidCommitProvisionalLoad.
//
// This test verifies that even if there is such a last-second GetInterface
// message originating from the previous document, it is no longer serviced.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       LateInterfaceRequestsFromOldDocumentNotDispatched) {
  const GURL first_url(embedded_test_server()->GetURL("/title1.html"));
  const GURL second_url(embedded_test_server()->GetURL("/title2.html"));

  // Prepare an InterfaceProviderRequest with no pending requests.
  service_manager::mojom::InterfaceProviderPtr interface_provider;
  service_manager::mojom::InterfaceProviderRequest interface_provider_request =
      mojo::MakeRequest(&interface_provider);

  // Set up a cunning mechnism to replace the |interface_provider_request|
  // argument in next DidCommitProvisionalLoad message with the rigged
  // |interface_provider_request| from above, whose client end is controlled by
  // this test; then trigger a navigation.
  {
    ScopedFakeInterfaceProviderRequestInjector injector(
        shell()->web_contents());
    test::ScopedInterfaceFilterBypass filter_bypass;
    injector.set_fake_request_for_next_commit(
        std::move(interface_provider_request));

    ASSERT_TRUE(NavigateToURL(shell(), first_url));
    ASSERT_EQ(first_url, injector.url_of_last_commit());
    ASSERT_TRUE(injector.original_request_of_last_commit().is_pending());
  }

  // Prepare an interface request for FrameHostTestInterface.
  mojom::FrameHostTestInterfacePtr test_interface;
  auto test_interface_request = mojo::MakeRequest(&test_interface);

  // Set up |dispatched_interface_request_callback| that would be invoked if the
  // interface request for FrameHostTestInterface was ever dispatched to the
  // RenderFrameHostImpl.
  base::MockCallback<base::RepeatingClosure>
      dispatched_interface_request_callback;
  auto* main_rfh = shell()->web_contents()->GetMainFrame();
  ScopedInterfaceRequestMonitor monitor(
      main_rfh, mojom::FrameHostTestInterface::Name_,
      dispatched_interface_request_callback.Get());

  // Set up the |test_interface request| to arrive on the InterfaceProvider
  // connection corresponding to the old document in the middle of the firing of
  // WebContentsObserver::DidFinishNavigation.
  // TODO(engedy): Should we PostTask() this instead just before synchronously
  // invoking DidCommitProvisionalLoad?
  //
  // Also set up |navigation_finished_callback| to be invoked afterwards, as a
  // sanity check to ensure that the request injection is actually executed.
  base::MockCallback<base::RepeatingClosure> navigation_finished_callback;
  DidFinishNavigationObserver navigation_finish_observer(
      main_rfh, base::BindLambdaForTesting([&]() {
        interface_provider->GetInterface(
            mojom::FrameHostTestInterface::Name_,
            test_interface_request.PassMessagePipe());
        std::move(navigation_finished_callback).Run();
      }));

  // The InterfaceProvider connection that semantically belongs to the old
  // document, but whose client end is actually controlled by this test, should
  // still be alive and well.
  ASSERT_TRUE(test_interface.is_bound());
  ASSERT_FALSE(test_interface.encountered_error());

  // Expect that the GetInterface message will never be dispatched, but the
  // DidFinishNavigation callback wll be invoked.
  EXPECT_CALL(dispatched_interface_request_callback, Run()).Times(0);
  EXPECT_CALL(navigation_finished_callback, Run());

  // Start the same-process navigation.
  ASSERT_TRUE(NavigateToURL(shell(), second_url));

  // Wait for a connection error on the |test_interface| as a signal, after
  // which it can be safely assumed that no GetInterface message will ever be
  // dispatched from that old InterfaceConnection.
  base::RunLoop run_loop;
  test_interface.set_connection_error_handler(run_loop.QuitWhenIdleClosure());
  run_loop.Run();

  EXPECT_TRUE(test_interface.encountered_error());
}

// Test the edge case where the `window` global object asssociated with the
// initial empty document is re-used for document corresponding to the first
// real committed load. This happens when the security origins of the two
// documents are the same. We do not want to recalculate this in the browser
// process, however, so for the first commit we leave it up to the renderer
// whether it wants to replace the InterfaceProvider connection or not.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       InterfaceProviderRequestIsOptionalForFirstCommit) {
  const GURL main_frame_url(embedded_test_server()->GetURL("/title1.html"));
  const GURL subframe_url(embedded_test_server()->GetURL("/title2.html"));

  service_manager::mojom::InterfaceProviderPtr interface_provider;
  auto stub_interface_provider_request = mojo::MakeRequest(&interface_provider);
  service_manager::mojom::InterfaceProviderRequest
      null_interface_provider_request(nullptr);

  for (auto* interface_provider_request :
       {&stub_interface_provider_request, &null_interface_provider_request}) {
    SCOPED_TRACE(interface_provider_request->is_pending());

    ASSERT_TRUE(NavigateToURL(shell(), main_frame_url));

    ScopedFakeInterfaceProviderRequestInjector injector(
        shell()->web_contents());
    injector.set_fake_request_for_next_commit(
        std::move(*interface_provider_request));

    // Must set 'src` before adding the iframe element to the DOM, otherwise it
    // will load `about:blank` as the first real load instead of |subframe_url|.
    // See: https://crbug.com/778318.
    //
    // Note that the child frame will first cycle through loading the initial
    // empty document regardless of when/how/if the `src` attribute is set.
    const auto script = base::StringPrintf(
        "let f = document.createElement(\"iframe\");"
        "f.src=\"%s\"; "
        "document.body.append(f);",
        subframe_url.spec().c_str());
    ASSERT_TRUE(ExecuteScript(shell(), script));

    WaitForLoadStop(shell()->web_contents());

    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetFrameTree()
                              ->root();
    ASSERT_EQ(1u, root->child_count());
    FrameTreeNode* child = root->child_at(0u);

    EXPECT_FALSE(injector.original_request_of_last_commit().is_pending());
    EXPECT_TRUE(child->has_committed_real_load());
    EXPECT_EQ(subframe_url, child->current_url());
  }
}

// Regression test for https://crbug.com/821022.
//
// Test the edge case of the above, namely, where the following commits take
// place in a subframe embedded into a document at `http://foo.com/`:
//
//  1) the initial empty document (`about:blank`)
//  2) `about:blank#ref`
//  3) `http://foo.com`
//
// Here, (2) should classify as a same-document navigation, and (3) should be
// considered the first real load. Because the first real load is same-origin
// with the initial empty document, the latter's `window` global object
// asssociated with the initial empty document is re-used for document
// corresponding to the first real committed load.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTest,
    InterfaceProviderRequestNotPresentForFirstRealLoadAfterAboutBlankWithRef) {
  const GURL kMainFrameURL(embedded_test_server()->GetURL("/title1.html"));
  const GURL kSubframeURLTwo("about:blank#ref");
  const GURL kSubframeURLThree(embedded_test_server()->GetURL("/title2.html"));
  const auto kNavigateToOneThenTwoScript = base::StringPrintf(
      "var f = document.createElement(\"iframe\");"
      "f.src=\"%s\"; "
      "document.body.append(f);",
      kSubframeURLTwo.spec().c_str());
  const auto kNavigateToThreeScript =
      base::StringPrintf("f.src=\"%s\";", kSubframeURLThree.spec().c_str());

  ASSERT_TRUE(NavigateToURL(shell(), kMainFrameURL));

  // Trigger navigation (1) by creating a new subframe, and then trigger
  // navigation (2) by setting it's `src` attribute before adding it to the DOM.
  //
  // We must set 'src` before adding the iframe element to the DOM, otherwise it
  // will load `about:blank` as the first real load instead of
  // |kSubframeURLTwo|. See: https://crbug.com/778318.
  //
  // Note that the child frame will first cycle through loading the initial
  // empty document regardless of when/how/if the `src` attribute is set.

  ASSERT_TRUE(ExecuteScript(shell(), kNavigateToOneThenTwoScript));
  WaitForLoadStop(shell()->web_contents());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1u, root->child_count());
  FrameTreeNode* child = root->child_at(0u);

  EXPECT_FALSE(child->has_committed_real_load());
  EXPECT_EQ(kSubframeURLTwo, child->current_url());
  EXPECT_EQ(url::Origin::Create(kMainFrameURL), child->current_origin());

  // Set the `src` attribute again to trigger navigation (3).

  TestFrameNavigationObserver commit_observer(child->current_frame_host());
  ScopedFakeInterfaceProviderRequestInjector injector(shell()->web_contents());
  injector.set_fake_request_for_next_commit(nullptr);

  ASSERT_TRUE(ExecuteScript(shell(), kNavigateToThreeScript));
  commit_observer.WaitForCommit();

  EXPECT_FALSE(injector.original_request_of_last_commit().is_pending());

  EXPECT_TRUE(child->has_committed_real_load());
  EXPECT_EQ(kSubframeURLThree, child->current_url());
  EXPECT_EQ(url::Origin::Create(kMainFrameURL), child->current_origin());
}

// Verify that if the UMA histograms are correctly recording if interface
// provider requests are getting dropped because they racily arrive from the
// previously active document (after the next navigation already committed).
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DroppedInterfaceRequestCounter) {
  const GURL kUrl1(embedded_test_server()->GetURL("/title1.html"));
  const GURL kUrl2(embedded_test_server()->GetURL("/title2.html"));
  const GURL kUrl3(embedded_test_server()->GetURL("/title3.html"));
  const GURL kUrl4(embedded_test_server()->GetURL("/empty.html"));

  // The 31-bit hash of the string "content.mojom:BrowserTarget".
  const int32_t kHashOfContentMojomBrowserTarget = 0x1730feb8;

  // Client ends of the fake interface provider requests injected for the first
  // and second navigations.
  service_manager::mojom::InterfaceProviderPtr interface_provider_1;
  service_manager::mojom::InterfaceProviderPtr interface_provider_2;

  base::RunLoop wait_until_connection_error_loop_1;
  base::RunLoop wait_until_connection_error_loop_2;

  {
    ScopedFakeInterfaceProviderRequestInjector injector(
        shell()->web_contents());
    injector.set_fake_request_for_next_commit(
        mojo::MakeRequest(&interface_provider_1));
    interface_provider_1.set_connection_error_handler(
        wait_until_connection_error_loop_1.QuitClosure());
    ASSERT_TRUE(NavigateToURL(shell(), kUrl1));
  }

  {
    ScopedFakeInterfaceProviderRequestInjector injector(
        shell()->web_contents());
    injector.set_fake_request_for_next_commit(
        mojo::MakeRequest(&interface_provider_2));
    interface_provider_2.set_connection_error_handler(
        wait_until_connection_error_loop_2.QuitClosure());
    ASSERT_TRUE(NavigateToURL(shell(), kUrl2));
  }

  // Simulate two interface requests corresponding to the first navigation
  // arrived after the second navigation was committed, hence were dropped.
  interface_provider_1->GetInterface("content.mojom.BrowserTarget",
                                     CreateDisconnectedMessagePipeHandle());
  interface_provider_1->GetInterface("content.mojom.BrowserTarget",
                                     CreateDisconnectedMessagePipeHandle());

  // RFHI destroys the DroppedInterfaceRequestLogger from navigation `n` on
  // navigation `n+2`. Histrograms are recorded on destruction, there should
  // be a single sample indicating two requests having been dropped for the
  // first URL.
  {
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(NavigateToURL(shell(), kUrl3));
    histogram_tester.ExpectUniqueSample(
        "RenderFrameHostImpl.DroppedInterfaceRequests", 2, 1);
    histogram_tester.ExpectUniqueSample(
        "RenderFrameHostImpl.DroppedInterfaceRequestName",
        kHashOfContentMojomBrowserTarget, 2);
  }

  // Simulate one interface request dropped for the second URL.
  interface_provider_2->GetInterface("content.mojom.BrowserTarget",
                                     CreateDisconnectedMessagePipeHandle());

  // A final navigation should record the sample from the second URL.
  {
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(NavigateToURL(shell(), kUrl4));
    histogram_tester.ExpectUniqueSample(
        "RenderFrameHostImpl.DroppedInterfaceRequests", 1, 1);
    histogram_tester.ExpectUniqueSample(
        "RenderFrameHostImpl.DroppedInterfaceRequestName",
        kHashOfContentMojomBrowserTarget, 1);
  }

  // Both the DroppedInterfaceRequestLogger for the first and second URLs are
  // destroyed -- even more interfacerequests should not cause any crashes.
  interface_provider_1->GetInterface("content.mojom.BrowserTarget",
                                     CreateDisconnectedMessagePipeHandle());
  interface_provider_2->GetInterface("content.mojom.BrowserTarget",
                                     CreateDisconnectedMessagePipeHandle());

  // The interface connections should be broken.
  wait_until_connection_error_loop_1.Run();
  wait_until_connection_error_loop_2.Run();
}

// Regression test for https://crbug.com/852350
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       GetCanonicalUrlAfterRendererCrash) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "beforeunload.html")));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame =
      static_cast<RenderFrameHostImpl*>(wc->GetMainFrame());

  // Make the renderer crash.
  RenderProcessHost* renderer_process = main_frame->GetProcess();
  RenderProcessHostWatcher crash_observer(
      renderer_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  renderer_process->Shutdown(0);
  crash_observer.Wait();

  main_frame->GetCanonicalUrlForSharing(base::DoNothing());
}

// This test makes sure that when a blocked frame commits with a different URL,
// it doesn't lead to a leaked NavigationHandle.  This is a regression test for
// https://crbug.com/872803.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ErrorPagesShouldntLeakNavigationHandles) {
  GURL main_url(embedded_test_server()->GetURL(
      "foo.com", "/frame_tree/page_with_one_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  GURL blocked_url(embedded_test_server()->GetURL(
      "blocked.com", "/frame-ancestors-none.html"));
  WebContents* web_contents = shell()->web_contents();
  NavigationHandleObserver nav_handle_observer(web_contents, blocked_url);
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "child0", blocked_url));

  // Verify that the NavigationHandle / NavigationRequest didn't leak.
  RenderFrameHostImpl* frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetAllFrames()[1]);
  EXPECT_EQ(0u, frame->GetNavigationEntryIdsPendingCommit().size());

  // TODO(lukasza, clamy): https://crbug.com/784904: Verify that
  // WebContentsObserver::DidFinishNavigation was called with the same
  // NavigationHandle as WebContentsObserver::DidStartNavigation.  This requires
  // properly matching the commit IPC to the NavigationHandle (ignoring that
  // their URLs do not match - matching instead using navigation id or mojo
  // interface identity).
  //
  // Subsequent checks don't make sense before WCO::DidFinishNavigation is
  // called - this is why ASSERT_TRUE is used here.
  //   ASSERT_TRUE(nav_handle_observer.has_committed());
  //   EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE,
  //       nav_handle_observer.net_error_code());

  // TODO(lukasza): https://crbug.com/759184: Verify
  // |nav_handle_observer.last_committed_url()| below - this should be possible
  // once we handle frame-ancestors CSP in the browser process and commit it
  // with the original URL.
  //   EXPECT_EQ(blocked_url, nav_handle_observer.last_committed_url());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       BeforeUnloadDialogSuppressedForDiscard) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestJavaScriptDialogManager dialog_manager;
  wc->SetDelegate(&dialog_manager);

  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "beforeunload.html")));
  // Disable the hang monitor, otherwise there will be a race between the
  // beforeunload dialog and the beforeunload hang timer.
  wc->GetMainFrame()->DisableBeforeUnloadHangMonitorForTesting();

  // Give the page a user gesture so javascript beforeunload works, and then
  // dispatch a before unload with discard as a reason. This should return
  // without any dialog being seen.
  wc->GetMainFrame()->ExecuteJavaScriptWithUserGestureForTests(
      base::string16());
  wc->GetMainFrame()->DispatchBeforeUnload(
      RenderFrameHostImpl::BeforeUnloadType::DISCARD, false);
  dialog_manager.Wait();
  EXPECT_EQ(0, dialog_manager.num_beforeunload_dialogs_seen());
  EXPECT_EQ(1, dialog_manager.num_beforeunload_fired_seen());
  EXPECT_FALSE(dialog_manager.proceed());

  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       PendingDialogMakesDiscardUnloadReturnFalse) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestJavaScriptDialogManager dialog_manager;
  wc->SetDelegate(&dialog_manager);

  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "beforeunload.html")));
  // Disable the hang monitor, otherwise there will be a race between the
  // beforeunload dialog and the beforeunload hang timer.
  wc->GetMainFrame()->DisableBeforeUnloadHangMonitorForTesting();

  // Give the page a user gesture so javascript beforeunload works, and then
  // dispatch a before unload with discard as a reason. This should return
  // without any dialog being seen.
  wc->GetMainFrame()->ExecuteJavaScriptWithUserGestureForTests(
      base::string16());

  // Launch an alert javascript dialog. This pending dialog should block a
  // subsequent discarding before unload request.
  wc->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("setTimeout(function(){alert('hello');}, 10);"));
  dialog_manager.Wait();
  EXPECT_EQ(0, dialog_manager.num_beforeunload_dialogs_seen());
  EXPECT_EQ(0, dialog_manager.num_beforeunload_fired_seen());

  // Dispatch a before unload request while the first is still blocked
  // on the dialog, and expect it to return false immediately (synchronously).
  wc->GetMainFrame()->DispatchBeforeUnload(
      RenderFrameHostImpl::BeforeUnloadType::DISCARD, false);
  dialog_manager.Wait();
  EXPECT_EQ(0, dialog_manager.num_beforeunload_dialogs_seen());
  EXPECT_EQ(1, dialog_manager.num_beforeunload_fired_seen());
  EXPECT_FALSE(dialog_manager.proceed());

  // Clear the existing javascript dialog so that the associated IPC message
  // doesn't leak.
  dialog_manager.Run(true, base::string16());

  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       NotifiesProcessHostOfAudibleAudio) {
  const auto RunPostedTasks = []() {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop.QuitClosure());
    run_loop.Run();
  };

  // Note: Just using the beforeunload.html test document to spin-up a
  // renderer. Any document will do.
  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "beforeunload.html")));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  auto* frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());
  auto* process = static_cast<RenderProcessHostImpl*>(frame->GetProcess());
  ASSERT_EQ(0, process->get_media_stream_count_for_testing());

  // Audible audio output should cause the media stream count to increment.
  frame->OnAudibleStateChanged(true);
  RunPostedTasks();
  EXPECT_EQ(1, process->get_media_stream_count_for_testing());

  // Silence should cause the media stream count to decrement.
  frame->OnAudibleStateChanged(false);
  RunPostedTasks();
  EXPECT_EQ(0, process->get_media_stream_count_for_testing());

  // Start audible audio output again, and then crash the renderer. Expect the
  // media stream count to be zero after the crash.
  frame->OnAudibleStateChanged(true);
  RunPostedTasks();
  EXPECT_EQ(1, process->get_media_stream_count_for_testing());
  RenderProcessHostWatcher crash_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0);
  crash_observer.Wait();
  RunPostedTasks();
  EXPECT_EQ(0, process->get_media_stream_count_for_testing());
}

}  // namespace content
