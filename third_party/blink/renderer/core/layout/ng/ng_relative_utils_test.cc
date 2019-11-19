// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace {

const LayoutUnit kHorizontalSize{100};
const LayoutUnit kVerticalSize{200};
const LayoutUnit kLeft{3};
const LayoutUnit kRight{5};
const LayoutUnit kTop{7};
const LayoutUnit kBottom{9};
const LayoutUnit kAuto{-1};
const LayoutUnit kZero{0};

class NGRelativeUtilsTest : public testing::Test {
 protected:
  void SetUp() override {
    style_ = ComputedStyle::Create();
    style_->SetPosition(EPosition::kRelative);
    container_size_ = PhysicalSize{kHorizontalSize, kVerticalSize};
  }

  void SetTRBL(LayoutUnit top,
               LayoutUnit right,
               LayoutUnit bottom,
               LayoutUnit left) {
    style_->SetTop(top == kAuto ? Length::Auto() : Length::Fixed(top.ToInt()));
    style_->SetRight(right == kAuto ? Length::Auto()
                                    : Length::Fixed(right.ToInt()));
    style_->SetBottom(bottom == kAuto ? Length::Auto()
                                      : Length::Fixed(bottom.ToInt()));
    style_->SetLeft(left == kAuto ? Length::Auto()
                                  : Length::Fixed(left.ToInt()));
  }

  scoped_refptr<ComputedStyle> style_;
  PhysicalSize container_size_;
};

TEST_F(NGRelativeUtilsTest, HorizontalTB) {
  PhysicalOffset offset;

  // Everything auto defaults to kZero,kZero
  SetTRBL(kAuto, kAuto, kAuto, kAuto);
  offset = ComputeRelativeOffset(*style_, WritingMode::kHorizontalTb,
                                 TextDirection::kLtr, container_size_);
  EXPECT_EQ(offset.left, kZero);
  EXPECT_EQ(offset.top, kZero);

  // Set all sides
  SetTRBL(kTop, kRight, kBottom, kLeft);

  // kLtr
  offset = ComputeRelativeOffset(*style_, WritingMode::kHorizontalTb,
                                 TextDirection::kLtr, container_size_);
  EXPECT_EQ(offset.left, kLeft);
  EXPECT_EQ(offset.top, kTop);

  // kRtl
  offset = ComputeRelativeOffset(*style_, WritingMode::kHorizontalTb,
                                 TextDirection::kRtl, container_size_);
  EXPECT_EQ(offset.left, -kRight);
  EXPECT_EQ(offset.top, kTop);

  // Set only non-default sides
  SetTRBL(kAuto, kRight, kBottom, kAuto);
  offset = ComputeRelativeOffset(*style_, WritingMode::kHorizontalTb,
                                 TextDirection::kLtr, container_size_);
  EXPECT_EQ(offset.left, -kRight);
  EXPECT_EQ(offset.top, -kBottom);
}

TEST_F(NGRelativeUtilsTest, VerticalRightLeft) {
  PhysicalOffset offset;

  // Set all sides
  SetTRBL(kTop, kRight, kBottom, kLeft);

  // kLtr
  offset = ComputeRelativeOffset(*style_, WritingMode::kVerticalRl,
                                 TextDirection::kLtr, container_size_);
  EXPECT_EQ(offset.top, kTop);
  EXPECT_EQ(offset.left, kLeft);

  // kRtl
  offset = ComputeRelativeOffset(*style_, WritingMode::kVerticalRl,
                                 TextDirection::kRtl, container_size_);
  EXPECT_EQ(offset.top, -kBottom);
  EXPECT_EQ(offset.left, kLeft);

  // Set only non-default sides
  SetTRBL(kAuto, kRight, kBottom, kAuto);
  offset = ComputeRelativeOffset(*style_, WritingMode::kVerticalRl,
                                 TextDirection::kLtr, container_size_);
  EXPECT_EQ(offset.top, -kBottom);
  EXPECT_EQ(offset.left, -kRight);
}

TEST_F(NGRelativeUtilsTest, VerticalLeftRight) {
  PhysicalOffset offset;

  // Set all sides
  SetTRBL(kTop, kRight, kBottom, kLeft);

  // kLtr
  offset = ComputeRelativeOffset(*style_, WritingMode::kVerticalLr,
                                 TextDirection::kLtr, container_size_);
  EXPECT_EQ(offset.top, kTop);
  EXPECT_EQ(offset.left, kLeft);

  // kRtl
  offset = ComputeRelativeOffset(*style_, WritingMode::kVerticalLr,
                                 TextDirection::kRtl, container_size_);
  EXPECT_EQ(offset.top, -kBottom);
  EXPECT_EQ(offset.left, kLeft);

  // Set only non-default sides
  SetTRBL(kAuto, kRight, kBottom, kAuto);
  offset = ComputeRelativeOffset(*style_, WritingMode::kVerticalLr,
                                 TextDirection::kLtr, container_size_);
  EXPECT_EQ(offset.top, -kBottom);
  EXPECT_EQ(offset.left, -kRight);
}

}  // namespace
}  // namespace blink
