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

TEST(MouseWheelRailsFilterMacTest, Functionality) {
  WebInputEvent::RailsMode mode;
  MouseWheelRailsFilterMac filter;

  // Start with a mostly-horizontal event and see that it is locked
  // horizontally and continues to be locked.
  mode =
      filter.UpdateRailsMode(MakeEvent(WebMouseWheelEvent::kPhaseBegan, 2, 1));
  EXPECT_EQ(mode, WebInputEvent::kRailsModeHorizontal);
  mode = filter.UpdateRailsMode(
      MakeEvent(WebMouseWheelEvent::kPhaseChanged, 2, 2));
  EXPECT_EQ(mode, WebInputEvent::kRailsModeHorizontal);
  mode = filter.UpdateRailsMode(
      MakeEvent(WebMouseWheelEvent::kPhaseChanged, 10, -4));
  EXPECT_EQ(mode, WebInputEvent::kRailsModeHorizontal);

  // Change from horizontal to vertical and back.
  mode =
      filter.UpdateRailsMode(MakeEvent(WebMouseWheelEvent::kPhaseBegan, 4, 1));
  EXPECT_EQ(mode, WebInputEvent::kRailsModeHorizontal);
  mode = filter.UpdateRailsMode(
      MakeEvent(WebMouseWheelEvent::kPhaseChanged, 3, 4));
  EXPECT_EQ(mode, WebInputEvent::kRailsModeHorizontal);
  mode = filter.UpdateRailsMode(
      MakeEvent(WebMouseWheelEvent::kPhaseChanged, 1, 4));
  EXPECT_EQ(mode, WebInputEvent::kRailsModeVertical);
  mode = filter.UpdateRailsMode(
      MakeEvent(WebMouseWheelEvent::kPhaseChanged, 10, 0));
  EXPECT_EQ(mode, WebInputEvent::kRailsModeHorizontal);

  // Make sure zeroes don't break things.
  mode = filter.UpdateRailsMode(
      MakeEvent(WebMouseWheelEvent::kPhaseChanged, 0, 0));
  EXPECT_EQ(mode, WebInputEvent::kRailsModeFree);
  mode =
      filter.UpdateRailsMode(MakeEvent(WebMouseWheelEvent::kPhaseBegan, 0, 0));
  EXPECT_EQ(mode, WebInputEvent::kRailsModeFree);
}

}  // namespace
}  // namespace content
