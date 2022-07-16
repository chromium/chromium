// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/mouse_event.h"

#include <limits>
#include <tuple>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mouse_event_init.h"
#include "third_party/blink/renderer/platform/geometry/double_point.h"
#include "ui/gfx/geometry/point.h"

namespace blink {

class MouseEventScreenClientPagePositionTest
    : public ::testing::TestWithParam<std::tuple<double, double>> {};
class MouseEventLayerPositionTest
    : public ::testing::TestWithParam<std::tuple<double, double>> {};

TEST_P(MouseEventScreenClientPagePositionTest, PositionAsExpected) {
  MouseEvent& mouse_event = *MouseEvent::Create();
  DoublePoint input_location(std::get<0>(GetParam()), std::get<0>(GetParam()));
  DoublePoint expected_location(std::get<1>(GetParam()),
                                std::get<1>(GetParam()));

  mouse_event.screen_location_ = input_location;
  mouse_event.client_location_ = input_location;
  mouse_event.page_location_ = input_location;

  ASSERT_EQ(mouse_event.clientX(), expected_location.X());
  ASSERT_EQ(mouse_event.clientY(), expected_location.Y());
  ASSERT_EQ(mouse_event.screenX(), expected_location.X());
  ASSERT_EQ(mouse_event.screenY(), expected_location.Y());
  ASSERT_EQ(mouse_event.pageX(), expected_location.X());
  ASSERT_EQ(mouse_event.pageY(), expected_location.Y());
}

INSTANTIATE_TEST_SUITE_P(
    MouseEventScreenClientPagePositionNoOverflow,
    MouseEventScreenClientPagePositionTest,
    ::testing::Values(
        std::make_tuple(std::numeric_limits<int>::min() * 1.0,
                        std::numeric_limits<int>::min() * 1.0),
        std::make_tuple(std::numeric_limits<int>::min() * 1.0 - 1.55,
                        std::numeric_limits<int>::min() * 1.0 - 2.0),
        std::make_tuple(std::numeric_limits<int>::max() * 1.0,
                        std::numeric_limits<int>::max() * 1.0),
        std::make_tuple(std::numeric_limits<int>::max() * 1.0 + 1.55,
                        std::numeric_limits<int>::max() * 1.0 + 1.00),
        std::make_tuple(std::numeric_limits<double>::lowest(),
                        std::ceil(std::numeric_limits<double>::lowest())),
        std::make_tuple(std::numeric_limits<double>::lowest() + 1.45,
                        std::ceil(std::numeric_limits<double>::lowest() +
                                  1.45)),
        std::make_tuple(std::numeric_limits<double>::max(),
                        std::floor(std::numeric_limits<double>::max())),
        std::make_tuple(std::numeric_limits<double>::max() - 1.45,
                        std::floor(std::numeric_limits<double>::max() -
                                   1.45))));

TEST_P(MouseEventLayerPositionTest, LayerPositionAsExpected) {
  DoublePoint input_layer_location(std::get<0>(GetParam()),
                                   std::get<0>(GetParam()));
  gfx::Point expected_layer_location(std::get<1>(GetParam()),
                                     std::get<1>(GetParam()));

  MouseEventInit& mouse_event_init = *MouseEventInit::Create();
  mouse_event_init.setClientX(input_layer_location.X());
  mouse_event_init.setClientY(input_layer_location.Y());
  MouseEvent mouse_event("mousedown", &mouse_event_init);

  ASSERT_EQ(mouse_event.layerX(), expected_layer_location.x());
  ASSERT_EQ(mouse_event.layerY(), expected_layer_location.y());
}

INSTANTIATE_TEST_SUITE_P(
    MouseEventLayerPositionNoOverflow,
    MouseEventLayerPositionTest,
    ::testing::Values(
        std::make_tuple(std::numeric_limits<int>::min() * 1.0,
                        std::numeric_limits<int>::min()),
        std::make_tuple(std::numeric_limits<int>::min() * 1.0 - 1.45,
                        std::numeric_limits<int>::min()),
        std::make_tuple(std::numeric_limits<int>::max() * 1.0,
                        std::numeric_limits<int>::max()),
        std::make_tuple(std::numeric_limits<int>::max() * 1.0 + 1.45,
                        std::numeric_limits<int>::max()),
        std::make_tuple(std::numeric_limits<double>::lowest(),
                        std::numeric_limits<int>::min()),
        std::make_tuple(std::numeric_limits<double>::lowest() + 1.45,
                        std::numeric_limits<int>::min()),
        std::make_tuple(std::numeric_limits<double>::max(),
                        std::numeric_limits<int>::max()),
        std::make_tuple(std::numeric_limits<double>::max() - 1.45,
                        std::numeric_limits<int>::max())));
}  // namespace blink
