// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/input_utils.h"

#include "build/build_config.h"
#include "pdf/test/mouse_event_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"

namespace chrome_pdf {

namespace {

void CheckNormalizeMouseEventIsNoOp(const blink::WebMouseEvent& event) {
  blink::WebMouseEvent normalized_event = NormalizeMouseEvent(event);
  EXPECT_EQ(event.button, normalized_event.button);
  EXPECT_EQ(event.GetModifiers(), normalized_event.GetModifiers());
  EXPECT_EQ(event.GetType(), normalized_event.GetType());
}

}  // namespace

TEST(InputUtilsTest, NormalizeMouseEventLeftMouseDown) {
  CheckNormalizeMouseEventIsNoOp(
      MouseEventBuilder()
          .SetType(blink::WebInputEvent::Type::kMouseDown)
          .SetButton(blink::WebPointerProperties::Button::kLeft)
          .Build());
}

TEST(InputUtilsTest, NormalizeMouseEventMiddleMouseDown) {
  CheckNormalizeMouseEventIsNoOp(
      MouseEventBuilder()
          .SetType(blink::WebInputEvent::Type::kMouseDown)
          .SetButton(blink::WebPointerProperties::Button::kMiddle)
          .Build());
}

TEST(InputUtilsTest, NormalizeMouseEventRightMouseDown) {
  CheckNormalizeMouseEventIsNoOp(
      MouseEventBuilder()
          .SetType(blink::WebInputEvent::Type::kMouseDown)
          .SetButton(blink::WebPointerProperties::Button::kRight)
          .Build());
}

TEST(InputUtilsTest, NormalizeMouseEventLeftMouseUp) {
  CheckNormalizeMouseEventIsNoOp(
      MouseEventBuilder()
          .SetType(blink::WebInputEvent::Type::kMouseUp)
          .SetButton(blink::WebPointerProperties::Button::kLeft)
          .Build());
}

TEST(InputUtilsTest, NormalizeMouseEventMiddleMouseUp) {
  CheckNormalizeMouseEventIsNoOp(
      MouseEventBuilder()
          .SetType(blink::WebInputEvent::Type::kMouseUp)
          .SetButton(blink::WebPointerProperties::Button::kMiddle)
          .Build());
}

TEST(InputUtilsTest, NormalizeMouseEventRightMouseUp) {
  CheckNormalizeMouseEventIsNoOp(
      MouseEventBuilder()
          .SetType(blink::WebInputEvent::Type::kMouseUp)
          .SetButton(blink::WebPointerProperties::Button::kRight)
          .Build());
}

TEST(InputUtilsTest, NormalizeMouseEventCtrlLeftMouseDown) {
  blink::WebMouseEvent event =
      MouseEventBuilder()
          .SetType(blink::WebInputEvent::Type::kMouseDown)
          .SetButton(blink::WebPointerProperties::Button::kLeft)
          .SetModifiers(blink::WebInputEvent::Modifiers::kControlKey)
          .Build();

#if BUILDFLAG(IS_MAC)
  blink::WebMouseEvent normalized_event = NormalizeMouseEvent(event);
  EXPECT_EQ(blink::WebPointerProperties::Button::kRight,
            normalized_event.button);
  EXPECT_EQ(blink::WebInputEvent::Modifiers::kRightButtonDown,
            normalized_event.GetModifiers());
  EXPECT_EQ(event.GetType(), normalized_event.GetType());
#else
  CheckNormalizeMouseEventIsNoOp(event);
#endif
}

TEST(InputUtilsTest, NormalizeMouseEventCtrlLefttMouseUp) {
  CheckNormalizeMouseEventIsNoOp(
      MouseEventBuilder()
          .SetType(blink::WebInputEvent::Type::kMouseUp)
          .SetButton(blink::WebPointerProperties::Button::kLeft)
          .SetModifiers(blink::WebInputEvent::Modifiers::kControlKey)
          .Build());
}

}  // namespace chrome_pdf
