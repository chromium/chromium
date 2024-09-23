// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/text_input_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ime/virtual_keyboard_controller.h"
#include "ui/base/ime/virtual_keyboard_controller_observer.h"

namespace content {

// This class observes TextInputManager for changes in
// |TextInputState.vk_policy|.
class TextInputManagerVkPolicyObserver : public TextInputManagerObserverBase {
 public:
  TextInputManagerVkPolicyObserver(
      WebContents* web_contents,
      ui::mojom::VirtualKeyboardPolicy expected_value)
      : TextInputManagerObserverBase(web_contents),
        expected_value_(expected_value) {
    tester()->SetUpdateTextInputStateCalledCallback(
        base::BindRepeating(&TextInputManagerVkPolicyObserver::VerifyVkPolicy,
                            base::Unretained(this)));
  }

  TextInputManagerVkPolicyObserver(const TextInputManagerVkPolicyObserver&) =
      delete;
  TextInputManagerVkPolicyObserver operator=(
      const TextInputManagerVkPolicyObserver&) = delete;

 private:
  void VerifyVkPolicy() {
    ui::mojom::VirtualKeyboardPolicy value;
    if (tester()->GetTextInputVkPolicy(&value) && expected_value_ == value)
      OnSuccess();
  }

  ui::mojom::VirtualKeyboardPolicy expected_value_;
};

// This class observes TextInputManager for changes in
// |TextInputState.last_vk_visibility_request|.
class TextInputManagerVkVisibilityRequestObserver
    : public TextInputManagerObserverBase {
 public:
  TextInputManagerVkVisibilityRequestObserver(
      WebContents* web_contents,
      ui::mojom::VirtualKeyboardVisibilityRequest expected_value)
      : TextInputManagerObserverBase(web_contents),
        expected_value_(expected_value) {
    tester()->SetUpdateTextInputStateCalledCallback(base::BindRepeating(
        &TextInputManagerVkVisibilityRequestObserver::VerifyVkVisibilityRequest,
        base::Unretained(this)));
  }

  TextInputManagerVkVisibilityRequestObserver(
      const TextInputManagerVkVisibilityRequestObserver&) = delete;
  TextInputManagerVkVisibilityRequestObserver operator=(
      const TextInputManagerVkVisibilityRequestObserver&) = delete;

 private:
  void VerifyVkVisibilityRequest() {
    ui::mojom::VirtualKeyboardVisibilityRequest value;
    if (tester()->GetTextInputVkVisibilityRequest(&value) &&
        expected_value_ == value)
      OnSuccess();
  }

  ui::mojom::VirtualKeyboardVisibilityRequest expected_value_;
};

// This class observes TextInputManager for changes in
// |TextInputState.show_ime_if_needed|.
class TextInputManagerShowImeIfNeededObserver
    : public TextInputManagerObserverBase {
 public:
  TextInputManagerShowImeIfNeededObserver(WebContents* web_contents,
                                          bool expected_value)
      : TextInputManagerObserverBase(web_contents),
        expected_value_(expected_value) {
    tester()->SetUpdateTextInputStateCalledCallback(base::BindRepeating(
        &TextInputManagerShowImeIfNeededObserver::VerifyShowImeIfNeeded,
        base::Unretained(this)));
  }

  TextInputManagerShowImeIfNeededObserver(
      const TextInputManagerShowImeIfNeededObserver&) = delete;
  TextInputManagerShowImeIfNeededObserver operator=(
      const TextInputManagerShowImeIfNeededObserver&) = delete;

 private:
  void VerifyShowImeIfNeeded() {
    bool show_ime_if_needed;
    if (tester()->GetTextInputShowImeIfNeeded(&show_ime_if_needed) &&
        expected_value_ == show_ime_if_needed)
      OnSuccess();
  }

  bool expected_value_ = false;
};

class RenderWidgetHostViewAuraBrowserMockIMETest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    server_.AddDefaultHandlers(GetTestDataFilePath());
    server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(server_.Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "VirtualKeyboard,EditContext");
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
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

  ui::BrowserAccessibility* FindNode(ax::mojom::Role role,
                                     const std::string& name_or_value) {
    ui::BrowserAccessibility* root =
        GetManager()->GetBrowserAccessibilityRoot();
    CHECK(root);
    return FindNodeInSubtree(*root, role, name_or_value);
  }

  ui::BrowserAccessibilityManager* GetManager() {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    return web_contents->GetRootBrowserAccessibilityManager();
  }

  void LoadInitialAccessibilityTreeFromHtml(const std::string& html) {
    AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                           ui::kAXModeComplete,
                                           ax::mojom::Event::kLoadComplete);
    GURL html_data_url("data:text/html," + html);
    EXPECT_TRUE(NavigateToURL(shell(), html_data_url));
    ASSERT_TRUE(waiter.WaitForNotification());
  }

  net::EmbeddedTestServer server_{net::EmbeddedTestServer::TYPE_HTTPS};

 private:
  ui::BrowserAccessibility* FindNodeInSubtree(
      ui::BrowserAccessibility& node,
      ax::mojom::Role role,
      const std::string& name_or_value) {
    const std::string& name =
        node.GetStringAttribute(ax::mojom::StringAttribute::kName);
    const std::string value = base::UTF16ToUTF8(node.GetValueForControl());
    if (node.GetRole() == role &&
        (name == name_or_value || value == name_or_value)) {
      return &node;
    }

    for (unsigned int i = 0; i < node.PlatformChildCount(); ++i) {
      ui::BrowserAccessibility* result =
          FindNodeInSubtree(*node.PlatformGetChild(i), role, name_or_value);
      if (result)
        return result;
    }
    return nullptr;
  }
};

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserMockIMETest,
                       VirtualKeyboardAccessibilityFocusTest) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <div><button>Before</button></div>
      <div contenteditable>Editable text</div>
      <div><button>After</button></div>
      )HTML");

  ui::BrowserAccessibility* target =
      FindNode(ax::mojom::Role::kGenericContainer, "Editable text");
  ASSERT_NE(nullptr, target);
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  auto* root = web_contents->GetPrimaryFrameTree().root();
  web_contents->GetPrimaryFrameTree().SetFocusedFrame(
      root, root->current_frame_host()->GetSiteInstance()->group());

  AccessibilityNotificationWaiter waiter2(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kFocus);
  GetManager()->SetFocus(*target);
  GetManager()->DoDefaultAction(*target);
  ASSERT_TRUE(waiter2.WaitForNotification());

  ui::BrowserAccessibility* focus = GetManager()->GetFocus();
  EXPECT_EQ(focus->GetId(), target->GetId());
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserMockIMETest,
                       VirtualKeyboardShowVKTest) {
  GURL start_url = server_.GetURL("a.test", "/virtual-keyboard.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  auto* root = web_contents->GetPrimaryFrameTree().root();
  web_contents->GetPrimaryFrameTree().SetFocusedFrame(
      root, root->current_frame_host()->GetSiteInstance()->group());

  // Send a touch event so that RenderWidgetHostViewAura will create the
  // keyboard observer (requires last_pointer_type_ to be TOUCH).
  // Tap on the third textarea to open VK.
  TextInputManagerVkPolicyObserver type_observer_auto(
      web_contents, ui::mojom::VirtualKeyboardPolicy::AUTO);
  const int top = EvalJs(shell(), "elemRect3.top").ExtractInt();
  const int left = EvalJs(shell(), "elemRect3.left").ExtractInt();
  const int width = EvalJs(shell(), "elemRect3.width").ExtractInt();
  const int height = EvalJs(shell(), "elemRect3.height").ExtractInt();
  gfx::Point tap_point = gfx::Point(left + width / 2, top + height / 2);
  SimulateTouchEventAt(web_contents, ui::EventType::kTouchPressed, tap_point);
  SimulateTapDownAt(web_contents, tap_point);
  SimulateTapAt(web_contents, tap_point);
  SimulateTouchEventAt(web_contents, ui::EventType::kTouchReleased, tap_point);
  type_observer_auto.Wait();
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserMockIMETest,
                       DontShowVKOnJSFocus) {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  TextInputManagerShowImeIfNeededObserver show_ime_observer_false(web_contents,
                                                                  false);
  // Note: This data URL has JS that focuses the edit control.
  GURL start_url = server_.GetURL("a.test", "/virtual-keyboard.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));
  show_ime_observer_false.Wait();

  // Send a touch event so that RenderWidgetHostViewAura will create the
  // keyboard observer (requires last_pointer_type_ to be TOUCH).
  // Tap on the third textarea to open VK.
  TextInputManagerShowImeIfNeededObserver show_ime_observer_true(web_contents,
                                                                 true);
  const int top = EvalJs(shell(), "elemRect3.top").ExtractInt();
  const int left = EvalJs(shell(), "elemRect3.left").ExtractInt();
  const int width = EvalJs(shell(), "elemRect3.width").ExtractInt();
  const int height = EvalJs(shell(), "elemRect3.height").ExtractInt();
  gfx::Point tap_point = gfx::Point(left + width / 2, top + height / 2);
  SimulateTouchEventAt(web_contents, ui::EventType::kTouchPressed, tap_point);
  SimulateTapDownAt(web_contents, tap_point);
  SimulateTapAt(web_contents, tap_point);
  SimulateTouchEventAt(web_contents, ui::EventType::kTouchReleased, tap_point);
  show_ime_observer_true.Wait();
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserMockIMETest,
                       ShowAndThenHideVK) {
  GURL start_url = server_.GetURL("a.test", "/virtual-keyboard.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  auto* root = web_contents->GetPrimaryFrameTree().root();
  web_contents->GetPrimaryFrameTree().SetFocusedFrame(
      root, root->current_frame_host()->GetSiteInstance()->group());

  // Send a touch event so that RenderWidgetHostViewAura will create the
  // keyboard observer (requires last_pointer_type_ to be TOUCH).
  // Tap on the third textarea to open VK.
  TextInputManagerVkVisibilityRequestObserver type_observer_show(
      web_contents, ui::mojom::VirtualKeyboardVisibilityRequest::SHOW);
  const int top = EvalJs(shell(), "elemRect1.top").ExtractInt();
  const int left = EvalJs(shell(), "elemRect1.left").ExtractInt();
  const int width = EvalJs(shell(), "elemRect1.width").ExtractInt();
  const int height = EvalJs(shell(), "elemRect1.height").ExtractInt();
  gfx::Point tap_point = gfx::Point(left + width / 2, top + height / 2);
  SimulateTouchEventAt(web_contents, ui::EventType::kTouchPressed, tap_point);
  SimulateTapDownAt(web_contents, tap_point);
  SimulateTapAt(web_contents, tap_point);
  SimulateTouchEventAt(web_contents, ui::EventType::kTouchReleased, tap_point);
  type_observer_show.Wait();
  TextInputManagerVkVisibilityRequestObserver type_observer_hide(
      web_contents, ui::mojom::VirtualKeyboardVisibilityRequest::HIDE);
  SimulateKeyPress(web_contents, ui::DomKey::ENTER, ui::DomCode::ENTER,
                   ui::VKEY_RETURN, false, false, false, false);
  type_observer_hide.Wait();
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserMockIMETest,
                       ShowAndThenHideVKInEditContext) {
  GURL start_url = server_.GetURL("a.test", "/virtual-keyboard.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  auto* root = web_contents->GetPrimaryFrameTree().root();
  web_contents->GetPrimaryFrameTree().SetFocusedFrame(
      root, root->current_frame_host()->GetSiteInstance()->group());

  // Send a touch event so that RenderWidgetHostViewAura will create the
  // keyboard observer (requires last_pointer_type_ to be TOUCH).
  // Tap on the third textarea to open VK.
  TextInputManagerVkVisibilityRequestObserver type_observer_show(
      web_contents, ui::mojom::VirtualKeyboardVisibilityRequest::SHOW);
  const int top = EvalJs(shell(), "elemRect2.top").ExtractInt();
  const int left = EvalJs(shell(), "elemRect2.left").ExtractInt();
  const int width = EvalJs(shell(), "elemRect2.width").ExtractInt();
  const int height = EvalJs(shell(), "elemRect2.height").ExtractInt();
  gfx::Point tap_point = gfx::Point(left + width / 2, top + height / 2);
  SimulateTouchEventAt(web_contents, ui::EventType::kTouchPressed, tap_point);
  SimulateTapDownAt(web_contents, tap_point);
  SimulateTapAt(web_contents, tap_point);
  SimulateTouchEventAt(web_contents, ui::EventType::kTouchReleased, tap_point);
  type_observer_show.Wait();
  TextInputManagerVkVisibilityRequestObserver type_observer_hide(
      web_contents, ui::mojom::VirtualKeyboardVisibilityRequest::HIDE);
  SimulateKeyPress(web_contents, ui::DomKey::ENTER, ui::DomCode::ENTER,
                   ui::VKEY_RETURN, false, false, false, false);
  type_observer_hide.Wait();
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserMockIMETest,
                       VKVisibilityRequestInDeletedDocument) {
  const char kVirtualKeyboardDataURL[] =
      "data:text/html,<!DOCTYPE html>"
      "<body>"
      "<textarea id='txt3' virtualkeyboardpolicy='manual' "
      "onfocusin='FocusIn1()'></textarea>"
      "<script>"
      " let elemRect = txt3.getBoundingClientRect();"
      " function FocusIn1() {"
      "   navigator.virtualKeyboard.show();"
      "   const child = document.createElement(\"iframe\");"
      "   document.body.appendChild(child);"
      "   const childDocument = child.contentDocument;"
      "   const textarea = childDocument.createElement('textarea');"
      "   textarea.setAttribute(\"virtualKeyboardPolicy\", \"manual\");"
      "   childDocument.body.appendChild(textarea);"
      "   textarea.addEventListener(\"onfocusin\", e => {"
      "   child.remove();"
      "   });"
      "  child.contentWindow.focus();"
      "  textarea.focus();"
      "  }"
      "</script>"
      "</body>";
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kVirtualKeyboardDataURL)));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  auto* root = web_contents->GetPrimaryFrameTree().root();
  web_contents->GetPrimaryFrameTree().SetFocusedFrame(
      root, root->current_frame_host()->GetSiteInstance()->group());

  // Send a touch event so that RenderWidgetHostViewAura will create the
  // keyboard observer (requires last_pointer_type_ to be TOUCH).
  TextInputManagerVkVisibilityRequestObserver type_observer_none(
      web_contents, ui::mojom::VirtualKeyboardVisibilityRequest::NONE);
  const int top = EvalJs(shell(), "elemRect.top").ExtractInt();
  const int left = EvalJs(shell(), "elemRect.left").ExtractInt();
  SimulateTapDownAt(web_contents, gfx::Point(left + 1, top + 1));
  SimulateTapAt(web_contents, gfx::Point(left + 1, top + 1));
  type_observer_none.Wait();
}
#endif  // BUILDFLAG(IS_WIN)

// This class observes TextInputManager for changes in `active_view_`.
class TextInputManagerStateObserver : public TextInputManagerObserverBase {
 public:
  explicit TextInputManagerStateObserver(WebContents* web_contents)
      : TextInputManagerObserverBase(web_contents) {
    tester()->SetUpdateTextInputStateCalledCallback(base::BindRepeating(
        &TextInputManagerStateObserver::VerifyActiveViewOnStateUpdate,
        base::Unretained(this)));
  }
  ~TextInputManagerStateObserver() override = default;

  void WaitUntilActiveViewIsUpdated() {
    base::RunLoop run_loop;
    quit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  RenderWidgetHostView* last_updated_active_view() {
    return last_updated_active_view_;
  }

 private:
  void VerifyActiveViewOnStateUpdate() {
    RenderWidgetHostView* active_view =
        const_cast<RenderWidgetHostView*>(tester()->GetActiveView());
    EXPECT_TRUE(active_view);
    last_updated_active_view_ = active_view;
    RenderWidgetHostImpl* widget_host =
        static_cast<RenderWidgetHostImpl*>(active_view->GetRenderWidgetHost());
    EXPECT_TRUE(widget_host->frame_tree()->is_primary());
    OnSuccess();
    if (quit_callback_)
      std::move(quit_callback_).Run();
  }

  raw_ptr<RenderWidgetHostView> last_updated_active_view_ = nullptr;
  base::OnceClosure quit_callback_;
};

class RenderWidgetHostViewAuraPrerenderingBrowserTest
    : public ContentBrowserTest {
 public:
  RenderWidgetHostViewAuraPrerenderingBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &RenderWidgetHostViewAuraPrerenderingBrowserTest::GetWebContents,
            base::Unretained(this))) {}
  ~RenderWidgetHostViewAuraPrerenderingBrowserTest() override = default;

  WebContents* GetWebContents() { return shell()->web_contents(); }

  test::PrerenderTestHelper& prerender_helper() { return prerender_helper_; }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(test_server_handle_ =
                    embedded_test_server()->StartAndReturnHandle());
  }

 private:
  test::PrerenderTestHelper prerender_helper_;
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
};

// Tests that `active_view_` from TextInputManager is not set for prerendering.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraPrerenderingBrowserTest,
                       TextInputManagerActiveView) {
  const GURL kVirtualKeyboardUrl =
      embedded_test_server()->GetURL("/virtual-keyboard.html");
  const GURL kEmptyUrl = embedded_test_server()->GetURL("/empty.html");

  auto* web_contents = GetWebContents();
  TextInputManagerStateObserver type_observer_show(web_contents);
  // Navigate to a simple page.
  ASSERT_TRUE(NavigateToURL(shell(), kEmptyUrl));
  EXPECT_EQ(web_contents->GetLastCommittedURL(), kEmptyUrl);
  EXPECT_EQ(type_observer_show.last_updated_active_view(), nullptr);

  // Start prerendering `kVirtualKeyboardUrl`.
  prerender_helper().AddPrerender(kVirtualKeyboardUrl);
  // Prerendering shouldn't update `active_view_` from TextInputManager.
  EXPECT_EQ(type_observer_show.last_updated_active_view(), nullptr);

  prerender_helper().NavigatePrimaryPage(kVirtualKeyboardUrl);
  type_observer_show.WaitUntilActiveViewIsUpdated();
  // If the page is activated, it updates `active_view_` from TextInputManager.
  EXPECT_EQ(web_contents->GetLastCommittedURL(), kVirtualKeyboardUrl);
  EXPECT_EQ(type_observer_show.last_updated_active_view(),
            web_contents->GetPrimaryMainFrame()->GetView());
}

}  // namespace content
