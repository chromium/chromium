// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/draw_utils/page_boundary_intersect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace chrome_pdf {

namespace {

constexpr gfx::Rect kRect(5, 10, 20, 30);
constexpr gfx::PointF kInsidePoint(15, 20);
constexpr gfx::PointF kTopLeftPoint(5, 10);
constexpr gfx::PointF kTopPoint(15, 10);
constexpr gfx::PointF kTopRightPoint(24.9999f, 10);
constexpr gfx::PointF kRightPoint(24.9999f, 30);
constexpr gfx::PointF kBottomRightPoint(24.9999f, 39.9999f);
constexpr gfx::PointF kBottomPoint(20, 39.9999f);
constexpr gfx::PointF kBottomLeftPoint(5, 39.9999f);
constexpr gfx::PointF kLeftPoint(5, 30);

constexpr float kTolerance = 0.001f;

}  // namespace

TEST(PageBoundaryIntersectPoint, Left) {
  EXPECT_POINTF_NEAR(gfx::PointF(5, 13.3333f),
                     CalculatePageBoundaryIntersectPoint(kRect, kInsidePoint,
                                                         gfx::PointF(0, 10)),
                     kTolerance);
  EXPECT_POINTF_NEAR(gfx::PointF(5, 20),
                     CalculatePageBoundaryIntersectPoint(kRect, kInsidePoint,
                                                         gfx::PointF(0, 20)),
                     kTolerance);
  EXPECT_POINTF_NEAR(gfx::PointF(5, 33.3333f),
                     CalculatePageBoundaryIntersectPoint(kRect, kInsidePoint,
                                                         gfx::PointF(0, 40)),
                     kTolerance);
}

TEST(PageBoundaryIntersectPoint, Top) {
  EXPECT_POINTF_NEAR(gfx::PointF(10, 10),
                     CalculatePageBoundaryIntersectPoint(kRect, kInsidePoint,
                                                         gfx::PointF(5, 0)),
                     kTolerance);
  EXPECT_POINTF_NEAR(gfx::PointF(15, 10),
                     CalculatePageBoundaryIntersectPoint(kRect, kInsidePoint,
                                                         gfx::PointF(15, 0)),
                     kTolerance);
  EXPECT_POINTF_NEAR(gfx::PointF(17.5f, 10),
                     CalculatePageBoundaryIntersectPoint(kRect, kInsidePoint,
                                                         gfx::PointF(20, 0)),
                     kTolerance);
}

TEST(PageBoundaryIntersectPoint, Right) {
  EXPECT_POINTF_NEAR(gfx::PointF(25, 16.6667f),
                     CalculatePageBoundaryIntersectPoint(kRect, kInsidePoint,
                                                         gfx::PointF(30, 15)),
                     kTolerance);
  EXPECT_POINTF_NEAR(gfx::PointF(25, 20),
                     CalculatePageBoundaryIntersectPoint(kRect, kInsidePoint,
                                                         gfx::PointF(30, 20)),
                     kTolerance);
  EXPECT_POINTF_NEAR(gfx::PointF(25, 23.3333f),
                     CalculatePageBoundaryIntersectPoint(kRect, kInsidePoint,
                                                         gfx::PointF(30, 25)),
                     kTolerance);
}

TEST(PageBoundaryIntersectPoint, Bottom) {
  EXPECT_POINTF_NEAR(gfx::PointF(8.3333f, 40),
                     CalculatePageBoundaryIntersectPoint(kRect, kInsidePoint,
                                                         gfx::PointF(5, 50)),
                     kTolerance);
  EXPECT_POINTF_NEAR(gfx::PointF(15, 40),
                     CalculatePageBoundaryIntersectPoint(kRect, kInsidePoint,
                                                         gfx::PointF(15, 50)),
                     kTolerance);
  EXPECT_POINTF_NEAR(gfx::PointF(18.3333f, 40),
                     CalculatePageBoundaryIntersectPoint(kRect, kInsidePoint,
                                                         gfx::PointF(20, 50)),
                     kTolerance);
}

TEST(PageBoundaryIntersectPoint, BorderGoingOutwards) {
  EXPECT_POINTF_NEAR(kTopLeftPoint,
                     CalculatePageBoundaryIntersectPoint(kRect, kTopLeftPoint,
                                                         gfx::PointF(10, 0)),
                     kTolerance);
  EXPECT_POINTF_NEAR(
      kTopPoint,
      CalculatePageBoundaryIntersectPoint(kRect, kTopPoint, gfx::PointF(10, 0)),
      kTolerance);
  EXPECT_POINTF_NEAR(kTopRightPoint,
                     CalculatePageBoundaryIntersectPoint(kRect, kTopRightPoint,
                                                         gfx::PointF(10, 0)),
                     kTolerance);
  EXPECT_POINTF_NEAR(kRightPoint,
                     CalculatePageBoundaryIntersectPoint(kRect, kRightPoint,
                                                         gfx::PointF(30, 10)),
                     kTolerance);
  EXPECT_POINTF_NEAR(kBottomRightPoint,
                     CalculatePageBoundaryIntersectPoint(
                         kRect, kBottomRightPoint, gfx::PointF(30, 10)),
                     kTolerance);
  EXPECT_POINTF_NEAR(kBottomPoint,
                     CalculatePageBoundaryIntersectPoint(kRect, kBottomPoint,
                                                         gfx::PointF(15, 50)),
                     kTolerance);
  EXPECT_POINTF_NEAR(kBottomLeftPoint,
                     CalculatePageBoundaryIntersectPoint(
                         kRect, kBottomLeftPoint, gfx::PointF(15, 50)),
                     kTolerance);
  EXPECT_POINTF_NEAR(kLeftPoint,
                     CalculatePageBoundaryIntersectPoint(kRect, kLeftPoint,
                                                         gfx::PointF(0, 10)),
                     kTolerance);
}

TEST(PageBoundaryIntersectPoint, BorderGoingAcrossPage) {
  EXPECT_POINTF_NEAR(gfx::PointF(12.5f, 40),
                     CalculatePageBoundaryIntersectPoint(kRect, kTopLeftPoint,
                                                         gfx::PointF(15, 50)),
                     kTolerance);
  EXPECT_POINTF_NEAR(gfx::PointF(15, 40),
                     CalculatePageBoundaryIntersectPoint(kRect, kTopPoint,
                                                         gfx::PointF(15, 50)),
                     kTolerance);
  EXPECT_POINTF_NEAR(gfx::PointF(5, 34),
                     CalculatePageBoundaryIntersectPoint(kRect, kTopRightPoint,
                                                         gfx::PointF(0, 40)),
                     kTolerance);
  EXPECT_POINTF_NEAR(gfx::PointF(22.5f, 40),
                     CalculatePageBoundaryIntersectPoint(kRect, kRightPoint,
                                                         gfx::PointF(20, 50)),
                     kTolerance);
  EXPECT_POINTF_NEAR(gfx::PointF(21.25f, 10),
                     CalculatePageBoundaryIntersectPoint(
                         kRect, kBottomRightPoint, gfx::PointF(20, 0)),
                     kTolerance);
  EXPECT_POINTF_NEAR(gfx::PointF(20, 10),
                     CalculatePageBoundaryIntersectPoint(kRect, kBottomPoint,
                                                         gfx::PointF(20, 0)),
                     kTolerance);
  EXPECT_POINTF_NEAR(gfx::PointF(16.25f, 10),
                     CalculatePageBoundaryIntersectPoint(
                         kRect, kBottomLeftPoint, gfx::PointF(20, 0)),
                     kTolerance);
  EXPECT_POINTF_NEAR(gfx::PointF(25, 26),
                     CalculatePageBoundaryIntersectPoint(kRect, kLeftPoint,
                                                         gfx::PointF(80, 15)),
                     kTolerance);
}

TEST(PageBoundaryIntersectPoint, BorderTopBottomEdge) {
  EXPECT_POINTF_NEAR(kTopLeftPoint,
                     CalculatePageBoundaryIntersectPoint(kRect, kTopLeftPoint,
                                                         gfx::PointF(0, 10)),
                     kTolerance);
  EXPECT_POINTF_NEAR(
      kTopLeftPoint,
      CalculatePageBoundaryIntersectPoint(kRect, kTopPoint, gfx::PointF(0, 10)),
      kTolerance);
  EXPECT_POINTF_NEAR(kTopLeftPoint,
                     CalculatePageBoundaryIntersectPoint(kRect, kTopRightPoint,
                                                         gfx::PointF(0, 10)),
                     kTolerance);

  EXPECT_POINTF_NEAR(kTopRightPoint,
                     CalculatePageBoundaryIntersectPoint(kRect, kTopLeftPoint,
                                                         gfx::PointF(30, 10)),
                     kTolerance);
  EXPECT_POINTF_NEAR(kTopRightPoint,
                     CalculatePageBoundaryIntersectPoint(kRect, kTopPoint,
                                                         gfx::PointF(30, 10)),
                     kTolerance);
  EXPECT_POINTF_NEAR(kTopRightPoint,
                     CalculatePageBoundaryIntersectPoint(kRect, kTopRightPoint,
                                                         gfx::PointF(30, 10)),
                     kTolerance);

  EXPECT_POINTF_NEAR(kBottomLeftPoint,
                     CalculatePageBoundaryIntersectPoint(
                         kRect, kBottomLeftPoint, gfx::PointF(0, 40)),
                     kTolerance);
  EXPECT_POINTF_NEAR(kBottomLeftPoint,
                     CalculatePageBoundaryIntersectPoint(kRect, kBottomPoint,
                                                         gfx::PointF(0, 40)),
                     kTolerance);
  EXPECT_POINTF_NEAR(kBottomLeftPoint,
                     CalculatePageBoundaryIntersectPoint(
                         kRect, kBottomRightPoint, gfx::PointF(0, 40)),
                     kTolerance);

  EXPECT_POINTF_NEAR(kBottomRightPoint,
                     CalculatePageBoundaryIntersectPoint(
                         kRect, kBottomLeftPoint, gfx::PointF(30, 40)),
                     kTolerance);
  EXPECT_POINTF_NEAR(kBottomRightPoint,
                     CalculatePageBoundaryIntersectPoint(kRect, kBottomPoint,
                                                         gfx::PointF(30, 40)),
                     kTolerance);
  EXPECT_POINTF_NEAR(kBottomRightPoint,
                     CalculatePageBoundaryIntersectPoint(
                         kRect, kBottomRightPoint, gfx::PointF(30, 40)),
                     kTolerance);
}

TEST(PageBoundaryIntersectPoint, BorderLeftRightEdge) {
  EXPECT_POINTF_NEAR(kTopLeftPoint,
                     CalculatePageBoundaryIntersectPoint(kRect, kTopLeftPoint,
                                                         gfx::PointF(5, 0)),
                     kTolerance);
  EXPECT_POINTF_NEAR(
      kTopLeftPoint,
      CalculatePageBoundaryIntersectPoint(kRect, kLeftPoint, gfx::PointF(5, 0)),
      kTolerance);
  EXPECT_POINTF_NEAR(kTopLeftPoint,
                     CalculatePageBoundaryIntersectPoint(
                         kRect, kBottomLeftPoint, gfx::PointF(5, 0)),
                     kTolerance);

  EXPECT_POINTF_NEAR(kBottomLeftPoint,
                     CalculatePageBoundaryIntersectPoint(kRect, kTopLeftPoint,
                                                         gfx::PointF(5, 50)),
                     kTolerance);
  EXPECT_POINTF_NEAR(kBottomLeftPoint,
                     CalculatePageBoundaryIntersectPoint(kRect, kLeftPoint,
                                                         gfx::PointF(5, 50)),
                     kTolerance);
  EXPECT_POINTF_NEAR(kBottomLeftPoint,
                     CalculatePageBoundaryIntersectPoint(
                         kRect, kBottomLeftPoint, gfx::PointF(5, 50)),
                     kTolerance);

  EXPECT_POINTF_NEAR(kTopRightPoint,
                     CalculatePageBoundaryIntersectPoint(kRect, kTopRightPoint,
                                                         gfx::PointF(25, 0)),
                     kTolerance);
  EXPECT_POINTF_NEAR(kTopRightPoint,
                     CalculatePageBoundaryIntersectPoint(kRect, kRightPoint,
                                                         gfx::PointF(25, 0)),
                     kTolerance);
  EXPECT_POINTF_NEAR(kTopRightPoint,
                     CalculatePageBoundaryIntersectPoint(
                         kRect, kBottomRightPoint, gfx::PointF(25, 0)),
                     kTolerance);

  EXPECT_POINTF_NEAR(kBottomRightPoint,
                     CalculatePageBoundaryIntersectPoint(kRect, kTopRightPoint,
                                                         gfx::PointF(25, 50)),
                     kTolerance);
  EXPECT_POINTF_NEAR(kBottomRightPoint,
                     CalculatePageBoundaryIntersectPoint(kRect, kRightPoint,
                                                         gfx::PointF(25, 50)),
                     kTolerance);
  EXPECT_POINTF_NEAR(kBottomRightPoint,
                     CalculatePageBoundaryIntersectPoint(
                         kRect, kBottomRightPoint, gfx::PointF(25, 50)),
                     kTolerance);
}

}  // namespace chrome_pdf
