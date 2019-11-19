/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/core/events/gesture_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/touch_event.h"
#include "third_party/blink/renderer/core/events/wheel_event.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/input/touch.h"
#include "third_party/blink/renderer/core/input/touch_list.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

namespace {

KeyboardEvent* CreateKeyboardEventWithLocation(
    KeyboardEvent::KeyLocationCode location) {
  KeyboardEventInit* key_event_init = KeyboardEventInit::Create();
  key_event_init->setBubbles(true);
  key_event_init->setCancelable(true);
  key_event_init->setLocation(location);
  return MakeGarbageCollected<KeyboardEvent>("keydown", key_event_init);
}

int GetModifiersForKeyLocationCode(KeyboardEvent::KeyLocationCode location) {
  KeyboardEvent* event = CreateKeyboardEventWithLocation(location);
  WebKeyboardEventBuilder converted_event(*event);
  return converted_event.GetModifiers();
}

void RegisterMockedURL(const std::string& base_url,
                       const std::string& file_name) {
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8(base_url), test::CoreTestDataPath(),
      WebString::FromUTF8(file_name));
}

}  // namespace

TEST(WebInputEventConversionTest, WebKeyboardEventBuilder) {
  // Test key location conversion.
  int modifiers =
      GetModifiersForKeyLocationCode(KeyboardEvent::kDomKeyLocationStandard);
  EXPECT_FALSE(modifiers & WebInputEvent::kIsKeyPad ||
               modifiers & WebInputEvent::kIsLeft ||
               modifiers & WebInputEvent::kIsRight);

  modifiers =
      GetModifiersForKeyLocationCode(KeyboardEvent::kDomKeyLocationLeft);
  EXPECT_TRUE(modifiers & WebInputEvent::kIsLeft);
  EXPECT_FALSE(modifiers & WebInputEvent::kIsKeyPad ||
               modifiers & WebInputEvent::kIsRight);

  modifiers =
      GetModifiersForKeyLocationCode(KeyboardEvent::kDomKeyLocationRight);
  EXPECT_TRUE(modifiers & WebInputEvent::kIsRight);
  EXPECT_FALSE(modifiers & WebInputEvent::kIsKeyPad ||
               modifiers & WebInputEvent::kIsLeft);

  modifiers =
      GetModifiersForKeyLocationCode(KeyboardEvent::kDomKeyLocationNumpad);
  EXPECT_TRUE(modifiers & WebInputEvent::kIsKeyPad);
  EXPECT_FALSE(modifiers & WebInputEvent::kIsLeft ||
               modifiers & WebInputEvent::kIsRight);
}

TEST(WebInputEventConversionTest, WebMouseEventBuilder) {
  TouchEvent* event = TouchEvent::Create();
  WebMouseEventBuilder mouse(nullptr, nullptr, *event);
  EXPECT_EQ(WebInputEvent::kUndefined, mouse.GetType());
}

TEST(WebInputEventConversionTest, InputEventsScaling) {
  const std::string base_url("http://www.test1.com/");
  const std::string file_name("fixed_layout.html");

  RegisterMockedURL(base_url, file_name);
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view =
      web_view_helper.InitializeAndLoad(base_url + file_name);
  web_view->GetSettings()->SetViewportEnabled(true);
  int page_width = 640;
  int page_height = 480;
  web_view->MainFrameWidget()->Resize(WebSize(page_width, page_height));
  web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
      WebWidget::LifecycleUpdateReason::kTest);

  web_view->SetPageScaleFactor(3);

  LocalFrameView* view =
      To<LocalFrame>(web_view->GetPage()->MainFrame())->View();

  {
    WebMouseEvent web_mouse_event(WebInputEvent::kMouseMove,
                                  WebInputEvent::kNoModifiers,
                                  WebInputEvent::GetStaticTimeStampForTests());
    web_mouse_event.SetPositionInWidget(15, 15);
    web_mouse_event.SetPositionInScreen(15, 15);
    web_mouse_event.movement_x = 15;
    web_mouse_event.movement_y = 15;

    WebMouseEvent transformed_event =
        TransformWebMouseEvent(view, web_mouse_event);
    IntPoint position =
        FlooredIntPoint(transformed_event.PositionInRootFrame());
    EXPECT_EQ(5, position.X());
    EXPECT_EQ(5, position.Y());
    EXPECT_EQ(15, transformed_event.PositionInScreen().x);
    EXPECT_EQ(15, transformed_event.PositionInScreen().y);

    EXPECT_EQ(15, transformed_event.movement_x);
    EXPECT_EQ(15, transformed_event.movement_y);
  }

  {
    WebGestureEvent web_gesture_event(
        WebInputEvent::kGestureScrollUpdate, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests(),
        WebGestureDevice::kTouchscreen);
    web_gesture_event.SetPositionInWidget(WebFloatPoint(15, 18));
    web_gesture_event.SetPositionInScreen(WebFloatPoint(20, 22));
    web_gesture_event.data.scroll_update.delta_x = 45;
    web_gesture_event.data.scroll_update.delta_y = 48;
    web_gesture_event.data.scroll_update.velocity_x = 40;
    web_gesture_event.data.scroll_update.velocity_y = 42;
    web_gesture_event.data.scroll_update.inertial_phase =
        WebGestureEvent::InertialPhaseState::kMomentum;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntPoint position =
        FlooredIntPoint(scaled_gesture_event.PositionInRootFrame());
    EXPECT_EQ(5, position.X());
    EXPECT_EQ(6, position.Y());
    EXPECT_EQ(20, scaled_gesture_event.PositionInScreen().x);
    EXPECT_EQ(22, scaled_gesture_event.PositionInScreen().y);
    EXPECT_EQ(15, scaled_gesture_event.DeltaXInRootFrame());
    EXPECT_EQ(16, scaled_gesture_event.DeltaYInRootFrame());
    // TODO: The velocity values may need to be scaled to page scale in
    // order to remain consist with delta values.
    EXPECT_EQ(40, scaled_gesture_event.VelocityX());
    EXPECT_EQ(42, scaled_gesture_event.VelocityY());
    EXPECT_EQ(WebGestureEvent::InertialPhaseState::kMomentum,
              scaled_gesture_event.InertialPhase());
  }

  {
    WebGestureEvent web_gesture_event(
        WebInputEvent::kGestureScrollEnd, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests(),
        WebGestureDevice::kTouchscreen);
    web_gesture_event.SetPositionInWidget(WebFloatPoint(15, 18));
    web_gesture_event.SetPositionInScreen(WebFloatPoint(20, 22));

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntPoint position =
        FlooredIntPoint(scaled_gesture_event.PositionInRootFrame());
    EXPECT_EQ(5, position.X());
    EXPECT_EQ(6, position.Y());
    EXPECT_EQ(20, scaled_gesture_event.PositionInScreen().x);
    EXPECT_EQ(22, scaled_gesture_event.PositionInScreen().y);
    EXPECT_EQ(WebGestureEvent::InertialPhaseState::kUnknownMomentum,
              scaled_gesture_event.InertialPhase());
  }

  {
    WebGestureEvent web_gesture_event(
        WebInputEvent::kGestureTap, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests(),
        WebGestureDevice::kTouchscreen);
    web_gesture_event.data.tap.width = 15;
    web_gesture_event.data.tap.height = 15;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntSize area = FlooredIntSize(scaled_gesture_event.TapAreaInRootFrame());
    EXPECT_EQ(5, area.Width());
    EXPECT_EQ(5, area.Height());
  }

  {
    WebGestureEvent web_gesture_event(
        WebInputEvent::kGestureTapUnconfirmed, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests(),
        WebGestureDevice::kTouchscreen);
    web_gesture_event.data.tap.width = 30;
    web_gesture_event.data.tap.height = 30;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntSize area = FlooredIntSize(scaled_gesture_event.TapAreaInRootFrame());
    EXPECT_EQ(10, area.Width());
    EXPECT_EQ(10, area.Height());
  }

  {
    WebGestureEvent web_gesture_event(
        WebInputEvent::kGestureTapDown, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests(),
        WebGestureDevice::kTouchscreen);
    web_gesture_event.data.tap_down.width = 9;
    web_gesture_event.data.tap_down.height = 9;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntSize area = FlooredIntSize(scaled_gesture_event.TapAreaInRootFrame());
    EXPECT_EQ(3, area.Width());
    EXPECT_EQ(3, area.Height());
  }

  {
    WebGestureEvent web_gesture_event(
        WebInputEvent::kGestureShowPress, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests(),
        WebGestureDevice::kTouchscreen);
    web_gesture_event.data.show_press.width = 18;
    web_gesture_event.data.show_press.height = 18;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntSize area = FlooredIntSize(scaled_gesture_event.TapAreaInRootFrame());
    EXPECT_EQ(6, area.Width());
    EXPECT_EQ(6, area.Height());
  }

  {
    WebGestureEvent web_gesture_event(
        WebInputEvent::kGestureLongPress, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests(),
        WebGestureDevice::kTouchscreen);
    web_gesture_event.data.long_press.width = 15;
    web_gesture_event.data.long_press.height = 15;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntSize area = FlooredIntSize(scaled_gesture_event.TapAreaInRootFrame());
    EXPECT_EQ(5, area.Width());
    EXPECT_EQ(5, area.Height());
  }

  {
    WebGestureEvent web_gesture_event(
        WebInputEvent::kGestureTwoFingerTap, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests(),
        WebGestureDevice::kTouchscreen);
    web_gesture_event.data.two_finger_tap.first_finger_width = 15;
    web_gesture_event.data.two_finger_tap.first_finger_height = 15;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntSize area = FlooredIntSize(scaled_gesture_event.TapAreaInRootFrame());
    EXPECT_EQ(5, area.Width());
    EXPECT_EQ(5, area.Height());
  }

  {
    WebPointerEvent web_pointer_event(
        WebInputEvent::kPointerDown,
        WebPointerProperties(1, WebPointerProperties::PointerType::kTouch,
                             WebPointerProperties::Button::kLeft,
                             WebFloatPoint(10.8f, 10.5f),
                             WebFloatPoint(10.8f, 10.5f), 30, 30),
        6.6f, 9.9f);
    EXPECT_FLOAT_EQ(10.8f, web_pointer_event.PositionInScreen().x);
    EXPECT_FLOAT_EQ(10.5f, web_pointer_event.PositionInScreen().y);
    EXPECT_FLOAT_EQ(10.8f, web_pointer_event.PositionInWidget().x);
    EXPECT_FLOAT_EQ(10.5f, web_pointer_event.PositionInWidget().y);
    EXPECT_FLOAT_EQ(6.6f, web_pointer_event.width);
    EXPECT_FLOAT_EQ(9.9f, web_pointer_event.height);
    EXPECT_EQ(30, web_pointer_event.movement_x);
    EXPECT_EQ(30, web_pointer_event.movement_y);

    WebPointerEvent transformed_event =
        TransformWebPointerEvent(view, web_pointer_event)
            .WebPointerEventInRootFrame();
    EXPECT_FLOAT_EQ(10.8f, transformed_event.PositionInScreen().x);
    EXPECT_FLOAT_EQ(10.5f, transformed_event.PositionInScreen().y);
    EXPECT_FLOAT_EQ(3.6f, transformed_event.PositionInWidget().x);
    EXPECT_FLOAT_EQ(3.5f, transformed_event.PositionInWidget().y);
    EXPECT_FLOAT_EQ(2.2f, transformed_event.width);
    EXPECT_FLOAT_EQ(3.3f, transformed_event.height);
    EXPECT_EQ(30, transformed_event.movement_x);
    EXPECT_EQ(30, transformed_event.movement_y);
  }
}

TEST(WebInputEventConversionTest, InputEventsTransform) {
  const std::string base_url("http://www.test2.com/");
  const std::string file_name("fixed_layout.html");

  RegisterMockedURL(base_url, file_name);
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view =
      web_view_helper.InitializeAndLoad(base_url + file_name);
  web_view->GetSettings()->SetViewportEnabled(true);
  int page_width = 640;
  int page_height = 480;
  web_view->MainFrameWidget()->Resize(WebSize(page_width, page_height));
  web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
      WebWidget::LifecycleUpdateReason::kTest);

  web_view->SetPageScaleFactor(2);

  LocalFrameView* view =
      To<LocalFrame>(web_view->GetPage()->MainFrame())->View();

  {
    WebMouseEvent web_mouse_event(WebInputEvent::kMouseMove,
                                  WebInputEvent::kNoModifiers,
                                  WebInputEvent::GetStaticTimeStampForTests());
    web_mouse_event.SetPositionInWidget(90, 90);
    web_mouse_event.SetPositionInScreen(90, 90);
    web_mouse_event.movement_x = 60;
    web_mouse_event.movement_y = 60;

    WebMouseEvent transformed_event =
        TransformWebMouseEvent(view, web_mouse_event);
    FloatPoint position = transformed_event.PositionInRootFrame();

    EXPECT_FLOAT_EQ(45, position.X());
    EXPECT_FLOAT_EQ(45, position.Y());
    EXPECT_EQ(90, transformed_event.PositionInScreen().x);
    EXPECT_EQ(90, transformed_event.PositionInScreen().y);
    EXPECT_EQ(60, transformed_event.movement_x);
    EXPECT_EQ(60, transformed_event.movement_y);
  }

  {
    WebMouseEvent web_mouse_event1(WebInputEvent::kMouseMove,
                                   WebInputEvent::kNoModifiers,
                                   WebInputEvent::GetStaticTimeStampForTests());
    web_mouse_event1.SetPositionInWidget(90, 90);
    web_mouse_event1.SetPositionInScreen(90, 90);
    web_mouse_event1.movement_x = 60;
    web_mouse_event1.movement_y = 60;

    WebMouseEvent web_mouse_event2 = web_mouse_event1;
    web_mouse_event2.SetPositionInWidget(web_mouse_event1.PositionInWidget().x,
                                         120);
    web_mouse_event2.SetPositionInScreen(web_mouse_event1.PositionInScreen().x,
                                         120);
    web_mouse_event2.movement_y = 30;

    WebVector<const WebInputEvent*> events;
    events.emplace_back(&web_mouse_event1);
    events.emplace_back(&web_mouse_event2);

    Vector<WebMouseEvent> coalescedevents =
        TransformWebMouseEventVector(view, events);
    EXPECT_EQ(events.size(), coalescedevents.size());

    FloatPoint position = coalescedevents[0].PositionInRootFrame();
    EXPECT_FLOAT_EQ(45, position.X());
    EXPECT_FLOAT_EQ(45, position.Y());
    EXPECT_EQ(90, coalescedevents[0].PositionInScreen().x);
    EXPECT_EQ(90, coalescedevents[0].PositionInScreen().y);

    EXPECT_EQ(60, coalescedevents[0].movement_x);
    EXPECT_EQ(60, coalescedevents[0].movement_y);

    position = coalescedevents[1].PositionInRootFrame();
    EXPECT_FLOAT_EQ(45, position.X());
    EXPECT_FLOAT_EQ(60, position.Y());
    EXPECT_EQ(90, coalescedevents[1].PositionInScreen().x);
    EXPECT_EQ(120, coalescedevents[1].PositionInScreen().y);

    EXPECT_EQ(60, coalescedevents[1].movement_x);
    EXPECT_EQ(30, coalescedevents[1].movement_y);
  }

  {
    WebGestureEvent web_gesture_event(
        WebInputEvent::kGestureScrollUpdate, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests(),
        WebGestureDevice::kTouchscreen);
    web_gesture_event.SetPositionInWidget(WebFloatPoint(90, 90));
    web_gesture_event.SetPositionInScreen(WebFloatPoint(90, 90));
    web_gesture_event.data.scroll_update.delta_x = 60;
    web_gesture_event.data.scroll_update.delta_y = 60;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    FloatPoint position = scaled_gesture_event.PositionInRootFrame();

    EXPECT_FLOAT_EQ(45, position.X());
    EXPECT_FLOAT_EQ(45, position.Y());
    EXPECT_EQ(90, scaled_gesture_event.PositionInScreen().x);
    EXPECT_EQ(90, scaled_gesture_event.PositionInScreen().y);
    EXPECT_EQ(30, scaled_gesture_event.DeltaXInRootFrame());
    EXPECT_EQ(30, scaled_gesture_event.DeltaYInRootFrame());
  }

  {
    WebGestureEvent web_gesture_event(
        WebInputEvent::kGestureTap, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests(),
        WebGestureDevice::kTouchscreen);
    web_gesture_event.data.tap.width = 30;
    web_gesture_event.data.tap.height = 30;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntSize area = FlooredIntSize(scaled_gesture_event.TapAreaInRootFrame());
    EXPECT_EQ(15, area.Width());
    EXPECT_EQ(15, area.Height());
  }

  {
    WebGestureEvent web_gesture_event(
        WebInputEvent::kGestureTapUnconfirmed, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests(),
        WebGestureDevice::kTouchscreen);
    web_gesture_event.data.tap.width = 30;
    web_gesture_event.data.tap.height = 30;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntSize area = FlooredIntSize(scaled_gesture_event.TapAreaInRootFrame());
    EXPECT_EQ(15, area.Width());
    EXPECT_EQ(15, area.Height());
  }

  {
    WebGestureEvent web_gesture_event(
        WebInputEvent::kGestureTapDown, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests(),
        WebGestureDevice::kTouchscreen);
    web_gesture_event.data.tap_down.width = 30;
    web_gesture_event.data.tap_down.height = 30;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntSize area = FlooredIntSize(scaled_gesture_event.TapAreaInRootFrame());
    EXPECT_EQ(15, area.Width());
    EXPECT_EQ(15, area.Height());
  }

  {
    WebGestureEvent web_gesture_event(
        WebInputEvent::kGestureShowPress, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests(),
        WebGestureDevice::kTouchscreen);
    web_gesture_event.data.show_press.width = 30;
    web_gesture_event.data.show_press.height = 30;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntSize area = FlooredIntSize(scaled_gesture_event.TapAreaInRootFrame());
    EXPECT_EQ(15, area.Width());
    EXPECT_EQ(15, area.Height());
  }

  {
    WebGestureEvent web_gesture_event(
        WebInputEvent::kGestureLongPress, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests(),
        WebGestureDevice::kTouchscreen);
    web_gesture_event.data.long_press.width = 30;
    web_gesture_event.data.long_press.height = 30;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntSize area = FlooredIntSize(scaled_gesture_event.TapAreaInRootFrame());
    EXPECT_EQ(15, area.Width());
    EXPECT_EQ(15, area.Height());
  }

  {
    WebGestureEvent web_gesture_event(
        WebInputEvent::kGestureTwoFingerTap, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests(),
        WebGestureDevice::kTouchscreen);
    web_gesture_event.data.two_finger_tap.first_finger_width = 30;
    web_gesture_event.data.two_finger_tap.first_finger_height = 30;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntSize area = FlooredIntSize(scaled_gesture_event.TapAreaInRootFrame());
    EXPECT_EQ(15, area.Width());
    EXPECT_EQ(15, area.Height());
  }

  {
    WebPointerEvent web_pointer_event(
        WebInputEvent::kPointerDown,
        WebPointerProperties(1, WebPointerProperties::PointerType::kTouch,
                             WebPointerProperties::Button::kLeft,
                             WebFloatPoint(90, 90), WebFloatPoint(90, 90)),
        30, 30);

    WebPointerEvent transformed_event =
        TransformWebPointerEvent(view, web_pointer_event)
            .WebPointerEventInRootFrame();

    EXPECT_FLOAT_EQ(90, transformed_event.PositionInScreen().x);
    EXPECT_FLOAT_EQ(90, transformed_event.PositionInScreen().y);
    EXPECT_FLOAT_EQ(45, transformed_event.PositionInWidget().x);
    EXPECT_FLOAT_EQ(45, transformed_event.PositionInWidget().y);
    EXPECT_FLOAT_EQ(15, transformed_event.width);
    EXPECT_FLOAT_EQ(15, transformed_event.height);
  }

  {
    WebPointerEvent web_pointer_event1(
        WebInputEvent::kPointerDown,
        WebPointerProperties(1, WebPointerProperties::PointerType::kTouch,
                             WebPointerProperties::Button::kLeft,
                             WebFloatPoint(90, 90), WebFloatPoint(90, 90)),
        30, 30);

    WebPointerEvent web_pointer_event2(
        WebInputEvent::kPointerDown,
        WebPointerProperties(1, WebPointerProperties::PointerType::kTouch,
                             WebPointerProperties::Button::kLeft,
                             WebFloatPoint(120, 90), WebFloatPoint(120, 90)),
        60, 30);

    WebVector<const WebInputEvent*> events;
    events.emplace_back(&web_pointer_event1);
    events.emplace_back(&web_pointer_event2);

    Vector<WebPointerEvent> coalescedevents =
        TransformWebPointerEventVector(view, events);
    EXPECT_EQ(events.size(), coalescedevents.size());

    WebPointerEvent transformed_event =
        coalescedevents[0].WebPointerEventInRootFrame();
    EXPECT_FLOAT_EQ(90, transformed_event.PositionInScreen().x);
    EXPECT_FLOAT_EQ(90, transformed_event.PositionInScreen().y);
    EXPECT_FLOAT_EQ(45, transformed_event.PositionInWidget().x);
    EXPECT_FLOAT_EQ(45, transformed_event.PositionInWidget().y);
    EXPECT_FLOAT_EQ(15, transformed_event.width);
    EXPECT_FLOAT_EQ(15, transformed_event.height);

    transformed_event = coalescedevents[1].WebPointerEventInRootFrame();
    EXPECT_FLOAT_EQ(120, transformed_event.PositionInScreen().x);
    EXPECT_FLOAT_EQ(90, transformed_event.PositionInScreen().y);
    EXPECT_FLOAT_EQ(60, transformed_event.PositionInWidget().x);
    EXPECT_FLOAT_EQ(45, transformed_event.PositionInWidget().y);
    EXPECT_FLOAT_EQ(30, transformed_event.width);
    EXPECT_FLOAT_EQ(15, transformed_event.height);
  }
}

TEST(WebInputEventConversionTest, InputEventsConversions) {
  const std::string base_url("http://www.test3.com/");
  const std::string file_name("fixed_layout.html");

  RegisterMockedURL(base_url, file_name);
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view =
      web_view_helper.InitializeAndLoad(base_url + file_name);
  int page_width = 640;
  int page_height = 480;
  web_view->MainFrameWidget()->Resize(WebSize(page_width, page_height));
  web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
      WebWidget::LifecycleUpdateReason::kTest);

  LocalFrameView* view =
      To<LocalFrame>(web_view->GetPage()->MainFrame())->View();
  {
    WebGestureEvent web_gesture_event(
        WebInputEvent::kGestureTap, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests(),
        WebGestureDevice::kTouchscreen);
    web_gesture_event.SetPositionInWidget(WebFloatPoint(10, 10));
    web_gesture_event.SetPositionInScreen(WebFloatPoint(10, 10));
    web_gesture_event.data.tap.tap_count = 1;
    web_gesture_event.data.tap.width = 10;
    web_gesture_event.data.tap.height = 10;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntPoint position =
        FlooredIntPoint(scaled_gesture_event.PositionInRootFrame());
    EXPECT_EQ(10.f, position.X());
    EXPECT_EQ(10.f, position.Y());
    EXPECT_EQ(10.f, scaled_gesture_event.PositionInScreen().x);
    EXPECT_EQ(10.f, scaled_gesture_event.PositionInScreen().y);
    EXPECT_EQ(1, scaled_gesture_event.TapCount());
  }
}

TEST(WebInputEventConversionTest, VisualViewportOffset) {
  const std::string base_url("http://www.test4.com/");
  const std::string file_name("fixed_layout.html");

  RegisterMockedURL(base_url, file_name);
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view =
      web_view_helper.InitializeAndLoad(base_url + file_name);
  int page_width = 640;
  int page_height = 480;
  web_view->MainFrameWidget()->Resize(WebSize(page_width, page_height));
  web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
      WebWidget::LifecycleUpdateReason::kTest);

  web_view->SetPageScaleFactor(2);

  FloatPoint visual_offset(35, 60);
  web_view->GetPage()->GetVisualViewport().SetLocation(visual_offset);

  LocalFrameView* view =
      To<LocalFrame>(web_view->GetPage()->MainFrame())->View();

  {
    WebMouseEvent web_mouse_event(WebInputEvent::kMouseMove,
                                  WebInputEvent::kNoModifiers,
                                  WebInputEvent::GetStaticTimeStampForTests());
    web_mouse_event.SetPositionInWidget(10, 10);
    web_mouse_event.SetPositionInScreen(10, 10);

    WebMouseEvent transformed_mouse_event =
        TransformWebMouseEvent(view, web_mouse_event);
    IntPoint position =
        FlooredIntPoint(transformed_mouse_event.PositionInRootFrame());
    EXPECT_EQ(5 + visual_offset.X(), position.X());
    EXPECT_EQ(5 + visual_offset.Y(), position.Y());
    EXPECT_EQ(10, transformed_mouse_event.PositionInScreen().x);
    EXPECT_EQ(10, transformed_mouse_event.PositionInScreen().y);
  }

  {
    WebMouseWheelEvent web_mouse_wheel_event(
        WebInputEvent::kMouseWheel, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests());
    web_mouse_wheel_event.SetPositionInWidget(10, 10);
    web_mouse_wheel_event.SetPositionInScreen(10, 10);

    WebMouseWheelEvent scaled_mouse_wheel_event =
        TransformWebMouseWheelEvent(view, web_mouse_wheel_event);
    IntPoint position =
        FlooredIntPoint(scaled_mouse_wheel_event.PositionInRootFrame());
    EXPECT_EQ(5 + visual_offset.X(), position.X());
    EXPECT_EQ(5 + visual_offset.Y(), position.Y());
    EXPECT_EQ(10, scaled_mouse_wheel_event.PositionInScreen().x);
    EXPECT_EQ(10, scaled_mouse_wheel_event.PositionInScreen().y);
  }

  {
    WebGestureEvent web_gesture_event(
        WebInputEvent::kGestureScrollUpdate, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests(),
        WebGestureDevice::kTouchscreen);
    web_gesture_event.SetPositionInWidget(WebFloatPoint(10, 10));
    web_gesture_event.SetPositionInScreen(WebFloatPoint(10, 10));

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntPoint position =
        FlooredIntPoint(scaled_gesture_event.PositionInRootFrame());
    EXPECT_EQ(5 + visual_offset.X(), position.X());
    EXPECT_EQ(5 + visual_offset.Y(), position.Y());
    EXPECT_EQ(10, scaled_gesture_event.PositionInScreen().x);
    EXPECT_EQ(10, scaled_gesture_event.PositionInScreen().y);
  }

  {
    WebPointerEvent web_pointer_event(
        WebInputEvent::kPointerDown,
        WebPointerProperties(1, WebPointerProperties::PointerType::kTouch,
                             WebPointerProperties::Button::kLeft,
                             WebFloatPoint(10.6f, 10.4f),
                             WebFloatPoint(10.6f, 10.4f)),
        10, 10);

    EXPECT_FLOAT_EQ(10.6f, web_pointer_event.PositionInScreen().x);
    EXPECT_FLOAT_EQ(10.4f, web_pointer_event.PositionInScreen().y);
    EXPECT_FLOAT_EQ(10.6f, web_pointer_event.PositionInWidget().x);
    EXPECT_FLOAT_EQ(10.4f, web_pointer_event.PositionInWidget().y);

    WebPointerEvent transformed_event =
        TransformWebPointerEvent(view, web_pointer_event)
            .WebPointerEventInRootFrame();
    EXPECT_FLOAT_EQ(10.6f, transformed_event.PositionInScreen().x);
    EXPECT_FLOAT_EQ(10.4f, transformed_event.PositionInScreen().y);
    EXPECT_FLOAT_EQ(5.3f + visual_offset.X(),
                    transformed_event.PositionInWidget().x);
    EXPECT_FLOAT_EQ(5.2f + visual_offset.Y(),
                    transformed_event.PositionInWidget().y);
  }
}

TEST(WebInputEventConversionTest, ElasticOverscroll) {
  const std::string base_url("http://www.test5.com/");
  const std::string file_name("fixed_layout.html");

  RegisterMockedURL(base_url, file_name);
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view =
      web_view_helper.InitializeAndLoad(base_url + file_name);
  int page_width = 640;
  int page_height = 480;
  web_view->MainFrameWidget()->Resize(WebSize(page_width, page_height));
  web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
      WebWidget::LifecycleUpdateReason::kTest);

  LocalFrameView* view =
      To<LocalFrame>(web_view->GetPage()->MainFrame())->View();

  gfx::Vector2dF elastic_overscroll(10, -20);
  web_view->MainFrameWidget()->ApplyViewportChanges(
      {gfx::ScrollOffset(), elastic_overscroll, 1.0f, false, 0.0f});

  // Just elastic overscroll.
  {
    WebMouseEvent web_mouse_event(WebInputEvent::kMouseMove,
                                  WebInputEvent::kNoModifiers,
                                  WebInputEvent::GetStaticTimeStampForTests());
    web_mouse_event.SetPositionInWidget(10, 50);
    web_mouse_event.SetPositionInScreen(10, 50);

    WebMouseEvent transformed_mouse_event =
        TransformWebMouseEvent(view, web_mouse_event);
    IntPoint position =
        FlooredIntPoint(transformed_mouse_event.PositionInRootFrame());

    EXPECT_EQ(web_mouse_event.PositionInWidget().x + elastic_overscroll.x(),
              position.X());
    EXPECT_EQ(web_mouse_event.PositionInWidget().y + elastic_overscroll.y(),
              position.Y());
    EXPECT_EQ(web_mouse_event.PositionInScreen().x,
              transformed_mouse_event.PositionInScreen().x);
    EXPECT_EQ(web_mouse_event.PositionInScreen().y,
              transformed_mouse_event.PositionInScreen().y);
  }

  // Elastic overscroll and pinch-zoom (this doesn't actually ever happen,
  // but ensure that if it were to, the overscroll would be applied after the
  // pinch-zoom).
  float page_scale = 2;
  web_view->SetPageScaleFactor(page_scale);
  FloatPoint visual_offset(35, 60);
  web_view->GetPage()->GetVisualViewport().SetLocation(visual_offset);
  {
    WebMouseEvent web_mouse_event(WebInputEvent::kMouseMove,
                                  WebInputEvent::kNoModifiers,
                                  WebInputEvent::GetStaticTimeStampForTests());
    web_mouse_event.SetPositionInWidget(10, 10);
    web_mouse_event.SetPositionInScreen(10, 10);

    WebMouseEvent transformed_mouse_event =
        TransformWebMouseEvent(view, web_mouse_event);
    IntPoint position =
        FlooredIntPoint(transformed_mouse_event.PositionInRootFrame());

    EXPECT_EQ(web_mouse_event.PositionInWidget().x / page_scale +
                  visual_offset.X() + elastic_overscroll.x(),
              position.X());
    EXPECT_EQ(web_mouse_event.PositionInWidget().y / page_scale +
                  visual_offset.Y() + elastic_overscroll.y(),
              position.Y());
    EXPECT_EQ(web_mouse_event.PositionInScreen().x,
              transformed_mouse_event.PositionInScreen().x);
    EXPECT_EQ(web_mouse_event.PositionInScreen().y,
              transformed_mouse_event.PositionInScreen().y);
  }
}

// Page reload/navigation should not reset elastic overscroll.
TEST(WebInputEventConversionTest, ElasticOverscrollWithPageReload) {
  const std::string base_url("http://www.test6.com/");
  const std::string file_name("fixed_layout.html");

  RegisterMockedURL(base_url, file_name);
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view =
      web_view_helper.InitializeAndLoad(base_url + file_name);
  int page_width = 640;
  int page_height = 480;
  web_view->MainFrameWidget()->Resize(WebSize(page_width, page_height));
  web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
      WebWidget::LifecycleUpdateReason::kTest);

  gfx::Vector2dF elastic_overscroll(10, -20);
  web_view->MainFrameWidget()->ApplyViewportChanges(
      {gfx::ScrollOffset(), elastic_overscroll, 1.0f, false, 0.0f});
  frame_test_helpers::ReloadFrame(
      web_view_helper.GetWebView()->MainFrameImpl());
  LocalFrameView* view =
      To<LocalFrame>(web_view->GetPage()->MainFrame())->View();

  // Just elastic overscroll.
  {
    WebMouseEvent web_mouse_event(WebInputEvent::kMouseMove,
                                  WebInputEvent::kNoModifiers,
                                  WebInputEvent::GetStaticTimeStampForTests());
    web_mouse_event.SetPositionInWidget(10, 50);
    web_mouse_event.SetPositionInScreen(10, 50);

    WebMouseEvent transformed_mouse_event =
        TransformWebMouseEvent(view, web_mouse_event);
    IntPoint position =
        FlooredIntPoint(transformed_mouse_event.PositionInRootFrame());

    EXPECT_EQ(web_mouse_event.PositionInWidget().x + elastic_overscroll.x(),
              position.X());
    EXPECT_EQ(web_mouse_event.PositionInWidget().y + elastic_overscroll.y(),
              position.Y());
    EXPECT_EQ(web_mouse_event.PositionInScreen().x,
              transformed_mouse_event.PositionInScreen().x);
    EXPECT_EQ(web_mouse_event.PositionInScreen().y,
              transformed_mouse_event.PositionInScreen().y);
  }
}

}  // namespace blink
