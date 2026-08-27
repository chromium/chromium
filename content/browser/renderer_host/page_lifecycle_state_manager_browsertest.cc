// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

class PageLifecycleStateManagerBrowserTest : public ContentBrowserTest {
 public:
  ~PageLifecycleStateManagerBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "VisibilityStateEntry");
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  void StartRecordingEvents(RenderFrameHostImpl* rfh) {
    EXPECT_TRUE(ExecJs(rfh, R"(
      window.testObservedEvents = [];
      let event_list = [
        'freeze',
        'resume',
        'visibilitychange',
      ];
      for (event_name of event_list) {
        let result = event_name;
        document.addEventListener(event_name, event => {
          window.testObservedEvents.push('document.' + result);
        });
      }
    )"));
  }

  void StartPerformanceObserver(RenderFrameHostImpl* rfh, int numEntries) {
    EXPECT_TRUE(ExecJs(rfh,
                       R"(
      window.performanceObserverEntries = [];
      window.performanceObserverPromise = new Promise(resolve => {
        new PerformanceObserver(entries => {
          entries.getEntries().forEach(e => {
            console.log(e.name + " " + e.startTime);
            window.performanceObserverEntries.push(e.name);
          });
          if (window.performanceObserverEntries.length === )" +
                           base::NumberToString(numEntries) + R"()
            resolve(true);
        }).observe({type: 'visibility-state', buffered: true});
      });
    )",
                       EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  }

  void MatchEventList(RenderFrameHostImpl* rfh,
                      base::Value list,
                      base::Location location = base::Location::Current()) {
    EXPECT_EQ(list, EvalJs(rfh, "window.testObservedEvents"))
        << location.ToString();
  }

  RenderViewHostImpl* render_view_host() {
    return static_cast<RenderViewHostImpl*>(
        shell()->web_contents()->GetPrimaryMainFrame()->GetRenderViewHost());
  }

  RenderFrameHostImpl* current_frame_host() {
    return static_cast<WebContentsImpl*>(shell()->web_contents())
        ->GetPrimaryFrameTree()
        .root()
        ->current_frame_host();
  }
};

IN_PROC_BROWSER_TEST_F(PageLifecycleStateManagerBrowserTest, SetFrozen) {
  EXPECT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  RenderFrameHostImpl* rfh = current_frame_host();
  StartRecordingEvents(rfh);

  // Hide and freeze the page.
  shell()->web_contents()->WasHidden();
  EXPECT_EQ(PageVisibilityState::kHidden, rfh->GetVisibilityState());
  shell()->web_contents()->SetPageFrozen(true);

  // Resume the page.
  shell()->web_contents()->SetPageFrozen(false);

  MatchEventList(rfh, ListValueOf("document.visibilitychange",
                                  "document.freeze", "document.resume"));
}

IN_PROC_BROWSER_TEST_F(PageLifecycleStateManagerBrowserTest, SetVisibility) {
  EXPECT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  RenderFrameHostImpl* rfh = current_frame_host();
  EXPECT_EQ(PageVisibilityState::kVisible, rfh->GetVisibilityState());
  StartRecordingEvents(rfh);
  StartPerformanceObserver(rfh, 2);

  // Hide the page.
  shell()->web_contents()->WasHidden();
  EXPECT_EQ(PageVisibilityState::kHidden, rfh->GetVisibilityState());

  MatchEventList(rfh, ListValueOf("document.visibilitychange"));

  EXPECT_EQ(true, EvalJs(rfh,
                         "(async () => { return await "
                         "window.performanceObserverPromise;})()"));
  EXPECT_EQ(ListValueOf("visible", "hidden"),
            EvalJs(rfh, "window.performanceObserverEntries"));
}

// TODO(crbug.com/40786254): Test is flaky on Win
#if BUILDFLAG(IS_WIN)
#define MAYBE_CrossProcessIframeHiddenAnFrozen \
  DISABLED_CrossProcessIframeHiddenAnFrozen
#else
#define MAYBE_CrossProcessIframeHiddenAnFrozen CrossProcessIframeHiddenAnFrozen
#endif
IN_PROC_BROWSER_TEST_F(PageLifecycleStateManagerBrowserTest,
                       MAYBE_CrossProcessIframeHiddenAnFrozen) {
  EXPECT_TRUE(embedded_test_server()->Start());
  // Load a page with a cross-process iframe.
  GURL url_a_b(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a_b));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  StartRecordingEvents(rfh_a);
  StartRecordingEvents(rfh_b);

  // Hide and freeze the page.
  shell()->web_contents()->WasHidden();
  EXPECT_EQ(PageVisibilityState::kHidden, rfh_a->GetVisibilityState());
  EXPECT_EQ(PageVisibilityState::kHidden, rfh_b->GetVisibilityState());

  shell()->web_contents()->SetPageFrozen(true);

  // Resume the page.
  shell()->web_contents()->SetPageFrozen(false);

  // Make sure that the cross-process iframe also got events.
  MatchEventList(rfh_a, ListValueOf("document.visibilitychange",
                                    "document.freeze", "document.resume"));
  MatchEventList(rfh_b, ListValueOf("document.visibilitychange",
                                    "document.freeze", "document.resume"));
}

IN_PROC_BROWSER_TEST_F(PageLifecycleStateManagerBrowserTest,
                       CreateIframeInHiddenPage) {
  EXPECT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/empty.html");

  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  RenderFrameHostImpl* rfh_parent = current_frame_host();

  // Hide the page.
  shell()->web_contents()->WasHidden();
  EXPECT_EQ(PageVisibilityState::kHidden, rfh_parent->GetVisibilityState());

  // Create an iframe in a hidden page.
  EXPECT_TRUE(ExecJs(rfh_parent, R"(
    let iframe = document.createElement('iframe');
    document.body.append(iframe);
  )"));
  ASSERT_EQ(1u, rfh_parent->child_count());

  // Make sure that the created iframe's initial visibility is correctly set.
  RenderFrameHostImpl* rfh_child =
      rfh_parent->child_at(0)->current_frame_host();
  EXPECT_EQ(PageVisibilityState::kHidden, rfh_child->GetVisibilityState());

  // Show the page.
  shell()->web_contents()->WasShown();
  EXPECT_EQ(PageVisibilityState::kVisible, rfh_parent->GetVisibilityState());
  EXPECT_EQ(PageVisibilityState::kVisible, rfh_child->GetVisibilityState());
}

IN_PROC_BROWSER_TEST_F(PageLifecycleStateManagerBrowserTest,
                       CreateNewWindowVisibilityChange) {
  if (!SiteIsolationPolicy::UseDedicatedProcessesForAllSites())
    return;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));

  // 1) Navigate to A and open a popup.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  EXPECT_EQ(1u, rfh_a->GetSiteInstance()->GetRelatedActiveContentsCount());
  Shell* popup = OpenPopup(rfh_a, url_a, "");
  EXPECT_EQ(2u, rfh_a->GetSiteInstance()->GetRelatedActiveContentsCount());

  RenderFrameHostImpl* popup_frame = static_cast<RenderFrameHostImpl*>(
      popup->web_contents()->GetPrimaryMainFrame());
  StartRecordingEvents(popup_frame);

  popup->web_contents()->WasHidden();
  EXPECT_EQ(PageVisibilityState::kHidden, popup_frame->GetVisibilityState());
  popup->web_contents()->WasShown();
  EXPECT_EQ(PageVisibilityState::kVisible, popup_frame->GetVisibilityState());

  MatchEventList(popup_frame, ListValueOf("document.visibilitychange",
                                          "document.visibilitychange"));
}

IN_PROC_BROWSER_TEST_F(PageLifecycleStateManagerBrowserTest,
                       MicrotaskRunnableDuringResumeEvent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  RenderFrameHostImpl* rfh = current_frame_host();
  // 1. Register a resume listener that schedules a microtask (Promise.then).
  // If the context is frozen during the event, the microtask will be blocked.
  ASSERT_TRUE(ExecJs(rfh, R"(
    window.resumeMicrotaskRan = false;
    document.addEventListener('resume', () => {
      Promise.resolve().then(() => {
        window.resumeMicrotaskRan = true;
      });
    });
  )",
                     EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // 2. Hide and freeze the page.
  shell()->web_contents()->WasHidden();
  EXPECT_EQ(PageVisibilityState::kHidden, rfh->GetVisibilityState());
  shell()->web_contents()->SetPageFrozen(true);

  // 3. Resume the page.
  shell()->web_contents()->SetPageFrozen(false);

  // 4. Assert that the microtask was allowed to run.
  // EvalJs will execute and implicitly run the microtask checkpoint if needed.
  EXPECT_EQ(true, EvalJs(rfh, "window.resumeMicrotaskRan"));
}

// Helper class to wait for the renderer to acknowledge a page freeze.
class FreezeWaiter : public PageLifecycleStateManager::TestDelegate {
 public:
  void OnLastAcknowledgedStateChanged(
      const blink::mojom::PageLifecycleState& old_state,
      const blink::mojom::PageLifecycleState& new_state) override {
    if (new_state.is_frozen && run_loop_) {
      run_loop_->Quit();
    }
  }
  void Wait() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
};

// Tests that opening and dismissing a JavaScript dialog in a renderer
// containing a frozen page with a PerformanceObserver does not crash due to
// redundant SuspendObserver calls during kPaused -> kFrozen transitions.
IN_PROC_BROWSER_TEST_F(PageLifecycleStateManagerBrowserTest,
                       PerformanceObserverFrozenPageDialog) {
  EXPECT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("a.com", "/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  RenderFrameHostImpl* rfh = current_frame_host();

  // Create a PerformanceObserver and generate a performance mark inside the
  // 'freeze' event listener so that the observer is active right when the page
  // transitions to frozen, moving it into suspended_observers_.
  EXPECT_TRUE(ExecJs(rfh, R"(
    window.observer = new PerformanceObserver(() => {});
    window.observer.observe({entryTypes: ['mark']});
    document.addEventListener('freeze', () => {
      performance.mark('mark_during_freeze');
    });
  )"));

  // Open a same-origin popup before freezing so both pages share the exact
  // same renderer process and Page::OrdinaryPages().
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(rfh, JsReplace("window.open($1);", test_url)));
  Shell* second_shell = new_shell_observer.GetShell();
  EXPECT_TRUE(WaitForLoadStop(second_shell->web_contents()));
  RenderFrameHostImpl* second_rfh =
      static_cast<WebContentsImpl*>(second_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root()
          ->current_frame_host();
  ASSERT_EQ(rfh->GetProcess(), second_rfh->GetProcess());

  FreezeWaiter freeze_waiter;
  static_cast<RenderViewHostImpl*>(rfh->GetRenderViewHost())
      ->GetPageLifecycleStateManager()
      ->SetDelegateForTesting(&freeze_waiter);

  // Hide and freeze the first page, waiting for the renderer ACK.
  shell()->web_contents()->WasHidden();
  shell()->web_contents()->SetPageFrozen(true);
  freeze_waiter.Wait();
  static_cast<RenderViewHostImpl*>(rfh->GetRenderViewHost())
      ->GetPageLifecycleStateManager()
      ->SetDelegateForTesting(nullptr);

  // Trigger a modal dialog (alert) in the second tab.
  // ScopedPagePauser pauses all pages in the renderer. When dismissed, it
  // unpauses all pages, transitioning the first (frozen) page from kPaused
  // back to kFrozen.
  //
  // AppModalDialogWaiter captures the dialog request and automatically
  // dismisses it across all platforms (preventing hangs on macOS/Windows where
  // ShellJavaScriptDialog shows a native modal UI dialog).
  AppModalDialogWaiter dialog_waiter(second_shell);
  EXPECT_TRUE(ExecJs(second_rfh, "alert('test');"));
  dialog_waiter.Wait();

  // Verify the renderer process and frames are still live and did not crash.
  EXPECT_TRUE(rfh->IsRenderFrameLive());
  EXPECT_TRUE(second_rfh->IsRenderFrameLive());
  EXPECT_TRUE(rfh->GetProcess()->IsInitializedAndNotDead());
}

}  // namespace content
