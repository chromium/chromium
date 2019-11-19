// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/document_layout.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {

namespace {

class DocumentLayoutOptionsTest : public testing::Test {
 protected:
  DocumentLayout::Options options_;
};

class DocumentLayoutTest : public testing::Test {
 protected:
  DocumentLayout layout_;
};

// TODO(kmoon): Need to use this with EXPECT_PRED2 instead of just using
// EXPECT_EQ, due to ADL issues with pp::Size's operator== (defined in global
// namespace, instead of in "pp").
inline bool PpSizeEq(const pp::Size& lhs, const pp::Size& rhs) {
  return lhs == rhs;
}

inline bool PpRectEq(const pp::Rect& lhs, const pp::Rect& rhs) {
  return lhs == rhs;
}

TEST_F(DocumentLayoutOptionsTest, DefaultConstructor) {
  EXPECT_EQ(options_.default_page_orientation(), PageOrientation::kOriginal);
}

TEST_F(DocumentLayoutOptionsTest, CopyConstructor) {
  options_.RotatePagesClockwise();

  DocumentLayout::Options copy(options_);
  EXPECT_EQ(copy.default_page_orientation(), PageOrientation::kClockwise90);

  options_.RotatePagesClockwise();
  EXPECT_EQ(copy.default_page_orientation(), PageOrientation::kClockwise90);
}

TEST_F(DocumentLayoutOptionsTest, CopyAssignment) {
  options_.RotatePagesClockwise();

  DocumentLayout::Options copy;
  EXPECT_EQ(copy.default_page_orientation(), PageOrientation::kOriginal);
  copy = options_;
  EXPECT_EQ(copy.default_page_orientation(), PageOrientation::kClockwise90);

  options_.RotatePagesClockwise();
  EXPECT_EQ(copy.default_page_orientation(), PageOrientation::kClockwise90);
}

TEST_F(DocumentLayoutOptionsTest, RotatePagesClockwise) {
  options_.RotatePagesClockwise();
  EXPECT_EQ(options_.default_page_orientation(), PageOrientation::kClockwise90);

  options_.RotatePagesClockwise();
  EXPECT_EQ(options_.default_page_orientation(),
            PageOrientation::kClockwise180);

  options_.RotatePagesClockwise();
  EXPECT_EQ(options_.default_page_orientation(),
            PageOrientation::kClockwise270);

  options_.RotatePagesClockwise();
  EXPECT_EQ(options_.default_page_orientation(), PageOrientation::kOriginal);
}

TEST_F(DocumentLayoutOptionsTest, RotatePagesCounterclockwise) {
  options_.RotatePagesCounterclockwise();
  EXPECT_EQ(options_.default_page_orientation(),
            PageOrientation::kClockwise270);

  options_.RotatePagesCounterclockwise();
  EXPECT_EQ(options_.default_page_orientation(),
            PageOrientation::kClockwise180);

  options_.RotatePagesCounterclockwise();
  EXPECT_EQ(options_.default_page_orientation(), PageOrientation::kClockwise90);

  options_.RotatePagesCounterclockwise();
  EXPECT_EQ(options_.default_page_orientation(), PageOrientation::kOriginal);
}

TEST_F(DocumentLayoutTest, DefaultConstructor) {
  EXPECT_EQ(layout_.options().default_page_orientation(),
            PageOrientation::kOriginal);
  EXPECT_FALSE(layout_.dirty());
  EXPECT_PRED2(PpSizeEq, layout_.size(), pp::Size(0, 0));
  EXPECT_EQ(layout_.page_count(), 0u);
}

TEST_F(DocumentLayoutTest, SetOptionsDoesNotRecomputeLayout) {
  layout_.ComputeSingleViewLayout({pp::Size(100, 200)});
  EXPECT_PRED2(PpSizeEq, layout_.size(), pp::Size(100, 200));

  DocumentLayout::Options options;
  options.RotatePagesClockwise();
  layout_.SetOptions(options);
  EXPECT_EQ(layout_.options().default_page_orientation(),
            PageOrientation::kClockwise90);
  EXPECT_PRED2(PpSizeEq, layout_.size(), pp::Size(100, 200));
}

TEST_F(DocumentLayoutTest, DirtySetOnOrientationChange) {
  DocumentLayout::Options options;
  layout_.SetOptions(options);
  EXPECT_FALSE(layout_.dirty());

  options.RotatePagesClockwise();
  layout_.SetOptions(options);
  EXPECT_TRUE(layout_.dirty());
}

TEST_F(DocumentLayoutTest, ComputeSingleViewLayout) {
  std::vector<pp::Size> page_sizes{
      {300, 400}, {400, 500}, {300, 400}, {200, 300}};
  layout_.ComputeSingleViewLayout(page_sizes);
  ASSERT_EQ(4u, layout_.page_count());
  EXPECT_PRED2(PpRectEq, pp::Rect(50, 0, 300, 400), layout_.page_rect(0));
  EXPECT_PRED2(PpRectEq, pp::Rect(0, 404, 400, 500), layout_.page_rect(1));
  EXPECT_PRED2(PpRectEq, pp::Rect(50, 908, 300, 400), layout_.page_rect(2));
  EXPECT_PRED2(PpRectEq, pp::Rect(100, 1312, 200, 300), layout_.page_rect(3));
  EXPECT_PRED2(PpRectEq, pp::Rect(55, 3, 290, 390),
               layout_.page_bounds_rect(0));
  EXPECT_PRED2(PpRectEq, pp::Rect(5, 407, 390, 490),
               layout_.page_bounds_rect(1));
  EXPECT_PRED2(PpRectEq, pp::Rect(55, 911, 290, 390),
               layout_.page_bounds_rect(2));
  EXPECT_PRED2(PpRectEq, pp::Rect(105, 1315, 190, 290),
               layout_.page_bounds_rect(3));
  EXPECT_PRED2(PpSizeEq, pp::Size(400, 1612), layout_.size());

  page_sizes = {{240, 300}, {320, 400}, {250, 360}, {300, 600}, {270, 555}};
  layout_.ComputeSingleViewLayout(page_sizes);
  ASSERT_EQ(5u, layout_.page_count());
  EXPECT_PRED2(PpRectEq, pp::Rect(40, 0, 240, 300), layout_.page_rect(0));
  EXPECT_PRED2(PpRectEq, pp::Rect(0, 304, 320, 400), layout_.page_rect(1));
  EXPECT_PRED2(PpRectEq, pp::Rect(35, 708, 250, 360), layout_.page_rect(2));
  EXPECT_PRED2(PpRectEq, pp::Rect(10, 1072, 300, 600), layout_.page_rect(3));
  EXPECT_PRED2(PpRectEq, pp::Rect(25, 1676, 270, 555), layout_.page_rect(4));
  EXPECT_PRED2(PpRectEq, pp::Rect(45, 3, 230, 290),
               layout_.page_bounds_rect(0));
  EXPECT_PRED2(PpRectEq, pp::Rect(5, 307, 310, 390),
               layout_.page_bounds_rect(1));
  EXPECT_PRED2(PpRectEq, pp::Rect(40, 711, 240, 350),
               layout_.page_bounds_rect(2));
  EXPECT_PRED2(PpRectEq, pp::Rect(15, 1075, 290, 590),
               layout_.page_bounds_rect(3));
  EXPECT_PRED2(PpRectEq, pp::Rect(30, 1679, 260, 545),
               layout_.page_bounds_rect(4));
  EXPECT_PRED2(PpSizeEq, pp::Size(320, 2231), layout_.size());
}

TEST_F(DocumentLayoutTest, ComputeTwoUpViewLayout) {
  // Test case where the widest page is on the right.
  std::vector<pp::Size> page_sizes{
      {826, 1066}, {1066, 826}, {826, 1066}, {826, 900}};
  layout_.ComputeTwoUpViewLayout(page_sizes);
  ASSERT_EQ(4u, layout_.page_count());
  EXPECT_PRED2(PpRectEq, pp::Rect(240, 0, 826, 1066), layout_.page_rect(0));
  EXPECT_PRED2(PpRectEq, pp::Rect(1066, 0, 1066, 826), layout_.page_rect(1));
  EXPECT_PRED2(PpRectEq, pp::Rect(240, 1066, 826, 1066), layout_.page_rect(2));
  EXPECT_PRED2(PpRectEq, pp::Rect(1066, 1066, 826, 900), layout_.page_rect(3));
  EXPECT_PRED2(PpRectEq, pp::Rect(245, 3, 820, 1056),
               layout_.page_bounds_rect(0));
  EXPECT_PRED2(PpRectEq, pp::Rect(1067, 3, 1060, 816),
               layout_.page_bounds_rect(1));
  EXPECT_PRED2(PpRectEq, pp::Rect(245, 1069, 820, 1056),
               layout_.page_bounds_rect(2));
  EXPECT_PRED2(PpRectEq, pp::Rect(1067, 1069, 820, 890),
               layout_.page_bounds_rect(3));
  EXPECT_PRED2(PpSizeEq, pp::Size(2132, 2132), layout_.size());

  // Test case where the widest page is on the left.
  page_sizes = {{1066, 826}, {820, 1056}, {820, 890}, {826, 1066}};
  layout_.ComputeTwoUpViewLayout(page_sizes);
  ASSERT_EQ(4u, layout_.page_count());
  EXPECT_PRED2(PpRectEq, pp::Rect(0, 0, 1066, 826), layout_.page_rect(0));
  EXPECT_PRED2(PpRectEq, pp::Rect(1066, 0, 820, 1056), layout_.page_rect(1));
  EXPECT_PRED2(PpRectEq, pp::Rect(246, 1056, 820, 890), layout_.page_rect(2));
  EXPECT_PRED2(PpRectEq, pp::Rect(1066, 1056, 826, 1066), layout_.page_rect(3));
  EXPECT_PRED2(PpRectEq, pp::Rect(5, 3, 1060, 816),
               layout_.page_bounds_rect(0));
  EXPECT_PRED2(PpRectEq, pp::Rect(1067, 3, 814, 1046),
               layout_.page_bounds_rect(1));
  EXPECT_PRED2(PpRectEq, pp::Rect(251, 1059, 814, 880),
               layout_.page_bounds_rect(2));
  EXPECT_PRED2(PpRectEq, pp::Rect(1067, 1059, 820, 1056),
               layout_.page_bounds_rect(3));
  EXPECT_PRED2(PpSizeEq, pp::Size(2132, 2122), layout_.size());

  // Test case where there's an odd # of pages.
  page_sizes = {{200, 300}, {400, 200}, {300, 600}, {250, 500}, {300, 400}};
  layout_.ComputeTwoUpViewLayout(page_sizes);
  ASSERT_EQ(5u, layout_.page_count());
  EXPECT_PRED2(PpRectEq, pp::Rect(200, 0, 200, 300), layout_.page_rect(0));
  EXPECT_PRED2(PpRectEq, pp::Rect(400, 0, 400, 200), layout_.page_rect(1));
  EXPECT_PRED2(PpRectEq, pp::Rect(100, 300, 300, 600), layout_.page_rect(2));
  EXPECT_PRED2(PpRectEq, pp::Rect(400, 300, 250, 500), layout_.page_rect(3));
  EXPECT_PRED2(PpRectEq, pp::Rect(100, 900, 300, 400), layout_.page_rect(4));
  EXPECT_PRED2(PpRectEq, pp::Rect(205, 3, 194, 290),
               layout_.page_bounds_rect(0));
  EXPECT_PRED2(PpRectEq, pp::Rect(401, 3, 394, 190),
               layout_.page_bounds_rect(1));
  EXPECT_PRED2(PpRectEq, pp::Rect(105, 303, 294, 590),
               layout_.page_bounds_rect(2));
  EXPECT_PRED2(PpRectEq, pp::Rect(401, 303, 244, 490),
               layout_.page_bounds_rect(3));
  EXPECT_PRED2(PpRectEq, pp::Rect(105, 903, 290, 390),
               layout_.page_bounds_rect(4));
  EXPECT_PRED2(PpSizeEq, pp::Size(800, 1300), layout_.size());
}

TEST_F(DocumentLayoutTest, DirtySetOnSingleViewLayoutInputChange) {
  layout_.ComputeSingleViewLayout({pp::Size(100, 200)});
  EXPECT_TRUE(layout_.dirty());
  layout_.clear_dirty();
  EXPECT_FALSE(layout_.dirty());

  layout_.ComputeSingleViewLayout({pp::Size(100, 200)});
  EXPECT_FALSE(layout_.dirty());

  layout_.ComputeSingleViewLayout({pp::Size(200, 100)});
  EXPECT_TRUE(layout_.dirty());
  layout_.clear_dirty();

  layout_.ComputeSingleViewLayout({pp::Size(200, 100), pp::Size(300, 300)});
  EXPECT_TRUE(layout_.dirty());
  layout_.clear_dirty();

  layout_.ComputeSingleViewLayout({pp::Size(200, 100)});
  EXPECT_TRUE(layout_.dirty());
}

TEST_F(DocumentLayoutTest, DirtySetOnTwoUpViewLayoutInputChange) {
  layout_.ComputeTwoUpViewLayout({pp::Size(100, 200), pp::Size(200, 100)});
  EXPECT_TRUE(layout_.dirty());
  layout_.clear_dirty();
  EXPECT_FALSE(layout_.dirty());

  layout_.ComputeTwoUpViewLayout({pp::Size(100, 200), pp::Size(200, 100)});
  EXPECT_FALSE(layout_.dirty());

  layout_.ComputeTwoUpViewLayout({pp::Size(200, 100), pp::Size(100, 200)});
  EXPECT_TRUE(layout_.dirty());
  layout_.clear_dirty();

  layout_.ComputeTwoUpViewLayout(
      {pp::Size(200, 100), pp::Size(100, 200), pp::Size(300, 300)});
  EXPECT_TRUE(layout_.dirty());
  layout_.clear_dirty();

  layout_.ComputeTwoUpViewLayout({pp::Size(200, 100), pp::Size(100, 200)});
  EXPECT_TRUE(layout_.dirty());
}

}  // namespace

}  // namespace chrome_pdf
