// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/model_context_user_data.h"

#include <utility>
#include <vector>

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "mojo/public/cpp/bindings/lib/validation_context.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

namespace content {

namespace {

using ReadyState = RenderFrameHost::ScriptToolsReadyState;
using Notification = std::pair<RenderFrameHost*, ReadyState>;

// Records each `ScriptToolsReadyStateChanged()` call, with the frame's state
// at the time.
class ScriptToolsReadyObserver : public WebContentsObserver {
 public:
  explicit ScriptToolsReadyObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void ScriptToolsReadyStateChanged(RenderFrameHost* rfh) override {
    notifications_.emplace_back(rfh, rfh->GetScriptToolsReadyState());
  }

  const std::vector<Notification>& notifications() const {
    return notifications_;
  }

  void ClearNotifications() { notifications_.clear(); }

 private:
  std::vector<Notification> notifications_;
};

}  // namespace

class ModelContextUserDataBrowserTest : public ContentBrowserTest {
 public:
  ModelContextUserDataBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kWebMCP, blink::features::kWebMCPWaitForToolsUntil},
        {});
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    // Note: 127.0.0.1 is a secure context, as `document.modelContext` requires.
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(
        NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  RenderFrameHost* main_rfh() { return web_contents()->GetPrimaryMainFrame(); }

  // Puts the main document in the tools waiting state.
  [[nodiscard]] bool MarkScriptToolsWaiting() {
    return ExecJs(web_contents(), R"(
        window.toolsReady = new Promise(resolve => {
          window.resolveToolsReady = resolve;
        });
        document.modelContext.waitForToolsUntil(window.toolsReady);
    )") && WaitForScriptToolsReadyState(ReadyState::kWaiting);
  }

  // Returns once the main frame has reached `state`.
  [[nodiscard]] bool WaitForScriptToolsReadyState(ReadyState state) {
    return base::test::RunUntil(
        [&]() { return main_rfh()->GetScriptToolsReadyState() == state; });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Ensures that a frame reports the expected ready states across document load
// and `waitForToolsUntil()` promises.
IN_PROC_BROWSER_TEST_F(ModelContextUserDataBrowserTest, ReadyStateTransitions) {
  EXPECT_FALSE(ModelContextUserData::GetForCurrentDocument(main_rfh()));
  EXPECT_EQ(main_rfh()->GetScriptToolsReadyState(), ReadyState::kReady);

  ScriptToolsReadyObserver observer(web_contents());
  ASSERT_TRUE(MarkScriptToolsWaiting());
  ASSERT_TRUE(ExecJs(web_contents(), "window.resolveToolsReady()"));
  ASSERT_TRUE(WaitForScriptToolsReadyState(ReadyState::kReady));
  EXPECT_THAT(
      observer.notifications(),
      testing::ElementsAre(Notification(main_rfh(), ReadyState::kWaiting),
                           Notification(main_rfh(), ReadyState::kReady)));

  // Navigating the frame transitions through `kLoading` to `kReady` at
  // DOMContentLoaded.
  observer.ClearNotifications();
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));
  EXPECT_THAT(
      observer.notifications(),
      testing::ElementsAre(Notification(main_rfh(), ReadyState::kLoading),
                           Notification(main_rfh(), ReadyState::kReady)));
}

// Ensures that a document replacement (`javascript:` URL) and `document.open()`
// update the ready state when promises are pending or settled.
// Note: `RenderFrameHost::IsDOMContentLoaded()` is not reset in the browser on
// `javascript:` document replacement or `document.open()`, so the state does
// not transition back through `kLoading` in those cases.
// TODO(crbug.com/555173199): Fix this once the spec resolves on the document
// lifecycle to wait for.
IN_PROC_BROWSER_TEST_F(ModelContextUserDataBrowserTest,
                       ReplacementDocumentAndDocumentOpen) {
  ASSERT_TRUE(MarkScriptToolsWaiting());
  ScriptToolsReadyObserver observer(web_contents());

  ASSERT_TRUE(
      ExecJs(web_contents(),
             "location.href = 'javascript:\"<title>replaced</title>\"'"));
  ASSERT_TRUE(WaitForScriptToolsReadyState(ReadyState::kReady));
  EXPECT_EQ("replaced", EvalJs(web_contents(), "document.title"));
  EXPECT_THAT(observer.notifications(), testing::ElementsAre(Notification(
                                            main_rfh(), ReadyState::kReady)));

  observer.ClearNotifications();
  ASSERT_TRUE(ExecJs(web_contents(), R"(
      document.open();
      window.toolsReady = new Promise(resolve => {
        window.resolveToolsReady = resolve;
      });
      document.modelContext.waitForToolsUntil(window.toolsReady);
  )"));
  ASSERT_TRUE(WaitForScriptToolsReadyState(ReadyState::kWaiting));
  ASSERT_TRUE(ExecJs(web_contents(), "window.resolveToolsReady()"));
  ASSERT_TRUE(WaitForScriptToolsReadyState(ReadyState::kReady));
  EXPECT_THAT(
      observer.notifications(),
      testing::ElementsAre(Notification(main_rfh(), ReadyState::kWaiting),
                           Notification(main_rfh(), ReadyState::kReady)));
}

// Ensures that a promise created in another document counts as settled once
// that document is destroyed.
IN_PROC_BROWSER_TEST_F(ModelContextUserDataBrowserTest,
                       PromiseFromRemovedDocumentCountsAsSettled) {
  RenderFrameHost* child =
      CreateSubframe(static_cast<WebContentsImpl*>(web_contents()), "child",
                     embedded_test_server()->GetURL("/title1.html"),
                     /*wait_for_navigation=*/true);
  ASSERT_TRUE(child);

  // Test a cross-document promise that resolves normally (`a`), one that
  // resolves after the child frame is removed (`b`), and one that never
  // settles on its own.
  ASSERT_TRUE(ExecJs(child, R"(
      window.a = Promise.withResolvers();
      window.b = Promise.withResolvers();
      parent.document.modelContext.waitForToolsUntil(window.a.promise);
      parent.document.modelContext.waitForToolsUntil(window.b.promise);
      parent.document.modelContext.waitForToolsUntil(new Promise(() => {}));
      window.a.resolve();
  )"));
  ASSERT_TRUE(WaitForScriptToolsReadyState(ReadyState::kWaiting));

  ASSERT_TRUE(ExecJs(web_contents(), R"(
      const iframe = document.getElementById('child');
      const resolveB = iframe.contentWindow.b.resolve;
      iframe.remove();
      resolveB();
  )"));
  ASSERT_TRUE(WaitForScriptToolsReadyState(ReadyState::kReady));
}

// Ensures that a crashed renderer reports the correct state.
IN_PROC_BROWSER_TEST_F(ModelContextUserDataBrowserTest, CrashedRenderer) {
  ASSERT_TRUE(MarkScriptToolsWaiting());

  ScopedAllowRendererCrashes allow_renderer_crashes(web_contents());
  RenderProcessHost* process = main_rfh()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0);
  crash_observer.Wait();

  EXPECT_TRUE(ModelContextUserData::GetForCurrentDocument(main_rfh()));
  EXPECT_EQ(main_rfh()->GetScriptToolsReadyState(), ReadyState::kNoRenderer);
}

// Ensures that a frame where WebMCP is not enabled reports a bad message if it
// updates its script tools ready state.
IN_PROC_BROWSER_TEST_F(ModelContextUserDataBrowserTest,
                       FrameWithoutToolsPolicyIsKilled) {
  // No `allow` attribute, so the `tools` permissions policy is disabled in
  // this cross-origin iframe.
  RenderFrameHost* child =
      CreateSubframe(static_cast<WebContentsImpl*>(web_contents()), "child",
                     embedded_test_server()->GetURL("b.test", "/title1.html"),
                     /*wait_for_navigation=*/true);
  ASSERT_TRUE(child);

  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  ModelContextUserData::GetOrCreateForCurrentDocument(child)
      ->SetScriptToolsReady(false);
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "WebMCPWaitForToolsUntil is not enabled.");
}

}  // namespace content
