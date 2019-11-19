// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/gesture_event_stream_validator.h"

#include "content/common/input/synthetic_web_input_event_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::WebInputEvent;
using blink::WebGestureEvent;

namespace content {
namespace {

const blink::WebGestureDevice kDefaultGestureDevice =
    blink::WebGestureDevice::kTouchscreen;

blink::WebGestureEvent Build(WebInputEvent::Type type) {
  blink::WebGestureEvent event =
      SyntheticWebGestureEventBuilder::Build(type, kDefaultGestureDevice);
  // Default to providing a (valid) non-zero fling velocity.
  if (type == WebInputEvent::kGestureFlingStart)
    event.data.fling_start.velocity_x = 5;
  return event;
}

}  // namespace

TEST(GestureEventStreamValidator, ValidScroll) {
  GestureEventStreamValidator validator;
  std::string error_msg;
  WebGestureEvent event;

  event = Build(WebInputEvent::kGestureScrollBegin);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::kGestureScrollUpdate);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::kGestureScrollEnd);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
}

TEST(GestureEventStreamValidator, InvalidScroll) {
  GestureEventStreamValidator validator;
  std::string error_msg;
  WebGestureEvent event;

  // No preceding ScrollBegin.
  event = Build(WebInputEvent::kGestureScrollUpdate);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  // No preceding ScrollBegin.
  event = Build(WebInputEvent::kGestureScrollEnd);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  event = Build(WebInputEvent::kGestureScrollBegin);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  // Already scrolling.
  event = Build(WebInputEvent::kGestureScrollBegin);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  event = Build(WebInputEvent::kGestureScrollEnd);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  // Scroll already ended.
  event = Build(WebInputEvent::kGestureScrollEnd);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());
}

TEST(GestureEventStreamValidator, ValidFling) {
  GestureEventStreamValidator validator;
  std::string error_msg;
  WebGestureEvent event;

  event = Build(WebInputEvent::kGestureScrollBegin);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::kGestureFlingStart);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
}

TEST(GestureEventStreamValidator, InvalidFling) {
  GestureEventStreamValidator validator;
  std::string error_msg;
  WebGestureEvent event;

  // No preceding ScrollBegin.
  event = Build(WebInputEvent::kGestureFlingStart);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  // Zero velocity.
  event = Build(WebInputEvent::kGestureScrollBegin);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::kGestureFlingStart);
  event.data.fling_start.velocity_x = 0;
  event.data.fling_start.velocity_y = 0;
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());
}

TEST(GestureEventStreamValidator, ValidPinch) {
  GestureEventStreamValidator validator;
  std::string error_msg;
  WebGestureEvent event;

  event = Build(WebInputEvent::kGesturePinchBegin);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::kGesturePinchUpdate);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::kGesturePinchEnd);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
}

TEST(GestureEventStreamValidator, InvalidPinch) {
  GestureEventStreamValidator validator;
  std::string error_msg;
  WebGestureEvent event;

  // No preceding PinchBegin.
  event = Build(WebInputEvent::kGesturePinchUpdate);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  event = Build(WebInputEvent::kGesturePinchBegin);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  // ScrollBegin while pinching.
  event = Build(WebInputEvent::kGestureScrollBegin);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  // ScrollEnd while pinching.
  event = Build(WebInputEvent::kGestureScrollEnd);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  // Pinch already begun.
  event = Build(WebInputEvent::kGesturePinchBegin);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  event = Build(WebInputEvent::kGesturePinchEnd);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  // Pinch already ended.
  event = Build(WebInputEvent::kGesturePinchEnd);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());
}

TEST(GestureEventStreamValidator, ValidTap) {
  GestureEventStreamValidator validator;
  std::string error_msg;
  WebGestureEvent event;

  event = Build(WebInputEvent::kGestureTapDown);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::kGestureTapCancel);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::kGestureTapDown);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::kGestureTapUnconfirmed);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::kGestureTapCancel);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::kGestureTapDown);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::kGestureTap);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  // DoubleTap does not require a TapDown (unlike Tap, TapUnconfirmed and
  // TapCancel).
  event = Build(WebInputEvent::kGestureDoubleTap);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
}

TEST(GestureEventStreamValidator, InvalidTap) {
  GestureEventStreamValidator validator;
  std::string error_msg;
  WebGestureEvent event;

  // No preceding TapDown.
  event = Build(WebInputEvent::kGestureTapUnconfirmed);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  event = Build(WebInputEvent::kGestureTapCancel);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  event = Build(WebInputEvent::kGestureTap);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  // TapDown already terminated.
  event = Build(WebInputEvent::kGestureTapDown);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::kGestureDoubleTap);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::kGestureTapCancel);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  // TapDown already terminated.
  event = Build(WebInputEvent::kGestureTapDown);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::kGestureTap);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::kGestureTapCancel);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());
}

}  // namespace content
