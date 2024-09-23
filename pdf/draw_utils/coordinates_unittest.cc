// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/draw_utils/coordinates.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace chrome_pdf {
namespace draw_utils {

namespace {

constexpr int kBottomSeparator = 4;
constexpr int kHorizontalSeparator = 1;
constexpr gfx::Insets kLeftInsets =
    gfx::Insets::TLBR(/*top=*/3, /*left=*/5, /*bottom=*/7, /*right=*/1);
constexpr gfx::Insets kRightInsets =
    gfx::Insets::TLBR(/*top=*/3, /*left=*/1, /*bottom=*/7, /*right=*/5);
constexpr gfx::Insets kSingleViewInsets =
    gfx::Insets::TLBR(/*top=*/3, /*left=*/5, /*bottom=*/7, /*right=*/5);

}  // namespace

TEST(CoordinateTest, AdjustBottomGapForRightSidePage) {
  gfx::Rect bottom_gap(0, 10, 500, 100);
  AdjustBottomGapForRightSidePage(250, &bottom_gap);
  EXPECT_EQ(gfx::Rect(250, 10, 250, 100), bottom_gap);

  bottom_gap.SetRect(15, 20, 700, 100);
  AdjustBottomGapForRightSidePage(365, &bottom_gap);
  EXPECT_EQ(gfx::Rect(365, 20, 350, 100), bottom_gap);

  bottom_gap.SetRect(100, 40, 951, 200);
  AdjustBottomGapForRightSidePage(450, &bottom_gap);
  EXPECT_EQ(gfx::Rect(450, 40, 475, 200), bottom_gap);
}

TEST(CoordinateTest, CenterRectHorizontally) {
  gfx::Rect page_rect(10, 20, 400, 300);
  CenterRectHorizontally(600, &page_rect);
  EXPECT_EQ(gfx::Rect(100, 20, 400, 300), page_rect);

  page_rect.SetRect(300, 450, 500, 700);
  CenterRectHorizontally(800, &page_rect);
  EXPECT_EQ(gfx::Rect(150, 450, 500, 700), page_rect);

  page_rect.SetRect(800, 100, 200, 250);
  CenterRectHorizontally(350, &page_rect);
  EXPECT_EQ(gfx::Rect(75, 100, 200, 250), page_rect);
}

TEST(CoordinateTest, ExpandDocumentSize) {
  gfx::Size doc_size(100, 400);

  // Test various expansion sizes.
  gfx::Size rect_size(100, 200);
  ExpandDocumentSize(rect_size, &doc_size);
  EXPECT_EQ(gfx::Size(100, 600), doc_size);

  rect_size.SetSize(200, 150);
  ExpandDocumentSize(rect_size, &doc_size);
  EXPECT_EQ(gfx::Size(200, 750), doc_size);

  rect_size.SetSize(100, 300);
  ExpandDocumentSize(rect_size, &doc_size);
  EXPECT_EQ(gfx::Size(200, 1050), doc_size);

  rect_size.SetSize(250, 400);
  ExpandDocumentSize(rect_size, &doc_size);
  EXPECT_EQ(gfx::Size(250, 1450), doc_size);
}

TEST(CoordinateTest, GetBottomGapBetweenRects) {
  EXPECT_EQ(gfx::Rect(95, 600, 350, 50),
            GetBottomGapBetweenRects(600, {95, 200, 350, 450}));

  EXPECT_EQ(gfx::Rect(200, 500, 350, 10),
            GetBottomGapBetweenRects(500, {200, 0, 350, 510}));

  // Test rectangle with a negative bottom value.
  EXPECT_EQ(gfx::Rect(150, -100, 400, 150),
            GetBottomGapBetweenRects(-100, {150, 0, 400, 50}));

  // Test case where `page_rect_bottom` >= `dirty_rect.bottom()`.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0),
            GetBottomGapBetweenRects(1400, {0, 10, 300, 500}));
}

TEST(CoordinateTest, GetMostVisiblePage) {
  // Test a single-view layout.
  std::vector<IndexedPage> visible_pages = {
      {0, {0, 0, 50, 100}}, {1, {0, 100, 100, 100}}, {2, {0, 200, 100, 200}}};
  EXPECT_EQ(0, GetMostVisiblePage(visible_pages, {0, 0, 100, 100}));
  EXPECT_EQ(1, GetMostVisiblePage(visible_pages, {0, 100, 100, 100}));
  EXPECT_EQ(0, GetMostVisiblePage(visible_pages, {0, 50, 100, 100}));
  EXPECT_EQ(1, GetMostVisiblePage(visible_pages, {0, 51, 100, 100}));
  EXPECT_EQ(2, GetMostVisiblePage(visible_pages, {0, 180, 100, 100}));
  EXPECT_EQ(1, GetMostVisiblePage(visible_pages, {0, 160, 100, 100}));
  EXPECT_EQ(0, GetMostVisiblePage(visible_pages, {0, 77, 50, 50}));
  EXPECT_EQ(1, GetMostVisiblePage(visible_pages, {0, 85, 50, 50}));
  EXPECT_EQ(0, GetMostVisiblePage(visible_pages, {0, 0, 400, 400}));

  // Test a two-up view layout.
  visible_pages = {{0, {100, 0, 300, 400}},
                   {1, {400, 0, 400, 300}},
                   {2, {0, 400, 400, 250}},
                   {3, {400, 400, 200, 400}},
                   {4, {50, 800, 350, 200}}};
  EXPECT_EQ(0, GetMostVisiblePage(visible_pages, {0, 0, 400, 500}));
  EXPECT_EQ(2, GetMostVisiblePage(visible_pages, {0, 200, 400, 500}));
  EXPECT_EQ(3, GetMostVisiblePage(visible_pages, {400, 200, 400, 500}));
  EXPECT_EQ(3, GetMostVisiblePage(visible_pages, {200, 200, 400, 500}));
  EXPECT_EQ(0, GetMostVisiblePage(visible_pages, {0, 0, 1600, 2000}));

  // Test case where no pages intersect with the viewport.
  EXPECT_EQ(0, GetMostVisiblePage(visible_pages, {1000, 2000, 150, 200}));

  // Test empty vector.
  std::vector<IndexedPage> empty_pages;
  EXPECT_EQ(-1, GetMostVisiblePage(empty_pages, {100, 200, 300, 400}));
}

TEST(CoordinateTest, GetPageInsetsForTwoUpView) {
  // Page is on the left side and isn't the last page in the document.
  EXPECT_EQ(kLeftInsets, GetPageInsetsForTwoUpView(0, 10, kSingleViewInsets,
                                                   kHorizontalSeparator));

  // Page is on the left side and is the last page in the document.
  EXPECT_EQ(kSingleViewInsets,
            GetPageInsetsForTwoUpView(10, 11, kSingleViewInsets,
                                      kHorizontalSeparator));

  // Only one page in the document.
  EXPECT_EQ(
      kSingleViewInsets,
      GetPageInsetsForTwoUpView(0, 1, kSingleViewInsets, kHorizontalSeparator));

  // Page is on the right side of the document.
  EXPECT_EQ(kRightInsets, GetPageInsetsForTwoUpView(1, 4, kSingleViewInsets,
                                                    kHorizontalSeparator));
}

TEST(CoordinateTest, GetRectForSingleView) {
  // Test portrait pages.
  EXPECT_EQ(gfx::Rect(50, 500, 200, 400),
            GetRectForSingleView({200, 400}, {300, 500}));
  EXPECT_EQ(gfx::Rect(50, 600, 100, 340),
            GetRectForSingleView({100, 340}, {200, 600}));

  // Test landscape pages.
  EXPECT_EQ(gfx::Rect(0, 1000, 500, 450),
            GetRectForSingleView({500, 450}, {500, 1000}));
  EXPECT_EQ(gfx::Rect(25, 1500, 650, 200),
            GetRectForSingleView({650, 200}, {700, 1500}));
}

TEST(CoordinateTest, GetScreenRect) {
  const gfx::Rect rect(10, 20, 200, 300);

  // Test various zooms with the position at the origin.
  EXPECT_EQ(gfx::Rect(10, 20, 200, 300), GetScreenRect(rect, {0, 0}, 1));
  EXPECT_EQ(gfx::Rect(15, 30, 300, 450), GetScreenRect(rect, {0, 0}, 1.5));
  EXPECT_EQ(gfx::Rect(5, 10, 100, 150), GetScreenRect(rect, {0, 0}, 0.5));

  // Test various zooms with the position elsewhere.
  EXPECT_EQ(gfx::Rect(-390, -10, 200, 300), GetScreenRect(rect, {400, 30}, 1));
  EXPECT_EQ(gfx::Rect(-385, 0, 300, 450), GetScreenRect(rect, {400, 30}, 1.5));
  EXPECT_EQ(gfx::Rect(-395, -20, 100, 150),
            GetScreenRect(rect, {400, 30}, 0.5));

  // Test various zooms with a negative position.
  EXPECT_EQ(gfx::Rect(-90, 70, 200, 300), GetScreenRect(rect, {100, -50}, 1));
  EXPECT_EQ(gfx::Rect(-85, 80, 300, 450), GetScreenRect(rect, {100, -50}, 1.5));
  EXPECT_EQ(gfx::Rect(-95, 60, 100, 150), GetScreenRect(rect, {100, -50}, 0.5));

  // Test an empty rect always outputs an empty rect.
  const gfx::Rect empty_rect;
  EXPECT_EQ(gfx::Rect(-20, -500, 0, 0),
            GetScreenRect(empty_rect, {20, 500}, 1));
  EXPECT_EQ(gfx::Rect(-20, -500, 0, 0),
            GetScreenRect(empty_rect, {20, 500}, 1.5));
  EXPECT_EQ(gfx::Rect(-20, -500, 0, 0),
            GetScreenRect(empty_rect, {20, 500}, 0.5));
}

TEST(CoordinateTest, GetSurroundingRect) {
  constexpr int kDocWidth = 1000;

  // Test various position, sizes, and document width.
  EXPECT_EQ(gfx::Rect(0, 97, 1000, 314),
            GetSurroundingRect(100, 300, kSingleViewInsets, kDocWidth,
                               kBottomSeparator));
  EXPECT_EQ(gfx::Rect(0, 37, 1000, 214),
            GetSurroundingRect(40, 200, kSingleViewInsets, kDocWidth,
                               kBottomSeparator));
  EXPECT_EQ(gfx::Rect(0, 197, 1000, 514),
            GetSurroundingRect(200, 500, kSingleViewInsets, kDocWidth,
                               kBottomSeparator));
  EXPECT_EQ(
      gfx::Rect(0, -103, 200, 314),
      GetSurroundingRect(-100, 300, kSingleViewInsets, 200, kBottomSeparator));
}

TEST(CoordinateTest, GetLeftFillRect) {
  // Testing various rectangles with different positions and sizes.
  gfx::Rect page_rect(10, 20, 400, 500);
  EXPECT_EQ(gfx::Rect(0, 17, 5, 514),
            GetLeftFillRect(page_rect, kSingleViewInsets, kBottomSeparator));

  page_rect.SetRect(200, 300, 400, 350);
  EXPECT_EQ(gfx::Rect(0, 297, 195, 364),
            GetLeftFillRect(page_rect, kSingleViewInsets, kBottomSeparator));

  page_rect.SetRect(800, 650, 20, 15);
  EXPECT_EQ(gfx::Rect(0, 647, 795, 29),
            GetLeftFillRect(page_rect, kSingleViewInsets, kBottomSeparator));

  // Testing rectangle with a negative y-component.
  page_rect.SetRect(50, -200, 100, 300);
  EXPECT_EQ(gfx::Rect(0, -203, 45, 314),
            GetLeftFillRect(page_rect, kSingleViewInsets, kBottomSeparator));
}

TEST(CoordinateTest, GetRightFillRect) {
  constexpr int kDocWidth = 1000;

  // Testing various rectangles with different positions, sizes, and document
  // widths.
  gfx::Rect page_rect(10, 20, 400, 500);
  EXPECT_EQ(gfx::Rect(415, 17, 585, 514),
            GetRightFillRect(page_rect, kSingleViewInsets, kDocWidth,
                             kBottomSeparator));

  page_rect.SetRect(200, 300, 400, 350);
  EXPECT_EQ(gfx::Rect(605, 297, 395, 364),
            GetRightFillRect(page_rect, kSingleViewInsets, kDocWidth,
                             kBottomSeparator));

  page_rect.SetRect(200, 300, 400, 350);
  EXPECT_EQ(
      gfx::Rect(605, 297, 195, 364),
      GetRightFillRect(page_rect, kSingleViewInsets, 800, kBottomSeparator));

  // Testing rectangle with a negative y-component.
  page_rect.SetRect(50, -200, 100, 300);
  EXPECT_EQ(gfx::Rect(155, -203, 845, 314),
            GetRightFillRect(page_rect, kSingleViewInsets, kDocWidth,
                             kBottomSeparator));
}

TEST(CoordinateTest, GetBottomFillRect) {
  // Testing various rectangles with different positions and sizes.
  gfx::Rect page_rect(10, 20, 400, 500);
  EXPECT_EQ(gfx::Rect(5, 527, 410, 4),
            GetBottomFillRect(page_rect, kSingleViewInsets, kBottomSeparator));

  page_rect.SetRect(200, 300, 400, 350);
  EXPECT_EQ(gfx::Rect(195, 657, 410, 4),
            GetBottomFillRect(page_rect, kSingleViewInsets, kBottomSeparator));

  page_rect.SetRect(800, 650, 20, 15);
  EXPECT_EQ(gfx::Rect(795, 672, 30, 4),
            GetBottomFillRect(page_rect, kSingleViewInsets, kBottomSeparator));

  // Testing rectangle with a negative y-component.
  page_rect.SetRect(50, -200, 100, 300);
  EXPECT_EQ(gfx::Rect(45, 107, 110, 4),
            GetBottomFillRect(page_rect, kSingleViewInsets, kBottomSeparator));
}

TEST(CoordinateTest, GetLeftRectForTwoUpView) {
  EXPECT_EQ(gfx::Rect(100, 100, 200, 400),
            GetLeftRectForTwoUpView({200, 400}, {300, 100}));
  EXPECT_EQ(gfx::Rect(0, 0, 300, 400),
            GetLeftRectForTwoUpView({300, 400}, {300, 0}));

  // Test empty rect gets positioned.
  EXPECT_EQ(gfx::Rect(100, 0, 0, 0), GetLeftRectForTwoUpView({0, 0}, {100, 0}));
}

TEST(CoordinateTest, GetRightRectForTwoUpView) {
  EXPECT_EQ(gfx::Rect(300, 100, 200, 400),
            GetRightRectForTwoUpView({200, 400}, {300, 100}));
  EXPECT_EQ(gfx::Rect(300, 0, 300, 400),
            GetRightRectForTwoUpView({300, 400}, {300, 0}));

  // Test empty rect gets positioned.
  EXPECT_EQ(gfx::Rect(100, 0, 0, 0),
            GetRightRectForTwoUpView({0, 0}, {100, 0}));
}

}  // namespace draw_utils
}  // namespace chrome_pdf
