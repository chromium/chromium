// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_aura.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/events/event_utils.h"

namespace content {
namespace {

#if defined(OS_CHROMEOS)
const char kMinimalPageDataURL[] =
    "data:text/html,<html><head></head><body>Hello, world</body></html>";

// Run the current message loop for a short time without unwinding the current
// call stack.
void GiveItSomeTime() {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(250));
  run_loop.Run();
}
#endif  // defined(OS_CHROMEOS)

class FakeWebContentsDelegate : public WebContentsDelegate {
 public:
  FakeWebContentsDelegate() = default;
  ~FakeWebContentsDelegate() override = default;

  void SetShowStaleContentOnEviction(bool value) {
    show_stale_content_on_eviction_ = value;
  }

  bool ShouldShowStaleContentOnEviction(WebContents* source) override {
    return show_stale_content_on_eviction_;
  }

 private:
  bool show_stale_content_on_eviction_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeWebContentsDelegate);
};

}  // namespace

class RenderWidgetHostViewAuraBrowserTest : public ContentBrowserTest {
 public:
  RenderViewHost* GetRenderViewHost() const {
    RenderViewHost* const rvh = shell()->web_contents()->GetRenderViewHost();
    CHECK(rvh);
    return rvh;
  }

  RenderWidgetHostViewAura* GetRenderWidgetHostView() const {
    return static_cast<RenderWidgetHostViewAura*>(
        GetRenderViewHost()->GetWidget()->GetView());
  }

  DelegatedFrameHost* GetDelegatedFrameHost() const {
    return GetRenderWidgetHostView()->delegated_frame_host_.get();
  }
};

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserTest,
                       StaleFrameContentOnEvictionNormal) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kMinimalPageDataURL)));

  // Make sure the renderer submits at least one frame before hiding it.
  RenderFrameSubmissionObserver submission_observer(shell()->web_contents());
  if (!submission_observer.render_frame_count())
    submission_observer.WaitForAnyFrameSubmission();

  FakeWebContentsDelegate delegate;
  delegate.SetShowStaleContentOnEviction(true);
  shell()->web_contents()->SetDelegate(&delegate);

  // Initially there should be no stale content set.
  EXPECT_FALSE(
      GetDelegatedFrameHost()->stale_content_layer_->has_external_content());
  EXPECT_EQ(GetDelegatedFrameHost()->frame_eviction_state_,
            DelegatedFrameHost::FrameEvictionState::kNotStarted);

  // Hide the view and evict the frame. This should trigger a copy of the stale
  // frame content.
  GetRenderWidgetHostView()->Hide();
  static_cast<viz::FrameEvictorClient*>(GetDelegatedFrameHost())
      ->EvictDelegatedFrame();
  EXPECT_EQ(GetDelegatedFrameHost()->frame_eviction_state_,
            DelegatedFrameHost::FrameEvictionState::kPendingEvictionRequests);

  // Wait until the stale frame content is copied and set onto the layer.
  while (!GetDelegatedFrameHost()->stale_content_layer_->has_external_content())
    GiveItSomeTime();

  EXPECT_EQ(GetDelegatedFrameHost()->frame_eviction_state_,
            DelegatedFrameHost::FrameEvictionState::kNotStarted);

  // Unhidding the view should reset the stale content layer to show the new
  // frame content.
  GetRenderWidgetHostView()->Show();
  EXPECT_FALSE(
      GetDelegatedFrameHost()->stale_content_layer_->has_external_content());
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserTest,
                       StaleFrameContentOnEvictionRejected) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kMinimalPageDataURL)));

  // Wait for first frame activation when a surface is embedded.
  while (!GetDelegatedFrameHost()->HasSavedFrame())
    GiveItSomeTime();

  FakeWebContentsDelegate delegate;
  delegate.SetShowStaleContentOnEviction(true);
  shell()->web_contents()->SetDelegate(&delegate);

  // Initially there should be no stale content set.
  EXPECT_FALSE(
      GetDelegatedFrameHost()->stale_content_layer_->has_external_content());
  EXPECT_EQ(GetDelegatedFrameHost()->frame_eviction_state_,
            DelegatedFrameHost::FrameEvictionState::kNotStarted);

  // Hide the view and evict the frame. This should trigger a copy of the stale
  // frame content.
  GetRenderWidgetHostView()->Hide();
  static_cast<viz::FrameEvictorClient*>(GetDelegatedFrameHost())
      ->EvictDelegatedFrame();
  EXPECT_EQ(GetDelegatedFrameHost()->frame_eviction_state_,
            DelegatedFrameHost::FrameEvictionState::kPendingEvictionRequests);

  GetRenderWidgetHostView()->Show();
  EXPECT_EQ(GetDelegatedFrameHost()->frame_eviction_state_,
            DelegatedFrameHost::FrameEvictionState::kNotStarted);

  // Wait until the stale frame content is copied and the result callback is
  // complete.
  GiveItSomeTime();

  // This should however not set the stale content as the view is visible and
  // new frames are being submitted.
  EXPECT_FALSE(
      GetDelegatedFrameHost()->stale_content_layer_->has_external_content());
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserTest,
                       StaleFrameContentOnEvictionNone) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kMinimalPageDataURL)));

  // Wait for first frame activation when a surface is embedded.
  while (!GetDelegatedFrameHost()->HasSavedFrame())
    GiveItSomeTime();

  FakeWebContentsDelegate delegate;
  delegate.SetShowStaleContentOnEviction(false);
  shell()->web_contents()->SetDelegate(&delegate);

  // Initially there should be no stale content set.
  EXPECT_FALSE(
      GetDelegatedFrameHost()->stale_content_layer_->has_external_content());
  EXPECT_EQ(GetDelegatedFrameHost()->frame_eviction_state_,
            DelegatedFrameHost::FrameEvictionState::kNotStarted);

  // Hide the view and evict the frame. This should not trigger a copy of the
  // stale frame content as the WebContentDelegate returns false.
  GetRenderWidgetHostView()->Hide();
  static_cast<viz::FrameEvictorClient*>(GetDelegatedFrameHost())
      ->EvictDelegatedFrame();

  EXPECT_EQ(GetDelegatedFrameHost()->frame_eviction_state_,
            DelegatedFrameHost::FrameEvictionState::kNotStarted);

  // Wait for a while to ensure any copy requests that were sent out are not
  // completed. There shouldnt be any requests sent however.
  GiveItSomeTime();
  EXPECT_FALSE(
      GetDelegatedFrameHost()->stale_content_layer_->has_external_content());
}
#endif  // #if defined(OS_CHROMEOS)

class RenderWidgetHostViewAuraDevtoolsBrowserTest
    : public content::DevToolsProtocolTest {
 public:
  RenderWidgetHostViewAuraDevtoolsBrowserTest() = default;

 protected:
  aura::Window* window() {
    return static_cast<content::RenderWidgetHostViewAura*>(
               shell()->web_contents()->GetRenderWidgetHostView())
        ->window();
  }

  bool HasChildPopup() const {
    return static_cast<content::RenderWidgetHostViewAura*>(
               shell()->web_contents()->GetRenderWidgetHostView())
        ->popup_child_host_view_;
  }
};

// This test opens a select popup which inside breaks the debugger
// which enters a nested event loop.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraDevtoolsBrowserTest,
                       NoCrashOnSelect) {
  GURL page(
      "data:text/html;charset=utf-8,"
      "<!DOCTYPE html>"
      "<html>"
      "<body>"
      "<select id=\"ddlChoose\">"
      " <option value=\"\">Choose</option>"
      " <option value=\"A\">A</option>"
      " <option value=\"B\">B</option>"
      " <option value=\"C\">C</option>"
      "</select>"
      "<script type=\"text/javascript\">"
      "  document.getElementById('ddlChoose').addEventListener('change', "
      "    function () {"
      "      debugger;"
      "    });"
      "  function focusSelectMenu() {"
      "    document.getElementById('ddlChoose').focus();"
      "  }"
      "  function noop() {"
      "  }"
      "</script>"
      "</body>"
      "</html>");

  EXPECT_TRUE(NavigateToURL(shell(), page));
  auto* wc = shell()->web_contents();
  Attach();
  SendCommand("Debugger.enable", nullptr);

  ASSERT_TRUE(ExecuteScript(wc, "focusSelectMenu();"));
  SimulateKeyPress(wc, ui::DomKey::FromCharacter(' '), ui::DomCode::SPACE,
                   ui::VKEY_SPACE, false, false, false, false);

  // Wait until popup is opened.
  while (!HasChildPopup()) {
    base::RunLoop().RunUntilIdle();
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  }

  // Send down and enter to select next item and cause change listener to fire.
  // The event listener causes devtools to break (and enter a nested event
  // loop).
  ui::KeyEvent press_down(ui::ET_KEY_PRESSED, ui::VKEY_DOWN,
                          ui::DomCode::ARROW_DOWN, ui::EF_NONE,
                          ui::DomKey::ARROW_DOWN, ui::EventTimeForNow());
  ui::KeyEvent release_down(ui::ET_KEY_RELEASED, ui::VKEY_DOWN,
                            ui::DomCode::ARROW_DOWN, ui::EF_NONE,
                            ui::DomKey::ARROW_DOWN, ui::EventTimeForNow());
  ui::KeyEvent press_enter(ui::ET_KEY_PRESSED, ui::VKEY_RETURN,
                           ui::DomCode::ENTER, ui::EF_NONE, ui::DomKey::ENTER,
                           ui::EventTimeForNow());
  ui::KeyEvent release_enter(ui::ET_KEY_RELEASED, ui::VKEY_RETURN,
                             ui::DomCode::ENTER, ui::EF_NONE, ui::DomKey::ENTER,
                             ui::EventTimeForNow());
  auto* host_view_aura = static_cast<content::RenderWidgetHostViewAura*>(
      wc->GetRenderWidgetHostView());
  host_view_aura->OnKeyEvent(&press_down);
  host_view_aura->OnKeyEvent(&release_down);
  host_view_aura->OnKeyEvent(&press_enter);
  host_view_aura->OnKeyEvent(&release_enter);
  WaitForNotification("Debugger.paused");

  // Close the widget window while inside the nested event loop.
  // This will cause the RenderWidget to be destroyed while we are inside a
  // method and used to cause UAF when the nested event loop unwinds.
  window()->Hide();

  // Disconnect devtools causes script to resume. This causes the unwind of the
  // nested event loop. The RenderWidget that was entered has been destroyed,
  // make sure that we detect this and don't touch any members in the class.
  Detach();

  // Try to access the renderer process, it would have died if
  // crbug.com/1032984 wasn't fixed.
  ASSERT_TRUE(ExecuteScript(wc, "noop();"));
}

class RenderWidgetHostViewAuraActiveWidgetTest : public ContentBrowserTest {
 public:
  RenderWidgetHostViewAuraActiveWidgetTest() = default;
  ~RenderWidgetHostViewAuraActiveWidgetTest() override = default;

  // Helper function to check |isActivated| for a given frame.
  bool FrameIsActivated(content::RenderFrameHost* rfh) {
    bool active = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        rfh,
        "window.domAutomationController.send(window.internals.isActivated())",
        &active));
    return active;
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kExposeInternalsForTesting);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add content/test/data for cross_site_iframe_factory.html
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");

    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAuraActiveWidgetTest);
};

// In this test, toggling the value of 'active' state changes the
// active state of frame on the renderer side.
// SimulateActiveStateForWidget toggles the 'active' state of widget
// over IPC.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraActiveWidgetTest,
                       FocusIsInactive) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  content::WebContents* web_contents = shell()->web_contents();

  // The main_frame_a should have a focus to start with.
  // On renderer side, blink::FocusController's both 'active' and
  //'focus' states are set to true.
  content::RenderFrameHost* main_frame = web_contents->GetMainFrame();
  EXPECT_TRUE(FrameIsActivated(main_frame));

  // After changing the 'active' state of main_frame to false
  // blink::FocusController's 'active' set to false.
  content::SimulateActiveStateForWidget(main_frame, false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(FrameIsActivated(main_frame));

  // After changing the 'active' state of main_frame to true
  // blink::FocusController's 'active' set to true.
  content::SimulateActiveStateForWidget(main_frame, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(FrameIsActivated(main_frame));
}

}  // namespace content
