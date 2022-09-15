// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/empty_cursor_filter.h"

#include <ostream>

#include "remoting/proto/control.pb.h"
#include "remoting/protocol/cursor_shape_stub.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

// Chromoting cursors are always RGBA.
const int kBytesPerPixel = 4;

const int kTestCursorWidth = 100;
const int kTestCursorHeight = 64;
const int kTestCursorHotspotX = 10;
const int kTestCursorHotspotY = 30;
const int kTestCursorDataSize =
    kTestCursorWidth * kTestCursorHeight * kBytesPerPixel;

protocol::CursorShapeInfo CreateTransparentCursorShape() {
  protocol::CursorShapeInfo transparent_cursor;
  transparent_cursor.set_width(kTestCursorWidth);
  transparent_cursor.set_height(kTestCursorHeight);
  transparent_cursor.set_hotspot_x(kTestCursorHotspotX);
  transparent_cursor.set_hotspot_y(kTestCursorHotspotY);
  transparent_cursor.mutable_data()->resize(kTestCursorDataSize, 0);
  return transparent_cursor;
}

protocol::CursorShapeInfo CreateOpaqueCursorShape() {
  protocol::CursorShapeInfo cursor = CreateTransparentCursorShape();
  cursor.mutable_data()->assign(kTestCursorDataSize, 0x01);
  return cursor;
}

MATCHER_P(EqualsCursorShape, cursor_shape, "") {
  // TODO(wez): Should not assume that all fields were set.
  return arg.data() == cursor_shape.data() &&
         arg.width() == cursor_shape.width() &&
         arg.height() == cursor_shape.height() &&
         arg.hotspot_x() == cursor_shape.hotspot_x() &&
         arg.hotspot_y() == cursor_shape.hotspot_y();
}

}  // namespace

namespace protocol {

// This pretty-printer must be defined under remoting::protocol to be used.
::std::ostream& operator<<(::std::ostream& os, const CursorShapeInfo& cursor) {
  return os << "[w:" << cursor.width() << ", h:" << cursor.height()
            << ", h.x:" << cursor.hotspot_x() << ", h.y:" << cursor.hotspot_y()
            << ", data.size:" << cursor.data().size() << "]";
}

} // namespace protocol

// Verify that EmptyCursorShape() generates a normalized empty cursor.
TEST(EmptyCursorFilterTest, EmptyCursorShape) {
  const protocol::CursorShapeInfo& empty_cursor = EmptyCursorShape();

  // TODO(wez): Replace these individual asserts with IsCursorShapeValid()?
  ASSERT_TRUE(empty_cursor.has_data());
  ASSERT_TRUE(empty_cursor.has_width());
  ASSERT_TRUE(empty_cursor.has_height());
  ASSERT_TRUE(empty_cursor.has_hotspot_x());
  ASSERT_TRUE(empty_cursor.has_hotspot_y());

  EXPECT_EQ(0, empty_cursor.width());
  EXPECT_EQ(0, empty_cursor.height());
  EXPECT_EQ(0, empty_cursor.hotspot_x());
  EXPECT_EQ(0, empty_cursor.hotspot_y());
  EXPECT_TRUE(empty_cursor.data().empty());
}

// Verify that IsCursorShapeEmpty returns true only for normalized empty
// cursors, not for opaque or transparent non-empty cursors.
TEST(EmptyCursorFilterTest, IsCursorShapeEmpty) {
  const protocol::CursorShapeInfo& kEmptyCursor = EmptyCursorShape();
  EXPECT_TRUE(IsCursorShapeEmpty(kEmptyCursor));

  const protocol::CursorShapeInfo& kOpaqueCursor = CreateOpaqueCursorShape();
  EXPECT_FALSE(IsCursorShapeEmpty(kOpaqueCursor));

  const protocol::CursorShapeInfo& kTransparentCursor =
      CreateTransparentCursorShape();
  EXPECT_FALSE(IsCursorShapeEmpty(kTransparentCursor));
}

// Verify that EmptyCursorFilter behaves correctly for normalized empty cursors.
TEST(EmptyCursorFilterTest, EmptyCursor) {
  const protocol::CursorShapeInfo& kEmptyCursor = EmptyCursorShape();
  protocol::MockCursorShapeStub cursor_stub;
  EmptyCursorFilter cursor_filter(&cursor_stub);

  EXPECT_CALL(cursor_stub, SetCursorShape(EqualsCursorShape(kEmptyCursor)));

  cursor_filter.SetCursorShape(kEmptyCursor);
}

// Verify that EmptyCursorFilter turns transparent cursors into empty ones.
TEST(EmptyCursorFilterTest, TransparentCursor) {
  const protocol::CursorShapeInfo& kEmptyCursor = EmptyCursorShape();
  protocol::MockCursorShapeStub cursor_stub;
  EmptyCursorFilter cursor_filter(&cursor_stub);

  EXPECT_CALL(cursor_stub, SetCursorShape(EqualsCursorShape(kEmptyCursor)));

  cursor_filter.SetCursorShape(CreateTransparentCursorShape());
}

// Verify that EmptyCursorFilter leaves non-transparent cursors alone.
TEST(EmptyCursorFilterTest, NonTransparentCursor) {
  const protocol::CursorShapeInfo& kOpaqueCursor = CreateOpaqueCursorShape();
  protocol::MockCursorShapeStub cursor_stub;
  EmptyCursorFilter cursor_filter(&cursor_stub);

  EXPECT_CALL(cursor_stub, SetCursorShape(EqualsCursorShape(kOpaqueCursor)));

  cursor_filter.SetCursorShape(kOpaqueCursor);
}

}  // namespace remoting
