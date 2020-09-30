// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/observer_list.h"
#include "base/win/windows_version.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
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
#include "content/public/test/text_input_test_utils.h"
#include "content/shell/browser/shell.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/ime/input_method_keyboard_controller.h"
#include "ui/base/ime/input_method_keyboard_controller_observer.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ime/mock_input_method.h"
#include "ui/base/ime/text_input_type.h"

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

class MockKeyboardController : public ui::InputMethodKeyboardController {
 public:
  bool DisplayVirtualKeyboard() override {
    is_keyboard_visible_ = true;
    return true;
  }
  void DismissVirtualKeyboard() override {}
  void AddObserver(
      ui::InputMethodKeyboardControllerObserver* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(
      ui::InputMethodKeyboardControllerObserver* observer) override {
    observers_.RemoveObserver(observer);
  }

  void NotifyObserversOnKeyboardShown(gfx::Rect dip_rect) {
    is_keyboard_visible_ = true;
    for (ui::InputMethodKeyboardControllerObserver& observer : observers_)
      observer.OnKeyboardVisible(dip_rect);
  }

  void NotifyObserversOnKeyboardHidden() {
    is_keyboard_visible_ = false;
    for (ui::InputMethodKeyboardControllerObserver& observer : observers_)
      observer.OnKeyboardHidden();
  }

  bool IsKeyboardVisible() override { return is_keyboard_visible_; }

 private:
  base::ObserverList<ui::InputMethodKeyboardControllerObserver,
                     false>::Unchecked observers_;
  bool is_keyboard_visible_ = false;
};

class InputMethodKeyboardObserver : public ui::InputMethodObserver {
 public:
  // ui::InputMethodObserver:
  void OnFocus() override {}
  void OnBlur() override {}
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {}
  void OnShowVirtualKeyboardIfEnabled() override {
    is_keyboard_display_called_ = true;
  }
  void OnTextInputStateChanged(const ui::TextInputClient* client) override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {}
  bool IsKeyboardDisplayCalled() { return is_keyboard_display_called_; }

 private:
  bool is_keyboard_display_called_ = false;
};

class KeyboardControllerMockInputMethod : public ui::MockInputMethod {
 public:
  KeyboardControllerMockInputMethod() : ui::MockInputMethod(nullptr) {}
  ui::InputMethodKeyboardController* GetInputMethodKeyboardController()
      override {
    return &mock_keyboard_controller_;
  }

  MockKeyboardController* GetMockKeyboardController() {
    return &mock_keyboard_controller_;
  }

 private:
  MockKeyboardController mock_keyboard_controller_;
};

class RenderWidgetHostViewAuraBrowserMockIMETest : public ContentBrowserTest {
 public:
  void SetUp() override {
    input_method_ = new KeyboardControllerMockInputMethod;
    mock_keyboard_observer_ = new InputMethodKeyboardObserver;
    input_method_->AddObserver(mock_keyboard_observer_);
    // transfers ownership.
    ui::SetUpInputMethodForTesting(input_method_);
    ContentBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "VirtualKeyboard,EditContext");
    ContentBrowserTest::SetUpCommandLine(command_line);
  }

  RenderViewHost* GetRenderViewHost() const {
    RenderViewHost* const rvh = shell()->web_contents()->GetRenderViewHost();
    CHECK(rvh);
    return rvh;
  }

  RenderWidgetHostViewAura* GetRenderWidgetHostView() const {
    return static_cast<RenderWidgetHostViewAura*>(
        GetRenderViewHost()->GetWidget()->GetView());
  }

  BrowserAccessibility* FindNode(ax::mojom::Role role,
                                 const std::string& name_or_value) {
    BrowserAccessibility* root = GetManager()->GetRoot();
    CHECK(root);
    return FindNodeInSubtree(*root, role, name_or_value);
  }

  BrowserAccessibilityManager* GetManager() {
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
    waiter.WaitForNotification();
  }

 protected:
  KeyboardControllerMockInputMethod* input_method_ = nullptr;
  InputMethodKeyboardObserver* mock_keyboard_observer_ = nullptr;

 private:
  BrowserAccessibility* FindNodeInSubtree(BrowserAccessibility& node,
                                          ax::mojom::Role role,
                                          const std::string& name_or_value) {
    const std::string& name =
        node.GetStringAttribute(ax::mojom::StringAttribute::kName);
    const std::string& value = base::UTF16ToUTF8(node.GetValue());
    if (node.GetRole() == role &&
        (name == name_or_value || value == name_or_value)) {
      return &node;
    }

    for (unsigned int i = 0; i < node.PlatformChildCount(); ++i) {
      BrowserAccessibility* result =
          FindNodeInSubtree(*node.PlatformGetChild(i), role, name_or_value);
      if (result)
        return result;
    }
    return nullptr;
  }
};

#ifdef OS_WIN
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserMockIMETest,
                       VirtualKeyboardIntegrationTest) {
  // The keyboard input pane events are not supported on Win7.
  if (base::win::GetVersion() <= base::win::Version::WIN7) {
    return;
  }

  const char kVirtualKeyboardDataURL[] =
      "data:text/html,<!DOCTYPE html>"
      "<script>"
      "  let VKRect, x, y, width, height, numEvents = 0;"
      "  navigator.virtualKeyboard.overlaysContent = true;"
      "  navigator.virtualKeyboard.addEventListener('geometrychange',"
      "   evt => {"
      "     numEvents++;"
      "     let r = evt.boundingRect;"
      "     x = r.x; y = r.y; width = r.width; height = r.height;"
      "     VKRect = navigator.virtualKeyboard.boundingRect"
      "   }, false);"
      "</script>";
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kVirtualKeyboardDataURL)));

  // Send a touch event so that RenderWidgetHostViewAura will create the
  // keyboard observer (requires last_pointer_type_ to be TOUCH).
  ui::TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(30, 30),
                       base::TimeTicks::Now(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  RenderWidgetHostViewAura* rwhvi = GetRenderWidgetHostView();
  rwhvi->OnTouchEvent(&press);

  // Emulate input type text focus in the root frame (the only frame), by
  // setting frame focus and updating TextInputState. This is a more direct way
  // of triggering focus in a textarea in the web page.
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  auto* root = web_contents->GetFrameTree()->root();
  web_contents->GetFrameTree()->SetFocusedFrame(
      root, root->current_frame_host()->GetSiteInstance());

  ui::mojom::TextInputState text_input_state;
  text_input_state.show_ime_if_needed = true;
  text_input_state.type = ui::TEXT_INPUT_TYPE_TEXT;

  TextInputManager* text_input_manager = rwhvi->GetTextInputManager();
  text_input_manager->UpdateTextInputState(rwhvi, text_input_state);

  // Send through a keyboard showing event with a rect, and verify the
  // javascript event fires with the appropriate values.
  constexpr int kKeyboardX = 0;
  constexpr int kKeyboardY = 200;
  constexpr int kKeyboardWidth = 200;
  constexpr int kKeyboardHeight = 200;
  gfx::Rect keyboard_rect(kKeyboardX, kKeyboardY, kKeyboardWidth,
                          kKeyboardHeight);
  input_method_->GetMockKeyboardController()->NotifyObserversOnKeyboardShown(
      keyboard_rect);

  // There are x and y-offsets for the main frame in content_browsertest
  // hosting. We need to take these into account for the expected values.
  gfx::PointF root_widget_origin(0.f, 0.f);
  rwhvi->TransformPointToRootSurface(&root_widget_origin);
  const int expected_width = kKeyboardWidth - root_widget_origin.x();
  const int expected_y = kKeyboardY - root_widget_origin.y();

  EXPECT_EQ(1, EvalJs(shell(), "numEvents"));
  EXPECT_EQ(0, EvalJs(shell(), "x"));
  EXPECT_EQ(expected_y, EvalJs(shell(), "y"));
  EXPECT_EQ(expected_width, EvalJs(shell(), "width"));
  EXPECT_EQ(kKeyboardHeight, EvalJs(shell(), "height"));
  EXPECT_EQ(0, EvalJs(shell(), "VKRect.x"));
  EXPECT_EQ(expected_y, EvalJs(shell(), "VKRect.y"));
  EXPECT_EQ(expected_width, EvalJs(shell(), "VKRect.width"));
  EXPECT_EQ(kKeyboardHeight, EvalJs(shell(), "VKRect.height"));

  input_method_->GetMockKeyboardController()->NotifyObserversOnKeyboardHidden();
  EXPECT_EQ(2, EvalJs(shell(), "numEvents"));
  EXPECT_EQ(0, EvalJs(shell(), "width"));
  EXPECT_EQ(0, EvalJs(shell(), "height"));
  EXPECT_EQ(0, EvalJs(shell(), "x"));
  EXPECT_EQ(0, EvalJs(shell(), "y"));
  EXPECT_EQ(0, EvalJs(shell(), "VKRect.x"));
  EXPECT_EQ(0, EvalJs(shell(), "VKRect.y"));
  EXPECT_EQ(0, EvalJs(shell(), "VKRect.width"));
  EXPECT_EQ(0, EvalJs(shell(), "VKRect.height"));

  // Flip the policy back to non-overlay, verify the event doesn't fire
  ignore_result(
      EvalJs(shell(), "navigator.virtualKeyboard.overlaysContent = false"));
  input_method_->GetMockKeyboardController()->NotifyObserversOnKeyboardShown(
      keyboard_rect);
  EXPECT_EQ(2, EvalJs(shell(), "numEvents"));
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserMockIMETest,
                       VirtualKeyboardCSSEnvVarIntegrationTest) {
  // The keyboard input pane events are not supported on Win7.
  if (base::win::GetVersion() <= base::win::Version::WIN7) {
    return;
  }

  const char kVirtualKeyboardDataURL[] =
      "data:text/html,<!DOCTYPE html>"
      "<style>"
      "  .target {"
      "    margin-top: env(keyboard-inset-top);"
      "    margin-left: env(keyboard-inset-left);"
      "    margin-bottom: env(keyboard-inset-bottom);"
      "    margin-right: env(keyboard-inset-right);"
      "  }"
      "</style>"
      "<body>"
      "<div class='target'></div>"
      "<script>"
      "  let numEvents = 0;"
      "  navigator.virtualKeyboard.overlaysContent = true;"
      "  const e = document.getElementsByClassName('target')[0];"
      "  const style = window.getComputedStyle(e, null);"
      "  navigator.virtualKeyboard.addEventListener('geometrychange',"
      "   evt => {"
      "     numEvents++;"
      "   }, false);"
      "</script></body>";
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kVirtualKeyboardDataURL)));

  EXPECT_EQ(
      "0px",
      EvalJs(shell(), "style.getPropertyValue('margin-top')").ExtractString());
  EXPECT_EQ(
      "0px",
      EvalJs(shell(), "style.getPropertyValue('margin-left')").ExtractString());
  EXPECT_EQ("0px", EvalJs(shell(), "style.getPropertyValue('margin-bottom')")
                       .ExtractString());
  EXPECT_EQ("0px", EvalJs(shell(), "style.getPropertyValue('margin-right')")
                       .ExtractString());

  RenderWidgetHostViewAura* rwhvi = GetRenderWidgetHostView();

  // Send a touch event so that RenderWidgetHostViewAura will create the
  // keyboard observer (requires last_pointer_type_ to be TOUCH).
  ui::TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(30, 30),
                       base::TimeTicks::Now(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  rwhvi->OnTouchEvent(&press);

  // Emulate input type text focus in the root frame (the only frame), by
  // setting frame focus and updating TextInputState. This is a more direct way
  // of triggering focus in a textarea in the web page.
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  auto* root = web_contents->GetFrameTree()->root();
  web_contents->GetFrameTree()->SetFocusedFrame(
      root, root->current_frame_host()->GetSiteInstance());

  ui::mojom::TextInputState text_input_state;
  text_input_state.show_ime_if_needed = true;
  text_input_state.type = ui::TEXT_INPUT_TYPE_TEXT;

  TextInputManager* text_input_manager = rwhvi->GetTextInputManager();
  text_input_manager->UpdateTextInputState(rwhvi, text_input_state);

  // Send through a keyboard showing event with a rect, and verify the
  // javascript event fires with the appropriate values.
  constexpr int kKeyboardX = 0;
  constexpr int kKeyboardY = 200;
  constexpr int kKeyboardWidth = 200;
  constexpr int kKeyboardHeight = 200;
  gfx::Rect keyboard_rect(kKeyboardX, kKeyboardY, kKeyboardWidth,
                          kKeyboardHeight);
  input_method_->GetMockKeyboardController()->NotifyObserversOnKeyboardShown(
      keyboard_rect);

  // There are x and y-offsets for the main frame in content_browsertest
  // hosting. We need to take these into account for the expected values.
  gfx::PointF root_widget_origin(0.f, 0.f);
  rwhvi->TransformPointToRootSurface(&root_widget_origin);

  EXPECT_EQ(1, EvalJs(shell(), "numEvents"));
  EXPECT_EQ(
      "161px",
      EvalJs(shell(), "style.getPropertyValue('margin-top')").ExtractString());
  EXPECT_EQ(
      "0px",
      EvalJs(shell(), "style.getPropertyValue('margin-left')").ExtractString());
  EXPECT_EQ("198px", EvalJs(shell(), "style.getPropertyValue('margin-right')")
                         .ExtractString());
  EXPECT_EQ("361px", EvalJs(shell(), "style.getPropertyValue('margin-bottom')")
                         .ExtractString());

  input_method_->GetMockKeyboardController()->NotifyObserversOnKeyboardHidden();
  EXPECT_EQ(2, EvalJs(shell(), "numEvents"));
  EXPECT_EQ(
      "0px",
      EvalJs(shell(), "style.getPropertyValue('margin-top')").ExtractString());
  EXPECT_EQ(
      "0px",
      EvalJs(shell(), "style.getPropertyValue('margin-left')").ExtractString());
  EXPECT_EQ("0px", EvalJs(shell(), "style.getPropertyValue('margin-right')")
                       .ExtractString());
  EXPECT_EQ("0px", EvalJs(shell(), "style.getPropertyValue('margin-bottom')")
                       .ExtractString());
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserMockIMETest,
                       VirtualKeyboardCSSEnvVarWidthAndHeightIntegrationTest) {
  // The keyboard input pane events are not supported on Win7.
  if (base::win::GetVersion() <= base::win::Version::WIN7) {
    return;
  }

  const char kVirtualKeyboardDataURL[] =
      "data:text/html,<!DOCTYPE html>"
      "<style>"
      "  .target {"
      "    width: env(keyboard-inset-width);"
      "    height: env(keyboard-inset-height);"
      "  }"
      "</style>"
      "<body>"
      "<div class='target'></div>"
      "<script>"
      "  let numEvents = 0;"
      "  navigator.virtualKeyboard.overlaysContent = true;"
      "  const e = document.getElementsByClassName('target')[0];"
      "  const style = window.getComputedStyle(e, null);"
      "  navigator.virtualKeyboard.addEventListener('geometrychange',"
      "   evt => {"
      "     numEvents++;"
      "   }, false);"
      "</script></body>";
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kVirtualKeyboardDataURL)));

  EXPECT_EQ("0px",
            EvalJs(shell(), "style.getPropertyValue('width')").ExtractString());
  EXPECT_EQ(
      "0px",
      EvalJs(shell(), "style.getPropertyValue('height')").ExtractString());

  RenderWidgetHostViewAura* rwhvi = GetRenderWidgetHostView();

  // Send a touch event so that RenderWidgetHostViewAura will create the
  // keyboard observer (requires last_pointer_type_ to be TOUCH).
  ui::TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(30, 30),
                       base::TimeTicks::Now(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  rwhvi->OnTouchEvent(&press);

  // Emulate input type text focus in the root frame (the only frame), by
  // setting frame focus and updating TextInputState. This is a more direct way
  // of triggering focus in a textarea in the web page.
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  auto* root = web_contents->GetFrameTree()->root();
  web_contents->GetFrameTree()->SetFocusedFrame(
      root, root->current_frame_host()->GetSiteInstance());

  ui::mojom::TextInputState text_input_state;
  text_input_state.show_ime_if_needed = true;
  text_input_state.type = ui::TEXT_INPUT_TYPE_TEXT;

  TextInputManager* text_input_manager = rwhvi->GetTextInputManager();
  text_input_manager->UpdateTextInputState(rwhvi, text_input_state);

  // Send through a keyboard showing event with a rect, and verify the
  // javascript event fires with the appropriate values.
  constexpr int kKeyboardX = 0;
  constexpr int kKeyboardY = 200;
  constexpr int kKeyboardWidth = 200;
  constexpr int kKeyboardHeight = 200;
  gfx::Rect keyboard_rect(kKeyboardX, kKeyboardY, kKeyboardWidth,
                          kKeyboardHeight);
  input_method_->GetMockKeyboardController()->NotifyObserversOnKeyboardShown(
      keyboard_rect);

  // There are x and y-offsets for the main frame in content_browsertest
  // hosting. We need to take these into account for the expected values.
  gfx::PointF root_widget_origin(0.f, 0.f);
  rwhvi->TransformPointToRootSurface(&root_widget_origin);

  EXPECT_EQ(1, EvalJs(shell(), "numEvents"));
  EXPECT_EQ("198px",
            EvalJs(shell(), "style.getPropertyValue('width')").ExtractString());
  EXPECT_EQ(
      "200px",
      EvalJs(shell(), "style.getPropertyValue('height')").ExtractString());

  input_method_->GetMockKeyboardController()->NotifyObserversOnKeyboardHidden();
  EXPECT_EQ(2, EvalJs(shell(), "numEvents"));
  EXPECT_EQ("0px",
            EvalJs(shell(), "style.getPropertyValue('width')").ExtractString());
  EXPECT_EQ(
      "0px",
      EvalJs(shell(), "style.getPropertyValue('height')").ExtractString());
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserMockIMETest,
                       VirtualKeyboardAccessibilityFocusTest) {
  // The keyboard input pane events are not supported on Win7.
  if (base::win::GetVersion() <= base::win::Version::WIN7) {
    return;
  }

  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <div><button>Before</button></div>
      <div contenteditable>Editable text</div>
      <div><button>After</button></div>
      )HTML");

  BrowserAccessibility* target =
      FindNode(ax::mojom::Role::kGenericContainer, "Editable text");
  ASSERT_NE(nullptr, target);
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  auto* root = web_contents->GetFrameTree()->root();
  web_contents->GetFrameTree()->SetFocusedFrame(
      root, root->current_frame_host()->GetSiteInstance());

  AccessibilityNotificationWaiter waiter2(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kFocus);
  GetManager()->SetFocus(*target);
  GetManager()->DoDefaultAction(*target);
  waiter2.WaitForNotification();

  BrowserAccessibility* focus = GetManager()->GetFocus();
  EXPECT_EQ(focus->GetId(), target->GetId());

  EXPECT_EQ(true, mock_keyboard_observer_->IsKeyboardDisplayCalled());
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserMockIMETest,
                       VirtualKeyboardShowVKTest) {
  // The keyboard input pane events are not supported on Win7.
  if (base::win::GetVersion() <= base::win::Version::WIN7) {
    return;
  }

  const char kVirtualKeyboardDataURL[] =
      "data:text/html,<!DOCTYPE html>"
      "<body>"
      "<textarea id='txt3'></textarea>"
      "<script>"
      " let elemRect = txt3.getBoundingClientRect();"
      "</script>"
      "</body>";
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kVirtualKeyboardDataURL)));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  auto* root = web_contents->GetFrameTree()->root();
  web_contents->GetFrameTree()->SetFocusedFrame(
      root, root->current_frame_host()->GetSiteInstance());

  // Send a touch event so that RenderWidgetHostViewAura will create the
  // keyboard observer (requires last_pointer_type_ to be TOUCH).
  // Tap on the third textarea to open VK.
  TextInputManagerVkPolicyObserver type_observer_auto(
      web_contents, ui::mojom::VirtualKeyboardPolicy::AUTO);
  const int top = EvalJs(shell(), "elemRect.top").ExtractInt();
  const int left = EvalJs(shell(), "elemRect.left").ExtractInt();
  SimulateTapDownAt(web_contents, gfx::Point(left + 1, top + 1));
  SimulateTapAt(web_contents, gfx::Point(left + 1, top + 1));
  type_observer_auto.Wait();
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserMockIMETest,
                       DontShowVKOnJSFocus) {
  // The keyboard input pane events are not supported on Win7.
  if (base::win::GetVersion() <= base::win::Version::WIN7) {
    return;
  }

  const char kVirtualKeyboardDataURL[] =
      "data:text/html,<!DOCTYPE html>"
      "<body>"
      "<textarea id='txt3'></textarea>"
      "<script>"
      " let elemRect = txt3.getBoundingClientRect();"
      " txt3.focus();"
      "</script>"
      "</body>";
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  TextInputManagerShowImeIfNeededObserver show_ime_observer_false(web_contents,
                                                                  false);
  // Note: This data URL has JS that focuses the edit control.
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kVirtualKeyboardDataURL)));
  show_ime_observer_false.Wait();

  // Send a touch event so that RenderWidgetHostViewAura will create the
  // keyboard observer (requires last_pointer_type_ to be TOUCH).
  // Tap on the third textarea to open VK.
  TextInputManagerShowImeIfNeededObserver show_ime_observer_true(web_contents,
                                                                 true);
  const int top = EvalJs(shell(), "elemRect.top").ExtractInt();
  const int left = EvalJs(shell(), "elemRect.left").ExtractInt();
  SimulateTapDownAt(web_contents, gfx::Point(left + 1, top + 1));
  SimulateTapAt(web_contents, gfx::Point(left + 1, top + 1));
  show_ime_observer_true.Wait();
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserMockIMETest,
                       ShowAndThenHideVK) {
  // The keyboard input pane events are not supported on Win7.
  if (base::win::GetVersion() <= base::win::Version::WIN7) {
    return;
  }

  const char kVirtualKeyboardDataURL[] =
      "data:text/html,<!DOCTYPE html>"
      "<body>"
      "<textarea id='txt3' virtualkeyboardpolicy='manual' "
      "onfocusin='FocusIn1()' onkeydown='HideVKCalled()'></textarea>"
      "<script>"
      " let elemRect = txt3.getBoundingClientRect();"
      " function FocusIn1() {"
      "   navigator.virtualKeyboard.show();"
      "  }"
      " function HideVKCalled() {"
      "   navigator.virtualKeyboard.hide();"
      "  }"
      "</script>"
      "</body>";
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kVirtualKeyboardDataURL)));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  auto* root = web_contents->GetFrameTree()->root();
  web_contents->GetFrameTree()->SetFocusedFrame(
      root, root->current_frame_host()->GetSiteInstance());

  // Send a touch event so that RenderWidgetHostViewAura will create the
  // keyboard observer (requires last_pointer_type_ to be TOUCH).
  // Tap on the third textarea to open VK.
  TextInputManagerVkVisibilityRequestObserver type_observer_show(
      web_contents, ui::mojom::VirtualKeyboardVisibilityRequest::SHOW);
  const int top = EvalJs(shell(), "elemRect.top").ExtractInt();
  const int left = EvalJs(shell(), "elemRect.left").ExtractInt();
  SimulateTapDownAt(web_contents, gfx::Point(left + 1, top + 1));
  SimulateTapAt(web_contents, gfx::Point(left + 1, top + 1));
  type_observer_show.Wait();
  TextInputManagerVkVisibilityRequestObserver type_observer_hide(
      web_contents, ui::mojom::VirtualKeyboardVisibilityRequest::HIDE);
  SimulateKeyPress(web_contents, ui::DomKey::ENTER, ui::DomCode::ENTER,
                   ui::VKEY_RETURN, false, false, false, false);
  type_observer_hide.Wait();
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserMockIMETest,
                       ShowAndThenHideVKInEditContext) {
  // The keyboard input pane events are not supported on Win7.
  if (base::win::GetVersion() <= base::win::Version::WIN7) {
    return;
  }

  const char kVirtualKeyboardDataURL[] =
      "data:text/html,<!DOCTYPE html>"
      "<body>"
      "<textarea id='txt3' virtualkeyboardpolicy='manual' "
      "onfocusin='FocusIn1()' onkeydown='HideVKCalled()'></textarea>"
      "<script>"
      " let elemRect = txt3.getBoundingClientRect();"
      " const editContext = new EditContext();"
      " editContext.inputPanelPolicy = \"manual\";"
      " function FocusIn1() {"
      "   editContext.focus();"
      "   navigator.virtualKeyboard.show();"
      "  }"
      " function HideVKCalled() {"
      "   navigator.virtualKeyboard.hide();"
      "  }"
      "</script>"
      "</body>";
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kVirtualKeyboardDataURL)));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  auto* root = web_contents->GetFrameTree()->root();
  web_contents->GetFrameTree()->SetFocusedFrame(
      root, root->current_frame_host()->GetSiteInstance());

  // Send a touch event so that RenderWidgetHostViewAura will create the
  // keyboard observer (requires last_pointer_type_ to be TOUCH).
  // Tap on the third textarea to open VK.
  TextInputManagerVkVisibilityRequestObserver type_observer_show(
      web_contents, ui::mojom::VirtualKeyboardVisibilityRequest::SHOW);
  const int top = EvalJs(shell(), "elemRect.top").ExtractInt();
  const int left = EvalJs(shell(), "elemRect.left").ExtractInt();
  SimulateTapDownAt(web_contents, gfx::Point(left + 1, top + 1));
  SimulateTapAt(web_contents, gfx::Point(left + 1, top + 1));
  type_observer_show.Wait();
  TextInputManagerVkVisibilityRequestObserver type_observer_hide(
      web_contents, ui::mojom::VirtualKeyboardVisibilityRequest::HIDE);
  SimulateKeyPress(web_contents, ui::DomKey::ENTER, ui::DomCode::ENTER,
                   ui::VKEY_RETURN, false, false, false, false);
  type_observer_hide.Wait();
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserMockIMETest,
                       VKVisibilityRequestInDeletedDocument) {
  // The keyboard input pane events are not supported on Win7.
  if (base::win::GetVersion() <= base::win::Version::WIN7) {
    return;
  }

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
  auto* root = web_contents->GetFrameTree()->root();
  web_contents->GetFrameTree()->SetFocusedFrame(
      root, root->current_frame_host()->GetSiteInstance());

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
#endif  // #ifdef OS_WIN

}  // namespace content
