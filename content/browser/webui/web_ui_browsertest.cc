// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/fenced_frame/fenced_frame.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_entry_restore_context_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_ui_browsertest_util.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "ui/events/base_event_utils.h"
#include "ui/webui/untrusted_web_ui_browsertest_util.h"

namespace content {

namespace {

using WebUIImplBrowserTest = ContentBrowserTest;

// TODO(crbug.com/40290702): Shared workers are not available on Android.
#if !BUILDFLAG(IS_ANDROID)
const char kLoadSharedWorkerScript[] = R"(
    new Promise((resolve) => {
      const sharedWorker = new SharedWorker($1);
      sharedWorker.port.onmessage = (event) => {
        resolve(event.data === 'pong');
      };
      sharedWorker.port.postMessage('ping');
    });
  )";
#endif  // !BUILDFLAG(IS_ANDROID)

const char kLoadDedicatedWorkerScript[] = R"(
    new Promise((resolve) => {
      const worker = new Worker($1);
      worker.onmessage = (event) => {
        resolve(event.data === 'pong');
      };
      worker.postMessage('ping');
    });
  )";

class TestWebUIMessageHandler : public WebUIMessageHandler {
 public:
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "messageRequiringGesture",
        base::BindRepeating(&TestWebUIMessageHandler::OnMessageRequiringGesture,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "notifyFinish",
        base::BindRepeating(&TestWebUIMessageHandler::OnNotifyFinish,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "sendMessage",
        base::BindRepeating(&TestWebUIMessageHandler::OnSendMessage,
                            base::Unretained(this)));
  }

  void set_finish_closure(base::RepeatingClosure closure) {
    finish_closure_ = std::move(closure);
  }

  int message_requiring_gesture_count() const {
    return message_requiring_gesture_count_;
  }

  void set_send_message_closure(base::OnceClosure closure) {
    send_message_closure_ = std::move(closure);
  }

 private:
  void OnMessageRequiringGesture(const base::Value::List& args) {
    ++message_requiring_gesture_count_;
  }

  void OnNotifyFinish(const base::Value::List& args) {
    if (finish_closure_)
      finish_closure_.Run();
  }

  void OnSendMessage(const base::Value::List& args) {
    // This message will be invoked when WebContents changes the main RFH
    // and the old main RFH is still alive during navigating from WebUI page
    // to cross-site. WebUI message should be handled with old main RFH.

    if (send_message_closure_)
      std::move(send_message_closure_).Run();

    // AllowJavascript should not have a CHECK crash.
    AllowJavascript();

    // WebUI::CallJavascriptFunctionUnsafe should be run with old main RFH.
    web_ui()->CallJavascriptFunctionUnsafe("test");

    if (finish_closure_)
      std::move(finish_closure_).Run();
  }

  int message_requiring_gesture_count_ = 0;
  base::RepeatingClosure finish_closure_;
  base::OnceClosure send_message_closure_;
};

class WebUIRequiringGestureBrowserTest : public ContentBrowserTest {
 public:
  WebUIRequiringGestureBrowserTest() {
    clock_.SetNowTicks(base::TimeTicks::Now());
    ui::SetEventTickClockForTesting(&clock_);
  }

  ~WebUIRequiringGestureBrowserTest() override {
    ui::SetEventTickClockForTesting(nullptr);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(NavigateToURL(web_contents(), GetWebUIURL(kChromeUIGpuHost)));
    auto test_handler = std::make_unique<TestWebUIMessageHandler>();
    test_handler_ = test_handler.get();
    web_contents()->GetWebUI()->AddMessageHandler(std::move(test_handler));
  }
  void TearDownOnMainThread() override { test_handler_ = nullptr; }

 protected:
  void SendMessageAndWaitForFinish() {
    main_rfh()->ExecuteJavaScriptForTests(
        u"chrome.send('messageRequiringGesture');"
        u"chrome.send('notifyFinish');",
        base::NullCallback(), ISOLATED_WORLD_ID_GLOBAL);
    base::RunLoop run_loop;
    test_handler()->set_finish_closure(run_loop.QuitClosure());
    run_loop.Run();
  }

  void AdvanceClock(base::TimeDelta delta) { clock_.Advance(delta); }

  WebContents* web_contents() { return shell()->web_contents(); }
  RenderFrameHost* main_rfh() { return web_contents()->GetPrimaryMainFrame(); }

  TestWebUIMessageHandler* test_handler() { return test_handler_; }

 private:
  base::SimpleTestTickClock clock_;

  // Owned by the WebUI associated with the WebContents.
  raw_ptr<TestWebUIMessageHandler> test_handler_ = nullptr;
};

}  // namespace

// Tests that navigating between WebUIs of different types results in
// SiteInstance swap when running in process-per-tab process model.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest, ForceSwapOnDifferenteWebUITypes) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kProcessPerTab);
  WebContents* web_contents = shell()->web_contents();

  const GURL web_ui_url(GetWebUIURL(kChromeUIHistogramHost));
  EXPECT_TRUE(WebUIControllerFactoryRegistry::GetInstance()->UseWebUIForURL(
      web_contents->GetBrowserContext(), web_ui_url));
  ASSERT_TRUE(NavigateToURL(web_contents, web_ui_url));
  EXPECT_TRUE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      web_contents->GetPrimaryMainFrame()->GetProcess()->GetID()));

  // Capture the SiteInstance before navigating for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      web_contents->GetSiteInstance());
  auto orig_browsing_instance_id = orig_site_instance->GetBrowsingInstanceId();

  // Navigate to a different WebUI type and ensure that the SiteInstance
  // has changed and the new process also has WebUI bindings.
  const GURL web_ui_url2(GetWebUIURL(kChromeUIGpuHost));
  EXPECT_TRUE(WebUIConfigMap::GetInstance().GetConfig(
      web_contents->GetBrowserContext(), web_ui_url2));
  ASSERT_TRUE(NavigateToURL(web_contents, web_ui_url2));
  auto* new_site_instance = web_contents->GetSiteInstance();
  EXPECT_NE(orig_site_instance, new_site_instance);
  EXPECT_NE(orig_browsing_instance_id,
            new_site_instance->GetBrowsingInstanceId());
  EXPECT_TRUE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      web_contents->GetPrimaryMainFrame()->GetProcess()->GetID()));
}

// Tests that a WebUI page will stay in the initial RenderFrameHost and its
// SiteInstance when we navigate to it from the initial blank page.
//
// While WebUI navigations require a BrowsingInstance swap and hence typically
// force a RenderFrameHost swap, the initial RenderFrameHost is in an unused
// process and unassigned SiteInstance, and so it effectively satisfies the
// BrowsingInstance swap requirement. So, it should be reused without requiring
// an extra RenderFrameHost swap.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest,
                       ReuseInitialRenderFrameHostOnFirstNavigation) {
  WebContents* web_contents = shell()->web_contents();
  scoped_refptr<SiteInstance> orig_site_instance(
      web_contents->GetSiteInstance());
  EXPECT_FALSE(
      static_cast<SiteInstanceImpl*>(orig_site_instance.get())->HasSite());
  RenderFrameHostWrapper initial_rfh(
      shell()->web_contents()->GetPrimaryMainFrame());
  EXPECT_TRUE(initial_rfh->GetProcess()->IsUnused());

  // Navigate from the initial blank page to the WebUI URL.
  const GURL web_ui_url(GetWebUIURL(kChromeUIHistogramHost));
  EXPECT_TRUE(WebUIControllerFactoryRegistry::GetInstance()->UseWebUIForURL(
      web_contents->GetBrowserContext(), web_ui_url));
  ASSERT_TRUE(NavigateToURL(web_contents, web_ui_url));

  EXPECT_TRUE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      web_contents->GetPrimaryMainFrame()->GetProcess()->GetID()));
  auto* new_site_instance = web_contents->GetSiteInstance();
  EXPECT_EQ(orig_site_instance, new_site_instance);
  EXPECT_TRUE(
      static_cast<SiteInstanceImpl*>(orig_site_instance.get())->HasSite());
  EXPECT_EQ(initial_rfh.get(), shell()->web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(initial_rfh);
  EXPECT_FALSE(initial_rfh->GetProcess()->IsUnused());
}

// Check that starting a WebUI navigation while an about:blank navigation in an
// initial RenderFrameHost is pending commit does not crash.  See
// https://crbug.com/1492076.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest,
                       StartWebUINavigationWhileAboutBlankIsPendingCommit) {
  WebContents* web_contents = shell()->web_contents();
  RenderFrameHostImplWrapper initial_rfh(web_contents->GetPrimaryMainFrame());
  EXPECT_TRUE(initial_rfh->GetProcess()->IsUnused());
  EXPECT_FALSE(initial_rfh->has_committed_any_navigation());
  EXPECT_TRUE(initial_rfh->is_initial_empty_document());

  // Start an about:blank navigation, but don't wait for commit.  Since
  // about:blank doesn't run through the typical navigation throttles, it
  // proceeds straight to CommitNavigation, so when LoadURL() returns,
  // about:blank is pending commit and waiting for DidCommitNavigation.
  shell()->LoadURL(GURL(url::kAboutBlankURL));

  // Because has_committed_any_navigation() is modified in CommitNavigation(),
  // it should have become true now. is_initial_empty_document() is modified in
  // DidCommitNavigation, so it hasn't been updated yet.
  EXPECT_TRUE(initial_rfh->has_committed_any_navigation());
  EXPECT_TRUE(initial_rfh->is_initial_empty_document());

  // Now, start a navigation to a WebUI URL.  This used to crash in
  // https://crbug.com/1492076.
  const GURL web_ui_url(GetWebUIURL(kChromeUIHistogramHost));
  // Note: NavigateToURL() does a WaitForLoadStop() before starting a
  // navigation, so start the navigation via LoadURL instead.
  TestNavigationManager manager(web_contents, web_ui_url);
  shell()->LoadURL(web_ui_url);

  // After the WebUI navigation finishes, check that it didn't reuse the initial
  // RFH, since it was already considered to have a committed/committing
  // navigation (to about:blank) when the WebUI navigation started.
  EXPECT_TRUE(manager.WaitForNavigationFinished());
  RenderFrameHostImplWrapper final_rfh(web_contents->GetPrimaryMainFrame());
  EXPECT_NE(initial_rfh.get(), final_rfh.get());
  EXPECT_TRUE(final_rfh->has_committed_any_navigation());
  EXPECT_FALSE(final_rfh->is_initial_empty_document());
}

// Check that navigating to a WebUI page from a crashed about:blank page will
// work correctly, properly granting WebUI bindings and avoiding the reuse of
// the initial SiteInstance and process.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest, NavigateFromCrashedAboutBlank) {
  WebContents* web_contents = shell()->web_contents();
  scoped_refptr<SiteInstance> orig_site_instance(
      web_contents->GetSiteInstance());
  EXPECT_FALSE(
      static_cast<SiteInstanceImpl*>(orig_site_instance.get())->HasSite());
  RenderFrameHostImplWrapper initial_rfh(web_contents->GetPrimaryMainFrame());
  EXPECT_TRUE(initial_rfh->GetProcess()->IsUnused());
  EXPECT_TRUE(initial_rfh->is_initial_empty_document());
  EXPECT_FALSE(initial_rfh->has_committed_any_navigation());

  // Explicitly navigate to about:blank.  Note that this stays in the initial
  // RenderFrameHost but resets is_initial_empty_document() to false.
  ASSERT_TRUE(NavigateToURL(web_contents, GURL(url::kAboutBlankURL)));
  ASSERT_TRUE(initial_rfh.get());
  EXPECT_FALSE(initial_rfh->is_initial_empty_document());
  EXPECT_TRUE(initial_rfh->has_committed_any_navigation());
  EXPECT_TRUE(initial_rfh->GetProcess()->IsUnused());

  // Crash the initial process.  Note that this resets
  // has_committed_any_navigation(), but is_initial_empty_document() is still
  // false.
  RenderProcessHostWatcher crash_observer(
      initial_rfh->GetProcess(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(initial_rfh->GetProcess()->Shutdown(0));
  crash_observer.Wait();
  EXPECT_FALSE(initial_rfh->is_initial_empty_document());
  EXPECT_FALSE(initial_rfh->has_committed_any_navigation());

  // Navigate from the crashed blank page to a WebUI URL.
  const GURL web_ui_url(GetWebUIURL(kChromeUIHistogramHost));
  EXPECT_TRUE(WebUIControllerFactoryRegistry::GetInstance()->UseWebUIForURL(
      web_contents->GetBrowserContext(), web_ui_url));
  ASSERT_TRUE(NavigateToURL(web_contents, web_ui_url));

  // The explicit about:blank commit should prevent subsequent WebUI
  // navigations from reusing the current RenderFrameHost, as it's no longer
  // the initial one. Crashing the about:blank page shouldn't affect this, so
  // the WebUI navigation should end up in a fresh SiteInstance and process.
  EXPECT_NE(orig_site_instance, web_contents->GetSiteInstance());
  EXPECT_NE(orig_site_instance->GetProcess(),
            web_contents->GetPrimaryMainFrame()->GetProcess());

  // Check that the resulting WebUI page has bindings, and its process is
  // marked as used.
  EXPECT_TRUE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      web_contents->GetPrimaryMainFrame()->GetProcess()->GetID()));
  EXPECT_FALSE(web_contents->GetPrimaryMainFrame()->GetProcess()->IsUnused());
}

// Check that when over the process limit, a WebUI navigation in one tab does
// not reuse an initial RFH's unused process in another unrelated tab.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest,
                       DoNotReuseInitialRenderFrameHostForDifferentTab) {
  // Force all SiteInstances to always try to reuse an available process.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // Ensure the initial RenderFrameHost has an unused process to start with.
  WebContents* web_contents = shell()->web_contents();
  scoped_refptr<SiteInstance> orig_site_instance(
      web_contents->GetSiteInstance());
  RenderFrameHostWrapper initial_rfh(
      shell()->web_contents()->GetPrimaryMainFrame());
  EXPECT_TRUE(initial_rfh->GetProcess()->IsUnused());

  // Create a new WebUI window in an unrelated SiteInstance. When the new
  // SiteInstance obtains its process (as part of creating the initial RFH in
  // the new WebContents), it should *not* reuse the initial process from the
  // first window.  Despite being unused, that first process should only be
  // allowed to be used by navigations in `shell()` and not other windows.  In
  // practice, this matters for scenarios like chrome://*.top-chrome/ WebUI
  // URLs, which should all share a single process, rather than reusing
  // arbitrary unused processes from unrelated windows.
  const GURL web_ui_url(GetWebUIURL(kChromeUIHistogramHost));
  Shell* new_shell = Shell::CreateNewWindow(
      shell()->web_contents()->GetBrowserContext(), GURL(),
      SiteInstanceImpl::CreateForURL(
          shell()->web_contents()->GetBrowserContext(), web_ui_url),
      gfx::Size());
  ASSERT_TRUE(NavigateToURL(new_shell->web_contents(), web_ui_url));
  EXPECT_NE(initial_rfh->GetProcess(),
            new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());
  EXPECT_TRUE(initial_rfh->GetProcess()->IsUnused());

  // Create a third window and ensure it shares a process with the second
  // window, rather than reusing the still-unused process from the first window.
  Shell* new_shell2 = Shell::CreateNewWindow(
      shell()->web_contents()->GetBrowserContext(), GURL(),
      SiteInstanceImpl::CreateForURL(
          shell()->web_contents()->GetBrowserContext(), web_ui_url),
      gfx::Size());
  ASSERT_TRUE(NavigateToURL(new_shell2->web_contents(), web_ui_url));
  EXPECT_NE(initial_rfh->GetProcess(),
            new_shell2->web_contents()->GetPrimaryMainFrame()->GetProcess());
  EXPECT_EQ(new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess(),
            new_shell2->web_contents()->GetPrimaryMainFrame()->GetProcess());
  EXPECT_TRUE(initial_rfh->GetProcess()->IsUnused());
}

// Check that if two initial RenderFrameHosts in different tabs share a
// process, and that process becomes locked to a normal web site after a
// navigation in the first tab, then that process should not be reused for a
// WebUI navigation in the second tab, even though it's still the initial
// RFH's process in that tab.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest,
                       DoNotReuseInitialRenderFrameHostWithUsedProcess) {
  // Force all SiteInstances to always try to reuse an available process.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // The test starts with an unused process in the initial RFH.
  EXPECT_TRUE(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->IsUnused());

  // Create a new tab with an unassigned SiteInstance.  That SiteInstance
  // should reuse the available unused process from the first tab.
  Shell* new_shell = Shell::CreateNewWindow(
      shell()->web_contents()->GetBrowserContext(), GURL(),
      SiteInstanceImpl::Create(shell()->web_contents()->GetBrowserContext()),
      gfx::Size());
  EXPECT_EQ(shell()->web_contents()->GetPrimaryMainFrame()->GetProcess(),
            new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());
  EXPECT_TRUE(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->IsUnused());

  // Navigate first tab to a normal web URL.  This should mark the corresponding
  // process as used, but the process is still shared with new_shell's RFH.
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(
      NavigateToURL(shell()->web_contents(),
                    embedded_test_server()->GetURL("/simple_page.html")));
  EXPECT_FALSE(new_shell->web_contents()
                   ->GetPrimaryMainFrame()
                   ->GetProcess()
                   ->IsUnused());
  EXPECT_EQ(shell()->web_contents()->GetPrimaryMainFrame()->GetProcess(),
            new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());
  EXPECT_FALSE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID()));

  // Navigate the second tab to a WebUI URL.  This should not reuse the
  // initial RFH's process and should end up in a new process.
  const GURL web_ui_url(GetWebUIURL(kChromeUIHistogramHost));
  ASSERT_TRUE(NavigateToURL(new_shell->web_contents(), web_ui_url));
  EXPECT_NE(shell()->web_contents()->GetPrimaryMainFrame()->GetProcess(),
            new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());
  EXPECT_TRUE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID()));
}

// Check that if two initial RenderFrameHosts in different tabs share a
// process, then that process can be reused for a WebUI navigation in one of
// these tabs.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest,
                       ReuseInitialProcessSharedByMultipleTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Force all SiteInstances to always try to reuse an available process.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // The test starts with an unused process in the initial RFH.
  EXPECT_TRUE(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->IsUnused());

  // Create a new tab with an unassigned SiteInstance.  That SiteInstance
  // should reuse the available unused process from the first tab.
  Shell* new_shell = Shell::CreateNewWindow(
      shell()->web_contents()->GetBrowserContext(), GURL(),
      SiteInstanceImpl::Create(shell()->web_contents()->GetBrowserContext()),
      gfx::Size());
  EXPECT_EQ(shell()->web_contents()->GetPrimaryMainFrame()->GetProcess(),
            new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());
  EXPECT_TRUE(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->IsUnused());

  // Create a third tab, also with an unassigned SiteInstance and also reusing
  // the first tab's initial process.
  Shell* third_shell = Shell::CreateNewWindow(
      shell()->web_contents()->GetBrowserContext(), GURL(),
      SiteInstanceImpl::Create(shell()->web_contents()->GetBrowserContext()),
      gfx::Size());
  EXPECT_EQ(shell()->web_contents()->GetPrimaryMainFrame()->GetProcess(),
            third_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());

  // Navigate the second tab to a WebUI URL.  This should reuse the
  // initial process, lock it to the WebUI site, and mark it as used.
  const GURL web_ui_url(GetWebUIURL(kChromeUIHistogramHost));
  ASSERT_TRUE(NavigateToURL(new_shell->web_contents(), web_ui_url));
  EXPECT_EQ(shell()->web_contents()->GetPrimaryMainFrame()->GetProcess(),
            new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());
  EXPECT_FALSE(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->IsUnused());
  EXPECT_TRUE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID()));

  // Navigate the first tab to a normal web URL.  This should not stay in the
  // the initial process, which is now used by WebUI.
  EXPECT_TRUE(
      NavigateToURL(shell()->web_contents(),
                    embedded_test_server()->GetURL("/simple_page.html")));
  EXPECT_NE(shell()->web_contents()->GetPrimaryMainFrame()->GetProcess(),
            new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());
  EXPECT_FALSE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID()));
  EXPECT_TRUE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID()));

  // Navigate the third tab to the same WebUI URL.  This should stay in the
  // third tab's initial process which is shared with the second tab, since the
  // second and third tab's WebUI URLs are same-site.
  ASSERT_TRUE(NavigateToURL(third_shell->web_contents(), web_ui_url));
  EXPECT_EQ(new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess(),
            third_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());
}

// Check that when a tab with a WebUI page is cloned, the new tab reuses the old
// tab's SiteInstance and process when it loads its copy of the WebUI page.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest, ReuseProcessInClonedTab) {
  // Load a normal page and then a WebUI page in the initial tab.
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(
      NavigateToURL(shell()->web_contents(),
                    embedded_test_server()->GetURL("/simple_page.html")));

  const GURL web_ui_url(GetWebUIURL(kChromeUIHistogramHost));
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), web_ui_url));

  // Clone the tab with these two NavigationEntries.
  std::unique_ptr<WebContents> cloned_tab = shell()->web_contents()->Clone();
  WebContentsImpl* cloned_tab_impl =
      static_cast<WebContentsImpl*>(cloned_tab.get());
  NavigationController& new_controller = cloned_tab_impl->GetController();
  EXPECT_TRUE(new_controller.IsInitialNavigation());
  EXPECT_TRUE(new_controller.NeedsReload());
  EXPECT_EQ(2, new_controller.GetEntryCount());
  EXPECT_EQ(1, new_controller.GetLastCommittedEntryIndex());

  // The cloned WebContents will use the old tab's current SiteInstance for its
  // initial RFH.  That means its initial SiteInstance should already have a
  // site, and it should keep the same process (from the old tab).
  // TODO(crbug.com/40277187): these expectations may change in the future if
  // duplicating tabs stops inheriting the old tab's SiteInstance.
  EXPECT_EQ(shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance(),
            cloned_tab->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_TRUE(
      cloned_tab_impl->GetPrimaryMainFrame()->GetSiteInstance()->HasSite());
  EXPECT_EQ(shell()->web_contents()->GetPrimaryMainFrame()->GetProcess(),
            cloned_tab->GetPrimaryMainFrame()->GetProcess());
  EXPECT_FALSE(
      cloned_tab_impl->GetPrimaryMainFrame()->GetProcess()->IsUnused());

  // Load the cloned tab.  This should reuse the old tab's WebUI process.
  // TODO(crbug.com/40277187): this expectation may change in the future if
  // duplicating tabs stops inheriting the old tab's SiteInstance.
  {
    TestNavigationObserver clone_observer(cloned_tab_impl);
    new_controller.LoadIfNecessary();
    clone_observer.Wait();
  }
  EXPECT_EQ(shell()->web_contents()->GetPrimaryMainFrame()->GetProcess(),
            cloned_tab->GetPrimaryMainFrame()->GetProcess());
}

// Check that doing a session restore of a WebUI NavigationEntry in a new tab
// can reuse the new tab's initial RFH.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest, ReuseInitialRFHInRestoredTab) {
  //  Load a WebUI URL.
  const GURL web_ui_url(GetWebUIURL(kChromeUIHistogramHost));
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), web_ui_url));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();

  // Create a NavigationEntry with the same PageState as the current entry.
  std::unique_ptr<NavigationEntryImpl> restored_entry =
      NavigationEntryImpl::FromNavigationEntry(
          NavigationController::CreateNavigationEntry(
              web_ui_url, Referrer(), std::nullopt /* initiator_origin= */,
              /* initiator_base_url= */ std::nullopt,
              ui::PAGE_TRANSITION_RELOAD, false, std::string(),
              controller.GetBrowserContext(),
              nullptr /* blob_url_loader_factory */));
  std::unique_ptr<NavigationEntryRestoreContextImpl> context =
      std::make_unique<NavigationEntryRestoreContextImpl>();
  restored_entry->SetPageState(entry->GetPageState(), context.get());

  // Create a new shell for session restore. At this point, since we haven't
  // loaded anything yet, the restored shell's initial RFH should still be in an
  // unassigned SiteInstance and an unused process.
  Shell* restore_shell = Shell::CreateNewWindow(controller.GetBrowserContext(),
                                                GURL(), nullptr, gfx::Size());
  WebContentsImpl* restore_contents =
      static_cast<WebContentsImpl*>(restore_shell->web_contents());
  RenderFrameHostWrapper restore_rfh(restore_contents->GetPrimaryMainFrame());
  scoped_refptr<SiteInstance> restore_site_instance(
      restore_rfh->GetSiteInstance());
  EXPECT_TRUE(restore_rfh->GetProcess()->IsUnused());
  EXPECT_FALSE(
      static_cast<SiteInstanceImpl*>(restore_site_instance.get())->HasSite());
  EXPECT_FALSE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      restore_rfh->GetProcess()->GetID()));
  EXPECT_NE(shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance(),
            restore_site_instance);

  // Restore and load the WebUI entry in the new shell.
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  entries.push_back(std::move(restored_entry));
  NavigationControllerImpl& restore_controller =
      restore_contents->GetController();
  restore_controller.Restore(entries.size() - 1, RestoreType::kRestored,
                             &entries);
  ASSERT_EQ(0u, entries.size());
  EXPECT_EQ(1, restore_controller.GetEntryCount());
  {
    TestNavigationObserver restore_observer(restore_contents);
    restore_controller.LoadIfNecessary();
    restore_observer.Wait();
  }
  // The initial RFH should be reused by the restored WebUI navigation, its
  // SiteInstance should now have a site, and its process should now be marked
  // as used and gain WebUI bindings.  Note that SiteInstances and
  // BrowsingInstances are not currently persisted in session history, so this
  // does not load in the original tab's SiteInstance and BrowsingInstance. This
  // could change in the future.
  EXPECT_EQ(restore_contents->GetPrimaryMainFrame()->GetSiteInstance(),
            restore_site_instance);
  EXPECT_TRUE(
      static_cast<SiteInstanceImpl*>(restore_site_instance.get())->HasSite());
  EXPECT_EQ(restore_contents->GetPrimaryMainFrame(), restore_rfh.get());
  ASSERT_TRUE(restore_rfh.get());
  EXPECT_FALSE(restore_rfh->GetProcess()->IsUnused());
  EXPECT_TRUE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      restore_rfh->GetProcess()->GetID()));
}

// Tests that navigating from chrome:// to chrome-untrusted:// results in
// SiteInstance swap.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest, ForceSwapOnFromChromeToUntrusted) {
  WebContents* web_contents = shell()->web_contents();
  content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test-host"));

  const GURL web_ui_url(GetWebUIURL(kChromeUIHistogramHost));
  EXPECT_TRUE(WebUIControllerFactoryRegistry::GetInstance()->UseWebUIForURL(
      web_contents->GetBrowserContext(), web_ui_url));

  ASSERT_TRUE(NavigateToURL(web_contents, web_ui_url));
  EXPECT_TRUE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      web_contents->GetPrimaryMainFrame()->GetProcess()->GetID()));

  // Capture the SiteInstance before navigating for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      web_contents->GetSiteInstance());
  auto orig_browsing_instance_id = orig_site_instance->GetBrowsingInstanceId();

  // Navigate to chrome-untrusted:// and ensure that the SiteInstance
  // has changed and the new process has no WebUI bindings.
  ASSERT_TRUE(NavigateToURL(web_contents,
                            GetChromeUntrustedUIURL("test-host/title1.html")));
  auto* new_site_instance = web_contents->GetSiteInstance();
  EXPECT_NE(orig_site_instance, new_site_instance);
  EXPECT_NE(orig_browsing_instance_id,
            new_site_instance->GetBrowsingInstanceId());
  EXPECT_FALSE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      web_contents->GetPrimaryMainFrame()->GetProcess()->GetID()));
}

// Tests that navigating from chrome-untrusted:// to chrome:// results in
// SiteInstance swap.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest, ForceSwapOnFromUntrustedToChrome) {
  WebContents* web_contents = shell()->web_contents();
  content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test-host"));

  ASSERT_TRUE(NavigateToURL(web_contents,
                            GetChromeUntrustedUIURL("test-host/title1.html")));
  EXPECT_FALSE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      web_contents->GetPrimaryMainFrame()->GetProcess()->GetID()));

  // Capture the SiteInstance before navigating for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      web_contents->GetSiteInstance());
  auto orig_browsing_instance_id = orig_site_instance->GetBrowsingInstanceId();

  // Navigate to a WebUI and ensure that the SiteInstance has changed and the
  // new process has WebUI bindings.
  const GURL web_ui_url(GetWebUIURL(kChromeUIHistogramHost));
  EXPECT_TRUE(WebUIControllerFactoryRegistry::GetInstance()->UseWebUIForURL(
      web_contents->GetBrowserContext(), web_ui_url));

  ASSERT_TRUE(NavigateToURL(web_contents, web_ui_url));
  auto* new_site_instance = web_contents->GetSiteInstance();
  EXPECT_NE(orig_site_instance, new_site_instance);
  EXPECT_NE(orig_browsing_instance_id,
            new_site_instance->GetBrowsingInstanceId());
  EXPECT_TRUE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      web_contents->GetPrimaryMainFrame()->GetProcess()->GetID()));
}

IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest, SameDocumentNavigationsAndReload) {
  auto* web_contents = shell()->web_contents();
  ASSERT_TRUE(NavigateToURL(web_contents, GetWebUIURL(kChromeUIHistogramHost)));

  auto owned_test_handler = std::make_unique<TestWebUIMessageHandler>();
  auto* test_handler = owned_test_handler.get();
  web_contents->GetWebUI()->AddMessageHandler(std::move(owned_test_handler));
  test_handler->AllowJavascriptForTesting();

  // Push onto window.history. Back should now be an in-page navigation.
  ASSERT_TRUE(
      ExecJs(web_contents,
             "window.history.pushState({}, '', location.href + '#foo')"));
  shell()->GoBackOrForward(-1);
  EXPECT_TRUE(WaitForLoadStop(web_contents));

  // Test handler should still have JavaScript allowed after in-page navigation.
  EXPECT_TRUE(test_handler->IsJavascriptAllowed());

  shell()->Reload();
  EXPECT_TRUE(WaitForLoadStop(web_contents));

  // Verify that after a reload, the test handler has been disallowed.
  if (!ShouldCreateNewHostForAllFrames()) {
    EXPECT_FALSE(test_handler->IsJavascriptAllowed());
  } else {
    // If the RenderFrameHost and WebUI objects changed after navigation, the
    // `TestWebUIMessageHandler` will point to a stale WebUI and we can't check
    // the `IsJavascriptAllowed()` value there. So use a new handler here and
    // check the value on the new handler instead.
    auto owned_test_handler2 = std::make_unique<TestWebUIMessageHandler>();
    auto* test_handler2 = owned_test_handler2.get();
    web_contents->GetWebUI()->AddMessageHandler(std::move(owned_test_handler2));
    EXPECT_FALSE(test_handler2->IsJavascriptAllowed());
  }
}

// A WebUI message that should require a user gesture is ignored if there is no
// recent input event.
IN_PROC_BROWSER_TEST_F(WebUIRequiringGestureBrowserTest,
                       MessageRequiringGestureIgnoredIfNoGesture) {
  SendMessageAndWaitForFinish();
  EXPECT_EQ(0, test_handler()->message_requiring_gesture_count());
}

IN_PROC_BROWSER_TEST_F(WebUIRequiringGestureBrowserTest,
                       MessageRequiringGestureIgnoresRendererOnlyGesture) {
  // Note: this doesn't use SendMessageAndWaitForFinish() since this test needs
  // to use a test-only helper to instantiate a scoped user gesture in the
  // renderer.
  main_rfh()->ExecuteJavaScriptWithUserGestureForTests(
      u"chrome.send('messageRequiringGesture');"
      u"chrome.send('notifyFinish');",
      base::NullCallback(), ISOLATED_WORLD_ID_GLOBAL);
  base::RunLoop run_loop;
  test_handler()->set_finish_closure(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(0, test_handler()->message_requiring_gesture_count());
}

IN_PROC_BROWSER_TEST_F(WebUIRequiringGestureBrowserTest,
                       MessageRequiringGestureIgnoresNonInteractiveEvents) {
  // Mouse enter / mouse move / mouse leave should not be considered input
  // events that interact with the page.
  content::SimulateMouseEvent(web_contents(),
                              blink::WebInputEvent::Type::kMouseEnter,
                              gfx::Point(50, 50));
  content::SimulateMouseEvent(web_contents(),
                              blink::WebInputEvent::Type::kMouseMove,
                              gfx::Point(50, 50));
  content::SimulateMouseEvent(web_contents(),
                              blink::WebInputEvent::Type::kMouseLeave,
                              gfx::Point(50, 50));
  // Nor should mouse wheel.
  content::SimulateMouseWheelEvent(web_contents(), gfx::Point(50, 50),
                                   gfx::Vector2d(0, 100),
                                   blink::WebMouseWheelEvent::kPhaseBegan);
  SendMessageAndWaitForFinish();
  EXPECT_EQ(0, test_handler()->message_requiring_gesture_count());
}

IN_PROC_BROWSER_TEST_F(
    WebUIRequiringGestureBrowserTest,
    // TODO(crbug.com/40851657): Re-enable this test
    DISABLED_MessageRequiringGestureAllowedWithInteractiveEvent) {
  // Simulate a click at Now.
  content::SimulateMouseClick(web_contents(), 0,
                              blink::WebMouseEvent::Button::kLeft);

  // Now+0 should be allowed.
  SendMessageAndWaitForFinish();
  EXPECT_EQ(1, test_handler()->message_requiring_gesture_count());

  // Now+5 seconds should be allowed.
  AdvanceClock(base::Seconds(5));
  SendMessageAndWaitForFinish();
  EXPECT_EQ(2, test_handler()->message_requiring_gesture_count());

  // Anything after that should be disallowed though.
  AdvanceClock(base::Microseconds(1));
  SendMessageAndWaitForFinish();
  EXPECT_EQ(2, test_handler()->message_requiring_gesture_count());
}

// Verify that we can successfully navigate to a chrome-untrusted:// URL.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest, UntrustedSchemeLoads) {
  content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test-host"));

  const GURL untrusted_url(GetChromeUntrustedUIURL("test-host/title2.html"));
  auto* web_contents = shell()->web_contents();
  EXPECT_TRUE(NavigateToURL(web_contents, untrusted_url));
  EXPECT_EQ(u"Title Of Awesomeness", web_contents->GetTitle());
}

// Verify that we can successfully navigate to a chrome-untrusted:// URL
// without a crash while WebUI::Send is being performed.
// TODO(crbug.com/40773523): Enable this test once a root cause is identified.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest, DISABLED_NavigateWhileWebUISend) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto* web_contents = shell()->web_contents();
  ASSERT_TRUE(NavigateToURL(web_contents, GetWebUIURL(kChromeUIGpuHost)));

  auto owned_test_handler = std::make_unique<TestWebUIMessageHandler>();
  auto* test_handler = owned_test_handler.get();
  web_contents->GetWebUI()->AddMessageHandler(std::move(owned_test_handler));

  auto* webui = static_cast<WebUIImpl*>(web_contents->GetWebUI());
  EXPECT_EQ(web_contents->GetPrimaryMainFrame(), webui->GetRenderFrameHost());

  test_handler->set_finish_closure(base::BindLambdaForTesting([&]() {
    EXPECT_NE(web_contents->GetPrimaryMainFrame(), webui->GetRenderFrameHost());
  }));

  bool received_send_message = false;
  test_handler->set_send_message_closure(
      base::BindLambdaForTesting([&]() { received_send_message = true; }));

  base::RunLoop run_loop;
  web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"onunload=function() { chrome.send('sendMessage')}",
      base::BindOnce([](base::OnceClosure callback,
                        base::Value) { std::move(callback).Run(); },
                     run_loop.QuitClosure()),
      ISOLATED_WORLD_ID_GLOBAL);
  run_loop.Run();

  RenderFrameDeletedObserver delete_observer(
      web_contents->GetPrimaryMainFrame());
  EXPECT_TRUE(NavigateToURL(
      web_contents, embedded_test_server()->GetURL("/simple_page.html")));
  delete_observer.WaitUntilDeleted();

  EXPECT_TRUE(received_send_message);
}

IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest, CoopCoepPolicies) {
  auto* web_contents = shell()->web_contents();

  TestUntrustedDataSourceHeaders headers;
  headers.cross_origin_opener_policy =
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep;
  content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::make_unique<ui::TestUntrustedWebUIConfig>("isolated", headers));

  const GURL isolated_url(GetChromeUntrustedUIURL("isolated/title2.html"));
  ASSERT_TRUE(NavigateToURL(web_contents, isolated_url));

  auto* main_frame = web_contents->GetPrimaryMainFrame();
  EXPECT_EQ(true, EvalJs(main_frame, "window.crossOriginIsolated;",
                         EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
}

// Regression test for: https://crbug.com/1308391
// Check content/ supports its embedders closing WebContent during WebUI
// destruction, after the RenderFrameHost owning it has unloaded.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest,
                       SynchronousWebContentDeletionInUnload) {
  static std::unique_ptr<WebContents> web_contents;
  web_contents = WebContents::Create(
      WebContents::CreateParams(shell()->web_contents()->GetBrowserContext()));
  // Install a WebUI. When destroyed, it executes a callback releasing the
  // WebContent.
  class Config : public WebUIConfig {
   public:
    Config() : WebUIConfig(kChromeUIUntrustedScheme, "test-host") {}
    std::unique_ptr<WebUIController> CreateWebUIController(
        WebUI* web_ui,
        const GURL& url) final {
      class Controller : public WebUIController {
       public:
        explicit Controller(WebUI* web_ui) : WebUIController(web_ui) {
          AddUntrustedDataSource(web_contents->GetBrowserContext(),
                                 "test-host");
        }
        ~Controller() override { web_contents.reset(); }
      };
      return std::make_unique<Controller>(web_ui);
    }
  };

  content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::make_unique<Config>());
  ASSERT_TRUE(NavigateToURL(web_contents.get(),
                            GetChromeUntrustedUIURL("test-host/title1.html")));
  RenderFrameHost* main_rfh = web_contents->GetPrimaryMainFrame();
  RenderFrameDeletedObserver rfh_deleted(web_contents->GetPrimaryMainFrame());
  RenderFrameDeletedObserver delete_observer(main_rfh);
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL dummy_url = embedded_test_server()->GetURL("/simple_page.html");
  web_contents->GetController().LoadURL(
      dummy_url, content::Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  delete_observer.WaitUntilDeleted();
  ASSERT_FALSE(web_contents);
}

class WebUIRequestSchemesTest : public ContentBrowserTest {
 public:
  WebUIRequestSchemesTest() = default;

  WebUIRequestSchemesTest(const WebUIRequestSchemesTest&) = delete;

  WebUIRequestSchemesTest& operator=(const WebUIRequestSchemesTest&) = delete;

  TestWebUIControllerFactory* factory() { return &factory_; }

 private:
  TestWebUIControllerFactory factory_;
  ScopedWebUIControllerFactoryRegistration factory_registration_{&factory_};
};

// Verify that by default WebUI's child process security policy can request
// default schemes such as chrome.
//
// ChildProcessSecurityPolicy::CanRequestURL() always returns true for the
// following schemes, but in practice there are other checks that stop WebUIs
// from accessing these schemes.
IN_PROC_BROWSER_TEST_F(WebUIRequestSchemesTest, DefaultSchemesCanBeRequested) {
  auto* web_contents = shell()->web_contents();

  std::string host_and_path = "test-host/title2.html";
  const GURL chrome_url(GetWebUIURL(host_and_path));
  GURL url;

  std::vector<std::string> requestable_schemes = {
      // WebSafe Schemes:
      url::kHttpScheme, url::kHttpsScheme, url::kDataScheme, url::kWsScheme,
      url::kWssScheme,
      // Default added as requestable schemes:
      url::kFileScheme, kChromeUIScheme};

  std::vector<std::string> unrequestable_schemes = {
      kChromeDevToolsScheme, url::kBlobScheme, kChromeUIUntrustedScheme,
      base::StrCat({url::kFileSystemScheme, ":", kChromeUIUntrustedScheme})};

  ASSERT_TRUE(NavigateToURL(web_contents, chrome_url));

  for (const auto& requestable_scheme : requestable_schemes) {
    url = GURL(base::StrCat(
        {requestable_scheme, url::kStandardSchemeSeparator, host_and_path}));
    EXPECT_TRUE(ChildProcessSecurityPolicy::GetInstance()->CanRequestURL(
        web_contents->GetPrimaryMainFrame()->GetProcess()->GetID(), url));
  }

  for (const auto& unrequestable_scheme : unrequestable_schemes) {
    url = GURL(base::StrCat(
        {unrequestable_scheme, url::kStandardSchemeSeparator, host_and_path}));
    EXPECT_FALSE(ChildProcessSecurityPolicy::GetInstance()->CanRequestURL(
        web_contents->GetPrimaryMainFrame()->GetProcess()->GetID(), url));
  }
}

// Verify that we can successfully allow non-default URL schemes to
// be requested by the WebUI's child process security policy.
IN_PROC_BROWSER_TEST_F(WebUIRequestSchemesTest,
                       AllowAdditionalSchemesToBeRequested) {
  auto* web_contents = shell()->web_contents();

  std::string host_and_path = "test-host/title2.html";
  GURL url;

  // All URLs with a web safe scheme, or with a scheme not
  // handled by ContentBrowserClient are requestable. All other schemes are
  // not requestable.
  std::vector<std::string> requestable_schemes = {
      // WebSafe schemes:
      url::kHttpScheme,
      url::kHttpsScheme,
      url::kDataScheme,
      url::kWsScheme,
      url::kWssScheme,
      // Default added as requestable schemes:
      "file",
      kChromeUIScheme,
      // Schemes given requestable access:
      kChromeUIUntrustedScheme,
      base::StrCat({url::kFileSystemScheme, ":", kChromeUIUntrustedScheme}),
  };
  std::vector<std::string> unrequestable_schemes = {
      kChromeDevToolsScheme, url::kBlobScheme,
      base::StrCat({url::kFileSystemScheme, ":", kChromeDevToolsScheme})};

  const GURL chrome_ui_url = GetWebUIURL(base::StrCat(
      {host_and_path, "?requestableschemes=", kChromeUIUntrustedScheme, ",",
       url::kWsScheme}));

  ASSERT_TRUE(NavigateToURL(web_contents, chrome_ui_url));

  for (const auto& requestable_scheme : requestable_schemes) {
    url = GURL(base::StrCat(
        {requestable_scheme, url::kStandardSchemeSeparator, host_and_path}));
    EXPECT_TRUE(ChildProcessSecurityPolicy::GetInstance()->CanRequestURL(
        web_contents->GetPrimaryMainFrame()->GetProcess()->GetID(), url));
  }

  for (const auto& unrequestable_scheme : unrequestable_schemes) {
    url = GURL(base::StrCat(
        {unrequestable_scheme, url::kStandardSchemeSeparator, host_and_path}));
    EXPECT_FALSE(ChildProcessSecurityPolicy::GetInstance()->CanRequestURL(
        web_contents->GetPrimaryMainFrame()->GetProcess()->GetID(), url));
  }
}

class WebUIWorkerTest : public ContentBrowserTest {
 public:
  WebUIWorkerTest() = default;

  WebUIWorkerTest(const WebUIWorkerTest&) = delete;

  WebUIWorkerTest& operator=(const WebUIWorkerTest&) = delete;

 protected:
  void SetUntrustedWorkerSrcToWebUIConfig(bool allow_embedded_frame) {
    TestUntrustedDataSourceHeaders headers;

    if (allow_embedded_frame) {
      // Allow the frame to be embedded in the chrome main page.
      headers.frame_ancestors.emplace().push_back("chrome://trusted");
    }

    // These two lines are to avoid:
    // "TypeError: Failed to construct 'SharedWorker': This document requires
    // 'TrustedScriptURL' assignment."
    headers.script_src = "worker-src chrome-untrusted://untrusted;";
    headers.no_trusted_types = true;

    content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
        std::make_unique<ui::TestUntrustedWebUIConfig>("untrusted", headers));
    if (allow_embedded_frame) {
      AddUntrustedDataSource(shell()->web_contents()->GetBrowserContext(),
                             "untrusted", headers);
    }
  }

  EvalJsResult RunWorkerTest(const GURL& page_url,
                             const GURL& worker_url,
                             const std::string& worker_script) {
    auto* web_contents = shell()->web_contents();
    EXPECT_TRUE(NavigateToURL(web_contents, page_url));

    return EvalJs(web_contents,
                  JsReplace(worker_script, worker_url.spec().c_str()),
                  EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */);
  }

 private:
  TestWebUIControllerFactory factory_;
  content::ScopedWebUIControllerFactoryRegistration factory_registration_{
      &factory_};
};

class WebUIDedicatedWorkerTest : public WebUIWorkerTest,
                                 public testing::WithParamInterface<bool> {
 public:
  WebUIDedicatedWorkerTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(blink::features::kPlzDedicatedWorker);
    } else {
      feature_list_.InitAndDisableFeature(blink::features::kPlzDedicatedWorker);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, WebUIDedicatedWorkerTest, testing::Bool());

// TODO(crbug.com/40290702): Shared workers are not available on Android.
#if !BUILDFLAG(IS_ANDROID)
// Verify that we can create SharedWorker with scheme "chrome://" under
// WebUI page.
IN_PROC_BROWSER_TEST_F(WebUIWorkerTest, CanCreateWebUISharedWorkerForWebUI) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(true, RunWorkerTest(
                      GetWebUIURL("test-host/title2.html?notrustedtypes=true"),
                      GetWebUIURL("test-host/web_ui_shared_worker.js"),
                      kLoadSharedWorkerScript));
}

// Verify that pages with scheme other than "chrome://" cannot create
// SharedWorker with scheme "chrome://".
IN_PROC_BROWSER_TEST_F(WebUIWorkerTest,
                       CannotCreateWebUISharedWorkerForNonWebUI) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EvalJsResult result = RunWorkerTest(
      embedded_test_server()->GetURL("/title1.html?notrustedtypes=true"),
      GetWebUIURL("test-host/web_ui_shared_worker.js"),
      kLoadSharedWorkerScript);

  std::string expected_failure =
      "a JavaScript error: \"SecurityError: Failed to construct 'SharedWorker'";
  EXPECT_THAT(result.error, ::testing::StartsWith(expected_failure));
}

// Test that we can start a Shared Worker from a chrome-untrusted:// iframe.
IN_PROC_BROWSER_TEST_F(WebUIWorkerTest,
                       CanCreateSharedWorkerFromUntrustedIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = shell()->web_contents();

  SetUntrustedWorkerSrcToWebUIConfig(/*allow_embedded_frame=*/true);

  // Set up the urls.
  const GURL web_ui_url(
      GetWebUIURL("trusted/"
                  "title2.html?notrustedtypes=true&requestableschemes=chrome-"
                  "untrusted&childsrc="));
  const GURL untrusted_iframe_url(
      GetChromeUntrustedUIURL("untrusted/title1.html"));
  const GURL untrusted_worker_url(
      GetChromeUntrustedUIURL("untrusted/web_ui_shared_worker.js"));

  // Navigate to a chrome:// main page.
  EXPECT_TRUE(NavigateToURL(web_contents, web_ui_url));
  auto* main_frame = web_contents->GetPrimaryMainFrame();
  // Add an iframe in chrome-untrusted://.
  EXPECT_EQ(true,
            EvalJs(main_frame,
                   JsReplace("var frame = document.createElement('iframe');\n"
                             "frame.src=$1;\n"
                             "!!document.body.appendChild(frame);\n",
                             untrusted_iframe_url.spec().c_str()),
                   EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
  EXPECT_TRUE(WaitForLoadStop(web_contents));

  // Get the chrome-untrusted:// iframe.
  RenderFrameHost* child = ChildFrameAt(main_frame, 0);
  EXPECT_EQ(untrusted_iframe_url, child->GetLastCommittedURL());

  // Start a shared worker from the chrome-untrusted iframe.
  EXPECT_EQ(true, EvalJs(child,
                         JsReplace(kLoadSharedWorkerScript,
                                   untrusted_worker_url.spec().c_str()),
                         EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
}

// Test that we can create a shared worker from a chrome-untrusted:// main
// frame.
IN_PROC_BROWSER_TEST_F(WebUIWorkerTest,
                       CanCreateUntrustedWebUISharedWorkerForUntrustedWebUI) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SetUntrustedWorkerSrcToWebUIConfig(/*allow_embedded_frame=*/false);

  const GURL untrusted_page_url(
      GetChromeUntrustedUIURL("untrusted/title2.html"));

  EXPECT_EQ(true, RunWorkerTest(untrusted_page_url,
                                GetChromeUntrustedUIURL(
                                    "untrusted/web_ui_shared_worker.js"),
                                kLoadSharedWorkerScript));
  EXPECT_EQ(untrusted_page_url, shell()->web_contents()->GetLastCommittedURL());
}

// Verify that chrome:// pages cannot create a SharedWorker with scheme
// "chrome-untrusted://".
IN_PROC_BROWSER_TEST_F(WebUIWorkerTest,
                       CannotCreateUntrustedWebUISharedWorkerFromTrustedWebUI) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EvalJsResult result = RunWorkerTest(
      GetWebUIURL("trusted/title2.html?notrustedtypes=true"),
      GetChromeUntrustedUIURL("untrusted/web_ui_shared_worker.js"),
      kLoadSharedWorkerScript);

  std::string expected_failure =
      "a JavaScript error: \"SecurityError: Failed to construct "
      "'SharedWorker': "
      "Script at 'chrome-untrusted://untrusted/web_ui_shared_worker.js' cannot "
      "be accessed from origin 'chrome://trusted'";
  EXPECT_THAT(result.error, ::testing::StartsWith(expected_failure));
}

// Verify that pages with scheme other than "chrome-untrusted://" cannot create
// a SharedWorker with scheme "chrome-untrusted://".
IN_PROC_BROWSER_TEST_F(WebUIWorkerTest,
                       CannotCreateUntrustedWebUISharedWorkerForWebURL) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EvalJsResult result = RunWorkerTest(
      embedded_test_server()->GetURL("localhost",
                                     "/title1.html?notrustedtypes=true"),
      GetChromeUntrustedUIURL("untrusted/web_ui_shared_worker.js"),
      kLoadSharedWorkerScript);

  std::string expected_failure =
      "a JavaScript error: \"SecurityError: Failed to construct "
      "'SharedWorker': "
      "Script at 'chrome-untrusted://untrusted/web_ui_shared_worker.js' cannot "
      "be accessed from origin 'http://localhost";
  EXPECT_THAT(result.error, ::testing::StartsWith(expected_failure));
}

// Verify that pages with scheme "chrome-untrusted://" cannot create a
// SharedWorker with scheme "chrome://".
IN_PROC_BROWSER_TEST_F(WebUIWorkerTest,
                       CannotCreateWebUISharedWorkerForUntrustedPage) {
  SetUntrustedWorkerSrcToWebUIConfig(/*allow_embedded_frame=*/false);

  EvalJsResult result = RunWorkerTest(
      GetChromeUntrustedUIURL("untrusted/title2.html?notrustedtypes=true"),
      GetWebUIURL("trusted/web_ui_shared_worker.js"), kLoadSharedWorkerScript);

  std::string expected_failure =
      "a JavaScript error: \"SecurityError: Failed to construct "
      "'SharedWorker': Script "
      "at 'chrome://trusted/web_ui_shared_worker.js' cannot be accessed from "
      "origin 'chrome-untrusted://untrusted'.";
  EXPECT_THAT(result.error, ::testing::StartsWith(expected_failure));
}

#endif  // !BUILDFLAG(IS_ANDROID)

// Verify that we can create a Worker with scheme "chrome://" under WebUI page.
IN_PROC_BROWSER_TEST_P(WebUIDedicatedWorkerTest,
                       CanCreateWebUIDedicatedWorkerForWebUI) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(true,
            RunWorkerTest(
                GURL(GetWebUIURL("test-host/title2.html?notrustedtypes=true")),
                GURL(GetWebUIURL("test-host/web_ui_dedicated_worker.js")),
                kLoadDedicatedWorkerScript));
}

// Verify that pages with scheme other than "chrome://" cannot create a Worker
// with scheme "chrome://".
IN_PROC_BROWSER_TEST_P(WebUIDedicatedWorkerTest,
                       CannotCreateWebUIDedicatedWorkerForNonWebUI) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EvalJsResult result = RunWorkerTest(
      GURL(embedded_test_server()->GetURL("/title1.html?notrustedtypes=true")),
      GURL(GetWebUIURL("test-host/web_ui_dedicated_worker.js")),
      kLoadDedicatedWorkerScript);

  std::string expected_failure =
      "a JavaScript error: \"SecurityError: Failed to construct 'Worker'";
  EXPECT_THAT(result.error, ::testing::StartsWith(expected_failure));
}

// Test that we can start a Worker from a chrome-untrusted:// iframe.
IN_PROC_BROWSER_TEST_P(WebUIDedicatedWorkerTest,
                       CanCreateDedicatedWorkerFromUntrustedIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = shell()->web_contents();

  SetUntrustedWorkerSrcToWebUIConfig(/*allow_embedded_frame=*/true);

  // Set up the urls.
  const GURL web_ui_url(
      GetWebUIURL("trusted/"
                  "title2.html?notrustedtypes=true&requestableschemes=chrome-"
                  "untrusted&childsrc="));
  const GURL untrusted_iframe_url(
      GetChromeUntrustedUIURL("untrusted/title1.html"));
  const GURL untrusted_worker_url(
      GetChromeUntrustedUIURL("untrusted/web_ui_dedicated_worker.js"));

  // Navigate to a chrome:// main page.
  EXPECT_TRUE(NavigateToURL(web_contents, web_ui_url));
  auto* main_frame = web_contents->GetPrimaryMainFrame();
  // Add an iframe in chrome-untrusted://.
  EXPECT_EQ(true,
            EvalJs(main_frame,
                   JsReplace("var frame = document.createElement('iframe');\n"
                             "frame.src=$1;\n"
                             "!!document.body.appendChild(frame);\n",
                             untrusted_iframe_url.spec().c_str()),
                   EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
  EXPECT_TRUE(WaitForLoadStop(web_contents));

  // Get the chrome-untrusted:// iframe.
  RenderFrameHost* child = ChildFrameAt(main_frame, 0);
  EXPECT_EQ(untrusted_iframe_url, child->GetLastCommittedURL());

  // Start a worker from the chrome-untrusted iframe.
  EXPECT_EQ(true, EvalJs(child,
                         JsReplace(kLoadDedicatedWorkerScript,
                                   untrusted_worker_url.spec().c_str()),
                         EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
}

// Test that we can create a Worker from a chrome-untrusted:// main frame.
IN_PROC_BROWSER_TEST_P(
    WebUIDedicatedWorkerTest,
    CanCreateUntrustedWebUIDedicatedWorkerForUntrustedWebUI) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SetUntrustedWorkerSrcToWebUIConfig(/*allow_embedded_frame=*/false);

  EXPECT_EQ(true,
            RunWorkerTest(
                GetChromeUntrustedUIURL("untrusted/title2.html"),
                GetChromeUntrustedUIURL("untrusted/web_ui_dedicated_worker.js"),
                kLoadDedicatedWorkerScript));
}

// Verify that chrome:// pages cannot create a Worker with scheme
// "chrome-untrusted://".
IN_PROC_BROWSER_TEST_P(
    WebUIDedicatedWorkerTest,
    CannotCreateUntrustedWebUIDedicatedWorkerFromTrustedWebUI) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EvalJsResult result = RunWorkerTest(
      GetWebUIURL("trusted/title2.html?notrustedtypes=true"),
      GetChromeUntrustedUIURL("untrusted/web_ui_dedicated_worker.js"),
      kLoadDedicatedWorkerScript);

  std::string expected_failure =
      "a JavaScript error: \"SecurityError: Failed to construct 'Worker': "
      "Script at 'chrome-untrusted://untrusted/web_ui_dedicated_worker.js' "
      "cannot be accessed from origin 'chrome://trusted'";
  EXPECT_THAT(result.error, ::testing::StartsWith(expected_failure));
}

// Verify that pages with scheme other than "chrome-untrusted://" cannot create
// a Worker with scheme "chrome-untrusted://".
IN_PROC_BROWSER_TEST_P(WebUIDedicatedWorkerTest,
                       CannotCreateUntrustedWebUIDedicatedWorkerForWebURL) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EvalJsResult result = RunWorkerTest(
      embedded_test_server()->GetURL("localhost",
                                     "/title1.html?notrustedtypes=true"),
      GetChromeUntrustedUIURL("untrusted/web_ui_dedicated_worker.js"),
      kLoadDedicatedWorkerScript);

  std::string expected_failure =
      "a JavaScript error: \"SecurityError: Failed to construct 'Worker': "
      "Script at 'chrome-untrusted://untrusted/web_ui_dedicated_worker.js' "
      "cannot be accessed from origin 'http://localhost";
  EXPECT_THAT(result.error, ::testing::StartsWith(expected_failure));
}

// Verify that pages with scheme "chrome-untrusted://" cannot create a Worker
// with scheme "chrome://".
IN_PROC_BROWSER_TEST_P(WebUIDedicatedWorkerTest,
                       CannotCreateWebUIDedicatedWorkerForUntrustedPage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SetUntrustedWorkerSrcToWebUIConfig(/*allow_embedded_frame=*/false);

  EvalJsResult result = RunWorkerTest(
      GetChromeUntrustedUIURL("untrusted/title2.html?notrustedtypes=true"),
      GetWebUIURL("trusted/web_ui_dedicated_worker.js"),
      kLoadDedicatedWorkerScript);

  std::string expected_failure =
      "a JavaScript error: \"SecurityError: Failed to construct 'Worker': "
      "Script "
      "at 'chrome://trusted/web_ui_dedicated_worker.js' cannot be accessed "
      "from origin 'chrome-untrusted://untrusted'.";
  EXPECT_THAT(result.error, ::testing::StartsWith(expected_failure));
}

}  // namespace content
