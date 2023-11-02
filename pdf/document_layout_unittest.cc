// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/document_layout.h"

#include "base/i18n/rtl.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "pdf/page_orientation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace chrome_pdf {

namespace {

class DocumentLayoutOptionsTest : public testing::Test {
 protected:
  DocumentLayout::Options options_;
};

TEST_F(DocumentLayoutOptionsTest, DefaultConstructor) {
  EXPECT_EQ(options_.direction(), base::i18n::UNKNOWN_DIRECTION);
  EXPECT_EQ(options_.default_page_orientation(), PageOrientation::kOriginal);
  EXPECT_EQ(options_.page_spread(), DocumentLayout::PageSpread::kOneUp);
}

TEST_F(DocumentLayoutOptionsTest, CopyConstructor) {
  options_.set_direction(base::i18n::RIGHT_TO_LEFT);
  options_.RotatePagesClockwise();
  options_.set_page_spread(DocumentLayout::PageSpread::kTwoUpOdd);

  DocumentLayout::Options copy(options_);
  EXPECT_EQ(copy.direction(), base::i18n::RIGHT_TO_LEFT);
  EXPECT_EQ(copy.default_page_orientation(), PageOrientation::kClockwise90);
  EXPECT_EQ(copy.page_spread(), DocumentLayout::PageSpread::kTwoUpOdd);

  options_.set_direction(base::i18n::LEFT_TO_RIGHT);
  options_.RotatePagesClockwise();
  options_.set_page_spread(DocumentLayout::PageSpread::kOneUp);
  EXPECT_EQ(copy.direction(), base::i18n::RIGHT_TO_LEFT);
  EXPECT_EQ(copy.default_page_orientation(), PageOrientation::kClockwise90);
  EXPECT_EQ(copy.page_spread(), DocumentLayout::PageSpread::kTwoUpOdd);
}

TEST_F(DocumentLayoutOptionsTest, CopyAssignment) {
  options_.set_direction(base::i18n::RIGHT_TO_LEFT);
  options_.RotatePagesClockwise();
  options_.set_page_spread(DocumentLayout::PageSpread::kTwoUpOdd);

  DocumentLayout::Options copy = options_;
  EXPECT_EQ(copy.direction(), base::i18n::RIGHT_TO_LEFT);
  EXPECT_EQ(copy.default_page_orientation(), PageOrientation::kClockwise90);
  EXPECT_EQ(copy.page_spread(), DocumentLayout::PageSpread::kTwoUpOdd);

  options_.set_direction(base::i18n::LEFT_TO_RIGHT);
  options_.RotatePagesClockwise();
  options_.set_page_spread(DocumentLayout::PageSpread::kOneUp);
  EXPECT_EQ(copy.direction(), base::i18n::RIGHT_TO_LEFT);
  EXPECT_EQ(copy.default_page_orientation(), PageOrientation::kClockwise90);
  EXPECT_EQ(copy.page_spread(), DocumentLayout::PageSpread::kTwoUpOdd);
}

TEST_F(DocumentLayoutOptionsTest, Equals) {
  EXPECT_TRUE(options_ == options_);

  DocumentLayout::Options copy;
  EXPECT_TRUE(copy == options_);

  options_.set_direction(base::i18n::RIGHT_TO_LEFT);
  EXPECT_FALSE(copy == options_);

  copy.set_direction(base::i18n::RIGHT_TO_LEFT);
  EXPECT_TRUE(copy == options_);

  options_.RotatePagesClockwise();
  EXPECT_FALSE(copy == options_);

  copy.RotatePagesClockwise();
  EXPECT_TRUE(copy == options_);

  options_.RotatePagesCounterclockwise();
  EXPECT_FALSE(copy == options_);

  copy.RotatePagesCounterclockwise();
  EXPECT_TRUE(copy == options_);

  options_.set_page_spread(DocumentLayout::PageSpread::kTwoUpOdd);
  EXPECT_FALSE(copy == options_);

  copy.set_page_spread(DocumentLayout::PageSpread::kTwoUpOdd);
  EXPECT_TRUE(copy == options_);
}

TEST_F(DocumentLayoutOptionsTest, NotEquals) {
  // Given that "!=" is defined as "!(==)", minimal tests should be sufficient
  // here.
  EXPECT_FALSE(options_ != options_);

  DocumentLayout::Options copy;
  EXPECT_FALSE(copy != options_);

  options_.RotatePagesClockwise();
  EXPECT_TRUE(copy != options_);

  copy.RotatePagesClockwise();
  EXPECT_FALSE(copy != options_);
}

TEST_F(DocumentLayoutOptionsTest, ToValueDefault) {
  base::Value value(options_.ToValue());

  EXPECT_THAT(value, base::test::IsJson(R"({
    "direction": 0,
    "defaultPageOrientation": 0,
    "twoUpViewEnabled": false,
  })"));
}

TEST_F(DocumentLayoutOptionsTest, ToValueModified) {
  options_.set_direction(base::i18n::LEFT_TO_RIGHT);
  options_.RotatePagesClockwise();
  options_.set_page_spread(DocumentLayout::PageSpread::kTwoUpOdd);
  base::Value value(options_.ToValue());

  EXPECT_THAT(value, base::test::IsJson(R"({
    "direction": 2,
    "defaultPageOrientation": 1,
    "twoUpViewEnabled": true,
  })"));
}

TEST_F(DocumentLayoutOptionsTest, FromValueDefault) {
  base::Value value = base::test::ParseJson(R"({
    "direction": 0,
    "defaultPageOrientation": 0,
    "twoUpViewEnabled": false,
  })");
  options_.FromValue(value.GetDict());

  EXPECT_EQ(options_, DocumentLayout::Options());
}

TEST_F(DocumentLayoutOptionsTest, FromValueModified) {
  base::Value value = base::test::ParseJson(R"({
    "direction": 2,
    "defaultPageOrientation": 1,
    "twoUpViewEnabled": true,
  })");
  options_.FromValue(value.GetDict());

  EXPECT_EQ(options_.direction(), base::i18n::LEFT_TO_RIGHT);
  EXPECT_EQ(options_.default_page_orientation(), PageOrientation::kClockwise90);
  EXPECT_EQ(options_.page_spread(), DocumentLayout::PageSpread::kTwoUpOdd);
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

class DocumentLayoutTest : public testing::Test {
 protected:
  void SetPageSpread(DocumentLayout::PageSpread page_spread) {
    DocumentLayout::Options options;
    options.set_page_spread(page_spread);
    layout_.SetOptions(options);
  }

  DocumentLayout layout_;
};

TEST_F(DocumentLayoutTest, DefaultConstructor) {
  EXPECT_EQ(layout_.options().default_page_orientation(),
            PageOrientation::kOriginal);
  EXPECT_EQ(layout_.options().page_spread(),
            DocumentLayout::PageSpread::kOneUp);
  EXPECT_FALSE(layout_.dirty());
  EXPECT_EQ(layout_.size(), gfx::Size(0, 0));
  EXPECT_EQ(layout_.page_count(), 0u);
}

TEST_F(DocumentLayoutTest, SetOptionsDoesNotRecomputeLayout) {
  layout_.ComputeLayout({{100, 200}});
  EXPECT_EQ(layout_.size(), gfx::Size(100, 200));

  DocumentLayout::Options options;
  options.RotatePagesClockwise();
  layout_.SetOptions(options);
  EXPECT_EQ(layout_.options().default_page_orientation(),
            PageOrientation::kClockwise90);
  EXPECT_EQ(layout_.size(), gfx::Size(100, 200));
}

TEST_F(DocumentLayoutTest, DirtySetOnOptionsChange) {
  DocumentLayout::Options options;
  layout_.SetOptions(options);
  EXPECT_FALSE(layout_.dirty());

  options.RotatePagesClockwise();
  layout_.SetOptions(options);
  EXPECT_TRUE(layout_.dirty());

  layout_.clear_dirty();

  options.set_page_spread(DocumentLayout::PageSpread::kTwoUpOdd);
  layout_.SetOptions(options);
  EXPECT_TRUE(layout_.dirty());
}

TEST_F(DocumentLayoutTest, DirtyNotSetOnSameOptions) {
  DocumentLayout::Options options;
  options.set_page_spread(DocumentLayout::PageSpread::kTwoUpOdd);
  layout_.SetOptions(options);
  EXPECT_TRUE(layout_.dirty());

  layout_.clear_dirty();

  options.set_page_spread(DocumentLayout::PageSpread::kTwoUpOdd);
  layout_.SetOptions(options);
  EXPECT_FALSE(layout_.dirty());
}

TEST_F(DocumentLayoutTest, ComputeLayoutOneUp) {
  SetPageSpread(DocumentLayout::PageSpread::kOneUp);

  std::vector<gfx::Size> page_sizes{
      {300, 400}, {400, 500}, {300, 400}, {200, 300}};
  layout_.ComputeLayout(page_sizes);
  ASSERT_EQ(4u, layout_.page_count());
  EXPECT_EQ(gfx::Rect(50, 0, 300, 400), layout_.page_rect(0));
  EXPECT_EQ(gfx::Rect(0, 404, 400, 500), layout_.page_rect(1));
  EXPECT_EQ(gfx::Rect(50, 908, 300, 400), layout_.page_rect(2));
  EXPECT_EQ(gfx::Rect(100, 1312, 200, 300), layout_.page_rect(3));
  EXPECT_EQ(gfx::Rect(55, 3, 290, 390), layout_.page_bounds_rect(0));
  EXPECT_EQ(gfx::Rect(5, 407, 390, 490), layout_.page_bounds_rect(1));
  EXPECT_EQ(gfx::Rect(55, 911, 290, 390), layout_.page_bounds_rect(2));
  EXPECT_EQ(gfx::Rect(105, 1315, 190, 290), layout_.page_bounds_rect(3));
  EXPECT_EQ(gfx::Size(400, 1612), layout_.size());

  page_sizes = {{240, 300}, {320, 400}, {250, 360}, {300, 600}, {270, 555}};
  layout_.ComputeLayout(page_sizes);
  ASSERT_EQ(5u, layout_.page_count());
  EXPECT_EQ(gfx::Rect(40, 0, 240, 300), layout_.page_rect(0));
  EXPECT_EQ(gfx::Rect(0, 304, 320, 400), layout_.page_rect(1));
  EXPECT_EQ(gfx::Rect(35, 708, 250, 360), layout_.page_rect(2));
  EXPECT_EQ(gfx::Rect(10, 1072, 300, 600), layout_.page_rect(3));
  EXPECT_EQ(gfx::Rect(25, 1676, 270, 555), layout_.page_rect(4));
  EXPECT_EQ(gfx::Rect(45, 3, 230, 290), layout_.page_bounds_rect(0));
  EXPECT_EQ(gfx::Rect(5, 307, 310, 390), layout_.page_bounds_rect(1));
  EXPECT_EQ(gfx::Rect(40, 711, 240, 350), layout_.page_bounds_rect(2));
  EXPECT_EQ(gfx::Rect(15, 1075, 290, 590), layout_.page_bounds_rect(3));
  EXPECT_EQ(gfx::Rect(30, 1679, 260, 545), layout_.page_bounds_rect(4));
  EXPECT_EQ(gfx::Size(320, 2231), layout_.size());
}

TEST_F(DocumentLayoutTest, ComputeLayoutOneUpWithNoPages) {
  SetPageSpread(DocumentLayout::PageSpread::kOneUp);

  layout_.ComputeLayout({});
  ASSERT_EQ(0u, layout_.page_count());
}

TEST_F(DocumentLayoutTest, DirtySetOnLayoutInputChangeOneUp) {
  SetPageSpread(DocumentLayout::PageSpread::kOneUp);

  layout_.ComputeLayout({{100, 200}});
  EXPECT_TRUE(layout_.dirty());
  layout_.clear_dirty();
  EXPECT_FALSE(layout_.dirty());

  layout_.ComputeLayout({{100, 200}});
  EXPECT_FALSE(layout_.dirty());

  layout_.ComputeLayout({{200, 100}});
  EXPECT_TRUE(layout_.dirty());
  layout_.clear_dirty();

  layout_.ComputeLayout({{200, 100}, {300, 300}});
  EXPECT_TRUE(layout_.dirty());
  layout_.clear_dirty();

  layout_.ComputeLayout({{200, 100}});
  EXPECT_TRUE(layout_.dirty());
}

TEST_F(DocumentLayoutTest, ComputeLayoutTwoUpOdd) {
  SetPageSpread(DocumentLayout::PageSpread::kTwoUpOdd);

  // Test case where the widest page is on the right.
  std::vector<gfx::Size> page_sizes{
      {826, 1066}, {1066, 826}, {826, 1066}, {826, 900}};
  layout_.ComputeLayout(page_sizes);
  ASSERT_EQ(4u, layout_.page_count());
  EXPECT_EQ(gfx::Rect(240, 0, 826, 1066), layout_.page_rect(0));
  EXPECT_EQ(gfx::Rect(1066, 0, 1066, 826), layout_.page_rect(1));
  EXPECT_EQ(gfx::Rect(240, 1066, 826, 1066), layout_.page_rect(2));
  EXPECT_EQ(gfx::Rect(1066, 1066, 826, 900), layout_.page_rect(3));
  EXPECT_EQ(gfx::Rect(245, 3, 820, 1056), layout_.page_bounds_rect(0));
  EXPECT_EQ(gfx::Rect(1067, 3, 1060, 816), layout_.page_bounds_rect(1));
  EXPECT_EQ(gfx::Rect(245, 1069, 820, 1056), layout_.page_bounds_rect(2));
  EXPECT_EQ(gfx::Rect(1067, 1069, 820, 890), layout_.page_bounds_rect(3));
  EXPECT_EQ(gfx::Size(2132, 2132), layout_.size());

  // Test case where the widest page is on the left.
  page_sizes = {{1066, 826}, {820, 1056}, {820, 890}, {826, 1066}};
  layout_.ComputeLayout(page_sizes);
  ASSERT_EQ(4u, layout_.page_count());
  EXPECT_EQ(gfx::Rect(0, 0, 1066, 826), layout_.page_rect(0));
  EXPECT_EQ(gfx::Rect(1066, 0, 820, 1056), layout_.page_rect(1));
  EXPECT_EQ(gfx::Rect(246, 1056, 820, 890), layout_.page_rect(2));
  EXPECT_EQ(gfx::Rect(1066, 1056, 826, 1066), layout_.page_rect(3));
  EXPECT_EQ(gfx::Rect(5, 3, 1060, 816), layout_.page_bounds_rect(0));
  EXPECT_EQ(gfx::Rect(1067, 3, 814, 1046), layout_.page_bounds_rect(1));
  EXPECT_EQ(gfx::Rect(251, 1059, 814, 880), layout_.page_bounds_rect(2));
  EXPECT_EQ(gfx::Rect(1067, 1059, 820, 1056), layout_.page_bounds_rect(3));
  EXPECT_EQ(gfx::Size(2132, 2122), layout_.size());

  // Test case where there's an odd # of pages.
  page_sizes = {{200, 300}, {400, 200}, {300, 600}, {250, 500}, {300, 400}};
  layout_.ComputeLayout(page_sizes);
  ASSERT_EQ(5u, layout_.page_count());
  EXPECT_EQ(gfx::Rect(200, 0, 200, 300), layout_.page_rect(0));
  EXPECT_EQ(gfx::Rect(400, 0, 400, 200), layout_.page_rect(1));
  EXPECT_EQ(gfx::Rect(100, 300, 300, 600), layout_.page_rect(2));
  EXPECT_EQ(gfx::Rect(400, 300, 250, 500), layout_.page_rect(3));
  EXPECT_EQ(gfx::Rect(100, 900, 300, 400), layout_.page_rect(4));
  EXPECT_EQ(gfx::Rect(205, 3, 194, 290), layout_.page_bounds_rect(0));
  EXPECT_EQ(gfx::Rect(401, 3, 394, 190), layout_.page_bounds_rect(1));
  EXPECT_EQ(gfx::Rect(105, 303, 294, 590), layout_.page_bounds_rect(2));
  EXPECT_EQ(gfx::Rect(401, 303, 244, 490), layout_.page_bounds_rect(3));
  EXPECT_EQ(gfx::Rect(105, 903, 290, 390), layout_.page_bounds_rect(4));
  EXPECT_EQ(gfx::Size(800, 1300), layout_.size());
}

TEST_F(DocumentLayoutTest, ComputeLayoutTwoUpOddWithNoPages) {
  SetPageSpread(DocumentLayout::PageSpread::kTwoUpOdd);

  layout_.ComputeLayout({});
  ASSERT_EQ(0u, layout_.page_count());
}

TEST_F(DocumentLayoutTest, DirtySetOnLayoutInputChangeTwoUpOdd) {
  SetPageSpread(DocumentLayout::PageSpread::kTwoUpOdd);

  layout_.ComputeLayout({{100, 200}, {200, 100}});
  EXPECT_TRUE(layout_.dirty());
  layout_.clear_dirty();
  EXPECT_FALSE(layout_.dirty());

  layout_.ComputeLayout({{100, 200}, {200, 100}});
  EXPECT_FALSE(layout_.dirty());

  layout_.ComputeLayout({{200, 100}, {100, 200}});
  EXPECT_TRUE(layout_.dirty());
  layout_.clear_dirty();

  layout_.ComputeLayout({{200, 100}, {100, 200}, {300, 300}});
  EXPECT_TRUE(layout_.dirty());
  layout_.clear_dirty();

  layout_.ComputeLayout({{200, 100}, {100, 200}});
  EXPECT_TRUE(layout_.dirty());
}

}  // namespace

}  // namespace chrome_pdf
