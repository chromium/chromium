// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/ui/desktop_viewport.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

const float EPSILON = 0.001f;

}  // namespace

class DesktopViewportTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

 protected:
  void AssertTransformationReceived(const base::Location& from_here,
                                    float scale,
                                    float offset_x,
                                    float offset_y);

  ViewMatrix ReleaseReceivedTransformation();

  DesktopViewport viewport_;

 private:
  void OnTransformationChanged(const ViewMatrix& matrix);

  ViewMatrix received_transformation_;
};

void DesktopViewportTest::SetUp() {
  viewport_.RegisterOnTransformationChangedCallback(
      base::BindRepeating(&DesktopViewportTest::OnTransformationChanged,
                          base::Unretained(this)),
      true);
}

void DesktopViewportTest::TearDown() {
  ASSERT_TRUE(received_transformation_.IsEmpty());
}

void DesktopViewportTest::AssertTransformationReceived(
    const base::Location& from_here,
    float scale,
    float offset_x,
    float offset_y) {
  ASSERT_FALSE(received_transformation_.IsEmpty())
      << "Matrix has not been received yet."
      << "Location: " << from_here.ToString();
  ViewMatrix expected(scale, {offset_x, offset_y});
  std::array<float, 9> expected_array = expected.ToMatrixArray();
  std::array<float, 9> actual_array = received_transformation_.ToMatrixArray();

  for (int i = 0; i < 9; i++) {
    float diff = expected_array[i] - actual_array[i];
    ASSERT_TRUE(diff > -EPSILON && diff < EPSILON)
        << "Matrix doesn't match. \n"
        << base::StringPrintf("Expected scale: %f, offset: (%f, %f)\n",
                              expected_array[0], expected_array[2],
                              expected_array[5])
        << base::StringPrintf("Actual scale: %f, offset: (%f, %f)\n",
                              actual_array[0], actual_array[2], actual_array[5])
        << "Location: " << from_here.ToString();
  }

  received_transformation_ = ViewMatrix();
}

ViewMatrix DesktopViewportTest::ReleaseReceivedTransformation() {
  EXPECT_FALSE(received_transformation_.IsEmpty());
  ViewMatrix out = received_transformation_;
  received_transformation_ = ViewMatrix();
  return out;
}

void DesktopViewportTest::OnTransformationChanged(const ViewMatrix& matrix) {
  ASSERT_TRUE(received_transformation_.IsEmpty())
      << "Previous matrix has not been asserted.";
  received_transformation_ = matrix;
}

TEST_F(DesktopViewportTest, TestViewportInitialization1) {
  // VP < DP. Desktop shrinks to fit.
  // +====+------+
  // | VP | DP   |
  // |    |      |
  // +====+------+
  viewport_.SetDesktopSize(8, 6);
  viewport_.SetSurfaceSize(2, 3);
  AssertTransformationReceived(FROM_HERE, 0.5f, 0.f, 0.f);
}

TEST_F(DesktopViewportTest, TestViewportInitialization2) {
  // VP < DP. Desktop shrinks to fit.
  // +-----------------+
  // |       DP        |
  // |                 |
  // +=================+
  // |       VP        |
  // +=================+
  viewport_.SetDesktopSize(8, 6);
  viewport_.SetSurfaceSize(3, 2);
  AssertTransformationReceived(FROM_HERE, 0.375, 0.f, 0.f);
}

TEST_F(DesktopViewportTest, TestViewportInitialization3) {
  // VP < DP. Desktop shrinks to fit.
  // +========+----+
  // |  VP    | DP |
  // +========+----+
  viewport_.SetDesktopSize(9, 3);
  viewport_.SetSurfaceSize(2, 1);
  AssertTransformationReceived(FROM_HERE, 1 / 3.f, 0.f, 0.f);
}

TEST_F(DesktopViewportTest, TestViewportInitialization4) {
  // VP > DP. Desktop grows to fit.
  // +====+------+
  // | VP | DP   |
  // |    |      |
  // +====+------+
  viewport_.SetDesktopSize(2, 1);
  viewport_.SetSurfaceSize(3, 4);
  AssertTransformationReceived(FROM_HERE, 4.f, 0.f, 0.f);
}

TEST_F(DesktopViewportTest, TestMoveDesktop) {
  // +====+------+
  // | VP | DP   |
  // |    |      |
  // +====+------+
  viewport_.SetDesktopSize(8, 6);
  viewport_.SetSurfaceSize(2, 3);
  AssertTransformationReceived(FROM_HERE, 0.5f, 0.f, 0.f);

  // <--- DP
  // +------+====+
  // | DP   | VP |
  // |      |    |
  // +------+====+
  viewport_.MoveDesktop(-2.f, 0.f);
  AssertTransformationReceived(FROM_HERE, 0.5f, -2.f, 0.f);

  //      +====+
  // +----| VP |
  // | DP |    |
  // |    +====+
  // +--------+
  // Bounces back.
  viewport_.MoveDesktop(-1.f, 1.f);
  AssertTransformationReceived(FROM_HERE, 0.5f, -2.f, 0.f);
}

TEST_F(DesktopViewportTest, TestMoveAndScaleDesktop) {
  // Number in surface coordinate.
  //
  // +====+------+
  // | VP | DP   |
  // |    |      | 3
  // +====+------+
  //        4
  viewport_.SetDesktopSize(8, 6);
  viewport_.SetSurfaceSize(2, 3);
  AssertTransformationReceived(FROM_HERE, 0.5f, 0.f, 0.f);

  // Scale at pivot point (2, 3) by 1.5x.
  // +------------------+
  // |                  |
  // |   +====+   DP    | 4.5
  // |   | VP |         |
  // |   |    |         |
  // +---+====+---------+
  //       2     6
  viewport_.ScaleDesktop(2.f, 3.f, 1.5f);
  AssertTransformationReceived(FROM_HERE, 0.75f, -1.f, -1.5f);

  // Move VP to the top-right.
  // +-------------+====+
  // |             | VP |
  // |     DP      |    |
  // |             +====+ 4.5
  // |               2  |
  // +------------------+
  //         6
  viewport_.MoveDesktop(-10000.f, 10000.f);
  AssertTransformationReceived(FROM_HERE, 0.75f, -4.f, 0.f);

  // Scale at (2, 0) by 0.5x.
  //      VP
  //       +====+
  //    +--+----+
  // DP |  |    |
  //    +--+----+
  //       +====+
  viewport_.ScaleDesktop(2.f, 0.f, 0.5f);
  AssertTransformationReceived(FROM_HERE, 0.375, -1.f, 0.375f);

  // Scale all the way down.
  // +========+
  // |   VP   |
  // +--------+
  // |   DP   |
  // +--------+
  // |        |
  // +========+
  viewport_.ScaleDesktop(20.f, 0.f, 0.0001f);
  AssertTransformationReceived(FROM_HERE, 0.25f, 0.f, 0.75f);
}

TEST_F(DesktopViewportTest, TestSetViewportCenter) {
  // Numbers in desktop coordinates.
  //
  // +====+------+
  // | VP | DP   |
  // |    |      | 6
  // +====+------+
  //        8
  viewport_.SetDesktopSize(8, 6);
  viewport_.SetSurfaceSize(2, 3);
  AssertTransformationReceived(FROM_HERE, 0.5f, 0.f, 0.f);

  //  1.6
  // +==+--------+
  // |VP|2.4  DP |
  // +==+        | 6
  // +-----------+
  //       8
  viewport_.ScaleDesktop(0.f, 0.f, 2.5f);
  AssertTransformationReceived(FROM_HERE, 1.25f, 0.f, 0.f);

  // Move VP to center of the desktop.
  // +------------------+
  // |      +1.6=+      |
  // |      | VP |2.4   | 6
  // |      +====+      |
  // +------------------+
  //          8
  viewport_.SetViewportCenter(4.f, 3.f);
  AssertTransformationReceived(FROM_HERE, 1.25f, -4.f, -2.25f);

  // Move it out of bound and bounce it back.
  // +------------------+
  // |                  |
  // |     DP           |
  // |               +====+
  // |               | VP |
  // +---------------|    |
  //                 +====+
  viewport_.SetViewportCenter(1000.f, 1000.f);
  AssertTransformationReceived(FROM_HERE, 1.25f, -8.f, -4.5f);
}

TEST_F(DesktopViewportTest, TestScaleDesktop) {
  // Number in surface coordinate.
  //
  // +====+------+
  // | VP | DP   |
  // |    |      | 3
  // +====+------+
  //        4
  viewport_.SetDesktopSize(8, 6);
  viewport_.SetSurfaceSize(2, 3);
  AssertTransformationReceived(FROM_HERE, 0.5f, 0.f, 0.f);

  ViewMatrix old_transformation(0.5f, {0.f, 0.f});

  ViewMatrix::Point surface_point = old_transformation.MapPoint({1.2f, 1.3f});

  // Scale a little bit at a pivot point.
  viewport_.ScaleDesktop(surface_point.x, surface_point.y, 1.1f);

  ViewMatrix new_transformation = ReleaseReceivedTransformation();

  // Verify the pivot point is fixed.
  ViewMatrix::Point new_surface_point =
      new_transformation.MapPoint({1.2f, 1.3f});
  ASSERT_FLOAT_EQ(surface_point.x, new_surface_point.x);
  ASSERT_FLOAT_EQ(surface_point.y, new_surface_point.y);

  // Verify the scale is correct.
  ASSERT_FLOAT_EQ(old_transformation.GetScale() * 1.1f,
                  new_transformation.GetScale());
}

TEST_F(DesktopViewportTest, AsymmetricalSafeInsetsPanAndZoom) {
  // Initialize with 6x5 desktop and 6x5 screen with this safe inset:
  // left: 2, top: 2, right: 1, bottom: 1
  viewport_.SetDesktopSize(6, 5);
  viewport_.SetSafeInsets(2, 2, 1, 1);
  viewport_.SetSurfaceSize(6, 5);

  // Viewport is initialized to fit the inset area instead of the whole surface
  // area.
  AssertTransformationReceived(FROM_HERE, 0.5, 2, 2);

  // Move the viewport all the way to the bottom right.
  viewport_.MoveViewport(100, 100);

  // The bottom right of the desktop is stuck with the bottom right of the
  // safe area.
  AssertTransformationReceived(FROM_HERE, 0.5, 2, 1.5);

  // Zoom the viewport on the bottom right of the safe area to match the
  // resolution of the surface.
  viewport_.ScaleDesktop(5, 4, 2);
  AssertTransformationReceived(FROM_HERE, 1, -1, -1);

  // Move the desktop by <1, 1>. Now it perfectly fits the surface.
  viewport_.MoveDesktop(1, 1);
  AssertTransformationReceived(FROM_HERE, 1, 0, 0);

  // Move the desktop all the way to the top left. Now it stucks with the top
  // left corner of the safe area.
  viewport_.MoveDesktop(100, 100);
  AssertTransformationReceived(FROM_HERE, 1, 2, 2);
}

TEST_F(DesktopViewportTest, SingleNotchSafeInsetPanAndZoom) {
  // Initialize with 6x5 desktop and 6x5 screen with this safe inset:
  // left: 1, right: 1, top: 0, bottom: 0
  viewport_.SetDesktopSize(6, 5);
  viewport_.SetSafeInsets(1, 1, 0, 0);
  viewport_.SetSurfaceSize(6, 5);
  AssertTransformationReceived(FROM_HERE, 5 / 6.f, 1, 1);

  viewport_.MoveViewport(100, 100);
  AssertTransformationReceived(FROM_HERE, 5 / 6.f, 1, 5 / 6.f);

  viewport_.ScaleDesktop(6, 5, 1.2);
  AssertTransformationReceived(FROM_HERE, 1, 0, 0);
}

TEST_F(DesktopViewportTest, SymmetricSafeInsetPanAndZoom) {
  // Initialize with 6x5 desktop and 6x5 screen with this safe inset:
  // left: 1, right: 1, top: 1, bottom: 1
  viewport_.SetDesktopSize(6, 5);
  viewport_.SetSafeInsets(1, 1, 1, 1);
  viewport_.SetSurfaceSize(6, 5);
  AssertTransformationReceived(FROM_HERE, 2 / 3.f, 1, 1);

  viewport_.MoveViewport(100, 100);
  AssertTransformationReceived(FROM_HERE, 2 / 3.f, 1, 2 / 3.f);

  viewport_.ScaleDesktop(5, 4, 1.5);
  AssertTransformationReceived(FROM_HERE, 1, -1, -1);

  viewport_.MoveDesktop(1, 1);
  AssertTransformationReceived(FROM_HERE, 1, 0, 0);
}

TEST_F(DesktopViewportTest, RemoveSafeInsets) {
  // Initialize with 6x5 desktop and 6x5 screen with this safe inset:
  // left: 2, top: 2, right: 1, bottom: 1
  viewport_.SetDesktopSize(6, 5);
  viewport_.SetSafeInsets(2, 2, 1, 1);
  viewport_.SetSurfaceSize(6, 5);

  AssertTransformationReceived(FROM_HERE, 0.5, 2, 2);

  // Move the viewport all the way to the bottom right.
  viewport_.MoveViewport(100, 100);
  AssertTransformationReceived(FROM_HERE, 0.5, 2, 1.5);

  // Now remove the safe insets.
  viewport_.SetSafeInsets(0, 0, 0, 0);

  // Desktop is now stretched to fit the whole surface.
  AssertTransformationReceived(FROM_HERE, 1, 0, 0);
}

TEST_F(DesktopViewportTest, AddAndRemoveSafeInsets) {
  // This test case tests showing and hiding soft keyboard.

  // Initialize with 12x9 desktop and screen with this safe inset:
  // left: 2, top: 2, right: 1, bottom: 1
  viewport_.SetDesktopSize(12, 9);
  viewport_.SetSafeInsets(2, 2, 1, 1);
  viewport_.SetSurfaceSize(12, 9);
  AssertTransformationReceived(FROM_HERE, 0.75, 2, 2);

  // Increase the bottom insets to simulate keyboard popup.
  viewport_.SetSafeInsets(2, 2, 1, 4);
  AssertTransformationReceived(FROM_HERE, 0.75, 2, 2);

  // Move the viewport all the way down. (=moving the desktop all the way up.)
  viewport_.MoveViewport(100, 100);
  AssertTransformationReceived(FROM_HERE, 0.75, 2, -1.75);

  // Now remove the extra insets.
  viewport_.SetSafeInsets(2, 2, 1, 1);

  // Viewport should bounce back.
  AssertTransformationReceived(FROM_HERE, 0.75, 2, 1.25);
}

}  // namespace remoting
