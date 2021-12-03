// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

TEST(LogicalSizeTest, AspectRatioFromSizeF) {
  LogicalSize logical;

  // Just test there is no precision loss when multiply/dividing through the
  // aspect-ratio.
  logical = LogicalSize::AspectRatioFromSizeF(gfx::SizeF(0.25f, 0.1f));
  EXPECT_EQ(LayoutUnit(250),
            LayoutUnit(100).MulDiv(logical.inline_size, logical.block_size));

  logical = LogicalSize::AspectRatioFromSizeF(gfx::SizeF(0.1f, 0.25f));
  EXPECT_EQ(LayoutUnit(40),
            LayoutUnit(100).MulDiv(logical.inline_size, logical.block_size));

  logical = LogicalSize::AspectRatioFromSizeF(gfx::SizeF(2.0f, 0.01f));
  EXPECT_EQ(LayoutUnit(20000),
            LayoutUnit(100).MulDiv(logical.inline_size, logical.block_size));

  logical = LogicalSize::AspectRatioFromSizeF(gfx::SizeF(0.01f, 2.0f));
  EXPECT_EQ(LayoutUnit(0.5),
            LayoutUnit(100).MulDiv(logical.inline_size, logical.block_size));
}

}  // namespace

}  // namespace blink
