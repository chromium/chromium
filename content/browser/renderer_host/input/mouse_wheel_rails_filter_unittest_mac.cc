// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/mouse_wheel_rails_filter_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::WebInputEvent;
using blink::WebMouseWheelEvent;

namespace content {
namespace {

WebMouseWheelEvent MakeEvent(WebMouseWheelEvent::Phase phase,
                             float delta_x,
                             float delta_y) {
  WebMouseWheelEvent event;
  event.phase = phase;
  event.delta_x = delta_x;
  event.delta_y = delta_y;
  return event;
}

TEST(MouseWheelRailsFilterMacTest, StronglyHorizontalStaysLocked) {
  WebInputEvent::RailsMode mode;
  MouseWheelRailsFilterMac filter;

  // A strongly horizontal gesture (ratio >= 2x) should stay locked.
  mode =
      filter.UpdateRailsMode(MakeEvent(WebMouseWheelEvent::kPhaseBegan, 4, 1));
  EXPECT_EQ(mode, WebInputEvent::RailsMode::kRailsModeHorizontal);
  mode = filter.UpdateRailsMode(
      MakeEvent(WebMouseWheelEvent::kPhaseChanged, 10, -4));
  EXPECT_EQ(mode, WebInputEvent::RailsMode::kRailsModeHorizontal);
}

TEST(MouseWheelRailsFilterMacTest, StronglyVerticalStaysLocked) {
  WebInputEvent::RailsMode mode;
  MouseWheelRailsFilterMac filter;

  mode =
      filter.UpdateRailsMode(MakeEvent(WebMouseWheelEvent::kPhaseBegan, 1, 5));
  EXPECT_EQ(mode, WebInputEvent::RailsMode::kRailsModeVertical);
  mode = filter.UpdateRailsMode(
      MakeEvent(WebMouseWheelEvent::kPhaseChanged, 0, 3));
  EXPECT_EQ(mode, WebInputEvent::RailsMode::kRailsModeVertical);
}

TEST(MouseWheelRailsFilterMacTest, DiagonalScrollIsFree) {
  WebInputEvent::RailsMode mode;
  MouseWheelRailsFilterMac filter;

  // Equal deltas on both axes. Clearly diagonal, should be free.
  mode =
      filter.UpdateRailsMode(MakeEvent(WebMouseWheelEvent::kPhaseBegan, 3, 3));
  EXPECT_EQ(mode, WebInputEvent::RailsMode::kRailsModeFree);
  mode = filter.UpdateRailsMode(
      MakeEvent(WebMouseWheelEvent::kPhaseChanged, 4, 3));
  EXPECT_EQ(mode, WebInputEvent::RailsMode::kRailsModeFree);
}

TEST(MouseWheelRailsFilterMacTest, DiagonalThenVerticalReRails) {
  WebInputEvent::RailsMode mode;
  MouseWheelRailsFilterMac filter;

  mode =
      filter.UpdateRailsMode(MakeEvent(WebMouseWheelEvent::kPhaseBegan, 3, 3));
  EXPECT_EQ(mode, WebInputEvent::RailsMode::kRailsModeFree);
  mode = filter.UpdateRailsMode(
      MakeEvent(WebMouseWheelEvent::kPhaseChanged, 3, 3));
  EXPECT_EQ(mode, WebInputEvent::RailsMode::kRailsModeFree);

  mode = filter.UpdateRailsMode(
      MakeEvent(WebMouseWheelEvent::kPhaseChanged, 0, 10));
  EXPECT_EQ(mode, WebInputEvent::RailsMode::kRailsModeVertical);
}

TEST(MouseWheelRailsFilterMacTest, NewGestureResetsState) {
  WebInputEvent::RailsMode mode;
  MouseWheelRailsFilterMac filter;

  mode =
      filter.UpdateRailsMode(MakeEvent(WebMouseWheelEvent::kPhaseBegan, 5, 1));
  EXPECT_EQ(mode, WebInputEvent::RailsMode::kRailsModeHorizontal);

  mode =
      filter.UpdateRailsMode(MakeEvent(WebMouseWheelEvent::kPhaseBegan, 1, 5));
  EXPECT_EQ(mode, WebInputEvent::RailsMode::kRailsModeVertical);
}

TEST(MouseWheelRailsFilterMacTest, SingleAxisWithZeroOtherAxisRails) {
  WebInputEvent::RailsMode mode;
  MouseWheelRailsFilterMac filter;

  // Pure horizontal (zero Y) should lock horizontally.
  mode =
      filter.UpdateRailsMode(MakeEvent(WebMouseWheelEvent::kPhaseBegan, 5, 0));
  EXPECT_EQ(mode, WebInputEvent::RailsMode::kRailsModeHorizontal);

  // Pure vertical (zero X) should lock vertically.
  mode =
      filter.UpdateRailsMode(MakeEvent(WebMouseWheelEvent::kPhaseBegan, 0, 5));
  EXPECT_EQ(mode, WebInputEvent::RailsMode::kRailsModeVertical);
}

TEST(MouseWheelRailsFilterMacTest, ZeroDeltasReturnFree) {
  WebInputEvent::RailsMode mode;
  MouseWheelRailsFilterMac filter;

  mode = filter.UpdateRailsMode(
      MakeEvent(WebMouseWheelEvent::kPhaseChanged, 0, 0));
  EXPECT_EQ(mode, WebInputEvent::RailsMode::kRailsModeFree);
  mode =
      filter.UpdateRailsMode(MakeEvent(WebMouseWheelEvent::kPhaseBegan, 0, 0));
  EXPECT_EQ(mode, WebInputEvent::RailsMode::kRailsModeFree);
}

}  // namespace
}  // namespace content
