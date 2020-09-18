// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/geometry_conversions.h"

#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

namespace chrome_pdf {

TEST(GeometryConversionsTest, PointFromPPPoint) {
  gfx::Point point = PointFromPPPoint(pp::Point(-1, 2));
  EXPECT_EQ(point, gfx::Point(-1, 2));

  point = PointFromPPPoint(PP_MakePoint(2, -1));
  EXPECT_EQ(point, gfx::Point(2, -1));
}

TEST(GeometryConversionsTest, PPPointFromPoint) {
  pp::Point pp_cpp_point = PPPointFromPoint(gfx::Point(-1, 2));
  EXPECT_EQ(pp_cpp_point.x(), -1);
  EXPECT_EQ(pp_cpp_point.y(), 2);

  PP_Point pp_c_point = PPPointFromPoint(gfx::Point(2, -1));
  EXPECT_EQ(pp_c_point.x, 2);
  EXPECT_EQ(pp_c_point.y, -1);
}

TEST(GeometryConversionsTest, PointFFromPPFloatPoint) {
  gfx::PointF float_point = PointFFromPPFloatPoint(pp::FloatPoint(-1.2f, 2.2f));
  EXPECT_EQ(float_point, gfx::PointF(-1.2f, 2.2f));

  float_point = PointFFromPPFloatPoint(PP_MakeFloatPoint(-2.2f, 1.2f));
  EXPECT_EQ(float_point, gfx::PointF(-2.2f, 1.2f));
}

TEST(GeometryConversionsTest, RectFromPPRect) {
  gfx::Rect rect = RectFromPPRect(pp::Rect(-1, 2, 3, 4));
  EXPECT_EQ(rect, gfx::Rect(-1, 2, 3, 4));

  rect = RectFromPPRect(PP_MakeRectFromXYWH(2, -1, 4, 3));
  EXPECT_EQ(rect, gfx::Rect(2, -1, 4, 3));
}

TEST(GeometryConversionsTest, PPRectFromRect) {
  pp::Rect pp_cpp_rect = PPRectFromRect(gfx::Rect(-1, 2, 3, 4));
  EXPECT_EQ(pp_cpp_rect.x(), -1);
  EXPECT_EQ(pp_cpp_rect.y(), 2);
  EXPECT_EQ(pp_cpp_rect.width(), 3);
  EXPECT_EQ(pp_cpp_rect.height(), 4);

  PP_Rect pp_c_rect = PPRectFromRect(gfx::Rect(2, -1, 4, 3));
  EXPECT_EQ(pp_c_rect.point.x, 2);
  EXPECT_EQ(pp_c_rect.point.y, -1);
  EXPECT_EQ(pp_c_rect.size.width, 4);
  EXPECT_EQ(pp_c_rect.size.height, 3);
}

TEST(GeometryConversionsTest, RectFFromPPFloatRect) {
  gfx::RectF rect =
      RectFFromPPFloatRect(pp::FloatRect(-1.0f, 2.1f, 3.2f, 4.3f));
  EXPECT_EQ(rect, gfx::RectF(-1.0f, 2.1f, 3.2f, 4.3f));

  rect =
      RectFFromPPFloatRect(PP_MakeFloatRectFromXYWH(2.9f, -1.8f, 4.7f, 3.6f));
  EXPECT_EQ(rect, gfx::RectF(2.9f, -1.8f, 4.7f, 3.6f));
}

TEST(GeometryConversionsTest, PPFloatRectFromRectF) {
  pp::FloatRect pp_cpp_rect =
      PPFloatRectFromRectF(gfx::RectF(-1.1f, 2.3f, 3.5f, 4.7f));
  EXPECT_EQ(pp_cpp_rect.x(), -1.1f);
  EXPECT_EQ(pp_cpp_rect.y(), 2.3f);
  EXPECT_EQ(pp_cpp_rect.width(), 3.5f);
  EXPECT_EQ(pp_cpp_rect.height(), 4.7f);

  PP_FloatRect pp_c_rect =
      PPFloatRectFromRectF(gfx::RectF(2.2f, -1.4f, 4.6f, 3.8f));
  EXPECT_EQ(pp_c_rect.point.x, 2.2f);
  EXPECT_EQ(pp_c_rect.point.y, -1.4f);
  EXPECT_EQ(pp_c_rect.size.width, 4.6f);
  EXPECT_EQ(pp_c_rect.size.height, 3.8f);
}

TEST(GeometryConversionsTest, SizeFromPPSize) {
  gfx::Size size = SizeFromPPSize(pp::Size(3, 4));
  EXPECT_EQ(size, gfx::Size(3, 4));

  size = SizeFromPPSize(PP_MakeSize(4, 3));
  EXPECT_EQ(size, gfx::Size(4, 3));
}

TEST(GeometryConversionsTest, PPSizeFromSize) {
  pp::Size pp_cpp_size = PPSizeFromSize(gfx::Size(3, 4));
  EXPECT_EQ(pp_cpp_size.width(), 3);
  EXPECT_EQ(pp_cpp_size.height(), 4);

  PP_Size pp_c_size = PPSizeFromSize(gfx::Size(4, 3));
  EXPECT_EQ(pp_c_size.width, 4);
  EXPECT_EQ(pp_c_size.height, 3);
}

TEST(GeometryConversionsTest, VectorFromPPPoint) {
  gfx::Vector2d point = VectorFromPPPoint(pp::Point(-1, 2));
  EXPECT_EQ(point, gfx::Vector2d(-1, 2));

  point = VectorFromPPPoint(PP_MakePoint(2, -1));
  EXPECT_EQ(point, gfx::Vector2d(2, -1));
}

TEST(GeometryConversionsTest, PPPointFromVector) {
  pp::Point pp_cpp_point = PPPointFromVector(gfx::Vector2d(-1, 2));
  EXPECT_EQ(pp_cpp_point.x(), -1);
  EXPECT_EQ(pp_cpp_point.y(), 2);

  PP_Point pp_c_point = PPPointFromVector(gfx::Vector2d(2, -1));
  EXPECT_EQ(pp_c_point.x, 2);
  EXPECT_EQ(pp_c_point.y, -1);
}

}  // namespace chrome_pdf
