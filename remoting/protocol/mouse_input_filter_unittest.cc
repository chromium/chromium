// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/mouse_input_filter.h"

#include <array>
#include <utility>

#include "base/containers/span.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/test_event_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

using ::testing::_;
using ::testing::InSequence;

namespace remoting::protocol {

using test::EqualsMouseMoveEvent;

namespace {

using Point = std::pair</* x */ int, /* y */ int>;

static MouseEvent MouseMoveEvent(int x, int y) {
  MouseEvent event;
  event.set_x(x);
  event.set_y(y);
  return event;
}

}  // namespace

class MouseInputFilterTest : public testing::Test {
 public:
  MouseInputFilterTest() = default;

  // Set the size of the client viewing rectangle.
  void SetClientSize(int width, int height) {
    mouse_filter_.set_input_size(width, height);
  }

  // Set the size of the host desktop. For multimon, this is the bounding box
  // that encloses all displays.
  void SetHostDesktop(int width, int height) {
    mouse_filter_.set_output_size(width, height);
  }

  // Set the size and offset of a single display in a multimon setup.
  void SetHostMultimonSingleDisplay(int x, int y, int width, int height) {
    mouse_filter_.set_output_offset(webrtc::DesktopVector(x, y));
    mouse_filter_.set_output_size(width, height);
  }

  void RunMouseTests(base::span<const Point> injected,
                     base::span<const Point> expected,
                     bool swap = false) {
    ASSERT_EQ(injected.size(), expected.size());
    {
      InSequence seq;
      for (const auto& [x, y] : expected) {
        EXPECT_CALL(mock_stub_, InjectMouseEvent(EqualsMouseMoveEvent(x, y)))
            .Times(1);
        if (swap) {
          EXPECT_CALL(mock_stub_, InjectMouseEvent(EqualsMouseMoveEvent(y, x)))
              .Times(1);
        }
      }
    }

    for (const auto& [x, y] : injected) {
      mouse_filter_.InjectMouseEvent(MouseMoveEvent(x, y));
      if (swap) {
        mouse_filter_.InjectMouseEvent(MouseMoveEvent(y, x));
      }
    }
  }

 protected:
  MockInputStub mock_stub_;
  MouseInputFilter mouse_filter_{&mock_stub_};
};

// Verify that no events get through if we don't set either dimensions.
TEST_F(MouseInputFilterTest, NoDimensionsSet) {
  EXPECT_CALL(mock_stub_, InjectMouseEvent(_)).Times(0);
  mouse_filter_.InjectMouseEvent(MouseMoveEvent(10, 10));
}

// Verify that no events get through if there's no input size.
TEST_F(MouseInputFilterTest, InputDimensionsZero) {
  SetHostDesktop(50, 50);

  EXPECT_CALL(mock_stub_, InjectMouseEvent(_)).Times(0);
  mouse_filter_.InjectMouseEvent(MouseMoveEvent(10, 10));
}

// Verify that no events get through if there's no output size.
TEST_F(MouseInputFilterTest, OutputDimensionsZero) {
  SetClientSize(40, 40);

  EXPECT_CALL(mock_stub_, InjectMouseEvent(_)).Times(0);
  mouse_filter_.InjectMouseEvent(MouseMoveEvent(10, 10));
}

// Verify that no events get through when input and output are both set to zero.
TEST_F(MouseInputFilterTest, BothDimensionsZero) {
  SetClientSize(0, 0);
  SetHostDesktop(0, 0);

  EXPECT_CALL(mock_stub_, InjectMouseEvent(_)).Times(0);
  mouse_filter_.InjectMouseEvent(MouseMoveEvent(10, 10));
}

// Verify that no events get through if the input and output are both set to
// one.  This is an edge case as a 1x1 desktop is nonsensical but it's good to
// have a test to exercise the code path in case of errant values being set.
TEST_F(MouseInputFilterTest, BothDimensionsOne) {
  SetClientSize(1, 1);
  SetHostDesktop(1, 1);

  EXPECT_CALL(mock_stub_, InjectMouseEvent(_)).Times(0);
  mouse_filter_.InjectMouseEvent(MouseMoveEvent(10, 10));
}

// Verify that a min-size desktop (2x2) is handled. This is an edge case test,
// not something we'd expect to need to handle in the real world.
TEST_F(MouseInputFilterTest, BothDimensionsTwo) {
  SetClientSize(2, 2);
  SetHostDesktop(2, 2);

  constexpr auto injected = std::to_array<Point>({{1, 1}});
  constexpr auto expected = std::to_array<Point>({{1, 1}});
  RunMouseTests(injected, expected, true);
}

// Verify that no events get through if negative dimensions are provided.
TEST_F(MouseInputFilterTest, NegativeDimensionsHandled) {
  SetClientSize(-42, -42);
  SetHostDesktop(-84, -84);

  EXPECT_CALL(mock_stub_, InjectMouseEvent(_)).Times(0);
  mouse_filter_.InjectMouseEvent(MouseMoveEvent(10, 10));
}

// Verify that all events get through, clamped to the output.
TEST_F(MouseInputFilterTest, NoScalingOrClipping) {
  SetClientSize(40, 40);
  SetHostDesktop(40, 40);

  constexpr auto injected = std::to_array<Point>(
      {{-5, 10}, {0, 10}, {-1, 10}, {15, 40}, {15, 45}, {15, 39}, {15, 25}});
  constexpr auto expected = std::to_array<Point>(
      {{0, 10}, {0, 10}, {0, 10}, {15, 39}, {15, 39}, {15, 39}, {15, 25}});

  RunMouseTests(injected, expected, true);
}

// Verify that we can up-scale with clamping.
TEST_F(MouseInputFilterTest, UpScalingAndClamping) {
  SetClientSize(40, 40);
  SetHostDesktop(80, 80);

  constexpr auto injected = std::to_array<Point>(
      {{-5, 10}, {0, 10}, {-1, 10}, {15, 40}, {15, 45}, {15, 39}, {15, 25}});
  constexpr auto expected = std::to_array<Point>(
      {{0, 20}, {0, 20}, {0, 20}, {30, 79}, {30, 79}, {30, 79}, {30, 51}});

  RunMouseTests(injected, expected, true);
}

// Verify that we can down-scale with clamping.
TEST_F(MouseInputFilterTest, DownScalingAndClamping) {
  SetClientSize(40, 40);
  SetHostDesktop(30, 30);

  constexpr auto injected = std::to_array<Point>(
      {{-5, 10}, {0, 10}, {-1, 10}, {15, 40}, {15, 45}, {15, 39}, {15, 25}});
  constexpr auto expected = std::to_array<Point>(
      {{0, 7}, {0, 7}, {0, 7}, {11, 29}, {11, 29}, {11, 29}, {11, 19}});

  RunMouseTests(injected, expected, true);
}

// Multimon tests

// Default display = Left (A)
// o-------------+-----------------+
// | A           | B               |
// | 2560x1440   | 3840x2160       |
// |             |                 |
// |-------------+                 |
//               +-----------------+
// o = desktop origin

TEST_F(MouseInputFilterTest, MultimonLeftDefault_FullDesktop) {
  SetClientSize(5120, 1728);
  SetHostMultimonSingleDisplay(0, 0, 6400, 2160);

  constexpr auto injected = std::to_array<Point>(
      {{9, 10}, {1559, 372}, {3053, 1662}, {4167, 99}, {5093, 889}});
  constexpr auto expected = std::to_array<Point>(
      {{11, 13}, {1949, 465}, {3816, 2078}, {5209, 124}, {6366, 1111}});

  RunMouseTests(injected, expected);
}

TEST_F(MouseInputFilterTest, MultimonLeftDefault_ShowLeftDisplay) {
  SetClientSize(2048, 1152);
  SetHostMultimonSingleDisplay(0, 0, 2560, 1440);

  constexpr auto injected = std::to_array<Point>({{12, 25}, {2011, 1099}});
  constexpr auto expected = std::to_array<Point>({{15, 31}, {2514, 1374}});
  RunMouseTests(injected, expected);
}

TEST_F(MouseInputFilterTest, MultimonLeftDefault_ShowRightDisplay) {
  SetClientSize(3072, 1728);
  SetHostMultimonSingleDisplay(2560, 0, 3840, 2160);

  constexpr auto injected = std::to_array<Point>({{175, 165}, {2948, 1532}});
  constexpr auto expected = std::to_array<Point>({{2779, 206}, {6245, 1915}});
  RunMouseTests(injected, expected);
}

// Default display = Right (A)
// +-----------------o-------------+
// | B               | A           |
// | 3840x2160       | 2560x1440   |
// |                 |             |
// |                 |-------------+
// +-----------------+
// o = desktop origin

TEST_F(MouseInputFilterTest, MultimonRightDefault_ShowLeftDisplay) {
  SetClientSize(3072, 1728);
  SetHostMultimonSingleDisplay(0, 0, 3840, 2160);

  constexpr auto injected = std::to_array<Point>({{64, 61}, {3029, 1649}});
  constexpr auto expected = std::to_array<Point>({{80, 76}, {3786, 2061}});
  RunMouseTests(injected, expected);
}

TEST_F(MouseInputFilterTest, MultimonRightDefault_ShowRightDisplay) {
  SetClientSize(2048, 1152);
  SetHostMultimonSingleDisplay(3840, 0, 2560, 1440);

  constexpr auto injected = std::to_array<Point>({{19, 20}, {2014, 1095}});
  constexpr auto expected = std::to_array<Point>({{3864, 25}, {6358, 1369}});
  RunMouseTests(injected, expected);
}

}  // namespace remoting::protocol
