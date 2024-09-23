// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/event_filter.h"

#include <fuchsia/web/cpp/fidl.h>

#include "base/types/cxx23_to_underlying.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/test/test_event.h"

using fuchsia::web::InputTypes;

struct EventTypeMappingEntry {
  ui::EventType ui_type;
  fuchsia::web::InputTypes fuchsia_type;
};

constexpr EventTypeMappingEntry kEventTypeMappings[] = {
    {ui::EventType::kMousePressed, InputTypes::MOUSE_CLICK},
    {ui::EventType::kMouseDragged, InputTypes::MOUSE_CLICK},
    {ui::EventType::kMouseReleased, InputTypes::MOUSE_CLICK},

    {ui::EventType::kMouseMoved, InputTypes::MOUSE_MOVE},
    {ui::EventType::kMouseEntered, InputTypes::MOUSE_MOVE},
    {ui::EventType::kMouseExited, InputTypes::MOUSE_MOVE},

    {ui::EventType::kMousewheel, InputTypes::MOUSE_WHEEL},

    {ui::EventType::kGestureTap, InputTypes::GESTURE_TAP},
    {ui::EventType::kGestureTapDown, InputTypes::GESTURE_TAP},
    {ui::EventType::kGestureTapCancel, InputTypes::GESTURE_TAP},
    {ui::EventType::kGestureTapUnconfirmed, InputTypes::GESTURE_TAP},
    {ui::EventType::kGestureDoubleTap, InputTypes::GESTURE_TAP},
    {ui::EventType::kGestureTwoFingerTap, InputTypes::GESTURE_TAP},
    {ui::EventType::kGestureLongPress, InputTypes::GESTURE_TAP},
    {ui::EventType::kGestureLongTap, InputTypes::GESTURE_TAP},

    {ui::EventType::kGesturePinchBegin, InputTypes::GESTURE_PINCH},
    {ui::EventType::kGesturePinchEnd, InputTypes::GESTURE_PINCH},
    {ui::EventType::kGesturePinchUpdate, InputTypes::GESTURE_PINCH},

    {ui::EventType::kGestureScrollBegin, InputTypes::GESTURE_DRAG},
    {ui::EventType::kGestureScrollEnd, InputTypes::GESTURE_DRAG},
    {ui::EventType::kGestureScrollUpdate, InputTypes::GESTURE_DRAG},
    {ui::EventType::kGestureSwipe, InputTypes::GESTURE_DRAG},
    {ui::EventType::kScroll, InputTypes::GESTURE_DRAG},
    {ui::EventType::kScrollFlingStart, InputTypes::GESTURE_DRAG},
    {ui::EventType::kScrollFlingCancel, InputTypes::GESTURE_DRAG},

    {ui::EventType::kKeyPressed, InputTypes::KEY},
    {ui::EventType::kKeyReleased, InputTypes::KEY},
};

constexpr ui::EventType kAlwaysAllowedEventTypes[] = {
    ui::EventType::kTouchReleased,   ui::EventType::kTouchPressed,
    ui::EventType::kTouchMoved,      ui::EventType::kTouchCancelled,
    ui::EventType::kDropTargetEvent, ui::EventType::kGestureShowPress,
    ui::EventType::kGestureBegin,    ui::EventType::kGestureEnd,
    ui::EventType::kCancelMode,      ui::EventType::kMouseCaptureChanged,
};

constexpr ui::EventType kUserEvent =
    static_cast<ui::EventType>(base::to_underlying(ui::EventType::kLast) + 1);

class EventFilterTest : public testing::Test {
 public:
  EventFilterTest() = default;
  ~EventFilterTest() override = default;

 protected:
  void OnEvent(ui::Event* event) { event_filter_.OnEvent(event); }

  EventFilter event_filter_;
};

TEST_F(EventFilterTest, AllowedByDefault) {
  for (const auto& entry : kEventTypeMappings) {
    ui::test::TestEvent event(entry.ui_type);
    ASSERT_FALSE(event.stopped_propagation());
    OnEvent(&event);
    EXPECT_FALSE(event.stopped_propagation());
  }
}

TEST_F(EventFilterTest, SelectivelyAllowed) {
  event_filter_.ConfigureInputTypes(fuchsia::web::InputTypes::ALL,
                                    fuchsia::web::AllowInputState::DENY);
  for (const auto& entry : kEventTypeMappings) {
    event_filter_.ConfigureInputTypes(entry.fuchsia_type,
                                      fuchsia::web::AllowInputState::ALLOW);
    ui::test::TestEvent event(entry.ui_type);
    ASSERT_FALSE(event.stopped_propagation());
    OnEvent(&event);
    EXPECT_FALSE(event.stopped_propagation());
  }
}

TEST_F(EventFilterTest, AllDenied) {
  event_filter_.ConfigureInputTypes(fuchsia::web::InputTypes::ALL,
                                    fuchsia::web::AllowInputState::DENY);
  for (const auto& entry : kEventTypeMappings) {
    ui::test::TestEvent event(entry.ui_type);
    ASSERT_FALSE(event.stopped_propagation());
    OnEvent(&event);
    EXPECT_TRUE(event.stopped_propagation());
  }
}

TEST_F(EventFilterTest, SelectivelyDenied) {
  for (const auto& entry : kEventTypeMappings) {
    event_filter_.ConfigureInputTypes(entry.fuchsia_type,
                                      fuchsia::web::AllowInputState::DENY);
    ui::test::TestEvent event(entry.ui_type);
    ASSERT_FALSE(event.stopped_propagation());
    OnEvent(&event);
    EXPECT_TRUE(event.stopped_propagation());
  }
}

TEST_F(EventFilterTest, AllowCombination) {
  event_filter_.ConfigureInputTypes(fuchsia::web::InputTypes::ALL,
                                    fuchsia::web::AllowInputState::DENY);
  event_filter_.ConfigureInputTypes(
      InputTypes::MOUSE_CLICK | InputTypes::MOUSE_WHEEL,
      fuchsia::web::AllowInputState::ALLOW);

  ui::test::TestEvent event1(ui::EventType::kMousePressed);
  ASSERT_FALSE(event1.stopped_propagation());
  OnEvent(&event1);
  EXPECT_FALSE(event1.stopped_propagation());

  ui::test::TestEvent event2(ui::EventType::kMousewheel);
  ASSERT_FALSE(event2.stopped_propagation());
  OnEvent(&event2);
  EXPECT_FALSE(event2.stopped_propagation());

  // Events not explicitly re-enabled are still denied.
  ui::test::TestEvent dropped_event(ui::EventType::kKeyPressed);
  ASSERT_FALSE(dropped_event.stopped_propagation());
  OnEvent(&dropped_event);
  EXPECT_TRUE(dropped_event.stopped_propagation());
}

TEST_F(EventFilterTest, AllowUnknown) {
  ui::test::TestEvent event(kUserEvent);
  ASSERT_FALSE(event.stopped_propagation());
  OnEvent(&event);
  EXPECT_FALSE(event.stopped_propagation());

  ui::test::TestEvent event2(ui::EventType::kUnknown);
  ASSERT_FALSE(event2.stopped_propagation());
  OnEvent(&event);
  EXPECT_FALSE(event2.stopped_propagation());
}

TEST_F(EventFilterTest, DenyUnknown) {
  event_filter_.ConfigureInputTypes(fuchsia::web::InputTypes::ALL,
                                    fuchsia::web::AllowInputState::DENY);
  ui::test::TestEvent event(kUserEvent);
  ASSERT_FALSE(event.stopped_propagation());
  OnEvent(&event);
  EXPECT_TRUE(event.stopped_propagation());

  ui::test::TestEvent event2(ui::EventType::kUnknown);
  ASSERT_FALSE(event2.stopped_propagation());
  OnEvent(&event2);
  EXPECT_TRUE(event2.stopped_propagation());
}

TEST_F(EventFilterTest, AllowUnknown_AllowAllAfterDenyAll) {
  event_filter_.ConfigureInputTypes(fuchsia::web::InputTypes::ALL,
                                    fuchsia::web::AllowInputState::DENY);
  event_filter_.ConfigureInputTypes(fuchsia::web::InputTypes::ALL,
                                    fuchsia::web::AllowInputState::ALLOW);
  ui::test::TestEvent event(kUserEvent);
  ASSERT_FALSE(event.stopped_propagation());
  OnEvent(&event);
  EXPECT_FALSE(event.stopped_propagation());
}

TEST_F(EventFilterTest, DenyUnknown_AllowSomeAfterDenyAll) {
  event_filter_.ConfigureInputTypes(fuchsia::web::InputTypes::ALL,
                                    fuchsia::web::AllowInputState::DENY);
  event_filter_.ConfigureInputTypes(fuchsia::web::InputTypes::MOUSE_CLICK,
                                    fuchsia::web::AllowInputState::ALLOW);
  ui::test::TestEvent event(kUserEvent);
  ASSERT_FALSE(event.stopped_propagation());
  OnEvent(&event);
  EXPECT_TRUE(event.stopped_propagation());
}

TEST_F(EventFilterTest, LowLevelAndControlAlwaysAllowed) {
  event_filter_.ConfigureInputTypes(fuchsia::web::InputTypes::ALL,
                                    fuchsia::web::AllowInputState::DENY);
  for (ui::EventType type : kAlwaysAllowedEventTypes) {
    ui::test::TestEvent event(type);
    ASSERT_FALSE(event.stopped_propagation());
    OnEvent(&event);
    EXPECT_FALSE(event.stopped_propagation());
  }
}
