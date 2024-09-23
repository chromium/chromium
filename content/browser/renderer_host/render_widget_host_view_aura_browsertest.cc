// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_aura.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
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
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/screen.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

#if BUILDFLAG(IS_WIN)
#include "content/browser/renderer_host/legacy_render_widget_host_win.h"
#endif

namespace content {
namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kMinimalPageDataURL[] =
    "data:text/html,<html><head></head><body>Hello, world</body></html>";

// Run the current message loop for a short time without unwinding the current
// call stack.
void GiveItSomeTime() {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(250));
  run_loop.Run();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class FakeWebContentsDelegate : public WebContentsDelegate {
 public:
  FakeWebContentsDelegate() = default;

  FakeWebContentsDelegate(const FakeWebContentsDelegate&) = delete;
  FakeWebContentsDelegate& operator=(const FakeWebContentsDelegate&) = delete;

  ~FakeWebContentsDelegate() override = default;

  void SetShowStaleContentOnEviction(bool value) {
    show_stale_content_on_eviction_ = value;
  }

  bool ShouldShowStaleContentOnEviction(WebContents* source) override {
    return show_stale_content_on_eviction_;
  }

 private:
  bool show_stale_content_on_eviction_ = false;
};

}  // namespace

class RenderWidgetHostViewAuraBrowserTest : public ContentBrowserTest {
 public:
  RenderViewHost* GetRenderViewHost() const {
    RenderViewHost* const rvh =
        shell()->web_contents()->GetPrimaryMainFrame()->GetRenderViewHost();
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

  bool HasChildPopup() const {
    return GetRenderWidgetHostView()->popup_child_host_view_;
  }

#if BUILDFLAG(IS_WIN)
  LegacyRenderWidgetHostHWND* GetLegacyRenderWidgetHostHWND() const {
    return GetRenderWidgetHostView()->legacy_render_widget_host_HWND_;
  }
#endif  // BUILDFLAG(IS_WIN)
};

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserTest, AuraWindowLookup) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  aura::Window* window = GetRenderWidgetHostView()->GetNativeView();
  ASSERT_TRUE(GetLegacyRenderWidgetHostHWND());
  HWND hwnd = GetLegacyRenderWidgetHostHWND()->hwnd();
  EXPECT_TRUE(hwnd);
  auto* window_tree_host = aura::WindowTreeHost::GetForAcceleratedWidget(hwnd);
  EXPECT_TRUE(window_tree_host);
  EXPECT_EQ(window->GetHost(), window_tree_host);
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserTest,
                       // TODO(crbug.com/40874148): Re-enable this test
                       // TODO(crbug.com/40873813): Re-enable this test
                       DISABLED_StaleFrameContentOnEvictionNormal) {
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
  auto* dfh = GetDelegatedFrameHost();
  static_cast<viz::FrameEvictorClient*>(dfh)->EvictDelegatedFrame(
      dfh->GetFrameEvictorForTesting()->CollectSurfaceIdsForEviction());
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
  auto* dfh = GetDelegatedFrameHost();
  static_cast<viz::FrameEvictorClient*>(dfh)->EvictDelegatedFrame(
      dfh->GetFrameEvictorForTesting()->CollectSurfaceIdsForEviction());
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
  auto* dfh = GetDelegatedFrameHost();
  static_cast<viz::FrameEvictorClient*>(dfh)->EvictDelegatedFrame(
      dfh->GetFrameEvictorForTesting()->CollectSurfaceIdsForEviction());

  EXPECT_EQ(GetDelegatedFrameHost()->frame_eviction_state_,
            DelegatedFrameHost::FrameEvictionState::kNotStarted);

  // Wait for a while to ensure any copy requests that were sent out are not
  // completed. There shouldnt be any requests sent however.
  GiveItSomeTime();
  EXPECT_FALSE(
      GetDelegatedFrameHost()->stale_content_layer_->has_external_content());
}
#endif  // #if BUILDFLAG(IS_CHROMEOS_ASH)

// TODO(crbug.com/40148102): fix the way how exo creates accelerated widgets. At
// the moment, they are created only after the client attaches a buffer to a
// surface, which is incorrect and results in the "[destroyed object]: error 1:
// popup parent not constructed" error.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_SetKeyboardFocusOnTapAfterDismissingPopup \
  DISABLED_SetKeyboardFocusOnTapAfterDismissingPopup
#else
#define MAYBE_SetKeyboardFocusOnTapAfterDismissingPopup \
  SetKeyboardFocusOnTapAfterDismissingPopup
#endif
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserTest,
                       MAYBE_SetKeyboardFocusOnTapAfterDismissingPopup) {
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
      "  function focusSelectMenu() {"
      "    document.getElementById('ddlChoose').focus();"
      "  }"
      "</script>"
      "</body>"
      "</html>");
  EXPECT_TRUE(NavigateToURL(shell(), page));

  auto* wc = shell()->web_contents();
  ASSERT_TRUE(ExecJs(wc, "focusSelectMenu();"));
  SimulateKeyPress(wc, ui::DomKey::FromCharacter(' '), ui::DomCode::SPACE,
                   ui::VKEY_SPACE, false, false, false, false);

  // Wait until popup is opened.
  EXPECT_TRUE(base::test::RunUntil([&]() { return HasChildPopup(); }));

  // Page is focused to begin with.
  ASSERT_TRUE(IsRenderWidgetHostFocused(GetRenderViewHost()->GetWidget()));

  // Tap outside the page to dismiss the pop-up.
  const gfx::Point kOutsidePointInRoot(1000, 300);
  ASSERT_FALSE(GetRenderWidgetHostView()->GetNativeView()->bounds().Contains(
      kOutsidePointInRoot));
  ui::test::EventGenerator generator(
      GetRenderWidgetHostView()->GetNativeView()->GetRootWindow());
  generator.GestureTapAt(kOutsidePointInRoot);
  RunUntilInputProcessed(GetRenderViewHost()->GetWidget());

  // Tap on the page.
  generator.GestureTapAt(
      GetRenderWidgetHostView()->GetNativeView()->bounds().CenterPoint());
  RunUntilInputProcessed(GetRenderViewHost()->GetWidget());

  // Page should stay focused after the tap.
  EXPECT_TRUE(IsRenderWidgetHostFocused(GetRenderViewHost()->GetWidget()));
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserTest,
                       UpdatesCaretBoundsAfterFrameScroll) {
  GURL page(
      "data:text/html;charset=utf-8,"
      "<!DOCTYPE html>"
      "<html>"
      "<body>"
      "<style>"
      "  %23scrollableDiv {"
      "  height: 10000px;"
      "  }"
      "  %23textfield {"
      "  margin-top: 100px;"
      "  }"
      "</style>"
      "<div id=\"scrollableDiv\">"
      "  <input id=\"textfield\" type=\"text\" value=\"Some editable text\">"
      "</div>"
      "<script type=\"text/javascript\">"
      "  function focusTextfield() {"
      "    document.getElementById('textfield').focus({'preventScroll': true});"
      "  }"
      "</script>"
      "</body>"
      "</html>");
  EXPECT_TRUE(NavigateToURL(shell(), page));
  GetRenderWidgetHostView()->SetSize(gfx::Size(600, 500));

  // Focus the textfield and wait for initial caret bounds.
  auto* web_contents = shell()->web_contents();
  {
    // The caret bounds can have briefly have an invalid zero size value when
    // the textfield initially focuses, so wait for non-zero caret size rather
    // than waiting for the first caret bounds update.
    NonZeroCaretSizeWaiter initial_caret_bounds_waiter(web_contents);
    ASSERT_TRUE(ExecJs(web_contents, "focusTextfield();"));
    initial_caret_bounds_waiter.Wait();
  }

  const gfx::Rect initial_caret_bounds =
      GetRenderWidgetHostView()->GetCaretBounds();
  EXPECT_NE(initial_caret_bounds, gfx::Rect());

  // Scroll and wait for caret bounds to update.
  {
    CaretBoundsUpdateWaiter caret_bounds_update_waiter(web_contents);
    ASSERT_TRUE(ExecJs(web_contents, "window.scrollBy(0, 50);"));
    caret_bounds_update_waiter.Wait();
  }

  EXPECT_EQ(GetRenderWidgetHostView()->GetCaretBounds().x(),
            initial_caret_bounds.x());
  EXPECT_LT(GetRenderWidgetHostView()->GetCaretBounds().y(),
            initial_caret_bounds.y());
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserTest,
                       UpdatesCaretBoundsAfterOverflowScroll) {
  GURL page(
      "data:text/html;charset=utf-8,"
      "<!DOCTYPE html>"
      "<html>"
      "<body>"
      "<style>"
      "  %23container {"
      "  height: 200px;"
      "  overflow: scroll;"
      "  }"
      "  %23scrollableDiv {"
      "  height: 1000px;"
      "  }"
      "  %23textfield {"
      "  margin-top: 100px;"
      "  }"
      "</style>"
      "<div id=\"container\">"
      "  <div id=\"scrollableDiv\">"
      "    <input id=\"textfield\" type=\"text\" value=\"Some editable text\">"
      "  </div>"
      "</div>"
      "<script type=\"text/javascript\">"
      "  function focusTextfield() {"
      "    document.getElementById('textfield').focus({'preventScroll': true});"
      "  }"
      "  function scrollContainerTopBy(dy) {"
      "    document.getElementById('container').scrollTop += dy;"
      "  }"
      "</script>"
      "</body>"
      "</html>");
  EXPECT_TRUE(NavigateToURL(shell(), page));
  GetRenderWidgetHostView()->SetSize(gfx::Size(600, 500));

  // Focus the textfield and wait for initial caret bounds.
  auto* web_contents = shell()->web_contents();
  {
    // The caret bounds can have briefly have an invalid zero size value when
    // the textfield initially focuses, so wait for non-zero caret size rather
    // than waiting for the first caret bounds update.
    NonZeroCaretSizeWaiter initial_caret_bounds_waiter(web_contents);
    ASSERT_TRUE(ExecJs(web_contents, "focusTextfield();"));
    initial_caret_bounds_waiter.Wait();
  }

  const gfx::Rect initial_caret_bounds =
      GetRenderWidgetHostView()->GetCaretBounds();
  EXPECT_NE(initial_caret_bounds, gfx::Rect());

  // Scroll and wait for caret bounds to update.
  {
    CaretBoundsUpdateWaiter caret_bounds_update_waiter(web_contents);
    ASSERT_TRUE(ExecJs(web_contents, "scrollContainerTopBy(50);"));
    caret_bounds_update_waiter.Wait();
  }

  EXPECT_EQ(GetRenderWidgetHostView()->GetCaretBounds().x(),
            initial_caret_bounds.x());
  EXPECT_LT(GetRenderWidgetHostView()->GetCaretBounds().y(),
            initial_caret_bounds.y());
}

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
  SendCommandSync("Debugger.enable");

  ASSERT_TRUE(ExecJs(wc, "focusSelectMenu();"));
  SimulateKeyPress(wc, ui::DomKey::FromCharacter(' '), ui::DomCode::SPACE,
                   ui::VKEY_SPACE, false, false, false, false);

  // Wait until popup is opened.
  EXPECT_TRUE(base::test::RunUntil([&]() { return HasChildPopup(); }));

  // Send down and enter to select next item and cause change listener to fire.
  // The event listener causes devtools to break (and enter a nested event
  // loop).
  ui::KeyEvent press_down(ui::EventType::kKeyPressed, ui::VKEY_DOWN,
                          ui::DomCode::ARROW_DOWN, ui::EF_NONE,
                          ui::DomKey::ARROW_DOWN, ui::EventTimeForNow());
  ui::KeyEvent release_down(ui::EventType::kKeyReleased, ui::VKEY_DOWN,
                            ui::DomCode::ARROW_DOWN, ui::EF_NONE,
                            ui::DomKey::ARROW_DOWN, ui::EventTimeForNow());
  ui::KeyEvent press_enter(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                           ui::DomCode::ENTER, ui::EF_NONE, ui::DomKey::ENTER,
                           ui::EventTimeForNow());
  ui::KeyEvent release_enter(ui::EventType::kKeyReleased, ui::VKEY_RETURN,
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
  ASSERT_TRUE(ExecJs(wc, "noop();"));
}

// Used to verify features under the environment whose device scale factor is 2.
class RenderWidgetHostViewAuraDSFBrowserTest
    : public RenderWidgetHostViewAuraBrowserTest {
 public:
  // RenderWidgetHostViewAuraBrowserTest:
  void SetUp() override {
    EnablePixelOutput(scale());
    RenderWidgetHostViewAuraBrowserTest::SetUp();
  }

  float scale() const { return 2.f; }
};

// Verifies the bounding box of the selection region.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraDSFBrowserTest,
                       SelectionRegionBoundingBox) {
  GURL page(
      "data:text/html;charset=utf-8,"
      "<!DOCTYPE html>"
      "<html>"
      "<body>"
      "<p id=\"text-content\">Gibbons are apes in the family Hylobatidae.</p>"
      "<script>"
      "  function selectText() {"
      "    const input = document.getElementById('text-content');"
      "    var range = document.createRange();"
      "    range.selectNodeContents(input);"
      "    var selection = window.getSelection();  "
      "    selection.removeAllRanges();"
      "    selection.addRange(range);"
      "  }"
      "  function getSelectionBounds() {"
      "    var r = "
      "document.getSelection().getRangeAt(0).getBoundingClientRect();"
      "    return [r.x, r.right, r.y, r.bottom]; "
      "  }"
      "</script>"
      "</body>"
      "</html>");
  EXPECT_TRUE(NavigateToURL(shell(), page));

  // Select text and wait until the bounding box updates.
  auto* wc = shell()->web_contents();
  BoundingBoxUpdateWaiter select_waiter(wc);
  ASSERT_TRUE(ExecJs(wc, "selectText();"));
  select_waiter.Wait();

  // Verify the device scale factor.
  const float device_scale_factor =
      GetRenderWidgetHostView()->GetDeviceScaleFactor();
  ASSERT_EQ(scale(), device_scale_factor);

  // Calculate the DIP size from the bounds in pixel. Follow exactly what is
  // done in `WebFrameWidgetImpl`.
  const base::Value eval_result =
      EvalJs(wc, "getSelectionBounds();").ExtractList();
  const int x = floor(eval_result.GetList()[0].GetDouble());
  const int right = ceil(eval_result.GetList()[1].GetDouble());
  const int y = floor(eval_result.GetList()[2].GetDouble());
  const int bottom = ceil(eval_result.GetList()[3].GetDouble());
  const int expected_dip_width = floor(right / scale()) - ceil(x / scale());
  const int expected_dip_height = floor(bottom / scale()) - ceil(y / scale());

  // Verify the DIP size of the bounding box.
  const gfx::Rect selection_bounds =
      GetRenderWidgetHostView()->GetSelectionBoundingBox();
  EXPECT_EQ(expected_dip_width, selection_bounds.width());
  EXPECT_EQ(expected_dip_height, selection_bounds.height());
}

class RenderWidgetHostViewAuraActiveWidgetTest : public ContentBrowserTest {
 public:
  RenderWidgetHostViewAuraActiveWidgetTest() = default;

  RenderWidgetHostViewAuraActiveWidgetTest(
      const RenderWidgetHostViewAuraActiveWidgetTest&) = delete;
  RenderWidgetHostViewAuraActiveWidgetTest& operator=(
      const RenderWidgetHostViewAuraActiveWidgetTest&) = delete;

  ~RenderWidgetHostViewAuraActiveWidgetTest() override = default;

  void SetUp() override {
    EnablePixelOutput(1.0);
    ContentBrowserTest::SetUp();
  }

  // Helper function to check |isActivated| for a given frame.
  bool FrameIsActivated(content::RenderFrameHost* rfh) {
    return EvalJs(rfh, "window.internals.isActivated()").ExtractBool();
  }

  bool FrameIsFocused(content::RenderFrameHost* rfh) {
    return EvalJs(rfh, "document.hasFocus()").ExtractBool();
  }

  RenderViewHost* GetRenderViewHost() const {
    RenderViewHost* const rvh =
        shell()->web_contents()->GetPrimaryMainFrame()->GetRenderViewHost();
    CHECK(rvh);
    return rvh;
  }

  RenderWidgetHostViewAura* GetRenderWidgetHostView() const {
    return static_cast<RenderWidgetHostViewAura*>(
        GetRenderViewHost()->GetWidget()->GetView());
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kExposeInternalsForTesting);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add content/test/data for cross_site_iframe_factory.html
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");

    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// In this test, toggling the value of 'active' state changes the
// active state of frame on the renderer side. Cross origin iframes
// are checked to ensure the active state is replicated across all
// processes. SimulateActiveStateForWidget toggles the 'active' state
// of widget over IPC.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraActiveWidgetTest,
                       FocusIsInactive) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  content::WebContents* web_contents = shell()->web_contents();

  // The main_frame_a should have a focus to start with.
  // On renderer side, blink::FocusController's both 'active' and
  //'focus' states are set to true.
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* iframe = ChildFrameAt(main_frame, 0);
  EXPECT_TRUE(FrameIsFocused(main_frame));
  EXPECT_TRUE(FrameIsActivated(iframe));
  EXPECT_TRUE(FrameIsFocused(main_frame));
  EXPECT_FALSE(FrameIsFocused(iframe));

  // After changing the 'active' state of main_frame to false
  // blink::FocusController's 'active' set to false and
  // document.hasFocus() will return false.
  content::SimulateActiveStateForWidget(main_frame, false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(FrameIsActivated(main_frame));
  EXPECT_FALSE(FrameIsActivated(iframe));
  EXPECT_FALSE(FrameIsFocused(main_frame));
  EXPECT_FALSE(FrameIsFocused(iframe));

  // After changing the 'active' state of main_frame to true
  // blink::FocusController's 'active' set to true and
  // document.hasFocus() will return true.
  content::SimulateActiveStateForWidget(main_frame, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(FrameIsActivated(main_frame));
  EXPECT_TRUE(FrameIsActivated(iframe));
  EXPECT_TRUE(FrameIsFocused(main_frame));
  EXPECT_FALSE(FrameIsFocused(iframe));

  // Now unfocus the main frame, this should keep the active state.
  main_frame->GetRenderWidgetHost()->Blur();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(FrameIsActivated(main_frame));
  EXPECT_TRUE(FrameIsActivated(iframe));
  EXPECT_FALSE(FrameIsFocused(main_frame));
  EXPECT_FALSE(FrameIsFocused(iframe));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Verifies that getting active input control accounts for iframe positioning.
// Flaky: crbug.com/1293700
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraActiveWidgetTest,
                       DISABLED_TextControlBoundingRegionInIframe) {
  GURL page(
      embedded_test_server()->GetURL("example.com", "/input_in_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), page));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Ensure both the main page and the iframe are loaded.
  ASSERT_EQ("OUTER_LOADED",
            EvalJs(root->current_frame_host(), "notifyWhenLoaded()"));
  ASSERT_EQ("LOADED", EvalJs(root->current_frame_host(),
                             "document.querySelector(\"iframe\").contentWindow."
                             "notifyWhenLoaded();"));
  // TODO(b/204006085): Remove this sleep call and replace with polling.
  GiveItSomeTime();

  std::optional<gfx::Rect> control_bounds;
  std::optional<gfx::Rect> selection_bounds;
  GetRenderWidgetHostView()->GetActiveTextInputControlLayoutBounds(
      &control_bounds, &selection_bounds);

  // 4000px from input offset inside input_box.html
  // 200px from input_in_iframe.html
  EXPECT_TRUE(control_bounds.has_value());
  ASSERT_EQ(4200, control_bounds->origin().y());
}
#endif

}  // namespace content
