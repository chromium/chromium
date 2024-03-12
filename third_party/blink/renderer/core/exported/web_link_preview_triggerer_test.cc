// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_link_preview_triggerer.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace blink {

class MockWebLinkPreviewTriggerer : public WebLinkPreviewTriggerer {
 public:
  MockWebLinkPreviewTriggerer() = default;
  ~MockWebLinkPreviewTriggerer() override = default;

  int LastKeyEventModifiers() const { return last_key_event_modifiers_; }

  const WebElement& HoverElement() const { return hover_element_; }

  const WebElement& MouseEventElement() const { return mouse_event_element_; }

  const std::optional<blink::WebMouseEvent::Button>& MouseEventButton() const {
    return mouse_event_button_;
  }

  const std::optional<int>& MouseEventClickCount() const {
    return mouse_event_click_count_;
  }

  void MaybeChangedKeyEventModifier(int modifiers) override {
    last_key_event_modifiers_ = modifiers;
  }

  void DidChangeHoverElement(blink::WebElement element) override {
    hover_element_ = element;
  }

  void DidAnchorElementReceiveMouseDownEvent(
      blink::WebElement anchor_element,
      blink::WebMouseEvent::Button button,
      int click_count) override {
    mouse_event_element_ = anchor_element;
    mouse_event_button_ = button;
    mouse_event_click_count_ = click_count;
  }

 private:
  int last_key_event_modifiers_ = blink::WebInputEvent::kNoModifiers;
  WebElement hover_element_;
  WebElement mouse_event_element_;
  std::optional<blink::WebMouseEvent::Button> mouse_event_button_;
  std::optional<int> mouse_event_click_count_;
};

class WebLinkPreviewTriggererTest : public PageTestBase {
 protected:
  void Initialize() {
    LocalFrame* local_frame = GetDocument().GetFrame();
    CHECK(local_frame);

    local_frame->SetLinkPreviewTriggererForTesting(
        std::make_unique<MockWebLinkPreviewTriggerer>());
  }

  void SetInnerHTML(const String& html) {
    GetDocument().documentElement()->setInnerHTML(html);
  }
};

TEST_F(WebLinkPreviewTriggererTest, MaybeChangedKeyEventModifierCalled) {
  Initialize();
  SetHtmlInnerHTML("<div></div>");
  MockWebLinkPreviewTriggerer* triggerer =
      static_cast<MockWebLinkPreviewTriggerer*>(
          GetDocument().GetFrame()->GetOrCreateLinkPreviewTriggerer());

  EXPECT_EQ(WebInputEvent::kNoModifiers, triggerer->LastKeyEventModifiers());

  WebKeyboardEvent e0{WebInputEvent::Type::kRawKeyDown, WebInputEvent::kAltKey,
                      WebInputEvent::GetStaticTimeStampForTests()};
  e0.dom_code = static_cast<int>(ui::DomCode::ALT_LEFT);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e0);

  EXPECT_EQ(WebInputEvent::kAltKey, triggerer->LastKeyEventModifiers());

  WebKeyboardEvent e1{WebInputEvent::Type::kKeyUp, WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests()};
  e1.dom_code = static_cast<int>(ui::DomCode::ALT_LEFT);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e1);

  EXPECT_EQ(WebInputEvent::kNoModifiers, triggerer->LastKeyEventModifiers());
}

TEST_F(WebLinkPreviewTriggererTest,
       MaybeChangedKeyEventModifierCalledWithNoModifiersOnMouseLeave) {
  Initialize();
  SetHtmlInnerHTML("<div></div>");
  MockWebLinkPreviewTriggerer* triggerer =
      static_cast<MockWebLinkPreviewTriggerer*>(
          GetDocument().GetFrame()->GetOrCreateLinkPreviewTriggerer());

  EXPECT_EQ(WebInputEvent::kNoModifiers, triggerer->LastKeyEventModifiers());

  WebKeyboardEvent e0{WebInputEvent::Type::kRawKeyDown, WebInputEvent::kAltKey,
                      WebInputEvent::GetStaticTimeStampForTests()};
  e0.dom_code = static_cast<int>(ui::DomCode::ALT_LEFT);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e0);

  EXPECT_EQ(WebInputEvent::kAltKey, triggerer->LastKeyEventModifiers());

  WebMouseEvent e1(WebMouseEvent::Type::kMouseLeave, gfx::PointF(262, 29),
                   gfx::PointF(329, 67),
                   WebPointerProperties::Button::kNoButton, 1,
                   WebInputEvent::Modifiers::kNoModifiers,
                   WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseLeaveEvent(e1);

  EXPECT_EQ(WebInputEvent::kNoModifiers, triggerer->LastKeyEventModifiers());
}

TEST_F(WebLinkPreviewTriggererTest, DidChangeHoverElementCalledOnHoverChanged) {
  Initialize();
  SetHtmlInnerHTML(
      "<style>"
      "  body { margin:0px; }"
      "  a { display:block; width:100px; height:100px; }"
      "</style>"
      "<body>"
      "  <a href=\"https://example.com\">anchor</a>"
      "</body>");
  MockWebLinkPreviewTriggerer* triggerer =
      static_cast<MockWebLinkPreviewTriggerer*>(
          GetDocument().GetFrame()->GetOrCreateLinkPreviewTriggerer());

  {
    gfx::PointF point(50, 50);
    WebMouseEvent mouse_move_event(WebInputEvent::Type::kMouseMove, point,
                                   point,
                                   WebPointerProperties::Button::kNoButton, 0,
                                   WebInputEvent::Modifiers::kNoModifiers,
                                   WebInputEvent::GetStaticTimeStampForTests());

    GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
        mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

    EXPECT_FALSE(triggerer->HoverElement().IsNull());
    EXPECT_EQ("A", triggerer->HoverElement().TagName());
    EXPECT_EQ("https://example.com",
              triggerer->HoverElement().GetAttribute("href"));
  }

  {
    gfx::PointF point(200, 200);
    WebMouseEvent mouse_move_event(WebInputEvent::Type::kMouseMove, point,
                                   point,
                                   WebPointerProperties::Button::kNoButton, 0,
                                   WebInputEvent::Modifiers::kNoModifiers,
                                   WebInputEvent::GetStaticTimeStampForTests());

    GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
        mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

    EXPECT_FALSE(triggerer->HoverElement().IsNull());
    EXPECT_EQ("HTML", triggerer->HoverElement().TagName());
  }
}

TEST_F(WebLinkPreviewTriggererTest,
       DidAnchorElementReceiveMouseDownEventCalledOnMousePress) {
  Initialize();
  SetHtmlInnerHTML(
      "<style>"
      "  body { margin:0px; }"
      "  a { display:block; width:100px; height:100px; }"
      "</style>"
      "<body>"
      "  <a href=\"https://example.com\">anchor</a>"
      "</body>");
  MockWebLinkPreviewTriggerer* triggerer =
      static_cast<MockWebLinkPreviewTriggerer*>(
          GetDocument().GetFrame()->GetOrCreateLinkPreviewTriggerer());

  gfx::PointF point(50, 50);
  WebMouseEvent mouse_down_event(WebInputEvent::Type::kMouseDown, point, point,
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kNoModifiers,
                                 WebInputEvent::GetStaticTimeStampForTests());

  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);

  EXPECT_FALSE(triggerer->MouseEventElement().IsNull());
  EXPECT_EQ("https://example.com",
            triggerer->MouseEventElement().GetAttribute("href"));
  EXPECT_EQ(WebMouseEvent::Button::kLeft, triggerer->MouseEventButton());
  EXPECT_EQ(1, triggerer->MouseEventClickCount());
}

}  // namespace blink
