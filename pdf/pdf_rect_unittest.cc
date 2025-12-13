// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_rect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"

namespace chrome_pdf {

TEST(PdfRectTest, DefaultConstructor) {
  static constexpr PdfRect kRect;
  EXPECT_EQ(0, kRect.left());
  EXPECT_EQ(0, kRect.bottom());
  EXPECT_EQ(0, kRect.right());
  EXPECT_EQ(0, kRect.top());
  EXPECT_EQ(0, kRect.width());
  EXPECT_EQ(0, kRect.height());
}

TEST(PdfRectTest, FloatsConstructor) {
  static constexpr PdfRect kRect(1.0f, 2.0f, 3.0f, 5.0f);
  EXPECT_EQ(1.0f, kRect.left());
  EXPECT_EQ(2.0f, kRect.bottom());
  EXPECT_EQ(3.0f, kRect.right());
  EXPECT_EQ(5.0f, kRect.top());
  EXPECT_EQ(2.0f, kRect.width());
  EXPECT_EQ(3.0f, kRect.height());
}

TEST(PdfRectTest, GfxRectFConstructor) {
  static constexpr PdfRect kRect(1.0f, 2.0f, 3.0f, 5.0f);
  const gfx::RectF rect = kRect.AsGfxRectF();
  const PdfRect pdf_rect(rect);
  EXPECT_EQ(kRect, pdf_rect);
}

TEST(PdfRectTest, WritableAccessors) {
  PdfRect rect;
  *rect.writable_left() = 5.0f;
  *rect.writable_bottom() = 6.0f;
  *rect.writable_right() = 7.0f;
  *rect.writable_top() = 8.0f;
  EXPECT_EQ(5.0f, rect.left());
  EXPECT_EQ(6.0f, rect.bottom());
  EXPECT_EQ(7.0f, rect.right());
  EXPECT_EQ(8.0f, rect.top());
}

TEST(PdfRectTest, AsGfxRectF) {
  static constexpr PdfRect kRect(1.0f, 2.0f, 3.0f, 5.0f);
  const gfx::RectF rect = kRect.AsGfxRectF();
  EXPECT_EQ(1.0f, rect.x());
  EXPECT_EQ(2.0f, rect.y());
  EXPECT_EQ(3.0f, rect.right());
  // Since gfx::RectF has its origin at the top-left, the bottom is the same as
  // PdfRect's top, and vice-versa.
  EXPECT_EQ(5.0f, rect.bottom());
}

TEST(PdfRectTest, Offset) {
  PdfRect rect(1.0f, 2.0f, 3.0f, 5.0f);
  rect.Offset(10.0f, 20.0f);
  EXPECT_EQ(11.0f, rect.left());
  EXPECT_EQ(22.0f, rect.bottom());
  EXPECT_EQ(13.0f, rect.right());
  EXPECT_EQ(25.0f, rect.top());

  rect.Offset(-5.0f, -15.0f);
  EXPECT_EQ(6.0f, rect.left());
  EXPECT_EQ(7.0f, rect.bottom());
  EXPECT_EQ(8.0f, rect.right());
  EXPECT_EQ(10.0f, rect.top());

  rect.Offset(0.0f, 0.0f);
  EXPECT_EQ(6.0f, rect.left());
  EXPECT_EQ(7.0f, rect.bottom());
  EXPECT_EQ(8.0f, rect.right());
  EXPECT_EQ(10.0f, rect.top());
}

TEST(PdfRectTest, IsEmpty) {
  PdfRect rect(1.0f, 2.0f, 3.0f, 5.0f);
  EXPECT_FALSE(rect.IsEmpty());

  // Zero width.
  rect = PdfRect(1.0f, 2.0f, 1.0f, 5.0f);
  EXPECT_TRUE(rect.IsEmpty());

  // Zero height.
  rect = PdfRect(1.0f, 2.0f, 3.0f, 2.0f);
  EXPECT_TRUE(rect.IsEmpty());

  // Default constructed.
  rect = PdfRect();
  EXPECT_TRUE(rect.IsEmpty());
}

TEST(PdfRectTest, Normalize) {
  PdfRect rect(3.0f, 4.0f, 1.0f, 2.0f);
  rect.Normalize();
  EXPECT_EQ(1.0f, rect.left());
  EXPECT_EQ(2.0f, rect.bottom());
  EXPECT_EQ(3.0f, rect.right());
  EXPECT_EQ(4.0f, rect.top());
}

TEST(PdfRectTest, Scale) {
  PdfRect rect(1.0f, 2.0f, 3.0f, 4.0f);
  rect.Scale(2.0f);
  EXPECT_EQ(2.0f, rect.left());
  EXPECT_EQ(4.0f, rect.bottom());
  EXPECT_EQ(6.0f, rect.right());
  EXPECT_EQ(8.0f, rect.top());
}

TEST(PdfRectTest, Intersect) {
  PdfRect rect(0.0f, 0.0f, 2.0f, 2.0f);
  static constexpr PdfRect kRect(1.0f, 1.0f, 3.0f, 3.0f);
  rect.Intersect(kRect);
  EXPECT_EQ(1.0f, rect.left());
  EXPECT_EQ(1.0f, rect.bottom());
  EXPECT_EQ(2.0f, rect.right());
  EXPECT_EQ(2.0f, rect.top());
}

TEST(PdfRectTest, IntersectEmpty) {
  {
    // Both rects empty.
    PdfRect rect;
    rect.Intersect(PdfRect());
    EXPECT_TRUE(rect.IsEmpty());
  }
  {
    // First rect empty.
    PdfRect rect;
    static constexpr PdfRect kRect(3.0f, 3.0f, 4.0f, 4.0f);
    rect.Intersect(kRect);
    EXPECT_TRUE(rect.IsEmpty());
  }
  {
    // Second rect empty.
    PdfRect rect(0.0f, 0.0f, 2.0f, 2.0f);
    rect.Intersect(PdfRect());
    EXPECT_TRUE(rect.IsEmpty());
  }
}

TEST(PdfRectTest, IntersectNonOverlapping) {
  {
    // Second rect above first.
    PdfRect rect(0.0f, 0.0f, 2.0f, 2.0f);
    static constexpr PdfRect kRect(0.0f, 3.0f, 2.0f, 4.0f);
    rect.Intersect(kRect);
    EXPECT_TRUE(rect.IsEmpty());
  }
  {
    // Second rect right of first.
    PdfRect rect(0.0f, 0.0f, 2.0f, 2.0f);
    static constexpr PdfRect kRect(3.0f, 0.0f, 4.0f, 2.0f);
    rect.Intersect(kRect);
    EXPECT_TRUE(rect.IsEmpty());
  }
  {
    // Second rect above and right of first.
    PdfRect rect(0.0f, 0.0f, 2.0f, 2.0f);
    static constexpr PdfRect kRect(3.0f, 3.0f, 4.0f, 4.0f);
    rect.Intersect(kRect);
    EXPECT_TRUE(rect.IsEmpty());
  }
}

TEST(PdfRectTest, Union) {
  PdfRect rect(0.0f, 0.0f, 2.0f, 2.0f);
  static constexpr PdfRect kRect(1.0f, 1.0f, 3.0f, 3.0f);
  rect.Union(kRect);
  EXPECT_EQ(0.0f, rect.left());
  EXPECT_EQ(0.0f, rect.bottom());
  EXPECT_EQ(3.0f, rect.right());
  EXPECT_EQ(3.0f, rect.top());
}

TEST(PdfRectTest, UnionEmpty) {
  static constexpr PdfRect kRect(3.0f, 3.0f, 4.0f, 4.0f);
  {
    // Both rects empty.
    PdfRect rect;
    rect.Union(PdfRect());
    EXPECT_TRUE(rect.IsEmpty());
  }
  {
    // First rect empty.
    PdfRect rect;
    rect.Union(kRect);
    EXPECT_EQ(kRect.left(), rect.left());
    EXPECT_EQ(kRect.bottom(), rect.bottom());
    EXPECT_EQ(kRect.right(), rect.right());
    EXPECT_EQ(kRect.top(), rect.top());
  }
  {
    // Second rect empty.
    PdfRect rect = kRect;
    rect.Union(PdfRect());
    EXPECT_EQ(kRect.left(), rect.left());
    EXPECT_EQ(kRect.bottom(), rect.bottom());
    EXPECT_EQ(kRect.right(), rect.right());
    EXPECT_EQ(kRect.top(), rect.top());
  }
}

}  // namespace chrome_pdf
