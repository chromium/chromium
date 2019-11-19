// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/mouse_input_filter.h"

#include "base/stl_util.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/test_event_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

using ::testing::_;
using ::testing::InSequence;

namespace remoting {
namespace protocol {

using test::EqualsMouseMoveEvent;

static MouseEvent MouseMoveEvent(int x, int y) {
  MouseEvent event;
  event.set_x(x);
  event.set_y(y);
  return event;
}

struct Point {
  int x;
  int y;
};

class MouseInputFilterTest : public testing::Test {
 public:
  MouseInputFilterTest() : mouse_filter_(&mock_stub_) {}

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

  void InjectMouse(const Point& point, bool swap = false) {
    mouse_filter_.InjectMouseEvent(MouseMoveEvent(point.x, point.y));
    if (swap)
      mouse_filter_.InjectMouseEvent(MouseMoveEvent(point.y, point.x));
  }

  void ExpectNoMouse() {
    EXPECT_CALL(mock_stub_, InjectMouseEvent(_)).Times(0);
  }

  void ExpectMouse(const Point& point, bool swap = false) {
    EXPECT_CALL(mock_stub_,
                InjectMouseEvent(EqualsMouseMoveEvent(point.x, point.y)))
        .Times(1);
    if (swap) {
      EXPECT_CALL(mock_stub_,
                  InjectMouseEvent(EqualsMouseMoveEvent(point.y, point.x)))
          .Times(1);
    }
  }

  void RunMouseTests(unsigned int len,
                     const Point* injected,
                     const Point* expected,
                     bool swap = false) {
    {
      InSequence s;
      for (unsigned int i = 0; i < len; ++i)
        ExpectMouse(expected[i], swap);
    }

    for (unsigned int i = 0; i < len; ++i)
      InjectMouse(injected[i], swap);
  }

 protected:
  MockInputStub mock_stub_;
  MouseInputFilter mouse_filter_;
};

// Verify that no events get through if we don't set either dimensions.
TEST_F(MouseInputFilterTest, BothDimensionsZero) {
  ExpectNoMouse();
  InjectMouse({10, 10});
}

// Verify that no events get through if there's no input size.
TEST_F(MouseInputFilterTest, InputDimensionsZero) {
  SetHostDesktop(50, 50);

  ExpectNoMouse();
  InjectMouse({10, 10});
}

// Verify that no events get through if there's no output size.
TEST_F(MouseInputFilterTest, OutputDimensionsZero) {
  SetClientSize(40, 40);

  ExpectNoMouse();
  InjectMouse({10, 10});
}

// Verify that all events get through, clamped to the output.
TEST_F(MouseInputFilterTest, NoScalingOrClipping) {
  SetClientSize(40, 40);
  SetHostDesktop(40, 40);

  const Point injected[] = {{-5, 10}, {0, 10},  {-1, 10}, {15, 40},
                            {15, 45}, {15, 39}, {15, 25}};
  const Point expected[] = {{0, 10},  {0, 10},  {0, 10}, {15, 39},
                            {15, 39}, {15, 39}, {15, 25}};

  RunMouseTests(base::size(expected), injected, expected, true);
}

// Verify that we can up-scale with clamping.
TEST_F(MouseInputFilterTest, UpScalingAndClamping) {
  SetClientSize(40, 40);
  SetHostDesktop(80, 80);

  const Point injected[] = {{-5, 10}, {0, 10},  {-1, 10}, {15, 40},
                            {15, 45}, {15, 39}, {15, 25}};
  const Point expected[] = {{0, 20},  {0, 20},  {0, 20}, {30, 79},
                            {30, 79}, {30, 79}, {30, 51}};

  RunMouseTests(base::size(expected), injected, expected, true);
}

// Verify that we can down-scale with clamping.
TEST_F(MouseInputFilterTest, DownScalingAndClamping) {
  SetClientSize(40, 40);
  SetHostDesktop(30, 30);

  const Point injected[] = {{-5, 10}, {0, 10},  {-1, 10}, {15, 40},
                            {15, 45}, {15, 39}, {15, 25}};
  const Point expected[] = {{0, 7},   {0, 7},   {0, 7},  {11, 29},
                            {11, 29}, {11, 29}, {11, 19}};

  RunMouseTests(base::size(expected), injected, expected, true);
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

  const Point injected[] = {
      {9, 10}, {1559, 372}, {3053, 1662}, {4167, 99}, {5093, 889}};
  const Point expected[] = {
      {11, 13}, {1949, 465}, {3816, 2078}, {5209, 124}, {6366, 1111}};

  RunMouseTests(base::size(expected), injected, expected);
}

TEST_F(MouseInputFilterTest, MultimonLeftDefault_ShowLeftDisplay) {
  SetClientSize(2048, 1152);
  SetHostMultimonSingleDisplay(0, 0, 2560, 1440);

  const Point injected[] = {{12, 25}, {2011, 1099}};
  const Point expected[] = {{15, 31}, {2514, 1374}};

  RunMouseTests(base::size(expected), injected, expected);
}

TEST_F(MouseInputFilterTest, MultimonLeftDefault_ShowRightDisplay) {
  SetClientSize(3072, 1728);
  SetHostMultimonSingleDisplay(2560, 0, 3840, 2160);

  const Point injected[] = {{175, 165}, {2948, 1532}};
  const Point expected[] = {{2779, 206}, {6245, 1915}};

  RunMouseTests(base::size(expected), injected, expected);
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

  const Point injected[] = {{64, 61}, {3029, 1649}};
  const Point expected[] = {{80, 76}, {3786, 2061}};

  RunMouseTests(base::size(expected), injected, expected);
}

TEST_F(MouseInputFilterTest, MultimonRightDefault_ShowRightDisplay) {
  SetClientSize(2048, 1152);
  SetHostMultimonSingleDisplay(3840, 0, 2560, 1440);

  const Point injected[] = {{19, 20}, {2014, 1095}};
  const Point expected[] = {{3864, 25}, {6358, 1369}};

  RunMouseTests(base::size(expected), injected, expected);
}

}  // namespace protocol
}  // namespace remoting
