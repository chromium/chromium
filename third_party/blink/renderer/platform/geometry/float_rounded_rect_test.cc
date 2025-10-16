/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/infinite_int_rect.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace blink {

TEST(FloatRoundedRectTest, IntersectsQuadIsInclusive) {
  FloatRoundedRect::Radii corner_radii(5);

  // A rect at (10, 10) with dimensions 20x20 and radii of size 5x5.
  FloatRoundedRect r(gfx::RectF(10, 10, 20, 20), corner_radii);

  // A quad fully inside the rounded rect should intersect.
  EXPECT_TRUE(r.IntersectsQuad(gfx::QuadF(gfx::RectF(11, 11, 8, 8))));

  // A quad fully outside the rounded rect should not intersect.
  EXPECT_FALSE(r.IntersectsQuad(gfx::QuadF(gfx::RectF(0, 0, 1, 1))));

  // A quad touching the top edge of the rounded rect should intersect.
  EXPECT_TRUE(r.IntersectsQuad(gfx::QuadF(gfx::RectF(15, 9, 5, 1))));

  // A quad touching the right edge of the rounded rect should intersect.
  EXPECT_TRUE(r.IntersectsQuad(gfx::QuadF(gfx::RectF(30, 15, 1, 1))));

  // A quad touching the bottom edge of the rounded rect should intersect.
  EXPECT_TRUE(r.IntersectsQuad(gfx::QuadF(gfx::RectF(15, 30, 1, 1))));

  // A quad touching the left edge of the rounded rect should intersect.
  EXPECT_TRUE(r.IntersectsQuad(gfx::QuadF(gfx::RectF(9, 15, 1, 1))));

  // A quad outside the top-left arc should not intersect.
  EXPECT_FALSE(r.IntersectsQuad(gfx::QuadF(gfx::RectF(10, 10, 1, 1))));

  // A quad inside the top-left arc should intersect.
  EXPECT_TRUE(r.IntersectsQuad(gfx::QuadF(gfx::RectF(13, 13, 1, 1))));

  // A quad outside the top-right arc should not intersect.
  EXPECT_FALSE(r.IntersectsQuad(gfx::QuadF(gfx::RectF(29, 10, 1, 1))));

  // A quad inside the top-right arc should intersect.
  EXPECT_TRUE(r.IntersectsQuad(gfx::QuadF(gfx::RectF(26, 13, 1, 1))));

  // A quad outside the bottom-right arc should not intersect.
  EXPECT_FALSE(r.IntersectsQuad(gfx::QuadF(gfx::RectF(29, 29, 1, 1))));

  // A quad inside the bottom-right arc should intersect.
  EXPECT_TRUE(r.IntersectsQuad(gfx::QuadF(gfx::RectF(26, 26, 1, 1))));

  // A quad outside the bottom-left arc should not intersect.
  EXPECT_FALSE(r.IntersectsQuad(gfx::QuadF(gfx::RectF(10, 29, 1, 1))));

  // A quad inside the bottom-left arc should intersect.
  EXPECT_TRUE(r.IntersectsQuad(gfx::QuadF(gfx::RectF(13, 26, 1, 1))));
}

TEST(FloatRoundedrectTest, ConstrainRadii) {
  FloatRoundedRect empty;
  empty.ConstrainRadii();
  EXPECT_EQ(FloatRoundedRect(), empty);

  FloatRoundedRect r1(-100, -100, 200, 200);
  r1.ConstrainRadii();
  EXPECT_EQ(FloatRoundedRect(-100, -100, 200, 200), r1);

  FloatRoundedRect r2(gfx::RectF(-100, -100, 200, 200), 10);
  r2.ConstrainRadii();
  EXPECT_EQ(FloatRoundedRect(gfx::RectF(-100, -100, 200, 200), 10), r2);

  FloatRoundedRect r3(gfx::RectF(-100, -100, 200, 200), 100);
  r3.ConstrainRadii();
  EXPECT_EQ(FloatRoundedRect(gfx::RectF(-100, -100, 200, 200), 100), r3);

  FloatRoundedRect r4(gfx::RectF(-100, -100, 200, 200), 160);
  r4.ConstrainRadii();
  EXPECT_EQ(FloatRoundedRect(gfx::RectF(-100, -100, 200, 200), 100), r4);

  FloatRoundedRect r5(gfx::RectF(-100, -100, 200, 200), gfx::SizeF(10, 20),
                      gfx::SizeF(100, 250), gfx::SizeF(200, 60),
                      gfx::SizeF(50, 150));
  r5.ConstrainRadii();
  EXPECT_EQ(FloatRoundedRect(gfx::RectF(-100, -100, 200, 200),
                             gfx::SizeF(5, 10), gfx::SizeF(50, 125),
                             gfx::SizeF(100, 30), gfx::SizeF(25, 75)),
            r5);

  FloatRoundedRect r6(gfx::RectF(-100, -100, 200, 200), gfx::SizeF(10, 20),
                      gfx::SizeF(60, 200), gfx::SizeF(250, 100),
                      gfx::SizeF(150, 50));
  r6.ConstrainRadii();
  EXPECT_EQ(FloatRoundedRect(gfx::RectF(-100, -100, 200, 200),
                             gfx::SizeF(5, 10), gfx::SizeF(30, 100),
                             gfx::SizeF(125, 50), gfx::SizeF(75, 25)),
            r6);

  FloatRoundedRect r7(gfx::RectF(0, 0, 85089, 21377),
                      gfx::SizeF(1388.89, 1388.89),
                      gfx::SizeF(58711.2, 14750.3), gfx::SizeF(0, 13467.7),
                      gfx::SizeF(85088.6, 21377.3));
  r7.ConstrainRadii();
  EXPECT_TRUE(r7.IsRenderable());
}

TEST(FloatRoundedRectTest, OutsetRect) {
  FloatRoundedRect r(gfx::RectF(0, 0, 100, 100));
  r.Outset(gfx::OutsetsF().set_top(1).set_right(2).set_bottom(3).set_left(4));
  EXPECT_EQ(FloatRoundedRect(gfx::RectF(-4, -1, 106, 104)), r);
  r.Outset(
      gfx::OutsetsF().set_top(-1).set_right(-2).set_bottom(-3).set_left(-4));
  EXPECT_EQ(FloatRoundedRect(gfx::RectF(0, 0, 100, 100)), r);
}

TEST(FloatRoundedRectTest, InsetRect) {
  FloatRoundedRect r(gfx::RectF(0, 0, 100, 100));
  r.Inset(gfx::InsetsF().set_top(1).set_right(2).set_bottom(3).set_left(4));
  EXPECT_EQ(FloatRoundedRect(gfx::RectF(4, 1, 94, 96)), r);
  r.Inset(gfx::InsetsF().set_top(-1).set_right(-2).set_bottom(-3).set_left(-4));
  EXPECT_EQ(FloatRoundedRect(gfx::RectF(0, 0, 100, 100)), r);
}

TEST(FloatRoundedRectTest, OutsetWithRadii) {
  FloatRoundedRect r(gfx::RectF(0, 0, 100, 100), gfx::SizeF(5, 10),
                     gfx::SizeF(15, 20), gfx::SizeF(0, 30), gfx::SizeF(35, 0));
  r.Outset(
      gfx::OutsetsF().set_top(40).set_right(30).set_bottom(20).set_left(10));
  // Zero components of radii should be kept unchanged to ensure sharp corners
  // are still sharp.
  EXPECT_EQ(FloatRoundedRect(gfx::RectF(-10, -40, 140, 160), gfx::SizeF(15, 50),
                             gfx::SizeF(45, 60), gfx::SizeF(0, 50),
                             gfx::SizeF(65, 0)),
            r);
}

TEST(FloatRoundedRectTest, InsetWithRadii) {
  FloatRoundedRect r(gfx::RectF(0, 0, 100, 100), gfx::SizeF(20, 30),
                     gfx::SizeF(40, 50), gfx::SizeF(0, 60), gfx::SizeF(70, 0));
  r.Inset(gfx::InsetsF().set_top(40).set_right(30).set_bottom(20).set_left(10));
  EXPECT_EQ(FloatRoundedRect(gfx::RectF(10, 40, 60, 40), gfx::SizeF(10, 0),
                             gfx::SizeF(10, 10), gfx::SizeF(0, 40),
                             gfx::SizeF(40, 0)),
            r);
}

// Outset() should keep zero components in radii to ensure sharp corners are
// still sharp.
TEST(FloatRoundedRectTest, InsetWithPartialZeroRadii) {
  FloatRoundedRect r(gfx::RectF(0, 0, 100, 100), gfx::SizeF(5, 0),
                     gfx::SizeF(0, 20), gfx::SizeF(0, 30), gfx::SizeF(35, 0));
  r.Inset(10);
  EXPECT_EQ(
      FloatRoundedRect(gfx::RectF(10, 10, 80, 80), gfx::SizeF(0, 0),
                       gfx::SizeF(0, 10), gfx::SizeF(0, 20), gfx::SizeF(25, 0)),
      r);
}

TEST(FloatRoundedRectTest, OutsetWithCornerCorrection) {
  FloatRoundedRect r(gfx::RectF(0, 0, 200, 200), gfx::SizeF(4, 8),
                     gfx::SizeF(12, 16), gfx::SizeF(0, 32), gfx::SizeF(64, 0));
  r.OutsetWithCornerCorrection(32);
  EXPECT_RECTF_EQ(r.Rect(), gfx::RectF(-32, -32, 264, 264));
  EXPECT_SIZEF_NEAR(r.GetRadii().TopLeft(), gfx::SizeF(14.5639, 26.5009f),
                    0.01f);
  EXPECT_SIZEF_NEAR(r.GetRadii().TopRight(), gfx::SizeF(36.201f, 44.0069f),
                    0.01f);
  EXPECT_SIZEF_NEAR(r.GetRadii().BottomLeft(), gfx::SizeF(0, 64), 0.01f);
  EXPECT_SIZEF_NEAR(r.GetRadii().BottomRight(), gfx::SizeF(96, 0), 0.01f);
}

TEST(FloatRoundedRectTest, OutsetWithCornerCorrectionNonUniform) {
  FloatRoundedRect r(gfx::RectF(0, 0, 200, 100), gfx::SizeF(50, 8),
                     gfx::SizeF(12, 56), gfx::SizeF(0, 32), gfx::SizeF(64, 0));
  r.OutsetWithCornerCorrection(32);
  EXPECT_RECTF_EQ(r.Rect(), gfx::RectF(-32, -32, 264, 164));
  EXPECT_SIZEF_NEAR(r.GetRadii().TopLeft(), gfx::SizeF(82, 26.5553), 0.01f);
  EXPECT_SIZEF_NEAR(r.GetRadii().TopRight(), gfx::SizeF(36.201f, 88), 0.01f);
  EXPECT_SIZEF_NEAR(r.GetRadii().BottomLeft(), gfx::SizeF(0, 64), 0.01f);
  EXPECT_SIZEF_NEAR(r.GetRadii().BottomRight(), gfx::SizeF(96, 0), 0.01f);
}

TEST(FloatRoundedRectTest, InsetToBeNonRenderable) {
  FloatRoundedRect pie(gfx::RectF(0, 0, 100, 100), gfx::SizeF(100, 100),
                       gfx::SizeF(), gfx::SizeF(), gfx::SizeF());
  EXPECT_TRUE(pie.IsRenderable());
  FloatRoundedRect small_pie = pie;
  small_pie.Inset(20);
  EXPECT_EQ(FloatRoundedRect(gfx::RectF(20, 20, 60, 60), gfx::SizeF(80, 80),
                             gfx::SizeF(), gfx::SizeF(), gfx::SizeF()),
            small_pie);
  EXPECT_FALSE(small_pie.IsRenderable());
  small_pie.Outset(20);
  EXPECT_EQ(pie, small_pie);
}

TEST(FloatRoundedRectTest, OutsetForShapeMargin) {
  FloatRoundedRect r(gfx::RectF(0, 0, 100, 100), gfx::SizeF(5, 10),
                     gfx::SizeF(15, 0), gfx::SizeF(0, 30), gfx::SizeF(0, 0));
  r.OutsetForShapeMargin(0);
  EXPECT_EQ(
      FloatRoundedRect(gfx::RectF(0, 0, 100, 100), gfx::SizeF(5, 10),
                       gfx::SizeF(15, 0), gfx::SizeF(0, 30), gfx::SizeF(0, 0)),
      r);
  r.OutsetForShapeMargin(5);
  EXPECT_EQ(
      FloatRoundedRect(gfx::RectF(-5, -5, 110, 110), gfx::SizeF(10, 15),
                       gfx::SizeF(20, 5), gfx::SizeF(5, 35), gfx::SizeF(5, 5)),
      r);
}

TEST(FloatRoundedRectTest, IntersectsQuadEnclosing) {
  gfx::SizeF one_radii(20, 20);
  FloatRoundedRect::Radii corner_radii;
  corner_radii.SetTopLeft(one_radii);
  corner_radii.SetTopRight(one_radii);
  corner_radii.SetBottomLeft(one_radii);
  corner_radii.SetBottomRight(one_radii);
  // A rect at (100, 25) with dimensions 100x100 and radii of size 20x20.
  FloatRoundedRect r(gfx::RectF(100, 25, 100, 100), corner_radii);

  // Encloses `r` without intersecting any of the geometry (corners or base
  // rectangle).
  gfx::QuadF fully_outside(gfx::PointF(150, -30), gfx::PointF(255, 75),
                           gfx::PointF(150, 180), gfx::PointF(45, 75));
  EXPECT_TRUE(r.IntersectsQuad(fully_outside));

  // Encloses `r`, touching at the corners of the base rectangle.
  gfx::QuadF touching(gfx::PointF(150, -25), gfx::PointF(250, 75),
                      gfx::PointF(150, 175), gfx::PointF(50, 75));
  EXPECT_TRUE(r.IntersectsQuad(touching));

  // Encloses `r`, crossing through the rounded corners (without intersecting
  // them).
  gfx::QuadF crossing_corners(gfx::PointF(150, -15), gfx::PointF(240, 75),
                              gfx::PointF(150, 165), gfx::PointF(60, 75));
  EXPECT_TRUE(r.IntersectsQuad(crossing_corners));
}

TEST(FloatRoundedRectTest, Conversion) {
  FloatRoundedRect r(gfx::RectF(100, 200, 300, 400), gfx::SizeF(5, 6),
                     gfx::SizeF(7, 8), gfx::SizeF(9, 10), gfx::SizeF(11, 12));
  gfx::RRectF gfx_r(r);
  SkRRect sk_r(r);
  EXPECT_EQ(r, FloatRoundedRect(gfx_r));
  EXPECT_EQ(r, FloatRoundedRect(sk_r));
}

TEST(FloatRoundedRectTest, ToString) {
  gfx::SizeF corner_rect(1, 2);
  FloatRoundedRect rounded_rect(
      gfx::RectF(3, 5, 7, 11),
      FloatRoundedRect::Radii(corner_rect, corner_rect, corner_rect,
                              corner_rect));
  EXPECT_EQ("3,5 7x11 radii:(tl:1x2; tr:1x2; bl:1x2; br:1x2)",
            rounded_rect.ToString());

  FloatRoundedRect infinite((gfx::RectF(InfiniteIntRect())));
  EXPECT_EQ("InfiniteIntRect", infinite.ToString());

  FloatRoundedRect rect_without_radii(gfx::RectF(1, 3, 5, 7));
  EXPECT_EQ("1,3 5x7", rect_without_radii.ToString());
}

}  // namespace blink
