// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/web/cpp/fidl.h>

#include "fuchsia/engine/browser/event_filter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"

using fuchsia::web::InputTypes;

struct EventTypeMappingEntry {
  ui::EventType ui_type;
  fuchsia::web::InputTypes fuchsia_type;
};

constexpr EventTypeMappingEntry kEventTypeMappings[] = {
    {ui::ET_MOUSE_PRESSED, InputTypes::MOUSE_CLICK},
    {ui::ET_MOUSE_DRAGGED, InputTypes::MOUSE_CLICK},
    {ui::ET_MOUSE_RELEASED, InputTypes::MOUSE_CLICK},

    {ui::ET_MOUSE_MOVED, InputTypes::MOUSE_MOVE},
    {ui::ET_MOUSE_ENTERED, InputTypes::MOUSE_MOVE},
    {ui::ET_MOUSE_EXITED, InputTypes::MOUSE_MOVE},

    {ui::ET_MOUSEWHEEL, InputTypes::MOUSE_WHEEL},

    {ui::ET_GESTURE_TAP, InputTypes::GESTURE_TAP},
    {ui::ET_GESTURE_TAP_DOWN, InputTypes::GESTURE_TAP},
    {ui::ET_GESTURE_TAP_CANCEL, InputTypes::GESTURE_TAP},
    {ui::ET_GESTURE_TAP_UNCONFIRMED, InputTypes::GESTURE_TAP},
    {ui::ET_GESTURE_DOUBLE_TAP, InputTypes::GESTURE_TAP},
    {ui::ET_GESTURE_TWO_FINGER_TAP, InputTypes::GESTURE_TAP},
    {ui::ET_GESTURE_LONG_PRESS, InputTypes::GESTURE_TAP},
    {ui::ET_GESTURE_LONG_TAP, InputTypes::GESTURE_TAP},

    {ui::ET_GESTURE_PINCH_BEGIN, InputTypes::GESTURE_PINCH},
    {ui::ET_GESTURE_PINCH_END, InputTypes::GESTURE_PINCH},
    {ui::ET_GESTURE_PINCH_UPDATE, InputTypes::GESTURE_PINCH},

    {ui::ET_GESTURE_SCROLL_BEGIN, InputTypes::GESTURE_DRAG},
    {ui::ET_GESTURE_SCROLL_END, InputTypes::GESTURE_DRAG},
    {ui::ET_GESTURE_SCROLL_UPDATE, InputTypes::GESTURE_DRAG},
    {ui::ET_GESTURE_SWIPE, InputTypes::GESTURE_DRAG},
    {ui::ET_SCROLL, InputTypes::GESTURE_DRAG},
    {ui::ET_SCROLL_FLING_START, InputTypes::GESTURE_DRAG},
    {ui::ET_SCROLL_FLING_CANCEL, InputTypes::GESTURE_DRAG},

    {ui::ET_KEY_PRESSED, InputTypes::KEY},
    {ui::ET_KEY_RELEASED, InputTypes::KEY},
};

constexpr ui::EventType kAlwaysAllowedEventTypes[] = {
    ui::ET_TOUCH_RELEASED,    ui::ET_TOUCH_PRESSED,
    ui::ET_TOUCH_MOVED,       ui::ET_TOUCH_CANCELLED,
    ui::ET_DROP_TARGET_EVENT, ui::ET_GESTURE_SHOW_PRESS,
    ui::ET_GESTURE_BEGIN,     ui::ET_GESTURE_END,
    ui::ET_CANCEL_MODE,       ui::ET_MOUSE_CAPTURE_CHANGED,
};

constexpr ui::EventType kUserEvent =
    static_cast<ui::EventType>(ui::ET_LAST + 1);

class TestEvent : public ui::Event {
 public:
  explicit TestEvent(ui::EventType type)
      : ui::Event(type, {} /* time_stamp */, 0 /* flags */) {}
  ~TestEvent() override = default;
};

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
    TestEvent event(entry.ui_type);
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
    TestEvent event(entry.ui_type);
    ASSERT_FALSE(event.stopped_propagation());
    OnEvent(&event);
    EXPECT_FALSE(event.stopped_propagation());
  }
}

TEST_F(EventFilterTest, AllDenied) {
  event_filter_.ConfigureInputTypes(fuchsia::web::InputTypes::ALL,
                                    fuchsia::web::AllowInputState::DENY);
  for (const auto& entry : kEventTypeMappings) {
    TestEvent event(entry.ui_type);
    ASSERT_FALSE(event.stopped_propagation());
    OnEvent(&event);
    EXPECT_TRUE(event.stopped_propagation());
  }
}

TEST_F(EventFilterTest, SelectivelyDenied) {
  for (const auto& entry : kEventTypeMappings) {
    event_filter_.ConfigureInputTypes(entry.fuchsia_type,
                                      fuchsia::web::AllowInputState::DENY);
    TestEvent event(entry.ui_type);
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

  TestEvent event1(ui::ET_MOUSE_PRESSED);
  ASSERT_FALSE(event1.stopped_propagation());
  OnEvent(&event1);
  EXPECT_FALSE(event1.stopped_propagation());

  TestEvent event2(ui::ET_MOUSEWHEEL);
  ASSERT_FALSE(event2.stopped_propagation());
  OnEvent(&event2);
  EXPECT_FALSE(event2.stopped_propagation());

  // Events not explicitly re-enabled are still denied.
  TestEvent dropped_event(ui::ET_KEY_PRESSED);
  ASSERT_FALSE(dropped_event.stopped_propagation());
  OnEvent(&dropped_event);
  EXPECT_TRUE(dropped_event.stopped_propagation());
}

TEST_F(EventFilterTest, AllowUnknown) {
  TestEvent event(kUserEvent);
  ASSERT_FALSE(event.stopped_propagation());
  OnEvent(&event);
  EXPECT_FALSE(event.stopped_propagation());

  TestEvent event2(ui::ET_UNKNOWN);
  ASSERT_FALSE(event2.stopped_propagation());
  OnEvent(&event);
  EXPECT_FALSE(event2.stopped_propagation());
}

TEST_F(EventFilterTest, DenyUnknown) {
  event_filter_.ConfigureInputTypes(fuchsia::web::InputTypes::ALL,
                                    fuchsia::web::AllowInputState::DENY);
  TestEvent event(kUserEvent);
  ASSERT_FALSE(event.stopped_propagation());
  OnEvent(&event);
  EXPECT_TRUE(event.stopped_propagation());

  TestEvent event2(ui::ET_UNKNOWN);
  ASSERT_FALSE(event2.stopped_propagation());
  OnEvent(&event2);
  EXPECT_TRUE(event2.stopped_propagation());
}

TEST_F(EventFilterTest, AllowUnknown_AllowAllAfterDenyAll) {
  event_filter_.ConfigureInputTypes(fuchsia::web::InputTypes::ALL,
                                    fuchsia::web::AllowInputState::DENY);
  event_filter_.ConfigureInputTypes(fuchsia::web::InputTypes::ALL,
                                    fuchsia::web::AllowInputState::ALLOW);
  TestEvent event(kUserEvent);
  ASSERT_FALSE(event.stopped_propagation());
  OnEvent(&event);
  EXPECT_FALSE(event.stopped_propagation());
}

TEST_F(EventFilterTest, DenyUnknown_AllowSomeAfterDenyAll) {
  event_filter_.ConfigureInputTypes(fuchsia::web::InputTypes::ALL,
                                    fuchsia::web::AllowInputState::DENY);
  event_filter_.ConfigureInputTypes(fuchsia::web::InputTypes::MOUSE_CLICK,
                                    fuchsia::web::AllowInputState::ALLOW);
  TestEvent event(kUserEvent);
  ASSERT_FALSE(event.stopped_propagation());
  OnEvent(&event);
  EXPECT_TRUE(event.stopped_propagation());
}

TEST_F(EventFilterTest, LowLevelAndControlAlwaysAllowed) {
  event_filter_.ConfigureInputTypes(fuchsia::web::InputTypes::ALL,
                                    fuchsia::web::AllowInputState::DENY);
  for (ui::EventType type : kAlwaysAllowedEventTypes) {
    TestEvent event(type);
    ASSERT_FALSE(event.stopped_propagation());
    OnEvent(&event);
    EXPECT_FALSE(event.stopped_propagation());
  }
}
