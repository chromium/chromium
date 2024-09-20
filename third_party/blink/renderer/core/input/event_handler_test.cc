// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/event_handler.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_pointer_event.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/dom_selection.h"
#include "third_party/blink/renderer/core/editing/editing_behavior.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_controller.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/testing/selection_sample.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-blink.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-blink.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"

namespace blink {

class EventHandlerTest : public PageTestBase {
 protected:
  void SetUp() override;
  void SetHtmlInnerHTML(const char* html_content);
  ShadowRoot* SetShadowContent(const char* shadow_content, const char* host);
};

class EventHandlerSimTest : public SimTest {
 public:
  void InitializeMousePositionAndActivateView(float x, float y) {
    WebMouseEvent mouse_move_event(WebMouseEvent::Type::kMouseMove,
                                   gfx::PointF(x, y), gfx::PointF(x, y),
                                   WebPointerProperties::Button::kNoButton, 0,
                                   WebInputEvent::Modifiers::kNoModifiers,
                                   WebInputEvent::GetStaticTimeStampForTests());
    GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
        mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

    WebView().MainFrameWidget()->SetFocus(true);
    WebView().SetIsActive(true);
  }

  void InjectScrollFromGestureEvents(cc::ElementId element_id,
                                     float delta_x,
                                     float delta_y) {
    WebGestureEvent gesture_scroll_begin{
        WebInputEvent::Type::kGestureScrollBegin, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests()};
    gesture_scroll_begin.data.scroll_begin.delta_x_hint = 0;
    gesture_scroll_begin.data.scroll_begin.delta_y_hint = -delta_y;
    gesture_scroll_begin.data.scroll_begin.scrollable_area_element_id =
        element_id.GetInternalValue();
    WebView().MainFrameWidget()->HandleInputEvent(
        WebCoalescedInputEvent(gesture_scroll_begin, ui::LatencyInfo()));

    WebGestureEvent gesture_scroll_update{
        WebInputEvent::Type::kGestureScrollUpdate, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests()};
    gesture_scroll_update.data.scroll_update.delta_x = delta_x;
    gesture_scroll_update.data.scroll_update.delta_y = -delta_y;
    WebView().MainFrameWidget()->HandleInputEvent(
        WebCoalescedInputEvent(gesture_scroll_update, ui::LatencyInfo()));

    WebGestureEvent gesture_scroll_end{
        WebInputEvent::Type::kGestureScrollEnd, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests()};
    WebView().MainFrameWidget()->HandleInputEvent(
        WebCoalescedInputEvent(gesture_scroll_end, ui::LatencyInfo()));
  }

  void DispatchElementTargetedGestureScroll(
      const WebGestureEvent& gesture_event) {
    GetWebFrameWidget().DispatchThroughCcInputHandler(gesture_event);
  }
};

WebPointerEvent CreateMinimalTouchPointerEvent(WebInputEvent::Type type,
                                               gfx::PointF position) {
  WebPointerEvent event(
      type,
      WebPointerProperties(1, WebPointerProperties::PointerType::kTouch,
                           WebPointerProperties::Button::kLeft, position,
                           position),
      1, 1);
  event.SetFrameScale(1);
  return event;
}

WebGestureEvent CreateMinimalGestureEvent(WebInputEvent::Type type,
                                          gfx::PointF position) {
  WebGestureEvent event(type, WebInputEvent::kNoModifiers,
                        base::TimeTicks::Now(), WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(position);
  event.SetPositionInScreen(position);
  event.data.long_press.width = 5;
  event.data.long_press.height = 5;
  event.SetFrameScale(1);
  return event;
}

// TODO(mustaq): We no longer needs any of these Builder classes because the
// fields are publicly modifiable.

class TapEventBuilder : public WebGestureEvent {
 public:
  TapEventBuilder(gfx::PointF position, int tap_count)
      : WebGestureEvent(WebInputEvent::Type::kGestureTap,
                        WebInputEvent::kNoModifiers,
                        base::TimeTicks::Now(),
                        WebGestureDevice::kTouchscreen) {
    SetPositionInWidget(position);
    SetPositionInScreen(position);
    data.tap.tap_count = tap_count;
    data.tap.width = 5;
    data.tap.height = 5;
    frame_scale_ = 1;
  }
};

class TapDownEventBuilder : public WebGestureEvent {
 public:
  explicit TapDownEventBuilder(gfx::PointF position)
      : WebGestureEvent(WebInputEvent::Type::kGestureTapDown,
                        WebInputEvent::kNoModifiers,
                        base::TimeTicks::Now(),
                        WebGestureDevice::kTouchscreen) {
    SetPositionInWidget(position);
    SetPositionInScreen(position);
    data.tap_down.width = 5;
    data.tap_down.height = 5;
    frame_scale_ = 1;
  }
};

class ShowPressEventBuilder : public WebGestureEvent {
 public:
  explicit ShowPressEventBuilder(gfx::PointF position)
      : WebGestureEvent(WebInputEvent::Type::kGestureShowPress,
                        WebInputEvent::kNoModifiers,
                        base::TimeTicks::Now(),
                        WebGestureDevice::kTouchscreen) {
    SetPositionInWidget(position);
    SetPositionInScreen(position);
    data.show_press.width = 5;
    data.show_press.height = 5;
    frame_scale_ = 1;
  }
};

class LongPressEventBuilder : public WebGestureEvent {
 public:
  explicit LongPressEventBuilder(gfx::PointF position)
      : WebGestureEvent(WebInputEvent::Type::kGestureLongPress,
                        WebInputEvent::kNoModifiers,
                        base::TimeTicks::Now(),
                        WebGestureDevice::kTouchscreen) {
    SetPositionInWidget(position);
    SetPositionInScreen(position);
    data.long_press.width = 5;
    data.long_press.height = 5;
    frame_scale_ = 1;
  }
};

class MousePressEventBuilder : public WebMouseEvent {
 public:
  MousePressEventBuilder(gfx::Point position_param,
                         int click_count_param,
                         WebMouseEvent::Button button_param)
      : WebMouseEvent(WebInputEvent::Type::kMouseDown,
                      WebInputEvent::kNoModifiers,
                      base::TimeTicks::Now()) {
    click_count = click_count_param;
    button = button_param;
    SetPositionInWidget(position_param.x(), position_param.y());
    SetPositionInScreen(position_param.x(), position_param.y());
    frame_scale_ = 1;
  }
};

void EventHandlerTest::SetUp() {
  PageTestBase::SetUp(gfx::Size(300, 400));
}

void EventHandlerTest::SetHtmlInnerHTML(const char* html_content) {
  GetDocument().documentElement()->setInnerHTML(String::FromUTF8(html_content));
  UpdateAllLifecyclePhasesForTest();
}

ShadowRoot* EventHandlerTest::SetShadowContent(const char* shadow_content,
                                               const char* host) {
  ShadowRoot* shadow_root =
      EditingTestBase::CreateShadowRootForElementWithIDAndSetInnerHTML(
          GetDocument(), host, shadow_content);
  return shadow_root;
}

TEST_F(EventHandlerTest, dragSelectionAfterScroll) {
  SetHtmlInnerHTML(
      "<style> body { margin: 0px; } .upper { width: 300px; height: 400px; }"
      ".lower { margin: 0px; width: 300px; height: 400px; } .line { display: "
      "block; width: 300px; height: 30px; } </style>"
      "<div class='upper'></div>"
      "<div class='lower'>"
      "<span class='line'>Line 1</span><span class='line'>Line 2</span><span "
      "class='line'>Line 3</span><span class='line'>Line 4</span><span "
      "class='line'>Line 5</span>"
      "<span class='line'>Line 6</span><span class='line'>Line 7</span><span "
      "class='line'>Line 8</span><span class='line'>Line 9</span><span "
      "class='line'>Line 10</span>"
      "</div>");

  LocalFrameView* frame_view = GetDocument().View();
  frame_view->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 400), mojom::blink::ScrollType::kProgrammatic);

  WebMouseEvent mouse_down_event(WebInputEvent::Type::kMouseDown,
                                 gfx::PointF(0, 0), gfx::PointF(100, 200),
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);

  ASSERT_TRUE(GetDocument()
                  .GetFrame()
                  ->GetEventHandler()
                  .GetSelectionController()
                  .MouseDownMayStartSelect());

  WebMouseEvent mouse_move_event(WebInputEvent::Type::kMouseMove,
                                 gfx::PointF(100, 50), gfx::PointF(200, 250),
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  GetPage().GetAutoscrollController().Animate();
  GetPage().Animator().ServiceScriptedAnimations(base::TimeTicks::Now());

  WebMouseEvent mouse_up_event(
      WebMouseEvent::Type::kMouseUp, gfx::PointF(100, 50),
      gfx::PointF(200, 250), WebPointerProperties::Button::kLeft, 1,
      WebInputEvent::kNoModifiers, WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(
      mouse_up_event);

  ASSERT_FALSE(GetDocument()
                   .GetFrame()
                   ->GetEventHandler()
                   .GetSelectionController()
                   .MouseDownMayStartSelect());

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  Range* range =
      CreateRange(EphemeralRange(Selection().GetSelectionInDOMTree().Anchor(),
                                 Selection().GetSelectionInDOMTree().Focus()));
  ASSERT_TRUE(range);
  EXPECT_EQ("Line 1\nLine 2", range->GetText());
}

TEST_F(EventHandlerTest, multiClickSelectionFromTap) {
  SetHtmlInnerHTML(
      "<style> body { margin: 0px; } .line { display: block; width: 300px; "
      "height: 30px; } </style>"
      "<body contenteditable='true'><span class='line' id='line'>One Two "
      "Three</span></body>");

  Node* line = GetDocument().getElementById(AtomicString("line"))->firstChild();

  TapEventBuilder single_tap_event(gfx::PointF(0, 0), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);
  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_EQ(Position(line, 0), Selection().GetSelectionInDOMTree().Anchor());

  // Multi-tap events on editable elements should trigger selection, just
  // like multi-click events.
  TapEventBuilder double_tap_event(gfx::PointF(0, 0), 2);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      double_tap_event);
  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_EQ(Position(line, 0), Selection().GetSelectionInDOMTree().Anchor());
  if (GetDocument()
          .GetFrame()
          ->GetEditor()
          .IsSelectTrailingWhitespaceEnabled()) {
    EXPECT_EQ(Position(line, 4), Selection().GetSelectionInDOMTree().Focus());
    EXPECT_EQ("One ", Selection().SelectedText().Utf8());
  } else {
    EXPECT_EQ(Position(line, 3), Selection().GetSelectionInDOMTree().Focus());
    EXPECT_EQ("One", Selection().SelectedText().Utf8());
  }

  TapEventBuilder triple_tap_event(gfx::PointF(0, 0), 3);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      triple_tap_event);
  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_EQ(Position(line, 0), Selection().GetSelectionInDOMTree().Anchor());
  EXPECT_EQ(Position(line, 13), Selection().GetSelectionInDOMTree().Focus());
  EXPECT_EQ("One Two Three", Selection().SelectedText().Utf8());
}

TEST_F(EventHandlerTest, multiClickSelectionFromTapDisabledIfNotEditable) {
  SetHtmlInnerHTML(
      "<style> body { margin: 0px; } .line { display: block; width: 300px; "
      "height: 30px; } </style>"
      "<span class='line' id='line'>One Two Three</span>");

  Node* line = GetDocument().getElementById(AtomicString("line"))->firstChild();

  TapEventBuilder single_tap_event(gfx::PointF(0, 0), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);
  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_EQ(Position(line, 0), Selection().GetSelectionInDOMTree().Anchor());

  // As the text is readonly, multi-tap events should not trigger selection.
  TapEventBuilder double_tap_event(gfx::PointF(0, 0), 2);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      double_tap_event);
  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_EQ(Position(line, 0), Selection().GetSelectionInDOMTree().Anchor());

  TapEventBuilder triple_tap_event(gfx::PointF(0, 0), 3);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      triple_tap_event);
  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_EQ(Position(line, 0), Selection().GetSelectionInDOMTree().Anchor());
}

TEST_F(EventHandlerTest, draggedInlinePositionTest) {
  SetHtmlInnerHTML(
      "<style>"
      "body { margin: 0px; }"
      ".line { font-family: sans-serif; background: blue; width: 300px; "
      "height: 30px; font-size: 40px; margin-left: 250px; }"
      "</style>"
      "<div style='width: 300px; height: 100px;'>"
      "<span class='line' draggable='true'>abcd</span>"
      "</div>");
  WebMouseEvent mouse_down_event(WebMouseEvent::Type::kMouseDown,
                                 gfx::PointF(262, 29), gfx::PointF(329, 67),
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);

  WebMouseEvent mouse_move_event(WebMouseEvent::Type::kMouseMove,
                                 gfx::PointF(618, 298), gfx::PointF(685, 436),
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  EXPECT_EQ(gfx::Point(12, 29), GetDocument()
                                    .GetFrame()
                                    ->GetEventHandler()
                                    .DragDataTransferLocationForTesting());
}

TEST_F(EventHandlerTest, draggedSVGImagePositionTest) {
  SetHtmlInnerHTML(
      "<style>"
      "body { margin: 0px; }"
      "[draggable] {"
      "-webkit-user-select: none; user-select: none; -webkit-user-drag: "
      "element; }"
      "</style>"
      "<div style='width: 300px; height: 100px;'>"
      "<svg width='500' height='500'>"
      "<rect x='100' y='100' width='100px' height='100px' fill='blue' "
      "draggable='true'/>"
      "</svg>"
      "</div>");
  WebMouseEvent mouse_down_event(WebMouseEvent::Type::kMouseDown,
                                 gfx::PointF(145, 144), gfx::PointF(212, 282),
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);

  WebMouseEvent mouse_move_event(WebMouseEvent::Type::kMouseMove,
                                 gfx::PointF(618, 298), gfx::PointF(685, 436),
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  EXPECT_EQ(gfx::Point(45, 44), GetDocument()
                                    .GetFrame()
                                    ->GetEventHandler()
                                    .DragDataTransferLocationForTesting());
}

TEST_F(EventHandlerTest, HitOnNothingDoesNotShowIBeam) {
  SetHtmlInnerHTML("");
  HitTestLocation location((PhysicalOffset(10, 10)));
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_FALSE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(
          GetDocument().body(), hit));
}

TEST_F(EventHandlerTest, HitOnTextShowsIBeam) {
  SetHtmlInnerHTML("blabla");
  Node* const text = GetDocument().body()->firstChild();
  HitTestLocation location(
      text->GetLayoutObject()->AbsoluteBoundingBoxRect().CenterPoint());
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_TRUE(text->CanStartSelection());
  EXPECT_TRUE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, HitOnUserSelectNoneDoesNotShowIBeam) {
  SetHtmlInnerHTML("<span style='user-select: none'>blabla</span>");
  Node* const text = GetDocument().body()->firstChild()->firstChild();
  HitTestLocation location(
      text->GetLayoutObject()->AbsoluteBoundingBoxRect().CenterPoint());
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_FALSE(text->CanStartSelection());
  EXPECT_FALSE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, ShadowChildCanOverrideUserSelectNone) {
  SetHtmlInnerHTML("<p style='user-select: none' id='host'></p>");
  ShadowRoot* const shadow_root = SetShadowContent(
      "<span style='user-select: text' id='bla'>blabla</span>", "host");

  Node* const text =
      shadow_root->getElementById(AtomicString("bla"))->firstChild();
  HitTestLocation location(
      text->GetLayoutObject()->AbsoluteBoundingBoxRect().CenterPoint());
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_TRUE(text->CanStartSelection());
  EXPECT_TRUE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, UserSelectAllCanOverrideUserSelectNone) {
  SetHtmlInnerHTML(
      "<div style='user-select: none'>"
      "<span style='user-select: all'>blabla</span>"
      "</div>");
  Node* const text =
      GetDocument().body()->firstChild()->firstChild()->firstChild();
  HitTestLocation location(
      text->GetLayoutObject()->AbsoluteBoundingBoxRect().CenterPoint());
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_TRUE(text->CanStartSelection());
  EXPECT_TRUE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, UserSelectNoneCanOverrideUserSelectAll) {
  SetHtmlInnerHTML(
      "<div style='user-select: all'>"
      "<span style='user-select: none'>blabla</span>"
      "</div>");
  Node* const text =
      GetDocument().body()->firstChild()->firstChild()->firstChild();
  HitTestLocation location(
      text->GetLayoutObject()->AbsoluteBoundingBoxRect().CenterPoint());
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_FALSE(text->CanStartSelection());
  EXPECT_FALSE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, UserSelectTextCanOverrideUserSelectNone) {
  SetHtmlInnerHTML(
      "<div style='user-select: none'>"
      "<span style='user-select: text'>blabla</span>"
      "</div>");
  Node* const text =
      GetDocument().body()->firstChild()->firstChild()->firstChild();
  HitTestLocation location(
      text->GetLayoutObject()->AbsoluteBoundingBoxRect().CenterPoint());
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_TRUE(text->CanStartSelection());
  EXPECT_TRUE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, UserSelectNoneCanOverrideUserSelectText) {
  SetHtmlInnerHTML(
      "<div style='user-select: text'>"
      "<span style='user-select: none'>blabla</span>"
      "</div>");
  Node* const text = GetDocument().body()->firstChild()->firstChild()->firstChild();
  HitTestLocation location(
      text->GetLayoutObject()->AbsoluteBoundingBoxRect().CenterPoint());
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_FALSE(text->CanStartSelection());
  EXPECT_FALSE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, ShadowChildCanOverrideUserSelectText) {
  SetHtmlInnerHTML("<p style='user-select: text' id='host'></p>");
  ShadowRoot* const shadow_root = SetShadowContent(
      "<span style='user-select: none' id='bla'>blabla</span>", "host");

  Node* const text =
      shadow_root->getElementById(AtomicString("bla"))->firstChild();
  HitTestLocation location(
      text->GetLayoutObject()->AbsoluteBoundingBoxRect().CenterPoint());
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_FALSE(text->CanStartSelection());
  EXPECT_FALSE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, InputFieldsCanStartSelection) {
  SetHtmlInnerHTML("<input value='blabla'>");
  auto* const field = To<HTMLInputElement>(GetDocument().body()->firstChild());
  Element* const text = field->InnerEditorElement();
  HitTestLocation location(
      text->GetLayoutObject()->AbsoluteBoundingBoxRect().CenterPoint());
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_TRUE(text->CanStartSelection());
  EXPECT_TRUE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, ReadOnlyInputDoesNotInheritUserSelect) {
  SetHtmlInnerHTML(
      "<div style='user-select: none'>"
      "<input id='sample' readonly value='blabla'>"
      "</div>");
  auto* const input = To<HTMLInputElement>(
      GetDocument().getElementById(AtomicString("sample")));
  Node* const text = input->InnerEditorElement()->firstChild();

  HitTestLocation location(
      text->GetLayoutObject()->AbsoluteBoundingBoxRect().CenterPoint());
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_TRUE(text->CanStartSelection());
  EXPECT_TRUE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, ImagesCannotStartSelection) {
  SetHtmlInnerHTML("<img>");
  auto* const img = To<Element>(GetDocument().body()->firstChild());
  HitTestLocation location(
      img->GetLayoutObject()->AbsoluteBoundingBoxRect().CenterPoint());
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_FALSE(img->CanStartSelection());
  EXPECT_FALSE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(img,
                                                                         hit));
}

TEST_F(EventHandlerTest, AnchorTextCannotStartSelection) {
  SetHtmlInnerHTML("<a href='bala'>link text</a>");
  Node* const link = GetDocument().body()->firstChild();
  HitTestLocation location(
      link->GetLayoutObject()->AbsoluteBoundingBoxRect().CenterPoint());
  HitTestResult result =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  Node* const text = link->firstChild();
  EXPECT_FALSE(text->CanStartSelection());
  EXPECT_TRUE(result.IsOverLink());
  // ShouldShowIBeamForNode() returns |cursor: auto|'s value.
  // In https://github.com/w3c/csswg-drafts/issues/1598 it was decided that:
  // a { cursor: auto } /* gives I-beam over links */
  EXPECT_TRUE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(
          text, result));
  EXPECT_EQ(GetDocument()
                .GetFrame()
                ->GetEventHandler()
                .SelectCursor(location, result)
                .value()
                .type(),
            ui::mojom::blink::CursorType::kHand);  // A hand signals ability to
                                                   // navigate.
}

TEST_F(EventHandlerTest, EditableAnchorTextCanStartSelection) {
  SetHtmlInnerHTML("<a contenteditable='true' href='bala'>editable link</a>");
  Node* const link = GetDocument().body()->firstChild();
  HitTestLocation location(
      link->GetLayoutObject()->AbsoluteBoundingBoxRect().CenterPoint());
  HitTestResult result =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  Node* const text = link->firstChild();
  EXPECT_TRUE(text->CanStartSelection());
  EXPECT_TRUE(result.IsOverLink());
  EXPECT_TRUE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(
          text, result));
  EXPECT_EQ(
      GetDocument()
          .GetFrame()
          ->GetEventHandler()
          .SelectCursor(location, result)
          .value()
          .type(),
      ui::mojom::blink::CursorType::kIBeam);  // An I-beam signals editability.
}

TEST_F(EventHandlerTest, CursorForVerticalResizableTextArea) {
  SetHtmlInnerHTML("<textarea style='resize:vertical'>vertical</textarea>");
  Node* const element = GetDocument().body()->firstChild();
  gfx::Point point =
      element->GetLayoutObject()->AbsoluteBoundingBoxRect().bottom_right();
  point.Offset(-5, -5);
  HitTestLocation location(point);
  HitTestResult result =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_EQ(GetDocument()
                .GetFrame()
                ->GetEventHandler()
                .SelectCursor(location, result)
                .value()
                .type(),
            // A north-south resize signals vertical resizability.
            ui::mojom::blink::CursorType::kNorthSouthResize);
}

TEST_F(EventHandlerTest, CursorForHorizontalResizableTextArea) {
  SetHtmlInnerHTML("<textarea style='resize:horizontal'>horizontal</textarea>");
  Node* const element = GetDocument().body()->firstChild();
  gfx::Point point =
      element->GetLayoutObject()->AbsoluteBoundingBoxRect().bottom_right();
  point.Offset(-5, -5);
  HitTestLocation location(point);
  HitTestResult result =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_EQ(GetDocument()
                .GetFrame()
                ->GetEventHandler()
                .SelectCursor(location, result)
                .value()
                .type(),
            // An east-west resize signals horizontal resizability.
            ui::mojom::blink::CursorType::kEastWestResize);
}

TEST_F(EventHandlerTest, CursorForResizableTextArea) {
  SetHtmlInnerHTML("<textarea style='resize:both'>both</textarea>");
  Node* const element = GetDocument().body()->firstChild();
  gfx::Point point =
      element->GetLayoutObject()->AbsoluteBoundingBoxRect().bottom_right();
  point.Offset(-5, -5);
  HitTestLocation location(point);
  HitTestResult result =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_EQ(GetDocument()
                .GetFrame()
                ->GetEventHandler()
                .SelectCursor(location, result)
                .value()
                .type(),
            // An south-east resize signals both horizontal and
            // vertical resizability.
            ui::mojom::blink::CursorType::kSouthEastResize);
}

TEST_F(EventHandlerTest, CursorForRtlResizableTextArea) {
  SetHtmlInnerHTML(
      "<textarea style='resize:both;direction:rtl'>both</textarea>");
  Node* const element = GetDocument().body()->firstChild();
  gfx::Point point =
      element->GetLayoutObject()->AbsoluteBoundingBoxRect().bottom_left();
  point.Offset(5, -5);
  HitTestLocation location(point);
  HitTestResult result =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_EQ(GetDocument()
                .GetFrame()
                ->GetEventHandler()
                .SelectCursor(location, result)
                .value()
                .type(),
            // An south-west resize signals both horizontal and
            // vertical resizability when direction is RTL.
            ui::mojom::blink::CursorType::kSouthWestResize);
}

TEST_F(EventHandlerTest, CursorForInlineVerticalWritingMode) {
  SetHtmlInnerHTML(
      "Test<p style='resize:both;writing-mode:vertical-lr;"
      "width:30px;height:30px;overflow:hidden;display:inline'>Test "
      "Test</p>Test");
  Node* const element = GetDocument().body()->firstChild()->nextSibling();
  gfx::Point point =
      element->GetLayoutObject()->AbsoluteBoundingBoxRect().origin();
  point.Offset(25, 25);
  HitTestLocation location(point);
  HitTestResult result =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_EQ(GetDocument()
                .GetFrame()
                ->GetEventHandler()
                .SelectCursor(location, result)
                .value()
                .type(),
            ui::mojom::blink::CursorType::kSouthEastResize);
}

TEST_F(EventHandlerTest, CursorForBlockVerticalWritingMode) {
  SetHtmlInnerHTML(
      "Test<p style='resize:both;writing-mode:vertical-lr;"
      "width:30px;height:30px;overflow:hidden;display:block'>Test "
      "Test</p>Test");
  Node* const element = GetDocument().body()->firstChild()->nextSibling();
  gfx::Point point =
      element->GetLayoutObject()->AbsoluteBoundingBoxRect().origin();
  point.Offset(25, 25);
  HitTestLocation location(point);
  HitTestResult result =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_EQ(GetDocument()
                .GetFrame()
                ->GetEventHandler()
                .SelectCursor(location, result)
                .value()
                .type(),
            ui::mojom::blink::CursorType::kSouthEastResize);
}

TEST_F(EventHandlerTest, implicitSend) {
  SetHtmlInnerHTML("<button>abc</button>");
  GetDocument().GetSettings()->SetSpatialNavigationEnabled(true);

  WebKeyboardEvent e{WebInputEvent::Type::kRawKeyDown,
                     WebInputEvent::kNoModifiers,
                     WebInputEvent::GetStaticTimeStampForTests()};
  e.dom_code = static_cast<int>(ui::DomCode::ARROW_DOWN);
  e.dom_key = ui::DomKey::ARROW_DOWN;
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);

  // TODO(crbug.com/949766) Should cleanup these magic numbers.
  e.dom_code = 0;
  e.dom_key = 0x00200310;
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
}

// Regression test for http://crbug.com/641403 to verify we use up-to-date
// layout tree for dispatching "contextmenu" event.
TEST_F(EventHandlerTest, sendContextMenuEventWithHover) {
  SetHtmlInnerHTML(
      "<style>*:hover { color: red; }</style>"
      "<div>foo</div>");
  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
      "document.addEventListener('contextmenu', event => "
      "event.preventDefault());");
  GetDocument().body()->AppendChild(script);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position(GetDocument().body(), 0))
          .Build(),
      SetSelectionOptions());
  WebMouseEvent mouse_down_event(
      WebMouseEvent::Type::kMouseDown, gfx::PointF(0, 0), gfx::PointF(100, 200),
      WebPointerProperties::Button::kRight, 1,
      WebInputEvent::Modifiers::kRightButtonDown, base::TimeTicks::Now());
  EXPECT_EQ(WebInputEventResult::kHandledApplication,
            GetDocument().GetFrame()->GetEventHandler().SendContextMenuEvent(
                mouse_down_event));
}

TEST_F(EventHandlerTest, EmptyTextfieldInsertionOnTap) {
  SetHtmlInnerHTML("<textarea cols=50 rows=50></textarea>");

  TapEventBuilder single_tap_event(gfx::PointF(200, 200), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_FALSE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, NonEmptyTextfieldInsertionOnTap) {
  SetHtmlInnerHTML("<textarea cols=50 rows=50>Enter text</textarea>");

  TapEventBuilder single_tap_event(gfx::PointF(200, 200), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_TRUE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, NewlineDivInsertionOnTap) {
  SetHtmlInnerHTML("<div contenteditable><br/></div>");

  TapEventBuilder single_tap_event(gfx::PointF(10, 10), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_TRUE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, EmptyTextfieldInsertionOnLongPress) {
  SetHtmlInnerHTML("<textarea cols=50 rows=50></textarea>");

  LongPressEventBuilder long_press_event(gfx::PointF(200, 200));
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      long_press_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_TRUE(Selection().IsHandleVisible());

  // Single Tap on an empty edit field should clear insertion handle
  TapEventBuilder single_tap_event(gfx::PointF(200, 200), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_FALSE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, NonEmptyTextfieldInsertionOnLongPress) {
  SetHtmlInnerHTML("<textarea cols=50 rows=50>Enter text</textarea>");

  LongPressEventBuilder long_press_event(gfx::PointF(200, 200));
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      long_press_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_TRUE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, SelectionOnDoublePress) {
  ScopedTouchTextEditingRedesignForTest touch_text_editing_redesign(true);
  SetHtmlInnerHTML(
      R"HTML(
        <div id='targetdiv' style='font-size:500%;width:50px;'>
        <p id='target' contenteditable>Test selection</p>
        </div>
      )HTML");

  Element* element = GetDocument().getElementById(AtomicString("target"));
  gfx::PointF tap_point = gfx::PointF(element->BoundsInWidget().CenterPoint());
  TapDownEventBuilder single_tap_down_event(tap_point);
  single_tap_down_event.data.tap_down.tap_down_count = 1;
  TapEventBuilder single_tap_event(tap_point, 1);
  TapDownEventBuilder double_tap_down_event(tap_point);
  double_tap_down_event.data.tap_down.tap_down_count = 2;
  TapEventBuilder double_tap_event(tap_point, 2);

  // Double press should select nearest word.
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_down_event);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      double_tap_down_event);
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_EQ(Selection().SelectedText(), "selection");

  // Releasing double tap should keep the selection.
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      double_tap_event);
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_EQ(Selection().SelectedText(), "selection");
}

TEST_F(EventHandlerTest, SelectionOnDoublePressPreventDefaultMousePress) {
  ScopedTouchTextEditingRedesignForTest touch_text_editing_redesign(true);
  GetDocument().GetSettings()->SetScriptEnabled(true);
  SetHtmlInnerHTML(
      R"HTML(
        <div id='targetdiv' style='font-size:500%;width:50px;'>
        <p id='target' contenteditable>Test selection</p>
        </div>
      )HTML");
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
      R"HTML(
        let targetDiv = document.getElementById('targetdiv');
        targetDiv.addEventListener('mousedown', (e) => {
          e.preventDefault();
        });
      )HTML");
  GetDocument().body()->AppendChild(script);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Element* element = GetDocument().getElementById(AtomicString("target"));
  gfx::PointF tap_point = gfx::PointF(element->BoundsInWidget().CenterPoint());
  TapDownEventBuilder single_tap_down_event(tap_point);
  single_tap_down_event.data.tap_down.tap_down_count = 1;
  TapEventBuilder single_tap_event(tap_point, 1);
  TapDownEventBuilder double_tap_down_event(tap_point);
  double_tap_down_event.data.tap_down.tap_down_count = 2;
  TapEventBuilder double_tap_event(tap_point, 2);

  // Double press should not select anything.
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_down_event);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      double_tap_down_event);
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());

  // Releasing double tap also should not select anything.
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      double_tap_event);
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
}

TEST_F(EventHandlerTest, ClearHandleAfterTap) {
  SetHtmlInnerHTML("<textarea cols=50  rows=10>Enter text</textarea>");

  // Show handle
  LongPressEventBuilder long_press_event(gfx::PointF(200, 10));
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      long_press_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_TRUE(Selection().IsHandleVisible());

  // Tap away from text area should clear handle
  TapEventBuilder single_tap_event(gfx::PointF(200, 350), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);

  ASSERT_FALSE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, HandleNotShownOnMouseEvents) {
  SetHtmlInnerHTML("<textarea cols=50 rows=50>Enter text</textarea>");

  MousePressEventBuilder left_mouse_press_event(
      gfx::Point(200, 200), 1, WebPointerProperties::Button::kLeft);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      left_mouse_press_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_FALSE(Selection().IsHandleVisible());

  MousePressEventBuilder right_mouse_press_event(
      gfx::Point(200, 200), 1, WebPointerProperties::Button::kRight);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      right_mouse_press_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_FALSE(Selection().IsHandleVisible());

  MousePressEventBuilder double_click_mouse_press_event(
      gfx::Point(200, 200), 2, WebPointerProperties::Button::kLeft);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      double_click_mouse_press_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  ASSERT_FALSE(Selection().IsHandleVisible());

  MousePressEventBuilder triple_click_mouse_press_event(
      gfx::Point(200, 200), 3, WebPointerProperties::Button::kLeft);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      triple_click_mouse_press_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  ASSERT_FALSE(Selection().IsHandleVisible());
}

// https://crbug.com/1410448
TEST_F(EventHandlerTest,
       TripleClickUserSelectNoneParagraphWithSelectableChildren) {
  LoadAhem(*GetDocument().GetFrame());
  InsertStyleElement("body { margin: 0; font: 20px/1 Ahem; }");

  SetBodyInnerHTML(R"HTML(<div style="user-select:none">
        <span style="user-select:text">
          <span style="user-select:text">Hel</span>
          lo
        </span>
        <span style="user-select:text"> lo </span>
        <span style="user-select:text">there</span>
      </div>)HTML");

  MousePressEventBuilder triple_click_mouse_press_event(
      gfx::Point(10, 10), 3, WebPointerProperties::Button::kLeft);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      triple_click_mouse_press_event);

  EXPECT_EQ(R"HTML(<div style="user-select:none">
        <span style="user-select:text">
          <span style="user-select:text">^Hel</span>
          lo
        </span>
        <span style="user-select:text"> lo </span>
        <span style="user-select:text">there|</span>
      </div>)HTML",
            SelectionSample::GetSelectionText(
                *GetDocument().body(), Selection().GetSelectionInDOMTree()));
}

TEST_F(EventHandlerTest, MisspellingContextMenuEvent) {
  if (GetDocument()
          .GetFrame()
          ->GetEditor()
          .Behavior()
          .ShouldSelectOnContextualMenuClick())
    return;

  SetHtmlInnerHTML("<textarea cols=50 rows=50>Mispellinggg</textarea>");

  TapEventBuilder single_tap_event(gfx::PointF(10, 10), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_TRUE(Selection().IsHandleVisible());

  GetDocument().GetFrame()->GetEventHandler().ShowNonLocatedContextMenu(
      nullptr, kMenuSourceTouchHandle);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_TRUE(Selection().IsHandleVisible());
}

// Tests that touch adjustment algorithm can handle editable elements without
// layout objects.
//
// TODO(mustaq): A fix for https://crbug.com/1230045 can make this test
// obsolete.
TEST_F(EventHandlerTest, TouchAdjustmentOnEditableDisplayContents) {
  SetHtmlInnerHTML(
      "<div style='display:contents' contenteditable='true'>TEXT</div>");
  TapEventBuilder single_tap_event(gfx::PointF(1, 1), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);

  LongPressEventBuilder long_press_event(gfx::PointF(1, 1));
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      long_press_event);

  // This test passes if it doesn't crash.
}

// Tests that `EventHandler` can gracefully handle a multi-touch gesture event
// for which the first touch pointer event was NOT sent to Blink but a latter
// touch pointer event was sent. https://crbug.com/1409069
TEST_F(EventHandlerTest, GestureHandlingForHeldBackTouchPointer) {
  SetHtmlInnerHTML("<div style='width:50px;height:50px'></div>");

  int32_t pointer_id_1 = 123;
  int32_t pointer_id_2 = 125;  // Must be greater than `pointer_id_1`.

  WebPointerEvent pointer_down_2 = CreateMinimalTouchPointerEvent(
      WebInputEvent::Type::kPointerDown, gfx::PointF(10, 10));
  pointer_down_2.unique_touch_event_id = pointer_id_2;
  GetDocument().GetFrame()->GetEventHandler().HandlePointerEvent(
      pointer_down_2, Vector<WebPointerEvent>(), Vector<WebPointerEvent>());

  WebGestureEvent two_finger_tap = CreateMinimalGestureEvent(
      WebInputEvent::Type::kGestureTwoFingerTap, gfx::PointF(20, 20));
  two_finger_tap.primary_unique_touch_event_id = pointer_id_1;

  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      two_finger_tap);

  // This test passes if it doesn't crash.
}

TEST_F(EventHandlerTest, dragEndInNewDrag) {
  SetHtmlInnerHTML(
      "<style>.box { width: 100px; height: 100px; display: block; }</style>"
      "<a class='box' href=''>Drag me</a>");

  WebMouseEvent mouse_down_event(
      WebInputEvent::Type::kMouseDown, gfx::PointF(50, 50), gfx::PointF(50, 50),
      WebPointerProperties::Button::kLeft, 1,
      WebInputEvent::Modifiers::kLeftButtonDown, base::TimeTicks::Now());
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);

  WebMouseEvent mouse_move_event(
      WebInputEvent::Type::kMouseMove, gfx::PointF(51, 50), gfx::PointF(51, 50),
      WebPointerProperties::Button::kLeft, 1,
      WebInputEvent::Modifiers::kLeftButtonDown, base::TimeTicks::Now());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  // This reproduces what might be the conditions of http://crbug.com/677916
  //
  // TODO(crbug.com/682047): The call sequence below should not occur outside
  // this contrived test. Given the current code, it is unclear how the
  // dragSourceEndedAt() call could occur before a drag operation is started.

  WebMouseEvent mouse_up_event(
      WebInputEvent::Type::kMouseUp, gfx::PointF(100, 50),
      gfx::PointF(200, 250), WebPointerProperties::Button::kLeft, 1,
      WebInputEvent::kNoModifiers, base::TimeTicks::Now());
  GetDocument().GetFrame()->GetEventHandler().DragSourceEndedAt(
      mouse_up_event, ui::mojom::blink::DragOperation::kNone);

  // This test passes if it doesn't crash.
}

// This test mouse move with modifier kRelativeMotionEvent
// should not start drag.
TEST_F(EventHandlerTest, FakeMouseMoveNotStartDrag) {
  SetHtmlInnerHTML(
      "<style>"
      "body { margin: 0px; }"
      ".line { font-family: sans-serif; background: blue; width: 300px; "
      "height: 30px; font-size: 40px; margin-left: 250px; }"
      "</style>"
      "<div style='width: 300px; height: 100px;'>"
      "<span class='line' draggable='true'>abcd</span>"
      "</div>");
  WebMouseEvent mouse_down_event(WebMouseEvent::Type::kMouseDown,
                                 gfx::PointF(262, 29), gfx::PointF(329, 67),
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);

  WebMouseEvent fake_mouse_move(
      WebMouseEvent::Type::kMouseMove, gfx::PointF(618, 298),
      gfx::PointF(685, 436), WebPointerProperties::Button::kLeft, 1,
      WebInputEvent::Modifiers::kLeftButtonDown |
          WebInputEvent::Modifiers::kRelativeMotionEvent,
      WebInputEvent::GetStaticTimeStampForTests());
  EXPECT_EQ(
      WebInputEventResult::kHandledSuppressed,
      GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
          fake_mouse_move, Vector<WebMouseEvent>(), Vector<WebMouseEvent>()));

  EXPECT_EQ(gfx::Point(0, 0), GetDocument()
                                  .GetFrame()
                                  ->GetEventHandler()
                                  .DragDataTransferLocationForTesting());
}

class TooltipCapturingChromeClient : public EmptyChromeClient {
 public:
  TooltipCapturingChromeClient() = default;

  void UpdateTooltipUnderCursor(LocalFrame&,
                                const String& str,
                                TextDirection) override {
    last_tooltip_text_ = str;
    // Always reset the bounds to zero as this function doesn't set bounds.
    last_tooltip_bounds_ = gfx::Rect();
    triggered_from_cursor_ = true;
  }

  void UpdateTooltipFromKeyboard(LocalFrame&,
                                 const String& str,
                                 TextDirection,
                                 const gfx::Rect& bounds) override {
    last_tooltip_text_ = str;
    last_tooltip_bounds_ = bounds;
    triggered_from_cursor_ = false;
  }

  void ClearKeyboardTriggeredTooltip(LocalFrame&) override {
    if (triggered_from_cursor_)
      return;

    last_tooltip_text_ = String();
    last_tooltip_bounds_ = gfx::Rect();
  }

  void ResetTooltip() {
    last_tooltip_text_ = "";
    last_tooltip_bounds_ = gfx::Rect();
  }

  const String& LastToolTipText() { return last_tooltip_text_; }
  const gfx::Rect& LastToolTipBounds() { return last_tooltip_bounds_; }

 private:
  String last_tooltip_text_;
  gfx::Rect last_tooltip_bounds_;
  bool triggered_from_cursor_ = false;
};

class EventHandlerTooltipTest : public EventHandlerTest {
 public:
  EventHandlerTooltipTest() = default;

  void SetUp() override {
    chrome_client_ = MakeGarbageCollected<TooltipCapturingChromeClient>();
    SetupPageWithClients(chrome_client_);
  }

  const String& LastToolTipText() { return chrome_client_->LastToolTipText(); }
  const gfx::Rect& LastToolTipBounds() {
    return chrome_client_->LastToolTipBounds();
  }
  void ResetTooltip() { chrome_client_->ResetTooltip(); }

 private:
  Persistent<TooltipCapturingChromeClient> chrome_client_;
};

TEST_F(EventHandlerTooltipTest, mouseLeaveClearsTooltip) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  SetHtmlInnerHTML(
      "<style>.box { width: 100%; height: 100%; }</style>"
      "<img src='image.png' class='box' title='tooltip'>link</img>");

  EXPECT_EQ(WTF::String(), LastToolTipText());

  WebMouseEvent mouse_move_event(
      WebInputEvent::Type::kMouseMove, gfx::PointF(51, 50), gfx::PointF(51, 50),
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      base::TimeTicks::Now());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  EXPECT_EQ("tooltip", LastToolTipText());

  WebMouseEvent mouse_leave_event(
      WebInputEvent::Type::kMouseLeave, gfx::PointF(0, 0), gfx::PointF(0, 0),
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      base::TimeTicks::Now());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseLeaveEvent(
      mouse_leave_event);

  EXPECT_EQ(WTF::String(), LastToolTipText());
}

// macOS doesn't have keyboard-triggered tooltips.
#if BUILDFLAG(IS_MAC)
#define MAYBE_FocusSetFromTabUpdatesTooltip \
  DISABLED_FocusSetFromTabUpdatesTooltip
#else
#define MAYBE_FocusSetFromTabUpdatesTooltip FocusSetFromTabUpdatesTooltip
#endif
// Moving the focus with the tab key should trigger a tooltip update.
TEST_F(EventHandlerTooltipTest, MAYBE_FocusSetFromTabUpdatesTooltip) {
  SetHtmlInnerHTML(
      R"HTML(
        <button id='b1' title='my tooltip 1'>button 1</button>
        <button id='b2'>button 2</button>
      )HTML");

  EXPECT_EQ(WTF::String(), LastToolTipText());
  EXPECT_EQ(gfx::Rect(), LastToolTipBounds());

  WebKeyboardEvent e{WebInputEvent::Type::kRawKeyDown,
                     WebInputEvent::kNoModifiers,
                     WebInputEvent::GetStaticTimeStampForTests()};
  e.dom_code = static_cast<int>(ui::DomCode::TAB);
  e.dom_key = ui::DomKey::TAB;
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);

  Element* element = GetDocument().getElementById(AtomicString("b1"));
  EXPECT_EQ("my tooltip 1", LastToolTipText());
  EXPECT_EQ(element->BoundsInWidget(), LastToolTipBounds());

  // Doing the same but for a button that doesn't have a tooltip text should
  // still trigger a tooltip update. The browser-side TooltipController will
  // handle this case.
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  element = GetDocument().getElementById(AtomicString("b2"));
  EXPECT_TRUE(LastToolTipText().IsNull());
  EXPECT_EQ(element->BoundsInWidget(), LastToolTipBounds());
}

// macOS doesn't have keyboard-triggered tooltips.
#if BUILDFLAG(IS_MAC)
#define MAYBE_FocusSetFromAccessKeyUpdatesTooltip \
  DISABLED_FocusSetFromAccessKeyUpdatesTooltip
#else
#define MAYBE_FocusSetFromAccessKeyUpdatesTooltip \
  FocusSetFromAccessKeyUpdatesTooltip
#endif
// Moving the focus by pressing the access key on button should trigger a
// tooltip update.
TEST_F(EventHandlerTooltipTest, MAYBE_FocusSetFromAccessKeyUpdatesTooltip) {
  SetHtmlInnerHTML(
      R"HTML(
        <button id='b' title='my tooltip' accessKey='a'>button</button>
      )HTML");

  EXPECT_EQ(WTF::String(), LastToolTipText());
  EXPECT_EQ(gfx::Rect(), LastToolTipBounds());

  WebKeyboardEvent e{WebInputEvent::Type::kRawKeyDown, WebInputEvent::kAltKey,
                     WebInputEvent::GetStaticTimeStampForTests()};
  e.unmodified_text[0] = 'a';
  GetDocument().GetFrame()->GetEventHandler().HandleAccessKey(e);

  Element* element = GetDocument().getElementById(AtomicString("b"));
  EXPECT_EQ("my tooltip", LastToolTipText());
  EXPECT_EQ(element->BoundsInWidget(), LastToolTipBounds());
}

// macOS doesn't have keyboard-triggered tooltips.
#if BUILDFLAG(IS_MAC)
#define MAYBE_FocusSetFromMouseDoesntUpdateTooltip \
  DISABLED_FocusSetFromMouseDoesntUpdateTooltip
#else
#define MAYBE_FocusSetFromMouseDoesntUpdateTooltip \
  FocusSetFromMouseDoesntUpdateTooltip
#endif
// Moving the focus to an element with a mouse action shouldn't update the
// tooltip.
TEST_F(EventHandlerTooltipTest, MAYBE_FocusSetFromMouseDoesntUpdateTooltip) {
  SetHtmlInnerHTML(
      R"HTML(
        <button id='b' title='my tooltip'>button</button>
      )HTML");

  EXPECT_EQ(WTF::String(), LastToolTipText());
  EXPECT_EQ(gfx::Rect(), LastToolTipBounds());

  Element* element = GetDocument().getElementById(AtomicString("b"));
  gfx::PointF mouse_press_point =
      gfx::PointF(element->BoundsInWidget().CenterPoint());
  WebMouseEvent mouse_press_event(
      WebInputEvent::Type::kMouseDown, mouse_press_point, mouse_press_point,
      WebPointerProperties::Button::kLeft, 1,
      WebInputEvent::Modifiers::kLeftButtonDown, base::TimeTicks::Now());
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_press_event);

  EXPECT_TRUE(LastToolTipText().IsNull());
  EXPECT_EQ(gfx::Rect(), LastToolTipBounds());
}

// macOS doesn't have keyboard-triggered tooltips.
#if BUILDFLAG(IS_MAC)
#define MAYBE_FocusSetFromScriptDoesntUpdateTooltip \
  DISABLED_FocusSetFromScriptDoesntUpdateTooltip
#else
#define MAYBE_FocusSetFromScriptDoesntUpdateTooltip \
  FocusSetFromScriptDoesntUpdateTooltip
#endif
// Moving the focus to an element with a script action (FocusType::kNone means
// that the focus was set from a script) shouldn't update the tooltip.
TEST_F(EventHandlerTooltipTest, MAYBE_FocusSetFromScriptDoesntUpdateTooltip) {
  SetHtmlInnerHTML(
      R"HTML(
        <button id='b' title='my tooltip'>button</button>
      )HTML");

  EXPECT_EQ(WTF::String(), LastToolTipText());
  EXPECT_EQ(gfx::Rect(), LastToolTipBounds());

  Element* element = GetDocument().getElementById(AtomicString("b"));
  element->Focus();

  EXPECT_TRUE(LastToolTipText().IsNull());
  EXPECT_EQ(gfx::Rect(), LastToolTipBounds());
}

// macOS doesn't have keyboard-triggered tooltips.
#if BUILDFLAG(IS_MAC)
#define MAYBE_FocusSetScriptInitiatedFromKeypressUpdatesTooltip \
  DISABLED_FocusSetScriptInitiatedFromKeypressUpdatesTooltip
#else
#define MAYBE_FocusSetScriptInitiatedFromKeypressUpdatesTooltip \
  FocusSetScriptInitiatedFromKeypressUpdatesTooltip
#endif
// Moving the focus with a keypress that leads to a script being called
// should trigger a tooltip update.
TEST_F(EventHandlerTooltipTest,
       MAYBE_FocusSetScriptInitiatedFromKeypressUpdatesTooltip) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  SetHtmlInnerHTML(
      R"HTML(
        <button id='b1' title='my tooltip 1'>button 1</button>
        <button id='b2'>button 2</button>
      )HTML");
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
      R"HTML(
        document.addEventListener('keydown', (e) => {
          if (e.keyCode == 37) {
            document.getElementById('b1').focus();
          } else if (e.keyCode == 39) {
            document.getElementById('b2').focus();
          }
        });
      )HTML");
  GetDocument().body()->AppendChild(script);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  EXPECT_EQ(WTF::String(), LastToolTipText());
  EXPECT_EQ(gfx::Rect(), LastToolTipBounds());

  WebKeyboardEvent e{WebInputEvent::Type::kRawKeyDown,
                     WebInputEvent::kNoModifiers,
                     WebInputEvent::GetStaticTimeStampForTests()};
  e.dom_code = static_cast<int>(ui::DomCode::ARROW_LEFT);
  e.dom_key = ui::DomKey::ARROW_LEFT;
  e.native_key_code = e.windows_key_code = blink::VKEY_LEFT;
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);

  Element* element = GetDocument().getElementById(AtomicString("b1"));
  EXPECT_EQ("my tooltip 1", LastToolTipText());
  EXPECT_EQ(element->BoundsInWidget(), LastToolTipBounds());

  // Doing the same but for a button that doesn't have a tooltip text should
  // still trigger a tooltip update. The browser-side TooltipController will
  // handle this case.
  WebKeyboardEvent e2{WebInputEvent::Type::kRawKeyDown,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests()};
  e2.dom_code = static_cast<int>(ui::DomCode::ARROW_RIGHT);
  e2.dom_key = ui::DomKey::ARROW_RIGHT;
  e2.native_key_code = e2.windows_key_code = blink::VKEY_RIGHT;
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e2);

  element = GetDocument().getElementById(AtomicString("b2"));
  EXPECT_TRUE(LastToolTipText().IsNull());

  // But when the Element::Focus() is called outside of a keypress context,
  // no tooltip is shown.
  element = GetDocument().getElementById(AtomicString("b1"));
  element->Focus(FocusOptions::Create());
  EXPECT_TRUE(LastToolTipText().IsNull());
}

// macOS doesn't have keyboard-triggered tooltips.
#if BUILDFLAG(IS_MAC)
#define MAYBE_FocusSetFromScriptClearsKeyboardTriggeredTooltip \
  DISABLED_FocusSetFromScriptClearsKeyboardTriggeredTooltip
#else
#define MAYBE_FocusSetFromScriptClearsKeyboardTriggeredTooltip \
  FocusSetFromScriptClearsKeyboardTriggeredTooltip
#endif
// Moving the focus programmatically to an element that doesn't have a title
// attribute set while the user previously set the focus from keyboard on an
// element with a title text should hide the tooltip.
TEST_F(EventHandlerTooltipTest,
       MAYBE_FocusSetFromScriptClearsKeyboardTriggeredTooltip) {
  SetHtmlInnerHTML(
      R"HTML(
        <button id='b1' title='my tooltip 1'>button 1</button>
        <button id='b2'>button 2</button>
      )HTML");

  // First, show a keyboard-triggered tooltip using the 'tab' key.
  WebKeyboardEvent e{WebInputEvent::Type::kRawKeyDown,
                     WebInputEvent::kNoModifiers,
                     WebInputEvent::GetStaticTimeStampForTests()};
  e.dom_code = static_cast<int>(ui::DomCode::TAB);
  e.dom_key = ui::DomKey::TAB;
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);

  Element* element = GetDocument().getElementById(AtomicString("b1"));
  EXPECT_EQ("my tooltip 1", LastToolTipText());
  EXPECT_EQ(element->BoundsInWidget(), LastToolTipBounds());

  // Validate that blurring an element that is not focused will not just hide
  // the tooltip. It wouldn't make sense.
  element = GetDocument().getElementById(AtomicString("b2"));
  element->blur();

  EXPECT_EQ("my tooltip 1", LastToolTipText());
  EXPECT_EQ(GetDocument().getElementById(AtomicString("b1"))->BoundsInWidget(),
            LastToolTipBounds());

  // Then, programmatically move the focus to another button that has no title
  // text. This should hide the tooltip.
  element->Focus();

  EXPECT_TRUE(LastToolTipText().IsNull());
  EXPECT_EQ(gfx::Rect(), LastToolTipBounds());

  // Move the focus on the first button again and validate that it trigger a
  // tooltip again.
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);

  element = GetDocument().getElementById(AtomicString("b1"));
  EXPECT_EQ("my tooltip 1", LastToolTipText());
  EXPECT_EQ(element->BoundsInWidget(), LastToolTipBounds());

  // Then, programmatically blur the button to validate that the tooltip gets
  // hidden.
  element->blur();

  EXPECT_TRUE(LastToolTipText().IsNull());
  EXPECT_EQ(gfx::Rect(), LastToolTipBounds());
}

// Moving the focus programmatically while a cursor-triggered tooltip is visible
// shouldn't hide the visible tooltip.
TEST_F(EventHandlerTooltipTest,
       FocusSetFromScriptDoesntClearCursorTriggeredTooltip) {
  SetHtmlInnerHTML(
      R"HTML(
        <style>.box { width: 100px; height: 100px; }</style>
        <img src='image.png' class='box' title='tooltip'>link</img>

        <button id='b2'>button 2</button>
      )HTML");
  // First, show a cursor-triggered tooltip.
  WebMouseEvent mouse_move_event(
      WebInputEvent::Type::kMouseMove, gfx::PointF(51, 50), gfx::PointF(51, 50),
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      base::TimeTicks::Now());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  EXPECT_EQ("tooltip", LastToolTipText());

  // Then, programmatically move the focus to another element.
  Element* element = GetDocument().getElementById(AtomicString("b2"));
  element->Focus();

  EXPECT_EQ("tooltip", LastToolTipText());
}

class UnbufferedInputEventsTrackingChromeClient : public EmptyChromeClient {
 public:
  UnbufferedInputEventsTrackingChromeClient() = default;

  void RequestUnbufferedInputEvents(LocalFrame*) override {
    received_unbuffered_request_ = true;
  }

  bool ReceivedRequestForUnbufferedInput() {
    bool value = received_unbuffered_request_;
    received_unbuffered_request_ = false;
    return value;
  }

 private:
  bool received_unbuffered_request_ = false;
};

class EventHandlerLatencyTest : public PageTestBase {
 protected:
  void SetUp() override {
    chrome_client_ =
        MakeGarbageCollected<UnbufferedInputEventsTrackingChromeClient>();
    SetupPageWithClients(chrome_client_);
  }

  void SetHtmlInnerHTML(const char* html_content) {
    GetDocument().documentElement()->setInnerHTML(
        String::FromUTF8(html_content));
    UpdateAllLifecyclePhasesForTest();
  }

  Persistent<UnbufferedInputEventsTrackingChromeClient> chrome_client_;
};

TEST_F(EventHandlerLatencyTest, NeedsUnbufferedInput) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  SetHtmlInnerHTML(
      "<canvas style='width: 100px; height: 100px' id='first' "
      "onpointermove='return;'>");

  auto& canvas = To<HTMLCanvasElement>(
      *GetDocument().getElementById(AtomicString("first")));

  ASSERT_FALSE(chrome_client_->ReceivedRequestForUnbufferedInput());

  WebMouseEvent mouse_press_event(
      WebInputEvent::Type::kMouseDown, gfx::PointF(51, 50), gfx::PointF(51, 50),
      WebPointerProperties::Button::kLeft, 0, WebInputEvent::kNoModifiers,
      base::TimeTicks::Now());
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_press_event);
  ASSERT_FALSE(chrome_client_->ReceivedRequestForUnbufferedInput());

  canvas.SetNeedsUnbufferedInputEvents(true);

  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_press_event);
  ASSERT_TRUE(chrome_client_->ReceivedRequestForUnbufferedInput());

  canvas.SetNeedsUnbufferedInputEvents(false);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_press_event);
  ASSERT_FALSE(chrome_client_->ReceivedRequestForUnbufferedInput());
}

TEST_F(EventHandlerSimTest, MouseUpOffScrollbarGeneratesScrollEnd) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div style='height:1000px'>
    Tall text to create viewport scrollbar</div>
  )HTML");

  Compositor().BeginFrame();
  EXPECT_EQ(GetWebFrameWidget().GetInjectedScrollEvents().size(), 0u);

  // PageTestBase sizes the page to 800x600. Click on the scrollbar
  // track, move off, then release the mouse and verify that GestureScrollEnd
  // was queued up.

  // If the scrollbar theme does not allow hit testing, we should not get
  // any injected gesture events. Mobile overlay scrollbar theme does not
  // allow hit testing.
  bool scrollbar_theme_allows_hit_test =
      GetDocument().GetPage()->GetScrollbarTheme().AllowsHitTest();

  const gfx::PointF scrollbar_forward_track(795, 560);
  WebMouseEvent mouse_down(WebInputEvent::Type::kMouseDown,
                           scrollbar_forward_track, scrollbar_forward_track,
                           WebPointerProperties::Button::kLeft, 0,
                           WebInputEvent::kNoModifiers, base::TimeTicks::Now());
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(mouse_down);

  // Mouse down on the scrollbar track should have generated GSB/GSU.
  if (scrollbar_theme_allows_hit_test) {
    EXPECT_EQ(GetWebFrameWidget().GetInjectedScrollEvents().size(), 2u);
    EXPECT_EQ(
        GetWebFrameWidget().GetInjectedScrollEvents()[0]->Event().GetType(),
        WebInputEvent::Type::kGestureScrollBegin);
    EXPECT_EQ(
        GetWebFrameWidget().GetInjectedScrollEvents()[1]->Event().GetType(),
        WebInputEvent::Type::kGestureScrollUpdate);
  } else {
    EXPECT_EQ(GetWebFrameWidget().GetInjectedScrollEvents().size(), 0u);
  }

  const gfx::PointF middle_of_page(100, 100);
  WebMouseEvent mouse_move(WebInputEvent::Type::kMouseMove, middle_of_page,
                           middle_of_page, WebPointerProperties::Button::kLeft,
                           0, WebInputEvent::kNoModifiers,
                           base::TimeTicks::Now());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  // Mouse move should not have generated any gestures.
  if (scrollbar_theme_allows_hit_test) {
    EXPECT_EQ(GetWebFrameWidget().GetInjectedScrollEvents().size(), 2u);
  } else {
    EXPECT_EQ(GetWebFrameWidget().GetInjectedScrollEvents().size(), 0u);
  }

  WebMouseEvent mouse_up(WebInputEvent::Type::kMouseUp, middle_of_page,
                         middle_of_page, WebPointerProperties::Button::kLeft, 0,
                         WebInputEvent::kNoModifiers, base::TimeTicks::Now());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(mouse_up);

  // Mouse up must generate GestureScrollEnd.
  if (scrollbar_theme_allows_hit_test) {
    EXPECT_EQ(GetWebFrameWidget().GetInjectedScrollEvents().size(), 3u);
    EXPECT_EQ(
        GetWebFrameWidget().GetInjectedScrollEvents()[2]->Event().GetType(),
        WebInputEvent::Type::kGestureScrollEnd);
  } else {
    EXPECT_EQ(GetWebFrameWidget().GetInjectedScrollEvents().size(), 0u);
  }
}

TEST_F(EventHandlerSimTest, MouseUpOnlyOnScrollbar) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div style='height:1000px'>
    Tall text to create viewport scrollbar</div>
  )HTML");

  Compositor().BeginFrame();

  EXPECT_EQ(GetWebFrameWidget().GetInjectedScrollEvents().size(), 0u);

  // Mouse down on the page, the move the mouse to the scrollbar and release.
  // Validate that we don't inject a ScrollEnd (since no ScrollBegin was
  // injected).

  const gfx::PointF middle_of_page(100, 100);
  WebMouseEvent mouse_down(WebInputEvent::Type::kMouseDown, middle_of_page,
                           middle_of_page, WebPointerProperties::Button::kLeft,
                           0, WebInputEvent::kNoModifiers,
                           base::TimeTicks::Now());
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(mouse_down);

  // Mouse down on the page should not generate scroll gestures.
  EXPECT_EQ(GetWebFrameWidget().GetInjectedScrollEvents().size(), 0u);

  const gfx::PointF scrollbar_forward_track(795, 560);
  WebMouseEvent mouse_move(WebInputEvent::Type::kMouseMove,
                           scrollbar_forward_track, scrollbar_forward_track,
                           WebPointerProperties::Button::kLeft, 0,
                           WebInputEvent::kNoModifiers, base::TimeTicks::Now());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  // Mouse move should not have generated any gestures.
  EXPECT_EQ(GetWebFrameWidget().GetInjectedScrollEvents().size(), 0u);

  WebMouseEvent mouse_up(WebInputEvent::Type::kMouseUp, scrollbar_forward_track,
                         scrollbar_forward_track,
                         WebPointerProperties::Button::kLeft, 0,
                         WebInputEvent::kNoModifiers, base::TimeTicks::Now());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(mouse_up);

  // Mouse up should not have generated any gestures.
  EXPECT_EQ(GetWebFrameWidget().GetInjectedScrollEvents().size(), 0u);
}

TEST_F(EventHandlerSimTest, RightClickNoGestures) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div style='height:1000px'>
    Tall text to create viewport scrollbar</div>
  )HTML");

  Compositor().BeginFrame();

  EXPECT_EQ(GetWebFrameWidget().GetInjectedScrollEvents().size(), 0u);

  // PageTestBase sizes the page to 800x600. Right click on the scrollbar
  // track, and release the mouse and verify that no gesture events are
  // queued up (right click doesn't scroll scrollbars).

  const gfx::PointF scrollbar_forward_track(795, 560);
  WebMouseEvent mouse_down(WebInputEvent::Type::kMouseDown,
                           scrollbar_forward_track, scrollbar_forward_track,
                           WebPointerProperties::Button::kRight, 0,
                           WebInputEvent::kNoModifiers, base::TimeTicks::Now());
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(mouse_down);

  EXPECT_EQ(GetWebFrameWidget().GetInjectedScrollEvents().size(), 0u);

  WebMouseEvent mouse_up(WebInputEvent::Type::kMouseUp, scrollbar_forward_track,
                         scrollbar_forward_track,
                         WebPointerProperties::Button::kRight, 0,
                         WebInputEvent::kNoModifiers, base::TimeTicks::Now());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(mouse_up);

  EXPECT_EQ(GetWebFrameWidget().GetInjectedScrollEvents().size(), 0u);
}

// https://crbug.com/976557 tracks the fix for re-enabling this test on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_GestureTapWithScrollSnaps DISABLED_GestureTapWithScrollSnaps
#else
#define MAYBE_GestureTapWithScrollSnaps GestureTapWithScrollSnaps
#endif

TEST_F(EventHandlerSimTest, MAYBE_GestureTapWithScrollSnaps) {
  // Create a page that has scroll snaps enabled for a scroller. Tap on the
  // scrollbar and verify that the SnapController does not immediately cancel
  // the resulting animation during the handling of GestureScrollEnd - this
  // should be deferred until the animation completes or is cancelled.

  // Enable scroll animations - this test relies on animations being
  // queued up in response to GestureScrollUpdate events.
  GetDocument().GetSettings()->SetScrollAnimatorEnabled(true);

  // Enable accelerated compositing in order to ensure the Page's
  // ScrollingCoordinator is initialized.
  GetDocument().GetSettings()->SetAcceleratedCompositingEnabled(true);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body { margin:0 }
      #container {
        overflow: scroll;
        width:500px;
        height:500px;
        scroll-snap-type: y mandatory;
      }
      div {
        height:400px;
        scroll-snap-align: start
      }
    </style>
    <body>
    <div id='container'>
    <div></div><div></div><div></div>
    </div>
  )HTML");

  Compositor().BeginFrame();

  EXPECT_EQ(GetWebFrameWidget().GetInjectedScrollEvents().size(), 0u);

  // Only run this test if scrollbars are hit-testable (they are not on
  // Android).
  bool scrollbar_theme_allows_hit_test =
      GetDocument().GetPage()->GetScrollbarTheme().AllowsHitTest();
  if (!scrollbar_theme_allows_hit_test)
    return;

  // kGestureTapDown sets the pressed parts which is a pre-requisite for
  // kGestureTap performing a scroll.
  const gfx::PointF scrollbar_forward_track(495, 450);
  TapDownEventBuilder tap_down(scrollbar_forward_track);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(tap_down);

  TapEventBuilder tap(scrollbar_forward_track, 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(tap);
  EXPECT_EQ(GetWebFrameWidget().GetInjectedScrollEvents().size(), 3u);

  const Vector<std::unique_ptr<blink::WebCoalescedInputEvent>>& data =
      GetWebFrameWidget().GetInjectedScrollEvents();
  EXPECT_EQ(data[0]->Event().GetType(),
            WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_EQ(data[1]->Event().GetType(),
            WebInputEvent::Type::kGestureScrollUpdate);
  EXPECT_EQ(data[2]->Event().GetType(), WebInputEvent::Type::kGestureScrollEnd);
  const WebGestureEvent& gsb =
      static_cast<const WebGestureEvent&>(data[0]->Event());
  const WebGestureEvent& gsu =
      static_cast<const WebGestureEvent&>(data[1]->Event());
  const WebGestureEvent& gse =
      static_cast<const WebGestureEvent&>(data[2]->Event());

  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(gsb);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(gsu);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(gse);

  // Ensure that there is an active animation on the scrollable area event
  // though GSE was handled. The actual handling should be deferred.
  Element* scrollable_div =
      GetDocument().getElementById(AtomicString("container"));
  ScrollableArea* scrollable_area =
      scrollable_div->GetLayoutBox()->GetScrollableArea();
  EXPECT_TRUE(scrollable_area->ExistingScrollAnimator());
  EXPECT_TRUE(scrollable_area->ExistingScrollAnimator()->HasRunningAnimation());

  // Run the animation for a few frames to ensure that snapping did not
  // immediately happen.
  // One frame to update run_state_, one to set start_time = now, then advance
  // two frames into the animation.
  const int kFramesToRun = 4;
  for (int i = 0; i < kFramesToRun; i++)
    Compositor().BeginFrame();

  EXPECT_NE(scrollable_area->GetScrollOffset().y(), 0);

  // Finish the animation, verify that we're back at 0 and not animating.
  Compositor().BeginFrame(0.3);

  EXPECT_EQ(scrollable_area->GetScrollOffset().y(), 0);
  EXPECT_FALSE(
      scrollable_area->ExistingScrollAnimator()->HasRunningAnimation());
}

// Test that leaving a window leaves mouse position unknown.
TEST_F(EventHandlerTest, MouseLeaveResetsUnknownState) {
  SetHtmlInnerHTML("<div></div>");
  WebMouseEvent mouse_down_event(WebMouseEvent::Type::kMouseDown,
                                 gfx::PointF(262, 29), gfx::PointF(329, 67),
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);
  EXPECT_FALSE(
      GetDocument().GetFrame()->GetEventHandler().IsMousePositionUnknown());

  WebMouseEvent mouse_leave_event(WebMouseEvent::Type::kMouseLeave,
                                  gfx::PointF(262, 29), gfx::PointF(329, 67),
                                  WebPointerProperties::Button::kNoButton, 1,
                                  WebInputEvent::Modifiers::kNoModifiers,
                                  WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseLeaveEvent(
      mouse_leave_event);
  EXPECT_TRUE(
      GetDocument().GetFrame()->GetEventHandler().IsMousePositionUnknown());
}

// Test that leaving an iframe sets the mouse position to unknown on that
// iframe.
TEST_F(EventHandlerSimTest, MouseLeaveIFrameResets) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));

  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://example.com/frame.html", "text/html");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    div {
      width: 200px;
      height: 200px;
    }
    iframe {
      width: 200px;
      height: 200px;
    }
    </style>
    <div></div>
    <iframe id='frame' src='frame.html'></iframe>
  )HTML");

  frame_resource.Complete("<!DOCTYPE html>");
  Compositor().BeginFrame();
  WebMouseEvent mouse_move_inside_event(
      WebMouseEvent::Type::kMouseMove, gfx::PointF(100, 229),
      gfx::PointF(100, 229), WebPointerProperties::Button::kNoButton, 0,
      WebInputEvent::Modifiers::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_inside_event, Vector<WebMouseEvent>(),
      Vector<WebMouseEvent>());
  EXPECT_FALSE(
      GetDocument().GetFrame()->GetEventHandler().IsMousePositionUnknown());
  auto* child_frame = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  child_frame->contentDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_FALSE(To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild())
                   ->GetEventHandler()
                   .IsMousePositionUnknown());

  WebMouseEvent mouse_move_outside_event(
      WebMouseEvent::Type::kMouseMove, gfx::PointF(300, 29),
      gfx::PointF(300, 29), WebPointerProperties::Button::kNoButton, 0,
      WebInputEvent::Modifiers::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_outside_event, Vector<WebMouseEvent>(),
      Vector<WebMouseEvent>());
  EXPECT_FALSE(
      GetDocument().GetFrame()->GetEventHandler().IsMousePositionUnknown());
  EXPECT_TRUE(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_TRUE(To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild())
                  ->GetEventHandler()
                  .IsMousePositionUnknown());
}

// Test that mouse down and move a small distance on a draggable element will
// not change cursor style.
TEST_F(EventHandlerSimTest, CursorStyleBeforeStartDragging) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    div {
      width: 300px;
      height: 100px;
      cursor: help;
    }
    </style>
    <div draggable='true'>foo</div>
  )HTML");
  Compositor().BeginFrame();

  WebMouseEvent mouse_down_event(WebMouseEvent::Type::kMouseDown,
                                 gfx::PointF(150, 50), gfx::PointF(150, 50),
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);

  WebMouseEvent mouse_move_event(WebMouseEvent::Type::kMouseMove,
                                 gfx::PointF(151, 50), gfx::PointF(151, 50),
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  EXPECT_EQ(ui::mojom::blink::CursorType::kHelp, GetDocument()
                                                     .GetFrame()
                                                     ->GetChromeClient()
                                                     .LastSetCursorForTesting()
                                                     .type());
}

// Ensure that tap on element in iframe should apply active state.
TEST_F(EventHandlerSimTest, TapActiveInFrame) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));

  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    iframe {
      width: 200px;
      height: 200px;
    }
    </style>
    <iframe id='iframe' src='iframe.html'>
    </iframe>
  )HTML");

  frame_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    div {
      width: 100px;
      height: 100px;
    }
    </style>
    <div></div>
  )HTML");
  Compositor().BeginFrame();

  auto* iframe_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("iframe")));
  Document* iframe_doc = iframe_element->contentDocument();

  TapDownEventBuilder tap_down(gfx::PointF(10, 10));
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(tap_down);

  ShowPressEventBuilder show_press(gfx::PointF(10, 10));
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(show_press);

  // TapDown and ShowPress active the iframe.
  EXPECT_TRUE(GetDocument().GetActiveElement());
  EXPECT_TRUE(iframe_doc->GetActiveElement());

  TapEventBuilder tap(gfx::PointF(10, 10), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(tap);

  // Should still active.
  EXPECT_TRUE(GetDocument().GetActiveElement());
  EXPECT_TRUE(iframe_doc->GetActiveElement());

  // The active will cancel after 15ms.
  test::RunDelayedTasks(base::Seconds(0.2));
  EXPECT_FALSE(GetDocument().GetActiveElement());
  EXPECT_FALSE(iframe_doc->GetActiveElement());
}

// Test that the hover is updated at the next begin frame after the compositor
// scroll ends.
TEST_F(EventHandlerSimTest, TestUpdateHoverAfterCompositorScrollAtBeginFrame) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body, html {
        margin: 0;
      }
      div {
        height: 300px;
        width: 100%;
      }
    </style>
    <body>
    <div class="hoverme" id="line1">hover over me</div>
    <div class="hoverme" id="line2">hover over me</div>
    <div class="hoverme" id="line3">hover over me</div>
    <div class="hoverme" id="line4">hover over me</div>
    <div class="hoverme" id="line5">hover over me</div>
    </body>
    <script>
      let array = document.getElementsByClassName('hoverme');
      for (let element of array) {
        element.addEventListener('mouseover', function (e) {
          this.innerHTML = "currently hovered";
        });
        element.addEventListener('mouseout', function (e) {
          this.innerHTML = "was hovered";
        });
      }
    </script>
  )HTML");
  Compositor().BeginFrame();

  // Set mouse position and active web view.
  InitializeMousePositionAndActivateView(1, 1);

  WebElement element1 = GetDocument().getElementById(AtomicString("line1"));
  WebElement element2 = GetDocument().getElementById(AtomicString("line2"));
  WebElement element3 = GetDocument().getElementById(AtomicString("line3"));
  EXPECT_EQ("currently hovered", element1.InnerHTML().Utf8());
  EXPECT_EQ("hover over me", element2.InnerHTML().Utf8());
  EXPECT_EQ("hover over me", element3.InnerHTML().Utf8());

  // Do a compositor scroll and set |hover_needs_update_at_scroll_end| to be
  // true in WebViewImpl.
  LocalFrameView* frame_view = GetDocument().View();
  frame_view->LayoutViewport()->DidCompositorScroll(gfx::PointF(0, 500));
  WebView().MainFrameWidget()->ApplyViewportChangesForTesting(
      {gfx::Vector2dF(), gfx::Vector2dF(), 1.0f, false, 0, 0,
       cc::BrowserControlsState::kBoth, true});
  ASSERT_EQ(500, frame_view->LayoutViewport()->GetScrollOffset().y());
  EXPECT_EQ("currently hovered", element1.InnerHTML().Utf8());
  EXPECT_EQ("hover over me", element2.InnerHTML().Utf8());
  EXPECT_EQ("hover over me", element3.InnerHTML().Utf8());

  // The fake mouse move event is dispatched at the begin frame to update hover.
  Compositor().BeginFrame();
  EXPECT_EQ("was hovered", element1.InnerHTML().Utf8());
  EXPECT_EQ("currently hovered", element2.InnerHTML().Utf8());
  EXPECT_EQ("hover over me", element3.InnerHTML().Utf8());
}

// Test that the hover is updated at the next begin frame after the smooth JS
// scroll ends.
TEST_F(EventHandlerSimTest, TestUpdateHoverAfterJSScrollAtBeginFrame) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 500));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body, html {
        margin: 0;
        height: 500vh;
      }
      div {
        height: 500px;
        width: 100%;
      }
    </style>
    <body>
    <div class="hoverme" id="hoverarea">hover over me</div>
    </body>
  )HTML");
  Compositor().BeginFrame();

  // Set mouse position and active web view.
  InitializeMousePositionAndActivateView(100, 100);

  Element* element = GetDocument().getElementById(AtomicString("hoverarea"));
  EXPECT_TRUE(element->IsHovered());

  // Find the scrollable area and set scroll offset.
  ScrollableArea* scrollable_area =
      GetDocument().GetLayoutView()->GetScrollableArea();
  bool finished = false;
  scrollable_area->SetScrollOffset(
      ScrollOffset(0, 1000), mojom::blink::ScrollType::kProgrammatic,
      mojom::blink::ScrollBehavior::kSmooth,
      ScrollableArea::ScrollCallback(WTF::BindOnce(
          [](bool* finished, ScrollableArea::ScrollCompletionMode) {
            *finished = true;
          },
          WTF::Unretained(&finished))));
  Compositor().BeginFrame();
  LocalFrameView* frame_view = GetDocument().View();
  ASSERT_EQ(0, frame_view->LayoutViewport()->GetScrollOffset().y());
  ASSERT_FALSE(finished);
  // Scrolling is in progress but the hover is not updated yet.
  Compositor().BeginFrame();
  // Start scroll animation, but it is not finished.
  Compositor().BeginFrame();
  ASSERT_GT(frame_view->LayoutViewport()->GetScrollOffset().y(), 0);
  ASSERT_FALSE(finished);

  // Mark hover state dirty but the hover state does not change after the
  // animation finishes.
  Compositor().BeginFrame(1);
  ASSERT_EQ(1000, frame_view->LayoutViewport()->GetScrollOffset().y());
  ASSERT_TRUE(finished);
  EXPECT_TRUE(element->IsHovered());

  // Hover state is updated after the begin frame.
  Compositor().BeginFrame();
  EXPECT_FALSE(element->IsHovered());
}

TEST_F(EventHandlerSimTest, LargeCustomCursorIntersectsViewport) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  SimSubresourceRequest cursor_request("https://example.com/100x100.png",
                                       "image/png");
  LoadURL("https://example.com/test.html");
  request.Complete(
      R"HTML(
        <!DOCTYPE html>
        <style>
        div {
          width: 100vw;
          height: 100vh;
          cursor: url('100x100.png') 50 50, auto;
        }
        </style>
        <div>foo</div>
      )HTML");
  GetDocument().UpdateStyleAndLayoutTree();

  cursor_request.Complete(
      *test::ReadFromFile(test::CoreTestDataPath("notifications/100x100.png")));

  Compositor().BeginFrame();

  EventHandler& event_handler = GetDocument().GetFrame()->GetEventHandler();

  struct TestCase {
    gfx::PointF point;
    bool custom_expected;
    float cursor_accessibility_scale_factor = 1.f;
    float device_scale_factor = 1.f;
    std::string ToString() const {
      return base::StringPrintf(
          "point: (%s), cursor-scale: %g, device-scale: %g, custom?: %d",
          point.ToString().c_str(), cursor_accessibility_scale_factor,
          device_scale_factor, custom_expected);
    }
  } test_cases[] = {
      // Test top left and bottom right, within viewport.
      {gfx::PointF(60, 60), true},
      {gfx::PointF(740, 540), true},
      // Test top left and bottom right, beyond viewport.
      {gfx::PointF(40, 40), false},
      {gfx::PointF(760, 560), false},
      // Test a larger cursor accessibility scale factor. crbug.com/1455005
      {gfx::PointF(110, 110), true, 2.f},
      {gfx::PointF(690, 490), true, 2.f},
      {gfx::PointF(90, 90), false, 2.f},
      {gfx::PointF(710, 510), false, 2.f},
      // Test a larger display device scale factor. crbug.com/1357442
      {gfx::PointF(110, 110), true, 1.f, 2.f},
      {gfx::PointF(690, 490), true, 1.f, 2.f},
      {gfx::PointF(90, 90), false, 1.f, 2.f},
      {gfx::PointF(710, 510), false, 1.f, 2.f},
  };
  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(test_case.ToString());
    DeviceEmulationParams params;
    params.device_scale_factor = test_case.device_scale_factor;
    WebView().EnableDeviceEmulation(params);
    event_handler.set_cursor_accessibility_scale_factor(
        test_case.cursor_accessibility_scale_factor);
    WebMouseEvent mouse_move_event(
        WebMouseEvent::Type::kMouseMove, test_case.point, test_case.point,
        WebPointerProperties::Button::kNoButton, 0, 0,
        WebInputEvent::GetStaticTimeStampForTests());
    event_handler.HandleMouseMoveEvent(mouse_move_event, {}, {});
    const ui::Cursor& cursor =
        GetDocument().GetFrame()->GetChromeClient().LastSetCursorForTesting();
    const ui::mojom::blink::CursorType expected_type =
        test_case.custom_expected ? ui::mojom::blink::CursorType::kCustom
                                  : ui::mojom::blink::CursorType::kPointer;
    EXPECT_EQ(expected_type, cursor.type());
  }
}

TEST_F(EventHandlerSimTest, SmallCustomCursorIntersectsViewport) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  SimSubresourceRequest cursor_request("https://example.com/48x48.png",
                                       "image/png");
  LoadURL("https://example.com/test.html");
  request.Complete(
      R"HTML(
        <!DOCTYPE html>
        <style>
        div {
          width: 300px;
          height: 100px;
          cursor: -webkit-image-set(url('48x48.png') 2x) 24 24, auto;
        }
        </style>
        <div>foo</div>
      )HTML");

  GetDocument().UpdateStyleAndLayoutTree();

  cursor_request.Complete(
      *test::ReadFromFile(test::CoreTestDataPath("notifications/48x48.png")));

  Compositor().BeginFrame();

  // Move the cursor so no part of it intersects the viewport.
  {
    WebMouseEvent mouse_move_event(
        WebMouseEvent::Type::kMouseMove, gfx::PointF(25, 25),
        gfx::PointF(25, 25), WebPointerProperties::Button::kNoButton, 0, 0,
        WebInputEvent::GetStaticTimeStampForTests());
    GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
        mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

    const ui::Cursor& cursor =
        GetDocument().GetFrame()->GetChromeClient().LastSetCursorForTesting();
    EXPECT_EQ(ui::mojom::blink::CursorType::kCustom, cursor.type());
  }

  // Now, move the cursor so that it intersects the visual viewport. The cursor
  // should not be removed because it is below
  // kMaximumCursorSizeWithoutFallback.
  {
    WebMouseEvent mouse_move_event(
        WebMouseEvent::Type::kMouseMove, gfx::PointF(23, 23),
        gfx::PointF(23, 23), WebPointerProperties::Button::kNoButton, 0, 0,
        WebInputEvent::GetStaticTimeStampForTests());
    GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
        mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

    const ui::Cursor& cursor =
        GetDocument().GetFrame()->GetChromeClient().LastSetCursorForTesting();
    EXPECT_EQ(ui::mojom::blink::CursorType::kCustom, cursor.type());
  }
}

TEST_F(EventHandlerSimTest, NeverExposeKeyboardEvent) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  GetDocument().GetSettings()->SetDontSendKeyEventsToJavascript(true);
  GetDocument().GetSettings()->SetScrollAnimatorEnabled(false);
  GetDocument().GetSettings()->SetWebAppScope(GetDocument().Url());
  WebView().MainFrameImpl()->LocalRootFrameWidget()->SetDisplayMode(
      blink::mojom::DisplayMode::kFullscreen);
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      height: 10000px;
    }
    </style>
    Last event: <br>
    <p id='log'>no event</p>
    <input id="input1" type="text">

    <script>
      document.addEventListener('keydown', (e) => {
        let log = document.getElementById('log');
        log.innerText = 'keydown cancelable=' + e.cancelable;
      });
      document.addEventListener('keyup', (e) => {
        let log = document.getElementById('log');
        log.innerText = 'keyup cancelable=' + e.cancelable;
      });
    </script>
  )HTML");
  Compositor().BeginFrame();

  WebElement element = GetDocument().getElementById(AtomicString("log"));
  WebKeyboardEvent e{WebInputEvent::Type::kRawKeyDown,
                     WebInputEvent::kNoModifiers,
                     WebInputEvent::GetStaticTimeStampForTests()};
  e.windows_key_code = VKEY_DOWN;
  // TODO(crbug.com/949766) Should cleanup these magic number.
  e.dom_key = 0x00200309;
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  EXPECT_EQ("no event", element.InnerHTML().Utf8());

  e.SetType(WebInputEvent::Type::kKeyUp);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  EXPECT_EQ("no event", element.InnerHTML().Utf8());

  e.SetType(WebInputEvent::Type::kKeyDown);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  EXPECT_EQ("no event", element.InnerHTML().Utf8());

  e.SetType(WebInputEvent::Type::kKeyUp);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  EXPECT_EQ("no event", element.InnerHTML().Utf8());

  // TODO(crbug.com/949766) Should cleanup these magic number.
  e.dom_key = 0x00200310;
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  EXPECT_NE("no event", element.InnerHTML().Utf8());

  e.SetType(WebInputEvent::Type::kKeyUp);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  EXPECT_NE("no event", element.InnerHTML().Utf8());

  e.SetType(WebInputEvent::Type::kKeyDown);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  EXPECT_NE("no event", element.InnerHTML().Utf8());

  e.SetType(WebInputEvent::Type::kKeyUp);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  EXPECT_NE("no event", element.InnerHTML().Utf8());
}

TEST_F(EventHandlerSimTest, NotExposeKeyboardEvent) {
  GetDocument().GetSettings()->SetDontSendKeyEventsToJavascript(true);
  GetDocument().GetSettings()->SetScrollAnimatorEnabled(false);
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      height: 10000px;
    }
    </style>
    Last event: <br>
    <p id='log'>no event</p>
    <input id="input1" type="text">

    <script>
      document.addEventListener('keydown', (e) => {
        let log = document.getElementById('log');
        log.innerText = 'keydown cancelable=' + e.cancelable;
      });
      document.addEventListener('keyup', (e) => {
        let log = document.getElementById('log');
        log.innerText = 'keyup cancelable=' + e.cancelable;
      });
    </script>
  )HTML");
  Compositor().BeginFrame();

  WebElement element = GetDocument().getElementById(AtomicString("log"));
  WebKeyboardEvent e{WebInputEvent::Type::kRawKeyDown,
                     WebInputEvent::kNoModifiers,
                     WebInputEvent::GetStaticTimeStampForTests()};
  e.windows_key_code = VKEY_DOWN;
  // TODO(crbug.com/949766) Should cleanup these magic number.
  e.dom_key = 0x00200309;
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  EXPECT_EQ("no event", element.InnerHTML().Utf8());

  e.SetType(WebInputEvent::Type::kKeyUp);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  EXPECT_EQ("no event", element.InnerHTML().Utf8());

  e.SetType(WebInputEvent::Type::kKeyDown);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  EXPECT_EQ("no event", element.InnerHTML().Utf8());

  e.SetType(WebInputEvent::Type::kKeyUp);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  EXPECT_EQ("no event", element.InnerHTML().Utf8());

  // Key send to js but not cancellable.
  e.dom_key = 0x00400031;
  e.SetType(WebInputEvent::Type::kRawKeyDown);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  EXPECT_EQ("keydown cancelable=false", element.InnerHTML().Utf8());

  e.SetType(WebInputEvent::Type::kKeyUp);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  EXPECT_EQ("keyup cancelable=false", element.InnerHTML().Utf8());

  // Key send to js and cancellable in editor.
  WebElement input = GetDocument().getElementById(AtomicString("input1"));
  GetDocument().SetFocusedElement(
      input.Unwrap<Element>(),
      FocusParams(SelectionBehaviorOnFocus::kNone,
                  mojom::blink::FocusType::kNone, nullptr));

  e.SetType(WebInputEvent::Type::kRawKeyDown);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  EXPECT_EQ("keydown cancelable=true", element.InnerHTML().Utf8());

  // Arrow key caused scroll down in post event dispatch process. Ensure page
  // scrolled.
  ScrollableArea* scrollable_area = GetDocument().View()->LayoutViewport();
  EXPECT_GT(scrollable_area->ScrollOffsetInt().y(), 0);
}

TEST_F(EventHandlerSimTest, DoNotScrollWithTouchpadIfOverflowIsHidden) {
  ResizeView(gfx::Size(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    #outer {
        width: 100vw;
        height: 100vh;
        overflow-x: hidden;
        overflow-y: scroll;
    }
    #inner {
        width: 300vw;
        height: 300vh;
    }
    </style>
    <body>
      <div id='outer'>
        <div id='inner'>
      </div>
    </body>
  )HTML");
  Compositor().BeginFrame();

  WebGestureEvent scroll_begin_event(
      WebInputEvent::Type::kGestureScrollBegin, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchpad);
  scroll_begin_event.SetPositionInWidget(gfx::PointF(10, 10));
  scroll_begin_event.SetPositionInScreen(gfx::PointF(10, 10));

  WebGestureEvent scroll_update_event(
      WebInputEvent::Type::kGestureScrollUpdate, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchpad);
  scroll_update_event.data.scroll_update.delta_x = -100;
  scroll_update_event.data.scroll_update.delta_y = -100;
  scroll_update_event.SetPositionInWidget(gfx::PointF(10, 10));
  scroll_update_event.SetPositionInScreen(gfx::PointF(10, 10));

  WebGestureEvent scroll_end_event(WebInputEvent::Type::kGestureScrollEnd,
                                   WebInputEvent::kNoModifiers,
                                   WebInputEvent::GetStaticTimeStampForTests(),
                                   blink::WebGestureDevice::kTouchpad);
  scroll_end_event.SetPositionInWidget(gfx::PointF(10, 10));
  scroll_end_event.SetPositionInScreen(gfx::PointF(10, 10));

  GetWebFrameWidget().DispatchThroughCcInputHandler(scroll_begin_event);
  GetWebFrameWidget().DispatchThroughCcInputHandler(scroll_update_event);
  GetWebFrameWidget().DispatchThroughCcInputHandler(scroll_end_event);

  Compositor().BeginFrame();
  EXPECT_EQ(0,
            GetDocument().getElementById(AtomicString("outer"))->scrollLeft());
}

TEST_F(EventHandlerSimTest, ElementTargetedGestureScroll) {
  ResizeView(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #scroller {
        overflow-y:scroll;
        height:200px;
      }
      #talldiv {
        height:1000px;
      }
    </style>
    <div id="talldiv">Tall text to create viewport scrollbar</div>
    <div id="scroller">
      <div style="height:2000px">To create subscroller scrollbar</div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Element* const scroller =
      GetDocument().getElementById(AtomicString("scroller"));
  constexpr float delta_y = 100;
  // Send GSB/GSU at 0,0 to target the viewport first, then verify that
  // the viewport scrolled accordingly.
  WebGestureEvent gesture_scroll_begin{
      WebInputEvent::Type::kGestureScrollBegin, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(),
      WebGestureDevice::kTouchscreen};
  gesture_scroll_begin.data.scroll_begin.delta_x_hint = 0;
  gesture_scroll_begin.data.scroll_begin.delta_y_hint = -delta_y;
  DispatchElementTargetedGestureScroll(gesture_scroll_begin);

  WebGestureEvent gesture_scroll_update{
      WebInputEvent::Type::kGestureScrollUpdate, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(),
      WebGestureDevice::kTouchscreen};
  gesture_scroll_update.data.scroll_update.delta_x = 0;
  gesture_scroll_update.data.scroll_update.delta_y = -delta_y;

  DispatchElementTargetedGestureScroll(gesture_scroll_update);

  WebGestureEvent gesture_scroll_end{
      WebInputEvent::Type::kGestureScrollEnd, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(),
      WebGestureDevice::kTouchscreen};
  DispatchElementTargetedGestureScroll(gesture_scroll_end);

  Compositor().BeginFrame();
  LocalFrameView* frame_view = GetDocument().View();
  ASSERT_EQ(frame_view->LayoutViewport()->GetScrollOffset().y(), delta_y);

  // Switch to the element_id-based targeting for GSB, then resend GSU
  // and validate that the subscroller scrolled (and that the viewport
  // did not).
  ScrollableArea* scrollable_area =
      scroller->GetLayoutBox()->GetScrollableArea();
  gesture_scroll_begin.data.scroll_begin.scrollable_area_element_id =
      scrollable_area->GetScrollElementId().GetInternalValue();

  DispatchElementTargetedGestureScroll(gesture_scroll_begin);
  DispatchElementTargetedGestureScroll(gesture_scroll_update);
  DispatchElementTargetedGestureScroll(gesture_scroll_end);

  Compositor().BeginFrame();
  ASSERT_EQ(scrollable_area->ScrollOffsetInt().y(), delta_y);
  ASSERT_EQ(frame_view->LayoutViewport()->GetScrollOffset().y(), delta_y);

  // Remove the scroller, update layout, and ensure the same gestures
  // don't crash or scroll the layout viewport.
  scroller->remove();
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  DispatchElementTargetedGestureScroll(gesture_scroll_begin);
  DispatchElementTargetedGestureScroll(gesture_scroll_update);
  DispatchElementTargetedGestureScroll(gesture_scroll_end);

  Compositor().BeginFrame();
  ASSERT_EQ(frame_view->LayoutViewport()->GetScrollOffset().y(), delta_y);
}

TEST_F(EventHandlerSimTest, ElementTargetedGestureScrollIFrame) {
  ResizeView(gfx::Size(800, 600));
  SimRequest request_outer("https://example.com/test-outer.html", "text/html");
  SimRequest request_inner("https://example.com/test-inner.html", "text/html");
  LoadURL("https://example.com/test-outer.html");
  request_outer.Complete(R"HTML(
    <!DOCTYPE html>
    <iframe id="iframe" src="test-inner.html"></iframe>
    <div style="height:1000px"></div>
    )HTML");

  request_inner.Complete(R"HTML(
    <!DOCTYPE html>
    <div style="height:1000px"></div>
  )HTML");
  Compositor().BeginFrame();

  auto* const iframe = To<HTMLFrameElementBase>(
      GetDocument().getElementById(AtomicString("iframe")));
  FrameView* child_frame_view =
      iframe->GetLayoutEmbeddedContent()->ChildFrameView();
  auto* local_child_frame_view = DynamicTo<LocalFrameView>(child_frame_view);
  ScrollableArea* scrollable_area = local_child_frame_view->GetScrollableArea();

  // Target the iframe scrollable area and make sure it scrolls when targeted
  // with gestures.
  constexpr float delta_y = 100;
  WebGestureEvent gesture_scroll_begin{
      WebInputEvent::Type::kGestureScrollBegin, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(),
      WebGestureDevice::kTouchscreen};
  gesture_scroll_begin.data.scroll_begin.delta_x_hint = 0;
  gesture_scroll_begin.data.scroll_begin.delta_y_hint = -delta_y;
  gesture_scroll_begin.data.scroll_begin.scrollable_area_element_id =
      scrollable_area->GetScrollElementId().GetInternalValue();
  DispatchElementTargetedGestureScroll(gesture_scroll_begin);

  WebGestureEvent gesture_scroll_update{
      WebInputEvent::Type::kGestureScrollUpdate, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(),
      WebGestureDevice::kTouchscreen};
  gesture_scroll_update.data.scroll_update.delta_x = 0;
  gesture_scroll_update.data.scroll_update.delta_y = -delta_y;

  DispatchElementTargetedGestureScroll(gesture_scroll_update);

  WebGestureEvent gesture_scroll_end{
      WebInputEvent::Type::kGestureScrollEnd, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(),
      WebGestureDevice::kTouchscreen};
  DispatchElementTargetedGestureScroll(gesture_scroll_end);

  Compositor().BeginFrame();
  LocalFrameView* frame_view = GetDocument().View();
  ASSERT_EQ(frame_view->LayoutViewport()->GetScrollOffset().y(), 0);
  ASSERT_EQ(scrollable_area->ScrollOffsetInt().y(), delta_y);
}

TEST_F(EventHandlerSimTest, ElementTargetedGestureScrollViewport) {
  ResizeView(gfx::Size(800, 600));
  // Set a page scale factor so that the VisualViewport will also scroll.
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div style="height:1000px">Tall text to create viewport scrollbar</div>
  )HTML");
  WebView().SetPageScaleFactor(2);
  Compositor().BeginFrame();

  // Delta in (scaled) physical pixels.
  constexpr float delta_y = 1400;
  const VisualViewport& visual_viewport =
      GetDocument().GetPage()->GetVisualViewport();
  const ScrollableArea& layout_viewport =
      *GetDocument().View()->LayoutViewport();

  WebGestureEvent gesture_scroll_begin{
      WebInputEvent::Type::kGestureScrollBegin, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(),
      WebGestureDevice::kTouchscreen};
  gesture_scroll_begin.data.scroll_begin.delta_x_hint = 0;
  gesture_scroll_begin.data.scroll_begin.delta_y_hint = -delta_y;

  // For a viewport-distributed scroll, cc::Viewport::ScrollBy expects the
  // layout viewport to be the "currently scrolling node".  On desktop, viewport
  // scrollbars are owned by the layout viewport, so scrollbar interactions will
  // inject appropriately-targeted GestureScrollBegin.  On Android, scrollbars
  // are owned by the visual viewport, but they don't support interactions, so
  // we never see injected GSB targeting the visual viewport.
  gesture_scroll_begin.data.scroll_begin.scrollable_area_element_id =
      layout_viewport.GetScrollElementId().GetInternalValue();

  GetWebFrameWidget().DispatchThroughCcInputHandler(gesture_scroll_begin);

  WebGestureEvent gesture_scroll_update{
      WebInputEvent::Type::kGestureScrollUpdate, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(),
      WebGestureDevice::kTouchscreen};
  gesture_scroll_update.data.scroll_update.delta_x = 0;
  gesture_scroll_update.data.scroll_update.delta_y = -delta_y;

  GetWebFrameWidget().DispatchThroughCcInputHandler(gesture_scroll_update);

  WebGestureEvent gesture_scroll_end{
      WebInputEvent::Type::kGestureScrollEnd, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(),
      WebGestureDevice::kTouchscreen};
  GetWebFrameWidget().DispatchThroughCcInputHandler(gesture_scroll_end);

  Compositor().BeginFrame();
  ASSERT_EQ(layout_viewport.GetScrollOffset().y(), 400);
  ASSERT_EQ(visual_viewport.GetScrollOffset().y(), 300);
}

TEST_F(EventHandlerSimTest, SelecteTransformedTextWhenCapturing) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
      <div id='target' style = "width:250px; transform: rotate(180deg)">
      Some text to select
      </div>
  )HTML");
  Compositor().BeginFrame();

  WebMouseEvent mouse_down_event(WebInputEvent::Type::kMouseDown,
                                 gfx::PointF(100, 20), gfx::PointF(0, 0),
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);

  ASSERT_TRUE(GetDocument()
                  .GetFrame()
                  ->GetEventHandler()
                  .GetSelectionController()
                  .MouseDownMayStartSelect());

  Element* target = GetDocument().getElementById(AtomicString("target"));
  GetDocument().GetFrame()->GetEventHandler().SetPointerCapture(
      PointerEventFactory::kMouseId, target);

  WebMouseEvent mouse_move_event(WebInputEvent::Type::kMouseMove,
                                 gfx::PointF(258, 20), gfx::PointF(0, 0),
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  WebMouseEvent mouse_up_event(
      WebMouseEvent::Type::kMouseUp, gfx::PointF(258, 20), gfx::PointF(0, 0),
      WebPointerProperties::Button::kLeft, 1, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(
      mouse_up_event);

  ASSERT_FALSE(GetDocument()
                   .GetFrame()
                   ->GetEventHandler()
                   .GetSelectionController()
                   .MouseDownMayStartSelect());

  ASSERT_TRUE(GetDocument().GetSelection());
  EXPECT_EQ("Some text to select", GetDocument().GetSelection()->toString());
}

// Test that mouse right button down and move to an iframe will route the events
// to iframe correctly.
TEST_F(EventHandlerSimTest, MouseRightButtonDownMoveToIFrame) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));

  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://example.com/frame.html", "text/html");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    div {
      width: 200px;
      height: 200px;
    }
    iframe {
      width: 200px;
      height: 200px;
    }
    </style>
    <div></div>
    <iframe id='frame' src='frame.html'></iframe>
  )HTML");

  frame_resource.Complete("<!DOCTYPE html>");
  Compositor().BeginFrame();
  WebMouseEvent mouse_down_outside_event(
      WebMouseEvent::Type::kMouseDown, gfx::PointF(300, 29),
      gfx::PointF(300, 29), WebPointerProperties::Button::kRight, 0,
      WebInputEvent::Modifiers::kRightButtonDown,
      WebInputEvent::GetStaticTimeStampForTests());
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_down_outside_event, ui::LatencyInfo()));

  WebMouseEvent mouse_move_outside_event(
      WebMouseEvent::Type::kMouseMove, gfx::PointF(300, 29),
      gfx::PointF(300, 29), WebPointerProperties::Button::kRight, 0,
      WebInputEvent::Modifiers::kRightButtonDown,
      WebInputEvent::GetStaticTimeStampForTests());
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_move_outside_event, ui::LatencyInfo()));

  WebMouseEvent mouse_move_inside_event(
      WebMouseEvent::Type::kMouseMove, gfx::PointF(100, 229),
      gfx::PointF(100, 229), WebPointerProperties::Button::kRight, 0,
      WebInputEvent::Modifiers::kRightButtonDown,
      WebInputEvent::GetStaticTimeStampForTests());
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_move_inside_event, ui::LatencyInfo()));
  EXPECT_FALSE(
      GetDocument().GetFrame()->GetEventHandler().IsMousePositionUnknown());
  EXPECT_FALSE(To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild())
                   ->GetEventHandler()
                   .IsMousePositionUnknown());
}

// Tests that pen dragging on an element and moves will keep the element active.
TEST_F(EventHandlerSimTest, PenDraggingOnElementActive) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));

  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    div {
      width: 200px;
      height: 200px;
    }
    </style>
    <div id="target"></div>
  )HTML");

  Compositor().BeginFrame();
  WebMouseEvent pen_down(WebMouseEvent::Type::kMouseDown, gfx::PointF(100, 100),
                         gfx::PointF(100, 100),
                         WebPointerProperties::Button::kLeft, 0,
                         WebInputEvent::Modifiers::kLeftButtonDown,
                         WebInputEvent::GetStaticTimeStampForTests());
  pen_down.pointer_type = blink::WebPointerProperties::PointerType::kPen;
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(pen_down, ui::LatencyInfo()));

  WebMouseEvent pen_move(WebMouseEvent::Type::kMouseMove, gfx::PointF(100, 100),
                         gfx::PointF(100, 100),
                         WebPointerProperties::Button::kLeft, 0,
                         WebInputEvent::Modifiers::kLeftButtonDown,
                         WebInputEvent::GetStaticTimeStampForTests());
  pen_move.pointer_type = blink::WebPointerProperties::PointerType::kPen;
  // Send first mouse move to update mouse event sates.
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(pen_move, ui::LatencyInfo()));

  // Send another mouse move again to update active element to verify mouse
  // event states.
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(pen_move, ui::LatencyInfo()));

  EXPECT_EQ(GetDocument().GetActiveElement(),
            GetDocument().getElementById(AtomicString("target")));
}

TEST_F(EventHandlerSimTest, TestNoCrashOnMouseWheelZeroDelta) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <body>
      <div id="area" style="width:100px;height:100px">
      </div>
      <p id='log'>no wheel event</p>
    </body>
    <script>
      document.addEventListener('wheel', (e) => {
        let log = document.getElementById('log');
        log.innerText = 'received wheel event, deltaX: ' + e.deltaX + ' deltaY: ' + e.deltaY;
      });
    </script>
  )HTML");
  Compositor().BeginFrame();

  // Set mouse position and active web view.
  InitializeMousePositionAndActivateView(50, 50);
  Compositor().BeginFrame();

  WebElement element = GetDocument().getElementById(AtomicString("log"));
  WebMouseWheelEvent wheel_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  wheel_event.SetPositionInScreen(50, 50);
  wheel_event.delta_x = 0;
  wheel_event.delta_y = 0;
  wheel_event.phase = WebMouseWheelEvent::kPhaseBegan;
  GetDocument().GetFrame()->GetEventHandler().HandleWheelEvent(wheel_event);
  EXPECT_EQ("received wheel event, deltaX: 0 deltaY: 0",
            element.InnerHTML().Utf8());
  ASSERT_EQ(0, GetDocument().View()->LayoutViewport()->GetScrollOffset().y());
  ASSERT_EQ(0, GetDocument().View()->LayoutViewport()->GetScrollOffset().x());
}

// The mouse wheel events which have the phases of "MayBegin" or "Cancel"
// should fire wheel events to the DOM.
TEST_F(EventHandlerSimTest, TestNoWheelEventWithPhaseMayBeginAndCancel) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <body>
      <div id="area" style="width:100px;height:100px">
      </div>
      <p id='log'>no wheel event</p>
    </body>
    <script>
      document.addEventListener('wheel', (e) => {
        let log = document.getElementById('log');
        log.innerText = 'received wheel event, deltaX: ' + e.deltaX + ' deltaY: ' + e.deltaY;
      });
    </script>
  )HTML");
  Compositor().BeginFrame();

  // Set mouse position and active web view.
  InitializeMousePositionAndActivateView(50, 50);
  Compositor().BeginFrame();

  WebElement element = GetDocument().getElementById(AtomicString("log"));
  WebMouseWheelEvent wheel_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  wheel_event.SetPositionInScreen(50, 50);
  wheel_event.delta_x = 0;
  wheel_event.delta_y = 0;
  wheel_event.phase = WebMouseWheelEvent::kPhaseMayBegin;
  GetDocument().GetFrame()->GetEventHandler().HandleWheelEvent(wheel_event);
  EXPECT_EQ("no wheel event", element.InnerHTML().Utf8());

  wheel_event.phase = WebMouseWheelEvent::kPhaseCancelled;
  GetDocument().GetFrame()->GetEventHandler().HandleWheelEvent(wheel_event);
  EXPECT_EQ("no wheel event", element.InnerHTML().Utf8());
}

// The mouse wheel events which have the phases of "End" should fire wheel
// events to the DOM, but for other phases like "Begin", "Change" and
// "Stationary", there should be wheels evnets fired to the DOM.
TEST_F(EventHandlerSimTest, TestWheelEventsWithDifferentPhases) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <body>
      <div id="area" style="width:100px;height:100px">
      </div>
      <p id='log'>no wheel event</p>
    </body>
    <script>
      document.addEventListener('wheel', (e) => {
        let log = document.getElementById('log');
        log.innerText = 'received wheel event, deltaX: ' + e.deltaX + ' deltaY: ' + e.deltaY;
      });
    </script>
  )HTML");
  Compositor().BeginFrame();

  // Set mouse position and active web view.
  InitializeMousePositionAndActivateView(50, 50);
  Compositor().BeginFrame();

  auto* element = GetDocument().getElementById(AtomicString("log"));
  WebMouseWheelEvent wheel_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  wheel_event.SetPositionInScreen(50, 50);
  wheel_event.delta_x = 0;
  wheel_event.delta_y = 0;
  wheel_event.phase = WebMouseWheelEvent::kPhaseMayBegin;
  GetDocument().GetFrame()->GetEventHandler().HandleWheelEvent(wheel_event);
  EXPECT_EQ("no wheel event", element->innerHTML().Utf8());

  wheel_event.delta_y = -1;
  wheel_event.phase = WebMouseWheelEvent::kPhaseBegan;
  element->setInnerHTML("no wheel event");
  GetDocument().GetFrame()->GetEventHandler().HandleWheelEvent(wheel_event);
  EXPECT_EQ("received wheel event, deltaX: 0 deltaY: 1",
            element->innerHTML().Utf8());

  wheel_event.delta_y = -2;
  wheel_event.phase = WebMouseWheelEvent::kPhaseChanged;
  element->setInnerHTML("no wheel event");
  GetDocument().GetFrame()->GetEventHandler().HandleWheelEvent(wheel_event);
  EXPECT_EQ("received wheel event, deltaX: 0 deltaY: 2",
            element->innerHTML().Utf8());

  wheel_event.delta_y = -3;
  wheel_event.phase = WebMouseWheelEvent::kPhaseChanged;
  element->setInnerHTML("no wheel event");
  GetDocument().GetFrame()->GetEventHandler().HandleWheelEvent(wheel_event);
  EXPECT_EQ("received wheel event, deltaX: 0 deltaY: 3",
            element->innerHTML().Utf8());

  wheel_event.delta_y = -4;
  wheel_event.phase = WebMouseWheelEvent::kPhaseStationary;
  element->setInnerHTML("no wheel event");
  GetDocument().GetFrame()->GetEventHandler().HandleWheelEvent(wheel_event);
  EXPECT_EQ("received wheel event, deltaX: 0 deltaY: 4",
            element->innerHTML().Utf8());

  wheel_event.delta_y = -5;
  wheel_event.phase = WebMouseWheelEvent::kPhaseChanged;
  element->setInnerHTML("no wheel event");
  GetDocument().GetFrame()->GetEventHandler().HandleWheelEvent(wheel_event);
  EXPECT_EQ("received wheel event, deltaX: 0 deltaY: 5",
            element->innerHTML().Utf8());

  wheel_event.delta_y = 0;
  wheel_event.phase = WebMouseWheelEvent::kPhaseEnded;
  element->setInnerHTML("no wheel event");
  GetDocument().GetFrame()->GetEventHandler().HandleWheelEvent(wheel_event);
  EXPECT_EQ("no wheel event", element->innerHTML().Utf8());
}

TEST_F(EventHandlerSimTest, TestScrollendFiresOnKeyUpAfterScroll) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        #scroller {
          overflow: scroll;
          height: 100px;
          height: 100px;
        }
        #spacer {
          height: 400px;
          height: 400px;
        }
      </style>
      <body>
        <p id='log'></p> <br>
        <div id="scroller" tabindex=0>
          <div id="spacer"></div>
        </div>
      </body>
      <script>
        scroller.addEventListener('scrollend', (e) => {
          let log = document.getElementById('log');
          log.innerText += 'scrollend';
        });
      </script>
      )HTML");
  Compositor().BeginFrame();
  WebKeyboardEvent e{WebInputEvent::Type::kRawKeyDown,
                     WebInputEvent::kNoModifiers,
                     WebInputEvent::GetStaticTimeStampForTests()};
  const int num_keydowns = 5;

  GetDocument()
      .getElementById(AtomicString("scroller"))
      ->Focus(FocusOptions::Create());
  // Send first keyDown.
  e.windows_key_code = VKEY_DOWN;
  e.SetType(WebInputEvent::Type::kKeyDown);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  // BeginFrame to create scroll_animation.
  Compositor().BeginFrame();
  // BeginFrame to Tick scroll_animation far enough to complete scroll.
  Compositor().BeginFrame(0.30);

  // The first invocation of BeginFrame will create another scroll_animation
  // and subsequent ones will update the animation target.
  for (int i = 0; i < num_keydowns - 1; i++) {
    GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
    Compositor().BeginFrame();
  }
  // BeginFrame to advance to the end of the last scroll animation.
  Compositor().BeginFrame(0.15 * num_keydowns);

  // Verify that we have not yet fired scrollend.
  EXPECT_EQ(
      GetDocument().getElementById(AtomicString("log"))->innerHTML().Utf8(),
      "");

  // Fire keyUp, which should tigger a scrollend event.
  e.SetType(WebInputEvent::Type::kKeyUp);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);

  Compositor().BeginFrame();
  EXPECT_EQ(
      GetDocument().getElementById(AtomicString("log"))->innerHTML().Utf8(),
      "scrollend");
}

TEST_F(EventHandlerSimTest, TestScrollendFiresAfterScrollWithEarlyKeyUp) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        #scroller {
          overflow: scroll;
          height: 100px;
          height: 100px;
        }
        #spacer {
          height: 400px;
          height: 400px;
        }
      </style>
      <body>
        <p id='log'></p> <br>
        <div id="scroller" tabindex=0>
          <div id="spacer"></div>
        </div>
      </body>
      <script>
        scroller.addEventListener('scrollend', (e) => {
          let log = document.getElementById('log');
          log.innerText += 'scrollend';
        });
      </script>
      )HTML");

  Compositor().BeginFrame();
  WebKeyboardEvent e{WebInputEvent::Type::kRawKeyDown,
                     WebInputEvent::kNoModifiers,
                     WebInputEvent::GetStaticTimeStampForTests()};

  GetDocument()
      .getElementById(AtomicString("scroller"))
      ->Focus(FocusOptions::Create());

  // Send first keyDown.
  e.windows_key_code = VKEY_DOWN;
  e.SetType(WebInputEvent::Type::kKeyDown);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  // BeginFrame to create first scroll_animation.
  Compositor().BeginFrame();
  // BeginFrame to tick first scroll_animation to completion.
  Compositor().BeginFrame(0.30);

  // Start a second scroll_animation that should end after the keyup event.
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  Compositor().BeginFrame();

  // Verify that we have not yet fired scrollend.
  EXPECT_EQ(
      GetDocument().getElementById(AtomicString("log"))->innerHTML().Utf8(),
      "");

  // Fire keyUp, which should not tigger a scrollend event since another scroll
  // is in progress.
  e.SetType(WebInputEvent::Type::kKeyUp);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);

  // Tick second scroll to completion which should fire scrollend.
  Compositor().BeginFrame(0.30);

  EXPECT_EQ(
      GetDocument().getElementById(AtomicString("log"))->innerHTML().Utf8(),
      "scrollend");
}

TEST_F(EventHandlerSimTest, TestScrollendFiresOnKeyUpAfterScrollInstant) {
  GetDocument().GetSettings()->SetScrollAnimatorEnabled(false);
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        #scroller {
          overflow: scroll;
          height: 100px;
          height: 100px;
        }
        #spacer {
          height: 400px;
          height: 400px;
        }
      </style>
      <body>
        <p id='log'></p> <br>
        <div id="scroller" tabindex=0>
          <div id="spacer"></div>
        </div>
      </body>
      <script>
        scroller.addEventListener('scrollend', (e) => {
          let log = document.getElementById('log');
          log.innerText += 'scrollend';
        });
      </script>
      )HTML");
  Compositor().BeginFrame();
  WebKeyboardEvent e{WebInputEvent::Type::kRawKeyDown,
                     WebInputEvent::kNoModifiers,
                     WebInputEvent::GetStaticTimeStampForTests()};
  const int num_keydowns = 5;

  GetDocument()
      .getElementById(AtomicString("scroller"))
      ->Focus(FocusOptions::Create());
  // Send first keyDown.
  e.windows_key_code = VKEY_DOWN;
  e.SetType(WebInputEvent::Type::kKeyDown);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
  // BeginFrame to trigger first instant scroll.
  Compositor().BeginFrame();

  // Trigger a sequence of instant scrolls.
  for (int i = 0; i < num_keydowns - 1; i++) {
    GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);
    Compositor().BeginFrame();
  }

  // Verify that we have not yet fired scrollend.
  EXPECT_EQ(
      GetDocument().getElementById(AtomicString("log"))->innerHTML().Utf8(),
      "");

  // Fire keyUp, which should trigger a scrollend event.
  e.SetType(WebInputEvent::Type::kKeyUp);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(e);

  Compositor().BeginFrame();
  EXPECT_EQ(
      GetDocument().getElementById(AtomicString("log"))->innerHTML().Utf8(),
      "scrollend");
}

TEST_F(EventHandlerSimTest, DiscardEventsToRecentlyMovedIframe) {
  base::FieldTrialParams field_trial_params;
  field_trial_params["time_ms"] = "500";
  field_trial_params["distance_factor"] = "0.5";
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kDiscardInputEventsToRecentlyMovedFrames, field_trial_params);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://cross-origin.com/iframe.html",
                            "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <iframe id='iframe' src='https://cross-origin.com/iframe.html'></iframe>
  )HTML");
  frame_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Hello, world!</div>
  )HTML");

  // The first BeginFrame() sets the last known position of the iframe. The
  // second BeginFrame(), after a delay with no layout changes, will mark the
  // iframe as having a stable position, so input events will not be discarded.
  Compositor().BeginFrame();
  GetDocument().GetFrame()->View()->ScheduleAnimation();
  Compositor().BeginFrame(0.5);

  // Iframe position is stable, do not discard events.
  WebInputEventResult event_result =
      GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
          WebMouseEvent(WebInputEvent::Type::kMouseDown, gfx::PointF(100, 50),
                        gfx::PointF(100, 50),
                        WebPointerProperties::Button::kLeft, 1,
                        WebInputEvent::Modifiers::kLeftButtonDown,
                        base::TimeTicks::Now()));
  EXPECT_NE(event_result, WebInputEventResult::kHandledSuppressed);
  event_result =
      GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(
          WebMouseEvent(WebInputEvent::Type::kMouseUp, gfx::PointF(100, 50),
                        gfx::PointF(100, 50),
                        WebPointerProperties::Button::kLeft, 1,
                        WebInputEvent::kNoModifiers, base::TimeTicks::Now()));
  EXPECT_NE(event_result, WebInputEventResult::kHandledSuppressed);

  Element* iframe =
      GetDocument().getElementById(AtomicString::FromUTF8("iframe"));
  ASSERT_TRUE(iframe);

  // Move iframe, but within the threshold for discarding. Events should not be
  // discarded.
  iframe->SetInlineStyleProperty(CSSPropertyID::kMarginLeft, "70px");
  iframe->SetInlineStyleProperty(CSSPropertyID::kMarginTop, "40px");
  Compositor().BeginFrame();
  event_result =
      GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
          WebMouseEvent(WebInputEvent::Type::kMouseDown, gfx::PointF(170, 90),
                        gfx::PointF(170, 90),
                        WebPointerProperties::Button::kLeft, 1,
                        WebInputEvent::Modifiers::kLeftButtonDown,
                        base::TimeTicks::Now()));
  EXPECT_NE(event_result, WebInputEventResult::kHandledSuppressed);
  event_result =
      GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(
          WebMouseEvent(WebInputEvent::Type::kMouseUp, gfx::PointF(170, 90),
                        gfx::PointF(170, 90),
                        WebPointerProperties::Button::kLeft, 1,
                        WebInputEvent::kNoModifiers, base::TimeTicks::Now()));
  EXPECT_NE(event_result, WebInputEventResult::kHandledSuppressed);

  // Move iframe past threshold for discarding; events should be discarded.
  iframe->SetInlineStyleProperty(CSSPropertyID::kMarginLeft, "200px");
  Compositor().BeginFrame();
  base::TimeTicks event_time =
      Compositor().LastFrameTime() + base::Milliseconds(400);

  event_result =
      GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
          WebMouseEvent(WebInputEvent::Type::kMouseDown, gfx::PointF(300, 90),
                        gfx::PointF(300, 90),
                        WebPointerProperties::Button::kLeft, 1,
                        WebInputEvent::Modifiers::kLeftButtonDown, event_time));
  EXPECT_EQ(event_result, WebInputEventResult::kHandledSuppressed);
  event_result =
      GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(
          WebMouseEvent(WebInputEvent::Type::kMouseUp, gfx::PointF(300, 90),
                        gfx::PointF(300, 90),
                        WebPointerProperties::Button::kLeft, 1,
                        WebInputEvent::kNoModifiers, event_time));
  EXPECT_EQ(event_result, WebInputEventResult::kHandledSuppressed);

  // A second click on the same target within 5 seconds of a discarded click
  // should be recorded as "mistakenly discarded".
  EXPECT_FALSE(
      To<HTMLIFrameElement>(iframe)
          ->contentDocument()
          ->Loader()
          ->GetUseCounter()
          .IsCounted(
              WebFeature::kInputEventToRecentlyMovedIframeMistakenlyDiscarded));
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      WebMouseEvent(WebInputEvent::Type::kMouseDown, gfx::PointF(300, 90),
                    gfx::PointF(300, 90), WebPointerProperties::Button::kLeft,
                    1, WebInputEvent::Modifiers::kLeftButtonDown, event_time));
  EXPECT_TRUE(
      To<HTMLIFrameElement>(iframe)
          ->contentDocument()
          ->Loader()
          ->GetUseCounter()
          .IsCounted(
              WebFeature::kInputEventToRecentlyMovedIframeMistakenlyDiscarded));
}

// Tests that click.pointerId is valid for a gesture tap for which no low-level
// pointer events (pointerdown, pointermove, pointerup etc) are sent from the
// browser to the renderer because of absence of relevant event listeners.
TEST_F(EventHandlerSimTest, ValidClickPointerIdForUnseenPointerEvent) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='pointer_id' style='width:100px;height:100px'></div>
    <script>
      document.body.addEventListener('click', e => {
        document.getElementById('pointer_id').textContent = e.pointerId;
      });
    </script>
  )HTML");

  WebElement pointer_id_elem =
      GetDocument().getElementById(AtomicString("pointer_id"));
  EXPECT_EQ("", pointer_id_elem.TextContent().Utf8());

  TapEventBuilder tap_event(gfx::PointF(20, 20), 1);

  // Blink-defined behavior: touch pointer-id starts at 2.
  tap_event.primary_unique_touch_event_id = 321;
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(tap_event);
  auto pointer_id_1 = stoi(pointer_id_elem.TextContent().Utf8());
  EXPECT_GT(pointer_id_1, 1);

  // Blink-defined behavior: pointer-id increases with each new event.
  tap_event.primary_unique_touch_event_id = 123;
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(tap_event);
  auto pointer_id_2 = stoi(pointer_id_elem.TextContent().Utf8());
  EXPECT_GT(pointer_id_2, pointer_id_1);
}

TEST_F(EventHandlerSimTest, GestureTapHoverState) {
  ResizeView(gfx::Size(800, 600));

  // RecomputeMouseHoverState() bails early if we are not focused.
  GetPage().SetFocused(true);

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        body { height: 1000px; margin: 0; }
        p { height: 100px; margin: 0; background: white; }
        p:hover { background: red; }
      </style>
      <body>
        <p id=a>A</p>
        <p id=b>B</p>
      </body>
      )HTML");

  Compositor().BeginFrame();
  Document& doc = GetDocument();
  LayoutObject* a = doc.getElementById(AtomicString("a"))->GetLayoutObject();
  LayoutObject* b = doc.getElementById(AtomicString("b"))->GetLayoutObject();

  auto ColorOf = [](const LayoutObject* lo) {
    const auto& bg_color_prop = GetCSSPropertyBackgroundColor();
    Color color = lo->Style()->VisitedDependentColor(bg_color_prop);
    return color.SerializeAsCSSColor();
  };
  String rgb_white = "rgb(255, 255, 255)";
  String rgb_red = "rgb(255, 0, 0)";

  EXPECT_EQ(rgb_white, ColorOf(a));
  EXPECT_EQ(rgb_white, ColorOf(b));

  TapEventBuilder tap(gfx::PointF(10, 10), 1);
  doc.GetFrame()->GetEventHandler().HandleGestureEvent(tap);
  Compositor().BeginFrame();

  // #a is hovered after tap.
  EXPECT_EQ(rgb_red, ColorOf(a));
  EXPECT_EQ(rgb_white, ColorOf(b));

  doc.scrollingElement()->scrollBy(0, 100);
  Compositor().BeginFrame();

  // #a is still hovered after scrolling away (crbug.com/366020097).
  EXPECT_EQ(rgb_red, ColorOf(a));
  EXPECT_EQ(rgb_white, ColorOf(b));
}

}  // namespace blink
